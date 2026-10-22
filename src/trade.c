// In region 4 (src/region.h): called through the stubs in src/dispatch.s,
// under these names here.
#define trade_reset trade_reset_r4
#define trade_turn trade_turn_r4
#define trade_agree trade_agree_r4
#define trade_propose trade_propose_r4

#include "trade.h"
#include "civ.h"
#include "combat.h"
#include "diplomacy.h"
#include "game.h"
#include "gameint.h"
#include "growth.h"
#include "input.h"
#include "map.h"
#include "screen.h"
#include "text.h"

#pragma clang section text="r4text" rodata="r4rodata"

HIGH_BSS byte deal_from[MAX_DEALS];
HIGH_BSS byte deal_to[MAX_DEALS];
HIGH_BSS byte deal_give[MAX_DEALS];
HIGH_BSS byte deal_back[MAX_DEALS];
HIGH_BSS word trade_got[PLAYERS];
HIGH_BSS byte trade_partners[PLAYERS];
HIGH_BSS byte trade_ask_get[PLAYERS];
HIGH_BSS byte trade_ask_give[PLAYERS];

// What can be traded: vision.md's luxury and strategic goods. The staples
// -- grain, fish, cattle -- feed the city that has them and go nowhere.
static const byte goods[] = {
    RES_OLIVES, RES_WINE, RES_CEDAR, RES_COPPER, RES_TIN, RES_DYE
};
#define GOODS (sizeof goods / sizeof goods[0])

#define MOOD_GIFT 15            // a gift, at once
#define MOOD_TRIBUTE 20         // a tribute refused, or a trade ended
#define TRIBUTE_MOOD (MOOD_WAR + 10) // colder than this, the strong demand

static void mood_down(byte p, byte q, byte d)
{
    byte m = diplomacy_mood[p][q];
    diplomacy_mood[p][q] = m > d ? (byte)(m - d) : 0;
}

static void mood_up(byte p, byte q, byte d)
{
    byte m = diplomacy_mood[p][q];
    diplomacy_mood[p][q] = m < MOOD_MAX - d ? (byte)(m + d) : MOOD_MAX;
}

// At peace, which a trade needs: not at war, and no ceasefire counting.
// The same test as diplomacy.c's relation, which is in another region.
static byte at_peace(byte p, byte q)
{
    return !at_war(p, q) && !diplomacy_truce[p][q];
}

static void recount(void)
{
    byte i, a, b;

    for (a = 0; a < PLAYERS; ++a) {
        trade_got[a] = 0;
        trade_partners[a] = 0;
    }
    for (i = 0; i < MAX_DEALS; ++i) {
        a = deal_from[i];
        if (a == DEAL_FREE) continue;
        b = deal_to[i];
        trade_got[b] |= (word)1 << deal_give[i];
        if (deal_back[i] != RES_NONE) trade_got[a] |= (word)1 << deal_back[i];
        trade_partners[a] |= (byte)(1 << b);
        trade_partners[b] |= (byte)(1 << a);
    }
}

void trade_reset(void)
{
    byte i;

    for (i = 0; i < MAX_DEALS; ++i) deal_from[i] = DEAL_FREE;
    for (i = 0; i < PLAYERS; ++i) {
        trade_ask_get[i] = RES_NONE;
        trade_ask_give[i] = RES_NONE;
    }
    recount();
}

static byte find_deal(byte p, byte q)
{
    byte i;
    for (i = 0; i < MAX_DEALS; ++i)
        if ((deal_from[i] == p && deal_to[i] == q)
            || (deal_from[i] == q && deal_to[i] == p))
            return i;
    return DEAL_FREE;
}

byte trade_agree(byte from, byte to, byte give, byte back)
{
    byte i;

    if (find_deal(from, to) != DEAL_FREE) return 0;
    for (i = 0; i < MAX_DEALS; ++i)
        if (deal_from[i] == DEAL_FREE) break;
    if (i == MAX_DEALS) return 0;
    deal_from[i] = from;
    deal_to[i] = to;
    deal_give[i] = give;
    deal_back[i] = back;
    if (back == RES_NONE) mood_up(to, from, MOOD_GIFT);
    recount();
    return 1;
}

// How much p wants resource r: nothing if it has it already; copper or tin
// most, above all the one that makes bronze with what it has.
static byte want(byte p, byte r)
{
    if (player_has(p, r)) return 0;
    if (r == RES_COPPER) return player_has(p, RES_TIN) ? 3 : 2;
    if (r == RES_TIN) return player_has(p, RES_COPPER) ? 3 : 2;
    return 1;
}

static byte strategic(byte r)
{
    return r == RES_COPPER || r == RES_TIN;
}

// The good `from` owns that `to` wants most, RES_NONE if none.
static byte best(byte from, byte to)
{
    byte i, r, w, top = 0, pick = RES_NONE;

    for (i = 0; i < GOODS; ++i) {
        r = goods[i];
        w = want(to, r);
        if (w > top && player_owns(from, r)) {
            top = w;
            pick = r;
        }
    }
    return pick;
}

// Whether q, a computer player, sends `out` to p for `in` (RES_NONE: a
// tribute). Tribute goes to the stronger; a trade to those it likes well
// enough, and copper or tin only for copper or tin, or to friends.
static byte accepts(byte q, byte p, byte out, byte in)
{
    byte m = diplomacy_mood[q][p];

    if (in == RES_NONE) return diplomacy_power[p] > diplomacy_power[q];
    if (m < MOOD_ACCEPT || !want(q, in)) return 0;
    return !strategic(out) || strategic(in) || m >= MOOD_PEACE;
}

