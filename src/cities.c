// Advisors -> Cities (23 Oct 2026; TODO.txt, the city advisor): the human's
// cities, after Civilization's City Status -- size, food a turn, shields,
// trade, and what each is producing and when -- and a city screen from it.
//
// **In region 6** with the menus' commands (src/command.c), which call it
// directly; it calls the city screen through its stub.

#include "city.h"
#include "cityscreen.h"
#include "civ.h"
#include "game.h"
#include "gameint.h"
#include "growth.h"
#include "input.h"
#include "screen.h"
#include "unit.h"

#pragma clang section text="r6text" rodata="r6rodata"

#define BUTTON_EXIT (ATTR_REVERSE | COLOR_RED)
#define EXIT_ROW (SCREEN_ROWS - 1)
#define EXIT_TEXT " EXIT "
#define EXIT_LEFT (SCREEN_COLUMNS - 2 - (sizeof EXIT_TEXT - 1))

#define TOP 4                   // the first city's row
#define ROWS 18                 // cities listed, at most
#define NAME_LEFT 3
#define PRODUCING_LEFT 45
#define FIGURES 4
static const char *const heading[FIGURES] = {
    "Size", "Food", "Shields", "Trade"
};
static const byte column_right[FIGURES] = { 20, 27, 35, 42 };

static byte listed[ROWS];       // the city on each row
static byte count;

static char digits[6];

static const char *number(word v)
{
    byte i = sizeof digits - 1;
    digits[i] = 0;
    do { digits[--i] = (char)('0' + v % 10); v /= 10; } while (v);
    return digits + i;
}

static byte length(const char *s)
{
    byte n = 0;
    while (s[n]) ++n;
    return n;
}

static void put_right(byte right, byte y, const char *s, byte colour)
{
    screen_putsxy(right + 1 - length(s), y, s, colour);
}

static void draw_row(byte i, byte selected)
{
    byte c = listed[i], y = TOP + i, eaten = 2 * city_size[c], x;
    byte colour = i == selected ? COLOR_WHITE : COLOR_LIGHTGREEN;
    const char *s;

    screen_putsxy(1, y, i == selected ? ">" : " ", COLOR_YELLOW);
    screen_putsxy(NAME_LEFT, y, city_name_of(c), colour);
    city_work(c);
    put_right(column_right[0], y, number(city_size[c]), colour);
    // Food a turn, after what the citizens eat.
    s = number(work_food >= eaten ? work_food - eaten : eaten - work_food);
    put_right(column_right[1], y, s, colour);
    screen_putsxy(column_right[1] - length(s), y,
                  work_food >= eaten ? "+" : "-",
                  work_food >= eaten ? colour : COLOR_RED);
    put_right(column_right[2], y, number(work_shields), colour);
    put_right(column_right[3], y, number(work_trade), colour);
    x = PRODUCING_LEFT;
    if (city_build[c] == KIND_NONE) {
        screen_putsxy(x, y, "nothing", COLOR_LIGHTGREY);
        return;
    }
    s = unit_kind_name[city_build[c]];
    screen_putsxy(x, y, s, colour);
    x += length(s) + 1;
    s = number(city_turns_left(c));
    screen_putsxy(x, y, s, COLOR_LIGHTGREY);
    screen_putsxy(x + length(s), y, city_turns_left(c) == 1 ? " turn"
                  : " turns", COLOR_LIGHTGREY);
}

// The list, with `selected` marked; until a city is picked (its index) or
// the player leaves (CITY_NONE).
static byte run(byte selected)
{
    byte c, i, key, was, row;

    sprite_hide(2);             // the minimap's outline, which is gone
    sprite_hide(3);
    screen_clear();
    screen_putsxy(NAME_LEFT, 1, "The cities of the", COLOR_WHITE);
    screen_putsxy(NAME_LEFT + 18, 1, player_people(PLAYER_HUMAN), COLOR_WHITE);
    screen_putsxy(NAME_LEFT, TOP - 1, "City", COLOR_LIGHTGREY);
    for (i = 0; i < FIGURES; ++i)
        put_right(column_right[i], TOP - 1, heading[i], COLOR_LIGHTGREY);
    screen_putsxy(PRODUCING_LEFT, TOP - 1, "Producing", COLOR_LIGHTGREY);

    count = 0;
    for (c = 0; c < city_count && count < ROWS; ++c)
        if (city_owner[c] == PLAYER_HUMAN) listed[count++] = c;
    if (!count) screen_putsxy(NAME_LEFT, TOP, "None yet", COLOR_LIGHTGREY);
    if (selected >= count) selected = 0;
    for (i = 0; i < count; ++i) draw_row(i, selected);
    screen_putsxy(0, EXIT_ROW, "Return or a click: the city   Esc: back",
                  COLOR_LIGHTGREY);
    screen_putsxy(EXIT_LEFT, EXIT_ROW, EXIT_TEXT, BUTTON_EXIT);

    for (;;) {
        screen_wait_raster(SCREEN_ROW_LINE(SCREEN_ROWS - 1));
        key = input_poll();
        was = selected;
        row = (byte)(input_y >> 3);
        if (input_moved() && row >= TOP && row < TOP + count)
            selected = row - TOP;
        if (key == KEY_UP && selected) --selected;
        if (key == KEY_DOWN && selected + 1 < count) ++selected;
        if (selected != was) {
            draw_row(was, selected);
            draw_row(selected, selected);
        }
        if (key == KEY_CLICK) {
            if (row == EXIT_ROW) return CITY_NONE;
            if (row >= TOP && row < TOP + count) return row - TOP;
        }
        if ((key == KEY_RETURN || key == KEY_SPACE) && count) return selected;
        if (key == KEY_ESC || key == KEY_RUNSTOP || key == 'x')
            return CITY_NONE;
    }
}

void cities_screen(void)
{
    byte i = 0;

    // Back to the list from a city's screen; the map once it is left.
    while ((i = run(i)) != CITY_NONE) {
        city_screen(listed[i]);
        // Its unit list may have woken a unit, as open_city() in game.c.
        if (unit_active == UNIT_NONE
            || unit_orders[unit_active] != ORDERS_NONE)
            select_unit(unit_active);
    }
    draw_screen();
}
