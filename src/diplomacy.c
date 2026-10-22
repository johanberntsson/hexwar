// In region 3 (src/region.h): called through the stubs in src/dispatch.s,
// under these names here.
#define diplomacy_reset diplomacy_reset_r3
#define diplomacy_contact diplomacy_contact_r3
#define diplomacy_turn diplomacy_turn_r3
#define diplomacy_greet diplomacy_greet_r3
#define diplomacy_screen diplomacy_screen_r3
#define declare_war declare_war_r3

#include "diplomacy.h"
#include "city.h"
#include "civ.h"
#include "combat.h"
#include "game.h"
#include "growth.h"
#include "input.h"
#include "map.h"
#include "popup.h"
#include "screen.h"
#include "text.h"
#include "trade.h"
#include "unit.h"

#pragma clang section text="r3text" rodata="r3rodata"

HIGH_BSS byte diplomacy_known[PLAYERS];
HIGH_BSS byte diplomacy_truce[PLAYERS][PLAYERS];
HIGH_BSS byte diplomacy_mood[PLAYERS][PLAYERS];
byte diplomacy_ax, diplomacy_ay;
byte diplomacy_waiting;
#ifdef DEBUG_MENU
byte diplomacy_auto;
#endif

// What each computer player has to say to the human when the map is next
// up, LETTER_* (diplomacy.h). Not saved: a game is saved from the map,
// after they have been read.
HIGH_BSS byte diplomacy_letter[PLAYERS];
#define letter diplomacy_letter
// The texts are TEXT_LETTER_GREET and the ones after it, in this order.
#define LETTER_TEXT(K) (TEXT_LETTER_GREET + (K))

// **The attitudes**, after Civilization's: how player p feels about q, a
// byte with MOOD_NEUTRAL in the middle (Calypsi and signed bytes do not
// mix; CLAUDE.md). Each people has a temper -- the base it drifts back
// to, one point a turn -- and at war it drifts lower and at peace higher.
// A declaration of war costs the declarer 40 with its victim and 10 with
// everybody else who knows it, as Civilization's Vendetta and reputation
// do; units next to somebody's city cost 2 a turn with them, and each
// pair of cities of two peoples within RIVAL_RANGE of each other 1 a turn
// both ways -- crowding, which is what sets Civilization's neighbours
// against each other; a refused offer costs 10. The drift back is one
// point every other turn, so one pair of rival cities wins over it. **The higher levels make everybody colder to the human**
// (game_level, which nothing read until now), as Civilization's levels
// make the computer more aggressive.
// MOOD_NEUTRAL, MOOD_MAX, MOOD_WAR, MOOD_PEACE, MOOD_ACCEPT and
// MOOD_REFUSED are in diplomacy.h, for trade.c.
#define MOOD_PEACE_BONUS 5
#define MOOD_TRADE_BONUS 10     // partners in a trade (trade.h)
#define RIVAL_RANGE 7
#define MOOD_WAR_MALUS 15
#define MOOD_VICTIM 40
#define MOOD_WITNESS 10
#define MOOD_BORDER 2
#define MOOD_LEVEL 5            // colder to the human, a level
#define MOOD_TRIBUTE_REFUSED 20

// Mycenaean raiders and the Hittite great king are quick to quarrel, the
// Canaanite merchants slow (vision.md section 3), by CIV_*.
static const byte temper[CIVS] = { 90, 85, 100, 110 };

#define CEASEFIRE_TURNS 16      // Civilization II's, then war again
#define WAR_FIRST_TURN 40       // nobody starts a war earlier
#define WAR_TIRED 10            // turns of war before a ceasefire is asked
#define WAR_WEARY 30            // ... by the stronger side as well
#define TRUCE_MAX 255

static byte base(byte p, byte q)
{
    byte b = temper[player_civ[p]];
    if (q == PLAYER_HUMAN) b -= (byte)(game_level * MOOD_LEVEL);
    return b;
}

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

