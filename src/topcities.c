// The Top Five Cities screen: the five largest cities the player has seen,
// whoever's they are, with their size and people.
//
// Plain text over every cell of the screen, as the help screen is, so
// game.c puts the map screen back on the way out.

// In region 2 (src/region.h): called through the stubs in src/dispatch.s,
// under these names here.
#define top_cities_screen top_cities_screen_r2

#include "topcities.h"
#include "city.h"
#include "civ.h"
#include "input.h"
#include "map.h"
#include "screen.h"

#pragma clang section text="r2text" rodata="r2rodata"

#define TITLE "Top Five Cities"
#define BUTTON_EXIT (ATTR_REVERSE | COLOR_RED)
#define EXIT_ROW (SCREEN_ROWS - 1)
#define EXIT_TEXT " EXIT "
#define EXIT_LEFT (SCREEN_COLUMNS - 2 - (sizeof EXIT_TEXT - 1))
#define LIST_LEFT 22
#define LIST_TOP 6
#define PEOPLE_LEFT (LIST_LEFT + 16)
#define SIZE_LEFT (LIST_LEFT + 30)

static char digits[4];

static const char *number(byte v)
{
    byte i = sizeof digits - 1;
    digits[i] = 0;
    do { digits[--i] = (char)('0' + v % 10); v /= 10; } while (v);
    return digits + i;
}

void top_cities_screen(void)
{
    static char place[] = "1.";
    byte key, i, c, best, y;
    dword shown = 0;            // bit c: city c is already on the list

    sprite_hide(2);             // the minimap's outline, which is gone
    sprite_hide(3);
    screen_clear();
    screen_putsxy((SCREEN_COLUMNS - (sizeof TITLE - 1)) / 2, 2, TITLE,
                  COLOR_WHITE);
    for (i = 0; i < 5; ++i) {
        y = LIST_TOP + 2 * i;
        place[0] = '1' + i;
        screen_putsxy(LIST_LEFT, y, place, COLOR_YELLOW);
        // The largest city not listed yet, the first of equals.
        best = CITY_NONE;
        for (c = 0; c < city_count; ++c) {
            if ((shown >> c) & 1) continue;
            if (!MAP_IS_EXPLORED(city_ax[c], city_ay[c])) continue;
            if (best == CITY_NONE || city_size[c] > city_size[best]) best = c;
        }
        if (best == CITY_NONE) continue;
        shown |= 1UL << best;
        screen_putsxy(LIST_LEFT + 3, y, city_name_of(best), COLOR_WHITE);
        screen_putsxy(PEOPLE_LEFT, y, player_adjective(city_owner[best]),
                      COLOR_LIGHTGREEN);
        screen_putsxy(SIZE_LEFT, y, "size ", COLOR_LIGHTGREY);
        screen_putsxy(SIZE_LEFT + 5, y, number(city_size[best]),
                      COLOR_LIGHTGREEN);
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
