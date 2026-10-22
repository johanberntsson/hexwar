// The help screen: every key and click the map screen answers to, opened by
// HELP or F1; and, the same way, the Civilopedia's pages (Help/Terrain
// Types), each a text in a frame.
#ifndef SEVENSHIPS_HELP_H
#define SEVENSHIPS_HELP_H

#include "mega65.h"

// Show text `id` -- TEXT_HELP for the keys, or a page, TEXT_PEDIA_HILLS +
// a terrain -- until the player leaves. Runs its own once-a-frame loop, like
// the city screen, and writes over the whole screen and hides sprites 2 and
// 3, so **the caller puts the map screen back afterwards**.
void help_screen(byte id);

#endif // SEVENSHIPS_HELP_H