void diplomacy_reset(void)
{
    byte p, q;

    for (p = 0; p < PLAYERS; ++p) {
        diplomacy_known[p] = 0;
        letter[p] = LETTER_NONE;
        for (q = 0; q < PLAYERS; ++q) {
            diplomacy_truce[p][q] = 0;
            diplomacy_mood[p][q] = base(p, q);
        }
    }
    diplomacy_waiting = 0;
    trade_reset();
}

// ---------------------------------------------------------------- relations

byte diplomacy_relation(byte p, byte q)
{
    if (at_war(p, q)) return RELATION_WAR;
    return diplomacy_truce[p][q] ? RELATION_CEASEFIRE : RELATION_PEACE;
}

static void set_truce(byte p, byte q, byte t)
{
    diplomacy_truce[p][q] = t;
    diplomacy_truce[q][p] = t;
}

static void make_peace(byte p, byte q, byte truce)
{
    combat_war[p] &= (byte)~(1 << q);
    combat_war[q] &= (byte)~(1 << p);
    set_truce(p, q, truce);
}

static void say(byte p, byte kind)
{
    letter[p] = kind;
    diplomacy_waiting = 1;
}


void declare_war(byte a, byte b)
{
    byte r;

    combat_war[a] |= (byte)(1 << b);
    combat_war[b] |= (byte)(1 << a);
    set_truce(a, b, 0);
    diplomacy_known[a] |= (byte)(1 << b);
    diplomacy_known[b] |= (byte)(1 << a);
    mood_down(b, a, MOOD_VICTIM);
    for (r = 0; r < PLAYERS; ++r)
        if (r != a && r != b && ((diplomacy_known[r] >> a) & 1))
            mood_down(r, a, MOOD_WITNESS);
    if (HUMAN_ASKED(b)) say(a, LETTER_WAR);
}

// Each player's strength, Civilization's military power: the attack and
// defence of all its units. Worked out once a turn by diplomacy_turn().
HIGH_BSS word diplomacy_power[PLAYERS];
#define power diplomacy_power

static void count_power(void)
{
    byte i, p, k;

    for (p = 0; p < PLAYERS; ++p) power[p] = 0;
    for (i = 0; i < MAX_UNITS; ++i) {
        k = unit_kind[i];
        p = unit_owner[i];
        if (k != KIND_NONE && p < PLAYERS)
            power[p] += unit_kind_attack[k] + unit_kind_defence[k];
    }
}

// Whether p would go to war with q: it dislikes q and is at least as
// strong (Civilization's test of who is strongest, without the
// continents).
static byte wants_war(byte p, byte q)
{
    return diplomacy_mood[p][q] < MOOD_WAR && power[p] >= power[q];
}

// Hexagons between two cells, round the east-west wrap; ai.c's.
static word distance(byte ax, byte ay, byte bx, byte by)
{
    int dx = (int)ax - (int)bx;
    int dy = (2 * (int)ay + (ax & 1)) - (2 * (int)by + (bx & 1));
    if (dx < 0) dx = -dx;
    if (dx > MAP_COLUMNS / 2) dx = MAP_COLUMNS - dx;
    if (dy < 0) dy = -dy;
    return (word)(dy + (dx > dy ? (dx - dy) / 2 : 0));
}

// Every pair of cities of two peoples who have met, near each other.
static void rivals(void)
{
    byte i, j, p, q;

    for (i = 0; i < city_count; ++i) {
        p = city_owner[i];
        if (p >= PLAYERS) continue;             // the rebels' (crisis.c)
        for (j = (byte)(i + 1); j < city_count; ++j) {
            q = city_owner[j];
            if (p == q || q >= PLAYERS || !((diplomacy_known[p] >> q) & 1)
                || distance(city_ax[i], city_ay[i], city_ax[j], city_ay[j])
                       > RIVAL_RANGE)
                continue;
            mood_down(p, q, 1);
            mood_down(q, p, 1);
        }
    }
}

static byte accepts(byte q, byte p, byte kind)
{
    if (wants_war(q, p)) return 0;
    return kind == LETTER_CEASEFIRE || diplomacy_mood[q][p] >= MOOD_ACCEPT;
}

