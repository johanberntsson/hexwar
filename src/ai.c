// In region 1 (src/region.h): called through the stubs in src/dispatch.s,
// under these names here.
#define ai_turn ai_turn_r1
#define ai_barbarians ai_barbarians_r1
// And for src/aibuild.c (aiint.h).
#define guards_wanted ai_guards_wanted_r1
#define find_far_site ai_find_far_site_r1
#define work_site ai_work_site_r1

#include "ai.h"
#include "aiint.h"
#include "city.h"
#include "combat.h"
#include "game.h"
#include "growth.h"
#include "improve.h"
#include "map.h"
#include "boat.h"
#include "emporium.h"
#include "path.h"
#include "sheet.h"
#include "unit.h"

#pragma clang section text="r1text" rodata="r1rodata"

// Expansion, after Civilization's production rules: a city builds a
// settler -- one at a time, and not counted against the unit cap, since it
// is gone once it has founded -- when it is guarded, has a citizen to spare
// (size 2; the settler takes one) and food to spare (more than 1 a turn),
// or the player has no settler at all, and has a site within SETTLE_RANGE
// hexagons: grassland, plains, floodplain or hills, SITE_LAND land
// neighbours or more, and at least CITY_SPACING hexagons from every city.
// No more than AI_CITY_CAP cities (aiint.h).
#define SETTLE_RANGE 6
#define CITY_SPACING 4
#define SITE_LAND 3

// Odds, as combat_worth() takes them: a computer player's warrior attacks
// when its attack is at least 3/4 of the defence, a barbarian at 1/4 --
// which is anything. Each makes for an enemy no further than its range.
#define AI_ODDS 3
#define AI_RANGE 8
#define CHARIOT_RANGE 16        // two moves a turn, and built for a war
#define BARB_ODDS 1
#define BARB_RANGE 24

// Barbarians appear from this turn on, one turn in BARB_CHANCE + 1 on
// average, no nearer anybody than BARB_SPACING, and never more than
// BARB_CAP of them at once.
#define BARB_FIRST_TURN 30
#define BARB_CHANCE 7
#define BARB_SPACING 6
#define BARB_CAP 8
#define BARB_TRIES 50

// Which way each exploring warrior is heading, DIR_*. Kept between turns so
// that it walks somewhere rather than jittering on the spot.
HIGH_BSS byte ai_heading[MAX_UNITS];

// **Where each player has been**, for its explorers: the map in blocks of
// SEEN_COLUMNS x SEEN_ROWS cells, 16 across and 8 down, a bit each --
// ai_seen[p][block column], bit block row. A block is seen once one of p's
// units or cities has been in it. ai_land says which blocks have any land
// at all, worked out by ai_place() (src/aiplace.c) from the map, which
// never changes.
#define SEEN_COLUMNS 8
#define SEEN_ROWS 4
#define SEEN_X (MAP_COLUMNS / SEEN_COLUMNS)
#if SEEN_X != 16 || MAP_ROWS / SEEN_ROWS > 8
#error "ai_seen in ai.h is 16 bytes a player, a bit a block row"
#endif
HIGH_BSS byte ai_seen[PLAYERS][SEEN_X];
HIGH_BSS byte ai_land[SEEN_X];
HIGH_BSS byte ai_sea[SEEN_X];   // the same for shallows, for the ships

// An explorer that has come no nearer its block in this many turns gives
// it up as out of reach -- over the water -- and counts it as seen.
#define PATIENCE 12

// Hexagons between two cells, round the east-west wrap. In doubled
// coordinates that is dy, plus whatever of dx the diagonal steps do not
// cover, two to a hexagon. 16-bit throughout; see the Calypsi notes.
static word distance(byte ax, byte ay, byte bx, byte by)
{
    int dx = (int)ax - (int)bx;
    int dy = (2 * (int)ay + (ax & 1)) - (2 * (int)by + (bx & 1));
    if (dx < 0) dx = -dx;
    if (dx > MAP_COLUMNS / 2) dx = MAP_COLUMNS - dx;
    if (dy < 0) dy = -dy;
    return (word)(dy + (dx > dy ? (dx - dy) / 2 : 0));
}

static byte land_neighbours(byte ax, byte ay)
{
    byte dir, nx, ny, n = 0, t;
    for (dir = 0; dir < 6; ++dir) {
        if (!map_neighbour(ax, ay, dir, &nx, &ny)) continue;
        t = MAP_TERRAIN(nx, ny);
        if (t != TILE_OCEAN && t != TILE_SHALLOW_OCEAN) ++n;
    }
    return n;
}

