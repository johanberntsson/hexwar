// Popups: a box of text over the middle of the map, closed with a click,
// fire, Return, space or Esc.
//
// Build the text a piece at a time and then show it:
//
//      popup_begin();
//      popup_add("The tribes of the ");
//      popup_add(civ_people[civ]);
//      popup_show();
//
// or, for a fixed text, just popup("..."). The text is word-wrapped to fit,
// and "\n" starts a new line ("\n\n" leaves a blank one).
#ifndef HEXWAR_POPUP_H
#define HEXWAR_POPUP_H

#include "mega65.h"

void popup_begin(void);
void popup_add(const char *s);
void popup_add_num(word v);

// Show what has been added, and return once it is closed, with the map
// under it put back. Runs its own once-a-frame loop, reading the input
// every frame like everything else does; needs view_init().
void popup_show(void);

void popup(const char *text);

// A question with Yes and No buttons: 1 for Yes (the button, y or Return),
// 0 for No (the button, n or Esc). A click anywhere else is ignored.
byte popup_ask(const char *text);
// The same, for a question built with popup_begin() and popup_add().
byte popup_ask_shown(void);

#endif // HEXWAR_POPUP_H
