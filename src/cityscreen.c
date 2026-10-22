// The city screen.
//
// **It is the map screen's own cells, rewritten.** Everything but the patch
// in the middle is ordinary text written over the panel, the minimap and the
// map window alike, and the patch is the window's own full-colour cells
// showing a part of the tile store that view_draw_patch() has just drawn
// into. So nothing new is allocated; the price is that game.c has to put
// the whole map screen back on the way out, which it does in a frame.

// In region 2 (src/region.h): called through the stubs in src/dispatch.s,
// under these names here.
#define city_screen city_screen_r2

#include "cityscreen.h"
#include "city.h"
#include "game.h"
#include "growth.h"
#include "input.h"
#include "map.h"
#include "screen.h"
#include "unit.h"
#include "view.h"

#pragma clang section text="r2text" rodata="r2rodata"

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

// The city's own figures, under the patch.
#define INFO_TOP (BOX_BOTTOM + 1)
#define INFO_BOTTOM (INFO_TOP + 6)

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

// The patch's seven hexagons as view_draw_patch() draws them: which way each
// is from the city, and the character cell of its middle on the screen.
static const byte patch_dir[7] = {
    DIR_NW, DIR_NE, DIR_W, DIR_NONE, DIR_E, DIR_SW, DIR_SE
};
static const byte patch_cx[7] = { 8, 16, 4, 12, 20, 8, 16 };
static const byte patch_cy[7] = { 2, 2, 5, 5, 5, 8, 8 };

// What a city can be told to build, in the order CHANGE lists it. A
// settler from a city of size 1 waits at its cost until the city grows.
// Then the spearman, only with bronze (player_bronze()), and the galley,
// only with cedar in a coastal city; without them CHANGE says what is
// missing. The fishing boat and the merchant ship, in a coastal city.
// change() fills it.
static byte buildable[KINDS];
static byte buildable_n;
#define BUILDABLE buildable_n

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

// The units in the list, by row: a click on one, or its number key, wakes
// it if it has orders and fortifies it if it is a warrior without. This is
// the way to a unit fortified in a city, which the map does not draw.
#define UNITS_LISTED (LIST_ROWS - 1)
static byte listed[UNITS_LISTED];
static byte listed_n;

static void draw_units(byte c)
{
    byte u, y = LIST_ROW, more = 0;
    byte x = TEXT_LEFT(UNITS_LEFT);

    blank_box(UNITS_LEFT, UNITS_RIGHT);
    screen_putsxy(x, HEADING_ROW, "Units", COLOR_WHITE);
    listed_n = 0;
    for (u = 0; u < MAX_UNITS; ++u) {
        if (unit_kind[u] == KIND_NONE || unit_ax[u] != city_ax[c]
            || unit_ay[u] != city_ay[c])
            continue;
        if (listed_n == UNITS_LISTED) {
            ++more;
            continue;
        }
        listed[listed_n++] = u;
        screen_putcxy(x, y, (char)('0' + listed_n), COLOR_YELLOW);
        screen_putsxy(x + 2, y, unit_kind_name[unit_kind[u]], TEXT_COLOUR);
        if (unit_orders[u] == ORDERS_FORTIFY)
            screen_putsxy(x + 11, y, "fortified", COLOR_LIGHTGREY);
        else if (unit_orders[u] != ORDERS_NONE)
            screen_putsxy(x + 11, y, "working", COLOR_LIGHTGREY);
        ++y;
    }
    if (listed_n)
        screen_putsxy(x, LIST_ROW - 1, "number: wake/fortify",
                      COLOR_LIGHTGREY);
    if (y == LIST_ROW) screen_putsxy(x, y, "None", COLOR_LIGHTGREY);
    if (more) {
        const char *s = number(more);
        screen_putsxy(x, y, "and ", COLOR_LIGHTGREY);
        screen_putsxy(x + 4, y, s, COLOR_LIGHTGREY);
        screen_putsxy(x + 4 + length(s), y, " more", COLOR_LIGHTGREY);
    }
}

// Unit `i` of the list woken, or fortified; see listed[].
static void wake(byte c, byte i)
{
    byte u;

    if (i >= listed_n) return;
    u = listed[i];
    if (unit_orders[u] != ORDERS_NONE) unit_orders[u] = ORDERS_NONE;
    else if (unit_kind[u] == KIND_WARRIOR || unit_kind[u] == KIND_SPEARMAN)
        unit_orders[u] = ORDERS_FORTIFY;
    else return;
    draw_units(c);
}