// The nearest unit or city to (ax, ay) whose owner `me` is at war with --
// or of any owner, for me = OWNER_ANY -- and how far it is. 0xffff with
// none.
#define OWNER_ANY 0xff
static byte near_x, near_y;

static byte enemy(byte me, byte owner)
{
    return me == OWNER_ANY || (owner != me && at_war(me, owner));
}

static word nearest(byte me, byte ax, byte ay)
{
    byte i;
    word d, best = 0xffff;
    for (i = 0; i < MAX_UNITS; ++i) {
        if (unit_kind[i] == KIND_NONE || !enemy(me, unit_owner[i])) continue;
        d = distance(ax, ay, unit_ax[i], unit_ay[i]);
        if (d < best) {
            best = d;
            near_x = unit_ax[i];
            near_y = unit_ay[i];
        }
    }
    for (i = 0; i < city_count; ++i) {
        if (!enemy(me, city_owner[i])) continue;
        d = distance(ax, ay, city_ax[i], city_ay[i]);
        if (d < best) {
            best = d;
            near_x = city_ax[i];
            near_y = city_ay[i];
        }
    }
    return best;
}

// What the resources a city on (ax, ay) would have are worth, on it and
// its neighbours: one each, and copper and tin two, for bronze. No two are
// neighbours (tools/mapgen.py), so at most three, and six.
static byte worth(byte ax, byte ay)
{
    byte r = map_resource_at(ax, ay);
    return r == RES_NONE ? 0 : r == RES_COPPER || r == RES_TIN ? 2 : 1;
}

static byte resources_round(byte ax, byte ay)
{
    byte dir, nx, ny, n = worth(ax, ay);
    for (dir = 0; dir < 6; ++dir)
        if (map_neighbour(ax, ay, dir, &nx, &ny)) n += worth(nx, ny);
    return n;
}

// The best site for a city within SETTLE_RANGE of (fx, fy), in
// (site_x, site_y); 0 if there is none. (fx, fy) itself may be occupied --
// by the settler looking -- and anywhere else may not. The nearest, but a
// resource in reach is worth half a hexagon's walk, and copper or tin a
// whole one: the score is twice the distance, plus six, less what the
// resources are worth (resources_round()).
static byte site_x, site_y;

#define NO_SITE 0x7fff
// With a galley a player looks FERRY_RANGE far for a site, over the water
// (find_far_site()); otherwise SETTLE_RANGE.
#define FERRY_RANGE 16
static byte site_range = SETTLE_RANGE;

static byte find_site(byte fx, byte fy)
{
    int dx, dy, y, rows = site_range / 2 + 1, columns = 2 * site_range;
    byte x, c, t, ok;
    word d, best = NO_SITE;

    for (dy = -rows; dy <= rows; ++dy) {
        y = (int)fy + dy;
        if (y < 2 || y > MAP_ROWS - 3) continue;    // as ai_place()
        // A hexagon east is two columns.
        for (dx = -columns; dx <= columns; ++dx) {
            x = (byte)((int)fx + dx + MAP_COLUMNS) & (MAP_COLUMNS - 1);
            t = MAP_TERRAIN(x, y);
            if (t != TILE_GRASS && t != TILE_PLAINS && t != TILE_FLOODPLAIN
                && t != TILE_HILLS) continue;
            d = distance(fx, fy, x, (byte)y);
            if (d > site_range || 2 * d >= best) continue;
            if ((map_tile[y][x] & MAP_OCCUPIED) && d) continue;
            if (land_neighbours(x, (byte)y) < SITE_LAND) continue;
            ok = 1;
            for (c = 0; c < city_count; ++c)
                if (distance(x, (byte)y, city_ax[c], city_ay[c])
                    < CITY_SPACING) {
                    ok = 0;
                    break;
                }
            if (!ok) continue;
            d = 2 * d + 6 - resources_round(x, (byte)y);
            if (d >= best) continue;
            best = d;
            site_x = x;
            site_y = (byte)y;
        }
    }
    return best != NO_SITE;
}


// One step of unit u in direction dir, if it may go there.
static byte step(byte u, byte dir)
{
    byte nx, ny;
    if (!map_neighbour(unit_ax[u], unit_ay[u], dir, &nx, &ny)) return 0;
    if (unit_can_enter(unit_owner[u], unit_kind[u], nx, ny) != ENTER_OK) return 0;
    unit_move_to(u, nx, ny);
    --unit_moves[u];
    return 1;
}

static byte home(byte u);
static void explore_out(byte u);
static void approach(byte u, byte bx, byte by);

static void seen(byte p, byte ax, byte ay)
{
    ai_seen[p][ax / SEEN_COLUMNS] |= (byte)(1 << (ay / SEEN_ROWS));
}

