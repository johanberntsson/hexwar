// The help screen: every key and click the map screen answers to, opened by
// HELP or F1.
#ifndef HEXWAR_HELP_H
#define HEXWAR_HELP_H

#include "mega65.h"

// Show the keys until the player leaves. Runs its own once-a-frame loop, like
// the city screen, and writes over the whole screen and hides sprites 2 and
// 3, so **the caller puts the map screen back afterwards**.
void help_screen(void);

#endif // HEXWAR_HELP_H
