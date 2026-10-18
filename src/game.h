// The game: the turn loop, the player's orders, and the panel that shows
// them.
#ifndef HEXWAR_GAME_H
#define HEXWAR_GAME_H

#include "mega65.h"

// Set up a new game: one settler for the human, on land near where the view
// starts. Needs the map loaded. Clears whatever a game before it left --
// units, cities, what was explored -- so it can be called again.
void game_init(void);

// What game_run() ended with.
#define GAME_QUIT 0             // Esc, or the game was over: back to BASIC
#define GAME_RETIRE 1           // Game/Retire: back to the title screen

// Play until the player quits, retires or the game is over.
byte game_run(void);

#endif // HEXWAR_GAME_H
