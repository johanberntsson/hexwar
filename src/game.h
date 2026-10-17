// The game: the turn loop, the player's orders, and the panel that shows
// them. TODO.txt's milestone 1.
#ifndef HEXWAR_GAME_H
#define HEXWAR_GAME_H

#include "mega65.h"

// Set up a new game: one settler for the human, on land near where the view
// starts. Needs the map loaded.
void game_init(void);

// Play until the player quits or the game is over. Returns to main(), which
// puts the screen back for BASIC.
void game_run(void);

#endif // HEXWAR_GAME_H
