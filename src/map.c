#include "map.h"
#include "sheet.h"
#include "minimap.h"

HIGH_BSS byte map_tile[MAP_ROWS][MAP_COLUMNS];
HIGH_BSS byte map_res_count;
HIGH_BSS byte map_res_first[2 * MAP_ROWS + 1];
HIGH_BSS byte map_res_ax[MAP_RES_MAX];
HIGH_BSS byte map_res_kind[MAP_RES_MAX];
#ifdef DEBUG_MENU
byte map_show;
#endif

// RES_* order. Grain, fish, cedar and dye have no improved picture yet.
const byte map_res_improved_by[RES_COUNT] = {
    0, 0, 0, MAP_IRRIGATED, MAP_IRRIGATED, MAP_IRRIGATED, 0, MAP_MINED,
    MAP_MINED, 0
};
const byte map_res_improved[RES_COUNT] = {
    0, 0, 0, TILE_CATTLE_PASTURE, TILE_OLIVES_GROVE, TILE_WINE_VINEYARD, 0,
    TILE_COPPER_MINE, TILE_TIN_MINE, 0
};

// One step in each direction, in doubled coordinates; DIR_* order. File scope
// and 16 bit, because small static const arrays of int8_t inside a function
// have been seen to compile to nothing with Calypsi 5.18 (CLAUDE.md).
static const int dir_dhx[6] = { 2, 1, -1, -2, -1, 1 };
static const int dir_dhy[6] = { 0, 1, 1, 0, -1, -1 };

byte map_neighbour(byte ax, byte ay, byte dir, byte *nax, byte *nay)
{
    int hx = (int)ax + dir_dhx[dir];
    int hy = 2 * (int)ay + (ax & 1) + dir_dhy[dir];

    if (hy < 0 || hy >= 2 * MAP_ROWS) return 0;
    if (hx < 0) hx += MAP_COLUMNS;
    else if (hx >= MAP_COLUMNS) hx -= MAP_COLUMNS;
    // hx and hy keep the same parity -- every step changes both by an odd
    // amount or neither -- and MAP_COLUMNS is even, so the wrap keeps it too.
    *nax = (byte)hx;
    *nay = (byte)((hy - (hx & 1)) >> 1);
    return 1;
}

byte map_resource_at(byte ax, byte ay)
{
    byte hy = (byte)(2 * ay) + (ax & 1);
    byte i, end = map_res_first[hy + 1];

    for (i = map_res_first[hy]; i < end; ++i)
        if (map_res_ax[i] == ax) return map_res_kind[i];
    return RES_NONE;
}

byte map_direction(byte ax, byte ay, byte bx, byte by)
{
    byte dir, nx, ny;
    for (dir = 0; dir < 6; ++dir) {
        if (map_neighbour(ax, ay, dir, &nx, &ny) && nx == bx && ny == by)
            return dir;
    }
    return DIR_NONE;
}

void map_reset(void)
{
    byte ax, ay;
#ifdef DEBUG_MENU
    map_show = 0;
#endif
    for (ay = 0; ay < MAP_ROWS; ++ay)
        for (ax = 0; ax < MAP_COLUMNS; ++ax)
            map_tile[ay][ax] &= MAP_TERRAIN_MASK;
}

static void explore(byte ax, byte ay)
{
    if (map_tile[ay][ax] & MAP_EXPLORED) return;
    map_tile[ay][ax] |= MAP_EXPLORED;
    minimap_plot(ax, ay);
}

void map_explore(byte ax, byte ay)
{
    byte dir, nx, ny;
    explore(ax, ay);
    for (dir = 0; dir < 6; ++dir) {
        if (map_neighbour(ax, ay, dir, &nx, &ny)) explore(nx, ny);
    }
}

void map_cell_at(word cx, word cy, byte *ax, byte *ay)
{
    // Work in half characters, so that the point is the middle of the
    // character and a tile's centre, (4*vx + 4, 3*hy + 2), is whole too.
    int px = 2 * (int)cx + 1;
    int py = 2 * (int)cy + 1;
    int q = (int)(cx / 4);
    int r = (int)(cy / 3);
    word best = 0xffff;
    int vx, hy;

    // The tiles whose rectangles hold the character start at most one column
    // left and one hexagon row up; one row down is checked as well, since its
    // centre can be nearer than a rectangle that merely contains the point.
    for (vx = q - 1; vx <= q; ++vx) {
        for (hy = r - 1; hy <= r + 1; ++hy) {
            int dx, dy;
            word d;
            if (hy < 0 || hy >= 2 * MAP_ROWS || ((vx ^ hy) & 1)) continue;
            dx = px - (8 * vx + 8);
            dy = py - (6 * hy + 4);
            // A character is 4 units wide and 8 tall on the glass, so a
            // half-character is 2 by 4, and distance squared is
            // (2dx)^2 + (4dy)^2, which is proportional to this.
            d = (word)(dx * dx + 4 * dy * dy);
            if (d < best) {
                best = d;
                // vx is -1 just left of the seam; the wrap brings it round
                // to MAP_COLUMNS - 1.
                *ax = (byte)(vx < 0 ? vx + MAP_COLUMNS : vx);
                *ay = (byte)((hy - (vx & 1)) >> 1);
            }
        }
    }
}
