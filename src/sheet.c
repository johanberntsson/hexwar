// In region 0 (src/region.h): called through the stubs in src/dispatch.s,
// under these names here.
#define sheet_load sheet_load_r0

#include <fcntl.h>
#include <calypsi/stubs.h>

#include "sheet.h"
#include "minimap.h"
#include "screen.h"
#include "unit.h"
#include "view.h"

#pragma clang section text="r0text" rodata="r0rodata"

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

HIGH_BSS byte sheet_row_lead[TILE_COUNT * TILE_ROWS];
HIGH_BSS byte sheet_row_width[TILE_COUNT * TILE_ROWS];

// Note which expanded characters of a tile row have a pixel in them, as the
// row's characters go past left to right: the left half of a source
// character is expanded character 2c, the right half 2c + 1. The width
// holds the end until the row's last character turns it into a width.
static void note_extent(byte row, byte col)
{
    byte c = col % TILE_COLUMNS;
    byte k = (byte)((col / TILE_COLUMNS) * TILE_ROWS + row);
    byte y, left = 0, right = 0;

    for (y = 0; y < 64; y += 8) {
        left |= source_char[y] | source_char[y + 1] | source_char[y + 2]
                | source_char[y + 3];
        right |= source_char[y + 4] | source_char[y + 5] | source_char[y + 6]
                 | source_char[y + 7];
    }
    if (c == 0) {
        sheet_row_lead[k] = 0xff;
        sheet_row_width[k] = 0;
    }
    if (left || right) {
        byte from = left ? 2 * c : 2 * c + 1;
        if (from < sheet_row_lead[k]) sheet_row_lead[k] = from;
        sheet_row_width[k] = right ? 2 * c + 2 : 2 * c + 1;
    }
    if (c == TILE_COLUMNS - 1) {
        if (sheet_row_lead[k] == 0xff) sheet_row_lead[k] = 0;
        else sheet_row_width[k] -= sheet_row_lead[k];
    }
}

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
    byte tile = col / TILE_COLUMNS;
    dword at = SHEET_TILE(tile);
    lcopy((dword)(word)expanded_pair,
          at + (word)row * TILE_ROW_BYTES + 128U * (col % TILE_COLUMNS),
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
            note_extent(row, col);
            expand_char(row, col);
        }
    }
    return 1;
}

// **The owners' flags** (VIEW_FLAG_ROW in src/view.h), drawn here rather
// than read: a pennant on a pole, in source pixels, which are widened to two
// as the sheet's are. 'c' is the owner's colour; the pole and the border are
// greys, since palette index 0 is transparent. File scope: see the Calypsi
// notes in CLAUDE.md about small const arrays inside a function.
static const char flag_shape[8][9] = {
    "pkkkkkkk",
    "pkccccck",
    "pkccccck",
    "pkccccck",
    "pkkkkkkk",
    "p.......",
    "p.......",
    "p.......",
};
#define FLAG_POLE 12            // grey
#define FLAG_BORDER 11          // dark grey

static void make_flags(void)
{
    byte o, x, y, c;
    byte *p;

    for (o = 0; o <= PLAYERS; ++o) {
        for (y = 0; y < 8; ++y)
            for (x = 0; x < 8; ++x) {
                switch (flag_shape[y][x]) {
                case 'p': c = FLAG_POLE; break;
                case 'k': c = FLAG_BORDER; break;
                case 'c': c = player_colour[o]; break;
                default: c = 0;
                }
                // Source pixel x is expanded pixels 2x and 2x + 1: character
                // x / 4 of the two, column 2 * (x % 4) of its line.
                p = expanded_pair + 64 * (x >> 2) + 8 * y + 2 * (x & 3);
                p[0] = p[1] = c;
            }
        lcopy((dword)(word)expanded_pair, VIEW_FLAG(o), sizeof expanded_pair);
    }
}

byte sheet_load(void)
{
    byte ok;
    int fd = _Stub_open(SHEET_FILE_NAME, O_RDONLY | O_BINARY);
    if (fd < 0) return 0;
    ok = read_sheet(fd);
    _Stub_close(fd);
    if (ok) make_flags();
    return ok;
}
