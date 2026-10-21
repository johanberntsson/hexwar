// game.c's state and panel helpers, shared with turn.c (region 0), which
// has the end of a turn and the start of a game. Not for anything else:
// the names are game.c's own, prefixed here so that they do not clash.
#ifndef SEVENSHIPS_GAMEINT_H
#define SEVENSHIPS_GAMEINT_H

#include "mega65.h"

#define off_x game_off_x
#define off_y game_off_y
#define dirty game_dirty
#define contact game_contact
#define greet game_greet
#define hover_dirty game_hover_dirty
#define hover_unit game_hover_unit
#define game_over game_is_over
#define quit game_quit
#define retired game_retired
#define loaded game_loaded
#define selecting game_selecting
#define line game_line
#define line_clear game_line_clear
#define line_add game_line_add
#define line_add_num game_line_add_num
#define message game_message
#define draw_status game_draw_status
#define select_unit game_select_unit

extern word off_x, off_y;       // the view, in map characters
extern byte dirty;              // the view wants redrawing
extern byte contact;            // see whether a people has come in sight
extern byte greet;              // peoples just met, greeted after a redraw
extern byte hover_dirty;        // the readout of what is under the arrow
extern byte hover_unit;
extern byte game_over;
extern byte quit;
extern byte retired;
extern byte loaded;
extern byte selecting;          // tile select is up
extern char line[41];           // the panel's line being built

void line_clear(void);
void line_add(const char *s);
void line_add_num(word v);
// Two lines of message in the panel. `second` may be `line`.
void message(const char *first, const char *second);
void draw_status(void);
// Hand the orders to the next unit after `after` that wants some.
void select_unit(byte after);

// In turn.c, region 0.
void game_end_turn(void);
// Debug: Play 50 Turns; does nothing without DEBUG_MENU.
#ifndef AUTOPLAY_TURNS
#define AUTOPLAY_TURNS 50       // `make AUTOPLAY_TURNS=250` for long tests
#endif
void game_autoplay(byte n);

#endif // SEVENSHIPS_GAMEINT_H
