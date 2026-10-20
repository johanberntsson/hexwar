// What a city's land yields, which of it the city works, and how it grows:
// milestone 6, after Civilization.
//
// **Every hexagon yields food, shields and trade** by its terrain (the
// tables in growth.c), plus 1 food for irrigation and shields for a mine.
// **A city of size n works its own hexagon and the n best of its six
// neighbours** -- best by three times the food, twice the shields and the
// trade, so growth comes first, or by the food above all if that would
// leave the city hungry -- picked afresh whenever they are asked for, so
// nothing about them is stored or saved. Citizens beyond the six
// work nothing but still eat. Cities are at least CITY_SPACING (4) apart,
// so two never share a hexagon.
//
// **Every citizen eats 2 food**, and the rest goes into the food box, which
// holds GROWTH_BOX(size); full, the city grows by one and the box empties.
// Empty and still short, it shrinks by one, down to size 1. Trade is
// worked out and shown but nothing uses it yet.
//
// **Per-turn code, in region 1** (src/region.h), called through stubs in
// src/dispatch.s; the results come back in the resident globals below.
#ifndef SEVENSHIPS_GROWTH_H
#define SEVENSHIPS_GROWTH_H

#include "mega65.h"

#define GROWTH_BOX(SIZE) (((SIZE) + 1) * 10)
#define GROWTH_MAX_SIZE 20      // GROWTH_BOX stays in a byte

// What city_work() found: the neighbours worked, bit DIR_* set for each,
// and the city's totals, its own hexagon included.
extern byte work_mask;
extern byte work_food;
extern byte work_shields;
extern byte work_trade;

// Pick the hexagons city c works and total what they yield, into the
// globals above.
void city_work(byte c);

// One turn of food for city c, at the end of the turn. GROWTH_NONE, or
// GROWTH_GREW or GROWTH_SHRANK if its size changed.
#define GROWTH_NONE 0
#define GROWTH_GREW 1
#define GROWTH_SHRANK 2
byte city_grow(byte c);

#endif // SEVENSHIPS_GROWTH_H
