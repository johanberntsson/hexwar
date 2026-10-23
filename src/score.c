// In region 5 (src/region.h): called through the stub in src/dispatch.s,
// under this name here.
#define score_screen score_screen_r5

#include "score.h"
#include "city.h"
#include "civ.h"
#include "combat.h"
#include "diplomacy.h"
#include "game.h"
#include "growth.h"
#include "input.h"
#include "map.h"
#include "picture.h"
#include "screen.h"
#include "trade.h"
#include "treasury.h"
#include "culture.h"
#include "unit.h"
#include "view.h"

#pragma clang section text="r5text" rodata="r5rodata"

#define BUTTON_EXIT (ATTR_REVERSE | COLOR_RED)
#define EXIT_ROW (SCREEN_ROWS - 1)
#define EXIT_TEXT " EXIT "
#define EXIT_LEFT (SCREEN_COLUMNS - 2 - (sizeof EXIT_TEXT - 1))

// The table: a row a people, a column a figure, right-aligned at these.
#define TABLE_TOP 6
#define PEOPLE_LEFT 8
#define FIGURES 7
static const char *const heading[FIGURES] = {
    "Cities", "Citizens", "Goods", "Deals", "Gold", "Legacy", "Score"
};
static const byte column_right[FIGURES] = { 27, 37, 44, 51, 58, 66, 74 };
#define OUTCOME_ROW (TABLE_TOP + 2 * PLAYERS + 3)

// The goods that count, as trade.h has them.
#define GOLD_POINT 200          // by turn 300 a treasury is 1000-3500

static const byte goods[] = {
    RES_OLIVES, RES_WINE, RES_CEDAR, RES_COPPER, RES_TIN, RES_DYE, RES_HORSES
};

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

// People p's figures, FIGURES of them, the score last.
static word figure[FIGURES];

static void figures(byte p)
{
    byte c, i;

    for (i = 0; i < FIGURES; ++i) figure[i] = 0;
    for (c = 0; c < city_count; ++c)
        if (city_owner[c] == p) {
            ++figure[0];
            figure[1] += city_size[c];
        }
    for (i = 0; i < sizeof goods; ++i)
        if (player_has(p, goods[i])) ++figure[2];
    for (i = 0; i < MAX_DEALS; ++i)
        if (deal_from[i] != DEAL_FREE
            && (deal_from[i] == p || deal_to[i] == p))
            ++figure[3];
    figure[4] = player_gold[p];
    figure[5] = player_legacy[p];
    // vision.md's wealth: a point for every GOLD_POINT in the treasury;
    // and its legacy, the temples and the wonders (culture.h).
    figure[6] = figure[1] + 2 * figure[0] + 2 * figure[2] + 3 * figure[3]
                + figure[4] / GOLD_POINT + figure[5];
}

static void put_year(byte x, byte y)
{
    screen_putsxy(x, y, number(game_year), COLOR_WHITE);
    screen_putsxy(x + length(number(game_year)) + 1, y, "BC", COLOR_WHITE);
}

// Whether the human's score is above every other people still standing.
static byte highest(void)
{
    byte p;
    word mine;

    figures(PLAYER_HUMAN);
    mine = figure[FIGURES - 1];
    for (p = 0; p < PLAYERS; ++p) {
        if (p == PLAYER_HUMAN || !player_alive(p)) continue;
        figures(p);
        if (figure[FIGURES - 1] >= mine) return 0;
    }
    return 1;
}

// **The legacy victory** (vision.md 5B): the human's legacy points above
// every other people's, fallen or not -- a people can win it
// posthumously.
static byte legacy_first(void)
{
    byte p;
    word mine = player_legacy[PLAYER_HUMAN];

    if (!mine) return 0;
    for (p = 0; p < PLAYERS; ++p)
        if (p != PLAYER_HUMAN && player_legacy[p] >= mine) return 0;
    return 1;
}

// Until a key or a click.
static void wait_key(void)
{
    byte key;
    do {
        screen_wait_raster(SCREEN_ROW_LINE(SCREEN_ROWS - 1));
        key = input_poll();
    } while (!key);
}

// **The end's picture** (src/picture.h), in the map window, with the
// panel cleared but for a line or two, before the table.
static void end_picture(byte outcome)
{
    screen_clear();
    view_init();
    picture_show(outcome);
    screen_putsxy(1, 10, "The end of the", COLOR_WHITE);
    screen_putsxy(1, 11, "Bronze Age", COLOR_WHITE);
    screen_putsxy(1, 13, "Press a key", COLOR_LIGHTGREY);
    wait_key();
    picture_end();
}

