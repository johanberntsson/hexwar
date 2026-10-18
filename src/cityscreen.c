// The city screen.
//
// **It is the map screen's own cells, rewritten.** Everything but the patch
// in the middle is ordinary text written over the panel, the minimap and the
// map window alike, and the patch is the window's own full-colour cells
// showing a part of the tile store that view_draw_patch() has just drawn
// into. So nothing new is allocated; the price is that game.c has to put
// the whole map screen back on the way out, which it does in a frame.

#include "cityscreen.h"
#include "city.h"
#include "input.h"
#include "map.h"
#include "screen.h"
#include "unit.h"
#include "view.h"

#define TEXT_COLOUR COLOR_LIGHTGREEN
#define BORDER_COLOUR COLOR_LIGHTGREY
#define BUTTON_CHANGE (ATTR_REVERSE | COLOR_CYAN)
#define BUTTON_EXIT (ATTR_REVERSE | COLOR_RED)

// Three boxes across rows 2-13: the units, the city, what it builds.
#define BOX_TOP 2
#define BOX_BOTTOM 13
#define UNITS_LEFT 0
#define UNITS_RIGHT 25
#define PATCH_LEFT 28           // the patch itself; its frame is round it
#define PATCH_TOP 3
#define BUILD_LEFT 54
#define BUILD_RIGHT 79

#define TEXT_LEFT(BOX) ((BOX) + 2)
#define HEADING_ROW (BOX_TOP + 1)
#define LIST_ROW (BOX_TOP + 3)  // units, the item, the choices
#define LIST_ROWS (BOX_BOTTOM - LIST_ROW)

#define CHANGE_ROW (BOX_BOTTOM - 2)
#define CHANGE_TEXT " CHANGE "
#define EXIT_ROW (SCREEN_ROWS - 1)
#define EXIT_TEXT " EXIT "
#define EXIT_LEFT (SCREEN_COLUMNS - 2 - (sizeof EXIT_TEXT - 1))

#if PATCH_LEFT < VIEW_LEFT || PATCH_LEFT + VIEW_PATCH_COLUMNS > VIEW_LEFT + VIEW_COLUMNS \
    || PATCH_TOP < VIEW_TOP || PATCH_TOP + VIEW_PATCH_ROWS > VIEW_TOP + VIEW_ROWS
#error "the patch has to be inside the map window, whose cells are the store's"
#endif

// What a city can be told to build, in the order CHANGE lists it.
static const byte buildable[] = { KIND_WARRIOR, KIND_BUILDER };
#define BUILDABLE (sizeof buildable)

static char digits[6];

static const char *number(word v)
{
    byte i = sizeof digits - 1;
    digits[i] = 0;
    do { digits[--i] = (char)('0' + v % 10); v /= 10; } while (v);
    return digits + i;
}

// The width of a string, for laying out what follows it.
static byte length(const char *s)
{
    byte n = 0;
    while (s[n]) ++n;
    return n;
}

// Whether the tip of the arrow is on row `row`, columns x0 to x1 inclusive.
static byte pointer_on(byte row, byte x0, byte x1)
{
    byte col = (byte)(input_x >> 2);
    return (byte)(input_y >> 3) == row && col >= x0 && col <= x1;
}

static void blank_box(byte left, byte right)
{
    byte y;
    for (y = BOX_TOP + 1; y < BOX_BOTTOM; ++y)
        screen_fill(left + 1, y, right - left - 1, TEXT_COLOUR);
}

