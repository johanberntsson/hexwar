// The computer players: TODO.txt's milestone 3, the simplest thing that
// plays. Each one founds a city with its settler, builds a warrior, then a
// builder, then warriors that explore; the first warrior home stays to
// guard the city, and a builder goes to a hexagon next to its city and
// stays there until there are improvements to make.
//
// **Per-turn code, and in a file of its own**, so that it can go into a
// banked region when the 32 KB runs out (CLAUDE.md, "Growing past 32 KB").
#ifndef HEXWAR_AI_H
#define HEXWAR_AI_H

#include "mega65.h"

// Put each computer player's settler on grassland with land round it, as
// far from every other settler as a few hundred tries can find. After the
// human's settler is placed.
void ai_place(void);

// Player p's turn: moves back, what each city builds, and every unit's
// move. Production itself is end_turn()'s, for every player at once.
void ai_turn(byte p);

#endif // HEXWAR_AI_H
