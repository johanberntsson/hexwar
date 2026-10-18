#include "menu.h"
#include "input.h"
#include "minimap.h"
#include "screen.h"
#include "view.h"

#define BAR_COLOUR COLOR_LIGHTGREEN
#define ITEM_COLOUR COLOR_WHITE
#define BORDER_COLOUR COLOR_LIGHTGREY
#define SELECTED(C) (ATTR_REVERSE | (C))

#define MENUS 5
#define DROP_TOP 1              // the dropdown's frame; items start a row below

static const char *const menu_title[MENUS] = {
    "Game", "Orders", "Advisors", "World", "Civilopedia",
};
static const byte menu_x[MENUS] = { 0, 5, 12, 21, 27 };

// Each menu's items and what they ask for. An empty menu does not open.
static const char *const game_items[] = { "Retire" };
static const byte game_commands[] = { MENU_RETIRE };

// Show Map is for debugging: it lifts the fog of war, and again puts it back.
static const char *const orders_items[] = { "Show Map" };
static const byte orders_commands[] = { MENU_SHOW_MAP };

static const char *const *const menu_items[MENUS] = {
    game_items, orders_items, 0, 0, 0
};
static const byte *const menu_commands[MENUS] = {
    game_commands, orders_commands, 0, 0, 0
};
static const byte menu_count[MENUS] = { 1, 1, 0, 0, 0 };

static byte length(const char *s)
{
    byte n = 0;
    while (s[n]) ++n;
    return n;
}

void menu_draw(void)
{
    byte m;
    for (m = 0; m < MENUS; ++m)
        screen_putsxy(menu_x[m], 0, menu_title[m], BAR_COLOUR);
}

// The open dropdown.
static byte drop_menu, drop_left, drop_width;

static void draw_item(byte i, byte selected)
{
    byte colour = (i == selected) ? SELECTED(ITEM_COLOUR) : ITEM_COLOUR;
    byte y = DROP_TOP + 1 + i;
    screen_fill(drop_left + 1, y, drop_width - 2, colour);
    screen_putsxy(drop_left + 2, y, menu_items[drop_menu][i], colour);
}

static byte item_under_pointer(void)
{
    byte col = (byte)(input_x >> 2);
    byte row = (byte)(input_y >> 3);
    if (col <= drop_left || col >= drop_left + drop_width - 1) return 0xff;
    if (row <= DROP_TOP || row > DROP_TOP + menu_count[drop_menu]) return 0xff;
    return row - DROP_TOP - 1;
}

byte menu_click(void)
{
    byte col = (byte)(input_x >> 2);
    byte m, i, n, key, sel = 0xff, was, right, result = MENU_NONE;

    for (m = 0; m < MENUS; ++m)
        if (col >= menu_x[m] && col < menu_x[m] + length(menu_title[m]))
            break;
    if (m == MENUS || menu_count[m] == 0) return MENU_NONE;

    n = menu_count[m];
    drop_menu = m;
    drop_left = menu_x[m];
    drop_width = length(menu_title[m]);
    for (i = 0; i < n; ++i)
        if (length(menu_items[m][i]) > drop_width)
            drop_width = length(menu_items[m][i]);
    drop_width += 4;

    sprite_hide(2);             // the minimap's outline would show through
    sprite_hide(3);
    screen_putsxy(menu_x[m], 0, menu_title[m], SELECTED(BAR_COLOUR));
    screen_box(drop_left, DROP_TOP, drop_left + drop_width - 1,
               DROP_TOP + n + 1, BORDER_COLOUR);
    for (i = 0; i < n; ++i) draw_item(i, sel);

    for (;;) {
        screen_wait_raster(SCREEN_ROW_LINE(SCREEN_ROWS - 1));
        key = input_poll();
        was = sel;
        if (input_moved()) sel = item_under_pointer();
        if (key == KEY_UP) sel = (sel == 0xff || sel == 0) ? 0 : sel - 1;
        if (key == KEY_DOWN)
            sel = (sel == 0xff) ? 0 : (sel < n - 1 ? sel + 1 : sel);
        if (sel != was) {
            if (was != 0xff) draw_item(was, sel);
            if (sel != 0xff) draw_item(sel, sel);
        }
        if (key == KEY_CLICK) {
            i = item_under_pointer();
            if (i != 0xff) result = menu_commands[m][i];
            break;
        }
        if ((key == KEY_RETURN || key == KEY_SPACE) && sel != 0xff) {
            result = menu_commands[m][sel];
            break;
        }
        if (key == KEY_ESC || key == KEY_RUNSTOP) break;
    }

    // Put back what was under it: the minimap and its frame (the pixels
    // were never touched) and, if it reached that far, the top of the map
    // window, whose store was not touched either.
    menu_draw();
    minimap_show();
    right = drop_left + drop_width;
    if (right > VIEW_LEFT) {
        byte left = drop_left > VIEW_LEFT ? drop_left : VIEW_LEFT;
        view_restore(left - VIEW_LEFT, DROP_TOP - VIEW_TOP, right - left,
                     n + 2);
    }
    return result;
}
