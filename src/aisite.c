// Where a computer player's settlers found their cities, out of ai.c (24
// Oct 2026, to make room in region 1): in region 2, beside ai_build()
// (src/aibuild.c), which asks whether there is a site before it builds a
// settler, and improve_best(), which the builders call in the same loops,
// so that a settler looking costs no copy that the builders do not. ai.c
// calls these through stubs (aiint.h); aibuild.c directly.
#define ai_find_site ai_find_site_r2
#define ai_find_far_site ai_find_far_site_r2

#include "aiint.h"
#include "city.h"
#include "map.h"
#include "sheet.h"
#include "unit.h"

#pragma clang section text="r2text" rodata="r2rodata"

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

// With a galley a player looks FERRY_RANGE far for a site, over the water
// (ai_find_far_site()); otherwise SETTLE_RANGE.
#define FERRY_RANGE 16

byte ai_site_x, ai_site_y;

// ai.c's, copied: a call into region 1 for every hexagon looked at would
// cost more than the copy.
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

// What the resources a city on (ax, ay) would have are worth, on it and
// its RING: one each, and copper and tin two, for bronze.
static byte worth(byte ax, byte ay)
{
    byte r = map_resource_at(ax, ay);
    return r == RES_NONE ? 0 : r == RES_COPPER || r == RES_TIN ? 2 : 1;
}

static byte resources_round(byte ax, byte ay)
{
    byte dir, nx, ny, n = worth(ax, ay);
    for (dir = 0; dir < RING; ++dir)
        if (map_neighbour(ax, ay, dir, &nx, &ny)) n += worth(nx, ny);
    return n;
}

// The best site for a city within SETTLE_RANGE of (fx, fy), in
// (ai_site_x, ai_site_y); 0 if there is none. (fx, fy) itself may be occupied --
// by the settler looking -- and anywhere else may not. The nearest, but a
// resource in reach is worth half a hexagon's walk, and copper or tin a
// whole one: the score is twice the distance, plus six, less what the
// resources are worth (resources_round()).
#define NO_SITE 0x7fff
static byte site_range = SETTLE_RANGE;

byte ai_find_site(byte fx, byte fy)
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
            ai_site_x = x;
            ai_site_y = (byte)y;
        }
    }
    return best != NO_SITE;
}

// A site for player p within reach of (fx, fy): SETTLE_RANGE, or with a
// galley FERRY_RANGE, over the water.
byte ai_find_far_site(byte p, byte fx, byte fy)
{
    byte found = ai_find_site(fx, fy);
    if (!found && unit_count_kind(p, KIND_GALLEY)) {
        site_range = FERRY_RANGE;
        found = ai_find_site(fx, fy);
        site_range = SETTLE_RANGE;
    }
    return found;
}
