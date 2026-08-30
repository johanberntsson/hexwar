// The map view: a window on the text screen whose every character has a
// full-colour bitmap of its own, and the drawing of the hex map into it.
#ifndef HEXWAR_VIEW_H
#define HEXWAR_VIEW_H

#include "mega65.h"

// The window, in screen characters. These are the numbers the XC=BASIC version
// used -- fc_setMergeTileMode(10, 1, 60, 23, true) in basic/civ.bas -- and what
// basic/civ-basic.png shows: ten columns of panel down the left, the map in the
// rest, one row of margin top and bottom.
#define VIEW_LEFT 10
#define VIEW_TOP 1
#define VIEW_COLUMNS 60
#define VIEW_ROWS 23

// **Every cell in the window owns 64 bytes of its own, and that is the whole
// trick.** view_init() writes a *unique* character number into each of the
// 60x23 cells, once, so that their character definitions form one linear
// framebuffer at TILE_STORE. Drawing is then nothing but copying 64-byte
// blocks into it; the screen words never change again, which is what makes
// scrolling a redraw rather than a rearrangement.
#define TILE_STORE 0x40000UL
#define TILE_STORE_SIZE (64UL * VIEW_COLUMNS * VIEW_ROWS)   // 88320

// **This is why hexwar does not have to bank out the C65 ROM.** The store runs
// $40000-$559FF, entirely inside the free banks 4 and 5, so the ROM area at
// $20000-$3FFFF is never wanted -- see hexwar.scm. A larger window would not
// fit and would drag in all the machinery the other two MEGA65 projects here
// carry, so the check is worth keeping.
#if (TILE_STORE + TILE_STORE_SIZE) > 0x60000UL
#error "the tile store runs past $60000"
#endif

// Write the window's character numbers into screen RAM. Once, at startup.
void view_init(void);

// Blank the whole store. Tiles are drawn with a transparent copy, so this has
// to happen before a redraw or the previous picture shows through the gaps.
void view_clear(void);

// Blit one 8x4-character tile at a position in *view* character coordinates,
// clipped to the window. The position may be negative: a tile half off the
// left edge draws its right-hand part, which is what a scrolled view needs.
void view_draw_tile(int x, int y, byte tile);

// Draw the map into the window. (off_x, off_y) is the map-space character
// coordinate that lands on the window's top left corner, so incrementing off_x
// scrolls the map one character -- eight pixels -- to the left.
void view_redraw(word off_x, word off_y);

// The map's full extent in screen characters, which is what bounds off_x and
// off_y. See src/map.h for where the 4 and 6 come from.
#define MAP_CHAR_WIDTH (4 * MAP_COLUMNS + 4)
#define MAP_CHAR_HEIGHT (6 * MAP_ROWS + 4)
#define VIEW_MAX_OFF_X (MAP_CHAR_WIDTH - VIEW_COLUMNS)
#define VIEW_MAX_OFF_Y (MAP_CHAR_HEIGHT - VIEW_ROWS)

#endif // HEXWAR_VIEW_H
