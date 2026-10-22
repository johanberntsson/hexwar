// The help screen, and the Civilopedia's pages.
//
// Plain text over every cell of the screen, the map window's included, so
// game.c puts the map screen back on the way out as it does after the city
// screen. Nothing in the tile store is touched. **What it says is
// res/text/help.txt**, or a page's res/text/pedia_*.txt, read from attic RAM
// by text_show().

// In region 0 (src/region.h): called through the stubs in src/dispatch.s,
// under these names here.
#define help_screen help_screen_r0

#include "help.h"
#include "input.h"
#include "screen.h"
#include "text.h"

#pragma clang section text="r0text" rodata="r0rodata"

void help_screen(byte id)
{
    byte key;

    sprite_hide(2);             // the minimap's outline, which is gone
    sprite_hide(3);
    screen_clear();
    if (id != TEXT_HELP)        // a page, framed as Civilization's are
        screen_box(0, 0, SCREEN_COLUMNS - 1, SCREEN_ROWS - 1, COLOR_GREEN);
    text_show(id);

    for (;;) {
        screen_wait_raster(SCREEN_ROW_LINE(SCREEN_ROWS - 1));
        key = input_poll();
        if (key == KEY_HELP || key == KEY_ESC || key == KEY_RUNSTOP
            || key == KEY_RETURN || key == KEY_SPACE || key == KEY_CLICK)
            return;
    }
}
