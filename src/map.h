// The hex map.
//
// **Doubled coordinates**, as in https://www.redblobgames.com/grids/hexagons/.
// A hexagon is (hx, hy) with hx and hy always the same parity, so a step east
// is hx += 2 and the four diagonal neighbours are (hx +/- 1, hy +/- 1):
//
//     0,0     2,0     4,0
//         1,1     3,1
//     0,2     2,2     4,2
//         1,3     3,3
//
// That halves neatly into a rectangular array: array cell (ax, ay) is the
// hexagon (ax, 2*ay + (ax & 1)). 80x50 is the Civilization 1 standard map size,
// and 4000 bytes is what one of its ten layers costs.
#ifndef HEXWAR_MAP_H
#define HEXWAR_MAP_H

#include "mega65.h"

#define MAP_COLUMNS 80
#define MAP_ROWS 50

// Where a map cell is drawn, in screen characters, relative to the map's own
// top left. **These are the two numbers the whole view geometry rests on.**
//
// A tile is 8x4 characters once the sheet is x-expanded (src/sheet.h), and a
// hexagon column step is hx += 2, so horizontally a cell is at 4*hx = 4*ax --
// which puts the odd, offset rows exactly half a tile across without a special
// case. Vertically a hexagon row is 3 characters, so the tiles overlap by one
// row, and that overlap is what the transparent copy is for.
//
// The cc65 build had these as x_hex2screen and y_hex2screen in
// old-cc65/src/map.c, spelled `x*4 + 4*(y%2)` and `6*(y/2) + 3*(y%2)`. The
// second is just 3*hy; the first was missing the doubling that x-expansion
// needs, which is one of the reasons that build never drew a correct map.
#define MAP_CELL_X(AX) (4 * (word)(AX))
#define MAP_CELL_Y(AX, AY) (6 * (word)(AY) + 3 * ((AX) & 1))

extern byte map_tile[MAP_ROWS][MAP_COLUMNS];

// Fill the map with placeholder terrain. Stands in for the Civ 1 generator
// described in design.md.
void map_init(void);

#endif // HEXWAR_MAP_H
