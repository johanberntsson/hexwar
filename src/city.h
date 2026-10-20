// Cities: a place, a name, and what it is building.
#ifndef SEVENSHIPS_CITY_H
#define SEVENSHIPS_CITY_H

#include "mega65.h"
#include "unit.h"

#define MAX_CITIES 32
#define CITY_NONE 0xff

extern byte city_count;
extern byte city_owner[MAX_CITIES];
extern byte city_ax[MAX_CITIES];
extern byte city_ay[MAX_CITIES];
extern byte city_founder[MAX_CITIES];   // whose name list city_name is in
extern byte city_name[MAX_CITIES];      // index into the founder's names
extern byte city_build[MAX_CITIES];     // the KIND_* it is building, or
                                        // KIND_NONE for nothing (the AI's)
extern word city_shields[MAX_CITIES];   // towards it, of unit_kind_cost[]
extern byte city_size[MAX_CITIES];      // citizens, 1 and up (src/growth.h)
extern byte city_food[MAX_CITIES];      // in the food box, of GROWTH_BOX

// Bit p: player p has founded a city, so losing the last one is the end
// (combat.c:player_alive()).
extern byte city_founded;
// How many cities each player has named, so the next gets the next name.
extern byte city_named[PLAYERS];

// No cities, and every player's names unused, for a new game.
void city_reset(void);

// The city on (ax, ay), or CITY_NONE.
byte city_at(byte ax, byte ay);

// Found a city for `owner` on (ax, ay). CITY_NONE if there is one there
// already or the list is full.
byte city_found(byte owner, byte ax, byte ay);

// City c is `owner`'s now, keeping its name, and starts on warriors.
void city_capture(byte c, byte owner);

// City c is gone. **The last city takes its index**, so a loop over the
// cities that destroys one has to look at the same index again.
void city_destroy(byte c);

// A city's name, ten characters at most so that it fits the panel.
const char *city_name_of(byte c);

// Shields a turn: those of the hexagons city c works (city_work() in
// src/growth.h).
byte city_production(byte c);

// Turns until city `c` finishes what it is building, at least 1.
byte city_turns_left(byte c);

// One turn's production for city `c`, at the end of its owner's turn. When
// the shields reach the cost, the unit appears on the city's hexagon and the
// surplus carries over; returns its KIND_*, or KIND_NONE. With the unit list
// full the shields wait at the cost until there is room, and so they do for
// a settler until the city has a citizen to spare for it: a settler takes
// one, so it needs size 2.
byte city_produce(byte c);

#endif // SEVENSHIPS_CITY_H
