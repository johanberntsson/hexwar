// The tile sheet: an FCI image read off the disk, its palette, and the
// x-expanded copy of it that every tile is actually drawn from.
//
// **FCI** (Full Colour Image) is the format tools/png2fci.py writes. A header
// of nine bytes, a palette of RGB triples, the marker "IMG", and then the
// pixels as a row-major grid of 8x8 characters, 64 bytes each, one byte of
// palette index per pixel. res/tiles.fci is 132x4 characters: 33
// 4x4-character tiles side by side, in the order tiles/Makefile lists them.
// tools/mktiles.py writes it.
//
// **Palette index 0 is transparent.** The DMAgic's transparent copy is told to
// skip source bytes of 0 (see lcopy_transparent), which is what lets a hexagon
// be blitted over its neighbour without a rectangle of background around it.
#ifndef SEVENSHIPS_SHEET_H
#define SEVENSHIPS_SHEET_H

#include "mega65.h"

// Where the x-expanded sheet is built. **Each tile is 2 KB in one piece**,
// its four rows of eight characters one after the other, 512 bytes a row, so
// a tile's address is its base plus row * 512 wherever it is. The first
// SHEET_LOW_TILES are in bank 5, just above the view's tile store,
// $57000-$5FFFF, the rest of the bank since the minimap moved into the
// store's padding; the rest are in bank 1 at $17000-$1EFFF, where code
// regions 3 to 6 would have gone (src/region.h). CLAUDE.md, "When bank 5
// fills up", has why there.
//
// **Not attic RAM, where it used to be.** Search And Rescue measured DMA *out
// of* attic RAM on a real MEGA65 at 16.1 cycles a byte against 2.2 chip to
// chip, and a redraw reads about 160 KB of sheet -- three frames from attic,
// which would have brought the tearing back on the machine. xemu runs attic
// at chip speed, so it never showed. Bank 1 is chip RAM as well. Every
// source address is 16 bits in its bank, which is what view_draw_tile()
// wants; it changes the DMA's source bank only when a tile is in the other.
#define SHEET_EXPANDED 0x57000UL
#define SHEET_EXPANDED_BANK 0x05
#define SHEET_EXPANDED_ADDR 0x7000u
#define SHEET_LOW_TILES 18
#define SHEET_HIGH 0x17000UL
#define SHEET_HIGH_BANK 0x01
#define SHEET_HIGH_ADDR 0x7000u
#define SHEET_HIGH_TILES 16

// The sheet as it is in the file.
#define SHEET_COLUMNS 132
#define SHEET_ROWS 4

// A tile is 4x4 characters in the file and 8x4 once expanded.
#define TILE_COLUMNS 4
#define TILE_ROWS 4
#define TILE_EXPANDED_COLUMNS (TILE_COLUMNS * 2)
#define TILE_COUNT (SHEET_COLUMNS / TILE_COLUMNS)
#define TILE_ROW_BYTES (64U * TILE_EXPANDED_COLUMNS)       // 512
#define TILE_BYTES (TILE_ROW_BYTES * TILE_ROWS)             // 2048

#if SHEET_EXPANDED + 2048UL * SHEET_LOW_TILES > 0x60000UL
#error "the low part of the sheet runs past bank 5"
#endif
#if TILE_COUNT > SHEET_LOW_TILES + SHEET_HIGH_TILES
#error "more tiles than the sheet has room for"
#endif

// Tile numbers, in the order tiles/Makefile appends the PNGs into the sheet.
// **The terrains come first**, because a terrain value is its tile number
// (map_tile in src/map.h); TERRAIN_COUNT of them.
#define TILE_HILLS 0
#define TILE_GRASS 1
#define TILE_MOUNTAIN 2
#define TILE_OCEAN 3
#define TILE_SHALLOW_OCEAN 4
#define TILE_TUNDRA 5
#define TILE_PLAINS 6
#define TILE_FOREST 7           // trees baked over the plains (tiles/Makefile)
#define TILE_DESERT 8
#define TILE_FLOODPLAIN 9       // a river valley in the dry land: the grain
#define TERRAIN_COUNT 10
// Not terrain: these are drawn over a terrain tile, never instead of one.
#define TILE_CITY 10
#define TILE_SELECT 11          // a ring: the tile-select cursor (Tab)
#define TILE_SETTLER 12
#define TILE_WARRIOR 13
#define TILE_BUILDER 14
// Drawn *instead of* the terrain of a hexagon the player has not seen yet
// (MAP_EXPLORED in src/map.h): a solid black hexagon, never skipped, because
// the redraw does not clear and relies on every byte of the window being
// written. Its outline is a little inside the terrain tiles', but even the
// intersection of all the outlines tiles the plane, so any mixture of
// explored and unexplored hexagons still covers the window.
#define TILE_UNDISCOVERED 15
// The builders' improvements, drawn over the terrain
// straight after it (MAP_OVERLAY_TILE in src/map.h, in MAP_IMPROVEMENT's
// order).
#define TILE_OVERLAY_FIRST 16
#define TILE_IRRIGATION 16
#define TILE_MINE 17
// The resources, from here on in RES_* order (src/map.h): resource r is
// tile TILE_RESOURCE_FIRST - 1 + r. All in bank 1.
#define TILE_RESOURCE_FIRST 18
// Copper and tin once mined, drawn instead of the resource (RES_TILE in
// src/map.h); the bronze unit; and cattle, olives and wine once irrigated,
// the same way. One tile left in bank 1 after them.
#define TILE_COPPER_MINE 27
#define TILE_TIN_MINE 28
#define TILE_SPEARMAN 29
#define TILE_CATTLE_PASTURE 30
#define TILE_OLIVES_GROVE 31
#define TILE_WINE_VINEYARD 32

// The FCI on the disk. Capitals, because that is how diskutil.rb writes the
// host name tiles.fci -- see MAP_FILE_NAME.
#define SHEET_FILE_NAME "TILES.FCI"

#if SHEET_EXPANDED_ADDR + 2048UL * SHEET_LOW_TILES > 0x10000UL \
    || SHEET_HIGH_ADDR + 2048UL * SHEET_HIGH_TILES > 0x10000UL
#error "each part of the expanded sheet must fit in its bank for view_draw_tile"
#endif

// **Where each tile row has pixels**, worked out by sheet_load(): the first
// expanded character with any (lead) and how many from there to the last
// (width), 0 for a row with none. Indexed by tile * TILE_ROWS + row.
// view_draw_tile() copies only that much, which is what keeps an overlay or a
// unit -- a small picture in the middle of its tile -- cheap enough for the
// redraw: CLAUDE.md, "Tearing". In HIGH_BSS; sheet_load() sets all of it.
extern byte sheet_row_lead[TILE_COUNT * TILE_ROWS];
extern byte sheet_row_width[TILE_COUNT * TILE_ROWS];

// Read SHEET_FILE_NAME, upload its palette, and build the expanded sheet.
// Returns 0 if the file cannot be read or is not an FCI of the shape declared
// above.
byte sheet_load(void);

// Address of tile `tile` in the expanded sheet. The eight characters of
// each of its rows are TILE_ROW_BYTES contiguous bytes, which is what makes a
// tile four DMA jobs rather than thirty-two.
#define SHEET_TILE(TILE)                                                    \
    ((TILE) < SHEET_LOW_TILES                                               \
         ? SHEET_EXPANDED + (dword)TILE_BYTES * (TILE)                      \
         : SHEET_HIGH + (dword)TILE_BYTES * ((TILE) - SHEET_LOW_TILES))

#endif // SEVENSHIPS_SHEET_H
