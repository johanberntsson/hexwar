// In region 1 (src/region.h): called through the stubs in src/dispatch.s,
// under these names here.
#define improve_can improve_can_r1
#define improve_best improve_best_r1
#define improve_start improve_start_r1
#define improve_turn improve_turn_r1

#include "improve.h"
#include "map.h"
#include "sheet.h"
#include "unit.h"

#pragma clang section text="r1text" rodata="r1rodata"

// Turns of work, Civilization's.
#define IRRIGATE_TURNS 5
#define MINE_TURNS 10

// Water to irrigate from: the sea, a floodplain -- the river it stands for
// -- or irrigation already there, on (ax, ay) or next to it.
static byte watered(byte ax, byte ay)
{
    byte dir, nx, ny, m;

    if (MAP_TERRAIN(ax, ay) == TILE_FLOODPLAIN) return 1;
    for (dir = 0; dir < 6; ++dir) {
        if (!map_neighbour(ax, ay, dir, &nx, &ny)) continue;
        m = map_tile[ny][nx];
        switch (m & MAP_TERRAIN_MASK) {
        case TILE_OCEAN:
        case TILE_SHALLOW_OCEAN:
        case TILE_FLOODPLAIN:
            return 1;
        }
        if (m & MAP_IRRIGATED) return 1;
    }
    return 0;
}

byte improve_can(byte orders, byte ax, byte ay)
{
    byte m = map_tile[ay][ax];
    byte t = m & MAP_TERRAIN_MASK;

    if (orders == ORDERS_IRRIGATE) {
        if (m & MAP_IRRIGATED) return 0;
        if (t != TILE_GRASS && t != TILE_PLAINS && t != TILE_DESERT
            && t != TILE_FLOODPLAIN && t != TILE_HILLS)
            return 0;
        return watered(ax, ay);
    }
    if (m & MAP_MINED) return 0;
    return t == TILE_HILLS || t == TILE_MOUNTAIN || t == TILE_DESERT;
}

byte improve_best(byte ax, byte ay)
{
    byte t = MAP_TERRAIN(ax, ay);

    // Shields are all a city makes yet, so the hills get mines; and a
    // hexagon with either improvement already is left alone.
    if (map_tile[ay][ax] & MAP_IMPROVEMENT) return ORDERS_NONE;
    if (t != TILE_HILLS && improve_can(ORDERS_IRRIGATE, ax, ay))
        return ORDERS_IRRIGATE;
    if (improve_can(ORDERS_MINE, ax, ay)) return ORDERS_MINE;
    return ORDERS_NONE;
}

void improve_start(byte u, byte orders)
{
    unit_orders[u] = orders;
    unit_work[u] = 0;
    unit_moves[u] = 0;
}

// Whether u is a builder at work on `orders` (0 for any) on (ax, ay).
static byte working(byte u, byte orders, byte ax, byte ay)
{
    byte o = unit_orders[u];
    return unit_kind[u] == KIND_BUILDER
           && (o == ORDERS_IRRIGATE || o == ORDERS_MINE)
           && (!orders || o == orders)
           && unit_ax[u] == ax && unit_ay[u] == ay;
}

byte improve_turn(void)
{
    byte u, v, ax, ay, orders, done = ORDERS_NONE;
    word work;

    // Everybody's turn of work first, so that the totals below count this
    // turn for every builder on a hexagon, whichever comes first in the list.
    for (u = 0; u < MAX_UNITS; ++u)
        if (working(u, 0, unit_ax[u], unit_ay[u])) ++unit_work[u];

    for (u = 0; u < MAX_UNITS; ++u) {
        ax = unit_ax[u];
        ay = unit_ay[u];
        if (!working(u, 0, ax, ay)) continue;
        orders = unit_orders[u];
        // Another builder on the same hexagon may have finished it.
        if (!improve_can(orders, ax, ay)) {
            unit_orders[u] = ORDERS_NONE;
            continue;
        }
        // **Builders on one hexagon with the same orders work together**,
        // as Civilization's settlers do: their turns add up.
        work = 0;
        for (v = 0; v < MAX_UNITS; ++v)
            if (working(v, orders, ax, ay)) work += unit_work[v];
        if (work < (orders == ORDERS_IRRIGATE ? IRRIGATE_TURNS : MINE_TURNS))
            continue;
        map_tile[ay][ax] = (map_tile[ay][ax] & ~MAP_IMPROVEMENT)
                           | (orders == ORDERS_IRRIGATE ? MAP_IRRIGATED
                                                        : MAP_MINED);
        // All of them are free again, this turn.
        for (v = 0; v < MAX_UNITS; ++v)
            if (working(v, orders, ax, ay)) {
                unit_orders[v] = ORDERS_NONE;
                if (unit_owner[v] == PLAYER_HUMAN) done = orders;
            }
    }
    return done;
}