// The middle of the nearest block with land that p has not seen, from
// (ax, ay), into (block_x, block_y); 0 if p has seen them all.
static byte block_x, block_y;

static byte unseen_block(byte p, byte ax, byte ay)
{
    byte bx, by, cx, cy;
    word d, best = 0xffff;

    for (bx = 0; bx < SEEN_X; ++bx) {
        for (by = 0; by < MAP_ROWS / SEEN_ROWS; ++by) {
            if (!((ai_land[bx] >> by) & 1) || ((ai_seen[p][bx] >> by) & 1))
                continue;
            cx = bx * SEEN_COLUMNS + SEEN_COLUMNS / 2;
            cy = by * SEEN_ROWS + SEEN_ROWS / 2;
            d = distance(ax, ay, cx, cy);
            if (d < best) {
                best = d;
                block_x = cx;
                block_y = cy;
            }
        }
    }
    return best != 0xffff;
}

// **To the nearest land the player has not been to**, round water on the
// way (approach()), giving a block up after PATIENCE turns of coming no
// nearer; unit_work[] counts them, as it does a builder's turns. With
// everything seen, or for a barbarian, explore_out().
static void explore(byte u)
{
    byte p = unit_owner[u];
    word before;

    if (p == PLAYER_BARBARIAN || !unseen_block(p, unit_ax[u], unit_ay[u])) {
        explore_out(u);
        return;
    }
    before = distance(unit_ax[u], unit_ay[u], block_x, block_y);
    approach(u, block_x, block_y);
    if (unit_kind[u] == KIND_NONE) return;
    if (distance(unit_ax[u], unit_ay[u], block_x, block_y) < before) {
        unit_work[u] = 0;
    } else if (++unit_work[u] >= PATIENCE) {
        seen(p, block_x, block_y);
        unit_work[u] = 0;
    }
}

// **Outward**: of the steps it can take, the one that goes furthest from
// its nearest city, then the one on along its heading, then any, a random
// one of equals -- so that it strikes out and then follows the coast,
// rather than wandering round where it started. (A walk that only kept to
// a heading, turning now and then, saw 139 hexagons in 100 turns of
// autoplay.) A barbarian, with no city, keeps to its heading. Nowhere to
// go is a turn spent standing still. What explore() falls back on, and
// approach() when it is boxed in.
static void explore_out(byte u)
{
    byte d, nx, ny, best = DIR_NONE, c = home(u), r = rnd();
    byte heading = ai_heading[u] % 6;           // HIGH_BSS: not cleared
    word score, best_score = 0;

    for (d = 0; d < 6; ++d, r >>= 1) {
        if (!map_neighbour(unit_ax[u], unit_ay[u], d, &nx, &ny)) continue;
        if (unit_can_enter(unit_owner[u], unit_kind[u], nx, ny) != ENTER_OK) continue;
        score = 1 + (r & 1);
        if (d == heading) score += 2;
        if (c != CITY_NONE)
            score += 4 * distance(nx, ny, city_ax[c], city_ay[c]);
        if (score > best_score) {
            best_score = score;
            best = d;
        }
    }
    if (best == DIR_NONE || !step(u, best)) {
        unit_moves[u] = 0;
        return;
    }
    ai_heading[u] = best;
}

// The step from u's hexagon that comes nearest (bx, by), if any is nearer.
// If none is, water or somebody is in the way, and **it goes round**: a
// step that keeps the distance, the nearest to its heading and never
// straight back, which follows a coast. Only with none of those either
// does it explore. (Exploring at once, as it used to, led straight back
// the way it came, and a warrior paced between two hexagons for good.)
static void approach(byte u, byte bx, byte by)
{
    byte dir, nx, ny, best = DIR_NONE, side = DIR_NONE, turn, best_turn = 3;
    byte heading = ai_heading[u] % 6;
    word d, here = distance(unit_ax[u], unit_ay[u], bx, by), best_d = here;

    for (dir = 0; dir < 6; ++dir) {
        if (!map_neighbour(unit_ax[u], unit_ay[u], dir, &nx, &ny)) continue;
        if (unit_can_enter(unit_owner[u], unit_kind[u], nx, ny) != ENTER_OK) continue;
        d = distance(nx, ny, bx, by);
        if (d < best_d) {
            best_d = d;
            best = dir;
        } else if (d == here) {
            // How far round from the heading, 0 to 3; 3 is back.
            turn = (byte)((dir + 6 - heading) % 6);
            if (turn > 3) turn = 6 - turn;
            if (turn < best_turn) {
                best_turn = turn;
                side = dir;
            }
        }
    }
    if (best == DIR_NONE) best = side;
    if (best == DIR_NONE || !step(u, best)) {
        explore_out(u);
        return;
    }
    ai_heading[u] = best;
}

