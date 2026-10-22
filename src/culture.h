// Buildings and wonders (23 Oct 2026; vision.md section 5B, the legacy
// victory): a city's temple, and the Lion Gate of Mycenae, the first
// wonder.
//
// **They are built as units are**, as kinds after the units' (KIND_TEMPLE,
// KIND_LION_GATE in unit.h), and city_produce() hands a finished one to
// culture_built(). What a city has is city_culture[] (city.h), a bit a
// building, kept when the city is taken.
//
// **The temple** is Civilization's: 40 shields, and 1 gold a turn upkeep
// (treasury.h), the treasury's first expense; with too little gold it is
// sold, as Civilization sells a building. It has no happiness to make, so
// what it gives is LEGACY_TEMPLE legacy points.
//
// **The Lion Gate**, a wonder: one in the world, 160 shields, no upkeep,
// announced to everybody when built, and every other city building it
// changes to warriors, keeping its shields, as in Civilization. It doubles
// the defence of every unit in its city -- the walls of Mycenae; its
// nearest in Civilization is the Great Wall, which acts as city walls --
// and gives LEGACY_WONDER legacy points, Civilization's score for a wonder.
//
// **Legacy points** (player_legacy[]) are the builder's for good, whatever
// becomes of the building or the city afterwards: they count in the score,
// and the highest at the end is the legacy victory, even for a people that
// has fallen (score.c).
//
// **In region 6** (src/region.h), called through a stub from city.c.
#ifndef SEVENSHIPS_CULTURE_H
#define SEVENSHIPS_CULTURE_H

#include "mega65.h"
#include "unit.h"

#define CITY_TEMPLE 0x01
#define CITY_LION_GATE 0x02

#define WONDER_LION_GATE 0x01   // wonders_built
#define LEGACY_TEMPLE 3
#define LEGACY_WONDER 20

extern byte wonders_built;              // saved
extern word player_legacy[PLAYERS];     // saved

// Whether city c may build `kind`, a building: a temple where there is
// none, a wonder nobody has built.
#define CULTURE_CAN(C, K)                                                   \
    ((K) == KIND_TEMPLE ? !(city_culture[C] & CITY_TEMPLE)                  \
                        : !(wonders_built & WONDER_LION_GATE))

// City c has finished building `kind`. city.c's.
void culture_built(byte c, byte kind);

#endif // SEVENSHIPS_CULTURE_H
