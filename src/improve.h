// The builders' improvements, milestone 6: irrigation and mines.
//
// An improvement is two bits of the hexagon's map byte (MAP_IRRIGATED,
// MAP_MINED in src/map.h), drawn as an overlay over its terrain. A builder
// makes one by standing on the hexagon with standing orders --
// ORDERS_IRRIGATE or ORDERS_MINE -- for a number of turns, counted in
// unit_work[]; it asks for no orders meanwhile, and anything that wakes it
// starts its count again. Builders on one hexagon with the same orders work
// together: their counts add up, so two irrigate in three turns, not five.
// One improvement a hexagon: a mine replaces the irrigation, and the other
// way round, as in Civilization.
//
// Irrigation is grassland, plains, desert, floodplain or hills with water
// next to it -- the sea, a floodplain (which stands for a river) or another
// irrigated hexagon -- or a floodplain itself. Mines are on hills, mountains
// and desert. A mine adds shields (city.c); irrigation will add food once
// there is food.
//
// **Per-turn code, in region 1** with the computer players (src/region.h),
// called through stubs in src/dispatch.s.
#ifndef SEVENSHIPS_IMPROVE_H
#define SEVENSHIPS_IMPROVE_H

#include "mega65.h"

// Whether a builder could start `orders` (ORDERS_IRRIGATE or ORDERS_MINE)
// on (ax, ay): the terrain allows it, and it is not there already.
byte improve_can(byte orders, byte ax, byte ay);

// The improvement the computer players make on (ax, ay): a mine on hills
// and mountains, irrigation where it can go, else a mine; ORDERS_NONE if
// neither can.
byte improve_best(byte ax, byte ay);

// Builder u starts `orders` where it stands, which the caller has checked
// with improve_can(). Uses up its moves.
void improve_start(byte u, byte orders);

// A turn's work for every builder of every player that has the orders.
// Returns the orders of the last of the human's to finish this turn, or
// ORDERS_NONE, for the panel.
byte improve_turn(void);

#endif // SEVENSHIPS_IMPROVE_H
