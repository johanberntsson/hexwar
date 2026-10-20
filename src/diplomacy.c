// In region 2 (src/region.h): called through the stubs in src/dispatch.s,
// under these names here.
#define diplomacy_contact diplomacy_contact_r2
#define diplomacy_greet diplomacy_greet_r2
#define diplomacy_screen diplomacy_screen_r2

#include "diplomacy.h"
#include "city.h"
#include "civ.h"
#include "combat.h"
#include "input.h"
#include "map.h"
#include "popup.h"
#include "screen.h"
#include "unit.h"

#pragma clang section text="r2text" rodata="r2rodata"

byte diplomacy_met;
byte diplomacy_ax, diplomacy_ay;

#define TITLE "Diplomacy"
#define BUTTON_EXIT (ATTR_REVERSE | COLOR_RED)
#define EXIT_ROW (SCREEN_ROWS - 1)
#define EXIT_TEXT " EXIT "
#define EXIT_LEFT (SCREEN_COLUMNS - 2 - (sizeof EXIT_TEXT - 1))
#define LIST_LEFT 8
#define LIST_TOP 4
#define BLOCK_ROWS 5            // rows a people takes, with a blank one
#define STATE_LEFT 56           // "at war", "at peace", "not yet met"

// Whether (ax, ay) is on a hexagon the player has explored -- really,
// whatever Show Map says, or a debugging session would meet everybody.
#define SEEN(AX, AY) (map_tile[AY][AX] & MAP_EXPLORED)

byte diplomacy_contact(void)
{
    byte i, p, seen = 0;

    for (i = 0; i < MAX_UNITS; ++i) {
        p = unit_owner[i];
        if (unit_kind[i] != KIND_NONE && p != PLAYER_HUMAN
            && p != PLAYER_BARBARIAN && SEEN(unit_ax[i], unit_ay[i])
            && !((diplomacy_met >> p) & 1) && !((seen >> p) & 1)) {
            seen |= (byte)(1 << p);
            diplomacy_ax = unit_ax[i];
            diplomacy_ay = unit_ay[i];
        }
    }
    // A city after the units, so that it is what is shown if there is one.
    for (i = 0; i < city_count; ++i) {
        p = city_owner[i];
        if (p != PLAYER_HUMAN && SEEN(city_ax[i], city_ay[i])
            && !((diplomacy_met >> p) & 1)) {
            seen |= (byte)(1 << p);
            diplomacy_ax = city_ax[i];
            diplomacy_ay = city_ay[i];
        }
    }
    diplomacy_met |= seen;
    return seen;
}

void diplomacy_greet(byte met)
{
    byte p;

    for (p = PLAYER_FIRST_AI; p < PLAYERS; ++p) {
        if (!((met >> p) & 1)) continue;
        // The greeting is the letters' way of putting it (vision.md 3).
        popup_begin();
        popup_add("You have met the ");
        popup_add(player_people(p));
        popup_add(".\n\nTheir ruler is ");
        popup_add(civ_leader(player_civ[p]));
        popup_add(", ");
        popup_add(civ_title(player_civ[p]));
        popup_add(".");
        popup_show();
    }
}

static char digits[4];

static const char *number(byte v)
{
    byte i = sizeof digits - 1;
    digits[i] = 0;
    do { digits[--i] = (char)('0' + v % 10); v /= 10; } while (v);
    return digits + i;
}

// Write `s` at (*x, y) and move *x past it.
static void put(byte *x, byte y, const char *s, byte colour)
{
    screen_putsxy(*x, y, s, colour);
    while (*s++) ++*x;
}

static void put_count(byte *x, byte y, const char *what, byte n)
{
    put(x, y, what, COLOR_LIGHTGREY);
    put(x, y, number(n), COLOR_LIGHTGREEN);
    *x += 3;
}

// Player p's cities, citizens and units, on row y.
static void draw_counts(byte p, byte y)
{
    byte i, x = LIST_LEFT + 2, cities = 0, citizens = 0, units = 0;

    for (i = 0; i < city_count; ++i)
        if (city_owner[i] == p) {
            ++cities;
            citizens += city_size[i];
        }
    for (i = 0; i < MAX_UNITS; ++i)
        if (unit_kind[i] != KIND_NONE && unit_owner[i] == p) ++units;
    put_count(&x, y, "Cities ", cities);
    put_count(&x, y, "Citizens ", citizens);
    put_count(&x, y, "Units ", units);
}

// Player p's block: the people and how things stand, the ruler, the counts.
static void draw_player(byte p, byte y)
{
    byte x = LIST_LEFT, civ = player_civ[p];

    put(&x, y, player_people(p), COLOR_WHITE);
    if (p == PLAYER_HUMAN) {
        put(&x, y, " (you)", COLOR_LIGHTGREY);
    } else if (!((diplomacy_met >> p) & 1)) {
        screen_putsxy(STATE_LEFT, y, "not yet met", COLOR_LIGHTGREY);
        return;
    } else if (!player_alive(p)) {
        screen_putsxy(STATE_LEFT, y, "no more", COLOR_LIGHTGREY);
        return;
    } else if (at_war(PLAYER_HUMAN, p)) {
        screen_putsxy(STATE_LEFT, y, "at war", COLOR_RED);
    } else {
        screen_putsxy(STATE_LEFT, y, "at peace", COLOR_LIGHTGREEN);
    }
    x = LIST_LEFT + 2;
    put(&x, y + 1, civ_leader(civ), COLOR_YELLOW);
    put(&x, y + 1, ", ", COLOR_LIGHTGREY);
    put(&x, y + 1, civ_title(civ), COLOR_LIGHTGREY);
    draw_counts(p, y + 2);
}

void diplomacy_screen(void)
{
    byte key, p;

    sprite_hide(2);             // the minimap's outline, which is gone
    sprite_hide(3);
    screen_clear();
    screen_putsxy((SCREEN_COLUMNS - (sizeof TITLE - 1)) / 2, 1, TITLE,
                  COLOR_WHITE);
    for (p = 0; p < PLAYERS; ++p)
        draw_player(p, LIST_TOP + BLOCK_ROWS * p);
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