// p's city nearest u, or CITY_NONE.
static byte home(byte u)
{
    byte c, best = CITY_NONE;
    word d, best_d = 0xffff;
    for (c = 0; c < city_count; ++c) {
        if (city_owner[c] != unit_owner[u]) continue;
        d = distance(unit_ax[u], unit_ay[u], city_ax[c], city_ay[c]);
        if (d < best_d) {
            best_d = d;
            best = c;
        }
    }
    return best;
}

// How many of p's units other than u are fortified in city c.
static byte guards(byte p, byte u, byte c)
{
    byte v, n = 0;
    for (v = 0; v < MAX_UNITS; ++v)
        if (v != u && unit_kind[v] != KIND_NONE && unit_owner[v] == p
            && unit_ax[v] == city_ax[c] && unit_ay[v] == city_ay[c]
            && unit_orders[v] == ORDERS_FORTIFY)
            ++n;
    return n;
}

// Guards city c wants: one, and one more for every four citizens, as
// Civilization's computer players keep -- and one more with an enemy
// within THREAT_RANGE, its "defend" policy.
#define THREAT_RANGE 3

#define RECALL_RANGE 4

// **None before the barbarians come** (BARB_FIRST_TURN) unless an enemy is
// near: until then nobody attacks, and the warrior every people starts
// with explores instead of standing in its first city.
byte guards_wanted(byte c)
{
    byte n = 1 + (city_size[c] >> 2);
    // In the Crisis a city by the sea wants one more, for the Sea Peoples.
    if (game_turn >= GAME_CRISIS_TURN && city_coastal(c)) ++n;
    if (nearest(city_owner[c], city_ax[c], city_ay[c]) <= THREAT_RANGE)
        ++n;
    else if (game_turn < BARB_FIRST_TURN)
        n = 0;
    return n;
}

// Whether a spearman guards city c: fortified there. One that is only
// passing does not count, or the city went on building spearmen over the
// cap for one that never stayed (35 of them by turn 400, 22 Oct 2026).
static byte spearman_in(byte c)
{
    byte v;
    for (v = 0; v < MAX_UNITS; ++v)
        if (unit_kind[v] == KIND_SPEARMAN && unit_ax[v] == city_ax[c]
            && unit_ay[v] == city_ay[c] && unit_orders[v] == ORDERS_FORTIFY)
            return 1;
    return 0;
}

// Attack a neighbouring enemy if the odds are `odds` or better (see
// combat_worth()), never into the mountains with a chariot. Whether it did.
static byte attack_near(byte u, byte odds)
{
    byte dir, nx, ny, me = unit_owner[u];
    for (dir = 0; dir < 6; ++dir) {
        if (!map_neighbour(unit_ax[u], unit_ay[u], dir, &nx, &ny)) continue;
        if (!(map_tile[ny][nx] & MAP_OCCUPIED)) continue;
        if (unit_kind[u] == KIND_CHARIOT
            && MAP_TERRAIN(nx, ny) == TILE_MOUNTAIN) continue;
        if (!enemy(me, combat_holder(nx, ny))) continue;
        if (!combat_worth(unit_kind[u], nx, ny, odds)) continue;
        combat_attack(u, nx, ny);
        return 1;
    }
    return 0;
}

// Towards the nearest enemy within `range`, or exploring if there is none.
// **One that comes no nearer in FERRY_PATIENCE turns asks for the galley**
// (ferry()), as a settler does: the enemy is over the water.
static byte first_of(byte p, byte kind, byte work);
#define FERRY_PATIENCE 6
#define WANTS_FERRY 0xff

static void hunt(byte u, word range)
{
    byte p = unit_owner[u];
    word d = nearest(p, unit_ax[u], unit_ay[u]);

    if (d > range) {
        explore(u);
        return;
    }
    approach(u, near_x, near_y);
    if (unit_kind[u] == KIND_NONE || p == PLAYER_BARBARIAN) return;
    if (nearest(p, unit_ax[u], unit_ay[u]) < d)
        unit_work[u] = 0;
    else if (unit_work[u] < FERRY_PATIENCE)
        ++unit_work[u];
    else if (first_of(p, KIND_GALLEY, 0) != UNIT_NONE)
        unit_work[u] = WANTS_FERRY;
}

// A hexagon round city c that wants improving and has nobody on it, into
// (work_x, work_y): copper or tin to mine first, for bronze; then one the
// city works, as those are what an improvement adds to; then any. 0 if
// there is none.
static byte work_x, work_y;

