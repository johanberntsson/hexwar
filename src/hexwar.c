// Hexwar: a Civilization-style hex game for the MEGA65.
//
// This is the Calypsi rewrite of a 2022 cc65 proof of concept, which is kept
// under old-cc65/ until this reaches parity. The layout it is aiming at is
// basic/civ-basic.png, from the XC=BASIC version in basic/civ.bas.

#include "hexwar.h"
#include "game.h"
#include "input.h"
#include "map.h"
#include "mega65.h"
#include "screen.h"
#include "sheet.h"
#include "view.h"

// A message on an otherwise empty 8-bit screen, for the things that stop the
// program before it has a display of its own.
static void fatal(const char *message)
{
    screen_go8bit();
    puts("hexwar: ");
    puts(message);
    while (1) continue;
}

int main(void)
{
    mega65_io_enable();
    screen_init();

    // Both the tile sheet and the map come off the disk. The ROM is never
    // banked out, so there is no ordering rule about doing disk I/O before
    // touching the memory map -- which is what shaped main() in the cc65
    // build and in hexgame. Colour RAM was moved out of CBDOS's way in
    // screen_init(), so the display survives the reads.
    if (!sheet_load()) fatal("cannot read " SHEET_FILE_NAME
                             " or it is not the sheet this build expects");
    if (!map_load()) fatal("cannot read " MAP_FILE_NAME);

    view_init();
    input_init();
    game_init();

    // The store starts blank; after that view_redraw() covers every byte of it
    // and deliberately does not clear, so that it can stay ahead of the beam.
    view_clear();

    game_run();

    input_shutdown();
    screen_go8bit();
    return 0;
}
