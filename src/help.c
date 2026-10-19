// The help screen.
//
// Plain text over every cell of the screen, the map window's included, so
// game.c puts the map screen back on the way out as it does after the city
// screen. Nothing in the tile store is touched. **What it says is
// res/text/help.txt**, read from attic RAM by text_show().

#include "help.h"
#include "input.h"
#include "screen.h"
#include "text.h"

void help_screen(void)
{
    byte key;

    sprite_hide(2);             // the minimap's outline, which is gone
    sprite_hide(3);
    screen_clear();
    text_show(TEXT_HELP);

    for (;;) {
        screen_wait_raster(SCREEN_ROW_LINE(SCREEN_ROWS - 1));
        key = input_poll();
        if (key == KEY_HELP || key == KEY_ESC || key == KEY_RUNSTOP
            || key == KEY_RETURN || key == KEY_SPACE || key == KEY_CLICK)
            return;
    }
}
