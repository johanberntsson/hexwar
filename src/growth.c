// In region 1 (src/region.h): called through the stubs in src/dispatch.s,
// under these names here.
#define city_work city_work_r1
#define city_grow city_grow_r1
#define city_toggle city_toggle_r1
#define hex_yield hex_yield_r1
#define player_has player_has_r1
#define player_owns player_owns_r1

#include "growth.h"
#include "city.h"
#include "crisis.h"
#include "map.h"
#include "sheet.h"
#include "trade.h"

#pragma clang section text="r1text" rodata="r1rodata"

byte work_mask;
byte work_food;
byte work_shields;
byte work_trade;
byte work_idle;

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

// What a resource adds, by RES_* (src/map.h): Civilization's specials add
// two to four, and these keep to that. Tin and copper are worth more than
// their shields once there is bronze (TODO.txt, milestone 7b).
static const byte resource_food[RES_COUNT] = {
    0, 2, 2, 1, 1, 0, 0, 0, 0, 0
};
static const byte resource_shields[RES_COUNT] = {
    0, 0, 0, 1, 0, 0, 2, 2, 1, 0
};
static const byte resource_trade[RES_COUNT] = {
    0, 0, 0, 0, 2, 3, 0, 0, 0, 3
};

byte player_owns(byte p, byte r)
{
    byte c, dir, nx, ny, need = map_res_improved_by[r];

    for (c = 0; c < city_count; ++c) {
        if (city_owner[c] != p) continue;
        if (map_resource_at(city_ax[c], city_ay[c]) == r) return 1;
        for (dir = 0; dir < 6; ++dir)
            if (map_neighbour(city_ax[c], city_ay[c], dir, &nx, &ny)
                && (!need || (map_tile[ny][nx] & need))
                && map_resource_at(nx, ny) == r)
                return 1;
    }
    return 0;
}

byte player_has(byte p, byte r)
{
    return ((trade_got[p] >> r) & 1) || player_owns(p, r);
}

// What hexagon (ax, ay) yields, into these: hex_yield() hands them out.
byte hex_food, hex_shields, hex_trade;

static void yield(byte ax, byte ay)
{
    byte m = map_tile[ay][ax];
    byte t = m & MAP_TERRAIN_MASK;

    hex_food = terrain_food[t];
    // The Crisis's droughts (crisis.h): the dry land's food fails.
    if (crisis_drought && hex_food
        && (t == TILE_GRASS || t == TILE_PLAINS || t == TILE_DESERT))
        --hex_food;
    hex_shields = terrain_shields[t];
    hex_trade = terrain_trade[t];
    t = map_resource_at(ax, ay);
    hex_food += resource_food[t];
    hex_shields += resource_shields[t];
    hex_trade += resource_trade[t];
    t = m & MAP_TERRAIN_MASK;
    if (m & MAP_IRRIGATED) ++hex_food;
    if (m & MAP_MINED) hex_shields += mine_shields[t];
    // **Civilization's grassland shields**: half the grassland and river
    // squares make a shield, on a fixed pattern -- (x*7 + y*11) & 2 clear,
    // from OpenCivOne's TerrainMapCell.cs -- which is what gets a new city
    // going. Here on array cells, and for the floodplain, our river. Only
    // the low bits matter, so a byte will do.
    if (t == TILE_GRASS || t == TILE_FLOODPLAIN) {
        byte h = (byte)(ax * 7) + (byte)(ay * 11);
        if (!(h & 2)) ++hex_shields;
    }
}

void hex_yield(byte ax, byte ay)
{
    yield(ax, ay);
}

// Each neighbour of city c scored for working: `weight` times its food,
// twice its shields and its trade, plus one, so that 0 means no hexagon.
static byte scores[6];

static void score(byte c, byte weight)
{
    byte dir, nx, ny;
    for (dir = 0; dir < 6; ++dir) {
        scores[dir] = 0;
        if (!map_neighbour(city_ax[c], city_ay[c], dir, &nx, &ny)) continue;
        yield(nx, ny);
        scores[dir] = weight * hex_food + 2 * hex_shields + hex_trade + 1;
    }
}

// `mask` and the n best neighbours not in it, by scores[].
static byte pick(byte mask, byte n)
{
    byte dir, best, best_dir;
    for (; n; --n) {
        best = 0;
        best_dir = 0;
        for (dir = 0; dir < 6; ++dir)
            if (!((mask >> dir) & 1) && scores[dir] > best) {
                best = scores[dir];
                best_dir = dir;
            }
        if (!best) break;
        mask |= (byte)(1 << best_dir);
    }
    return mask;
}

static byte count(byte mask)
{
    byte n = 0;
    for (; mask; mask >>= 1) n += mask & 1;
    return n;
}

// The worked hexagons' totals, the city's own included, into work_*.
static void total(byte c, byte mask)
{
    byte ax = city_ax[c], ay = city_ay[c];
    byte dir, nx, ny, n = count(mask);

    // The city's own hexagon, which always makes a shield.
    yield(ax, ay);
    work_food = hex_food;
    work_shields = hex_shields ? hex_shields : 1;
    work_trade = hex_trade;
    for (dir = 0; dir < 6; ++dir) {
        if (!((mask >> dir) & 1)) continue;
        map_neighbour(ax, ay, dir, &nx, &ny);
        yield(nx, ny);
        work_food += hex_food;
        work_shields += hex_shields;
        work_trade += hex_trade;
    }
    work_mask = mask;
    work_idle = city_size[c] > n ? city_size[c] - n : 0;
}

// Growth first, at three to two to one; and if that leaves the city short
// of food, food above all, so that it does not starve for the sake of a
// shield -- which would otherwise have it grow and shrink by turns.
//
// **A city the player has set by hand keeps its hexagons**: only if it has
// more workers than citizens -- it shrank, or built a settler -- does the
// worst of them go.
void city_work(byte c)
{
    byte m = city_worked[c], dir, worst, low;

    if (m & CITY_MANUAL) {
        m &= CITY_WORKED;
        if (count(m) > city_size[c]) {
            score(c, 3);
            do {
                low = 0xff;
                worst = 0;
                for (dir = 0; dir < 6; ++dir)
                    if (((m >> dir) & 1) && scores[dir] < low) {
                        low = scores[dir];
                        worst = dir;
                    }
                m &= (byte)~(1 << worst);
            } while (count(m) > city_size[c]);
            city_worked[c] = m | CITY_MANUAL;
        }
        total(c, m);
        return;
    }
    score(c, 3);
    total(c, pick(0, city_size[c]));
    if (work_food < 2 * city_size[c]) {
        score(c, 16);
        total(c, pick(0, city_size[c]));
    }
}

byte city_toggle(byte c, byte dir)
{
    byte m, bit = (byte)(1 << dir), nx, ny;

    if (!map_neighbour(city_ax[c], city_ay[c], dir, &nx, &ny)) return 0;
    if (!(city_worked[c] & CITY_MANUAL)) {
        city_work(c);
        city_worked[c] = work_mask | CITY_MANUAL;
    }
    m = city_worked[c];
    if (m & bit) {
        m &= (byte)~bit;
    } else {
        if (count(m & CITY_WORKED) >= city_size[c]) return 0;
        m |= bit;
    }
    city_worked[c] = m;
    return 1;
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
        // Set by hand, the newcomer takes the best hexagon left, if any.
        if (city_worked[c] & CITY_MANUAL) {
            score(c, 3);
            city_worked[c] = pick(city_worked[c], 1);
        }
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
