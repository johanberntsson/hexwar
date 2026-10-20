#include "minimap.h"
#include "city.h"
#include "map.h"
#include "screen.h"
#include "sheet.h"
#include "input.h"
#include "unit.h"

#if MINIMAP_COLUMNS * 8 != MAP_COLUMNS || MINIMAP_ROWS * 8 != 2 * MAP_ROWS
#error "the minimap is a pixel a hexagon column and a line a hexagon row"
#endif
#if SHEET_EXPANDED + 64UL * SHEET_EXPANDED_COLUMNS * SHEET_ROWS > MINIMAP_STORE
#error "the expanded sheet runs into the minimap"
#endif
#if MINIMAP_STORE + MINIMAP_STORE_SIZE > 0x60000UL
#error "the minimap runs past $60000"
#endif
#if MINIMAP_PAD_CHARS >= MINIMAP_COLUMNS * MINIMAP_ROWS
#error "the minimap fits in the padding, so MINIMAP_STORE is not needed"
#endif

// Pixel values are palette indices, and 0-15 are the system colours that
// src/sheet.c leaves alone, so these are the C64's.
#define PAL_BLACK 0
#define PAL_WHITE 1
#define PAL_RED 2

#define PAL_PURPLE 4
#define PAL_ORANGE 8
#define PAL_BROWN 9

#define PAL_UNEXPLORED PAL_BLACK

// Cities and units in their player's colour, the human's white and the
// barbarians' brown.
static const byte player_colour[PLAYERS + 1] = {
    PAL_WHITE, PAL_RED, PAL_PURPLE, PAL_ORANGE, PAL_BROWN,
};

// Terrain t is palette entry PAL_TERRAIN + t: tools/mktiles.py puts a flat
// colour for each terrain there, ahead of the tiles' own colours, since the
// system palette has too few left over from the players' for ten terrains.
#define PAL_TERRAIN 16

// The outline, in sprite pixels. A sprite pixel at H640 is two screen pixels
// across, which is one hexagon here; the window is 60 characters = 7.5
// hexagons across and 23 rows = 7.7 hexagon rows down, so a frame of 9 with
// its edges on the window's edges.
#define BOX_W 9
#define BOX_H 9

// Scratch, so LOW_FREE: filled afresh every time, with no disk call between.
LOW_FREE static byte cell[64];

static byte colour(byte ax, byte ay)
{
    byte t = map_tile[ay][ax] | map_show;
    byte c;

    if (!(t & MAP_EXPLORED)) return PAL_UNEXPLORED;
    if (t & MAP_OCCUPIED) {
        c = city_at(ax, ay);
        return player_colour[c != CITY_NONE ? city_owner[c]
                                            : unit_owner[unit_at(ax, ay)]];
    }
    return PAL_TERRAIN + (t & MAP_TERRAIN_MASK);
}

// The character number of minimap character i (row-major, 16 a row): in the
// tile store's row padding for the first MINIMAP_PAD_CHARS, then at
// MINIMAP_STORE -- see src/minimap.h.
static word char_number(byte i)
{
    if (i < MINIMAP_PAD_CHARS)
        return (word)(TILE_STORE / 64)
               + (word)(i / MINIMAP_PAD_SLOTS) * (VIEW_ROW_STRIDE / 64)
               + VIEW_COLUMNS + i % MINIMAP_PAD_SLOTS;
    return (word)(MINIMAP_STORE / 64) + (i - MINIMAP_PAD_CHARS);
}

// Where character i's 64 bytes are. The call is kept out of the 32-bit
// expression, which would otherwise want _FillZPQ (CLAUDE.md).
static dword char_address(byte i)
{
    word n = char_number(i);
    return (dword)n << 6;
}

// Address of pixel (px, py): each character is 8 lines of 8 bytes. In
// words -- see the narrowing-cast note in CLAUDE.md.
static dword pixel(word px, word py)
{
    byte i = (byte)(((py >> 3) << 4) | (px >> 3));
    dword at = char_address(i);
    return at + (((py & 7) << 3) | (px & 7));
}

void minimap_plot(byte ax, byte ay)
{
    byte c = colour(ax, ay);
    word py = 2 * (word)ay + (ax & 1);
    word px = ax;

    lpoke(pixel(px, py), c);
    px = (px + 1) & (MAP_COLUMNS - 1);
    lpoke(pixel(px, py), c);
}

