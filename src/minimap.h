// The minimap: the whole world in a frame at the top of the panel, with an
// outline of the part the map window shows, as in
// civ1-reference/civ1-pic01.png.
//
// **It is full-colour characters, like the map window**, 16x8 of them, each
// with a 64-byte definition of its own in bank 5, so that together they are
// a 128x64-pixel framebuffer. A hexagon is two pixels across and one line
// down -- square on the glass at H640/V200 -- and its hexagon row is its
// line, so the odd rows sit a pixel across from the even ones exactly as the
// tiles do: hexagon (hx, hy) is pixels hx and hx + 1 of line hy, wrapping at
// the seam like the map itself.
//
// **The view's outline is two sprites, not pixels.** Scrolling moves them
// and touches nothing else, so it costs the redraw's frame budget nothing.
// Sprite 2 is the outline; sprite 3 is the part of it that has wrapped round
// the seam to the left edge, shown only while the view straddles it.
#ifndef SEVENSHIPS_MINIMAP_H
#define SEVENSHIPS_MINIMAP_H

#include "mega65.h"

// Where it is on the screen, in characters, inside its frame.
#define MINIMAP_LEFT 1
#define MINIMAP_TOP 2
#define MINIMAP_COLUMNS 16
#define MINIMAP_ROWS 8

// Its character definitions, 16 * 8 * 64 = 8192 bytes, $5E000-$5FFFF: the
// top of bank 5, leaving the 4 KB under it for two more tiles of expanded
// sheet. Chip RAM, because the VIC-IV cannot fetch characters from attic RAM.
#define MINIMAP_STORE 0x5e000UL
#define MINIMAP_STORE_SIZE (64UL * MINIMAP_COLUMNS * MINIMAP_ROWS)

// Write the minimap's character numbers and frame into the screen and draw
// all of it. Once, when the game screen goes up.
void minimap_init(void);

// Only the character numbers and the frame, for putting the minimap back on
// the screen after something else has used it -- the city screen. The
// pixels are still there.
void minimap_show(void);

// Redraw hexagon (ax, ay) from what map_tile now says about it. Called when
// a hexagon is explored, and whenever unit_mark() looks at one.
void minimap_plot(byte ax, byte ay);

// Put the outline round the part of the map the window shows from
// (off_x, off_y), view_redraw()'s arguments.
void minimap_frame(word off_x, word off_y);

// The hexagon under (x, y) in the arrow's coordinates (input_x, input_y), or
// 0 if that is not over the minimap.
byte minimap_hex(word x, word y, byte *ax, byte *ay);

#endif // SEVENSHIPS_MINIMAP_H
