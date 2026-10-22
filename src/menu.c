// In region 0 (src/region.h): called through the stubs in src/dispatch.s,
// under these names here.
#define menu_orders menu_orders_r0
#define menu_draw menu_draw_r0
#define menu_click menu_click_r0
#define menu_key menu_key_r0

#include "menu.h"
#include "game.h"
#include "input.h"
#include "map.h"
#include "minimap.h"
#include "screen.h"
#include "view.h"

#pragma clang section text="r0text" rodata="r0rodata"

#define BAR_COLOUR COLOR_LIGHTGREEN
#define ITEM_COLOUR COLOR_WHITE
#define BORDER_COLOUR COLOR_LIGHTGREY
#define SELECTED(C) (ATTR_REVERSE | (C))

#define DROP_TOP 1              // the dropdown's frame; items start a row below

// Game -> Options is a box of its own over the Game dropdown, as in
// Civilization (civ1-reference/civ1-game-options.png): a heading, then the
// options, each with a tick when it is on.
#define OPTIONS_LEFT 4
#define OPTIONS_TOP 3
#define TICK 0x7a               // a tick in the lower case character set

#ifdef DEBUG_MENU
#define MENUS 6
static const char *const menu_title[MENUS] = {
    "Game", "Orders", "Advisors", "World", "Debug", "Help",
};
static const byte menu_x[MENUS] = { 0, 5, 12, 21, 27, 33 };
#else
#define MENUS 5
static const char *const menu_title[MENUS] = {
    "Game", "Orders", "Advisors", "World", "Help",
};
static const byte menu_x[MENUS] = { 0, 5, 12, 21, 27 };
#endif

// Each menu's items and what they ask for. An empty menu does not open.
static const char *const game_items[] = { "Save Game", "Options", "Retire" };
static const byte game_commands[] = { MENU_SAVE, MENU_OPTIONS, MENU_RETIRE };

// Game -> Options, which is not on the bar: its index is MENUS.
static const char *const options_items[] = { "End of Turn" };
static const byte options_commands[] = { MENU_END_OF_TURN };

// Orders depends on the unit that has them, and menu_orders() fills it in.
static const char *orders_items[2];
static byte orders_commands[2];

#ifdef DEBUG_MENU
// Show Map lifts the fog of war, and again puts it back. Play 50 Turns
// hands the human's side to the computer for that long.
static const char *const debug_items[] = { "Show Map", "Play 50 Turns" };
static const byte debug_commands[] = { MENU_SHOW_MAP, MENU_PLAY_50 };
#endif

static const char *const advisors_items[] = { "Diplomacy" };
static const byte advisors_commands[] = { MENU_DIPLOMACY };

static const char *const world_items[] = { "Top Five Cities" };
static const byte world_commands[] = { MENU_TOP_CITIES };

static const char *const help_items[] = { "Keyboard Control" };
static const byte help_commands[] = { MENU_KEYS };

#ifdef DEBUG_MENU
static const char *const *const menu_items[MENUS + 1] = {
    game_items, orders_items, advisors_items, world_items, debug_items,
    help_items, options_items
};
static const byte *const menu_commands[MENUS + 1] = {
    game_commands, orders_commands, advisors_commands, world_commands,
    debug_commands, help_commands, options_commands
};
static byte menu_count[MENUS + 1] = { 3, 0, 1, 1, 2, 1, 1 };
#else
static const char *const *const menu_items[MENUS + 1] = {
    game_items, orders_items, advisors_items, world_items, help_items,
    options_items
};
static const byte *const menu_commands[MENUS + 1] = {
    game_commands, orders_commands, advisors_commands, world_commands,
    help_commands, options_commands
};
static byte menu_count[MENUS + 1] = { 3, 0, 1, 1, 1, 1 };
#endif

#define ORDERS 1                // Orders' index in the tables above
#define OPTIONS MENUS           // and Options'
#define DEBUG 4                 // and Debug's, without DEBUG_MENU none

static const char *order_name(byte command)
{
    switch (command) {
    case MENU_FOUND_CITY: return "Found City";
    case MENU_IRRIGATE: return "Irrigate";
    case MENU_MINE: return "Mine";
    }
    return "Fortify";
}

void menu_orders(byte command, byte second)
{
    byte n = 0;
    if (command != MENU_NONE) {
        orders_commands[n] = command;
        orders_items[n++] = order_name(command);
    }
    if (second != MENU_NONE) {
        orders_commands[n] = second;
        orders_items[n++] = order_name(second);
    }
    menu_count[ORDERS] = n;
}

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

// The open dropdown: its frame's top row and the first item's.
static byte drop_menu, drop_left, drop_width, drop_top, drop_first;

static void draw_item(byte i, byte selected)
{
    byte colour = (i == selected) ? SELECTED(ITEM_COLOUR) : ITEM_COLOUR;
    byte y = drop_first + i;
    screen_fill(drop_left + 1, y, drop_width - 2, colour);
    screen_putsxy(drop_left + 2, y, menu_items[drop_menu][i], colour);
    // The toggles are ticked when on: End of Turn, the only option, when
    // the game waits, and Debug's Show Map.
    if ((drop_menu == OPTIONS && game_end_wait)
#ifdef DEBUG_MENU
        || (drop_menu == DEBUG && i == 0 && map_show)
#endif
        )
        screen_putcodexy(drop_left + 1, y, TICK, colour);
}

