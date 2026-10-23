// In region 4 (src/region.h): called through the stubs in src/dispatch.s,
// under these names here.
#define emporium_order emporium_order_r4
#define emporium_ai emporium_ai_r4

#include "emporium.h"
#include "boat.h"
#include "city.h"
#include "combat.h"
#include "gameint.h"
#include "growth.h"
#include "map.h"
#include "path.h"
#include "sheet.h"
#include "trade.h"
#include "unit.h"

#pragma clang section text="r4text" rodata="r4rodata"

HIGH_BSS byte emporium_count;
HIGH_BSS byte emporium_owner[MAX_EMPORIA];
HIGH_BSS byte emporium_ax[MAX_EMPORIA];
HIGH_BSS byte emporium_ay[MAX_EMPORIA];

// Hexagons between two cells, as ai.c's distance().
static word distance(byte ax, byte ay, byte bx, byte by)
{
    int dx = (int)ax - (int)bx;
    int dy = (2 * (int)ay + (ax & 1)) - (2 * (int)by + (bx & 1));
    if (dx < 0) dx = -dx;
    if (dx > MAP_COLUMNS / 2) dx = MAP_COLUMNS - dx;
    if (dy < 0) dy = -dy;
    return (word)(dy + (dx > dy ? (dx - dy) / 2 : 0));
}

// The trade goods, as trade.c and score.c have them: what an emporium is
// for. Grain, fish and cattle feed the city that has them.
static byte good(byte r)
{
    return r == RES_OLIVES || r == RES_WINE || r == RES_CEDAR
           || r == RES_COPPER || r == RES_TIN || r == RES_DYE
           || r == RES_HORSES;
}

static byte is_water(byte t)
{
    return t == TILE_OCEAN || t == TILE_SHALLOW_OCEAN;
}

// Whether an emporium can go on (ax, ay): see emporium.h.
static byte fits(byte ax, byte ay)
{
    byte m = map_tile[ay][ax], dir, nx, ny, c, coast = 0;

    if (is_water(m & MAP_TERRAIN_MASK)
        || (m & (MAP_OCCUPIED | MAP_IMPROVEMENT))
        || map_resource_at(ax, ay) != RES_NONE
        || emporium_count == MAX_EMPORIA)
        return 0;
    for (dir = 0; dir < 6; ++dir)
        if (map_neighbour(ax, ay, dir, &nx, &ny)
            && MAP_TERRAIN(nx, ny) == TILE_SHALLOW_OCEAN)
            coast = 1;
    if (!coast) return 0;
    for (c = 0; c < city_count; ++c)
        if (distance(ax, ay, city_ax[c], city_ay[c]) < EMPORIUM_SPACING)
            return 0;
    return 1;
}

// How many goods round (ax, ay).
static byte goods_round(byte ax, byte ay)
{
    byte dir, nx, ny, n = 0;
    for (dir = 0; dir < 6; ++dir)
        if (map_neighbour(ax, ay, dir, &nx, &ny)
            && good(map_resource_at(nx, ny)))
            ++n;
    return n;
}

static void found(byte u, byte ax, byte ay)
{
    byte e = emporium_count++;
    emporium_owner[e] = unit_owner[u];
    emporium_ax[e] = ax;
    emporium_ay[e] = ay;
    map_tile[ay][ax] |= MAP_EMPORIUM;
    unit_remove(u);
}

void emporium_order(byte u)
{
    byte dir, nx, ny, n, best = 0, bx = 0, by = 0;

    if (unit_kind[u] == KIND_MERCHANT)
        for (dir = 0; dir < 6; ++dir) {
            if (!map_neighbour(unit_ax[u], unit_ay[u], dir, &nx, &ny)
                || !fits(nx, ny))
                continue;
            n = goods_round(nx, ny);
            if (n > best) {
                best = n;
                bx = nx;
                by = ny;
            }
        }
    if (!best) {
        message("No coast here", "with goods");
        return;
    }
    found(u, bx, by);
    message("Emporium", "founded");
    dirty = 1;
    hover_dirty = 1;
    select_unit(u);
}

// The nearest place for an emporium from (ax, ay) next to a good p has not
// got, copper and tin counted four hexagons nearer, into (site_x, site_y).
// The resources are a list (map.h), so this looks at their neighbours
// rather than at the whole map.
static byte site_x, site_y;

