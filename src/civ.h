// The peoples a player can lead (vision.md §3), the difficulty levels, and
// which people each player is. For now a people is a set of names; what
// makes them play differently comes later.
#ifndef HEXWAR_CIV_H
#define HEXWAR_CIV_H

#include "mega65.h"

#define CIV_MYCENAEANS 0
#define CIV_HITTITES 1
#define CIV_EGYPTIANS 2
#define CIV_CANAANITES 3
#define CIVS 4

// The people, for the menu ("Egyptians"); the adjective, for the panel
// ("Egyptian", ten characters at most); and a line about them.
extern const char *const civ_people[CIVS];
extern const char *const civ_adjective[CIVS];
extern const char *const civ_note[CIVS];

// The leader, and the title the letters will address them by (vision.md §3,
// "Diplomacy in the Voice of the Letters").
extern const char *const civ_leader[CIVS];
extern const char *const civ_title[CIVS];

// Easiest first. game_level is an index into these, and does nothing yet.
#define LEVELS 5
extern const char *const level_name[LEVELS];
extern const char *const level_note[LEVELS];
extern byte game_level;

// Which people each player is, PLAYER_HUMAN first: every people plays, so
// PLAYERS is CIVS. The human leads the Mycenaeans unless civ_choose() says
// otherwise, which is what a build without the title screen plays.
extern byte player_civ[CIVS];

// The human leads `civ`, and the computer players the others in list order.
void civ_choose(byte civ);

// The nth city name of a people, at most ten characters; past the end of
// the list they repeat.
const char *civ_city_name(byte civ, byte n);

#endif // HEXWAR_CIV_H
