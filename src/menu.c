// In region 4 (src/region.h): called through the stubs in src/dispatch.s,
// under these names here. It was in region 0 until that filled up (22 Oct
// 2026); opening a menu may now copy region 4 into its slot, 3 ms at most.
#define menu_orders menu_orders_r4
#define menu_draw menu_draw_r4
#define menu_click menu_click_r4
#define menu_key menu_key_r4

#include "menu.h"
#include "game.h"
#include "input.h"
#include "map.h"
#include "minimap.h"
#include "screen.h"
#include "sheet.h"
#include "text.h"
#include "unit.h"
#include "view.h"

#pragma clang section text="r4text" rodata="r4rodata"

#define BAR_COLOUR COLOR_LIGHTGREEN
#define ITEM_COLOUR COLOR_WHITE
#define BORDER_COLOUR COLOR_LIGHTGREY
#define SELECTED(C) (ATTR_REVERSE | (C))

#define DROP_TOP 1              // the dropdown's frame; items start a row below

// The submenus, Game -> Options and Help -> Terrain Types, are boxes of
// their own over their menu's dropdown, which stays up, as in Civilization
// (civ1-reference/civ1-game-options.png): a heading, then the items. They
// are not on the bar; their indexes in the tables below are MENUS and up.
#define SUB_DX 4                // from their menu's title
#define SUB_TOP 3
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

// The submenus. Options' items are ticked when on. Terrain Types' are
// the names in res/text/list_terrains.txt, in alphabetical order, as
// Civilization's Civilopedia lists them: item_name() and item_command().
static const char *const options_items[] = { "End of Turn" };
static const byte options_commands[] = { MENU_END_OF_TURN };
static const byte terrain_order[] = {
    TILE_DESERT, TILE_FLOODPLAIN, TILE_FOREST, TILE_GRASS, TILE_HILLS,
    TILE_MOUNTAIN, TILE_OCEAN, TILE_PLAINS, TILE_SHALLOW_OCEAN, TILE_TUNDRA
};
// Military Units' are the units' names, in alphabetical order too.
static const byte unit_order[] = {
    KIND_BUILDER, KIND_BOAT, KIND_GALLEY, KIND_MERCHANT, KIND_RAIDER,
    KIND_SETTLER, KIND_SPEARMAN, KIND_WARRIOR
};
#define UNIT_PAGES (sizeof unit_order)
#if MENU_UNIT != MENU_TERRAIN + TERRAIN_COUNT
#error "MENU_UNIT in menu.h has to follow the terrains' commands"
#endif
static const char *const sub_heading[] = {
    "Options:", "Terrain Types:", "Military Units:"
};

// Orders depends on the unit that has them, and menu_orders() fills it in.
static const char *orders_items[2];
static byte orders_commands[2];

#ifdef DEBUG_MENU
// Show Map lifts the fog of war, and again puts it back. Play 50 Turns
// hands the human's side to the computer for that long.
static const char *const debug_items[] = { "Show Map", "Play 50 Turns" };
static const byte debug_commands[] = { MENU_SHOW_MAP, MENU_PLAY_50 };
#endif

static const char *const advisors_items[] = { "Diplomacy", "Finance" };
static const byte advisors_commands[] = { MENU_DIPLOMACY, MENU_FINANCE };

static const char *const world_items[] = { "Top Five Cities", "Score" };
static const byte world_commands[] = { MENU_TOP_CITIES, MENU_SCORE };

static const char *const help_items[] = {
    "Keyboard Control", "Terrain Types", "Military Units"
};
static const byte help_commands[] = { MENU_KEYS, MENU_TERRAINS, MENU_UNITS };

#ifdef DEBUG_MENU
static const char *const *const menu_items[MENUS + 3] = {
    game_items, orders_items, advisors_items, world_items, debug_items,
    help_items, options_items, 0, 0
};
static const byte *const menu_commands[MENUS + 3] = {
    game_commands, orders_commands, advisors_commands, world_commands,
    debug_commands, help_commands, options_commands, 0, 0
};
static byte menu_count[MENUS + 3] = {
    3, 0, 2, 2, 2, 3, 1, TERRAIN_COUNT, UNIT_PAGES
};
#else
static const char *const *const menu_items[MENUS + 3] = {
    game_items, orders_items, advisors_items, world_items, help_items,
    options_items, 0, 0
};
static const byte *const menu_commands[MENUS + 3] = {
    game_commands, orders_commands, advisors_commands, world_commands,
    help_commands, options_commands, 0, 0
};
static byte menu_count[MENUS + 3] = {
    3, 0, 2, 2, 3, 1, TERRAIN_COUNT, UNIT_PAGES
};
#endif

#define ORDERS 1                // Orders' index in the tables above
#define DEBUG 4                 // and Debug's, without DEBUG_MENU none
#define HELP (MENUS - 1)
#define OPTIONS MENUS           // the submenus
#define TERRAINS (MENUS + 1)
#define UNITS (MENUS + 2)

