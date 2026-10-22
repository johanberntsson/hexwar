// The treasury (22 Oct 2026): what the cities' trade is for, after
// Civilization's taxes.
//
// **Every turn each city's trade, less its corruption, goes into its
// owner's gold**: there is no science or luxury to split it with yet, so
// the tax rate is all of it. Corruption is Civilization's under Despotism,
// trade x distance to the capital x 3 / 100, all of it but a little with
// no capital (distance 32, as there): the capital is the first city a
// people founded, while it holds it (crisis_capital()). **Each trade deal
// is worth TREASURY_DEAL gold a turn to both partners**, in the place of
// Civilization's trade routes. **The expenses are the temples' upkeep**,
// 1 gold a turn each, as Civilization's buildings cost; with an empty
// treasury a temple is sold (src/culture.h).
//
// **Gold buys production**, at Civilization's price (city_buy_cost() in
// src/cityscreen.c): the human with b on the city screen; the computer
// players as Civilization's do, turning gold into shields at two to one in
// their cities' turns -- up to a third of the treasury in a city nobody
// is in, otherwise a 64th.
//
// **In region 6** (src/region.h), with the menus: once a turn from
// game_end_turn(), and Advisors -> Finance. (Region 5 until 23 Oct 2026.)
#ifndef SEVENSHIPS_TREASURY_H
#define SEVENSHIPS_TREASURY_H

#include "mega65.h"
#include "unit.h"

#define TREASURY_DEAL 2
#define TREASURY_MAX 30000      // Civilization's ceiling
#define TREASURY_START 50       // the human's, at the easiest level only

// Each player's gold; HIGH_BSS, saved.
extern word player_gold[PLAYERS];

// A turn's income and upkeep for every player, and the computer players'
// buying.
void treasury_turn(void);

// Advisors -> Finance: the treasury, each of the human's cities' trade,
// corruption and gold, the deals, and the income a turn, until a key.
// Writes over the whole screen, so the caller puts the map back.
void finance_screen(void);

#endif // SEVENSHIPS_TREASURY_H
