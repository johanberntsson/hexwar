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
// hexagon (ax, 2*ay + (ax & 1)).
//
// **128x32, not Civilization 1's 80x50**, because in this layout a visual row
// of hexagons is every *other* column and an array row is *two* hexagon rows.
// 80x50 looked like 40 hexagons across by 100 down -- about 1:1.9 on the
// glass, portrait. 128x32 is 64 across by 64 down, 512 characters by 193 rows,
// which a character's 1:2 aspect makes about 1.33:1, landscape. One layer is
// 4096 bytes, near enough what 80x50 cost.
#ifndef HEXWAR_MAP_H
#define HEXWAR_MAP_H

#include "mega65.h"

#define MAP_COLUMNS 128
#define MAP_ROWS 32

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

// **The map wraps east to west**, so it repeats every MAP_WIDTH_CHARS
// characters across: column MAP_COLUMNS - 1 is followed by column 0, and its
// tile's right half overlaps column 0's left half exactly as any other pair of
// neighbours do. North and south do not wrap.
//
// That only works with an even number of columns. Column parity decides which
// hexagon row a cell is in, and an odd count would put column 0 in the wrong
// one each time round.
#define MAP_WIDTH_CHARS MAP_CELL_X(MAP_COLUMNS)                       // 512
#if MAP_COLUMNS & 1
#error "the east-west wrap needs an even MAP_COLUMNS"
#endif

// Terrain, one byte a hexagon. **The values are tile numbers** (TILE_* in
// src/sheet.h), because for now every terrain has exactly one tile; when that
// stops being true this layer keeps meaning terrain and the drawing grows a
// lookup table.
//
// **Bit 7 is not terrain: it says a city or a unit is on the hexagon**, and
// it is what lets view_redraw() find the few cells that need anything drawn
// over them without a second 4 KB layer or a search of the unit list per
// tile. unit_mark() keeps it right; read terrain through MAP_TERRAIN.
extern byte map_tile[MAP_ROWS][MAP_COLUMNS];
#define MAP_OCCUPIED 0x80
#define MAP_TERRAIN(AX, AY) (map_tile[AY][AX] & 0x7f)

// The six neighbours of a hexagon, clockwise from east. In doubled
// coordinates east and west are hx +/- 2 and the four diagonals hx +/- 1,
// hy +/- 1; the tiles are pointy-topped, so there is no north or south.
#define DIR_E 0
#define DIR_SE 1
#define DIR_SW 2
#define DIR_W 3
#define DIR_NW 4
#define DIR_NE 5
#define DIR_NONE 0xff

// The cell one step from (ax, ay) in direction `dir`, wrapping east-west.
// Returns 0, and leaves *nax and *nay alone, off the top or bottom.
byte map_neighbour(byte ax, byte ay, byte dir, byte *nax, byte *nay);

// Which way (bx, by) is from (ax, ay), or DIR_NONE if not a neighbour.
byte map_direction(byte ax, byte ay, byte bx, byte by);

// The map file, made by tools/mapgen.py (which documents the layout) and put
// on the disk next to the program by `make d81`.
//
// **The name is spelled in capitals because the disk spells it that way.**
// c1541 turns the host name hexwar.map into unshifted PETSCII, which is the
// bytes of ASCII "HEXWAR.MAP", and Calypsi passes literals through unchanged.
#define MAP_FILE_NAME "HEXWAR.MAP"
#define MAP_VERSION 1

// The cell whose hexagon is under map-space character (cx, cy), cx below
// MAP_WIDTH_CHARS. Tiles overlap, so a character can lie inside two tiles'
// rectangles; this picks the hexagon whose centre is nearest, measured on the
// glass, where a character is half as wide as it is tall.
void map_cell_at(word cx, word cy, byte *ax, byte *ay);

// Read the terrain layer of MAP_FILE_NAME into map_tile. Returns 0 if the
// file cannot be opened or is not a map of this size and version.
byte map_load(void);

#endif // HEXWAR_MAP_H
