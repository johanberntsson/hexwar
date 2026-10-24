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
#ifndef SEVENSHIPS_POPUP_H
#define SEVENSHIPS_POPUP_H

#include "mega65.h"

void popup_begin(void);
void popup_add(const char *s);
void popup_add_num(word v);
// Text `id` from TEXT.PAK (src/text.h), with each {arg N} in it replaced by
// args[N]. Plain text, line breaks and arguments only.
void popup_add_text(byte id, const char *const *args);

// Show what has been added, and return once it is closed, with the map
// under it put back. Runs its own once-a-frame loop, reading the input
// every frame like everything else does; needs view_init().
void popup_show(void);

void popup(const char *text);

// A question with Yes and No buttons: 1 for Yes, 0 for No. A click on a
// button, y or n answer at once, and Esc is No. One button is highlighted,
// No to begin with; cursor left and right or the arrow move that, and
// Return or space picks it. A click anywhere else is ignored.
byte popup_ask(const char *text);
// The same, for a question built with popup_begin() and popup_add().
byte popup_ask_shown(void);
// The same with another pair of buttons, 1 for the first and 0 for the
// second, which is highlighted to begin with; each button's key is its
// first letter. A new pair is an entry in each of popup.c's tables.
#define POPUP_YES_NO 1
#define POPUP_MOVE_VIEW 2       // Move unit, View city
byte popup_choose(const char *text, byte pair);

#endif // SEVENSHIPS_POPUP_H