byte work_site(byte c)
{
    byte pass, dir, nx, ny, w;

    city_work(c);
    for (pass = 0; pass < 3; ++pass) {
        for (dir = 0; dir < 6; ++dir) {
            if (pass == 1 && !((work_mask >> dir) & 1)) continue;
            if (!map_neighbour(city_ax[c], city_ay[c], dir, &nx, &ny))
                continue;
            if (map_tile[ny][nx] & MAP_OCCUPIED) continue;
            if (!pass) {
                w = map_resource_at(nx, ny);
                if (w != RES_COPPER && w != RES_TIN) continue;
            }
            if (improve_best(nx, ny) == ORDERS_NONE) continue;
            work_x = nx;
            work_y = ny;
            return 1;
        }
    }
    return 0;
}

// Copper or tin next to one of p's cities and not mined yet, the nearest
// to (ax, ay), into (work_x, work_y): bronze, which any of p's builders goes
// for before the work round its own city. Nobody may be on it but the
// builder looking. Whether there is one.
static byte strategic_site(byte p, byte ax, byte ay)
{
    byte c, dir, nx, ny, r;
    word d, best = 0xffff;

    for (c = 0; c < city_count; ++c) {
        if (city_owner[c] != p) continue;
        for (dir = 0; dir < 6; ++dir) {
            if (!map_neighbour(city_ax[c], city_ay[c], dir, &nx, &ny))
                continue;
            r = map_resource_at(nx, ny);
            if (r != RES_COPPER && r != RES_TIN) continue;
            if (!improve_can(ORDERS_MINE, nx, ny)) continue;
            if ((map_tile[ny][nx] & MAP_OCCUPIED) && (nx != ax || ny != ay))
                continue;
            d = distance(ax, ay, nx, ny);
            if (d >= best) continue;
            best = d;
            work_x = nx;
            work_y = ny;
        }
    }
    return best != 0xffff;
}

// Builder u towards a hexagon round city c that wants improving. Whether
// there was one.
static byte to_work(byte u, byte c)
{
    if (!work_site(c)) return 0;
    approach(u, work_x, work_y);
    return 1;
}

// A site for player p within reach of (fx, fy): SETTLE_RANGE, or with a
// galley FERRY_RANGE, over the water.

byte find_far_site(byte p, byte fx, byte fy)
{
    byte found = find_site(fx, fy);
    if (!found && first_of(p, KIND_GALLEY, 0) != UNIT_NONE) {
        site_range = FERRY_RANGE;
        found = find_site(fx, fy);
        site_range = SETTLE_RANGE;
    }
    return found;
}

// ------------------------------------------------------------- ferrying

// **Galleys ferry settlers** that cannot walk to their site -- it is over
// the water -- as Civilization's computer players ferry theirs, but
// without its continent numbers: a settler that has come no nearer its
// site in FERRY_PATIENCE turns, while its people has a galley, asks for
// one (unit_work[] = WANTS_FERRY) and waits on the coast; the galley comes
// alongside and the settler boards it; the galley then sails for the
// nearest site from the sea (find_site() from where it is), and next to
// land within reach of it wakes the settler, who steps ashore and walks
// on. **Armies too** (22 Oct 2026): a warrior or spearman that comes no
// nearer the enemy it hunts asks in the same way (hunt()), and the galley
// takes it to the nearest city of a people its owner is at war with
// (enemy_city()), where it goes ashore and attacks as it would on land.
// One galley a people.
#define LANDING_RANGE 2

static byte is_water(byte ax, byte ay)
{
    byte t = MAP_TERRAIN(ax, ay);
    return t == TILE_OCEAN || t == TILE_SHALLOW_OCEAN;
}

static byte next_to_land(byte ax, byte ay)
{
    byte dir, nx, ny;
    for (dir = 0; dir < 6; ++dir)
        if (map_neighbour(ax, ay, dir, &nx, &ny) && !is_water(nx, ny))
            return 1;
    return 0;
}

static byte next_to_water(byte ax, byte ay)
{
    byte dir, nx, ny;
    for (dir = 0; dir < 6; ++dir)
        if (map_neighbour(ax, ay, dir, &nx, &ny) && is_water(nx, ny))
            return 1;
    return 0;
}

// p's first unit of `kind`, with unit_work[] `work` if that is not 0.
static byte first_of(byte p, byte kind, byte work)
{
    byte v;
    for (v = 0; v < MAX_UNITS; ++v)
        if (unit_kind[v] == kind && unit_owner[v] == p
            && (!work || unit_work[v] == work))
            return v;
    return UNIT_NONE;
}