// p offers q a ceasefire or peace (LETTER_CEASEFIRE or LETTER_PEACE). The
// human is asked by letter when the map is next up; a computer player
// answers at once.
static void agreed(byte p, byte q, byte kind)
{
    make_peace(p, q, kind == LETTER_CEASEFIRE ? CEASEFIRE_TURNS : 0);
}

static void offer(byte p, byte q, byte kind)
{
    if (HUMAN_ASKED(q)) {
        say(p, kind);
    } else if (accepts(q, p, kind)) {
        agreed(p, q, kind);
    } else {
        mood_down(p, q, MOOD_REFUSED);
    }
}

// ------------------------------------------------------------------ contact

// **Contact is next to each other, as in Civilization**: a people is met
// when one of its units or cities is on a hexagon next to one of the
// other's units or cities -- not, as at first, anywhere the player had
// explored, which greeted peoples whose warrior was four hexagons off and
// looked like nobody at all. So this looks round everybody's units and
// cities, a hundred or so hexagons, and only at those with MAP_OCCUPIED
// set. A hexagon holds one owner's units and cities (moving onto
// another's is an attack), so the city, or else the top unit, says whose
// it is. The human's newly met wait in `newly` for diplomacy_contact().
static byte newly;
static byte border;             // diplomacy_turn(): count units by cities

static void look(byte me, byte my_city, byte ax, byte ay)
{
    byte c, p;

    if (!(map_tile[ay][ax] & MAP_OCCUPIED)) return;
    c = city_at(ax, ay);
    p = c != CITY_NONE ? city_owner[c] : unit_owner[unit_at(ax, ay)];
    if (p == me || p == PLAYER_BARBARIAN) return;
    // Somebody's units by my city, in peacetime.
    if (border && my_city && c == CITY_NONE && !at_war(me, p))
        mood_down(me, p, MOOD_BORDER);
    if ((diplomacy_known[me] >> p) & 1) return;
    diplomacy_known[me] |= (byte)(1 << p);
    diplomacy_known[p] |= (byte)(1 << me);
    if (me == PLAYER_HUMAN || p == PLAYER_HUMAN) {
        newly |= (byte)(1 << (me == PLAYER_HUMAN ? p : me));
        diplomacy_ax = ax;
        diplomacy_ay = ay;
    }
}

static void look_round(byte me, byte my_city, byte ax, byte ay)
{
    byte dir, nx, ny;
    for (dir = 0; dir < 6; ++dir)
        if (map_neighbour(ax, ay, dir, &nx, &ny)) look(me, my_city, nx, ny);
}

// Everybody's units and cities, or only the human's.
static void scan(byte all)
{
    byte i, p;

    for (i = 0; i < MAX_UNITS; ++i) {
        p = unit_owner[i];
        if (unit_kind[i] != KIND_NONE && p < PLAYERS
            && (all || p == PLAYER_HUMAN))
            look_round(p, 0, unit_ax[i], unit_ay[i]);
    }
    for (i = 0; i < city_count; ++i) {
        p = city_owner[i];
        if (p < PLAYERS && (all || p == PLAYER_HUMAN))
            look_round(p, 1, city_ax[i], city_ay[i]);
    }
}

byte diplomacy_contact(void)
{
    byte met;

    border = 0;
    scan(0);
    met = newly;
    newly = 0;
    return met;
}

// --------------------------------------------------------------- the turn

