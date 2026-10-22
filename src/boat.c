// In region 4 (src/region.h): called through the stub in src/dispatch.s,
// under this name here.
#define boat_explore boat_explore_r4
#define boat_fish boat_fish_r4
#define boat_order boat_order_r4

#include "boat.h"
#include "ai.h"
#include "city.h"
#include "gameint.h"
#include "map.h"
#include "path.h"
#include "sheet.h"
#include "unit.h"

#pragma clang section text="r4text" rodata="r4rodata"

// ai.c's blocks: SEEN_COLUMNS x SEEN_ROWS cells, a bit each.
#define SEEN_COLUMNS 8
#define SEEN_ROWS 4

// A few blocks a move at most may turn out out of reach before it gives
// up for the turn: each costs a search.
#define TRIES 4

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

// The shallows of the nearest block with any that p has not seen, from
// (ax, ay): the block in (block_x, block_y) and its shallows nearest
// (ax, ay) in (sea_x, sea_y). 0 if p has seen them all.
static byte block_x, block_y, sea_x, sea_y;

static byte unseen_sea(byte p, byte ax, byte ay)
{
    byte bx, by, x, y;
    word d, best = 0xffff;

    for (bx = 0; bx < 16; ++bx) {
        for (by = 0; by < MAP_ROWS / SEEN_ROWS; ++by) {
            if (!((ai_sea[bx] >> by) & 1) || ((ai_seen[p][bx] >> by) & 1))
                continue;
            for (y = by * SEEN_ROWS; y < (by + 1) * SEEN_ROWS; ++y)
                for (x = bx * SEEN_COLUMNS; x < (bx + 1) * SEEN_COLUMNS; ++x) {
                    if (MAP_TERRAIN(x, y) != TILE_SHALLOW_OCEAN) continue;
                    d = distance(ax, ay, x, y);
                    if (d >= best) continue;
                    best = d;
                    block_x = bx;
                    block_y = by;
                    sea_x = x;
                    sea_y = y;
                }
        }
    }
    return best != 0xffff;
}

byte boat_fish(byte u)
{
    byte ax = unit_ax[u], ay = unit_ay[u];

    if (unit_kind[u] != KIND_BOAT || map_resource_at(ax, ay) != RES_FISH
        || (map_tile[ay][ax] & MAP_IRRIGATED))
        return 0;
    map_tile[ay][ax] |= MAP_IRRIGATED;
    unit_remove(u);
    return 1;
}

void boat_order(byte u)
{
    if (!boat_fish(u)) {
        message("No fish", "to work here");
        return;
    }
    message("Fishing boats", "at work");
    dirty = 1;
    hover_dirty = 1;
    select_unit(u);
}

// Fish next to one of p's cities that is not worked and has nobody on it
// but u, the nearest to u, into (sea_x, sea_y). 0 if there is none.
static byte fish_site(byte p, byte u)
{
    byte c, dir, nx, ny;
    word d, best = 0xffff;

    for (c = 0; c < city_count; ++c) {
        if (city_owner[c] != p) continue;
        for (dir = 0; dir < 6; ++dir) {
            if (!map_neighbour(city_ax[c], city_ay[c], dir, &nx, &ny)
                || map_resource_at(nx, ny) != RES_FISH
                || (map_tile[ny][nx] & MAP_IRRIGATED))
                continue;
            if ((map_tile[ny][nx] & MAP_OCCUPIED)
                && (nx != unit_ax[u] || ny != unit_ay[u]))
                continue;
            d = distance(unit_ax[u], unit_ay[u], nx, ny);
            if (d >= best) continue;
            best = d;
            sea_x = nx;
            sea_y = ny;
        }
    }
    return best != 0xffff;
}

// p's first fishing boat, which explores while there is anything to.
static byte first_boat(byte p)
{
    byte v;
    for (v = 0; v < MAX_UNITS; ++v)
        if (unit_kind[v] == KIND_BOAT && unit_owner[v] == p) return v;
    return UNIT_NONE;
}

// One step of u towards (sea_x, sea_y), or onto it from next to it.
static byte sail_to(byte u)
{
    byte dir = sea_route(unit_ax[u], unit_ay[u], sea_x, sea_y), nx, ny;

    if (dir == DIR_NONE)
        dir = map_direction(unit_ax[u], unit_ay[u], sea_x, sea_y);
    if (dir == DIR_NONE
        || !map_neighbour(unit_ax[u], unit_ay[u], dir, &nx, &ny)
        || unit_can_enter(unit_owner[u], unit_kind[u], nx, ny) != ENTER_OK)
        return 0;
    unit_move_to(u, nx, ny);
    --unit_moves[u];
    return 1;
}

void boat_explore(byte u)
{
    byte p = unit_owner[u], dir, nx, ny, tries;

    if (unit_kind[u] == KIND_BOAT
        && (u != first_boat(p) || !unseen_sea(p, unit_ax[u], unit_ay[u]))
        && fish_site(p, u)) {
        if (unit_ax[u] == sea_x && unit_ay[u] == sea_y) {
            boat_fish(u);
            return;
        }
        if (sail_to(u)) return;
        unit_moves[u] = 0;          // in the way, or no way there yet
        return;
    }
    for (tries = 0; tries < TRIES; ++tries) {
        if (!unseen_sea(p, unit_ax[u], unit_ay[u])) break;
        dir = sea_route(unit_ax[u], unit_ay[u], sea_x, sea_y);
        if (dir != DIR_NONE
            && map_neighbour(unit_ax[u], unit_ay[u], dir, &nx, &ny)
            && unit_can_enter(p, unit_kind[u], nx, ny) == ENTER_OK) {
            unit_move_to(u, nx, ny);
            --unit_moves[u];
            return;
        }
        // There, next to it, or no way: seen, as far as a ship can see.
        ai_seen[p][block_x] |= (byte)(1 << block_y);
    }
    unit_moves[u] = 0;
}
