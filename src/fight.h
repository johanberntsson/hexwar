// What happened in a fight of the human's, in a popup: who won, with what,
// against whom, and where. The fights are noted by combat_attack()
// (combat.h); game.c brings each into view and calls fight_show() for it.
#ifndef SEVENSHIPS_FIGHT_H
#define SEVENSHIPS_FIGHT_H

#include "mega65.h"

// The popup for fight i, until it is closed.
void fight_show(byte i);

#endif // SEVENSHIPS_FIGHT_H
