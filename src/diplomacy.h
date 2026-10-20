// Contact between the peoples, and the Diplomacy screen (Advisors on the
// menu bar): who the player has met, and what is known of them.
//
// **A people is met when one of its units or cities is next to one of the
// player's units or cities**, as in Civilization. Checked by
// diplomacy_contact() after each of the player's moves and at the end of
// every turn, and remembered in diplomacy_met, which is saved: a people
// once met stays met. Milestone 8's diplomacy (TODO.txt) builds on it.
//
// **In region 2** (src/region.h), with the city screen.
#ifndef SEVENSHIPS_DIPLOMACY_H
#define SEVENSHIPS_DIPLOMACY_H

#include "mega65.h"

// Bit p: the human has met player p. game_init() clears it.
extern byte diplomacy_met;

// Note any people newly next to the player's units or cities, as met, and
// return them as a bitmask (bit p, as diplomacy_met), 0 if none.
// diplomacy_ax/ay are then where one of them is, for the caller to bring
// into view before diplomacy_greet() says who they are.
byte diplomacy_contact(void);
extern byte diplomacy_ax, diplomacy_ay;

// A popup over the map for each people in `met`, a diplomacy_contact()
// bitmask: "You have met the Hittites ...".
void diplomacy_greet(byte met);

// The Diplomacy screen until the player leaves. Writes over the whole
// screen and hides sprites 2 and 3, like the help screen, so **the caller
// puts the map screen back afterwards**.
void diplomacy_screen(void);

#endif // SEVENSHIPS_DIPLOMACY_H
