#include "city.h"
#include "unit.h"

byte city_count;
byte city_owner[MAX_CITIES];
byte city_ax[MAX_CITIES];
byte city_ay[MAX_CITIES];
byte city_name[MAX_CITIES];

// The human player is Mycenaean for now (vision.md §3), so these are the
// Mycenaean palace centres and towns of the Linear B tablets, roughly in
// order of importance, and then the AI's. No name is over ten characters,
// which is the width of the panel.
static const char *const human_names[] = {
    "Mycenae", "Tiryns", "Pylos", "Argos", "Thebes", "Athens",
    "Orchomenos", "Iolkos", "Midea", "Gla", "Sparta", "Knossos",
};
static const char *const ai_names[] = {
    "Hattusa", "Kanesh", "Sapinuwa", "Tarhuntasa", "Karkemish", "Ugarit",
};
#define HUMAN_NAMES (sizeof human_names / sizeof human_names[0])
#define AI_NAMES (sizeof ai_names / sizeof ai_names[0])

// How many cities each player has named, so the next gets the next name.
static byte named[2];

byte city_at(byte ax, byte ay)
{
    byte c;
    for (c = 0; c < city_count; ++c)
        if (city_ax[c] == ax && city_ay[c] == ay) return c;
    return CITY_NONE;
}

byte city_found(byte owner, byte ax, byte ay)
{
    byte c = city_count;
    if (c == MAX_CITIES || city_at(ax, ay) != CITY_NONE) return CITY_NONE;
    city_owner[c] = owner;
    city_ax[c] = ax;
    city_ay[c] = ay;
    city_name[c] = named[owner]++;
    ++city_count;
    unit_mark(ax, ay);
    return c;
}

const char *city_name_of(byte c)
{
    // Past the end of a list, names repeat; that is a problem for when a
    // player has founded a dozen cities, which nobody can yet.
    byte n = city_name[c];
    if (city_owner[c] == PLAYER_HUMAN) return human_names[n % HUMAN_NAMES];
    return ai_names[n % AI_NAMES];
}
