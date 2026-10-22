// The help screen, and the Civilopedia's pages.
//
// Plain text over every cell of the screen, the map window's included, so
// game.c puts the map screen back on the way out as it does after the city
// screen. Nothing in the tile store is touched. **What it says is
// res/text/help.txt**, or a page's res/text/pedia_*.txt, read from attic RAM
// by text_show(). A terrain's page has a picture as well: three hexagons of
// it in the map window's own cells, as the city screen's patch is, so the
// map is redrawn afterwards there too -- draw_screen() sees to that.

// In region 0 (src/region.h): called through the stubs in src/dispatch.s,
// under these names here.
#define help_screen help_screen_r0

#include "help.h"
#include "input.h"
#include "screen.h"
#include "sheet.h"
#include "text.h"
#include "unit.h"
#include "view.h"

#pragma clang section text="r0text" rodata="r0rodata"

// The picture, in screen cells: three hexagons, the middle one raised, as
// in Civilization's (civ1-reference/civ1-civilopedia03.png).
#define PICTURE_LEFT 20
#define PICTURE_TOP 2
#define PICTURE_COLUMNS (2 * TILE_EXPANDED_COLUMNS)         // 16
#define PICTURE_ROWS 7

#if PICTURE_LEFT < VIEW_LEFT || PICTURE_TOP < VIEW_TOP
#error "the picture has to be inside the map window, whose cells are the store's"
#endif

// Terrain t's picture, into the store under the picture's cells.
static void draw_picture(byte t)
{
    byte r;
    for (r = 0; r < PICTURE_ROWS; ++r)
        lfill(TILE_STORE + VIEW_ROW_STRIDE * (PICTURE_TOP - VIEW_TOP + r)
              + 64UL * (PICTURE_LEFT - VIEW_LEFT), 0, 64 * PICTURE_COLUMNS);
    view_blit_begin();
    view_draw_tile(PICTURE_LEFT - VIEW_LEFT, PICTURE_TOP - VIEW_TOP + 3, t);
    view_draw_tile(PICTURE_LEFT - VIEW_LEFT + 4, PICTURE_TOP - VIEW_TOP, t);
    view_draw_tile(PICTURE_LEFT - VIEW_LEFT + 8, PICTURE_TOP - VIEW_TOP + 3, t);
}

void help_screen(byte id)
{
    byte key, k;

    sprite_hide(2);             // the minimap's outline, which is gone
    sprite_hide(3);
    if (id >= TEXT_PEDIA_HILLS && id < TEXT_PEDIA_HILLS + TERRAIN_COUNT)
        draw_picture(id - TEXT_PEDIA_HILLS);
    // A unit's page (Help/Military Units): on grassland, a ship on the
    // shallows.
    k = id - TEXT_PEDIA_SETTLER + KIND_SETTLER;
    if (id >= TEXT_PEDIA_SETTLER && k < KINDS) {
        draw_picture(KIND_IS_SHIP(k) ? TILE_SHALLOW_OCEAN : TILE_GRASS);
        view_draw_tile(PICTURE_LEFT - VIEW_LEFT + 4, PICTURE_TOP - VIEW_TOP,
                       unit_kind_tile[k]);
    }
    screen_clear();
    if (id != TEXT_HELP) {      // a page, framed as Civilization's are
        screen_box(0, 0, SCREEN_COLUMNS - 1, SCREEN_ROWS - 1, COLOR_GREEN);
        view_restore(PICTURE_LEFT - VIEW_LEFT, PICTURE_TOP - VIEW_TOP,
                     PICTURE_COLUMNS, PICTURE_ROWS);
    }
    text_show(id);

    for (;;) {
        screen_wait_raster(SCREEN_ROW_LINE(SCREEN_ROWS - 1));
        key = input_poll();
        if (key == KEY_HELP || key == KEY_ESC || key == KEY_RUNSTOP
            || key == KEY_RETURN || key == KEY_SPACE || key == KEY_CLICK)
            return;
    }
}
