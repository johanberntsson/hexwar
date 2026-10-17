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

// **Rows are padded to 4096 bytes, and the padding buys 16-bit arithmetic.**
// A row of the window is 60 cells = 3840 bytes. Laid end to end, a row would
// start at an arbitrary offset and could straddle a 64 KB bank boundary, so
// every blit would need its destination taken apart from a 32-bit address --
// which measured at 57.6 us a job against the DMAgic's own 27.2, and a redraw
// is 320 jobs.
//
// At 4096 bytes a row, sixteen rows fit exactly in a bank: row r is at bank
// `4 + (r >> 4)`, offset `(r & 15) << 12`, and since a row is 3840 bytes it
// can never cross into the next bank. Both are byte and word operations. The
// padding costs 5888 bytes of RAM that nothing else wanted.
// Plain integer constants, not casts: these appear in the #if checks below,
// where the preprocessor has no idea what a `dword` is.
#define VIEW_ROW_STRIDE 4096UL
#define TILE_STORE_SIZE (VIEW_ROW_STRIDE * VIEW_ROWS)            // 94208
#define VIEW_ROW_BANK(R) ((byte)(4 + ((R) >> 4)))
#define VIEW_ROW_ADDR(R) ((word)(((R) & 15) << 12))

#if (64 * VIEW_COLUMNS) > VIEW_ROW_STRIDE
#error "a window row does not fit in VIEW_ROW_STRIDE"
#endif
#if (TILE_STORE & 0xffff) != 0
#error "VIEW_ROW_BANK assumes the store starts on a 64 KB boundary"
#endif

// **This is why hexwar does not have to bank out the C65 ROM.** The store runs
// $40000-$56EBF, entirely inside the free banks 4 and 5, so the ROM area at
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
// scrolls the map one character -- eight pixels -- to the left. off_x must be
// below MAP_WIDTH_CHARS; the map wraps east-west, and a window that straddles
// the seam simply carries on into column 0.
//
// **It does not clear first, and it draws strictly top to bottom.** Both are
// deliberate; see the tearing note in view.c. Call view_wait_for_redraw()
// immediately before it.
void view_redraw(word off_x, word off_y);

// Block until the beam has just left the bottom of the window, which is where
// a redraw has to start if it is to stay ahead of it.
//
// **This is half of what stopped the tearing.** A redraw takes about a third
// of a frame and writes the window top to bottom; starting it here gives it
// the bottom border and the vertical blank as a head start -- roughly eight
// milliseconds -- so every row is rewritten well before the beam comes back
// round to display it. The other half is that view_redraw() does not blank the
// store first.
void view_wait_for_redraw(void);

// **How far the view may travel, and why it stops short of the map's edge.**
//
// view_redraw() does not clear the store; it relies on the hexagons covering
// every byte of the window between them. They do -- but only where a cell has
// all its neighbours. At the very top of the map the first hexagon row has no
// row above it to fill the notches between its tiles, and a redraw at (0,0)
// measured 4256 bytes of the store that no tile wrote. Off the map's edge
// there is nothing to draw, so those bytes keep whatever the previous frame
// left in them.
//
// Holding the view one hexagon inside the map at the top and bottom makes
// every contributing row exist, and costs nothing at run time. The bounds come
// from asking which cells cover the window's outermost rows: a tile spans 4
// rows from 3*hy, so window row 0 needs hy >= (off_y-3)/3.
//
// **There is no such limit across**, because the map wraps east-west and every
// column has both its neighbours. off_x runs freely round 0..MAP_WIDTH_CHARS-1.
#define VIEW_MIN_OFF_Y 4
#define VIEW_MAX_OFF_Y (3 * (2 * MAP_ROWS - 1) - VIEW_ROWS)        // 166

#endif // HEXWAR_VIEW_H
