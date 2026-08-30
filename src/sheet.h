// The tile sheet: an FCI image linked into the program, its palette, and the
// x-expanded copy of it that every tile is actually drawn from.
//
// **FCI** (Full Colour Image) is the format tools/png2fci.py writes. A header
// of nine bytes, a palette of RGB triples, the marker "IMG", and then the
// pixels as a row-major grid of 8x8 characters, 64 bytes each, one byte of
// palette index per pixel. res/tiles.fci is 24x4 characters: six 4x4-character
// hex tiles side by side, in the order tiles/Makefile appends them.
//
// **Palette index 0 is transparent.** The DMAgic's transparent copy is told to
// skip source bytes of 0 (see lcopy_transparent), which is what lets a hexagon
// be blitted over its neighbour without a rectangle of background around it.
#ifndef HEXWAR_SHEET_H
#define HEXWAR_SHEET_H

#include "mega65.h"

// Where the x-expanded sheet is built, in attic RAM. 48 * 4 * 64 = 12288
// bytes. Attic RAM because it is the one place nothing else competes for, and
// because DMA reaches it as cheaply as anywhere else.
#define SHEET_EXPANDED 0x8000000UL

// The sheet as it is in the file, and as it is once expanded.
#define SHEET_COLUMNS 24
#define SHEET_ROWS 4
#define SHEET_EXPANDED_COLUMNS (SHEET_COLUMNS * 2)

// A tile is 4x4 characters in the file and 8x4 once expanded.
#define TILE_COLUMNS 4
#define TILE_ROWS 4
#define TILE_EXPANDED_COLUMNS (TILE_COLUMNS * 2)
#define TILE_COUNT (SHEET_COLUMNS / TILE_COLUMNS)

// Tile numbers, in the order tiles/Makefile appends the PNGs into the sheet.
#define TILE_DESERT_HILL 0
#define TILE_GRASS 1
#define TILE_MOUNTAIN 2
#define TILE_OCEAN 3
#define TILE_SHALLOW_OCEAN 4
#define TILE_TUNDRA 5

// Parse the linked FCI, upload its palette, and build the expanded sheet.
// Returns 0 and leaves nothing set up if the blob is not an FCI of the shape
// declared above.
byte sheet_init(void);

// Address of row `row` of tile `tile` in the expanded sheet. The eight
// characters of that row are TILE_EXPANDED_COLUMNS * 64 = 512 contiguous
// bytes, which is what makes a tile four DMA jobs rather than thirty-two.
#define SHEET_TILE_ROW(TILE, ROW)                              \
    (SHEET_EXPANDED + 64UL * ((dword)(ROW) * SHEET_EXPANDED_COLUMNS \
                              + (dword)(TILE) * TILE_EXPANDED_COLUMNS))

#endif // HEXWAR_SHEET_H