static void draw_units(byte c)
{
    byte u, y = LIST_ROW, more = 0;
    byte x = TEXT_LEFT(UNITS_LEFT);

    screen_putsxy(x, HEADING_ROW, "Units", COLOR_WHITE);
    for (u = 0; u < MAX_UNITS; ++u) {
        if (unit_kind[u] == KIND_NONE || unit_ax[u] != city_ax[c]
            || unit_ay[u] != city_ay[c])
            continue;
        if (y == LIST_ROW + LIST_ROWS - 1) {
            ++more;
            continue;
        }
        screen_putsxy(x, y, unit_kind_name[unit_kind[u]], TEXT_COLOUR);
        if (unit_orders[u] == ORDERS_FORTIFY)
            screen_putsxy(x + 8, y, "fortified", COLOR_LIGHTGREY);
        ++y;
    }
    if (y == LIST_ROW) screen_putsxy(x, y, "None", COLOR_LIGHTGREY);
    if (more) {
        const char *s = number(more);
        screen_putsxy(x, y, "and ", COLOR_LIGHTGREY);
        screen_putsxy(x + 4, y, s, COLOR_LIGHTGREY);
        screen_putsxy(x + 4 + length(s), y, " more", COLOR_LIGHTGREY);
    }
}

static void draw_build(byte c)
{
    byte x = TEXT_LEFT(BUILD_LEFT);
    byte kind = city_build[c];
    const char *s;

    blank_box(BUILD_LEFT, BUILD_RIGHT);
    screen_putsxy(x, HEADING_ROW, "Producing", COLOR_WHITE);
    screen_putsxy(x, LIST_ROW, unit_kind_name[kind], TEXT_COLOUR);
    s = number(city_shields[c]);
    screen_putsxy(x, LIST_ROW + 1, s, TEXT_COLOUR);
    x += length(s);
    screen_putsxy(x, LIST_ROW + 1, " of ", TEXT_COLOUR);
    s = number(unit_kind_cost[kind]);
    screen_putsxy(x + 4, LIST_ROW + 1, s, TEXT_COLOUR);
    screen_putsxy(x + 4 + length(s), LIST_ROW + 1, " shields", TEXT_COLOUR);

    // "3 a turn, 4 turns"
    x = TEXT_LEFT(BUILD_LEFT);
    s = number(city_production(c));
    screen_putsxy(x, LIST_ROW + 2, s, TEXT_COLOUR);
    x += length(s);
    screen_putsxy(x, LIST_ROW + 2, " a turn, ", TEXT_COLOUR);
    x += 9;
    s = number(city_turns_left(c));
    screen_putsxy(x, LIST_ROW + 2, s, TEXT_COLOUR);
    screen_putsxy(x + length(s), LIST_ROW + 2,
                  city_turns_left(c) == 1 ? " turn" : " turns", TEXT_COLOUR);
    screen_putsxy(TEXT_LEFT(BUILD_LEFT), CHANGE_ROW, CHANGE_TEXT,
                  BUTTON_CHANGE);
}

static void draw_choice(byte i, byte selected)
{
    byte x = TEXT_LEFT(BUILD_LEFT);
    byte kind = buildable[i];
    byte colour = (i == selected) ? (ATTR_REVERSE | COLOR_WHITE)
                                  : TEXT_COLOUR;

    screen_fill(x, LIST_ROW + i, BUILD_RIGHT - x - 1, colour);
    screen_putsxy(x + 1, LIST_ROW + i, unit_kind_name[kind], colour);
    screen_putsxy(x + 12, LIST_ROW + i, number(unit_kind_cost[kind]), colour);
}

static byte choice_under_pointer(void)
{
    byte i;
    for (i = 0; i < BUILDABLE; ++i)
        if (pointer_on(LIST_ROW + i, TEXT_LEFT(BUILD_LEFT), BUILD_RIGHT - 1))
            return i;
    return 0xff;
}

