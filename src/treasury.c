// In region 6 (src/region.h): called through the stubs in src/dispatch.s,
// under these names here.
#define treasury_turn treasury_turn_r6
#define finance_screen finance_screen_r6

#include "treasury.h"
#include "city.h"
#include "combat.h"
#include "crisis.h"
#include "culture.h"
#include "game.h"
#include "gameint.h"
#include "growth.h"
#include "input.h"
#include "map.h"
#include "screen.h"
#include "sheet.h"
#include "trade.h"

#pragma clang section text="r6text" rodata="r6rodata"

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

// p's capital, as crisis_capital() in region 5 has it: the first city it
// founded, while it holds it. A copy, as a stub would cost resident room.
static byte capital(byte p)
{
    byte c;
    for (c = 0; c < city_count; ++c)
        if (city_founder[c] == p && city_name[c] == 0 && city_owner[c] == p)
            return c;
    return CITY_NONE;
}

// City c's trade and its corruption, into these.
static byte trade, corruption;

static void income(byte c)
{
    byte k = capital(city_owner[c]);
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

// p's cities.
static byte cities(byte p)
{
    byte c, n = 0;
    for (c = 0; c < city_count; ++c)
        if (city_owner[c] == p) ++n;
    return n;
}

// The units beyond p's free ones, with n cities, and what a realm of n
// cities costs a turn (treasury.h).
static byte over_army(byte p, byte n)
{
    byte units = unit_count_kind(p, KIND_NONE);
    byte free = ARMY_FREE_BASE + ARMY_FREE_A_CITY * n;
    return units > free ? units - free : 0;
}

static word realm_cost(byte n)
{
    byte k = n > REALM_FREE ? n - REALM_FREE : 0;
    return (word)k * k;
}

// One of p's units disbanded for want of gold: a land unit on land, not a
// settler, the last in the list that is not in a city, or else the last
// in one -- never a ship, which might have somebody aboard.
static void disband(byte p)
{
    byte u, k, pick = UNIT_NONE, t;
    for (u = MAX_UNITS; u-- > 0;) {
        k = unit_kind[u];
        if (unit_owner[u] != p || k == KIND_NONE || k == KIND_SETTLER
            || KIND_IS_SHIP(k))
            continue;
        t = MAP_TERRAIN(unit_ax[u], unit_ay[u]);
        if (t == TILE_OCEAN || t == TILE_SHALLOW_OCEAN) continue;
        if (city_at(unit_ax[u], unit_ay[u]) == CITY_NONE) {
            pick = u;
            break;
        }
        if (pick == UNIT_NONE) pick = u;
    }
    if (pick == UNIT_NONE) return;
    if (p == PLAYER_HUMAN) message("No gold: a unit", "was disbanded");
    unit_remove(pick);
}

void treasury_turn(void)
{
    byte c, p, u, n;
    word buy, left, cost;

    for (p = 0; p < PLAYERS; ++p) add(p, TREASURY_DEAL * deals(p));
    for (c = 0; c < city_count; ++c) {
        p = city_owner[c];
        if (p >= PLAYERS) continue;             // the rebels' (crisis.c)
        income(c);
        add(p, trade - corruption);
        // A temple's upkeep, Civilization's 1 a turn; with nothing in the
        // treasury it is sold, as Civilization sells a building.
        if (city_culture[c] & CITY_TEMPLE) {
            if (player_gold[p]) {
                --player_gold[p];
            } else {
                city_culture[c] &= (byte)~CITY_TEMPLE;
                if (p == PLAYER_HUMAN) message("No gold: a temple", "was sold");
            }
        }
    }
    // The realm's cost, and the army's, once the turn's gold is in; one
    // unit goes for want of it.
    for (p = 0; p < PLAYERS; ++p) {
        n = cities(p);
        cost = realm_cost(n);
        player_gold[p] = player_gold[p] > cost ? player_gold[p] - cost : 0;
        cost = ARMY_UPKEEP * over_army(p, n);
        if (player_gold[p] >= cost) {
            player_gold[p] -= cost;
        } else {
            player_gold[p] = 0;
            disband(p);
        }
    }
    for (c = 0; c < city_count; ++c) {
        p = city_owner[c];
        if (p >= PLAYERS) continue;
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
// Cities listed; the rest are counted, not listed. **Everything has to end
// above EXIT_ROW**: a row past the screen is code slot 1 at $13000.
#define TABLE_ROWS 6
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

// An expense on row y: its name, how many, and the gold in red.
static void expense(byte y, const char *what, byte count, word gold)
{
    screen_putsxy(LEFT, y, what, COLOR_WHITE);
    put_right(column_right[0], y, number(count), COLOR_LIGHTGREEN);
    if (gold)
        screen_putsxy(column_right[2] - length(number(gold)), y, "-",
                      COLOR_RED);
    put_right(column_right[2], y, number(gold), COLOR_RED);
}

void finance_screen(void)
{
    byte c, i, y = TABLE_TOP + 2, key, n, temples = 0, mine = 0;
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
        ++mine;
        total += trade - corruption;
        if (city_culture[c] & CITY_TEMPLE) ++temples;
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
    expense(y + 1, "Temples' upkeep", temples, temples);
    n = over_army(PLAYER_HUMAN, mine);
    expense(y + 2, "Units beyond the free", n, ARMY_UPKEEP * n);
    expense(y + 3, "Cities beyond the free", mine > REALM_FREE
            ? mine - REALM_FREE : 0, realm_cost(mine));
    screen_putsxy(LEFT, y + 5, "Corruption grows with the distance from the "
                  "capital.", COLOR_LIGHTGREY);
    screen_putsxy(LEFT, y + 6, "Free: 4 units and 3 a city, and 10 cities.",
                  COLOR_LIGHTGREY);
    screen_putsxy(LEFT, y + 7, "Gold buys production: b on the city screen.",
                  COLOR_LIGHTGREY);
    screen_putsxy(0, EXIT_ROW, "Esc or Return to go on", COLOR_LIGHTGREY);
    screen_putsxy(EXIT_LEFT, EXIT_ROW, EXIT_TEXT, BUTTON_EXIT);

    do {
        screen_wait_raster(SCREEN_ROW_LINE(SCREEN_ROWS - 1));
        key = input_poll();
    } while (!key);
}
