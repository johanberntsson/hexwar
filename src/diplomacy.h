// Contact between the peoples, and the Diplomacy screen (Advisors on the
// menu bar): who the player has met, and what is known of them.
//
// **A people is met when one of its units or cities is on a hexagon the
// player has explored** -- seen, that is, as the map draws it. Checked by
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

// Note any people newly seen, and greet each with a popup over the map:
// "You have met the Hittites ...". The caller redraws the view if a popup
// was shown, which the return says.
byte diplomacy_contact(void);

// The Diplomacy screen until the player leaves. Writes over the whole
// screen and hides sprites 2 and 3, like the help screen, so **the caller
// puts the map screen back afterwards**.
void diplomacy_screen(void);

#endif // SEVENSHIPS_DIPLOMACY_H
