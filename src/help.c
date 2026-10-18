// The help screen.
//
// Plain text over every cell of the screen, the map window's included, so
// game.c puts the map screen back on the way out as it does after the city
// screen. Nothing in the tile store is touched.

#include "help.h"
#include "input.h"
#include "screen.h"

#define HEADING_COLOUR COLOR_WHITE
#define KEY_COLOUR COLOR_YELLOW
#define TEXT_COLOUR COLOR_LIGHTGREEN
#define BUTTON_EXIT (ATTR_REVERSE | COLOR_RED)

#define LEFT 2                  // the left column's keys
#define RIGHT 42                // the right column's
#define KEY_WIDTH 10            // what a key is told from its meaning by

#define EXIT_ROW (SCREEN_ROWS - 1)
#define EXIT_TEXT " EXIT "
#define EXIT_LEFT (SCREEN_COLUMNS - 2 - (sizeof EXIT_TEXT - 1))

// A heading is an entry with no key. Each column is a list ending in a 0.
struct help_line { const char *key; const char *text; };

static const struct help_line left_column[] = {
    { 0, "Moving a unit" },
    { "", "north-west, north-east" },      // keys drawn by help_screen()
    { "", "west, east" },
    { "", "south-west, south-east" },
    { "click", "next to the unit: there" },
    { "", "onto an enemy: attack" },
    { 0, "" },
    { 0, "Orders" },
    { "b", "found a city (settler)" },
    { "f", "fortify" },
    { "w", "wait: ask again later" },
    { "space", "skip this unit's turn" },
    { "Return", "end the turn" },
    { 0, 0 },
};

static const struct help_line right_column[] = {
    { 0, "The map" },
    { "cursor", "scroll the view" },
    { "click", "a unit: give it orders" },
    { "", "elsewhere: centre there" },
    { "2 clicks", "on a city: city screen" },
    { "minimap", "click to centre there" },
    { 0, "" },
    { 0, "The game" },
    { "menu bar", "click a title to open it" },
    { "", "this screen" },                 // key drawn by help_screen()
    { "Esc", "quit to BASIC" },
    { 0, 0 },
};

// The keys that differ between a real MEGA65 and xemu, which has no HELP and
// is given the keypad (see KEY_PAD_FIRST): the three moving lines' keys, then
// the diagram's three rows.
static const char *const move_keys[2][6] = {
    { "0  -", "p  *", ":  =", " 0 -", "p U *", " : =" },
    { "pad 7  9", "pad 4  6", "pad 1  3", " 7 9", "4 U 6", " 1 3" },
};

static void draw_column(byte x, const struct help_line *l)
{
    byte y = 3;

    for (; l->text; ++l, ++y) {
        if (l->key) {
            screen_putsxy(x, y, l->key, KEY_COLOUR);
            screen_putsxy(x + KEY_WIDTH, y, l->text, TEXT_COLOUR);
        } else {
            screen_putsxy(x, y, l->text, HEADING_COLOUR);
        }
    }
}

void help_screen(void)
{
    const char *const *keys;
    byte key, i;

    sprite_hide(2);             // the minimap's outline, which is gone
    sprite_hide(3);
    screen_clear();
    screen_putsxy((SCREEN_COLUMNS - 13) / 2, 0, "Hexwar's keys", COLOR_WHITE);
    draw_column(LEFT, left_column);
    draw_column(RIGHT, right_column);
    keys = move_keys[input_real_hw ? 0 : 1];
    for (i = 0; i < 3; ++i) screen_putsxy(LEFT, 4 + i, keys[i], KEY_COLOUR);
    screen_putsxy(RIGHT, 12, input_real_hw ? "HELP" : "F1", KEY_COLOUR);

    // The six keys as they sit on the keyboard, round the unit they move.
    screen_putsxy(LEFT, 18, "The moving keys sit round the unit like the "
                  "six sides of its hexagon:", TEXT_COLOUR);
    for (i = 0; i < 3; ++i)
        screen_putsxy(LEFT + 2, 20 + i, keys[3 + i], KEY_COLOUR);
    screen_putsxy(0, EXIT_ROW, input_real_hw ? "HELP, Esc or Return to go back"
                                             : "F1, Esc or Return to go back",
                  COLOR_LIGHTGREY);
    screen_putsxy(EXIT_LEFT, EXIT_ROW, EXIT_TEXT, BUTTON_EXIT);

    for (;;) {
        screen_wait_raster(SCREEN_ROW_LINE(SCREEN_ROWS - 1));
        key = input_poll();
        if (key == KEY_HELP || key == KEY_ESC || key == KEY_RUNSTOP
            || key == KEY_RETURN || key == KEY_SPACE || key == KEY_CLICK)
            return;
    }
}
