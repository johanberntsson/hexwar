// In region 4 (src/region.h): called through the stub in src/dispatch.s,
// under this name here.
#define sea_route sea_route_r4

#include "path.h"
#include "map.h"
#include "sheet.h"

#pragma clang section text="r4text" rodata="r4rodata"

// A bit a cell, and a ring of cells to visit. 128 is far more than the
// frontier of a search over the shallows ever holds; a search that fills
// it gives up, as if there were no way.
#define QUEUE 128
LOW_FREE static byte visited[MAP_ROWS * MAP_COLUMNS / 8];
LOW_FREE static byte queue_x[QUEUE];
LOW_FREE static byte queue_y[QUEUE];

static byte seen(byte ax, byte ay)
{
    word i = (word)ay * MAP_COLUMNS + ax;
    byte bit = (byte)(1 << (i & 7));
    byte *at = visited + (i >> 3);
    if (*at & bit) return 1;
    *at |= bit;
    return 0;
}

byte sea_route(byte fx, byte fy, byte tx, byte ty)
{
    byte head = 0, tail = 0, dir, x, y, nx, ny;
    word i;

    if ((fx == tx && fy == ty) || map_direction(fx, fy, tx, ty) != DIR_NONE)
        return DIR_NONE;
    for (i = 0; i < sizeof visited; ++i) visited[i] = 0;
    seen(tx, ty);
    queue_x[tail] = tx;
    queue_y[tail] = ty;
    tail = (byte)((tail + 1) & (QUEUE - 1));
    while (head != tail) {
        x = queue_x[head];
        y = queue_y[head];
        head = (byte)((head + 1) & (QUEUE - 1));
        for (dir = 0; dir < 6; ++dir) {
            if (!map_neighbour(x, y, dir, &nx, &ny)) continue;
            if (nx == fx && ny == fy) return map_direction(fx, fy, x, y);
            if (MAP_TERRAIN(nx, ny) != TILE_SHALLOW_OCEAN || seen(nx, ny))
                continue;
            queue_x[tail] = nx;
            queue_y[tail] = ny;
            tail = (byte)((tail + 1) & (QUEUE - 1));
            if (tail == head) return DIR_NONE;      // full: give up
        }
    }
    return DIR_NONE;
}
