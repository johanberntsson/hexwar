// In region 6 (src/region.h): called through the stub in src/dispatch.s,
// under this name here.
#define culture_built culture_built_r6

#include "culture.h"
#include "city.h"
#include "civ.h"
#include "diplomacy.h"
#include "popup.h"

#pragma clang section text="r6text" rodata="r6rodata"

HIGH_BSS byte wonders_built;
HIGH_BSS word player_legacy[PLAYERS];

// Nothing to announce during Debug/Play 50 Turns, as crisis.c.
#ifdef DEBUG_MENU
#define QUIET diplomacy_auto
#else
#define QUIET 0
#endif

void culture_built(byte c, byte kind)
{
    byte p = city_owner[c], i;

    city_build[c] = KIND_WARRIOR;           // what next is the player's
    if (p >= PLAYERS) return;               // the rebels' (crisis.c)
    if (kind == KIND_TEMPLE) {
        city_culture[c] |= CITY_TEMPLE;
        player_legacy[p] += LEGACY_TEMPLE;
        return;
    }
    city_culture[c] |= CITY_LION_GATE;
    wonders_built |= WONDER_LION_GATE;
    player_legacy[p] += LEGACY_WONDER;
    for (i = 0; i < city_count; ++i)
        if (city_build[i] == KIND_LION_GATE) city_build[i] = KIND_WARRIOR;
    if (QUIET) return;
    popup_begin();
    popup_add("The ");
    popup_add(player_people(p));
    popup_add(" have built the Lion Gate in ");
    popup_add(city_name_of(c));
    popup_add(": a wonder of the age, and the walls of their city.");
    popup_show();
}
