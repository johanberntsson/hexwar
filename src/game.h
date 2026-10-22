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
#define GAME_QUIT 0             // Esc: back to BASIC
#define GAME_RETIRE 1           // Game/Retire, or the game is over: the score
                                // has been shown; back to the title screen

// Play until the player quits, retires or the game is over.
byte game_run(void);

// After game_init(), read the saved game over it (save.c); non-zero if it
// went. game_run() then carries on from there, without the opening popup.
byte game_load(void);

// The DIR_* a moving key stands for -- 0 - p * : = and, in xemu, the
// keypad -- or DIR_NONE. The city screen uses them too.
byte key_dir(byte key);

// Game -> Options -> End of Turn, Civilization's: ticked, the game waits
// for Return at the end of every turn; not (the default), it waits only if
// no unit was given orders during the turn. A preference, not saved.
extern byte game_end_wait;

// **The calendar** (milestone 10): from 1600 BC, two years a turn until
// turn GAME_SLOW_TURN (1250 BC), then one, so that the Crisis is due at
// GAME_CRISIS_TURN (1200 BC) and the game ends after GAME_LAST_TURN
// (1026 BC) -- Civilization's 400 turns, the last 175 of them the Collapse.
#define GAME_SLOW_TURN 176
#define GAME_CRISIS_TURN 226
#define GAME_LAST_TURN 400

// The turn, from 1, and the year, BC; for save.c.
extern word game_turn;
extern word game_year;

#endif // SEVENSHIPS_GAME_H