static byte item_under_pointer(void)
{
    byte col = (byte)(input_x >> 2);
    byte row = (byte)(input_y >> 3);
    if (col <= drop_left || col >= drop_left + drop_width - 1) return 0xff;
    if (row < drop_first || row >= drop_first + menu_count[drop_menu])
        return 0xff;
    return row - drop_first;
}

// Put back what the open dropdown was over: the minimap and its frame (the
// pixels were never touched) and, if it reached that far, the top of the map
// window, whose store was not touched either.
static void close_drop(void)
{
    byte right = drop_left + drop_width;

    menu_draw();
    minimap_show();
    if (right > VIEW_LEFT) {
        byte left = drop_left > VIEW_LEFT ? drop_left : VIEW_LEFT;
        view_restore(left - VIEW_LEFT, drop_top - VIEW_TOP, right - left,
                     drop_first - drop_top + menu_count[drop_menu] + 1);
    }
}

// The KEY_MEGA code that opens menu M.
static byte menu_hotkey(byte m)
{
    byte k = KEY_MEGA(menu_title[m][0] | 0x20);
    return k;
}

// The next menu with items in it, STEP 1 to the right or MENUS - 1 to the
// left, round the end of the bar. M has items, so this stops.
static byte neighbour(byte m, byte step)
{
    do m = (m + step) % MENUS; while (!menu_count[m]);
    return m;
}

// Open menu M, which has items, with item SEL selected (0xff for none), and
// run it until something is picked or it is closed. Cursor left and right,
// or MEGA and another title's letter, move to that menu instead. Options,
// picked from Game, opens over Game's dropdown and leaves it showing; its
// own close_drop() puts back what both were over, since Game's lies within
// the minimap's frame.
static byte run(byte m, byte sel)
{
    byte i, n, key, was, next;

    for (;;) {
        n = menu_count[m];
        drop_menu = m;
        if (m == OPTIONS) {
            drop_left = OPTIONS_LEFT;
            drop_top = OPTIONS_TOP;
            drop_first = OPTIONS_TOP + 2;
            drop_width = 0;
        } else {
            drop_left = menu_x[m];
            drop_top = DROP_TOP;
            drop_first = DROP_TOP + 1;
            drop_width = length(menu_title[m]);
        }
        for (i = 0; i < n; ++i)
            if (length(menu_items[m][i]) > drop_width)
                drop_width = length(menu_items[m][i]);
        drop_width += 4;

        sprite_hide(2);         // the minimap's outline would show through
        sprite_hide(3);
        if (m != OPTIONS)
            screen_putsxy(menu_x[m], 0, menu_title[m], SELECTED(BAR_COLOUR));
        screen_box(drop_left, drop_top, drop_left + drop_width - 1,
                   drop_first + n, BORDER_COLOUR);
        if (m == OPTIONS) {
            screen_fill(drop_left + 1, drop_top + 1, drop_width - 2,
                        ITEM_COLOUR);
            screen_putsxy(drop_left + 1, drop_top + 1, "Options:",
                          ITEM_COLOUR);
        }
        for (i = 0; i < n; ++i) draw_item(i, sel);
        input_moved();          // only a move from here on selects

        next = m;
        while (next == m) {
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
            i = 0xff;
            if (key == KEY_CLICK) {
                i = item_under_pointer();
                if (i == 0xff) {
                    close_drop();
                    return MENU_NONE;
                }
            }
            if (key == KEY_RETURN || key == KEY_SPACE) i = sel;
            if (i != 0xff) {
                key = menu_commands[m][i];
                if (key != MENU_OPTIONS) {
                    close_drop();
                    return key;
                }
                next = OPTIONS;
                continue;
            }
            if (key == KEY_ESC || key == KEY_RUNSTOP) {
                close_drop();
                return MENU_NONE;
            }
            // From Options, on from Game.
            if (key == KEY_RIGHT) next = neighbour(m % MENUS, 1);
            if (key == KEY_LEFT) next = neighbour(m % MENUS, MENUS - 1);
            if (KEY_IS_MEGA(key)) {
                for (i = 0; i < MENUS; ++i)
                    if (key == menu_hotkey(i) && menu_count[i])
                        next = i;
            }
        }
        if (next != OPTIONS) close_drop();
        m = next;
        sel = 0;
    }
}

byte menu_click(void)
{
    byte col = (byte)(input_x >> 2);
    byte m;

    for (m = 0; m < MENUS; ++m)
        if (col >= menu_x[m] && col < menu_x[m] + length(menu_title[m]))
            break;
    if (m == MENUS || menu_count[m] == 0) return MENU_NONE;
    return run(m, 0xff);
}

byte menu_key(byte key)
{
    byte m;

    for (m = 0; m < MENUS; ++m)
        if (key == menu_hotkey(m) && menu_count[m])
            return run(m, 0);
    return MENU_NONE;
}