static byte find_site(byte p, byte ax, byte ay)
{
    byte hy, i, r, rx, ry, dir, nx, ny;
    word d, best = 0xffff;

    for (hy = 0; hy < 2 * MAP_ROWS; ++hy)
        for (i = map_res_first[hy]; i < map_res_first[hy + 1]; ++i) {
            r = map_res_kind[i];
            if (!good(r) || player_has(p, r)) continue;
            rx = map_res_ax[i];
            ry = hy >> 1;
            for (dir = 0; dir < 6; ++dir) {
                if (!map_neighbour(rx, ry, dir, &nx, &ny) || !fits(nx, ny))
                    continue;
                d = distance(ax, ay, nx, ny) + 4;
                if (r == RES_COPPER || r == RES_TIN) d -= 4;
                if (d >= best) continue;
                best = d;
                site_x = nx;
                site_y = ny;
            }
        }
    return best != 0xffff;
}

// **A merchant ship of a people that trades carries its deals** (trade.h):
// loaded, to the nearest coastal city of a partner; unloaded, home to the
// nearest coastal city of its own. Into (site_x, site_y); 0 if there is
// none.
static byte voyage(byte p, byte loaded, byte ax, byte ay)
{
    byte c, q;
    word d, best = 0xffff;

    for (c = 0; c < city_count; ++c) {
        q = city_owner[c];
        if (loaded ? q == p || q >= PLAYERS || !((trade_partners[p] >> q) & 1)
                   : q != p)
            continue;
        if (!city_coastal(c)) continue;
        d = distance(ax, ay, city_ax[c], city_ay[c]);
        if (d >= best) continue;
        best = d;
        site_x = city_ax[c];
        site_y = city_ay[c];
    }
    return best != 0xffff;
}

void emporium_ai(byte u)
{
    byte ax = unit_ax[u], ay = unit_ay[u], dir, nx, ny, p = unit_owner[u];

    // Trade first: next to the city it makes for, it waits for
    // trade_turn() to count the voyage; with no way there it explores,
    // and is kept for the trade rather than spent on an emporium.
    if (trade_partners[p]) {
        if (voyage(p, !unit_work[u], ax, ay)) {
            if (distance(ax, ay, site_x, site_y) <= 1) {
                unit_moves[u] = 0;
                return;
            }
            dir = sea_route(ax, ay, site_x, site_y);
            if (dir != DIR_NONE && map_neighbour(ax, ay, dir, &nx, &ny)
                && unit_can_enter(p, KIND_MERCHANT, nx, ny) == ENTER_OK) {
                unit_move_to(u, nx, ny);
                --unit_moves[u];
                return;
            }
        }
        boat_explore(u);
        return;
    }

    if (!find_site(p, ax, ay)) {
        boat_explore(u);
        return;
    }
    if (distance(ax, ay, site_x, site_y) == 1) {
        found(u, site_x, site_y);
        return;
    }
    dir = sea_route(ax, ay, site_x, site_y);
    if (dir != DIR_NONE && map_neighbour(ax, ay, dir, &nx, &ny)
        && unit_can_enter(p, KIND_MERCHANT, nx, ny) == ENTER_OK) {
        unit_move_to(u, nx, ny);
        --unit_moves[u];
        return;
    }
    // No way there, or somebody in it: look about meanwhile.
    boat_explore(u);
}

void emporium_turn(void)
{
    byte e, ax, ay, o, v;

    for (e = 0; e < emporium_count; ++e) {
        ax = emporium_ax[e];
        ay = emporium_ay[e];
        o = emporium_owner[e];
        if (!(map_tile[ay][ax] & MAP_OCCUPIED)) continue;
        v = unit_at(ax, ay);
        if (v == UNIT_NONE) continue;           // a city founded on it
        v = unit_owner[v];
        if (v == o || !at_war(o, v)) continue;
        if (o == PLAYER_HUMAN) message("Emporium", "burnt");
        map_tile[ay][ax] &= (byte)~MAP_IMPROVEMENT;
        // The last takes its place, and is looked at next.
        --emporium_count;
        emporium_owner[e] = emporium_owner[emporium_count];
        emporium_ax[e] = emporium_ax[emporium_count];
        emporium_ay[e] = emporium_ay[emporium_count];
        --e;
    }
}
