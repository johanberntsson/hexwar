// The city screen.
//
// **It is the map screen's own cells, rewritten.** Everything but the patch
// in the middle is ordinary text written over the panel, the minimap and the
// map window alike, and the patch is the window's own full-colour cells
// showing a part of the tile store that draw_patch() has just drawn into.
// So nothing new is allocated; the price is that game.c has to put the
// whole map screen back on the way out, which it does in a frame.

// In region 7 (src/region.h): called through the stubs in src/dispatch.s,
// under these names here. Its own region since the city's reach became
// two rings (23 Oct 2026), with the patch's drawing from view.c.
#define city_screen city_screen_r7

#include "cityscreen.h"
#include "city.h"
#include "culture.h"
#include "game.h"
#include "growth.h"
#include "input.h"
#include "map.h"
#include "screen.h"
#include "sheet.h"
#include "treasury.h"
#include "unit.h"
#include "view.h"

#pragma clang section text="r7text" rodata="r7rodata"

#define TEXT_COLOUR COLOR_LIGHTGREEN
#define BORDER_COLOUR COLOR_LIGHTGREY
#define BUTTON_CHANGE (ATTR_REVERSE | COLOR_CYAN)
#define BUTTON_EXIT (ATTR_REVERSE | COLOR_RED)
#define BUTTON_OFF (ATTR_REVERSE | COLOR_GREY)  // BUY, with too little gold

// The city's 19 hexagons in the middle, framed, with its figures under
// them; the units in a column on the left and what it builds on the right,
// rows 1-23.
#define BOX_TOP 1
#define BOX_BOTTOM 23
#define UNITS_LEFT 0
#define UNITS_RIGHT 18
#define BUILD_LEFT 61
#define BUILD_RIGHT 79

// **The patch**: the hexagons two steps round the city, five tiles across
// and five hexagon rows down, and a tile's foot.
#define PATCH_COLUMNS 40
#define PATCH_ROWS 16
#define PATCH_LEFT 20           // the patch itself; its frame is round it
#define PATCH_TOP 2
#define PATCHES 19

// The city's own figures, under the patch.
#define INFO_TOP (PATCH_TOP + PATCH_ROWS + 1)
#define INFO_BOTTOM BOX_BOTTOM

#define TEXT_LEFT(BOX) ((BOX) + 2)
#define HEADING_ROW (BOX_TOP + 1)
#define LIST_ROW (BOX_TOP + 3)  // units, the item
#define CHOICE_ROW (BOX_TOP + 2) // CHANGE's list

#define CHANGE_ROW (LIST_ROW + 8)
#define BUY_ROW (CHANGE_ROW + 2)
#define CHANGE_TEXT " CHANGE "
#define EXIT_ROW (SCREEN_ROWS - 1)
#define EXIT_TEXT " EXIT "
#define EXIT_LEFT (SCREEN_COLUMNS - 2 - (sizeof EXIT_TEXT - 1))

#if PATCH_LEFT < VIEW_LEFT || PATCH_LEFT + PATCH_COLUMNS > VIEW_LEFT + VIEW_COLUMNS \
    || PATCH_TOP < VIEW_TOP || PATCH_TOP + PATCH_ROWS > VIEW_TOP + VIEW_ROWS
#error "the patch has to be inside the map window, whose cells are the store's"
#endif

// The patch's hexagons in painter's order, top row first: which of the
// city's RING each is (map.h; PATCH_CITY for the city), and where its tile
// goes in the patch -- the hexagon rows are 3 characters apart and the
// tiles in one 8, so (dhx, dhy) in doubled coordinates is at
// (4 * (dhx + 4), 3 * (dhy + 2)).
#define PATCH_CITY 0xff
#define PATCH_MIDDLE 9          // the city's, where the keys start
static const byte patch_ring[PATCHES] = {
    14, 15, 16,
    13, DIR_NW, DIR_NE, 17,
    12, DIR_W, PATCH_CITY, DIR_E, 6,
    11, DIR_SW, DIR_SE, 7,
    10, 9, 8
};
static const byte patch_x[PATCHES] = {
    8, 16, 24,
    4, 12, 20, 28,
    0, 8, 16, 24, 32,
    4, 12, 20, 28,
    8, 16, 24
};
static const byte patch_y[PATCHES] = {
    0, 0, 0,
    3, 3, 3, 3,
    6, 6, 6, 6, 6,
    9, 9, 9, 9,
    12, 12, 12
};