// Once a turn, after the computer players have moved: who has met whom,
// the attitudes, the ceasefires running out, and what each computer
// player wants. **Civilization's way** (OpenCivOne's Diplomacy.cs): a
// pair is thought about one turn in four, staggered; a player that
// dislikes another and is half as strong again declares war; one tired of
// a war it is losing, or of any war long enough, asks for a ceasefire,
// which is refused only by a side that would declare war itself; and
// from a ceasefire, which runs out into war after CEASEFIRE_TURNS, one
// that has come to like the other offers peace. Peace between the
// computer players needs no embassy here and is not announced.
void diplomacy_turn(void)
{
    byte p, q, t, aim, rel;

    count_power();
    border = 1;
    scan(1);
    border = 0;
    rivals();

    for (p = 0; p < PLAYERS; ++p) {
        if (!player_alive(p)) continue;
        for (q = 0; q < PLAYERS; ++q) {
            if (q == p || !((diplomacy_known[p] >> q) & 1)
                || !player_alive(q))
                continue;
            rel = diplomacy_relation(p, q);

            // The attitude drifts back towards p's temper.
            aim = base(p, q);
            if (rel == RELATION_WAR) aim -= MOOD_WAR_MALUS;
            if (rel == RELATION_PEACE) aim += MOOD_PEACE_BONUS;
            if ((trade_partners[p] >> q) & 1) aim += MOOD_TRADE_BONUS;
            if (game_turn & 1) {
                if (diplomacy_mood[p][q] < aim) mood_up(p, q, 1);
                else if (diplomacy_mood[p][q] > aim) mood_down(p, q, 1);
            }

            // Each pair's clock once, by the lower player.
            t = diplomacy_truce[p][q];
            if (p < q) {
                if (rel == RELATION_WAR) {
                    if (t < TRUCE_MAX) set_truce(p, q, (byte)(t + 1));
                } else if (rel == RELATION_CEASEFIRE && !--t) {
                    combat_war[p] |= (byte)(1 << q);
                    combat_war[q] |= (byte)(1 << p);
                    set_truce(p, q, 0);
                    if (HUMAN_ASKED(p)) say(q, LETTER_TRUCE_OVER);
                    continue;
                } else if (rel == RELATION_CEASEFIRE) {
                    set_truce(p, q, t);
                }
            }

            // What p wants, one turn in four. The human decides for itself.
            if (p == PLAYER_HUMAN || ((game_turn + p + q) & 3)) continue;
            t = diplomacy_truce[p][q];
            if (rel == RELATION_WAR) {
                if (t >= WAR_WEARY || (t >= WAR_TIRED && power[p] < power[q]))
                    offer(p, q, LETTER_CEASEFIRE);
            } else if (rel == RELATION_CEASEFIRE) {
                if (diplomacy_mood[p][q] >= MOOD_PEACE)
                    offer(p, q, LETTER_PEACE);
            } else if (game_turn >= WAR_FIRST_TURN && wants_war(p, q)) {
                declare_war(p, q);
            }
        }
    }
    trade_turn();
}

// ------------------------------------------------------------- the letters

// The names a letter from p to the human is written with, copied out of
// text_line()'s two buffers, which would not hold them all: the two
// rulers, their titles, p's people, and for a trade the goods asked of the
// human and offered for it (trade.h), small in the middle of a sentence.
#define NAME_MAX 24
#define NAMES 7
HIGH_BSS static char names[NAMES][NAME_MAX];

static void copy_name(byte i, const char *s)
{
    byte n = 0;
    while (*s && n < NAME_MAX - 1) names[i][n++] = *s++;
    names[i][n] = 0;
    if (i >= 5 && names[i][0] >= 'A' && names[i][0] <= 'Z')
        names[i][0] += 'a' - 'A';
}

// Letter text `id` from p to the human, in a popup; for an offer, Yes or
// No, and otherwise 0.
static byte write_letter(byte p, byte id, byte ask)
{
    const char *args[NAMES];
    byte i, me = player_civ[PLAYER_HUMAN], civ = player_civ[p];

    copy_name(0, civ_leader(me));
    copy_name(1, civ_title(me));
    copy_name(2, civ_leader(civ));
    copy_name(3, civ_title(civ));
    copy_name(4, player_people(p));
    copy_name(5, text_line(TEXT_LIST_RESOURCES, trade_ask_get[p]));
    copy_name(6, text_line(TEXT_LIST_RESOURCES, trade_ask_give[p]));
    for (i = 0; i < NAMES; ++i) args[i] = names[i];
    popup_begin();
    popup_add_text(id, args);
    if (ask) return popup_ask_shown();
    popup_show();
    return 0;
}

