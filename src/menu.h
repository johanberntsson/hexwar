// The menu bar along the top row of the map screen -- Game, Orders, Debug,
// World, Help, after civ1-reference/civ1-pic01.png -- and its dropdowns.
// Debug is there only in a DEBUG_MENU build, which `make release` is not.
#ifndef SEVENSHIPS_MENU_H
#define SEVENSHIPS_MENU_H

#include "mega65.h"

// What a dropdown item asks for.
#define MENU_NONE 0
#define MENU_RETIRE 1
#define MENU_SHOW_MAP 2
#define MENU_FOUND_CITY 3
#define MENU_FORTIFY 4
#define MENU_TOP_CITIES 5
#define MENU_KEYS 6
#define MENU_PLAY_50 7
#define MENU_SAVE 8
#define MENU_IRRIGATE 9
#define MENU_MINE 10
#define MENU_DIPLOMACY 11
#define MENU_OPTIONS 12         // opens Options; menu_click() never returns it
#define MENU_END_OF_TURN 13

// What the Orders menu offers, for the unit that has the orders: up to two
// of MENU_FOUND_CITY, MENU_FORTIFY, MENU_IRRIGATE and MENU_MINE, MENU_NONE
// for none -- with none at all, Orders does not open. Set it before
// menu_click() or menu_key().
void menu_orders(byte command, byte second);

// Write the bar into row 0.
void menu_draw(void);

// The arrow has been clicked on row 0. Opens the dropdown of the title under
// it, if that has any items, and runs its own once-a-frame loop until an
// item is picked (returned) or the player clicks elsewhere or presses Esc
// (MENU_NONE). The dropdown is drawn over the minimap and the top of the map
// window and puts both back when it closes, but hides sprites 2 and 3, the
// minimap's outline: **the caller redraws the view**, which puts them back.
byte menu_click(void);

// The same from the keyboard: KEY_MEGA and a title's first letter, as Alt is
// on a PC, opens that menu with its first item selected. Any other key, or a
// menu with no items, opens nothing and returns MENU_NONE. In either, cursor
// left and right go on to the next menu that has items.
byte menu_key(byte key);

#endif // SEVENSHIPS_MENU_H