void minimap_show(void)
{
    byte cx, cy, i;
    word charnum;
    dword at;

    screen_box(MINIMAP_LEFT - 1, MINIMAP_TOP - 1,
               MINIMAP_LEFT + MINIMAP_COLUMNS, MINIMAP_TOP + MINIMAP_ROWS,
               COLOR_LIGHTGREY);

    // The screen words, as view_restore() writes the window's: a character
    // number of its own for every cell, and no colour or attributes, which
    // apply to full-colour characters too.
    i = 0;
    for (cy = 0; cy < MINIMAP_ROWS; ++cy) {
        for (cx = 0; cx < MINIMAP_COLUMNS; ++cx, ++i) {
            charnum = char_number(i);
            cell[2 * cx] = (byte)charnum;
            cell[2 * cx + 1] = (byte)(charnum >> 8);
        }
        at = SCREEN_CELL(MINIMAP_LEFT, MINIMAP_TOP + cy);
        lcopy((dword)(word)cell, at, 2 * MINIMAP_COLUMNS);
        lfill(COLOR_BASE + (at - SCREEN_BASE), 0, 2 * MINIMAP_COLUMNS);
    }
}

void minimap_init(void)
{
    byte cx, cy, x, y, i;
    word px, py;
    byte *p;
    dword at;

    minimap_show();
    i = 0;
    for (cy = 0; cy < MINIMAP_ROWS; ++cy) {
        // Each character's 64 pixels, built here and sent in one job.
        // A pixel whose column parity differs from its line's is the right
        // half of the hexagon one column to the left.
        for (cx = 0; cx < MINIMAP_COLUMNS; ++cx, ++i) {
            p = cell;
            for (y = 0; y < 8; ++y) {
                py = 8 * (word)cy + y;
                for (x = 0; x < 8; ++x) {
                    px = 8 * (word)cx + x;
                    if ((px ^ py) & 1) px = (px - 1) & (MAP_COLUMNS - 1);
                    *p++ = colour((byte)px, (byte)(py >> 1));
                }
            }
            at = char_address(i);
            lcopy((dword)(word)cell, at, 64);
        }
    }
}

// Box columns `from` to `to` of the outline into a sprite's shape, starting
// at the sprite's left edge. The sides are drawn only where the piece has
// them, so the two pieces of a wrapped outline join up.
static void outline(byte *s, byte from, byte to)
{
    byte r, c, k, bits;

    for (r = 0; r < 63; ++r) s[r] = 0;
    for (c = from; c <= to; ++c) {
        k = c - from;
        bits = (byte)(0x80 >> (k & 7));
        for (r = 0; r < BOX_H; ++r) {
            if (r == 0 || r == BOX_H - 1 || c == 0 || c == BOX_W - 1)
                s[3 * r + (k >> 3)] |= bits;
        }
    }
}

void minimap_frame(word off_x, word off_y)
{
    // A hexagon column is 4 map characters and one minimap pixel, but the
    // sprite's resolution is two pixels, so 8 characters a sprite pixel;
    // a hexagon row is 3 characters and one line.
    byte u = (byte)(off_x / 8);
    word x = MINIMAP_LEFT * 4;
    word y = MINIMAP_TOP * 8 + off_y / 3;
    byte width = MINIMAP_COLUMNS * 4;       // in sprite pixels, 64

    if (u + BOX_W <= width) {
        outline(sprite_shape(2), 0, BOX_W - 1);
        sprite_place(2, x + u, y);
        sprite_hide(3);
    } else {
        // Straddling the seam: the left part of the outline at the right
        // edge of the minimap, and the rest at its left edge.
        byte split = width - u;
        outline(sprite_shape(2), 0, split - 1);
        sprite_place(2, x + u, y);
        outline(sprite_shape(3), split, BOX_W - 1);
        sprite_place(3, x, y);
    }
}

byte minimap_hex(word x, word y, byte *ax, byte *ay)
{
    word px, py;

    if (x < MINIMAP_LEFT * 4 || x >= (MINIMAP_LEFT + MINIMAP_COLUMNS) * 4
        || y < MINIMAP_TOP * 8 || y >= (MINIMAP_TOP + MINIMAP_ROWS) * 8)
        return 0;
    // An arrow unit is two pixels, so this is the left one of them.
    px = 2 * (x - MINIMAP_LEFT * 4);
    py = y - MINIMAP_TOP * 8;
    if ((px ^ py) & 1) px = (px - 1) & (MAP_COLUMNS - 1);
    *ax = (byte)px;
    *ay = (byte)(py >> 1);
    return 1;
}
