// The score, and the screen that shows it (milestone 10): World -> Score
// during a game, and the final screen at its end -- turn GAME_LAST_TURN,
// the human's people the last left, or the human's out -- or on Retire.
//
// **A people's score**, after Civilization's: a point a citizen, two a
// city, two a good it has (olives, wine, cedar, copper, tin, dye; owned or
// traded for, player_has()), three a trade deal it is in. The outcome at
// the end is vision.md's: a people that has fallen is defeated; one that
// is still standing at the end has a survival victory, and with the
// highest score a hegemony victory. (The legacy victory needs monuments.)
//
// **In region 5** (src/region.h), slot 2: called once a click.
#ifndef SEVENSHIPS_SCORE_H
#define SEVENSHIPS_SCORE_H

#include "mega65.h"

// What the screen is for: the score so far; on Retire; or the end.
#define SCORE_NOW 0
#define SCORE_RETIRED 1
#define SCORE_END 2

// Show the score until the player leaves. Writes over the whole screen
// and hides sprites 2 and 3, like the help screen, so **the caller puts
// the map screen back afterwards** -- or, after the last, does not need to.
void score_screen(byte how);

#endif // SEVENSHIPS_SCORE_H
