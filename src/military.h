// Advisors -> Military (F2 on the map): how many units the human has of
// each kind, where they are, what the army costs and whom the human is at
// war with.
#ifndef SEVENSHIPS_MILITARY_H
#define SEVENSHIPS_MILITARY_H

#include "mega65.h"

// Until a key. Writes over the whole screen, so the caller puts the map
// back.
void military_screen(void);

#endif // SEVENSHIPS_MILITARY_H