// Land units aboard galley u.
static byte cargo(byte u)
{
    byte v, n = 0;
    if (!is_water(unit_ax[u], unit_ay[u])) return 0;
    for (v = 0; v < MAX_UNITS; ++v)
        if (unit_kind[v] != KIND_NONE && !KIND_IS_SHIP(unit_kind[v])
            && unit_ax[v] == unit_ax[u] && unit_ay[v] == unit_ay[u])
            ++n;
    return n;
}

// Settler or soldier u, waiting for a ferry: aboard a galley next to it, or towards
// the galley until it is on the coast. With the galley gone it gives up.
static void ferry_wait(byte u)
{
    byte dir, nx, ny, g = first_of(unit_owner[u], KIND_GALLEY, 0);

    if (g == UNIT_NONE) {
        unit_work[u] = 0;
        explore(u);
        return;
    }
    for (dir = 0; dir < 6; ++dir)
        if (map_neighbour(unit_ax[u], unit_ay[u], dir, &nx, &ny)
            && unit_can_enter(unit_owner[u], unit_kind[u], nx, ny)
                   == ENTER_BOARD) {
            unit_move_to(u, nx, ny);        // aboard, and sentried
            unit_work[u] = 0;
            unit_moves[u] = 0;
            return;
        }
    if (!is_water(unit_ax[u], unit_ay[u])
        && next_to_water(unit_ax[u], unit_ay[u])) {
        unit_moves[u] = 0;                  // on the coast: wait
        return;
    }
    approach(u, unit_ax[g], unit_ay[g]);
}

// Settler u, aboard and woken next to land: ashore, on the land next to it
// nearest its site.
static void go_ashore(byte u)
{
    byte dir, nx, ny, best = DIR_NONE;
    word d, best_d = 0xffff;

    if (find_site(unit_ax[u], unit_ay[u]))
        for (dir = 0; dir < 6; ++dir) {
            if (!map_neighbour(unit_ax[u], unit_ay[u], dir, &nx, &ny)
                || unit_can_enter(unit_owner[u], KIND_SETTLER, nx, ny)
                       != ENTER_OK)
                continue;
            d = distance(nx, ny, site_x, site_y);
            if (d < best_d) {
                best_d = d;
                best = dir;
            }
        }
    if (best == DIR_NONE || !step(u, best)) unit_moves[u] = 0;
}

// One step of galley u towards (tx, ty) along the shallows (sea_route(),
// src/path.c, rather than approach(), which got stuck behind every
// peninsula). Whether it went.
static byte sail(byte u, byte tx, byte ty)
{
    byte dir = sea_route(unit_ax[u], unit_ay[u], tx, ty);
    return dir != DIR_NONE && step(u, dir);
}

// The nearest city to (ax, ay) of a people p is at war with, the rebels'
// included, into (site_x, site_y). Whether there is one.
static byte enemy_city(byte p, byte ax, byte ay)
{
    byte c;
    word d, best = 0xffff;
    for (c = 0; c < city_count; ++c) {
        if (city_owner[c] == p || !at_war(p, city_owner[c])) continue;
        d = distance(ax, ay, city_ax[c], city_ay[c]);
        if (d < best) {
            best = d;
            site_x = city_ax[c];
            site_y = city_ay[c];
        }
    }
    return best != 0xffff;
}

// Where galley u's cargo is going, into (site_x, site_y): a settler's
// site, or a soldier's enemy. Whether there is anywhere.
static byte destination(byte u)
{
    byte v, p = unit_owner[u], ax = unit_ax[u], ay = unit_ay[u];
    for (v = 0; v < MAX_UNITS; ++v)
        if (unit_kind[v] == KIND_SETTLER && unit_ax[v] == ax
            && unit_ay[v] == ay)
            return find_far_site(p, ax, ay);
    return enemy_city(p, ax, ay);
}

// p's first land unit waiting for the galley, or UNIT_NONE.
static byte waiting(byte p)
{
    byte v;
    for (v = 0; v < MAX_UNITS; ++v)
        if (unit_kind[v] != KIND_NONE && !KIND_IS_SHIP(unit_kind[v])
            && unit_owner[v] == p && unit_work[v] == WANTS_FERRY)
            return v;
    return UNIT_NONE;
}

