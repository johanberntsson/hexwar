// Routes over the shallows, for galleys (milestone 9): the computer
// players' ferrying (ai.c), and later the Sea Peoples and sea trade.
//
// **A breadth-first search from the target out over the shallows** until
// it reaches the ship, so the cell that reached it is the first step. The
// shallows are 350 to 420 hexagons a map, so a search is a few
// milliseconds, cheap enough to do again for every step. A visited bitmap
// and a queue, scratch in LOW_FREE, rebuilt every time.
//
// **In region 4** (src/region.h), with trade: called from region 1, the
// AI, in slot 1, so nothing is copied but the first call of a turn.
#ifndef SEVENSHIPS_PATH_H
#define SEVENSHIPS_PATH_H

#include "mega65.h"

// The first step (DIR_*, map.h) from (fx, fy) towards (tx, ty) along the
// shallows -- neither cities nor the open sea -- or DIR_NONE: already next
// to it or on it, or no way there. (fx, fy) may be anything, a city among
// them; (tx, ty) too, and the route ends next to it.
byte sea_route(byte fx, byte fy, byte tx, byte ty);

#endif // SEVENSHIPS_PATH_H
