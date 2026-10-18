// The city screen, after Civilization's (civ1-reference/civ1-pic02.png):
// the city and its neighbours in the middle, the units in it on the left,
// and what it is building on the right with a CHANGE button. EXIT goes back
// to the map.
#ifndef SEVENSHIPS_CITYSCREEN_H
#define SEVENSHIPS_CITYSCREEN_H

#include "mega65.h"

// Show city `c` until the player leaves it. Runs its own once-a-frame loop,
// like a popup, and takes over the whole screen and sprites 2 and 3, so
// **the caller puts the map screen back afterwards** and redraws the view:
// the patch in the middle is drawn into the map window's tile store.
void city_screen(byte c);

#endif // SEVENSHIPS_CITYSCREEN_H
