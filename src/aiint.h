// ai.c's helpers that its other files use (src/aibuild.c), and the
// numbers they share. Not for anything else. The helpers are in region 1
// and called through stubs in src/dispatch.s; region 1 is slot 1's alone,
// so a call from a slot 2 region costs no copy.
#ifndef SEVENSHIPS_AIINT_H
#define SEVENSHIPS_AIINT_H

#include "mega65.h"

// A computer player stops building units, settlers apart, at AI_UNIT_BASE
// plus AI_UNITS_A_CITY for each city, up to AI_UNIT_MAX -- a guard and a
// builder a city and a few to explore -- so that three of them and the
// barbarians cannot fill the MAX_UNITS slots every player shares.
#define AI_UNIT_BASE 3
#define AI_UNITS_A_CITY 2
#define AI_UNIT_MAX 12

// No more than AI_CITY_CAP cities, settlers on their way counted;
// MAX_CITIES is shared with the human.
#define AI_CITY_CAP 6

// A player builds a galley once it has this many cities, and cedar.
#define GALLEY_CITIES 3

// Warriors a player keeps exploring, not guarding: one, and one more for
// every three cities. Without them the guards take every warrior built,
// and in 100 turns of autoplay the player had seen 139 of 1100 hexagons of
// a continent everybody shared, and met nobody.
#define EXPLORERS(CITIES) (1 + (CITIES) / 3)

// Guards city c wants (ai.c's guards_wanted()).
byte ai_guards_wanted(byte c);

// A site for a city within reach of (fx, fy) for player p, over the water
// with a galley; whether there is one (ai.c's find_far_site()).
byte ai_find_far_site(byte p, byte fx, byte fy);

// Whether a hexagon round city c wants improving and has nobody on it
// (ai.c's work_site()).
byte ai_work_site(byte c);

// What each of p's cities builds this turn: src/aibuild.c, region 2.
void ai_build(byte p);

#endif // SEVENSHIPS_AIINT_H
