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

#define RUMOUR_TURN 200
#define WARNING_TURN 215
#define DROUGHT_TURNS 8         // and up to 7 more
#define SEA_CAP 20              // barbarian units, the Sea Peoples' limit
#define SPEARMEN_TURN 300       // the raiders come with bronze from here
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
        if (trade_partners[city_owner[c]]) score += 3;
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

// A fleet: a galley a ship, and two raiders aboard each.
static void raid(void)
{
    byte c = target(), ships, kind, n;

    if (c == CITY_NONE) return;
    ships = 1 + (byte)((game_turn - GAME_CRISIS_TURN) / 80);
    kind = game_turn >= SPEARMEN_TURN ? KIND_SPEARMAN : KIND_WARRIOR;
    for (n = 0; n < ships; ++n) {
        if (unit_count_kind(PLAYER_BARBARIAN, KIND_NONE) + 3 > SEA_CAP
            || !landing(c)
            || unit_create(KIND_GALLEY, PLAYER_BARBARIAN, sea_x, sea_y)
                   == UNIT_NONE)
            break;
        unit_create(kind, PLAYER_BARBARIAN, sea_x, sea_y);
        unit_create(kind, PLAYER_BARBARIAN, sea_x, sea_y);
    }
    if (n && city_owner[c] == PLAYER_HUMAN)
        tell("Ships of the enemy have been seen off ", c, ".");
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
    byte c = any_city(), dir, nx, ny, u;

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
    // A revolt, by a city nobody guards.
    if (unit_at(city_ax[c], city_ay[c]) != UNIT_NONE) return;
    for (dir = u = 0; dir < 6 && u < 2; ++dir)
        if (map_neighbour(city_ax[c], city_ay[c], dir, &nx, &ny)
            && unit_can_enter(PLAYER_BARBARIAN, KIND_WARRIOR, nx, ny)
                   == ENTER_OK
            && !(map_tile[ny][nx] & MAP_OCCUPIED)
            && unit_create(KIND_WARRIOR, PLAYER_BARBARIAN, nx, ny)
                   != UNIT_NONE)
            ++u;
    if (u && city_owner[c] == PLAYER_HUMAN)
        tell("Revolt: rebels have taken up arms by ", c, ".");
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
    // More often as it goes on: 2 turns in 16 at first, 7 by the end.
    if ((rnd() & 15) < 2 + (game_turn - GAME_CRISIS_TURN) / 35) raid();
    if (!(rnd() & 15)) shock();
}
