// The computer players: TODO.txt's milestone 3, the simplest thing that
// plays. Each one founds a city with its settler, builds a warrior, then a
// builder, then warriors that explore; the first warrior home stays to
// guard the city, and a builder goes to a hexagon next to its city and
// stays there until there are improvements to make.
//
// Milestone 4: a warrior attacks an enemy next to it when the odds are
// good, goes home when its city has nobody guarding it, and otherwise
// heads for the nearest enemy within reach before it explores. Between the
// peoples it is peace unless the human declared war, so for now that means
// the barbarians -- who are here too, in ai_barbarians().
//
// **Per-turn code, and in a file of its own**, so that it can go into a
// banked region when the 32 KB runs out (CLAUDE.md, "Growing past 32 KB").
#ifndef SEVENSHIPS_AI_H
#define SEVENSHIPS_AI_H

#include "mega65.h"

// Put each computer player's settler on grassland with land round it, as
// far from every other settler as a few hundred tries can find. After the
// human's settler is placed.
void ai_place(void);

// Player p's turn: moves back, what each city builds, and every unit's
// move. Production itself is end_turn()'s, for every player at once.
void ai_turn(byte p);

// The barbarians' turn. From BARB_FIRST_TURN, now and then, a band of two
// warriors appears on land well away from everybody's units and cities;
// every barbarian attacks whatever it is next to, whatever the odds, or
// makes for the nearest unit or city, and burns a city it takes.
void ai_barbarians(word turn);

#endif // SEVENSHIPS_AI_H
