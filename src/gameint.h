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
#define hover_ax game_hover_ax
#define hover_ay game_hover_ay
#define sel_ax game_sel_ax
#define sel_ay game_sel_ay
#define line_len game_line_len
#define line_add_unit game_line_add_unit
#define pointer_hex game_pointer_hex
#define show_met game_show_met
#define unit_key game_unit_key
#define draw_screen game_draw_screen
#define open_help game_open_help
#define open_top_cities game_open_top_cities

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
extern byte sel_ax, sel_ay;     // ... and where its ring is
extern byte hover_ax, hover_ay; // the readout's hexagon, while hover_unit
extern byte line_len;
extern char line[41];           // the panel's line being built

void line_clear(void);
void line_add(const char *s);
void line_add_num(word v);
// "Hittite Warrior": a unit's people and kind.
void line_add_unit(byte owner, byte kind);
// The hexagon under the arrow's tip into (ax, ay); 0 if the arrow is not
// over the map window.
byte pointer_hex(byte *ax, byte *ay);
// Two lines of message in the panel. `second` may be `line`.
void message(const char *first, const char *second);
void draw_status(void);
// Hand the orders to the next unit after `after` that wants some.
void select_unit(byte after);

// For command.c (region 6), out of game.c: bring the newly met into view;
// a key for the unit with the orders; the map screen put back whole;
// help screen `id`, then the map; Top Five Cities, then the map.
void show_met(void);
void unit_key(byte key);
void draw_screen(void);
void open_help(byte id);
void open_top_cities(void);

// In command.c, region 6: do what a menu item asks for, picked by click
// or by key. Whatever it does, the view is redrawn afterwards, which puts
// back the minimap's outline the dropdown hid. Out of game.c on 23 Oct
// 2026, for resident room.
void game_menu_command(byte command);

// In cities.c, region 6: Advisors -> Cities, the human's cities in a list,
// and the city screen of the one picked, back to the list after it; the
// map screen put back at the end. Called from command.c, no stub.
void cities_screen(void);

// In hover.c, region 0: the readout of what is under the arrow, or the
// ring, or hover_ax/ay, at the end of the top row.
void game_draw_hover(void);

// In turn.c, region 0.
void game_end_turn(void);
// Debug: Play 50 Turns; does nothing without DEBUG_MENU.
#ifndef AUTOPLAY_TURNS
#define AUTOPLAY_TURNS 50       // `make AUTOPLAY_TURNS=250` for long tests
#endif
void game_autoplay(byte n);

#endif // SEVENSHIPS_GAMEINT_H
