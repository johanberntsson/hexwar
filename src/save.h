// Saving and loading a game: one file, SEVENSHIPS.SAV, on drive 8.
//
// **The file is the program's own memory**, every array and counter a game
// is made of written out as it stands, in the order of the table in
// save.c, after a four-byte header: "SSV" and SAVE_VERSION. So a save only
// fits the build that wrote it, or one whose state is laid out the same;
// **anything added to a game's state goes in the table, and changing what
// is in it means a new SAVE_VERSION**, so that an old file is refused
// rather than read into the wrong places.
//
// Per-turn code, in a file of its own, like ai.c.
#ifndef SEVENSHIPS_SAVE_H
#define SEVENSHIPS_SAVE_H

#include "mega65.h"

#define SAVE_FILE_NAME "SEVENSHIPS.SAV"
#define SAVE_VERSION 16

// Write the game to the file, replacing the last one. Non-zero if it went.
// A full disk is not noticed: that is in the drive's error channel, which
// nothing here reads.
byte save_write(void);

// Read the file over the game game_init() set up. Non-zero if it went; if
// not, what was there is half overwritten and only fit for another
// game_init().
byte save_read(void);

#endif // SEVENSHIPS_SAVE_H
