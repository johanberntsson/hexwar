// What a city's land yields, which of it the city works, and how it grows:
// milestone 6, after Civilization.
//
// **Every hexagon yields food, shields and trade** by its terrain (the
// tables in growth.c), plus 1 food for irrigation and shields for a mine,
// and a shield on half the grassland and floodplain, on Civilization's
// fixed pattern.
// **A city of size n works its own hexagon and the n best of its RING**
// (map.h: two rings, 18 hexagons, Civilization's fat cross) -- best by
// three times the food, twice the shields and the trade, so growth comes
// first, or by the food above all if that would leave the city hungry --
// picked afresh whenever they are asked for. **Unless the player has
// picked them** on the city screen (src/cityscreen.c): then they are kept in
// city_worked[], with CITY_MANUAL, and only growth (the best hexagon left)
// and shrinking (the worst goes) change them. Citizens working nothing --
// beyond the eighteen, or taken off a hexagon by hand -- are idle, and
// still eat. **A hexagon in reach of two cities is the nearer one's**, the
// earlier in the list on a tie: the other cannot work it.
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
#include "map.h"

#define GROWTH_BOX(SIZE) (((SIZE) + 1) * 10)
#define GROWTH_MAX_SIZE 20      // GROWTH_BOX stays in a byte

// What city_work() found, by the hexagons of the RING (map.h): which are
// worked, whose each is (the city's own, another city's, or CITY_NONE off
// the map) and where; and the city's totals, its own hexagon included.
extern byte work_on[RING];
extern byte work_owner[RING];
extern byte work_ring_x[RING], work_ring_y[RING];
extern byte work_food;
extern byte work_shields;
extern byte work_trade;
extern byte work_idle;          // citizens working no hexagon

// **Bronze** (milestone 7b, vision.md section 2): whether player p owns
// resource r -- on one of its cities, or in its RING and improved the way
// that resource is (mined copper and tin, irrigated cattle, olives and
// wine; grain, fish, cedar and dye need nothing), a builder's work -- and
// whether it has it, owned or received in trade (milestone 9, trade.h).
// Copper and tin together are bronze, which the spearman needs. Tin is in
// a few nodes at the top and bottom of the map, so most peoples have to
// trade for it. Only what a player owns can be traded on.
byte player_owns(byte p, byte r);
byte player_has(byte p, byte r);
#define player_bronze(P)                                                    \
    (player_has((P), RES_COPPER) && player_has((P), RES_TIN))

// What hexagon (ax, ay) yields, worked, into these.
extern byte hex_food, hex_shields, hex_trade;
void hex_yield(byte ax, byte ay);

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
