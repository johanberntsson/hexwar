// The peoples a player can lead (vision.md §3), the difficulty levels, and
// which people each player is. For now a people is a set of names; what
// makes them play differently comes later.
//
// **The names are texts** (res/text/list_*.txt, read with text_line()), so
// each of these returns a string that is good until the next call but one.
#ifndef SEVENSHIPS_CIV_H
#define SEVENSHIPS_CIV_H

#include "mega65.h"
#include "text.h"

#define CIV_MYCENAEANS 0
#define CIV_HITTITES 1
#define CIV_EGYPTIANS 2
#define CIV_CANAANITES 3
#define CIVS 4

// The people, for the menu ("Egyptians"); the adjective, for the panel
// ("Egyptian", ten characters at most); and a line about them. CIVS, the
// fifth line, is the barbarians' in the first two.
#define civ_people(C) text_line(TEXT_LIST_PEOPLES, (C))
#define civ_adjective(C) text_line(TEXT_LIST_ADJECTIVES, (C))
#define civ_note(C) text_line(TEXT_LIST_NOTES, (C))

// The leader, and the title the letters will address them by (vision.md §3,
// "Diplomacy in the Voice of the Letters").
#define civ_leader(C) text_line(TEXT_LIST_LEADERS, (C))
#define civ_title(C) text_line(TEXT_LIST_TITLES, (C))

// Easiest first. game_level is an index into these, and does nothing yet.
#define LEVELS 5
#define level_name(L) text_line(TEXT_LIST_LEVELS, (L))
#define level_note(L) text_line(TEXT_LIST_LEVEL_NOTES, (L))
extern byte game_level;

// Which people each player is, PLAYER_HUMAN first: every people plays, so
// PLAYERS is CIVS. The human leads the Mycenaeans unless civ_choose() says
// otherwise, which is what a build without the title screen plays.
extern byte player_civ[CIVS];

// The human leads `civ`, and the computer players the others in list order.
void civ_choose(byte civ);

// Player p's adjective and people as civ_adjective and civ_people have
// them, or the barbarians' for PLAYER_BARBARIAN (unit.h).
const char *player_adjective(byte p);
const char *player_people(byte p);

// The nth city name of a people, at most ten characters; past the end of
// the list they repeat.
const char *civ_city_name(byte civ, byte n);

#endif // SEVENSHIPS_CIV_H
