#include "city.h"
#include "civ.h"
#include "unit.h"

byte city_count;
byte city_owner[MAX_CITIES];
byte city_ax[MAX_CITIES];
byte city_ay[MAX_CITIES];
byte city_name[MAX_CITIES];

// How many cities each player has named, so the next gets the next name.
static byte named[2];

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
