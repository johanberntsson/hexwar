// The Top Five Cities screen, opened from World on the menu bar. Only the
// frame so far: the five places are empty until cities have something to
// be ranked by.
#ifndef SEVENSHIPS_TOPCITIES_H
#define SEVENSHIPS_TOPCITIES_H

#include "mega65.h"

// Show the list until the player leaves. Runs its own once-a-frame loop and
// writes over the whole screen and hides sprites 2 and 3, like the help
// screen, so **the caller puts the map screen back afterwards**.
void top_cities_screen(void);

#endif // SEVENSHIPS_TOPCITIES_H