// Galley u: with cargo aboard, to where it is going (destination());
// without, to a settler or a soldier waiting for it; with nobody to
// ferry, exploring the coasts (boat_explore()). One it cannot reach is
// let go, to walk or to ask again later.
static void ferry(byte u)
{
    byte s, p = unit_owner[u], ax = unit_ax[u], ay = unit_ay[u];

    if (cargo(u)) {
        if (destination(u)) {
            if (distance(ax, ay, site_x, site_y) <= LANDING_RANGE
                && next_to_land(ax, ay)) {
                unit_wake_cargo(u);
                unit_moves[u] = 0;
                return;
            }
            if (sail(u, site_x, site_y)) return;
        }
        boat_explore(u);
        return;
    }
    s = waiting(p);
    // Not until it is on the coast: the route to an inland hexagon fails,
    // and would let it go before it got there.
    if (s == UNIT_NONE || !next_to_water(unit_ax[s], unit_ay[s])) {
        boat_explore(u);
        return;
    }
    if (distance(ax, ay, unit_ax[s], unit_ay[s]) == 1 && is_water(ax, ay)) {
        unit_moves[u] = 0;                  // alongside: it boards
    } else if (!sail(u, unit_ax[s], unit_ay[s])) {
        unit_work[s] = 0;                   // out of reach
        boat_explore(u);
    }
}

static void move_unit(byte u)
{
    byte c, w, ax = unit_ax[u], ay = unit_ay[u];
    word d;

    if (unit_owner[u] == PLAYER_BARBARIAN) {
        if (!attack_near(u, BARB_ODDS)) hunt(u, BARB_RANGE);
        return;
    }
    switch (unit_kind[u]) {
    case KIND_SETTLER:
        // 1. With no city, found one where it stands; it was put on good
        // land. A settler built later makes for the nearest site and
        // founds a city there, or explores if there is none in reach --
        // or, coming no nearer it, waits for a galley (ferry_wait()).
        if (is_water(ax, ay)) {
            go_ashore(u);
            return;
        }
        if (home(u) != CITY_NONE) {
            if (!find_far_site(unit_owner[u], ax, ay)) {
                explore(u);
                return;
            }
            if (site_x != ax || site_y != ay) {
                if (unit_work[u] == WANTS_FERRY) {
                    ferry_wait(u);
                    return;
                }
                d = distance(ax, ay, site_x, site_y);
                approach(u, site_x, site_y);
                if (distance(unit_ax[u], unit_ay[u], site_x, site_y) < d)
                    unit_work[u] = 0;
                else if (unit_work[u] < FERRY_PATIENCE)
                    ++unit_work[u];
                else if (first_of(unit_owner[u], KIND_GALLEY, 0)
                         != UNIT_NONE)
                    unit_work[u] = WANTS_FERRY;
                return;
            }
        }
        if (city_found(unit_owner[u], ax, ay) != CITY_NONE) {
            unit_remove(u);
            return;
        }
        explore(u);
        break;
    case KIND_WARRIOR:
    case KIND_SPEARMAN:
    case KIND_CHARIOT:
        // A fair fight next to it first. Then a warrior in a city short of
        // guards (guards_wanted()) stays to guard it, and one within
        // RECALL_RANGE of it goes back; 4. the rest hunt the enemy nearby
        // or explore. Further out they do not come back: the city builds
        // its own guard, or every explorer would be called home as soon as
        // a city grew. A chariot, defence 1, only attacks and hunts.
        if (attack_near(u, AI_ODDS)) return;
        if (unit_work[u] == WANTS_FERRY) {
            // Waiting for the galley, while there is still a war to go to.
            if (enemy_city(unit_owner[u], ax, ay)) {
                ferry_wait(u);
                return;
            }
            unit_work[u] = 0;
        }
        c = home(u);
        // A spearman also goes home to a city with none, to take the
        // warriors' place (retire(), src/aibuild.c).
        if (c != CITY_NONE && unit_kind[u] != KIND_CHARIOT
            && distance(ax, ay, city_ax[c], city_ay[c]) <= RECALL_RANGE
            && (guards(unit_owner[u], u, c) < guards_wanted(c)
                || (unit_kind[u] == KIND_SPEARMAN && !spearman_in(c)))) {
            if (ax == city_ax[c] && ay == city_ay[c]) {
                unit_orders[u] = ORDERS_FORTIFY;
                unit_moves[u] = 0;
            } else {
                approach(u, city_ax[c], city_ay[c]);
            }
            return;
        }
        hunt(u, unit_kind[u] == KIND_CHARIOT ? CHARIOT_RANGE : AI_RANGE);
        break;
    case KIND_GALLEY:
        // Galleys ferry settlers (ferry()), and otherwise explore the
        // coasts, to the land nobody of theirs has seen, which is how a
        // people on an island meets the others.
        ferry(u);
        break;
    case KIND_BOAT:
        // A fishing boat explores the coasts, which is how a people meets
        // the others across the water before it has cedar for a galley.
        boat_explore(u);
        break;
    case KIND_MERCHANT:
        // To the coast by a good its people lacks, and an emporium there.
        emporium_ai(u);
        break;
    case KIND_BUILDER:
        // 5. Copper and tin next to any of its people's cities first, for
        // bronze (strategic_site()). Then improve the hexagons round its
        // city (improve_best()): where it stands if it is one of them and
        // wants something, or else the first of them that wants something
        // and has nobody on it. With all of them done it waits next to the
        // city.
        if (strategic_site(unit_owner[u], ax, ay)) {
            if (work_x == ax && work_y == ay)
                improve_start(u, ORDERS_MINE);
            else
                approach(u, work_x, work_y);
            return;
        }
        c = home(u);
        if (c == CITY_NONE) {
            explore(u);
            return;
        }
        if (distance(ax, ay, city_ax[c], city_ay[c]) == 1) {
            w = improve_best(ax, ay);
            if (w != ORDERS_NONE) {
                improve_start(u, w);
                return;
            }
        }
        if (to_work(u, c)) return;
        if (ax == city_ax[c] && ay == city_ay[c])
            explore(u);
        else if (distance(ax, ay, city_ax[c], city_ay[c]) > 1)
            approach(u, city_ax[c], city_ay[c]);
        else
            unit_moves[u] = 0;
        break;
    }
}

