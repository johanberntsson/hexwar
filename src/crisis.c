// In region 5 (src/region.h): called through the stub in src/dispatch.s,
// under this name here.
#define crisis_turn crisis_turn_r5

#include "crisis.h"
#include "city.h"
#include "civ.h"
#include "combat.h"
#include "diplomacy.h"
#include "game.h"
#include "map.h"
#include "popup.h"
#include "sheet.h"
#include "text.h"
#include "trade.h"

#pragma clang section text="r5text" rodata="r5rodata"

// Nothing to tell during Debug/Play 50 Turns; without the Debug menu there
// is none. Not in an &&: see the Calypsi notes in CLAUDE.md.
#ifdef DEBUG_MENU
#define QUIET diplomacy_auto
#else
#define QUIET 0
#endif

HIGH_BSS byte crisis_drought;
HIGH_BSS byte crisis_cities[PLAYERS];
HIGH_BSS byte crisis_capitals;

#define RUMOUR_TURN 200
#define WARNING_TURN 215
#define DROUGHT_TURNS 8         // and up to 7 more
#define SEA_CAP 20              // barbarian units, the Sea Peoples' limit
#define LANDING_NEAR 3          // a fleet appears this far from its city
#define LANDING_FAR 6           // ... and no further

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

// ----------------------------------------------------------------- letters

// The names a letter needs, as diplomacy.c's letters have them: the
// human's ruler and title, the writer's, and the writer's people.
#define NAMES 5
#define NAME_MAX 24
HIGH_BSS static char names[NAMES][NAME_MAX];

static void copy_name(byte i, const char *s)
{
    byte n = 0;
    while (*s && n < NAME_MAX - 1) names[i][n++] = *s++;
    names[i][n] = 0;
}

// Text `id` in a popup, from p if it is a letter (PLAYERS for none).
static void letter(byte id, byte p)
{
    const char *args[NAMES];
    byte i, me = player_civ[PLAYER_HUMAN], civ;

    if (QUIET) return;
    if (p < PLAYERS) {
        civ = player_civ[p];
        copy_name(0, civ_leader(me));
        copy_name(1, civ_title(me));
        copy_name(2, civ_leader(civ));
        copy_name(3, civ_title(civ));
        copy_name(4, player_people(p));
    }
    for (i = 0; i < NAMES; ++i) args[i] = names[i];
    popup_begin();
    popup_add_text(id, args);
    popup_show();
}

// A computer player the human has met and that is still standing, or
// PLAYERS.
static byte correspondent(void)
{
    byte p;
    for (p = PLAYER_FIRST_AI; p < PLAYERS; ++p)
        if (((diplomacy_known[PLAYER_HUMAN] >> p) & 1) && player_alive(p))
            return p;
    return PLAYERS;
}

// A short message in a popup, with a city's name in it: first, the name,
// then the rest.
static void tell(const char *first, byte c, const char *rest)
{
    if (QUIET) return;
    popup_begin();
    popup_add(first);
    popup_add(city_name_of(c));
    popup_add(rest);
    popup_show();
}

// ------------------------------------------------------------ Sea Peoples

// The coastal city the raiders make for: the largest, a trading people's
// counted larger, with a little chance in it. CITY_NONE if none.
static byte target(void)
{
    byte c, best = CITY_NONE, score, best_score = 0;

    for (c = 0; c < city_count; ++c) {
        if (!city_coastal(c)) continue;
        score = city_size[c] + (rnd() & 3) + 1;
        if (city_owner[c] < PLAYERS && trade_partners[city_owner[c]])
            score += 3;
        if (score > best_score) {
            best_score = score;
            best = c;
        }
    }
    return best;
}

// Empty shallows LANDING_NEAR to LANDING_FAR hexagons from city c, a random
// one, into (sea_x, sea_y). 0 if none.
static byte sea_x, sea_y;