// The menu each submenu opens from.
static const byte sub_parent[] = { 0, HELP, HELP };

static const char *item_name(byte m, byte i)
{
    if (m == TERRAINS) return text_line(TEXT_LIST_TERRAINS, terrain_order[i]);
    if (m == UNITS) return unit_kind_name[unit_order[i]];
    return menu_items[m][i];
}

static byte item_command(byte m, byte i)
{
    if (m == TERRAINS) return MENU_TERRAIN + terrain_order[i];
    if (m == UNITS) return MENU_UNIT + unit_order[i];
    return menu_commands[m][i];
}

static const char *order_name(byte command)
{
    switch (command) {
    case MENU_FOUND_CITY: return "Found City";
    case MENU_IRRIGATE: return "Irrigate";
    case MENU_MINE: return "Mine";
    case MENU_FISH: return "Fish";
    case MENU_EMPORIUM: return "Found Emporium";
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

// The open dropdown: its frame's top row and the first item's. With a
// submenu open, its menu's dropdown is still up as well, at parent_*.
static byte drop_menu, drop_left, drop_width, drop_top, drop_first;
static byte parent_menu, parent_left, parent_width, parent_rows;

static void draw_item(byte i, byte selected)
{
    byte colour = (i == selected) ? SELECTED(ITEM_COLOUR) : ITEM_COLOUR;
    byte y = drop_first + i;
    screen_fill(drop_left + 1, y, drop_width - 2, colour);
    screen_putsxy(drop_left + 2, y, item_name(drop_menu, i), colour);
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

// Put back the part of the map window, if any, that a box of WIDTH
// columns and ROWS rows was over. The store was not touched.
static void restore(byte left, byte top, byte width, byte rows)
{
    byte right = left + width;

    if (right > VIEW_LEFT) {
        if (left < VIEW_LEFT) left = VIEW_LEFT;
        view_restore(left - VIEW_LEFT, top - VIEW_TOP, right - left, rows);
    }
}

// Put back what the open dropdown was over, and a submenu's menu's too:
// the minimap and its frame (the pixels were never touched) and the map
// window.
static void close_drop(void)
{
    menu_draw();
    minimap_show();
    restore(drop_left, drop_top, drop_width,
            drop_first - drop_top + menu_count[drop_menu] + 1);
    if (drop_menu >= MENUS)
        restore(parent_left, DROP_TOP, parent_width, parent_rows);
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
// or MEGA and another title's letter, move to that menu instead. A
// submenu opens over its menu's dropdown and leaves it showing, and its
// close_drop() puts back what both were over.
static byte run(byte m, byte sel)
{
    byte i, n, key, was, next;

    for (;;) {
        n = menu_count[m];
        drop_menu = m;
        if (m >= MENUS) {
            parent_menu = sub_parent[m - MENUS];
            drop_left = menu_x[parent_menu] + SUB_DX;
            drop_top = SUB_TOP;
            drop_first = SUB_TOP + 2;
            drop_width = length(sub_heading[m - MENUS]);
        } else {
            drop_left = menu_x[m];
            drop_top = DROP_TOP;
            drop_first = DROP_TOP + 1;
            drop_width = length(menu_title[m]);
        }
        for (i = 0; i < n; ++i)
            if (length(item_name(m, i)) > drop_width)
                drop_width = length(item_name(m, i));
        drop_width += 4;

        sprite_hide(2);         // the minimap's outline would show through
        sprite_hide(3);
        if (m < MENUS)
            screen_putsxy(menu_x[m], 0, menu_title[m], SELECTED(BAR_COLOUR));
        screen_box(drop_left, drop_top, drop_left + drop_width - 1,
                   drop_first + n, BORDER_COLOUR);
        if (m >= MENUS) {
            screen_fill(drop_left + 1, drop_top + 1, drop_width - 2,
                        ITEM_COLOUR);
            screen_putsxy(drop_left + 1, drop_top + 1, sub_heading[m - MENUS],
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
                key = item_command(m, i);
                if (key != MENU_OPTIONS && key != MENU_TERRAINS
                    && key != MENU_UNITS) {
                    close_drop();
                    return key;
                }
                next = key == MENU_OPTIONS ? OPTIONS
                       : key == MENU_TERRAINS ? TERRAINS : UNITS;
                parent_left = drop_left;
                parent_width = drop_width;
                parent_rows = n + 2;
                continue;
            }
            if (key == KEY_ESC || key == KEY_RUNSTOP) {
                close_drop();
                return MENU_NONE;
            }
            // From a submenu, on from its menu.
            was = m < MENUS ? m : parent_menu;
            if (key == KEY_RIGHT) next = neighbour(was, 1);
            if (key == KEY_LEFT) next = neighbour(was, MENUS - 1);
            if (KEY_IS_MEGA(key)) {
                for (i = 0; i < MENUS; ++i)
                    if (key == menu_hotkey(i) && menu_count[i])
                        next = i;
            }
        }
        if (next < MENUS) close_drop();
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
