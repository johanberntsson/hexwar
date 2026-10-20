// The game: the turn loop, the player's orders, and the panel that shows
// them.
#ifndef SEVENSHIPS_GAME_H
#define SEVENSHIPS_GAME_H

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

// After game_init(), read the saved game over it (save.c); non-zero if it
// went. game_run() then carries on from there, without the opening popup.
byte game_load(void);

// The DIR_* a moving key stands for -- 0 - p * : = and, in xemu, the
// keypad -- or DIR_NONE. The city screen uses them too.
byte key_dir(byte key);

// The turn, from 1, and the year, BC; for save.c.
extern word game_turn;
extern word game_year;

#endif // SEVENSHIPS_GAME_H
