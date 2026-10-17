// The tile sheet: an FCI image read off the disk, its palette, and the
// x-expanded copy of it that every tile is actually drawn from.
//
// **FCI** (Full Colour Image) is the format tools/png2fci.py writes. A header
// of nine bytes, a palette of RGB triples, the marker "IMG", and then the
// pixels as a row-major grid of 8x8 characters, 64 bytes each, one byte of
// palette index per pixel. res/tiles.fci is 48x4 characters: twelve
// 4x4-character tiles side by side, in the order tiles/Makefile lists them.
// tools/mktiles.py writes it.
//
// **Palette index 0 is transparent.** The DMAgic's transparent copy is told to
// skip source bytes of 0 (see lcopy_transparent), which is what lets a hexagon
// be blitted over its neighbour without a rectangle of background around it.
#ifndef HEXWAR_SHEET_H
#define HEXWAR_SHEET_H

#include "mega65.h"

// Where the x-expanded sheet is built: bank 5, just above the view's tile
// store. 96 * 4 * 64 = 24576 bytes, $57000-$5CFFF.
//
// **Not attic RAM, where it used to be.** Search And Rescue measured DMA *out
// of* attic RAM on a real MEGA65 at 16.1 cycles a byte against 2.2 chip to
// chip, and a redraw reads about 160 KB of sheet -- three frames from attic,
// which would have brought the tearing back on the machine. xemu runs attic
// at chip speed, so it never showed. Every source address is then 16 bits in
// one bank, which is what view_draw_tile() wants anyway.
#define SHEET_EXPANDED 0x57000UL
#define SHEET_EXPANDED_BANK 0x05
#define SHEET_EXPANDED_ADDR 0x7000u

// The sheet as it is in the file, and as it is once expanded.
#define SHEET_COLUMNS 48
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
// Not terrain: these are drawn over a terrain tile, never instead of one.
#define TILE_CITY 6
#define TILE_SELECT 7           // the ring round the unit waiting for orders
#define TILE_SETTLER 8
#define TILE_WARRIOR 9
#define TILE_BUILDER 10
// Drawn *instead of* the terrain of a hexagon the player has not seen yet
// (MAP_EXPLORED in src/map.h): a solid black hexagon, never skipped, because
// the redraw does not clear and relies on every byte of the window being
// written. Its outline is a little inside the terrain tiles', but even the
// intersection of all the outlines tiles the plane, so any mixture of
// explored and unexplored hexagons still covers the window.
#define TILE_UNDISCOVERED 11

// The FCI on the disk. Capitals, because that is how diskutil.rb writes the
// host name tiles.fci -- see MAP_FILE_NAME.
#define SHEET_FILE_NAME "TILES.FCI"

#if SHEET_EXPANDED_ADDR + 64UL * SHEET_EXPANDED_COLUMNS * SHEET_ROWS > 0x10000UL
#error "the expanded sheet must fit in one bank for view_draw_tile"
#endif

// Read SHEET_FILE_NAME, upload its palette, and build the expanded sheet.
// Returns 0 if the file cannot be read or is not an FCI of the shape declared
// above.
byte sheet_load(void);

// Address of row `row` of tile `tile` in the expanded sheet. The eight
// characters of that row are TILE_EXPANDED_COLUMNS * 64 = 512 contiguous
// bytes, which is what makes a tile four DMA jobs rather than thirty-two.
#define SHEET_TILE_ROW(TILE, ROW)                              \
    (SHEET_EXPANDED + 64UL * ((dword)(ROW) * SHEET_EXPANDED_COLUMNS \
                              + (dword)(TILE) * TILE_EXPANDED_COLUMNS))

#endif // HEXWAR_SHEET_H
