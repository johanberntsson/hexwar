// Merchant ships and emporia (milestone 9, vision.md section 3): "small
// coastal trade outposts on islands or foreign shores, without founding
// full cities, which tap into rare resources".
//
// **A merchant ship** (KIND_MERCHANT: a coastal city, cost 30, 3 moves,
// the shallows, no hold) founds an emporium on an empty stretch of coast
// next to it and is used up, as Civilization's caravan is when it founds
// a trade route. The emporium is on land with nobody on it, no resource and
// no improvement, next to the shallows and at least EMPORIUM_SPACING from
// every city, and its people **owns the goods on the six hexagons round
// it** (player_owns() in src/growth.c), improved or not: tin and copper
// for bronze, cedar for galleys, and anything for trade and the score.
//
// **On the map it is the fourth value of the improvement bits**,
// MAP_EMPORIUM, drawn as TILE_EMPORIUM, which no builder can change; whose
// it is, is a list here. An enemy standing on it at the end of a turn --
// a raider, a barbarian, or a people at war with its owner -- burns it:
// the Crisis's network fracture.
//
// **In region 4** (src/region.h), beside sea_route(), called through stubs.
#ifndef SEVENSHIPS_EMPORIUM_H
#define SEVENSHIPS_EMPORIUM_H

#include "mega65.h"

#define MAX_EMPORIA 16
#define EMPORIUM_SPACING 2

// The emporia, emporium_count of them; HIGH_BSS, saved.
extern byte emporium_count;
extern byte emporium_owner[MAX_EMPORIA];
extern byte emporium_ax[MAX_EMPORIA];
extern byte emporium_ay[MAX_EMPORIA];

// The human's b for merchant ship u, the active unit: an emporium on the
// coast next to it, where the most goods are round it, and the panel says
// how it went. game.c's.
void emporium_order(byte u);

// A computer player's merchant ship u, with the moves it has left: to the
// nearest coast next to a good its people has not got, copper and tin
// first, along the shallows, and an emporium there; with nowhere to go it
// explores as a fishing boat does.
void emporium_ai(byte u);

// Once a turn, from trade_turn(): emporia with an enemy on them are burnt.
void emporium_turn(void);

#endif // SEVENSHIPS_EMPORIUM_H
