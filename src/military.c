// The military advisor (24 Oct 2026, the user's, for completeness beside
// Cities, Diplomacy and Finance): a row for each kind of unit the human
// can have, with how many, how many of them are fortified and how many at
// sea, and the kind's figures; then the army's upkeep (treasury.h) and the
// wars.

// In region 5 (src/region.h): called through the stub in src/dispatch.s,
// under this name here.
#define military_screen military_screen_r5

#include "military.h"
#include "city.h"
#include "civ.h"
#include "combat.h"
#include "diplomacy.h"
#include "input.h"
#include "map.h"
#include "screen.h"
#include "sheet.h"
#include "treasury.h"
#include "unit.h"

#pragma clang section text="r5text" rodata="r5rodata"

#define BUTTON_EXIT (ATTR_REVERSE | COLOR_RED)
#define EXIT_ROW (SCREEN_ROWS - 1)
#define EXIT_TEXT " EXIT "
#define EXIT_LEFT (SCREEN_COLUMNS - 2 - (sizeof EXIT_TEXT - 1))
#define LEFT 8
#define TABLE_TOP 4
#define FIGURES 6
static const char *const heading[FIGURES] = {
    "Units", "Fortified", "At sea", "Attack", "Defence", "Moves"
};
static const byte column_right[FIGURES] = { 32, 43, 51, 59, 68, 75 };

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

void military_screen(void)
{
    byte k, u, i, y = TABLE_TOP + 2, key, t, n, all = 0, cities = 0, free;
    byte figure[FIGURES];
    byte colour;

    sprite_hide(2);             // the minimap's outline, which is gone
    sprite_hide(3);
    screen_clear();
    screen_putsxy(LEFT, 2, "Military Advisor", COLOR_WHITE);
    screen_putsxy(LEFT, TABLE_TOP, "Kind", COLOR_LIGHTGREY);
    for (i = 0; i < FIGURES; ++i)
        put_right(column_right[i], TABLE_TOP, heading[i], COLOR_LIGHTGREY);

    // Every kind but the Sea Peoples' raider, which nobody builds.
    for (k = KIND_SETTLER; k < KINDS; ++k) {
        if (k == KIND_RAIDER) continue;
        figure[0] = figure[1] = figure[2] = 0;
        for (u = 0; u < MAX_UNITS; ++u) {
            if (unit_kind[u] != k || unit_owner[u] != PLAYER_HUMAN) continue;
            ++figure[0];
            t = MAP_TERRAIN(unit_ax[u], unit_ay[u]);
            if (!KIND_IS_SHIP(k)
                && (t == TILE_OCEAN || t == TILE_SHALLOW_OCEAN))
                ++figure[2];            // aboard a galley
            else if (unit_orders[u] == ORDERS_FORTIFY)
                ++figure[1];
        }
        figure[3] = unit_kind_attack[k];
        figure[4] = unit_kind_defence[k];
        figure[5] = unit_kind_moves[k];
        all += figure[0];
        colour = figure[0] ? COLOR_LIGHTGREEN : COLOR_LIGHTGREY;
        screen_putsxy(LEFT, y, unit_kind_name[k], colour);
        for (i = 0; i < FIGURES; ++i)
            put_right(column_right[i], y, number(figure[i]),
                      i == 0 && figure[0] ? COLOR_YELLOW : colour);
        ++y;
    }

    // The upkeep, as treasury_turn() takes it.
    for (i = 0; i < city_count; ++i)
        if (city_owner[i] == PLAYER_HUMAN) ++cities;
    free = ARMY_FREE_BASE + ARMY_FREE_A_CITY * cities;
    y += 1;
    screen_putsxy(LEFT, y, "Units in all", COLOR_WHITE);
    put_right(column_right[0], y, number(all), COLOR_YELLOW);
    screen_putsxy(LEFT, y + 1, "Free: 4, and 3 a city", COLOR_WHITE);
    put_right(column_right[0], y + 1, number(free), COLOR_LIGHTGREEN);
    screen_putsxy(LEFT, y + 2, "Upkeep, gold a turn", COLOR_WHITE);
    put_right(column_right[0], y + 2,
              number(all > free ? ARMY_UPKEEP * (all - free) : 0),
              all > free ? COLOR_RED : COLOR_LIGHTGREEN);

    // The wars, with the peoples met.
    y += 4;
    screen_putsxy(LEFT, y, "At war with:", COLOR_WHITE);
    n = 0;
    i = LEFT + 13;
    for (k = PLAYER_FIRST_AI; k < PLAYERS; ++k) {
        if (!(diplomacy_known[PLAYER_HUMAN] & (byte)(1 << k))
            || !player_alive(k) || !at_war(PLAYER_HUMAN, k))
            continue;
        if (n++) {
            screen_putsxy(i, y, ",", COLOR_WHITE);
            i += 2;
        }
        screen_putsxy(i, y, player_people(k), COLOR_RED);
        i += length(player_people(k));
    }
    screen_putsxy(i, y, n ? ", and the barbarians" : "the barbarians only",
                  n ? COLOR_WHITE : COLOR_LIGHTGREEN);

    screen_putsxy(0, EXIT_ROW, "Esc or Return to go on", COLOR_LIGHTGREY);
    screen_putsxy(EXIT_LEFT, EXIT_ROW, EXIT_TEXT, BUTTON_EXIT);

    do {
        screen_wait_raster(SCREEN_ROW_LINE(SCREEN_ROWS - 1));
        key = input_poll();
    } while (!key);
}
