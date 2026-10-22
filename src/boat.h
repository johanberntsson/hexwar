// The ships' exploring, for the computer players (milestone 9): the
// fishing boat's whole job until it has fish to work, and the galley's
// when it has nobody to ferry.
//
// **In region 4** (src/region.h), beside sea_route(), which it calls for
// every step; called from region 1, the AI.
#ifndef SEVENSHIPS_BOAT_H
#define SEVENSHIPS_BOAT_H

#include "mega65.h"

// Ship u, a computer player's, explores the coasts with the moves it has
// left: to the nearest block of the map with shallows that its people has
// not seen (ai_seen, ai_sea in ai.h), along the shallows by sea_route().
// A block it cannot reach, or has come next to its shallows, counts as
// seen. With every block seen it waits. Uses up at least one move.
void boat_explore(byte u);

#endif // SEVENSHIPS_BOAT_H
