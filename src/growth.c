// In region 1 (src/region.h): called through the stubs in src/dispatch.s,
// under these names here.
#define city_work city_work_r1
#define city_grow city_grow_r1

#include "growth.h"
#include "city.h"
#include "map.h"
#include "sheet.h"

#pragma clang section text="r1text" rodata="r1rodata"

byte work_mask;
byte work_food;
byte work_shields;
byte work_trade;

// By terrain value (TILE_* in src/sheet.h): Civilization's, with the hills
// for its hills, and the floodplain a river valley that feeds a city --
// the grain of vision.md. A mine adds mine_shields[].
static const byte terrain_food[TERRAIN_COUNT] = {
    1, 2, 0, 1, 1, 1, 1, 1, 0, 3
};
static const byte terrain_shields[TERRAIN_COUNT] = {
    0, 0, 1, 0, 0, 0, 1, 2, 1, 0
};
static const byte terrain_trade[TERRAIN_COUNT] = {
    0, 0, 0, 2, 2, 0, 0, 0, 0, 1
};
static const byte mine_shields[TERRAIN_COUNT] = {
    3, 0, 2, 0, 0, 0, 0, 0, 1, 0
};

// What hexagon (ax, ay) yields, into these.
static byte food, shields, trade;

static void yield(byte ax, byte ay)
{
    byte m = map_tile[ay][ax];
    byte t = m & MAP_TERRAIN_MASK;

    food = terrain_food[t];
    shields = terrain_shields[t];
    trade = terrain_trade[t];
    if (m & MAP_IRRIGATED) ++food;
    if (m & MAP_MINED) shields += mine_shields[t];
}

// Pick the hexagons city c works, scoring each neighbour as `weight` times
// its food, twice its shields and its trade, and total the yields.
static void work(byte c, byte weight)
{
    byte ax = city_ax[c], ay = city_ay[c];
    byte n, dir, nx, ny, best, best_dir;
    byte scores[6];

    // The city's own hexagon, which always makes a shield.
    yield(ax, ay);
    work_food = food;
    work_shields = shields ? shields : 1;
    work_trade = trade;

    for (dir = 0; dir < 6; ++dir) {
        scores[dir] = 0;
        if (!map_neighbour(ax, ay, dir, &nx, &ny)) continue;
        yield(nx, ny);
        // One more than the weighted yield, so that a hexagon that yields
        // nothing is still a hexagon, and 0 means none there.
        scores[dir] = weight * food + 2 * shields + trade + 1;
    }

    // The n best, a pass each: there are only six.
    work_mask = 0;
    for (n = city_size[c]; n; --n) {
        best = 0;
        best_dir = 0;
        for (dir = 0; dir < 6; ++dir)
            if (scores[dir] > best) {
                best = scores[dir];
                best_dir = dir;
            }
        if (!best) break;
        scores[best_dir] = 0;
        work_mask |= (byte)(1 << best_dir);
        map_neighbour(ax, ay, best_dir, &nx, &ny);
        yield(nx, ny);
        work_food += food;
        work_shields += shields;
        work_trade += trade;
    }
}

// Growth first, at three to two to one; and if that leaves the city short
// of food, food above all, so that it does not starve for the sake of a
// shield -- which would otherwise have it grow and shrink by turns.
void city_work(byte c)
{
    work(c, 3);
    if (work_food < 2 * city_size[c]) work(c, 16);
}

byte city_grow(byte c)
{
    byte size = city_size[c];
    byte eaten = 2 * size;

    city_work(c);
    if (work_food >= eaten) {
        city_food[c] += work_food - eaten;
        if (city_food[c] < GROWTH_BOX(size)) return GROWTH_NONE;
        city_food[c] = 0;
        if (size == GROWTH_MAX_SIZE) return GROWTH_NONE;
        city_size[c] = size + 1;
        return GROWTH_GREW;
    }
    eaten -= work_food;
    if (city_food[c] >= eaten) {
        city_food[c] -= eaten;
        return GROWTH_NONE;
    }
    city_food[c] = 0;
    if (size == 1) return GROWTH_NONE;
    city_size[c] = size - 1;
    return GROWTH_SHRANK;
}
