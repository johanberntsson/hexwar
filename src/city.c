#include "city.h"
#include "civ.h"
#include "map.h"
#include "sheet.h"
#include "unit.h"

// Shields a hexagon yields, by terrain value (a TILE_* number).
static const byte terrain_shields[] = {
    2,      // TILE_DESERT_HILL
    1,      // TILE_GRASS
    1,      // TILE_MOUNTAIN
    0,      // TILE_OCEAN
    0,      // TILE_SHALLOW_OCEAN
    0,      // TILE_TUNDRA
};

byte city_count;
byte city_owner[MAX_CITIES];
byte city_ax[MAX_CITIES];
byte city_ay[MAX_CITIES];
byte city_name[MAX_CITIES];
byte city_build[MAX_CITIES];
word city_shields[MAX_CITIES];

// How many cities each player has named, so the next gets the next name.
static byte named[2];

void city_reset(void)
{
    city_count = 0;
    named[0] = named[1] = 0;
}

byte city_at(byte ax, byte ay)
{
    byte c;
    for (c = 0; c < city_count; ++c)
        if (city_ax[c] == ax && city_ay[c] == ay) return c;
    return CITY_NONE;
}

byte city_found(byte owner, byte ax, byte ay)
{
    byte c = city_count;
    if (c == MAX_CITIES || city_at(ax, ay) != CITY_NONE) return CITY_NONE;
    city_owner[c] = owner;
    city_ax[c] = ax;
    city_ay[c] = ay;
    city_name[c] = named[owner]++;
    city_build[c] = KIND_WARRIOR;
    city_shields[c] = 0;
    ++city_count;
    unit_mark(ax, ay);
    return c;
}

const char *city_name_of(byte c)
{
    // Past the end of a list, names repeat; that is a problem for when a
    // player has founded a dozen cities, which nobody can yet.
    return civ_city_name(player_civ[city_owner[c]], city_name[c]);
}

byte city_production(byte c)
{
    byte ax = city_ax[c], ay = city_ay[c];
    byte dir, nx, ny, s, best = 0;
    byte own = terrain_shields[MAP_TERRAIN(ax, ay)];

    if (own == 0) own = 1;
    for (dir = 0; dir < 6; ++dir) {
        if (!map_neighbour(ax, ay, dir, &nx, &ny)) continue;
        s = terrain_shields[MAP_TERRAIN(nx, ny)];
        if (s > best) best = s;
    }
    return own + best;
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

    city_shields[c] += city_production(c);
    if (city_shields[c] < cost) return KIND_NONE;
    if (unit_create(kind, city_owner[c], city_ax[c], city_ay[c])
        == UNIT_NONE) {
        city_shields[c] = cost;
        return KIND_NONE;
    }
    city_shields[c] -= cost;
    return kind;
}