void diplomacy_greet(byte met)
{
    byte p, kind, rel;

    for (p = PLAYER_FIRST_AI; p < PLAYERS; ++p)
        if ((met >> p) & 1) write_letter(p, TEXT_LETTER_GREET, 0);

    for (p = PLAYER_FIRST_AI; p < PLAYERS; ++p) {
        kind = letter[p];
        letter[p] = LETTER_NONE;
        if (kind == LETTER_NONE || !player_alive(p)) continue;
        rel = diplomacy_relation(PLAYER_HUMAN, p);
        // An offer that things have overtaken is not made.
        if ((kind == LETTER_CEASEFIRE && rel != RELATION_WAR)
            || (kind == LETTER_PEACE && rel != RELATION_CEASEFIRE))
            continue;
        if (kind == LETTER_TRADE || kind == LETTER_TRIBUTE) {
            // Not if the peace or the good has gone since, or a trade has
            // been made.
            if (rel != RELATION_PEACE
                || ((trade_partners[p] >> PLAYER_HUMAN) & 1)
                || !player_owns(PLAYER_HUMAN, trade_ask_get[p]))
                continue;
            if (write_letter(p, LETTER_TEXT(kind), 1))
                trade_agree(PLAYER_HUMAN, p, trade_ask_get[p],
                            trade_ask_give[p]);
            else
                mood_down(p, PLAYER_HUMAN, kind == LETTER_TRIBUTE
                                               ? MOOD_TRIBUTE_REFUSED
                                               : MOOD_REFUSED);
        } else if (kind == LETTER_CEASEFIRE || kind == LETTER_PEACE) {
            if (write_letter(p, LETTER_TEXT(kind), 1))
                agreed(p, PLAYER_HUMAN, kind);
            else
                mood_down(p, PLAYER_HUMAN, MOOD_REFUSED);
        } else {
            write_letter(p, LETTER_TEXT(kind), 0);
        }
    }
    diplomacy_waiting = 0;
}

// ------------------------------------------------------ the Diplomacy screen

#define TITLE "Diplomacy"
#define BUTTON_EXIT (ATTR_REVERSE | COLOR_RED)
#define EXIT_ROW (SCREEN_ROWS - 1)
#define EXIT_TEXT " EXIT "
#define EXIT_LEFT (SCREEN_COLUMNS - 2 - (sizeof EXIT_TEXT - 1))
#define LIST_LEFT 8
#define LIST_TOP 3
#define BLOCK_ROWS 5            // rows a people takes
#define STATE_LEFT 56           // "at war", "at peace", "not yet met"

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

static const char *const mood_word[] = {
    "hostile", "cold", "wary", "cordial", "friendly"
};

// p's attitude to the human, in a word.
static const char *attitude(byte p)
{
    byte m = diplomacy_mood[p][PLAYER_HUMAN], i = 0;
    if (m >= MOOD_WAR - 10) ++i;
    if (m >= MOOD_WAR) ++i;
    if (m >= MOOD_PEACE) ++i;
    if (m >= MOOD_NEUTRAL + 10) ++i;
    return mood_word[i];
}

// What the human and p send each other, on row y.
static void draw_trade(byte p, byte y)
{
    byte i, x = LIST_LEFT + 2, from, give, back;

    for (i = 0; i < MAX_DEALS; ++i) {
        from = deal_from[i];
        if (from == DEAL_FREE) continue;
        if (from == PLAYER_HUMAN && deal_to[i] == p) {
            give = deal_give[i];
            back = deal_back[i];
        } else if (from == p && deal_to[i] == PLAYER_HUMAN) {
            give = deal_back[i];
            back = deal_give[i];
        } else {
            continue;
        }
        put(&x, y, "Trade: you send ", COLOR_LIGHTGREY);
        put(&x, y, give == RES_NONE ? "nothing"
                   : text_line(TEXT_LIST_RESOURCES, give), COLOR_LIGHTGREEN);
        put(&x, y, ", they send ", COLOR_LIGHTGREY);
        put(&x, y, back == RES_NONE ? "nothing"
                   : text_line(TEXT_LIST_RESOURCES, back), COLOR_LIGHTGREEN);
        return;
    }
}

