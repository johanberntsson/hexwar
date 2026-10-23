#include "view.h"
#include "map.h"
#include "screen.h"
#include "sheet.h"
#include "unit.h"

#if TILE_STORE + TILE_STORE_SIZE > SHEET_EXPANDED
#error "the tile store runs into the expanded sheet"
#endif

// One row of the window's screen words, built here and copied out in a single
// DMA job rather than 120 lpokes. LOW_FREE, like the other scratch buffers
// here: rebuilt every time, and never in use across a disk call.
LOW_FREE static byte row_words[VIEW_COLUMNS * 2];

void view_restore(byte col, byte row, byte columns, byte rows)
{
    byte y, x;
    word charnum;
    dword cell;

    // Cell (x, y) of the window gets the character number of its own 64 bytes
    // in the store: TILE_STORE/64 + row stride + column. With the row stride
    // padded to 4096 that is 64 characters a row, of which the window uses 60.
    for (y = row; y < row + rows; ++y) {
        charnum = (word)(TILE_STORE / 64) + (word)y * (VIEW_ROW_STRIDE / 64)
                  + col;
        for (x = 0; x < columns; ++x) {
            row_words[2 * x] = (byte)charnum;
            row_words[2 * x + 1] = (byte)(charnum >> 8);
            ++charnum;
        }
        cell = SCREEN_CELL(VIEW_LEFT + col, VIEW_TOP + y);
        lcopy((dword)(word)row_words, cell, 2 * columns);
        // No colour and no attributes: blink and underline apply to
        // full-colour characters too.
        lfill(COLOR_BASE + (cell - SCREEN_BASE), 0, 2 * columns);
    }
}

void view_init(void)
{
    view_restore(0, 0, VIEW_COLUMNS, VIEW_ROWS);
}

void view_clear(void)
{
    // Only the window's 3840 bytes of each row: the padding after them holds
    // most of the minimap (src/minimap.h).
    byte r;
    for (r = 0; r < VIEW_ROWS; ++r)
        lfill(TILE_STORE + VIEW_ROW_STRIDE * r, 0, 64 * VIEW_COLUMNS);
}

void view_wait_for_redraw(void)
{
    screen_wait_raster(SCREEN_ROW_LINE(VIEW_TOP + VIEW_ROWS));
}

// The DMA's source bank as blit_begin() or view_draw_tile() last set it.
static byte source_bank;

void view_blit_begin(void)
{
    blit_begin(0x00, SHEET_EXPANDED_BANK, 0x00, 0x00);
    source_bank = SHEET_EXPANDED_BANK;
}

void view_draw_tile(int x, int y, byte tile)
{
    byte r, bank, lo, hi, k, cut;
    word src;

    // The source is the packed sheet (src/sheet.h), every row of which is
    // inside one bank -- so a source address is 16 bits and the megabyte
    // never changes. The bank does, for a row in bank 1, so it is one store
    // when it changes and a compare when it does not.
    k = tile << 2;                  // TILE_ROWS a tile in sheet_row_*
    for (r = 0; r < TILE_ROWS; ++r, ++k) {
        int row = y + r;
        // Only the characters of the row that have pixels, which are all
        // the sheet keeps: all eight in the middle of a terrain tile, two
        // or three of a unit's, and none of an overlay's empty rows.
        byte width = sheet_row_width[k];
        int first = x + sheet_row_lead[k];  // leftmost column it touches

        if (!width || row < 0 || row >= VIEW_ROWS) continue;

        cut = 0;                    // characters clipped off the left
        if (first < 0) {
            if (first <= -(int)width) continue;
            cut = (byte)(-first);
            width -= cut;
            first = 0;
        }
        if (first >= VIEW_COLUMNS) continue;
        if (first + width > VIEW_COLUMNS) width = (byte)(VIEW_COLUMNS - first);

        // **One DMA job per tile row, not one per character.** The
        // characters of a tile row are contiguous in the packed sheet, and
        // the cells they land on are contiguous in the store, so the whole row
        // is a single copy. The same change was worth four times the speed in
        // ~/commodore/mega65/hexgame (fc_displayTile).
        //
        // All four arguments are 8 or 16 bit. Nothing here takes a 32-bit
        // address apart, which is what made the old lcopy_transparent path
        // three times the cost of the DMA it was asking for.
        bank = sheet_row_bank[k];
        if (bank != source_bank) {
            blit_source_bank(bank);
            source_bank = bank;
        }
        lo = sheet_row_lo[k];
        hi = sheet_row_hi[k];
        src = ((word)hi << 8) | lo;
        // A shift by six is a loop of calls in Calypsi, and nearly every
        // row has nothing clipped: 1 ms of the redraw (22 Oct 2026).
        if (cut) src += (word)cut << 6;
        blit(src,
             VIEW_ROW_BANK(row),
             VIEW_ROW_ADDR(row) + 64U * (word)first,
             (word)width * 64);
    }
}