static byte landing(byte c)
{
    int dx, dy, y;
    byte x, r, best = 0;

    for (dy = -LANDING_FAR / 2 - 1; dy <= LANDING_FAR / 2 + 1; ++dy) {
        y = (int)city_ay[c] + dy;
        if (y < 0 || y >= MAP_ROWS) continue;
        for (dx = -2 * LANDING_FAR; dx <= 2 * LANDING_FAR; ++dx) {
            x = (byte)((int)city_ax[c] + dx + MAP_COLUMNS) & (MAP_COLUMNS - 1);
            if (MAP_TERRAIN(x, y) != TILE_SHALLOW_OCEAN
                || (map_tile[y][x] & MAP_OCCUPIED))
                continue;
            r = distance(x, (byte)y, city_ax[c], city_ay[c]);
            if (r < LANDING_NEAR || r > LANDING_FAR) continue;
            r = rnd() | 1;
            if (r <= best) continue;
            best = r;
            sea_x = x;
            sea_y = (byte)y;
        }
    }
    return best != 0;
}

// A fleet: a galley a ship, and two raiders aboard each (KIND_RAIDER).
static void raid(void)
{
    byte c = target(), ships, n;

    if (c == CITY_NONE) return;
    ships = 1 + (byte)((game_turn - GAME_CRISIS_TURN) / 80);
    for (n = 0; n < ships; ++n) {
        if (unit_count_kind(PLAYER_BARBARIAN, KIND_NONE) + 3 > SEA_CAP
            || !landing(c)
            || unit_create(KIND_GALLEY, PLAYER_BARBARIAN, sea_x, sea_y)
                   == UNIT_NONE)
            break;
        unit_create(KIND_RAIDER, PLAYER_BARBARIAN, sea_x, sea_y);
        unit_create(KIND_RAIDER, PLAYER_BARBARIAN, sea_x, sea_y);
    }
    if (n && city_owner[c] == PLAYER_HUMAN)
        tell("Ships of the enemy have been seen off ", c, ".");
}

// --------------------------------------------------------------- collapse

// p's capital: the first city it founded -- the first name on its list --
// while it is still p's. CITY_NONE once it has been taken or burnt.
byte crisis_capital(byte p)
{
    byte c;
    for (c = 0; c < city_count; ++c)
        if (city_founder[c] == p && city_name[c] == 0 && city_owner[c] == p)
            return c;
    return CITY_NONE;
}

// City c goes over to the rebels -- the barbarians, who hold it now -- and
// so do p's units in it.
static void rebel(byte p, byte c)
{
    byte u;
    for (u = 0; u < MAX_UNITS; ++u)
        if (unit_kind[u] != KIND_NONE && unit_owner[u] == p
            && unit_ax[u] == city_ax[c] && unit_ay[u] == city_ay[c])
            unit_owner[u] = PLAYER_BARBARIAN;
    city_capture(c, PLAYER_BARBARIAN);
}

// **A people that has lost its capital breaks up**, as in Civilization,
// whose computer players split in two when their capital is taken and
// they have more than four cities (OpenCivOne's Actions.cs and
// Secession.cs): the cities furthest from the seat that is left, until a
// third of the citizens, go over -- there to a new people, here, with no
// player to spare, to the rebels. The seat is the largest city left, as
// the old capital may be ashes. With fewer than COLLAPSE_CITIES left,
// which is Civilization's five scaled to these peoples' six, nothing.
#define COLLAPSE_CITIES 3

static void collapse(byte p)
{
    byte c, seat = CITY_NONE, n = 0, far;
    word total = 0, gone = 0, d, best;

    for (c = 0; c < city_count; ++c) {
        if (city_owner[c] != p) continue;
        ++n;
        total += city_size[c];
        if (seat == CITY_NONE || city_size[c] > city_size[seat]) seat = c;
    }
    if (n < COLLAPSE_CITIES) return;
    if (!QUIET && ((diplomacy_known[PLAYER_HUMAN] >> p) & 1)) {
        popup_begin();
        popup_add("Civil war: the ");
        popup_add(player_people(p));
        popup_add(" have lost their capital, and ");
    }
    while (3 * gone < total) {
        far = CITY_NONE;
        best = 0;
        for (c = 0; c < city_count; ++c) {
            if (city_owner[c] != p) continue;
            d = distance(city_ax[c], city_ay[c],
                         city_ax[seat], city_ay[seat]);
            if (d > best) {
                best = d;
                far = c;
            }
        }
        if (far == CITY_NONE) break;
        gone += city_size[far];
        rebel(p, far);
        if (!QUIET && ((diplomacy_known[PLAYER_HUMAN] >> p) & 1)) {
            popup_add(city_name_of(far));
            popup_add(", ");
        }
    }
    if (!QUIET && ((diplomacy_known[PLAYER_HUMAN] >> p) & 1)) {
        popup_add("have gone over to the rebels.");
        popup_show();
    }
}