void score_screen(byte how)
{
    byte p, i, x, y, alive = player_alive(PLAYER_HUMAN), outcome;

    // Hegemony first, then legacy, which a fallen people can win; then
    // survival, or defeat. PICTURE_* numbers, which are the pictures.
    outcome = alive && highest() ? PICTURE_HEGEMONY
              : legacy_first() ? PICTURE_LEGACY
              : alive ? PICTURE_SURVIVAL : PICTURE_DEFEAT;
    sprite_hide(2);             // the minimap's outline, which is gone
    sprite_hide(3);
    if (how == SCORE_END) end_picture(outcome);
    screen_clear();
    screen_putsxy(PEOPLE_LEFT, 2,
                  how == SCORE_END ? "The end of the Bronze Age,"
                  : how == SCORE_RETIRED ? "Retired in"
                  : "The score in", COLOR_WHITE);
    put_year(how == SCORE_END ? PEOPLE_LEFT + 27
             : how == SCORE_RETIRED ? PEOPLE_LEFT + 11 : PEOPLE_LEFT + 13, 2);
    if (how == SCORE_NOW) {
        x = PEOPLE_LEFT;
        screen_putsxy(x, 3, "Turn", COLOR_LIGHTGREY);
        x += 5;
        screen_putsxy(x, 3, number(game_turn), COLOR_LIGHTGREY);
        x += length(number(game_turn));
        screen_putsxy(x + 1, 3, "of", COLOR_LIGHTGREY);
        screen_putsxy(x + 4, 3, number(GAME_LAST_TURN), COLOR_LIGHTGREY);
        if (game_turn < GAME_CRISIS_TURN)
            screen_putsxy(x + 8, 3, "-- the Crisis is due about 1200 BC",
                          COLOR_LIGHTGREY);
    }

    for (i = 0; i < FIGURES; ++i)
        put_right(column_right[i], TABLE_TOP, heading[i], COLOR_LIGHTGREY);
    for (p = 0; p < PLAYERS; ++p) {
        y = TABLE_TOP + 2 + 2 * p;
        screen_putsxy(PEOPLE_LEFT, y, player_people(p),
                      p == PLAYER_HUMAN ? COLOR_WHITE : COLOR_LIGHTGREEN);
        // During the game, only the peoples met; at the end, everybody.
        if (how != SCORE_END && p != PLAYER_HUMAN
            && !((diplomacy_known[PLAYER_HUMAN] >> p) & 1)) {
            screen_putsxy(column_right[0] - 5, y, "not yet met",
                          COLOR_LIGHTGREY);
            continue;
        }
        if (!player_alive(p)) {
            screen_putsxy(column_right[0] - 5, y, "fallen", COLOR_RED);
            continue;
        }
        figures(p);
        for (i = 0; i < FIGURES; ++i)
            put_right(column_right[i], y, number(figure[i]),
                      i == FIGURES - 1 ? COLOR_YELLOW : COLOR_LIGHTGREEN);
    }

    y = OUTCOME_ROW;
    if (how == SCORE_NOW) {
        screen_putsxy(PEOPLE_LEFT, y, "A point a citizen, two a city, two"
                      " a good, three a trade deal,", COLOR_LIGHTGREY);
        screen_putsxy(PEOPLE_LEFT, y + 1, "one for every 200 gold, and the"
                      " legacy of temples and wonders.", COLOR_LIGHTGREY);
    } else if (how == SCORE_RETIRED) {
        screen_putsxy(PEOPLE_LEFT, y, "You left before the end of the age.",
                      COLOR_WHITE);
    } else if (outcome == PICTURE_LEGACY) {
        screen_putsxy(PEOPLE_LEFT, y, alive ? "Your people's works outlast"
                      " the age:" : "Your people has fallen, but its works"
                      " outlast the age:", COLOR_YELLOW);
        screen_putsxy(PEOPLE_LEFT, y + 1, "a legacy victory.", COLOR_YELLOW);
    } else if (!alive) {
        screen_putsxy(PEOPLE_LEFT, y, "Your people has fallen: defeat.",
                      COLOR_RED);
    } else if (outcome == PICTURE_HEGEMONY) {
        screen_putsxy(PEOPLE_LEFT, y, "Your people came through the Collapse"
                      " with the highest score:", COLOR_YELLOW);
        screen_putsxy(PEOPLE_LEFT, y + 1, "a hegemony victory.",
                      COLOR_YELLOW);
    } else {
        screen_putsxy(PEOPLE_LEFT, y, "Your people came through the Collapse:"
                      " a survival victory.", COLOR_YELLOW);
    }
    screen_putsxy(0, EXIT_ROW, "Esc or Return to go on", COLOR_LIGHTGREY);
    screen_putsxy(EXIT_LEFT, EXIT_ROW, EXIT_TEXT, BUTTON_EXIT);
    wait_key();
}
