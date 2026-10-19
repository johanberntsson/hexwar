// The Top Five Cities screen.
//
// Plain text over every cell of the screen, as the help screen is, so
// game.c puts the map screen back on the way out.

#include "topcities.h"
#include "input.h"
#include "screen.h"

#define TITLE "Top Five Cities"
#define BUTTON_EXIT (ATTR_REVERSE | COLOR_RED)
#define EXIT_ROW (SCREEN_ROWS - 1)
#define EXIT_TEXT " EXIT "
#define EXIT_LEFT (SCREEN_COLUMNS - 2 - (sizeof EXIT_TEXT - 1))
#define LIST_LEFT 30
#define LIST_TOP 6

void top_cities_screen(void)
{
    static char place[] = "1.";
    byte key, i;

    sprite_hide(2);             // the minimap's outline, which is gone
    sprite_hide(3);
    screen_clear();
    screen_putsxy((SCREEN_COLUMNS - (sizeof TITLE - 1)) / 2, 2, TITLE,
                  COLOR_WHITE);
    for (i = 0; i < 5; ++i) {
        place[0] = '1' + i;
        screen_putsxy(LIST_LEFT, LIST_TOP + 2 * i, place, COLOR_YELLOW);
    }
    screen_putsxy(0, EXIT_ROW, "Esc or Return to go back", COLOR_LIGHTGREY);
    screen_putsxy(EXIT_LEFT, EXIT_ROW, EXIT_TEXT, BUTTON_EXIT);

    for (;;) {
        screen_wait_raster(SCREEN_ROW_LINE(SCREEN_ROWS - 1));
        key = input_poll();
        if (key == KEY_ESC || key == KEY_RUNSTOP || key == KEY_RETURN
            || key == KEY_SPACE || key == KEY_CLICK)
            return;
    }
}
