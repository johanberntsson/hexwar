// The Crisis (milestone 10, vision.md section 4): the Bronze Age Collapse,
// from GAME_CRISIS_TURN (1200 BC, game.h) to the end.
//
// * **Omens** before it: a rumour at turn 200, a warning from a ruler the
//   human has met at 215, and at the start Ammurapi's letter
//   (res/text/crisis_*.txt).
// * **The Sea Peoples**: fleets of barbarian galleys, warriors aboard
//   (spearmen from turn 300), appear on the shallows off the richest
//   coastal cities, more often and in more ships as the Crisis goes on.
//   They are PLAYER_BARBARIAN, so the barbarians' AI (ai.c) sails them,
//   lands them and burns cities with them; only their coming is new.
// * **Shocks**, one turn in sixteen: drought (a food less from grassland,
//   plains and desert for some turns, growth.c), earthquake (a city a
//   citizen smaller and an improvement next to it gone), plague (the
//   largest city a third smaller), revolt (rebels by a city left without
//   a guard).
// * **Pleas**: a ruler the human has met writes when the Sea Peoples or
//   anybody else have taken another of its cities.
// * **Network fracture** is trade.c's: a deal now and then lost at sea.
//
// The human hears by popup, except during Debug/Play 50 Turns.
//
// **In region 5** (src/region.h), slot 2, with the score: once a turn, from
// game_end_turn().
#ifndef SEVENSHIPS_CRISIS_H
#define SEVENSHIPS_CRISIS_H

#include "mega65.h"
#include "unit.h"

// Turns of drought left: read by yield() in growth.c. And each people's
// cities at the end of the last turn, for the pleas. Saved.
extern byte crisis_drought;
extern byte crisis_cities[PLAYERS];

// A turn of it, before the cities grow: does nothing before the omens.
void crisis_turn(void);

#endif // SEVENSHIPS_CRISIS_H
