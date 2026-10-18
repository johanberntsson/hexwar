#include "ai.h"
#include "city.h"
#include "combat.h"
#include "map.h"
#include "sheet.h"
#include "unit.h"

// How far apart the starting settlers should be, in hexagons, and how many
// tries each spacing gets before the next, closer one is accepted.
#define START_SPACING 16
#define START_SPACING_STEP 4
#define START_TRIES 200

// A computer player stops building at this many units, so that three of
// them cannot fill the 64 slots every player shares.
#define AI_UNIT_CAP 6

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
#define BARB_FIRST_TURN 15
#define BARB_CHANCE 7
#define BARB_SPACING 6
#define BARB_CAP 8
#define BARB_TRIES 50

// Which way each exploring warrior is heading, DIR_*. Kept between turns so
// that it walks somewhere rather than jittering on the spot.
HIGH_BSS static byte heading[MAX_UNITS];

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

void ai_place(void)
{
    byte p, ax, ay, tries;
    word spacing;

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

// On along its heading, turning now and then, and trying the other ways
// round when that is blocked. Nowhere to go is a turn spent standing still.
static void explore(byte u)
{
    byte d = heading[u] % 6, i, r = rnd();     // HIGH_BSS: not cleared

    if ((r & 3) == 0) d = (byte)((d + ((r & 4) ? 1 : 5)) % 6);
    for (i = 0; i < 6; ++i, d = (byte)((d + 1) % 6)) {
        if (step(u, d)) {
            heading[u] = d;
            return;
        }
    }
    unit_moves[u] = 0;
}

// The step from u's hexagon that comes nearest (bx, by), if any is nearer;
// if none is, water or somebody is in the way, and a step of exploring is
// what gets round it.
static void approach(byte u, byte bx, byte by)
{
    byte dir, nx, ny, best = DIR_NONE;
    word d, best_d = distance(unit_ax[u], unit_ay[u], bx, by);

    for (dir = 0; dir < 6; ++dir) {
        if (!map_neighbour(unit_ax[u], unit_ay[u], dir, &nx, &ny)) continue;
        if (unit_can_enter(unit_owner[u], nx, ny) != ENTER_OK) continue;
        d = distance(nx, ny, bx, by);
        if (d < best_d) {
            best_d = d;
            best = dir;
        }
    }
    if (best == DIR_NONE || !step(u, best)) explore(u);
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

// Whether another of p's units is fortified on (ax, ay).
static byte guarded(byte u, byte ax, byte ay)
{
    byte v;
    for (v = 0; v < MAX_UNITS; ++v)
        if (v != u && unit_kind[v] != KIND_NONE
            && unit_owner[v] == unit_owner[u] && unit_ax[v] == ax
            && unit_ay[v] == ay && unit_orders[v] == ORDERS_FORTIFY)
            return 1;
    return 0;
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

static void move_unit(byte u)
{
    byte c, ax = unit_ax[u], ay = unit_ay[u];

    if (unit_owner[u] == PLAYER_BARBARIAN) {
        if (!attack_near(u, BARB_ODDS)) hunt(u, BARB_RANGE);
        return;
    }
    switch (unit_kind[u]) {
    case KIND_SETTLER:
        // 1. With no city, found one where it stands; it was put on good
        // land. If there is a city already, wander until settlers are for
        // founding more.
        if (city_found(unit_owner[u], ax, ay) != CITY_NONE) {
            unit_remove(u);
            return;
        }
        explore(u);
        break;
    case KIND_WARRIOR:
        // A fair fight next to it first. Then the first warrior home in an
        // unguarded city stays to guard it, and any other goes home to; 4.
        // the rest hunt the enemy nearby or explore.
        if (attack_near(u, AI_ODDS)) return;
        c = home(u);
        if (c != CITY_NONE && !guarded(u, city_ax[c], city_ay[c])) {
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
        // 5. To a hexagon next to its city, and there it stays: there are no
        // improvements to make yet, so every hexagon is unimproved and the
        // nearest will do.
        c = home(u);
        if (c == CITY_NONE) {
            explore(u);
        } else if (ax == city_ax[c] && ay == city_ay[c]) {
            explore(u);
        } else if (distance(ax, ay, city_ax[c], city_ay[c]) > 1) {
            approach(u, city_ax[c], city_ay[c]);
        } else {
            unit_moves[u] = 0;
        }
        break;
    }
}

void ai_turn(byte p)
{
    byte c, u;
    byte units = unit_count_kind(p, KIND_NONE);

    if (!player_alive(p)) return;
    unit_begin_turn(p);

    // 2. No military, build some; 3. no builder, build one; then warriors
    // to explore -- up to the cap. Changing keeps the shields, so a city
    // half way through a warrior that is now a builder loses nothing.
    for (c = 0; c < city_count; ++c) {
        if (city_owner[c] != p) continue;
        if (units >= AI_UNIT_CAP)
            city_build[c] = KIND_NONE;
        else if (unit_count_kind(p, KIND_WARRIOR) == 0)
            city_build[c] = KIND_WARRIOR;
        else if (unit_count_kind(p, KIND_BUILDER) == 0)
            city_build[c] = KIND_BUILDER;
        else
            city_build[c] = KIND_WARRIOR;
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
