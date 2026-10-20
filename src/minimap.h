// The minimap: the whole world in a frame at the top of the panel, with an
// outline of the part the map window shows, as in
// civ1-reference/civ1-pic01.png.
//
// **It is full-colour characters, like the map window**, 16x5 of them, each
// with a 64-byte definition of its own in bank 5, so that together they are
// a 128x40-pixel framebuffer. A hexagon is two pixels across and one line
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
#include "view.h"

// Where it is on the screen, in characters, inside its frame.
#define MINIMAP_LEFT 1
#define MINIMAP_TOP 2
#define MINIMAP_COLUMNS 16
#define MINIMAP_ROWS 5

// Its character definitions, 80 of 64 bytes. Chip RAM, because the VIC-IV
// cannot fetch characters from attic RAM.
//
// **All of them are in the tile store's row padding**: each of the 23 rows
// has 256 bytes after the window's 3840 that the redraw never writes, four
// characters' worth (src/view.h), 92 in all. Character i of the minimap
// (row-major, 16 a row) is slot i % 4 of store row i / 4. (With the 128x32
// map it was 128 characters, and the last 36 were at $5F000.)
#define MINIMAP_PAD_SLOTS ((VIEW_ROW_STRIDE - 64 * VIEW_COLUMNS) / 64)     // 4
#define MINIMAP_PAD_CHARS (MINIMAP_PAD_SLOTS * VIEW_ROWS)                  // 92

// Each player's colour, a system palette entry, by owner (the barbarians
// last): the minimap's dots and the map's flags.
extern const byte player_colour[];

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