// Player p's block: the people and how things stand, the ruler, the
// counts, p's attitude and its wars with the others the human knows.
static void draw_player(byte p, byte y, byte selected)
{
    byte x = LIST_LEFT, civ = player_civ[p], q, rel;

    screen_putsxy(LIST_LEFT - 2, y, selected ? ">" : " ", COLOR_YELLOW);
    put(&x, y, player_people(p), COLOR_WHITE);
    if (p == PLAYER_HUMAN) {
        put(&x, y, " (you)", COLOR_LIGHTGREY);
    } else if (!((diplomacy_known[PLAYER_HUMAN] >> p) & 1)) {
        screen_putsxy(STATE_LEFT, y, "not yet met", COLOR_LIGHTGREY);
        return;
    } else if (!player_alive(p)) {
        screen_putsxy(STATE_LEFT, y, "no more", COLOR_LIGHTGREY);
        return;
    } else {
        rel = diplomacy_relation(PLAYER_HUMAN, p);
        x = STATE_LEFT;
        if (rel == RELATION_WAR) {
            put(&x, y, "at war", COLOR_RED);
        } else if (rel == RELATION_PEACE) {
            put(&x, y, "at peace", COLOR_LIGHTGREEN);
        } else {
            put(&x, y, "ceasefire ", COLOR_YELLOW);
            put(&x, y, number(diplomacy_truce[PLAYER_HUMAN][p]), COLOR_YELLOW);
        }
    }
    x = LIST_LEFT + 2;
    put(&x, y + 1, civ_leader(civ), COLOR_YELLOW);
    put(&x, y + 1, ", ", COLOR_LIGHTGREY);
    put(&x, y + 1, civ_title(civ), COLOR_LIGHTGREY);
    draw_counts(p, y + 2);
    if (p == PLAYER_HUMAN) return;
    x = LIST_LEFT + 2;
    put(&x, y + 3, "Attitude ", COLOR_LIGHTGREY);
    put(&x, y + 3, attitude(p), COLOR_LIGHTGREEN);
    for (q = PLAYER_FIRST_AI; q < PLAYERS; ++q) {
        if (q == p || !at_war(p, q) || !player_alive(q)
            || !((diplomacy_known[PLAYER_HUMAN] >> q) & 1))
            continue;
        x += 3;
        put(&x, y + 3, "at war with ", COLOR_LIGHTGREY);
        put(&x, y + 3, player_people(q), COLOR_RED);
    }
    draw_trade(p, y + 4);
}

// Whether the human can write to p: met, and still in the game.
static byte can_write(byte p)
{
    return ((diplomacy_known[PLAYER_HUMAN] >> p) & 1) && player_alive(p);
}

#define ACTION_ROW (EXIT_ROW - 1)

// The proposals the human can make to p, on the row above EXIT.
static void draw_actions(byte p)
{
    byte x = 0, rel;

    screen_putsxy(0, ACTION_ROW, "                                        "
                  "                                       ", COLOR_WHITE);
    if (p == PLAYER_HUMAN) {
        put(&x, ACTION_ROW, "Up and down choose a people", COLOR_LIGHTGREY);
        return;
    }
    rel = diplomacy_relation(PLAYER_HUMAN, p);
    if (rel == RELATION_WAR)
        put(&x, ACTION_ROW, "C propose a ceasefire   ", COLOR_WHITE);
    if (rel == RELATION_CEASEFIRE)
        put(&x, ACTION_ROW, "P propose peace   ", COLOR_WHITE);
    if (rel != RELATION_WAR)
        put(&x, ACTION_ROW, "W declare war   ", COLOR_WHITE);
    if (rel == RELATION_PEACE)
        put(&x, ACTION_ROW, (trade_partners[p] >> PLAYER_HUMAN) & 1
                            ? "T end the trade" : "T trade", COLOR_WHITE);
}

