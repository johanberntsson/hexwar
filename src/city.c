#include "city.h"
#include "culture.h"
#include "growth.h"
#include "civ.h"
#include "map.h"
#include "minimap.h"
#include "sheet.h"
#include "unit.h"

byte city_count;
byte city_founded;
// In HIGH_BSS, out of the 32 KB; city_count says which entries mean
// anything, and city_reset() sets it for every game.
HIGH_BSS byte city_owner[MAX_CITIES];
HIGH_BSS byte city_ax[MAX_CITIES];
HIGH_BSS byte city_ay[MAX_CITIES];
HIGH_BSS byte city_founder[MAX_CITIES];
HIGH_BSS byte city_name[MAX_CITIES];
HIGH_BSS byte city_build[MAX_CITIES];
HIGH_BSS word city_shields[MAX_CITIES];
HIGH_BSS byte city_size[MAX_CITIES];
HIGH_BSS byte city_food[MAX_CITIES];
HIGH_BSS byte city_worked[MAX_CITIES];
HIGH_BSS byte city_culture[MAX_CITIES];

// How many cities each player has named, so the next gets the next name.
byte city_named[PLAYERS];

void city_reset(void)
{
    byte p;
    city_count = 0;
    city_founded = 0;
    for (p = 0; p < PLAYERS; ++p) city_named[p] = 0;
}

byte city_at(byte ax, byte ay)
{
    byte c;
    for (c = 0; c < city_count; ++c)
        if (city_ax[c] == ax && city_ay[c] == ay) return c;
    return CITY_NONE;
}

byte city_coastal(byte c)
{
    byte dir, nx, ny;
    for (dir = 0; dir < 6; ++dir)
        if (map_neighbour(city_ax[c], city_ay[c], dir, &nx, &ny)
            && MAP_TERRAIN(nx, ny) == TILE_SHALLOW_OCEAN)
            return 1;
    return 0;
}

byte city_found(byte owner, byte ax, byte ay)
{
    byte c = city_count;
    if (c == MAX_CITIES || city_at(ax, ay) != CITY_NONE) return CITY_NONE;
    city_founder[c] = owner;
    city_ax[c] = ax;
    city_ay[c] = ay;
    city_name[c] = city_named[owner]++;
    city_size[c] = 1;
    city_food[c] = 0;
    city_culture[c] = 0;
    city_founded |= (byte)(1 << owner);
    ++city_count;
    city_capture(c, owner);
    unit_mark(ax, ay);
    return c;
}

void city_capture(byte c, byte owner)
{
    city_owner[c] = owner;
    city_worked[c] = 0;
    city_build[c] = KIND_WARRIOR;
    city_shields[c] = 0;
    minimap_plot(city_ax[c], city_ay[c]);
}

void city_destroy(byte c)
{
    byte ax = city_ax[c], ay = city_ay[c];
    byte last = --city_count;

    city_owner[c] = city_owner[last];
    city_founder[c] = city_founder[last];
    city_ax[c] = city_ax[last];
    city_ay[c] = city_ay[last];
    city_name[c] = city_name[last];
    city_build[c] = city_build[last];
    city_shields[c] = city_shields[last];
    city_size[c] = city_size[last];
    city_food[c] = city_food[last];
    city_worked[c] = city_worked[last];
    city_culture[c] = city_culture[last];
    unit_mark(ax, ay);
}

const char *city_name_of(byte c)
{
    // Past the end of a list, names repeat; that is a problem for when a
    // player has founded a dozen cities, which nobody can yet.
    return civ_city_name(player_civ[city_founder[c]], city_name[c]);
}

byte city_production(byte c)
{
    city_work(c);
    return work_shields;
}

byte city_turns_left(byte c)
{
    word cost = unit_kind_cost[city_build[c]];
    word have = city_shields[c];
    byte rate = city_production(c);

    if (have >= cost) return 1;
    return (byte)((cost - have + rate - 1) / rate);
}

byte city_produce(byte c)
{
    byte kind = city_build[c];
    word cost = unit_kind_cost[kind];

    if (kind == KIND_NONE) return KIND_NONE;    // told to build nothing
    city_shields[c] += city_production(c);
    if (city_shields[c] < cost) return KIND_NONE;
    if (KIND_IS_BUILDING(kind)) {               // src/culture.h
        city_shields[c] -= cost;
        culture_built(c, kind);
        return kind;
    }
    if ((kind == KIND_SETTLER && city_size[c] < 2)
        || unit_create(kind, city_owner[c], city_ax[c], city_ay[c])
               == UNIT_NONE) {
        city_shields[c] = cost;
        return KIND_NONE;
    }
    city_shields[c] -= cost;
    if (kind == KIND_SETTLER) {
        // The citizen goes with it, and the food box is the smaller one.
        if (city_food[c] >= GROWTH_BOX(--city_size[c]))
            city_food[c] = GROWTH_BOX(city_size[c]) - 1;
    }
    return kind;
}