// One step each way, DIR_* order, as patch_x/patch_y count: the keys move
// the arrow by these. 16 bit, at file scope (CLAUDE.md's Calypsi notes).
static const int step_x[6] = { 8, 4, -4, -8, -4, 4 };
static const int step_y[6] = { 0, 3, 3, 0, -3, -3 };

// What a city can be told to build, in the order CHANGE lists it. A
// settler from a city of size 1 waits at its cost until the city grows.
// Then the spearman, only with bronze (player_bronze()), the chariot, only
// with horses, and the galley, only with cedar in a coastal city; without
// them CHANGE says what is missing. The fishing boat and the merchant
// ship, in a coastal city. change() fills it.
static byte buildable[BUILD_KINDS];
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

// Write `s` at (*x, y) and move *x past it.
static void put(byte *x, byte y, const char *s, byte colour)
{
    screen_putsxy(*x, y, s, colour);
    *x += length(s);
}

// ------------------------------------------------------------- the patch

// **The city's picture**: its hexagon and its RING on black, with the
// cities and units on them, into the PATCH_COLUMNS x PATCH_ROWS of the
// store under the patch's cells. The rest of the store is left alone. It
// is no longer the map afterwards, so game.c redraws the view before the
// window shows the map again -- which needs no clear, since view_redraw()
// covers every byte. **A worked hexagon has the select ring, and one that
// is another city's that city's owner's flag** (work_on[], work_owner[]:
// city_work() first).
static void draw_patch(byte c)
{
    byte i, k, n, t, r, nx, ny, x, y;
    byte tiles[2];
    byte col = PATCH_LEFT - VIEW_LEFT, row = PATCH_TOP - VIEW_TOP;

    for (i = 0; i < PATCH_ROWS; ++i)
        lfill(TILE_STORE + VIEW_ROW_STRIDE * (row + i) + 64UL * col, 0,
              64 * PATCH_COLUMNS);

    view_blit_begin();
    for (i = 0; i < PATCHES; ++i) {
        r = patch_ring[i];
        nx = city_ax[c];
        ny = city_ay[c];
        if (r != PATCH_CITY) {
            // Off the top or bottom of the map there is nothing, and black.
            if (work_owner[r] == CITY_NONE) continue;
            nx = work_ring_x[r];
            ny = work_ring_y[r];
        }
        x = col + patch_x[i];
        y = row + patch_y[i];
        t = map_tile[ny][nx] | map_show;
        if (!(t & MAP_EXPLORED)) t = TILE_UNDISCOVERED;
        view_draw_tile(x, y, t & MAP_TERRAIN_MASK);
        if (MAP_HAS_OVERLAY(t))
            view_draw_tile(x, y, MAP_OVERLAY_TILE(t));
        if ((t & MAP_EXPLORED) && (n = map_resource_at(nx, ny)) != RES_NONE)
            view_draw_tile(x, y, RES_TILE(n, t));
    }
    // The overlays after all the terrain, as view_redraw() does a row
    // behind it. The active unit does not blink here, so it is always shown.
    unit_hidden = 0;
    for (i = 0; i < PATCHES; ++i) {
        r = patch_ring[i];
        nx = city_ax[c];
        ny = city_ay[c];
        if (r != PATCH_CITY) {
            if (work_owner[r] == CITY_NONE) continue;
            nx = work_ring_x[r];
            ny = work_ring_y[r];
        }
        x = col + patch_x[i];
        y = row + patch_y[i];
        if (r != PATCH_CITY && work_on[r])
            view_draw_tile(x, y, TILE_SELECT);
        t = map_tile[ny][nx] | map_show;
        if ((t & (MAP_EXPLORED | MAP_OCCUPIED)) == (MAP_EXPLORED | MAP_OCCUPIED)) {
            n = unit_overlay(nx, ny, tiles);
            for (k = 0; k < n; ++k) view_draw_tile(x, y, tiles[k]);
            if (n) view_draw_flag(x, y, unit_overlay_owner);
        } else if (r != PATCH_CITY && work_owner[r] != c) {
            view_draw_flag(x, y, city_owner[work_owner[r]]);
        }
    }
}

