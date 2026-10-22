// What each of a computer player's cities builds, out of ai.c (22 Oct
// 2026, to make room in region 1): once a turn a player, in region 2
// beside improve_best(), which work_site() calls for every city, so that
// nothing is copied into slot 2 but this region once a player. ai.c's
// helpers come through stubs (aiint.h).
#define ai_build ai_build_r2

#include "ai.h"
#include "aiint.h"
#include "city.h"
#include "culture.h"
#include "emporium.h"
#include "treasury.h"
#include "trade.h"
#include "growth.h"
#include "map.h"
#include "sheet.h"
#include "unit.h"

#pragma clang section text="r2text" rodata="r2rodata"

// How many of p's units are fortified in city c. ai.c has its own, which
// leaves one unit out; a copy is cheaper than a stub.
static byte guards(byte p, byte c)
{
    byte v, n = 0;
    for (v = 0; v < MAX_UNITS; ++v)
        if (unit_kind[v] != KIND_NONE && unit_owner[v] == p
            && unit_ax[v] == city_ax[c] && unit_ay[v] == city_ay[c]
            && unit_orders[v] == ORDERS_FORTIFY)
            ++n;
    return n;
}

// Whether a spearman is fortified in city c, as ai.c's.
static byte spearman_in(byte c)
{
    byte v;
    for (v = 0; v < MAX_UNITS; ++v)
        if (unit_kind[v] == KIND_SPEARMAN && unit_ax[v] == city_ax[c]
            && unit_ay[v] == city_ay[c] && unit_orders[v] == ORDERS_FORTIFY)
            return 1;
    return 0;
}

// With a spearman in city c, the warriors fortified there beyond the
// guards it wants are disbanded: the spearman has replaced them, and the
// unit cap and the places in the unit list are wanted for other things.
static void retire(byte p, byte c)
{
    byte v;
    if (!spearman_in(c)) return;
    for (v = 0; v < MAX_UNITS; ++v)
        if (guards(p, c) > ai_guards_wanted(c)
            && unit_kind[v] == KIND_WARRIOR && unit_owner[v] == p
            && unit_orders[v] == ORDERS_FORTIFY
            && unit_ax[v] == city_ax[c] && unit_ay[v] == city_ay[c])
            unit_remove(v);
}

// Whether another of p's cities than c is building the Lion Gate.
#define LION_GATE_SIZE 5
#define TEMPLE_SIZE 3

static byte building(byte p, byte c)
{
    byte i;
    for (i = 0; i < city_count; ++i)
        if (i != c && city_owner[i] == p && city_build[i] == KIND_LION_GATE)
            return 1;
    return 0;
}

// Whether city c has fish next to it that no fishing boat works yet.
static byte fish_to_work(byte c)
{
    byte dir, nx, ny;
    for (dir = 0; dir < 6; ++dir)
        if (map_neighbour(city_ax[c], city_ay[c], dir, &nx, &ny)
            && map_resource_at(nx, ny) == RES_FISH
            && !(map_tile[ny][nx] & MAP_IRRIGATED))
            return 1;
    return 0;
}

