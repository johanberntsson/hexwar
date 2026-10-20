// In region 1 (src/region.h): called through the stubs in src/dispatch.s,
// under these names here.
#define ai_place ai_place_r1
#define ai_turn ai_turn_r1
#define ai_barbarians ai_barbarians_r1

#include "ai.h"
#include "city.h"
#include "combat.h"
#include "game.h"
#include "growth.h"
#include "improve.h"
#include "map.h"
#include "sheet.h"
#include "unit.h"

#pragma clang section text="r1text" rodata="r1rodata"

// How far apart the starting settlers should be, in hexagons, and how many
// tries each spacing gets before the next, closer one is accepted.
#define START_SPACING 16
#define START_SPACING_STEP 4
#define START_TRIES 200

// A computer player stops building units, settlers apart, at AI_UNIT_BASE
// plus AI_UNITS_A_CITY for each city, up to AI_UNIT_MAX -- a guard and a
// builder a city and a few to explore -- so that three of them and the
// barbarians cannot fill the 64 slots every player shares.
#define AI_UNIT_BASE 3
#define AI_UNITS_A_CITY 2
#define AI_UNIT_MAX 12

// Expansion, after Civilization's production rules: a city builds a
// settler -- one at a time, and not counted against the unit cap, since it
// is gone once it has founded -- when it is guarded, has a citizen to spare
// (size 2; the settler takes one) and food to spare (more than 1 a turn),
// or the player has no settler at all, and has a site within SETTLE_RANGE
// hexagons: grassland, plains, floodplain or hills, SITE_LAND land
// neighbours or more, and at least CITY_SPACING hexagons from every city.
// No more than AI_CITY_CAP cities; MAX_CITIES is shared with the human.
#define SETTLE_RANGE 6
#define CITY_SPACING 4
#define SITE_LAND 3
#define AI_CITY_CAP 6

// Odds, as combat_worth() takes them: a computer player's warrior attacks
// when its attack is at least 3/4 of the defence, a barbarian at 1/4 --
// which is anything. Each makes for an enemy no further than its range.
#define AI_ODDS 3
#define AI_RANGE 8
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
// at all, worked out by ai_place() from the map, which never changes.
#define SEEN_COLUMNS 8
#define SEEN_ROWS 4
#define SEEN_X (MAP_COLUMNS / SEEN_COLUMNS)
#if SEEN_X != 16 || MAP_ROWS / SEEN_ROWS > 8
#error "ai_seen in ai.h is 16 bytes a player, a bit a block row"
#endif
HIGH_BSS byte ai_seen[PLAYERS][SEEN_X];
HIGH_BSS static byte ai_land[SEEN_X];

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

// The nearest site for a city within SETTLE_RANGE of (fx, fy), in
// (site_x, site_y); 0 if there is none. (fx, fy) itself may be occupied --
// by the settler looking -- and anywhere else may not.
static byte site_x, site_y;

static byte find_site(byte fx, byte fy)
{
    int dx, dy, y;
    byte x, c, t, ok;
    word d, best = 0xffff;

    for (dy = -(SETTLE_RANGE / 2 + 1); dy <= SETTLE_RANGE / 2 + 1; ++dy) {
        y = (int)fy + dy;
        if (y < 2 || y > MAP_ROWS - 3) continue;    // as ai_place()
        // A hexagon east is two columns.
        for (dx = -2 * SETTLE_RANGE; dx <= 2 * SETTLE_RANGE; ++dx) {
            x = (byte)((int)fx + dx + MAP_COLUMNS) & (MAP_COLUMNS - 1);
            d = distance(fx, fy, x, (byte)y);
            if (d > SETTLE_RANGE || d >= best) continue;
            t = MAP_TERRAIN(x, y);
            if (t != TILE_GRASS && t != TILE_PLAINS && t != TILE_FLOODPLAIN
                && t != TILE_HILLS) continue;
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
            best = d;
            site_x = x;
            site_y = (byte)y;
        }
    }
    return best != 0xffff;
}

void ai_place(void)
{
    byte p, ax, ay, tries, t;
    word spacing;

    // Nobody has been anywhere yet; and where the land is.
    for (ax = 0; ax < SEEN_X; ++ax) {
        for (p = 0; p < PLAYERS; ++p) ai_seen[p][ax] = 0;
        ai_land[ax] = 0;
    }
    for (ay = 0; ay < MAP_ROWS; ++ay)
        for (ax = 0; ax < MAP_COLUMNS; ++ax) {
            t = MAP_TERRAIN(ax, ay);
            if (t != TILE_OCEAN && t != TILE_SHALLOW_OCEAN)
                ai_land[ax / SEEN_COLUMNS] |= (byte)(1 << (ay / SEEN_ROWS));
        }

    for (p = PLAYER_FIRST_AI; p < PLAYERS; ++p) {
        for (spacing = START_SPACING;; spacing -= START_SPACING_STEP) {
            for (tries = 0; tries < START_TRIES; ++tries) {
                ax = rnd() & (MAP_COLUMNS - 1);
                ay = 2 + rnd() % (MAP_ROWS - 4);    // not on the edge rows
                if (MAP_TERRAIN(ax, ay) == TILE_GRASS
                    && !(map_tile[ay][ax] & MAP_OCCUPIED)
                    && land_neighbours(ax, ay) >= 4
                    && nearest(OWNER_ANY, ax, ay) >= spacing)
                    goto found;
            }
            if (spacing <= START_SPACING_STEP) break;   // no room: none
        }
        continue;
found:
        unit_create(KIND_SETTLER, p, ax, ay);
        unit_create(KIND_WARRIOR, p, ax, ay);   // as the human's, game.c
    }
}

