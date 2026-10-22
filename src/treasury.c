// In region 5 (src/region.h): called through the stubs in src/dispatch.s,
// under these names here.
#define treasury_turn treasury_turn_r5
#define finance_screen finance_screen_r5

#include "treasury.h"
#include "city.h"
#include "combat.h"
#include "crisis.h"
#include "game.h"
#include "growth.h"
#include "input.h"
#include "map.h"
#include "screen.h"
#include "trade.h"

#pragma clang section text="r5text" rodata="r5rodata"

HIGH_BSS word player_gold[PLAYERS];

#define NO_CAPITAL 32           // Civilization's distance without a palace

// Hexagons between two cells, as ai.c's distance().
static word distance(byte ax, byte ay, byte bx, byte by)
{
    int dx = (int)ax - (int)bx;
    int dy = (2 * (int)ay + (ax & 1)) - (2 * (int)by + (bx & 1));
    if (dx < 0) dx = -dx;
    if (dx > MAP_COLUMNS / 2) dx = MAP_COLUMNS - dx;
    if (dy < 0) dy = -dy;
    return (word)(dy + (dx > dy ? (dx - dy) / 2 : 0));
}

// City c's trade and its corruption, into these.
static byte trade, corruption;

static void income(byte c)
{
    byte k = crisis_capital(city_owner[c]);
    word d = k == CITY_NONE ? NO_CAPITAL
             : distance(city_ax[c], city_ay[c], city_ax[k], city_ay[k]);
    word lost;

    city_work(c);
    trade = work_trade;
    lost = (word)trade * d * 3 / 100;
    corruption = lost > trade ? trade : (byte)lost;
}

// Trade deals p is in.
static byte deals(byte p)
{
    byte i, n = 0;
    for (i = 0; i < MAX_DEALS; ++i)
        if (deal_from[i] != DEAL_FREE && (deal_from[i] == p || deal_to[i] == p))
            ++n;
    return n;
}

static void add(byte p, word g)
{
    word gold = player_gold[p];
    player_gold[p] = gold + g > TREASURY_MAX ? TREASURY_MAX : gold + g;
}

void treasury_turn(void)
{
    byte c, p, u;
    word buy, left;

    for (p = 0; p < PLAYERS; ++p) add(p, TREASURY_DEAL * deals(p));
    for (c = 0; c < city_count; ++c) {
        p = city_owner[c];
        if (p >= PLAYERS) continue;             // the rebels' (crisis.c)
        income(c);
        add(p, trade - corruption);
        // The computer players buy as Civilization's do: shields for gold,
        // two to one, for a soldier or a galley under way -- up to a third
        // of the treasury in a city with nobody in it, a 64th otherwise.
        // (Civilization's empty city builds a defender anyway. Buying its
        // settlers too emptied the cities: 29 citizens in all at turn 301
        // on seed 1 against 52 without buying.)
        u = city_build[c];
        if (p < PLAYER_FIRST_AI || !city_shields[c]
            || (u != KIND_WARRIOR && u != KIND_SPEARMAN && u != KIND_GALLEY))
            continue;
        u = unit_at(city_ax[c], city_ay[c]);
        buy = player_gold[p] / (u == UNIT_NONE ? 3 : 64);
        left = unit_kind_cost[city_build[c]];
        left = left > city_shields[c] ? left - city_shields[c] : 0;
        if (buy > left) buy = left;
        city_shields[c] += buy;
        player_gold[p] -= 2 * buy;
    }
}

// ------------------------------------------------------------ the screen

#define BUTTON_EXIT (ATTR_REVERSE | COLOR_RED)
#define EXIT_ROW (SCREEN_ROWS - 1)
#define EXIT_TEXT " EXIT "
#define EXIT_LEFT (SCREEN_COLUMNS - 2 - (sizeof EXIT_TEXT - 1))
#define LEFT 8
#define TABLE_TOP 5
#define TABLE_ROWS 14
#define FIGURES 3
static const char *const heading[FIGURES] = { "Trade", "Corruption", "Gold" };
static const byte column_right[FIGURES] = { 35, 48, 56 };

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

void finance_screen(void)
{
    byte c, i, y = TABLE_TOP + 2, key, n;
    word total = 0;

    sprite_hide(2);             // the minimap's outline, which is gone
    sprite_hide(3);
    screen_clear();
    screen_putsxy(LEFT, 2, "Treasury:", COLOR_WHITE);
    screen_putsxy(LEFT + 10, 2, number(player_gold[PLAYER_HUMAN]),
                  COLOR_YELLOW);
    screen_putsxy(LEFT + 11 + length(number(player_gold[PLAYER_HUMAN])), 2,
                  "gold", COLOR_WHITE);
    screen_putsxy(LEFT, TABLE_TOP, "City", COLOR_LIGHTGREY);
    for (i = 0; i < FIGURES; ++i)
        put_right(column_right[i], TABLE_TOP, heading[i], COLOR_LIGHTGREY);

    for (c = 0; c < city_count; ++c) {
        if (city_owner[c] != PLAYER_HUMAN) continue;
        income(c);
        total += trade - corruption;
        if (y >= TABLE_TOP + 2 + TABLE_ROWS) continue;  // counted, not listed
        screen_putsxy(LEFT, y, city_name_of(c), COLOR_LIGHTGREEN);
        put_right(column_right[0], y, number(trade), COLOR_LIGHTGREEN);
        put_right(column_right[1], y, number(corruption), COLOR_LIGHTGREEN);
        put_right(column_right[2], y, number(trade - corruption), COLOR_YELLOW);
        ++y;
    }
    n = deals(PLAYER_HUMAN);
    total += TREASURY_DEAL * n;
    ++y;
    screen_putsxy(LEFT, y, "Trade deals", COLOR_LIGHTGREEN);
    put_right(column_right[0], y, number(n), COLOR_LIGHTGREEN);
    put_right(column_right[2], y, number(TREASURY_DEAL * n), COLOR_YELLOW);
    y += 2;
    screen_putsxy(LEFT, y, "Income a turn", COLOR_WHITE);
    put_right(column_right[2], y, number(total), COLOR_YELLOW);
    screen_putsxy(LEFT, y + 1, "Expenses: none", COLOR_LIGHTGREY);
    screen_putsxy(LEFT, y + 3, "Corruption grows with the distance from the "
                  "capital.", COLOR_LIGHTGREY);
    screen_putsxy(LEFT, y + 4, "Gold buys production: b on the city screen.",
                  COLOR_LIGHTGREY);
    screen_putsxy(0, EXIT_ROW, "Esc or Return to go on", COLOR_LIGHTGREY);
    screen_putsxy(EXIT_LEFT, EXIT_ROW, EXIT_TEXT, BUTTON_EXIT);

    do {
        screen_wait_raster(SCREEN_ROW_LINE(SCREEN_ROWS - 1));
        key = input_poll();
    } while (!key);
}
