// Seven Ships of the Enemy: a Civilization-style hex game for the MEGA65.
//
// This is the Calypsi rewrite of a 2022 cc65 proof of concept. The layout
// started from basic/civ-basic.png, from the XC=BASIC version in
// basic/civ.bas, with the map moved right to leave room on the left as in
// civ1-reference. Both are in git history only; see CLAUDE.md.

#include "sevenships.h"
#include "game.h"
#include "input.h"
#include "map.h"
#include "mega65.h"
#include "screen.h"
#include "sheet.h"
#include "title.h"
#include "view.h"

// A message on an otherwise empty screen, for the things that stop the
// program before the game starts. Written with screen_putsxy() on the
// game's own display rather than puts() on BASIC's, which pulled about
// 1.1 KB of stdio into the link for this alone.
static void fatal(const char *message)
{
    screen_clear();
    screen_putsxy(0, 0, "sevenships: ", COLOR_WHITE);
    screen_putsxy(12, 0, message, COLOR_WHITE);
    while (1) continue;
}

int main(void)
{
    mega65_io_enable();
    screen_save();
    screen_init();
#ifndef NO_TITLE
    title_show();
#endif

    // Both the tile sheet and the map come off the disk. The ROM is never
    // banked out, so there is no ordering rule about doing disk I/O before
    // touching the memory map -- which is what shaped main() in the cc65
    // build and in hexgame. Colour RAM was moved out of CBDOS's way in
    // screen_init(), so the display survives the reads.
    if (!sheet_load()) fatal("cannot read " SHEET_FILE_NAME
                             " or it is not the sheet this build expects");
    if (!map_load()) fatal("cannot read " MAP_FILE_NAME);

    input_init();

    // Round again for every game retired from, back to the title screen.
    for (;;) {
        // The title's menu is on the plain text screen, so it has to be done
        // with before view_init() takes the map window's cells -- which it
        // does again for every game. `make NO_TITLE=1` goes straight to the
        // map, which is what a headless check of the view wants, and
        // straight into a new game after a retire.
#ifndef NO_TITLE
        title_show();
        if (title_run() == TITLE_QUIT) break;
        screen_clear();
#endif

        view_init();
        game_init();

        // The store starts blank; after that view_redraw() covers every byte
        // of it and deliberately does not clear, so that it can stay ahead of
        // the beam.
        view_clear();

        if (game_run() == GAME_QUIT) break;
    }

    input_shutdown();
    screen_restore();
    bank_basic_in();
    return 0;
}
