// In region 4 (src/region.h): called through the stub in src/dispatch.s,
// under this name here.
#define boat_explore boat_explore_r4

#include "boat.h"
#include "ai.h"
#include "map.h"
#include "path.h"
#include "sheet.h"
#include "unit.h"

#pragma clang section text="r4text" rodata="r4rodata"

// ai.c's blocks: SEEN_COLUMNS x SEEN_ROWS cells, a bit each.
#define SEEN_COLUMNS 8
#define SEEN_ROWS 4

// A few blocks a move at most may turn out out of reach before it gives
// up for the turn: each costs a search.
#define TRIES 4

// Hexagons between two cells, as ai.c's distance().
static word distance(byte ax, byte ay, byte bx, byte by)
{
    int dx = (int)ax - (int)bx;
    int dy = (2 * (int)ay + (ax & 1)) - (2 * (int)by + (bx & 1));
    if (dx < 0) dx = -dx;
    if (dx > MAP_COLUMNS / 2) dx = MAP_COLUMNS - dx;
    if (dy < 0) dy = -dy;
    return (word)(dy + (dx > dy ? (dx - dy) / 2 : 0));
}

// The shallows of the nearest block with any that p has not seen, from
// (ax, ay): the block in (block_x, block_y) and its shallows nearest
// (ax, ay) in (sea_x, sea_y). 0 if p has seen them all.
static byte block_x, block_y, sea_x, sea_y;

static byte unseen_sea(byte p, byte ax, byte ay)
{
    byte bx, by, x, y;
    word d, best = 0xffff;

    for (bx = 0; bx < 16; ++bx) {
        for (by = 0; by < MAP_ROWS / SEEN_ROWS; ++by) {
            if (!((ai_sea[bx] >> by) & 1) || ((ai_seen[p][bx] >> by) & 1))
                continue;
            for (y = by * SEEN_ROWS; y < (by + 1) * SEEN_ROWS; ++y)
                for (x = bx * SEEN_COLUMNS; x < (bx + 1) * SEEN_COLUMNS; ++x) {
                    if (MAP_TERRAIN(x, y) != TILE_SHALLOW_OCEAN) continue;
                    d = distance(ax, ay, x, y);
                    if (d >= best) continue;
                    best = d;
                    block_x = bx;
                    block_y = by;
                    sea_x = x;
                    sea_y = y;
                }
        }
    }
    return best != 0xffff;
}

void boat_explore(byte u)
{
    byte p = unit_owner[u], dir, nx, ny, tries;

    for (tries = 0; tries < TRIES; ++tries) {
        if (!unseen_sea(p, unit_ax[u], unit_ay[u])) break;
        dir = sea_route(unit_ax[u], unit_ay[u], sea_x, sea_y);
        if (dir != DIR_NONE
            && map_neighbour(unit_ax[u], unit_ay[u], dir, &nx, &ny)
            && unit_can_enter(p, unit_kind[u], nx, ny) == ENTER_OK) {
            unit_move_to(u, nx, ny);
            --unit_moves[u];
            return;
        }
        // There, next to it, or no way: seen, as far as a ship can see.
        ai_seen[p][block_x] |= (byte)(1 << block_y);
    }
    unit_moves[u] = 0;
}