void trade_turn(void)
{
    byte i, p, q, get, give, ended = 0;

    // A trade lasts as long as the peace -- and in the Crisis, one turn in
    // 32, the ships are lost: vision.md's network fracture.
    for (i = 0; i < MAX_DEALS; ++i) {
        p = deal_from[i];
        if (p == DEAL_FREE) continue;
        q = deal_to[i];
        if (game_turn >= GAME_CRISIS_TURN && !(rnd() & 31)) {
            if (p == PLAYER_HUMAN || q == PLAYER_HUMAN)
                message("Trade ships", "lost at sea");
        } else if (at_peace(p, q) && player_alive(p) && player_alive(q)) {
            continue;
        }
        deal_from[i] = DEAL_FREE;
        ended = 1;
    }
    if (ended) recount();

    // What each computer player asks for, from each player it is at peace
    // with and does not trade with yet, one turn in eight, staggered.
    for (p = PLAYER_FIRST_AI; p < PLAYERS; ++p) {
        if (!player_alive(p)) continue;
        for (q = 0; q < PLAYERS; ++q) {
            if (q == p || ((game_turn + p + 2 * q) & 7)
                || !((diplomacy_known[p] >> q) & 1) || !player_alive(q)
                || !at_peace(p, q)
                || find_deal(p, q) != DEAL_FREE)
                continue;
            get = best(q, p);
            if (get == RES_NONE) continue;
            give = best(p, q);
            if (give == RES_NONE
                && (diplomacy_mood[p][q] >= TRIBUTE_MOOD
                    || diplomacy_power[p] <= diplomacy_power[q]))
                continue;
            if (HUMAN_ASKED(q)) {
                trade_ask_get[p] = get;
                trade_ask_give[p] = give;
                diplomacy_letter[p] =
                    give == RES_NONE ? LETTER_TRIBUTE : LETTER_TRADE;
                diplomacy_waiting = 1;
            } else if (accepts(q, p, get, give)) {
                trade_agree(q, p, get, give);
            } else {
                mood_down(p, q, give == RES_NONE ? MOOD_TRIBUTE : MOOD_REFUSED);
            }
        }
    }
}

// ------------------------------------------------- the Diplomacy screen

static byte x;

static void put(byte row, const char *s, byte colour)
{
    screen_putsxy(x, row, s, colour);
    while (*s++) ++x;
}

static void clear_row(byte row)
{
    screen_putsxy(0, row, "                                        "
                  "                                       ", COLOR_WHITE);
    x = 0;
}

static byte wait_key(void)
{
    byte key;
    do {
        screen_wait_raster(SCREEN_ROW_LINE(SCREEN_ROWS - 1));
        key = input_poll();
    } while (!key);
    if (key >= 'A' && key <= 'Z') key += 'a' - 'A';
    return key;
}

// The goods `from` owns (and, if `to` is not PLAYERS, `to` has not got),
// listed on `row` after `what` with the keys 1 up, then a key: the good
// picked, RES_NONE for 0 when `gift` says what 0 is, or DEAL_FREE for
// anything else.
static byte choose(byte row, const char *what, byte from, byte to,
                   const char *gift)
{
    byte list[GOODS], n = 0, i, r, key;
    char k[3];

    clear_row(row);
    put(row, what, COLOR_YELLOW);
    for (i = 0; i < GOODS; ++i) {
        r = goods[i];
        if (!player_owns(from, r) || (to != PLAYERS && player_has(to, r)))
            continue;
        list[n++] = r;
        k[0] = (char)('0' + n);
        k[1] = ' ';
        k[2] = 0;
        put(row, k, COLOR_WHITE);
        put(row, text_line(TEXT_LIST_RESOURCES, r), COLOR_LIGHTGREEN);
        put(row, "  ", COLOR_WHITE);
    }
    if (gift) {
        put(row, "0 ", COLOR_WHITE);
        put(row, gift, COLOR_LIGHTGREEN);
    } else if (!n) {
        put(row, "none", COLOR_LIGHTGREY);
    }
    key = wait_key();
    if (key == '0' && gift) return RES_NONE;
    if (key > '0' && key <= (byte)('0' + n)) return list[key - '1'];
    return DEAL_FREE;
}

byte trade_propose(byte p, byte row)
{
    byte give, get, d = find_deal(PLAYER_HUMAN, p);

    if (d != DEAL_FREE) {
        clear_row(row);
        put(row, "End the trade with the ", COLOR_RED);
        put(row, player_people(p), COLOR_RED);
        put(row, "? Y or N", COLOR_RED);
        if (wait_key() != 'y') return 0;
        deal_from[d] = DEAL_FREE;
        recount();
        mood_down(p, PLAYER_HUMAN, MOOD_TRIBUTE);
        return 1;
    }
    give = choose(row, "Send which good?  ", PLAYER_HUMAN, p, 0);
    if (give == DEAL_FREE || give == RES_NONE) return 0;
    get = choose(row, "For which?  ", p, PLAYER_HUMAN, "nothing, a gift");
    if (get == DEAL_FREE) return 0;
    if (get == RES_NONE || accepts(p, PLAYER_HUMAN, get, give)) {
        trade_agree(PLAYER_HUMAN, p, give, get);
        diplomacy_letter[p] = LETTER_YES;
    } else {
        mood_down(p, PLAYER_HUMAN, MOOD_REFUSED);
        diplomacy_letter[p] = LETTER_NO;
    }
    diplomacy_waiting = 1;
    return 1;
}
