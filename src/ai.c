#include "ai.h"
#include "city.h"
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

// Which way each exploring warrior is heading, DIR_*. Kept between turns so
// that it walks somewhere rather than jittering on the spot.
HIGH_BSS static byte heading[MAX_UNITS];

// A 16-bit xorshift. The same seed every game, which keeps a headless run
// deterministic; nothing here needs better.
static word seed = 0x2a6d;

static byte rnd(void)
{
    seed ^= seed << 7;
    seed ^= seed >> 9;
    seed ^= seed << 8;
    return (byte)seed;
}

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

// The nearest of any player's units to (ax, ay); at the start of a game
// those are the settlers.
static word nearest_unit(byte ax, byte ay)
{
    byte u;
    word d, best = 0xffff;
    for (u = 0; u < MAX_UNITS; ++u) {
        if (unit_kind[u] == KIND_NONE) continue;
        d = distance(ax, ay, unit_ax[u], unit_ay[u]);
        if (d < best) best = d;
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
                    && nearest_unit(ax, ay) >= spacing)
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

// The step from u's hexagon that comes nearest (bx, by), if any is nearer.
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
    if (best == DIR_NONE || !step(u, best)) unit_moves[u] = 0;
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

static void move_unit(byte u)
{
    byte c, ax = unit_ax[u], ay = unit_ay[u];

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
        // The first warrior in an unguarded city stays to guard it; 4. the
        // rest explore.
        c = city_at(ax, ay);
        if (c != CITY_NONE && !guarded(u, ax, ay)) {
            unit_orders[u] = ORDERS_FORTIFY;
            unit_moves[u] = 0;
            return;
        }
        explore(u);
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