// ------------------------------------------------------------------ shocks

// A city at random, CITY_NONE with none.
static byte any_city(void)
{
    return city_count ? (byte)(rnd() % city_count) : CITY_NONE;
}

static void shrink(byte c, byte by)
{
    city_size[c] = city_size[c] > by ? city_size[c] - by : 1;
}

static void shock(void)
{
    byte c = any_city(), nx, ny, u, dir;     // dir: also the old owner

    if (c == CITY_NONE) return;
    switch (rnd() & 3) {
    case 0:
        crisis_drought = DROUGHT_TURNS + (rnd() & 7);
        if (!QUIET)
            popup("Drought: the rains have failed. Grassland, plains and "
                  "desert give a food less for a while.");
        return;
    case 1:
        // The improvement on one of its neighbours, if there is one there.
        shrink(c, 1);
        if (map_neighbour(city_ax[c], city_ay[c], rnd() % 6, &nx, &ny))
            map_tile[ny][nx] &= (byte)~MAP_IMPROVEMENT;
        if (city_owner[c] == PLAYER_HUMAN)
            tell("Earthquake: houses have fallen in ", c, ".");
        return;
    case 2:
        // The largest city, where the sickness spreads.
        for (u = 0; u < city_count; ++u)
            if (city_size[u] > city_size[c]) c = u;
        shrink(c, city_size[c] / 3);
        if (city_owner[c] == PLAYER_HUMAN)
            tell("Plague in ", c, ".");
        return;
    }
    // A revolt: a city nobody guards goes over to the rebels, who hold it
    // with a warrior of their own.
    if (unit_at(city_ax[c], city_ay[c]) != UNIT_NONE
        || city_owner[c] == PLAYER_BARBARIAN)
        return;
    dir = city_owner[c];
    city_capture(c, PLAYER_BARBARIAN);
    u = unit_create(KIND_WARRIOR, PLAYER_BARBARIAN, city_ax[c], city_ay[c]);
    if (u != UNIT_NONE) unit_orders[u] = ORDERS_FORTIFY;
    if (dir == PLAYER_HUMAN)
        tell("Revolt: rebels have taken ", c, ".");
}

// ------------------------------------------------------------------- a turn

void crisis_turn(void)
{
    byte p, c, n;

    if (game_turn == RUMOUR_TURN) letter(TEXT_CRISIS_RUMOUR, PLAYERS);
    if (game_turn == WARNING_TURN) {
        p = correspondent();
        if (p < PLAYERS) letter(TEXT_CRISIS_WARNING, p);
    }

    // A people that has lost its capital since the last turn breaks up;
    // whenever it happens, not only in the Crisis.
    for (p = PLAYER_FIRST_AI; p < PLAYERS; ++p) {
        n = (byte)(1 << p);
        if (crisis_capital(p) != CITY_NONE) {
            crisis_capitals |= n;
        } else if (crisis_capitals & n) {
            crisis_capitals &= (byte)~n;
            collapse(p);
        }
    }

    if (game_turn < GAME_CRISIS_TURN) return;
    if (game_turn == GAME_CRISIS_TURN) {
        p = correspondent();
        letter(p < PLAYERS ? TEXT_CRISIS_BEGINS : TEXT_CRISIS_ALONE, p);
    }

    // The pleas: a people the human knows that has lost a city since the
    // last turn writes, one letter a turn.
    for (p = PLAYER_FIRST_AI; p < PLAYERS; ++p) {
        for (c = 0, n = 0; c < city_count; ++c)
            if (city_owner[c] == p) ++n;
        if (n < crisis_cities[p] && n
            && ((diplomacy_known[PLAYER_HUMAN] >> p) & 1)
            && game_turn > GAME_CRISIS_TURN) {
            letter(TEXT_CRISIS_PLEA, p);
            crisis_cities[p] = n;
            break;
        }
        crisis_cities[p] = n;
    }

    if (crisis_drought) --crisis_drought;
    // More often as it goes on: 3 turns in 16 at first, 8 by the end.
    if ((rnd() & 15) < 3 + (game_turn - GAME_CRISIS_TURN) / 30) raid();
    if (!(rnd() & 15)) shock();
}