void ai_turn(byte p)
{
    byte c, u;

    if (!player_alive(p)) return;
    unit_begin_turn(p);

    for (u = 0; u < MAX_UNITS; ++u)
        if (unit_kind[u] != KIND_NONE && unit_owner[u] == p)
            seen(p, unit_ax[u], unit_ay[u]);
    for (c = 0; c < city_count; ++c)
        if (city_owner[c] == p) seen(p, city_ax[c], city_ay[c]);

    // What each city builds: src/aibuild.c, in region 2.
    ai_build(p);

    for (u = 0; u < MAX_UNITS; ++u) {
        if (unit_kind[u] == KIND_NONE || unit_owner[u] != p) continue;
        if (unit_orders[u] != ORDERS_NONE) continue;
        while (unit_kind[u] != KIND_NONE && unit_moves[u]) move_unit(u);
    }
}

// Land with nobody on it.
static byte empty_land(byte ax, byte ay)
{
    return unit_can_enter(PLAYER_BARBARIAN, KIND_WARRIOR, ax, ay) == ENTER_OK
           && !(map_tile[ay][ax] & MAP_OCCUPIED);
}

void ai_barbarians(word turn)
{
    byte u, c, ax, ay, tries, dir, nx, ny;

    unit_begin_turn(PLAYER_BARBARIAN);
    if (turn >= BARB_FIRST_TURN && !(rnd() & BARB_CHANCE)
        && unit_count_kind(PLAYER_BARBARIAN, KIND_NONE) < BARB_CAP) {
        for (tries = 0; tries < BARB_TRIES; ++tries) {
            ax = rnd() & (MAP_COLUMNS - 1);
            ay = 2 + rnd() % (MAP_ROWS - 4);
            if (!empty_land(ax, ay)
                || nearest(OWNER_ANY, ax, ay) < BARB_SPACING)
                continue;
            unit_create(KIND_WARRIOR, PLAYER_BARBARIAN, ax, ay);
            for (dir = 0; dir < 6; ++dir) {
                if (map_neighbour(ax, ay, dir, &nx, &ny)
                    && empty_land(nx, ny)) {
                    unit_create(KIND_WARRIOR, PLAYER_BARBARIAN, nx, ny);
                    break;
                }
            }
            break;
        }
    }

    // **The rebels' cities** (src/crisis.c, a people's collapse or a
    // revolt) keep a guard: they build a warrior while nobody is fortified
    // in them, the first barbarian in one stays as its guard, and a guard
    // stays put. The rest go raiding as barbarians do.
    for (c = 0; c < city_count; ++c)
        if (city_owner[c] == PLAYER_BARBARIAN)
            city_build[c] = guards(PLAYER_BARBARIAN, UNIT_NONE, c)
                            ? KIND_NONE : KIND_WARRIOR;
    for (u = 0; u < MAX_UNITS; ++u) {
        if (unit_owner[u] != PLAYER_BARBARIAN) continue;
        c = city_at(unit_ax[u], unit_ay[u]);
        if (c != CITY_NONE && unit_kind[u] != KIND_NONE
            && !KIND_IS_SHIP(unit_kind[u])) {
            if (unit_orders[u] == ORDERS_FORTIFY) continue;
            if (!guards(PLAYER_BARBARIAN, u, c)) {
                unit_orders[u] = ORDERS_FORTIFY;
                continue;
            }
        }
        while (unit_kind[u] != KIND_NONE && unit_moves[u]) move_unit(u);
    }
}
