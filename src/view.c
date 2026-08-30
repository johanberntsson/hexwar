#include "view.h"
#include "map.h"
#include "screen.h"
#include "sheet.h"

// One row of the window's screen words, built here and copied out in a single
// DMA job rather than 120 lpokes.
static byte row_words[VIEW_COLUMNS * 2];

void view_init(void)
{
    byte y, x;
    word charnum;

    // Cell (x, y) of the window gets the character number of its own 64 bytes
    // in the store: TILE_STORE/64 + row stride + column. With the row stride
    // padded to 4096 that is 64 characters a row, of which the window uses 60.
    for (y = 0; y < VIEW_ROWS; ++y) {
        charnum = (word)(TILE_STORE / 64) + (word)y * (VIEW_ROW_STRIDE / 64);
        for (x = 0; x < VIEW_COLUMNS; ++x) {
            row_words[2 * x] = (byte)charnum;
            row_words[2 * x + 1] = (byte)(charnum >> 8);
            ++charnum;
        }
        lcopy((dword)(word)row_words,
              SCREEN_CELL(VIEW_LEFT, VIEW_TOP + y),
              sizeof(row_words));
    }
}

void view_clear(void)
{
    // A DMA job's count is 16 bits, so the store is more than one job.
    dword at = TILE_STORE;
    dword left = TILE_STORE_SIZE;
    while (left) {
        word chunk = (left > 0x8000UL) ? 0x8000 : (word)left;
        lfill(at, 0, chunk);
        at += chunk;
        left -= chunk;
    }
}

void view_wait_for_redraw(void)
{
    screen_wait_raster(SCREEN_ROW_LINE(VIEW_TOP + VIEW_ROWS));
}

void view_draw_tile(int x, int y, byte tile)
{
    byte r;
    // The source is the expanded sheet in attic RAM, which is 12288 bytes at
    // a megabyte boundary -- so a source address is 16 bits and the megabyte
    // and bank never change. blit_begin() in view_redraw() has set them.
    word src = (word)(64U * ((word)tile * TILE_EXPANDED_COLUMNS));

    for (r = 0; r < TILE_ROWS; ++r, src += 64U * SHEET_EXPANDED_COLUMNS) {
        int row = y + r;
        int first = x;              // leftmost column this row would touch
        byte skip = 0;              // source characters clipped off the left
        byte width = TILE_EXPANDED_COLUMNS;

        if (row < 0 || row >= VIEW_ROWS) continue;

        if (first < 0) {
            if (first <= -TILE_EXPANDED_COLUMNS) continue;
            skip = (byte)(-first);
            width -= skip;
            first = 0;
        }
        if (first >= VIEW_COLUMNS) continue;
        if (first + width > VIEW_COLUMNS) width = (byte)(VIEW_COLUMNS - first);

        // **One DMA job per tile row, not one per character.** The eight
        // characters of a tile row are contiguous in the expanded sheet, and
        // the cells they land on are contiguous in the store, so the whole row
        // is a single copy. The same change was worth four times the speed in
        // ~/commodore/mega65/hexgame (fc_displayTile).
        //
        // All four arguments are 8 or 16 bit. Nothing here takes a 32-bit
        // address apart, which is what made the old lcopy_transparent path
        // three times the cost of the DMA it was asking for.
        blit(src + 64U * skip,
             VIEW_ROW_BANK(row),
             VIEW_ROW_ADDR(row) + 64U * (word)first,
             (word)width * 64);
    }
}

// **Scrolling is a whole-view redraw, and that is the point.**
//
// Neither earlier attempt at this worked. The cc65 build tried to shift the
// store with DMA row copies (scroll_up/scroll_down/scroll_left/scroll_right in
// old-cc65/src/graphics.c) and fill in the newly exposed edge; the XC=BASIC
// version did the same thing in fc_scrollMerge*. Both have to work out which
// tiles overlap a one-character strip and redraw them in the right order,
// against a lattice where a tile is eight characters wide and three tall.
//
// Redrawing the lot is simpler and, at about 320 DMA jobs, fits inside a frame.
//
// **It must not clear the store first, and it must draw top to bottom.** Both
// are what keep the redraw ahead of the raster; see the tearing note in
// CLAUDE.md. A clear would blank rows the beam has not reached yet, which is
// exactly the flash it is trying to avoid -- and it is unnecessary, because
// the hexagons tessellate and cover every byte of the window between them.
void view_redraw(word off_x, word off_y)
{
    byte hy;
    word hy_first, hy_last;
    word ax_first, ax_last;

    // Source megabyte $80 (the expanded sheet in attic RAM), bank 0 within it;
    // destination megabyte 0 (chip RAM); palette index 0 transparent.
    blit_begin(0x80, 0x00, 0x00, 0x00);

    // Which map cells can touch the window. A tile is 8 characters wide and
    // starts at 4*ax, and 4 rows tall starting at 3*hy, so anything beyond
    // these bounds is entirely outside and would only be clipped away.
    ax_first = (off_x < 8) ? 0 : (off_x - 8) / 4;
    ax_last = (off_x + VIEW_COLUMNS) / 4;
    if (ax_last >= MAP_COLUMNS) ax_last = MAP_COLUMNS - 1;

    hy_first = (off_y < 4) ? 0 : (off_y - 4) / 3;
    hy_last = (off_y + VIEW_ROWS) / 3;
    if (hy_last >= 2 * MAP_ROWS) hy_last = 2 * MAP_ROWS - 1;

    // **Iterate by hexagon row, not by array row.** Tiles overlap by one
    // character row, so a later draw has to be the lower one -- and within one
    // array row the odd columns sit three character rows below the even ones.
    // Stepping hy makes the screen y strictly increasing, which is both the
    // right painter's order and what lets the redraw stay ahead of the beam.
    for (hy = (byte)hy_first; hy <= (byte)hy_last; ++hy) {
        byte ay = hy >> 1;
        byte parity = hy & 1;
        word ax = ax_first;
        int ty = (int)(3 * (word)hy) - (int)off_y;

        // Only cells whose column parity matches this hexagon row exist.
        if ((ax & 1) != parity) ++ax;
        for (; ax <= ax_last; ax += 2) {
            view_draw_tile((int)(4 * ax) - (int)off_x, ty,
                           map_tile[ay][ax]);
        }
    }
}
