// Hexwar: a Civilization-style hex game for the MEGA65.
//
// This is the Calypsi rewrite of a 2022 cc65 proof of concept, which is kept
// under old-cc65/ until this reaches parity. The layout it is aiming at is
// basic/civ-basic.png, from the XC=BASIC version in basic/civ.bas.

#include "hexwar.h"
#include "map.h"
#include "mega65.h"
#include "screen.h"
#include "sheet.h"
#include "view.h"

// Where the view sits on the map, in screen characters. The cursor keys move
// this and nothing else; see view_redraw().
static word off_x;
static word off_y;

// The panel down the left ten columns. Text only, and it does not move, so it
// is drawn once. The lines are the ones basic/civ.bas puts there.
static void draw_panel(void)
{
    screen_putsxy(0, 0, "Game Orders Advisors World Civilopedia",
                  COLOR_LIGHTGREEN);
    screen_putsxy(0, 7, "30,000", COLOR_LIGHTGREEN);
    screen_putsxy(0, 8, "3300 BC", COLOR_LIGHTGREEN);
    screen_putsxy(0, 10, "Spartan", COLOR_LIGHTGREEN);
    screen_putsxy(0, 11, "Militia", COLOR_LIGHTGREEN);
    screen_putsxy(0, 12, "Moves: 1", COLOR_LIGHTGREEN);
    screen_putsxy(VIEW_LEFT + 20, SCREEN_ROWS - 1, "cursor keys - RETURN quits",
                  COLOR_LIGHTGREEN);
}

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
    byte key;
    byte dirty;

    mega65_io_enable();
    screen_init();

    // **Nothing here opens a file.** The tile sheet is linked in (build/tiles.s
    // from res/tiles.fci), so there is no ordering rule about doing disk I/O
    // before touching the memory map -- which is what shaped main() in the
    // cc65 build and in both of the other MEGA65 projects in this tree.
    if (!sheet_init()) fatal("tiles.fci is not the sheet this build expects");

    map_init();
    view_init();
    draw_panel();

    // Start somewhere inside the map rather than at its corner, so that the
    // first thing on screen shows the view clipping on all four sides.
    //
    // **START_OFF_X / START_OFF_Y are what let a headless run check
    // scrolling.** Nothing headless can press a cursor key, so the only way to
    // photograph the view at another offset is to start it there:
    // `make START_OFF_X=41` against a plain build must shift the map exactly
    // eight pixels left and leave the panel alone. Same trick as FLYNOW in
    // ~/commodore/SearchAndRescue.
    off_x = START_OFF_X;
    off_y = START_OFF_Y;
    dirty = 1;

    // The store starts blank; after that view_redraw() covers every byte of it
    // and deliberately does not clear, so that it can stay ahead of the beam.
    view_clear();

    for (;;) {
        if (dirty) {
            // Start the redraw the moment the beam leaves the map window, so
            // that it is rewriting rows the beam has already passed. Without
            // this the redraw lands in the middle of a displayed frame and the
            // map tears while a cursor key is held.
            view_wait_for_redraw();
            view_redraw(off_x, off_y);
            dirty = 0;
        }

        key = screen_getkey();
        if (!key) continue;

        // One character a step: eight pixels across, and one character row
        // down. A hexagon is 8 characters wide and 3 rows apart, so that is
        // eight sub-steps per column and three per row -- which is the
        // smoothness the vx/vy pair in the cc65 build was reaching for.
        switch (key) {
        case 157:                                   // cursor left
        case 'a':
            if (off_x > VIEW_MIN_OFF_X) { --off_x; dirty = 1; }
            break;
        case 29:                                    // cursor right
        case 'd':
            if (off_x < VIEW_MAX_OFF_X) { ++off_x; dirty = 1; }
            break;
        case 145:                                   // cursor up
        case 'w':
            if (off_y > VIEW_MIN_OFF_Y) { --off_y; dirty = 1; }
            break;
        case 17:                                    // cursor down
        case 's':
            if (off_y < VIEW_MAX_OFF_Y) { ++off_y; dirty = 1; }
            break;
        case 13:                                    // RETURN
            screen_go8bit();
            return 0;
        }
    }
}
