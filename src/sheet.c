#include "sheet.h"
#include "screen.h"

// The FCI image, linked in by build/tiles.s (tools/bin2s.py from
// res/tiles.fci). It is an ordinary array in the program area, so it can be
// read with a plain pointer -- no DMA needed on the source side.
extern const byte tiles_fci[];

// FCI header, nine bytes.
#define FCI_MAGIC0 'f'
#define FCI_ROWS 5
#define FCI_COLUMNS 6
#define FCI_OPTIONS 7
#define FCI_NUM_COLORS 8
#define FCI_PALETTE 9
// Bit 1 of the options byte: the first sixteen palette entries are the
// system's and must not be overwritten. res/tiles.fci has it set.
#define FCI_OPT_RESERVED_SYS_PALETTE 0x02

// One source character widened to two. The 128 bytes come out contiguous,
// which is the whole reason for doing it this way: they are one DMA job.
static byte expanded_pair[128];

// **Expand the sheet once at startup, not once per character drawn.**
//
// At H640/V200 a screen pixel is half as wide as it is tall, so a 32x32-pixel
// tile drawn as it comes out of the file looks squashed. The cc65 build fixed
// that at draw time: draw_tile_in_view in old-cc65/src/graphics.c ran an
// assembly routine (expand_fullcolor_char_x) over *every character of every
// tile*, through a shared 200-byte scratch buffer at $0334, and then issued
// two DMA jobs per source character.
//
// Doing it once at boot costs 96 iterations and leaves a sheet in which every
// tile is natively 8x4 characters. After this there is no expansion step in
// the drawing path, no shared scratch buffer to collide over, and a tile row
// is a single contiguous copy. The assembly file is gone with it.
static void sheet_expand(dword bitmap)
{
    byte row, col, y, x;
    const byte *src;
    dword dest;

    for (row = 0; row < SHEET_ROWS; ++row) {
        for (col = 0; col < SHEET_COLUMNS; ++col) {
            src = (const byte *)(word)(bitmap
                    + 64UL * ((word)row * SHEET_COLUMNS + col));
            // Each of the eight pixel rows: four source pixels into the first
            // destination character, four into the second, every one twice.
            for (y = 0; y < 8; ++y) {
                for (x = 0; x < 4; ++x) {
                    byte left = src[y * 8 + x];
                    byte right = src[y * 8 + 4 + x];
                    expanded_pair[y * 8 + 2 * x] = left;
                    expanded_pair[y * 8 + 2 * x + 1] = left;
                    expanded_pair[64 + y * 8 + 2 * x] = right;
                    expanded_pair[64 + y * 8 + 2 * x + 1] = right;
                }
            }
            dest = SHEET_EXPANDED + 64UL * ((word)row * SHEET_EXPANDED_COLUMNS
                                            + 2 * (word)col);
            lcopy((dword)(word)expanded_pair, dest, sizeof(expanded_pair));
        }
    }
}

byte sheet_init(void)
{
    byte num_colors, first, i;
    const byte *palette;
    word palette_size;
    dword bitmap;

    if (tiles_fci[0] != FCI_MAGIC0) return 0;
    if (tiles_fci[FCI_ROWS] != SHEET_ROWS) return 0;
    if (tiles_fci[FCI_COLUMNS] != SHEET_COLUMNS) return 0;

    num_colors = tiles_fci[FCI_NUM_COLORS];
    palette = tiles_fci + FCI_PALETTE;
    palette_size = 3u * ((word)num_colors + 1);

    // The pixels follow the palette and a three-byte "IMG" marker. **Upper
    // case, because that is what png2fci.py writes** -- Calypsi compiles
    // string literals as written, so a comparison against the outside world
    // has to be spelled the way the bytes really are.
    if (palette[palette_size] != 'I' || palette[palette_size + 1] != 'M'
        || palette[palette_size + 2] != 'G') {
        return 0;
    }
    bitmap = (dword)(word)(palette + palette_size + 3);

    // Entries 0-15 are the system's when the reserved flag is set, and
    // res/tiles.fci sets it. Entry 0 is the transparent index either way.
    first = (tiles_fci[FCI_OPTIONS] & FCI_OPT_RESERVED_SYS_PALETTE) ? 16 : 0;
    for (i = first; i <= num_colors; ++i) {
        const byte *entry = palette + 3u * i;
        screen_set_palette_entry(i, entry[0], entry[1], entry[2]);
        if (i == 255) break;    // num_colors is a byte; do not wrap
    }

    sheet_expand(bitmap);
    return 1;
}
