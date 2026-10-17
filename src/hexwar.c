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

// Which map cell is under the window's top left corner, as "ax,ay" right-
// aligned at the end of the top row. The window is 60 characters across and a
// map column is 4, so this changes every other step left or right; a hexagon
// row is 3 character rows, so up and down change ay every six steps.
static void draw_position(void)
{
    static char text[8];
    byte ax, ay;
    byte i = sizeof text - 1;

    map_cell_at(off_x, off_y, &ax, &ay);
    // Built backwards from the end, so no printf is linked for two numbers.
    // Padded on the left, so a shorter number overwrites a longer one.
    text[i] = 0;
    do { text[--i] = '0' + ay % 10; ay /= 10; } while (ay);
    text[--i] = ',';
    do { text[--i] = '0' + ax % 10; ax /= 10; } while (ax);
    while (i) text[--i] = ' ';
    screen_putsxy(SCREEN_COLUMNS - (sizeof text - 1), 0, text, COLOR_WHITE);
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

    // The tile sheet is linked in (build/tiles.s from res/tiles.fci); the map
    // comes off the disk. The ROM is never banked out, so there is no ordering
    // rule about doing disk I/O before touching the memory map -- which is
    // what shaped main() in the cc65 build and in both of the other MEGA65
    // projects in this tree. Colour RAM was moved out of CBDOS's way in
    // screen_init(), so the display survives the read.
    if (!sheet_init()) fatal("tiles.fci is not the sheet this build expects");
    if (!map_load()) fatal("cannot read " MAP_FILE_NAME);

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
            draw_position();
            dirty = 0;
        }

        key = screen_getkey();
        if (!key) continue;

        // Two characters a step across and one row down, because that is the
        // same distance on the glass: at H640/V200 a character is twice as
        // tall as it is wide. One character across moved the view at half the
        // speed of a step down. A hexagon is 8 characters wide and 3 rows
        // apart, so that is four sub-steps per column and three per row.
        //
        // Across there is no limit: the map wraps east-west, so off_x runs
        // round 0..MAP_WIDTH_CHARS-1 and view_redraw() draws across the seam.
        switch (key) {
        case 157:                                   // cursor left
        case 'a':
            if (off_x < 2) off_x += MAP_WIDTH_CHARS;
            off_x -= 2;
            dirty = 1;
            break;
        case 29:                                    // cursor right
        case 'd':
            off_x += 2;
            if (off_x >= MAP_WIDTH_CHARS) off_x -= MAP_WIDTH_CHARS;
            dirty = 1;
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