// The patch's hexagon under the arrow, or 0xff: the nearest middle of a
// hexagon, if it is within a hexagon's reach. In sprite units, which are
// square on the glass: four a character across, eight down.
static byte patch_under_pointer(void)
{
    byte i, best = 0xff;
    int dx, dy;
    word d, best_d = 18 * 18;

    for (i = 0; i < PATCHES; ++i) {
        dx = (int)input_x - 4 * (int)(PATCH_LEFT + patch_x[i] + 4);
        dy = (int)input_y - 8 * (int)(PATCH_TOP + patch_y[i] + 2) - 4;
        if (dx > 40 || dx < -40 || dy > 40 || dy < -40) continue;
        d = (word)(dx * dx + dy * dy);
        if (d >= best_d) continue;
        best_d = d;
        best = i;
    }
    return best;
}

// The arrow onto the middle of the patch's hexagon i.
static void point_at(byte i)
{
    input_point(4 * (PATCH_LEFT + patch_x[i] + 4),
                8 * (PATCH_TOP + patch_y[i] + 2) + 4);
}

// **The keys move the arrow** from hexagon to hexagon of the patch, the
// moving keys each way and the cursor keys across and, zigzagging, up and
// down, as tile select does on the map; from wherever it is, or the city
// if it is off the patch. Space then does what a click would.
static void step(byte dir)
{
    byte i = patch_under_pointer(), k;
    int x, y;

    if (i == 0xff) i = PATCH_MIDDLE;
    x = (int)patch_x[i] + step_x[dir];
    y = (int)patch_y[i] + step_y[dir];
    for (k = 0; k < PATCHES; ++k)
        if (patch_x[k] == x && patch_y[k] == y) {
            point_at(k);
            return;
        }
    point_at(i);
}

static byte key_step(byte key)
{
    byte i = patch_under_pointer();
    byte left = i != 0xff && patch_x[i] > 16;   // east of the middle column

    switch (key) {
    case KEY_LEFT: return DIR_W;
    case KEY_RIGHT: return DIR_E;
    case KEY_UP: return left ? DIR_NW : DIR_NE;
    case KEY_DOWN: return left ? DIR_SW : DIR_SE;
    }
    return key_dir(key);
}

// ------------------------------------------------------------- the units

// The units in the list, two rows each: a click on one, or its number key,
// wakes it if it has orders and fortifies it if it is a soldier without.
// This is the way to a unit fortified in a city, which the map does not
// draw.
#define UNITS_LISTED 9
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
            screen_putsxy(x + 2, y + 1, "fortified", COLOR_LIGHTGREY);
        else if (unit_orders[u] != ORDERS_NONE)
            screen_putsxy(x + 2, y + 1, "working", COLOR_LIGHTGREY);
        y += 2;
    }
    if (listed_n)
        screen_putsxy(x, LIST_ROW - 1, "1-9 wake/fortify", COLOR_LIGHTGREY);
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
    else if (unit_kind[u] == KIND_WARRIOR || unit_kind[u] == KIND_SPEARMAN
             || unit_kind[u] == KIND_CHARIOT)
        unit_orders[u] = ORDERS_FORTIFY;
    else return;
    draw_units(c);
}

static byte unit_under_pointer(void)
{
    byte i;
    for (i = 0; i < listed_n; ++i)
        if (pointer_on(LIST_ROW + 2 * i, TEXT_LEFT(UNITS_LEFT),
                       UNITS_RIGHT - 1)
            || pointer_on(LIST_ROW + 2 * i + 1, TEXT_LEFT(UNITS_LEFT),
                          UNITS_RIGHT - 1))
            return i;
    return 0xff;
}

