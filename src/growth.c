// In region 1 (src/region.h): called through the stubs in src/dispatch.s,
// under these names here.
#define city_work city_work_r1
#define city_grow city_grow_r1
#define hex_yield hex_yield_r1
#define player_has player_has_r1
#define player_owns player_owns_r1

#include "growth.h"
#include "city.h"
#include "crisis.h"
#include "emporium.h"
#include "map.h"
#include "sheet.h"
#include "trade.h"

#pragma clang section text="r1text" rodata="r1rodata"

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
// their shields once there is bronze (TODO.txt, milestone 7b). Horses are
// Civilization's plains special, two shields more.
static const byte resource_food[RES_COUNT] = {
    0, 2, 2, 1, 1, 0, 0, 0, 0, 0, 0
};
static const byte resource_shields[RES_COUNT] = {
    0, 0, 0, 1, 0, 0, 2, 2, 1, 0, 2
};
static const byte resource_trade[RES_COUNT] = {
    0, 0, 0, 0, 2, 3, 0, 0, 0, 3, 0
};

byte player_owns(byte p, byte r)
{
    byte c, dir, nx, ny, need = map_res_improved_by[r];

    // Round one of its emporia, improved or not (src/emporium.h).
    for (c = 0; c < emporium_count; ++c)
        if (emporium_owner[c] == p)
            for (dir = 0; dir < 6; ++dir)
                if (map_neighbour(emporium_ax[c], emporium_ay[c], dir, &nx, &ny)
                    && map_resource_at(nx, ny) == r)
                    return 1;

    for (c = 0; c < city_count; ++c) {
        if (city_owner[c] != p) continue;
        if (map_resource_at(city_ax[c], city_ay[c]) == r) return 1;
        for (dir = 0; dir < RING; ++dir)
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
    if (m & MAP_IRRIGATED) ++hex_food;
    // A pasture is the mine bit on cattle and horses (improve.h).
    if (m & MAP_MINED) {
        if (RES_PASTURE(t)) {
            ++hex_food;
            ++hex_shields;
        } else {
            hex_shields += mine_shields[m & MAP_TERRAIN_MASK];
        }
    }
    t = m & MAP_TERRAIN_MASK;
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

// **Whose each hexagon of city c's ring is**, into work_owner[], and where
// it is, into work_ring_x/work_ring_y. Cities 4 apart, as the computer
// players found them, share their outer rings, and the human may found them closer: **a
// hexagon is the nearer city's**, the earlier in the list on a tie, so the
// two never work it both and nothing about it has to be stored. Only the
// cities within twice the reach can share, NEAR_MAX of them at most.
HIGH_BSS byte work_on[RING];
HIGH_BSS byte work_owner[RING];
HIGH_BSS byte work_ring_x[RING], work_ring_y[RING];
HIGH_BSS static byte scores[RING];
#define NEAR_MAX 8

// ai.c's, in this region too.
word ai_distance(byte ax, byte ay, byte bx, byte by);

static void ring(byte c)
{
    byte i, k, n = 0, x = city_ax[c], y = city_ay[c];
    byte near[NEAR_MAX];
    word d, e;

    for (k = 0; k < city_count && n < NEAR_MAX; ++k)
        if (k != c && ai_distance(x, y, city_ax[k], city_ay[k]) <= 4)
            near[n++] = k;
    for (i = 0; i < RING; ++i) {
        work_owner[i] = CITY_NONE;
        if (!map_neighbour(x, y, i, &work_ring_x[i], &work_ring_y[i]))
            continue;
        work_owner[i] = c;
        d = i < RING_INNER ? 1 : 2;
        for (k = 0; k < n; ++k) {
            e = ai_distance(work_ring_x[i], work_ring_y[i],
                            city_ax[near[k]], city_ay[near[k]]);
            if (e < d || (e == d && near[k] < work_owner[i])) {
                d = e;
                work_owner[i] = near[k];
            }
        }
    }
}

// Each of city c's hexagons scored for working: `weight` times its food,
// twice its shields and its trade, plus one, so that 0 means one it cannot
// work. ring() first.
static void score(byte c, byte weight)
{
    byte i;
    for (i = 0; i < RING; ++i) {
        scores[i] = 0;
        if (work_owner[i] != c) continue;
        yield(work_ring_x[i], work_ring_y[i]);
        scores[i] = weight * hex_food + 2 * hex_shields + hex_trade + 1;
    }
}

// The n best hexagons not worked yet, by scores[], added to work_on[].
static void pick(byte n)
{
    byte i, best, best_i;
    for (; n; --n) {
        best = 0;
        best_i = 0;
        for (i = 0; i < RING; ++i)
            if (!work_on[i] && scores[i] > best) {
                best = scores[i];
                best_i = i;
            }
        if (!best) break;
        work_on[best_i] = 1;
    }
}

static byte count(void)
{
    byte i, n = 0;
    for (i = 0; i < RING; ++i) n += work_on[i];
    return n;
}

static void clear(void)
{
    byte i;
    for (i = 0; i < RING; ++i) work_on[i] = 0;
}

// city_worked[] (city.h) to work_on[] and back; storing makes it manual.
static void load(byte c)
{
    byte i;
    for (i = 0; i < RING; ++i)
        work_on[i] = (city_worked[i >> 3][c] >> (i & 7)) & 1;
}

static void store(byte c)
{
    byte i;
    city_worked[0][c] = city_worked[1][c] = 0;
    city_worked[CITY_MANUAL_BYTE][c] = CITY_MANUAL;
    for (i = 0; i < RING; ++i)
        if (work_on[i]) city_worked[i >> 3][c] |= (byte)(1 << (i & 7));
}

// The worked hexagons' totals, the city's own included, into work_*.
static void total(byte c)
{
    byte i, n = count();

    // The city's own hexagon, which always makes a shield.
    yield(city_ax[c], city_ay[c]);
    work_food = hex_food;
    work_shields = hex_shields ? hex_shields : 1;
    work_trade = hex_trade;
    for (i = 0; i < RING; ++i) {
        if (!work_on[i]) continue;
        yield(work_ring_x[i], work_ring_y[i]);
        work_food += hex_food;
        work_shields += hex_shields;
        work_trade += hex_trade;
    }
    work_idle = city_size[c] > n ? city_size[c] - n : 0;
}

// Growth first, at three to two to one; and if that leaves the city short
// of food, food above all, so that it does not starve for the sake of a
// shield -- which would otherwise have it grow and shrink by turns.
//
// **A city the player has set by hand keeps its hexagons**: only a hexagon
// a nearer city has taken since goes, and, if it has more workers than
// citizens -- it shrank, or built a settler -- the worst of them.
void city_work(byte c)
{
    byte i, worst, low;

    ring(c);
    if (city_worked[CITY_MANUAL_BYTE][c] & CITY_MANUAL) {
        load(c);
        for (i = 0; i < RING; ++i)
            if (work_owner[i] != c) work_on[i] = 0;
        score(c, 3);
        while (count() > city_size[c]) {
            low = 0xff;
            worst = 0;
            for (i = 0; i < RING; ++i)
                if (work_on[i] && scores[i] < low) {
                    low = scores[i];
                    worst = i;
                }
            work_on[worst] = 0;
        }
        store(c);
        total(c);
        return;
    }
    clear();
    score(c, 3);
    pick(city_size[c]);
    total(c);
    if (work_food < 2 * city_size[c]) {
        clear();
        score(c, 16);
        pick(city_size[c]);
        total(c);
    }
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
        // city_work() above left the ring and its hexagons in place.
        if (city_worked[CITY_MANUAL_BYTE][c] & CITY_MANUAL) {
            score(c, 3);
            pick(1);
            store(c);
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