// CHANGE: the list of what can be built, in the production box, until one
// is picked -- by click, Return or space -- or Esc keeps the old one.
// Shields already gathered carry over to the new choice.
static void change(byte c)
{
    byte sel = 0, i, key, was;

    for (i = 0; i < BUILDABLE; ++i)
        if (buildable[i] == city_build[c]) sel = i;

    blank_box(BUILD_LEFT, BUILD_RIGHT);
    screen_putsxy(TEXT_LEFT(BUILD_LEFT), HEADING_ROW, "Build what?",
                  COLOR_WHITE);
    screen_putsxy(TEXT_LEFT(BUILD_LEFT) + 12, LIST_ROW - 1, "shields",
                  COLOR_LIGHTGREY);
    for (i = 0; i < BUILDABLE; ++i) draw_choice(i, sel);

    for (;;) {
        screen_wait_raster(SCREEN_ROW_LINE(SCREEN_ROWS - 1));
        key = input_poll();
        was = sel;
        if (input_moved()) {
            i = choice_under_pointer();
            if (i != 0xff) sel = i;
        }
        if (key == KEY_UP && sel > 0) --sel;
        else if (key == KEY_DOWN && sel < BUILDABLE - 1) ++sel;
        if (sel != was) {
            draw_choice(was, sel);
            draw_choice(sel, sel);
        }
        if (!key) continue;
        if (key == KEY_RETURN || key == KEY_SPACE) {
            break;
        } else if (key == KEY_CLICK) {
            i = choice_under_pointer();
            if (i != 0xff) {
                sel = i;
                break;
            }
        } else if (key == KEY_ESC || key == KEY_RUNSTOP) {
            sel = 0xff;
            break;
        }
    }
    if (sel != 0xff) city_build[c] = buildable[sel];
    draw_build(c);
}

void city_screen(byte c)
{
    const char *name = city_name_of(c);
    byte key;

    sprite_hide(2);             // the minimap's outline, which is gone
    sprite_hide(3);

    // The picture first, so that the patch's cells show it the moment they
    // are put back.
    view_draw_patch(PATCH_LEFT - VIEW_LEFT, PATCH_TOP - VIEW_TOP,
                    city_ax[c], city_ay[c]);
    screen_clear();
    view_restore(PATCH_LEFT - VIEW_LEFT, PATCH_TOP - VIEW_TOP,
                 VIEW_PATCH_COLUMNS, VIEW_PATCH_ROWS);

    screen_putsxy((SCREEN_COLUMNS - length(name)) / 2, 0, name, COLOR_WHITE);
    screen_box(UNITS_LEFT, BOX_TOP, UNITS_RIGHT, BOX_BOTTOM, BORDER_COLOUR);
    screen_box(PATCH_LEFT - 1, PATCH_TOP - 1, PATCH_LEFT + VIEW_PATCH_COLUMNS,
               PATCH_TOP + VIEW_PATCH_ROWS, BORDER_COLOUR);
    screen_box(BUILD_LEFT, BOX_TOP, BUILD_RIGHT, BOX_BOTTOM, BORDER_COLOUR);
    draw_units(c);
    draw_build(c);
    screen_putsxy(0, EXIT_ROW, "c change  esc exit", COLOR_LIGHTGREY);
    screen_putsxy(EXIT_LEFT, EXIT_ROW, EXIT_TEXT, BUTTON_EXIT);

    for (;;) {
        screen_wait_raster(SCREEN_ROW_LINE(SCREEN_ROWS - 1));
        key = input_poll();
        if (!key) continue;
        if (key >= 'A' && key <= 'Z') key += 'a' - 'A';
        if (key == KEY_CLICK) {
            if (pointer_on(EXIT_ROW, EXIT_LEFT,
                           EXIT_LEFT + sizeof EXIT_TEXT - 2))
                return;
            if (pointer_on(CHANGE_ROW, TEXT_LEFT(BUILD_LEFT),
                           TEXT_LEFT(BUILD_LEFT) + sizeof CHANGE_TEXT - 2))
                change(c);
        } else if (key == KEY_ESC || key == KEY_RUNSTOP
                   || key == KEY_RETURN || key == 'x') {
            return;
        } else if (key == 'c') {
            change(c);
        }
    }
}