// ------------------------------------------------------- the city itself

// Size, food, shields and trade, from city_work()'s totals.
static void draw_info(byte c)
{
    byte size = city_size[c];
    byte box = GROWTH_BOX(size);
    byte eaten = 2 * size;
    byte x0 = PATCH_LEFT + 1;
    byte x, y, more;

    for (y = INFO_TOP + 1; y < INFO_BOTTOM; ++y)
        screen_fill(PATCH_LEFT, y, PATCH_COLUMNS, TEXT_COLOUR);
    y = INFO_TOP + 1;

    x = x0;
    put(&x, y, "Size ", COLOR_WHITE);
    put(&x, y, number(size), TEXT_COLOUR);
    x = x0 + 9;
    put(&x, y, "Food ", COLOR_WHITE);
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
    if (work_idle) {
        x = x0 + 31;
        put(&x, y, "Idle ", COLOR_WHITE);
        put(&x, y, number(work_idle), COLOR_YELLOW);
    }

    x = x0;
    put(&x, ++y, "Shields ", COLOR_WHITE);
    put(&x, y, number(work_shields), TEXT_COLOUR);
    x = x0 + 11;
    put(&x, y, "Trade ", COLOR_WHITE);
    put(&x, y, number(work_trade), TEXT_COLOUR);
    x = x0 + 21;
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

    // Its buildings (src/culture.h).
    x = x0;
    ++y;
    if (city_culture[c] & CITY_TEMPLE) put(&x, y, "Temple ", COLOR_YELLOW);
    if (city_culture[c] & CITY_LION_GATE) put(&x, y, "Lion Gate", COLOR_YELLOW);
}

// ------------------------------------------------------------ production

// **Civilization's price** for the rest of what city c builds: 2r + r*r/20
// gold for the r shields still wanted for a unit, 2r for a building and
// 4r for a wonder; twice that before the first shield. 0 if there is
// nothing to buy.
static byte buy_width;          // the BUY button's, for the click

static word buy_cost(byte c)
{
    word r, cost = unit_kind_cost[city_build[c]];

    if (city_build[c] == KIND_NONE || city_shields[c] >= cost) return 0;
    r = cost - city_shields[c];
    cost = city_build[c] == KIND_LION_GATE ? 4 * r
           : city_build[c] == KIND_TEMPLE ? 2 * r : 2 * r + r * r / 20;
    return city_shields[c] ? cost : 2 * cost;
}

static void draw_build(byte c)
{
    byte x = TEXT_LEFT(BUILD_LEFT);
    byte kind = city_build[c];
    byte turns = city_turns_left(c);
    const char *s;

    blank_box(BUILD_LEFT, BUILD_RIGHT);
    screen_putsxy(x, HEADING_ROW, "Producing", COLOR_WHITE);
    screen_putsxy(x, LIST_ROW, unit_kind_name[kind], TEXT_COLOUR);
    // "12/40 shields"
    put(&x, LIST_ROW + 1, number(city_shields[c]), TEXT_COLOUR);
    put(&x, LIST_ROW + 1, "/", TEXT_COLOUR);
    put(&x, LIST_ROW + 1, number(unit_kind_cost[kind]), TEXT_COLOUR);
    put(&x, LIST_ROW + 1, " shields", TEXT_COLOUR);

    // "3 a turn", "4 turns"
    x = TEXT_LEFT(BUILD_LEFT);
    put(&x, LIST_ROW + 2, number(city_production(c)), TEXT_COLOUR);
    put(&x, LIST_ROW + 2, " a turn", TEXT_COLOUR);
    x = TEXT_LEFT(BUILD_LEFT);
    put(&x, LIST_ROW + 3, number(turns), TEXT_COLOUR);
    put(&x, LIST_ROW + 3, turns == 1 ? " turn" : " turns", TEXT_COLOUR);
    // It takes a citizen with it (city_produce()).
    if (kind == KIND_SETTLER && city_size[c] < 2)
        screen_putsxy(TEXT_LEFT(BUILD_LEFT), LIST_ROW + 4,
                      "waits for size 2", COLOR_YELLOW);
    screen_putsxy(TEXT_LEFT(BUILD_LEFT), CHANGE_ROW, CHANGE_TEXT,
                  BUTTON_CHANGE);

    // The treasury, and BUY with its price on it.
    x = TEXT_LEFT(BUILD_LEFT);
    put(&x, LIST_ROW + 6, "Gold ", COLOR_WHITE);
    put(&x, LIST_ROW + 6, number(player_gold[PLAYER_HUMAN]), TEXT_COLOUR);
    buy_width = 0;
    if (!buy_cost(c)) return;
    s = number(buy_cost(c));
    buy_width = 6 + length(s);
    kind = player_gold[PLAYER_HUMAN] >= buy_cost(c) ? BUTTON_CHANGE
                                                    : BUTTON_OFF;
    x = TEXT_LEFT(BUILD_LEFT);
    screen_putsxy(x, BUY_ROW, " BUY ", kind);
    screen_putsxy(x + 5, BUY_ROW, s, kind);
    screen_putsxy(x + buy_width - 1, BUY_ROW, " ", kind);
}

