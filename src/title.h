// The title screen and its menu: Start Game, Load Saved Game, Quit.
#ifndef SEVENSHIPS_TITLE_H
#define SEVENSHIPS_TITLE_H

#include "mega65.h"

// The menu items, top to bottom, and what title_run() returns.
#define TITLE_START 0
#define TITLE_LOAD 1
#define TITLE_QUIT 2
#define TITLE_ITEMS 3
#define TITLE_NONE 0xff

// Clear the screen and put up the title, with "Loading" where the menu will
// go. Call before the disk is read, so there is something to look at.
void title_show(void);

// Run the menu until the player quits, starts a game or loads one, and
// return TITLE_QUIT, TITLE_START or TITLE_LOAD. Start Game goes on to
// choose a people and a difficulty level, which it leaves in player_civ[]
// and game_level (civ.h); Esc there goes back. The loading is the
// caller's, and if it failed, `load_failed` says so on the way back in.
// Needs input_init(). Leaves its last screen up; the caller clears it.
byte title_run(byte load_failed);

#endif // SEVENSHIPS_TITLE_H