// One step of unit u in direction dir, if it may go there.
static byte step(byte u, byte dir)
{
    byte nx, ny;
    if (!map_neighbour(unit_ax[u], unit_ay[u], dir, &nx, &ny)) return 0;
    if (unit_can_enter(unit_owner[u], nx, ny) != ENTER_OK) return 0;
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
        if (unit_can_enter(unit_owner[u], nx, ny) != ENTER_OK) continue;
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
        if (unit_can_enter(unit_owner[u], nx, ny) != ENTER_OK) continue;
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

// Warriors a player keeps exploring, not guarding: one, and one more for
// every three cities. Without them the guards take every warrior built,
// and in 100 turns of autoplay the player had seen 139 of 1100 hexagons of
// a continent everybody shared, and met nobody.
#define EXPLORERS(CITIES) (1 + (CITIES) / 3)
#define RECALL_RANGE 4

// **None before the barbarians come** (BARB_FIRST_TURN) unless an enemy is
// near: until then nobody attacks, and the warrior every people starts
// with explores instead of standing in its first city.
static byte guards_wanted(byte c)
{
    byte n = 1 + (city_size[c] >> 2);
    if (nearest(city_owner[c], city_ax[c], city_ay[c]) <= THREAT_RANGE)
        ++n;
    else if (game_turn < BARB_FIRST_TURN)
        n = 0;
    return n;
}

// Attack a neighbouring enemy if the odds are `odds` or better (see
// combat_worth()). Whether it did.
static byte attack_near(byte u, byte odds)
{
    byte dir, nx, ny, me = unit_owner[u];
    for (dir = 0; dir < 6; ++dir) {
        if (!map_neighbour(unit_ax[u], unit_ay[u], dir, &nx, &ny)) continue;
        if (!(map_tile[ny][nx] & MAP_OCCUPIED)) continue;
        if (!enemy(me, combat_holder(nx, ny))) continue;
        if (!combat_worth(unit_kind[u], nx, ny, odds)) continue;
        combat_attack(u, nx, ny);
        return 1;
    }
    return 0;
}

// Towards the nearest enemy within `range`, or exploring if there is none.
static void hunt(byte u, word range)
{
    if (nearest(unit_owner[u], unit_ax[u], unit_ay[u]) <= range)
        approach(u, near_x, near_y);
    else
        explore(u);
}

// A hexagon round city c that wants improving and has nobody on it, into
// (work_x, work_y): one the city works if there is one, as those are what
// an improvement adds to. 0 if there is none.
static byte work_x, work_y;

static byte work_site(byte c)
{
    byte pass, dir, nx, ny;

    city_work(c);
    for (pass = 0; pass < 2; ++pass) {
        for (dir = 0; dir < 6; ++dir) {
            if (!pass && !((work_mask >> dir) & 1)) continue;
            if (!map_neighbour(city_ax[c], city_ay[c], dir, &nx, &ny))
                continue;
            if (map_tile[ny][nx] & MAP_OCCUPIED) continue;
            if (improve_best(nx, ny) == ORDERS_NONE) continue;
            work_x = nx;
            work_y = ny;
            return 1;
        }
    }
    return 0;
}

// Builder u towards a hexagon round city c that wants improving. Whether
// there was one.
static byte to_work(byte u, byte c)
{
    if (!work_site(c)) return 0;
    approach(u, work_x, work_y);
    return 1;
}

static void move_unit(byte u)
{
    byte c, w, ax = unit_ax[u], ay = unit_ay[u];

    if (unit_owner[u] == PLAYER_BARBARIAN) {
        if (!attack_near(u, BARB_ODDS)) hunt(u, BARB_RANGE);
        return;
    }
    switch (unit_kind[u]) {
    case KIND_SETTLER:
        // 1. With no city, found one where it stands; it was put on good
        // land. A settler built later makes for the nearest site and
        // founds a city there, or explores if there is none in reach.
        if (home(u) != CITY_NONE) {
            if (!find_site(ax, ay)) {
                explore(u);
                return;
            }
            if (site_x != ax || site_y != ay) {
                approach(u, site_x, site_y);
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
        // A fair fight next to it first. Then a warrior in a city short of
        // guards (guards_wanted()) stays to guard it, and one within
        // RECALL_RANGE of it goes back; 4. the rest hunt the enemy nearby
        // or explore. Further out they do not come back: the city builds
        // its own guard, or every explorer would be called home as soon as
        // a city grew.
        if (attack_near(u, AI_ODDS)) return;
        c = home(u);
        if (c != CITY_NONE
            && distance(ax, ay, city_ax[c], city_ay[c]) <= RECALL_RANGE
            && guards(unit_owner[u], u, c) < guards_wanted(c)) {
            if (ax == city_ax[c] && ay == city_ay[c]) {
                unit_orders[u] = ORDERS_FORTIFY;
                unit_moves[u] = 0;
            } else {
                approach(u, city_ax[c], city_ay[c]);
            }
            return;
        }
        hunt(u, AI_RANGE);
        break;
    case KIND_BUILDER:
        // 5. Improve the hexagons round its city (improve_best()): where it
        // stands if it is one of them and wants something, or else the
        // first of them that wants something and has nobody on it. With
        // all of them done it waits next to the city.
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
    byte c, u, cap, kind;
    byte cities = 0, settlers, builders, explorers = 0;
    byte units = unit_count_kind(p, KIND_NONE) - unit_count_kind(p, KIND_SETTLER);

    if (!player_alive(p)) return;
    unit_begin_turn(p);

    for (u = 0; u < MAX_UNITS; ++u)
        if (unit_kind[u] != KIND_NONE && unit_owner[u] == p)
            seen(p, unit_ax[u], unit_ay[u]);
    for (c = 0; c < city_count; ++c)
        if (city_owner[c] == p) seen(p, city_ax[c], city_ay[c]);

    for (c = 0; c < city_count; ++c)
        if (city_owner[c] == p) ++cities;
    cap = AI_UNIT_BASE + AI_UNITS_A_CITY * cities;
    if (cap > AI_UNIT_MAX) cap = AI_UNIT_MAX;
    settlers = unit_count_kind(p, KIND_SETTLER);
    builders = unit_count_kind(p, KIND_BUILDER);
    for (u = 0; u < MAX_UNITS; ++u)
        if (unit_kind[u] == KIND_WARRIOR && unit_owner[u] == p
            && unit_orders[u] != ORDERS_FORTIFY)
            ++explorers;

    // What each city builds, decided afresh every turn, after Civilization:
    // 1. guards first, while it has fewer than guards_wanted() -- with
    // none at all even over the cap; then explorers, warriors that are not
    // guarding, up to EXPLORERS(); 2. a settler from a city with a citizen
    // and food to spare, or from the first with a citizen if there is no
    // settler about, and a site in reach -- settlers out and cities together no more than
    // AI_CITY_CAP; then up to the cap 3. a builder for each city that has
    // hexagons to improve, and 4. warriors to explore and fight. Changing
    // keeps the shields, so nothing is lost by it.
    for (c = 0; c < city_count; ++c) {
        if (city_owner[c] != p) continue;
        u = guards(p, UNIT_NONE, c);
        city_work(c);
        if (u < guards_wanted(c) && (u == 0 || units < cap)) {
            kind = KIND_WARRIOR;
        } else if (explorers < EXPLORERS(cities) && units < cap) {
            kind = KIND_WARRIOR;
            ++explorers;
        } else if (cities + settlers < AI_CITY_CAP && city_size[c] >= 2
                   && (work_food > 2 * city_size[c] + 1 || !settlers)
                   && find_site(city_ax[c], city_ay[c])) {
            kind = KIND_SETTLER;
            ++settlers;
        } else if (units >= cap) {
            kind = KIND_NONE;
        } else if (builders < cities && work_site(c)) {
            kind = KIND_BUILDER;
            ++builders;
        } else {
            kind = KIND_WARRIOR;
        }
        city_build[c] = kind;
    }

    for (u = 0; u < MAX_UNITS; ++u) {
        if (unit_kind[u] == KIND_NONE || unit_owner[u] != p) continue;
        if (unit_orders[u] != ORDERS_NONE) continue;
        while (unit_kind[u] != KIND_NONE && unit_moves[u]) move_unit(u);
    }
}

// Land with nobody on it.
static byte empty_land(byte ax, byte ay)
{
    return unit_can_enter(PLAYER_BARBARIAN, ax, ay) == ENTER_OK
           && !(map_tile[ay][ax] & MAP_OCCUPIED);
}

void ai_barbarians(word turn)
{
    byte u, ax, ay, tries, dir, nx, ny;

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

    for (u = 0; u < MAX_UNITS; ++u) {
        if (unit_owner[u] != PLAYER_BARBARIAN) continue;
        while (unit_kind[u] != KIND_NONE && unit_moves[u]) move_unit(u);
    }
}