// B or the BUY button: the rest of the production for gold, finished at
// the end of the turn, if the treasury has enough.
static void buy(byte c)
{
    word cost = buy_cost(c);
    if (!cost || player_gold[PLAYER_HUMAN] < cost) return;
    player_gold[PLAYER_HUMAN] -= cost;
    city_shields[c] = unit_kind_cost[city_build[c]];
    draw_build(c);
}

static void draw_choice(byte i, byte selected)
{
    byte x = TEXT_LEFT(BUILD_LEFT);
    byte kind = buildable[i];
    byte colour = (i == selected) ? (ATTR_REVERSE | COLOR_WHITE)
                                  : TEXT_COLOUR;

    screen_fill(x, CHOICE_ROW + i, BUILD_RIGHT - x, colour);
    screen_putsxy(x + 1, CHOICE_ROW + i, unit_kind_name[kind], colour);
    screen_putsxy(x + 13, CHOICE_ROW + i, number(unit_kind_cost[kind]),
                  colour);
}

static byte choice_under_pointer(void)
{
    byte i;
    for (i = 0; i < BUILDABLE; ++i)
        if (pointer_on(CHOICE_ROW + i, TEXT_LEFT(BUILD_LEFT), BUILD_RIGHT - 1))
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
    byte horses = player_has(PLAYER_HUMAN, RES_HORSES);
    byte coast = city_coastal(c), row;

    buildable[0] = KIND_WARRIOR;
    buildable[1] = KIND_SETTLER;
    buildable[2] = KIND_BUILDER;
    buildable_n = 3;
    if (copper && tin) buildable[buildable_n++] = KIND_SPEARMAN;
    if (horses) buildable[buildable_n++] = KIND_CHARIOT;
    if (cedar && coast) buildable[buildable_n++] = KIND_GALLEY;
    if (coast) {
        buildable[buildable_n++] = KIND_BOAT;
        buildable[buildable_n++] = KIND_MERCHANT;
    }
    for (i = KIND_TEMPLE; i < BUILD_KINDS; ++i)
        if (CULTURE_CAN(c, i)) buildable[buildable_n++] = i;

    for (i = 0; i < BUILDABLE; ++i)
        if (buildable[i] == city_build[c]) sel = i;

    blank_box(BUILD_LEFT, BUILD_RIGHT);
    screen_putsxy(TEXT_LEFT(BUILD_LEFT), HEADING_ROW, "Build what?",
                  COLOR_WHITE);
    for (i = 0; i < BUILDABLE; ++i) draw_choice(i, sel);
    // Under the list, a line each, what is missing for the rest.
    row = CHOICE_ROW + BUILDABLE + 1;
    if (!(copper && tin))
        screen_putsxy(TEXT_LEFT(BUILD_LEFT), row++,
                      copper ? "Spearman: tin"
                      : tin ? "Spearman: copper" : "Spearman: bronze",
                      COLOR_LIGHTGREY);
    if (!horses)
        screen_putsxy(TEXT_LEFT(BUILD_LEFT), row++, "Chariot: horses",
                      COLOR_LIGHTGREY);
    if (coast && !cedar)
        screen_putsxy(TEXT_LEFT(BUILD_LEFT), row, "Galley: cedar",
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

// ------------------------------------------------------------ the screen

// **The player picks the hexagons**: hexagon r of city c's RING, if it is
// the city's, let go if it is worked, its citizen idle, or taken if a
// citizen is idle; and city_worked[] (city.h) keeps them from now on,
// CITY_MANUAL. Whether anything changed. Out of src/growth.c, whose
// region 1 is full, after city_work() has set work_on[] and the rest.
static byte work_toggle(byte c, byte r)
{
    byte k, n = 0;

    city_work(c);
    if (work_owner[r] != c) return 0;
    if (!work_on[r]) {
        for (k = 0; k < RING; ++k) n += work_on[k];
        if (n >= city_size[c]) return 0;
    }
    work_on[r] ^= 1;
    city_worked[0][c] = city_worked[1][c] = 0;
    city_worked[CITY_MANUAL_BYTE][c] = CITY_MANUAL;
    for (k = 0; k < RING; ++k)
        if (work_on[k]) city_worked[k >> 3][c] |= (byte)(1 << (k & 7));
    return 1;
}

// The patch's hexagon i worked or not, by the player; everything that
// depends on it drawn again.
static void toggle(byte c, byte i)
{
    if (i == 0xff || patch_ring[i] == PATCH_CITY
        || !work_toggle(c, patch_ring[i]))
        return;
    city_work(c);
    draw_patch(c);
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
    draw_patch(c);
    screen_clear();
    view_restore(PATCH_LEFT - VIEW_LEFT, PATCH_TOP - VIEW_TOP,
                 PATCH_COLUMNS, PATCH_ROWS);

    screen_putsxy((SCREEN_COLUMNS - length(name)) / 2, 0, name, COLOR_WHITE);
    screen_box(UNITS_LEFT, BOX_TOP, UNITS_RIGHT, BOX_BOTTOM, BORDER_COLOUR);
    screen_box(PATCH_LEFT - 1, PATCH_TOP - 1, PATCH_LEFT + PATCH_COLUMNS,
               PATCH_TOP + PATCH_ROWS, BORDER_COLOUR);
    screen_box(BUILD_LEFT, BOX_TOP, BUILD_RIGHT, BOX_BOTTOM, BORDER_COLOUR);
    screen_box(PATCH_LEFT - 1, INFO_TOP, PATCH_LEFT + PATCH_COLUMNS,
               INFO_BOTTOM, BORDER_COLOUR);
    draw_info(c);
    draw_units(c);
    draw_build(c);
    screen_putsxy(0, EXIT_ROW,
                  "c change  b buy  keys or click: a hexagon  space: work it"
                  "  esc", COLOR_LIGHTGREY);
    screen_putsxy(EXIT_LEFT, EXIT_ROW, EXIT_TEXT, BUTTON_EXIT);
    point_at(PATCH_MIDDLE);

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
            else if (buy_width
                     && pointer_on(BUY_ROW, TEXT_LEFT(BUILD_LEFT),
                                   TEXT_LEFT(BUILD_LEFT) + buy_width - 1))
                buy(c);
            else if (unit_under_pointer() != 0xff)
                wake(c, unit_under_pointer());
            else
                toggle(c, patch_under_pointer());
        } else if (key == KEY_SPACE) {
            toggle(c, patch_under_pointer());
        } else if (key == KEY_ESC || key == KEY_RUNSTOP
                   || key == KEY_RETURN || key == 'x') {
            return;
        } else if (key == 'c') {
            change(c);
        } else if (key == 'b') {
            buy(c);
        } else if (key >= '1' && key < '1' + UNITS_LISTED) {
            wake(c, key - '1');
        } else {
            key = key_step(key);
            if (key != DIR_NONE) step(key);
        }
    }
}
