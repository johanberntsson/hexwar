#include <fcntl.h>
#include <calypsi/stubs.h>

#include "sheet.h"
#include "screen.h"

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

// One source character as read from the file, and widened to two. The 128
// bytes come out contiguous, which is the whole reason for doing it this
// way: they are one DMA job. HIGH_BSS rather than LOW_FREE, since they are
// in use between the disk reads.
HIGH_BSS static byte source_char[64];
HIGH_BSS static byte expanded_pair[128];

// **Expand the sheet once, as it is read, not once per character drawn.**
//
// At H640/V200 a screen pixel is half as wide as it is tall, so a 32x32-pixel
// tile drawn as it comes out of the file looks squashed. The cc65 build fixed
// that at draw time: draw_tile_in_view in old-cc65/src/graphics.c ran an
// assembly routine (expand_fullcolor_char_x) over *every character of every
// tile*, through a shared 200-byte scratch buffer at $0334, and then issued
// two DMA jobs per source character.
//
// Doing it once at boot leaves a sheet in which every tile is natively 8x4
// characters. After this there is no expansion step in the drawing path, no
// shared scratch buffer to collide over, and a tile row is a single
// contiguous copy.
static void expand_char(byte row, byte col)
{
    byte y, x;
    for (y = 0; y < 8; ++y) {
        const byte *src = source_char + 8 * y;
        byte *left = expanded_pair + 8 * y;
        byte *right = expanded_pair + 64 + 8 * y;
        for (x = 0; x < 4; ++x) {
            left[2 * x] = left[2 * x + 1] = src[x];
            right[2 * x] = right[2 * x + 1] = src[4 + x];
        }
    }
    lcopy((dword)(word)expanded_pair,
          SHEET_EXPANDED + 64UL * ((word)row * SHEET_EXPANDED_COLUMNS
                                   + 2 * (word)col),
          sizeof(expanded_pair));
}

// **The sheet comes off the disk rather than being linked in**, which gave
// back the 6.5 KB it used to take of the 32 KB program region and stops that
// growing with every tile. Nothing of the file is kept: the header and each
// palette entry are read into a few bytes and used, and each character is
// read, widened and sent to bank 5 before the next.
//
// **Every read is under 256 bytes**, for the reason in map_load(): the
// Calypsi stub miscounts a request that ends the file on a page boundary.
static byte read_sheet(int fd)
{
    static byte header[FCI_PALETTE];
    byte rgb[3];
    byte num_colors, first, i, row, col;

    if (_Stub_read(fd, header, sizeof header) != sizeof header) return 0;
    if (header[0] != FCI_MAGIC0) return 0;
    if (header[FCI_ROWS] != SHEET_ROWS) return 0;
    if (header[FCI_COLUMNS] != SHEET_COLUMNS) return 0;

    // Entries 0-15 are the system's when the reserved flag is set, and
    // res/tiles.fci sets it; they are in the file as zeros and skipped.
    // Entry 0 is the transparent index either way.
    num_colors = header[FCI_NUM_COLORS];
    first = (header[FCI_OPTIONS] & FCI_OPT_RESERVED_SYS_PALETTE) ? 16 : 0;
    i = 0;
    do {
        if (_Stub_read(fd, rgb, 3) != 3) return 0;
        if (i >= first) screen_set_palette_entry(i, rgb[0], rgb[1], rgb[2]);
    } while (i++ != num_colors);   // num_colors is a byte; do not wrap

    // **Upper case, because that is what the tools write** -- Calypsi
    // compiles string literals as written, so a comparison against the
    // outside world has to be spelled the way the bytes really are.
    if (_Stub_read(fd, rgb, 3) != 3) return 0;
    if (rgb[0] != 'I' || rgb[1] != 'M' || rgb[2] != 'G') return 0;

    for (row = 0; row < SHEET_ROWS; ++row) {
        for (col = 0; col < SHEET_COLUMNS; ++col) {
            if (_Stub_read(fd, source_char, 64) != 64) return 0;
            expand_char(row, col);
        }
    }
    return 1;
}

byte sheet_load(void)
{
    byte ok;
    int fd = _Stub_open(SHEET_FILE_NAME, O_RDONLY | O_BINARY);
    if (fd < 0) return 0;
    ok = read_sheet(fd);
    _Stub_close(fd);
    return ok;
}
