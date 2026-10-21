// Where the computer players start, out of ai.c into region 0 (21 Oct
// 2026, for room in region 1 for the ferrying): it runs once a game, from
// game_init() in turn.c, which is region 0 too, so it is called directly
// and needs no stub. distance() and land_neighbours() are ai.c's, copied.
#include "ai.h"
#include "city.h"
#include "combat.h"
#include "map.h"
#include "sheet.h"
#include "unit.h"

#pragma clang section text="r0text" rodata="r0rodata"

// How far apart the starting settlers should be, in hexagons, and how many
// tries each spacing gets before the next, closer one is accepted.
#define START_SPACING 16
#define START_SPACING_STEP 4
#define START_TRIES 200

#define SEEN_COLUMNS 8          // as ai.c's blocks
#define SEEN_ROWS 4
#define SEEN_X (MAP_COLUMNS / SEEN_COLUMNS)

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

// The nearest unit to (ax, ay), anybody's; there are no cities yet.
static word nearest_unit(byte ax, byte ay)
{
    byte i;
    word d, best = 0xffff;
    for (i = 0; i < MAX_UNITS; ++i) {
        if (unit_kind[i] == KIND_NONE) continue;
        d = distance(ax, ay, unit_ax[i], unit_ay[i]);
        if (d < best) best = d;
    }
    return best;
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
                    && nearest_unit(ax, ay) >= spacing)
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
