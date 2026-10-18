// Cities: a place, a name, and what it is building.
#ifndef HEXWAR_CITY_H
#define HEXWAR_CITY_H

#include "mega65.h"

#define MAX_CITIES 32
#define CITY_NONE 0xff

extern byte city_count;
extern byte city_owner[MAX_CITIES];
extern byte city_ax[MAX_CITIES];
extern byte city_ay[MAX_CITIES];
extern byte city_name[MAX_CITIES];      // index into the owner's name list
extern byte city_build[MAX_CITIES];     // the KIND_* it is building
extern word city_shields[MAX_CITIES];   // towards it, of unit_kind_cost[]

// The city on (ax, ay), or CITY_NONE.
byte city_at(byte ax, byte ay);

// Found a city for `owner` on (ax, ay). CITY_NONE if there is one there
// already or the list is full.
byte city_found(byte owner, byte ax, byte ay);

// A city's name, ten characters at most so that it fits the panel.
const char *city_name_of(byte c);

// **Production, until there is population and food to work tiles with**:
// a city makes the shields of its own hexagon (at least 1) plus those of its
// best neighbour, as a size-1 city working one tile would.
byte city_production(byte c);

// Turns until city `c` finishes what it is building, at least 1.
byte city_turns_left(byte c);

// One turn's production for city `c`, at the end of its owner's turn. When
// the shields reach the cost, the unit appears on the city's hexagon and the
// surplus carries over; returns its KIND_*, or KIND_NONE. With the unit list
// full the shields wait at the cost until there is room.
byte city_produce(byte c);

#endif // HEXWAR_CITY_H