// What each city builds, decided afresh every turn, after Civilization:
// 1. guards first, while it has fewer than guards_wanted() -- with none at
// all even over the cap; then explorers, warriors that are not guarding,
// up to EXPLORERS(); 2. a settler from a city with a citizen and food to
// spare, or from the first with a citizen if there is no settler about,
// and a site in reach -- settlers out and cities together no more than
// AI_CITY_CAP; then up to the cap 3. a builder for each city that has
// hexagons to improve, and 4. warriors to explore and fight. Changing
// keeps the shields, so nothing is lost by it. With bronze the guards are
// spearmen, defence 2 to a warrior's 1.
void ai_build(byte p)
{
    byte c, u, cap, kind, guard;
    byte cities = 0, settlers, builders, galleys, boats, merchants, explorers = 0;
    byte units = unit_count_kind(p, KIND_NONE) - unit_count_kind(p, KIND_SETTLER);

    for (c = 0; c < city_count; ++c)
        if (city_owner[c] == p) ++cities;
    cap = AI_UNIT_BASE + AI_UNITS_A_CITY * cities;
    if (cap > AI_UNIT_MAX) cap = AI_UNIT_MAX;
    settlers = unit_count_kind(p, KIND_SETTLER);
    builders = unit_count_kind(p, KIND_BUILDER);
    galleys = unit_count_kind(p, KIND_GALLEY);
    boats = unit_count_kind(p, KIND_BOAT);
    merchants = unit_count_kind(p, KIND_MERCHANT);
    for (u = 0; u < MAX_UNITS; ++u)
        if (unit_kind[u] == KIND_WARRIOR && unit_owner[u] == p
            && unit_orders[u] != ORDERS_FORTIFY)
            ++explorers;

    guard = player_bronze(p) ? KIND_SPEARMAN : KIND_WARRIOR;
    for (c = 0; c < city_count; ++c) {
        if (city_owner[c] != p) continue;
        retire(p, c);
        u = guards(p, c);
        city_work(c);
        if (u < ai_guards_wanted(c) && (u == 0 || units < cap)) {
            kind = guard;
        } else if (guard == KIND_SPEARMAN && !spearman_in(c)) {
            // Bronze come to a city guarded by warriors: one spearman, over
            // the cap if need be, since the warriors it replaces go once it
            // is in (retire()).
            kind = KIND_SPEARMAN;
        } else if (KIND_IS_BUILDING(city_build[c])
                   && CULTURE_CAN(c, city_build[c])) {
            // A building under way is finished, guards apart: changing
            // would spend its shields on warriors.
            kind = city_build[c];
        } else if (explorers < EXPLORERS(cities) && units < cap) {
            kind = KIND_WARRIOR;
            ++explorers;
        } else if (cities + settlers < AI_CITY_CAP && city_size[c] >= 2
                   && (work_food > 2 * city_size[c] + 1 || !settlers)
                   && ai_find_far_site(p, city_ax[c], city_ay[c])) {
            kind = KIND_SETTLER;
            ++settlers;
        } else if (!boats && city_coastal(c)) {
            // One fishing boat, to explore the coasts, over the cap if need
            // be: usually the first city's first build after its warrior.
            kind = KIND_BOAT;
            ++boats;
        } else if (!galleys && cities >= GALLEY_CITIES && city_coastal(c)
                   && player_has(p, RES_CEDAR)) {
            // One galley, to explore the coasts, over the cap if need be.
            kind = KIND_GALLEY;
            ++galleys;
        } else if (!merchants && cities >= 2 && city_size[c] >= 2
                   && city_coastal(c)
                   && (trade_partners[p]
                       || (guard != KIND_SPEARMAN
                           && emporium_count < MAX_EMPORIA))) {
            // A merchant ship, over the cap: to carry the player's trade
            // deals, which lapse without ships (src/trade.h), or without
            // deals to found an emporium by the copper or tin it lacks
            // (src/emporium.h). One at a time, and for emporia only without
            // bronze, so that a people does not ring the world with them.
            kind = KIND_MERCHANT;
            ++merchants;
        } else if (boats < 2 && city_size[c] >= 2 && fish_to_work(c)) {
            // A second boat, to work the fish (boat_explore()): over the
            // cap, since it is used up once it gets there. Not from a city
            // of size 1, which would rather grow and send a settler.
            kind = KIND_BOAT;
            ++boats;
        } else if (units >= cap) {
            // At the cap, culture (src/culture.h): the Lion Gate in a city
            // of LION_GATE_SIZE, one city at a time; a temple in a city of
            // TEMPLE_SIZE that can pay for it; else nothing.
            kind = KIND_NONE;
            if (city_size[c] >= LION_GATE_SIZE
                && CULTURE_CAN(c, KIND_LION_GATE) && !building(p, c))
                kind = KIND_LION_GATE;
            else if (city_size[c] >= TEMPLE_SIZE
                     && CULTURE_CAN(c, KIND_TEMPLE) && player_gold[p] >= 20)
                kind = KIND_TEMPLE;
        } else if (builders < cities && ai_work_site(c)) {
            kind = KIND_BUILDER;
            ++builders;
        } else {
            kind = KIND_WARRIOR;
        }
        city_build[c] = kind;
    }
}