static const word flag_addr[PLAYERS + 1] = {
    VIEW_FLAG_ADDR(0), VIEW_FLAG_ADDR(1), VIEW_FLAG_ADDR(2),
    VIEW_FLAG_ADDR(3), VIEW_FLAG_ADDR(4),
};

void view_draw_flag(int x, int y, byte owner)
{
    byte row;
    // Unsigned, so that one test each takes the negative ones out too.
    if ((word)x > VIEW_COLUMNS - 2 || (word)y >= VIEW_ROWS) return;
    row = (byte)y;
    if (source_bank != VIEW_ROW_BANK(VIEW_FLAG_ROW)) {
        blit_source_bank(VIEW_ROW_BANK(VIEW_FLAG_ROW));
        source_bank = VIEW_ROW_BANK(VIEW_FLAG_ROW);
    }
    blit(flag_addr[owner], VIEW_ROW_BANK(row),
         VIEW_ROW_ADDR(row) + ((word)x << 6), 128);
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
//
// **Cities and units go one hexagon row behind the terrain.** They are drawn
// over the terrain tile of their own cell, but a unit's feet are in the
// tile's bottom character row, which is also the top row of the hexagons
// below -- drawn later, and opaque in the middle. So the cells of row hy that
// have MAP_OCCUPIED set are noted while its terrain is drawn, and their
// overlays are drawn once row hy + 1's terrain is down. That is still top to
// bottom, three character rows behind the terrain at worst, which costs the
// beam race nothing.
#define MAX_PENDING 16      // a window row holds at most 16 cells

byte view_mark_hy = 0xff;
int view_mark_x;

static byte pending_n[2];
LOW_FREE static byte pending_ax[2][MAX_PENDING];
LOW_FREE static int pending_tx[2][MAX_PENDING];
static word overlay_off_x;      // view_redraw()'s off_x

// **The resources go with them**, first, under any city or unit. They are
// in the middle of their own hexagon, which nothing drawn later covers, so
// a row behind is as good as straight after the terrain -- and their list is
// by hexagon row, so this looks at the row's two or three resources across
// the whole map instead of testing every hexagon in view, which cost 0.8 ms.
// Drawn after the improvements, so that both can be told apart.
static void draw_overlays(byte hy, int ty)
{
    byte list = hy & 1;
    byte i, k, n;
    byte ay = hy >> 1;
    byte tiles[2];
    word x;

    n = map_res_first[hy + 1];
    for (i = map_res_first[hy]; i < n; ++i) {
        k = map_res_ax[i];
        // Where its tile starts, across the seam if need be: in the window
        // if from -8 to VIEW_COLUMNS - 1.
        x = (MAP_CELL_X(k) + 8 - overlay_off_x) & (MAP_WIDTH_CHARS - 1);
        if (x >= VIEW_COLUMNS + 8
            || !((map_tile[ay][k] | map_show) & MAP_EXPLORED))
            continue;
        view_draw_tile((int)x - 8, ty,
                       RES_TILE(map_res_kind[i], map_tile[ay][k]));
    }

    for (i = 0; i < pending_n[list]; ++i) {
        n = unit_overlay(pending_ax[list][i], ay, tiles);
        for (k = 0; k < n; ++k)
            view_draw_tile(pending_tx[list][i], ty, tiles[k]);
        if (n) view_draw_flag(pending_tx[list][i], ty, unit_overlay_owner);
    }
    pending_n[list] = 0;
    // The cursor's ring with them: only the row below overlaps its tile,
    // and that row's terrain is already down.
    if (hy == view_mark_hy) view_draw_tile(view_mark_x, ty, TILE_SELECT);
}

void view_redraw(word off_x, word off_y)
{
    byte hy;
    word hy_first, hy_last;
    word vx_first, vx_last;
    byte ax_first;
    byte show = map_show;       // Orders/Show Map; a local for the loop

    // Source: chip RAM, bank 5 (the expanded sheet); destination megabyte 0
    // (chip RAM); palette index 0 transparent.
    view_blit_begin();

    // Which map cells can touch the window. A tile is 8 characters wide and
    // starts at 4*ax, and 4 rows tall starting at 3*hy, so anything beyond
    // these bounds is entirely outside and would only be clipped away.
    //
    // **Across, the map wraps**, so columns are counted in an unwrapped space
    // shifted one map width to the right -- vx = ax + MAP_COLUMNS -- which keeps
    // everything unsigned when the window starts within a tile of the seam.
    // Only the array index is wrapped, in the loop below.
    vx_first = (off_x + MAP_WIDTH_CHARS - 8) / 4;
    vx_last = (off_x + VIEW_COLUMNS) / 4 + MAP_COLUMNS;
    ax_first = (byte)(vx_first % MAP_COLUMNS);

    hy_first = (off_y < 4) ? 0 : (off_y - 4) / 3;
    hy_last = (off_y + VIEW_ROWS) / 3;
    if (hy_last >= 2 * MAP_ROWS) hy_last = 2 * MAP_ROWS - 1;

    pending_n[0] = pending_n[1] = 0;
    overlay_off_x = off_x;

    // **Iterate by hexagon row, not by array row.** Tiles overlap by one
    // character row, so a later draw has to be the lower one -- and within one
    // array row the odd columns sit three character rows below the even ones.
    // Stepping hy makes the screen y strictly increasing, which is both the
    // right painter's order and what lets the redraw stay ahead of the beam.
    for (hy = (byte)hy_first; hy <= (byte)hy_last; ++hy) {
        byte ay = hy >> 1;
        byte parity = hy & 1;
        byte list = parity;
        word vx = vx_first;
        byte ax = ax_first;
        int tx;
        int ty = (int)(3 * (word)hy) - (int)off_y;

        // Only cells whose column parity matches this hexagon row exist.
        // MAP_COLUMNS is even, so vx and ax always have the same parity.
        if ((vx & 1) != parity) {
            ++vx;
            if (++ax == MAP_COLUMNS) ax = 0;
        }
        tx = (int)(4 * vx) - (int)(off_x + MAP_WIDTH_CHARS);
        for (; vx <= vx_last; vx += 2, tx += 8) {
            byte t = map_tile[ay][ax] | show;
            // An unexplored hexagon is drawn black, not skipped -- the
            // store is never cleared -- and whatever is on it stays hidden.
            if (!(t & MAP_EXPLORED)) {
                t = TILE_UNDISCOVERED;
            } else if (t & MAP_OCCUPIED) {
                byte n = pending_n[list];
                if (n < MAX_PENDING) {
                    pending_ax[list][n] = ax;
                    pending_tx[list][n] = tx;
                    pending_n[list] = n + 1;
                }
            }
            view_draw_tile(tx, ty, t & MAP_TERRAIN_MASK);
            // An improvement goes straight over its own terrain: it is
            // inside its hexagon, which nothing drawn later covers. An
            // unexplored hexagon's t is TILE_UNDISCOVERED, with these bits
            // clear. Fish worked by a fishing boat has the bits too, but no
            // overlay: its resource tile is the picture, as a pasture's is.
            // The test costs nothing on the hexagons with no improvement.
            if (MAP_HAS_OVERLAY(t))
                view_draw_tile(tx, ty, MAP_OVERLAY_TILE(t));
            ax += 2;
            if (ax >= MAP_COLUMNS) ax -= MAP_COLUMNS;
        }
        if (hy != (byte)hy_first) draw_overlays(hy - 1, ty - 3);
    }
    draw_overlays((byte)hy_last, (int)(3 * hy_last) - (int)off_y);
}
