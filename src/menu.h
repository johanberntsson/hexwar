// The menu bar along the top row of the map screen -- Game, Orders,
// Advisors, World, Civilopedia, as in civ1-reference/civ1-pic01.png -- and
// its dropdowns. Only Game has anything in it yet.
#ifndef HEXWAR_MENU_H
#define HEXWAR_MENU_H

#include "mega65.h"

// What a dropdown item asks for.
#define MENU_NONE 0
#define MENU_RETIRE 1

// Write the bar into row 0.
void menu_draw(void);

// The arrow has been clicked on row 0. Opens the dropdown of the title under
// it, if that has any items, and runs its own once-a-frame loop until an
// item is picked (returned) or the player clicks elsewhere or presses Esc
// (MENU_NONE). The dropdown is drawn over the minimap and the top of the map
// window and puts both back when it closes, but hides sprites 2 and 3, the
// minimap's outline: **the caller redraws the view**, which puts them back.
byte menu_click(void);

#endif // HEXWAR_MENU_H