static byte unit_under_pointer(void)
{
    byte i;
    for (i = 0; i < listed_n; ++i)
        if (pointer_on(LIST_ROW + i, TEXT_LEFT(UNITS_LEFT), UNITS_RIGHT - 1))
            return i;
    return 0xff;
}

// Write `s` at (*x, y) and move *x past it.
static void put(byte *x, byte y, const char *s, byte colour)
{
    screen_putsxy(*x, y, s, colour);
    *x += length(s);
}

// Size, food, shields and trade, from city_work()'s totals.
static void draw_info(byte c)
{
    byte size = city_size[c];
    byte box = GROWTH_BOX(size);
    byte eaten = 2 * size;
    byte x0 = PATCH_LEFT + 1;
    byte x, y, more;

    for (y = INFO_TOP + 1; y < INFO_BOTTOM; ++y)
        screen_fill(PATCH_LEFT, y, VIEW_PATCH_COLUMNS, TEXT_COLOUR);
    y = INFO_TOP + 1;

    x = x0;
    put(&x, y, "Size ", COLOR_WHITE);
    put(&x, y, number(size), TEXT_COLOUR);
    if (work_idle) {
        x = x0 + 13;
        put(&x, y, "Idle ", COLOR_WHITE);
        put(&x, y, number(work_idle), COLOR_YELLOW);
    }

    x = x0;
    put(&x, ++y, "Food ", COLOR_WHITE);
    put(&x, y, number(city_food[c]), TEXT_COLOUR);
    put(&x, y, " of ", TEXT_COLOUR);
    put(&x, y, number(box), TEXT_COLOUR);
    if (work_food >= eaten) {
        more = work_food - eaten;
        put(&x, y, ", +", TEXT_COLOUR);
    } else {
        more = eaten - work_food;
        put(&x, y, ", -", TEXT_COLOUR);
    }
    put(&x, y, number(more), TEXT_COLOUR);

    x = x0 + 5;
    ++y;
    if (work_food > eaten) {
        more = work_food - eaten;
        put(&x, y, "grows in ", TEXT_COLOUR);
        put(&x, y, number((box - city_food[c] + more - 1) / more),
            TEXT_COLOUR);
        put(&x, y, " turns", TEXT_COLOUR);
    } else {
        put(&x, y, work_food == eaten ? "not growing" : "starving",
            work_food == eaten ? TEXT_COLOUR : COLOR_YELLOW);
    }

    x = x0;
    put(&x, ++y, "Shields ", COLOR_WHITE);
    put(&x, y, number(work_shields), TEXT_COLOUR);
    x = x0 + 13;
    put(&x, y, "Trade ", COLOR_WHITE);
    put(&x, y, number(work_trade), TEXT_COLOUR);
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
    // It takes a citizen with it (city_produce()).
    if (kind == KIND_SETTLER && city_size[c] < 2)
        screen_putsxy(TEXT_LEFT(BUILD_LEFT), LIST_ROW + 3,
                      "waits for size 2", COLOR_YELLOW);
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
    screen_putsxy(x + 14, LIST_ROW + i, number(unit_kind_cost[kind]), colour);
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
    byte copper = player_has(PLAYER_HUMAN, RES_COPPER);
    byte tin = player_has(PLAYER_HUMAN, RES_TIN);
    byte cedar = player_has(PLAYER_HUMAN, RES_CEDAR);
    byte coast = city_coastal(c), row;

    buildable[0] = KIND_WARRIOR;
    buildable[1] = KIND_SETTLER;
    buildable[2] = KIND_BUILDER;
    buildable_n = 3;
    if (copper && tin) buildable[buildable_n++] = KIND_SPEARMAN;
    if (cedar && coast) buildable[buildable_n++] = KIND_GALLEY;
    if (coast) {
        buildable[buildable_n++] = KIND_BOAT;
        buildable[buildable_n++] = KIND_MERCHANT;
    }

    for (i = 0; i < BUILDABLE; ++i)
        if (buildable[i] == city_build[c]) sel = i;

    blank_box(BUILD_LEFT, BUILD_RIGHT);
    screen_putsxy(TEXT_LEFT(BUILD_LEFT), HEADING_ROW, "Build what?",
                  COLOR_WHITE);
    screen_putsxy(TEXT_LEFT(BUILD_LEFT) + 14, LIST_ROW - 1, "shields",
                  COLOR_LIGHTGREY);
    for (i = 0; i < BUILDABLE; ++i) draw_choice(i, sel);
    // Straight under the list: with the merchant ship there is no room
    // for a blank line when the bronze note has two.
    row = LIST_ROW + BUILDABLE;
    if (!(copper && tin)) {
        screen_putsxy(TEXT_LEFT(BUILD_LEFT), row++,
                      "Spearman needs bronze:", COLOR_LIGHTGREY);
        screen_putsxy(TEXT_LEFT(BUILD_LEFT), row++,
                      copper ? "tin" : tin ? "copper" : "copper and tin",
                      COLOR_LIGHTGREY);
    }
    if (coast && !cedar)
        screen_putsxy(TEXT_LEFT(BUILD_LEFT), row, "Galley needs cedar",
                      COLOR_LIGHTGREY);

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

// The neighbour under the arrow, as a DIR_*, or DIR_NONE: the nearest
// middle of a hexagon, if it is within a hexagon's reach. In sprite units,
// which are square on the glass: four a character across, eight down.
static byte dir_under_pointer(void)
{
    byte i, best = DIR_NONE;
    int dx, dy;
    word d, best_d = 18 * 18;

    for (i = 0; i < 7; ++i) {
        dx = (int)input_x - 4 * (int)(PATCH_LEFT + patch_cx[i]);
        dy = (int)input_y - 8 * (int)(PATCH_TOP + patch_cy[i]) - 4;
        if (dx > 40 || dx < -40 || dy > 40 || dy < -40) continue;
        d = (word)(dx * dx + dy * dy);
        if (d >= best_d) continue;
        best_d = d;
        best = patch_dir[i];
    }
    return best;
}

// Neighbour `dir` worked or not, by the player; everything that depends on
// it drawn again.
static void toggle(byte c, byte dir)
{
    if (dir == DIR_NONE || !city_toggle(c, dir)) return;
    city_work(c);
    view_draw_patch(PATCH_LEFT - VIEW_LEFT, PATCH_TOP - VIEW_TOP,
                    city_ax[c], city_ay[c], work_mask);
    draw_info(c);
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
    city_work(c);
    view_draw_patch(PATCH_LEFT - VIEW_LEFT, PATCH_TOP - VIEW_TOP,
                    city_ax[c], city_ay[c], work_mask);
    screen_clear();
    view_restore(PATCH_LEFT - VIEW_LEFT, PATCH_TOP - VIEW_TOP,
                 VIEW_PATCH_COLUMNS, VIEW_PATCH_ROWS);

    screen_putsxy((SCREEN_COLUMNS - length(name)) / 2, 0, name, COLOR_WHITE);
    screen_box(UNITS_LEFT, BOX_TOP, UNITS_RIGHT, BOX_BOTTOM, BORDER_COLOUR);
    screen_box(PATCH_LEFT - 1, PATCH_TOP - 1, PATCH_LEFT + VIEW_PATCH_COLUMNS,
               PATCH_TOP + VIEW_PATCH_ROWS, BORDER_COLOUR);
    screen_box(BUILD_LEFT, BOX_TOP, BUILD_RIGHT, BOX_BOTTOM, BORDER_COLOUR);
    screen_box(PATCH_LEFT - 1, INFO_TOP, PATCH_LEFT + VIEW_PATCH_COLUMNS,
               INFO_BOTTOM, BORDER_COLOUR);
    draw_info(c);
    draw_units(c);
    draw_build(c);
    screen_putsxy(0, EXIT_ROW,
                  "c change  click a hexagon or its moving key: work it  "
                  "esc exit", COLOR_LIGHTGREY);
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
            else if (unit_under_pointer() != 0xff)
                wake(c, unit_under_pointer());
            else
                toggle(c, dir_under_pointer());
        } else if (key == KEY_ESC || key == KEY_RUNSTOP
                   || key == KEY_RETURN || key == 'x') {
            return;
        } else if (key == 'c') {
            change(c);
        } else if (key >= '1' && key < '1' + UNITS_LISTED) {
            wake(c, key - '1');
        } else {
            toggle(c, key_dir(key));
        }
    }
}
