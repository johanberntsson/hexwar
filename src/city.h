// Cities. For now a city is a place and a name; what it builds is milestone 2
// (TODO.txt).
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

// The city on (ax, ay), or CITY_NONE.
byte city_at(byte ax, byte ay);

// Found a city for `owner` on (ax, ay). CITY_NONE if there is one there
// already or the list is full.
byte city_found(byte owner, byte ax, byte ay);

// A city's name, ten characters at most so that it fits the panel.
const char *city_name_of(byte c);

#endif // HEXWAR_CITY_H