static void draw_list(byte selected)
{
    byte p;
    for (p = 0; p < PLAYERS; ++p)
        draw_player(p, LIST_TOP + BLOCK_ROWS * p, p == selected);
    draw_actions(selected);
}

// The next player the human can write to from `p` in direction `step`
// (1 or PLAYERS - 1), or the human if there is none.
static byte next_people(byte p, byte step)
{
    byte i;
    for (i = 1; i < PLAYERS; ++i) {
        p = (byte)((p + step) % PLAYERS);
        if (p != PLAYER_HUMAN && can_write(p)) return p;
    }
    return PLAYER_HUMAN;
}

// The human proposes LETTER_CEASEFIRE or LETTER_PEACE to p, and p's answer
// is a letter, read when the map is back.
static void propose(byte p, byte kind)
{
    if (accepts(p, PLAYER_HUMAN, kind)) {
        agreed(PLAYER_HUMAN, p, kind);
        say(p, LETTER_YES);
    } else {
        mood_down(p, PLAYER_HUMAN, MOOD_REFUSED);
        say(p, LETTER_NO);
    }
}

void diplomacy_screen(void)
{
    byte key, p, sel = PLAYER_HUMAN, rel, y;

    sprite_hide(2);             // the minimap's outline, which is gone
    sprite_hide(3);
    screen_clear();
    screen_putsxy((SCREEN_COLUMNS - (sizeof TITLE - 1)) / 2, 1, TITLE,
                  COLOR_WHITE);
    sel = next_people(PLAYER_HUMAN, 1);
    screen_putsxy(0, EXIT_ROW, "Esc or Return to go back", COLOR_LIGHTGREY);
    screen_putsxy(EXIT_LEFT, EXIT_ROW, EXIT_TEXT, BUTTON_EXIT);
    draw_list(sel);

    for (;;) {
        screen_wait_raster(SCREEN_ROW_LINE(SCREEN_ROWS - 1));
        key = input_poll();
        if (key >= 'A' && key <= 'Z') key += 'a' - 'A';
        if (key == KEY_ESC || key == KEY_RUNSTOP || key == KEY_RETURN
            || key == KEY_SPACE)
            return;
        p = sel;
        if (key == KEY_CLICK) {
            // A people's block picks it, and anywhere else goes back.
            y = (byte)(input_y >> 3);
            if (y < LIST_TOP || y >= LIST_TOP + BLOCK_ROWS * PLAYERS)
                return;
            y = (byte)((y - LIST_TOP) / BLOCK_ROWS);
            if (y != PLAYER_HUMAN && can_write(y)) p = y;
        } else if (key == KEY_DOWN) {
            p = next_people(sel, 1);
        } else if (key == KEY_UP) {
            p = next_people(sel, PLAYERS - 1);
        }
        if (p != sel) {
            sel = p;
            draw_list(sel);
            continue;
        }
        if (sel == PLAYER_HUMAN) continue;
        rel = diplomacy_relation(PLAYER_HUMAN, sel);
        if (key == 'c' && rel == RELATION_WAR) {
            propose(sel, LETTER_CEASEFIRE);
            return;
        }
        if (key == 'p' && rel == RELATION_CEASEFIRE) {
            propose(sel, LETTER_PEACE);
            return;
        }
        if (key == 't' && rel == RELATION_PEACE) {
            if (trade_propose(sel, ACTION_ROW)) return;
            draw_actions(sel);
            continue;
        }
        if (key == 'w' && rel != RELATION_WAR) {
            y = 0;
            put(&y, ACTION_ROW, "Declare war on the ", COLOR_RED);
            put(&y, ACTION_ROW, player_people(sel), COLOR_RED);
            put(&y, ACTION_ROW, "? Y or N            ", COLOR_RED);
            do {
                screen_wait_raster(SCREEN_ROW_LINE(SCREEN_ROWS - 1));
                key = input_poll();
            } while (!key);
            if (key == 'y' || key == 'Y') {
                declare_war(PLAYER_HUMAN, sel);
                return;
            }
            draw_actions(sel);
        }
    }
}
