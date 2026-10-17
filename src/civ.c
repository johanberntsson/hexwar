#include "civ.h"
#include "unit.h"

const char *const civ_people[CIVS] = {
    "Mycenaeans", "Hittites", "Egyptians", "Canaanites",
};
const char *const civ_adjective[CIVS] = {
    "Mycenaean", "Hittite", "Egyptian", "Canaanite",
};
const char *const civ_note[CIVS] = {
    "Sea traders of the Aegean: wine, oil and fish, but little grain land",
    "Chariots and Anatolian copper: strong on land, far from the sea",
    "The grain and manpower of the Nile, but short of cedar and tin",
    "Rich coastal cities with cedar and purple dye, but weak armies",
};

// One famous ruler each, from anywhere in the period rather than all alive
// in 1600 BC. **No Mycenaean king's name survives for certain** -- the Linear
// B tablets name the office, wanax, not the man -- so theirs is Homer's.
// Ammurapi, the last king of Ugarit, would be the other choice for the
// Canaanites, but he is the one whose letter is the title.
const char *const civ_leader[CIVS] = {
    "Agamemnon", "Suppiluliuma", "Thutmose III", "Niqmaddu II",
};
const char *const civ_title[CIVS] = {
    "Wanax of Mycenae", "Great King of Hatti", "King of Egypt",
    "King of Ugarit",
};

const char *const level_name[LEVELS] = {
    "Scribe", "Governor", "Prince", "King", "Great King",
};
const char *const level_note[LEVELS] = {
    "The easiest: for learning the game",
    "Rivals who are slow to grow and slow to anger",
    "A fair contest",
    "Rivals who grow fast and remember slights",
    "The hardest: rivals who call no one brother",
};

byte game_level;
byte player_civ[2] = { CIV_MYCENAEANS, CIV_HITTITES };

// Each people's palace centres and towns, roughly in order of importance.
// No name is over ten characters, which is the width of the panel.
#define CITY_NAMES 12
static const char *const city_names[CIVS][CITY_NAMES] = {
    {   // the Linear B tablets' palaces and towns
        "Mycenae", "Tiryns", "Pylos", "Argos", "Thebes", "Athens",
        "Orchomenos", "Iolkos", "Midea", "Gla", "Sparta", "Knossos",
    },
    {
        "Hattusa", "Kanesh", "Sapinuwa", "Tarhuntasa", "Karkemish", "Samuha",
        "Nerik", "Arinna", "Kussara", "Sarissa", "Tapikka", "Zippalanda",
    },
    {   // the Egyptians' own names: Waset is Thebes, Men-nefer Memphis
        "Waset", "Men-nefer", "Iunu", "Abdju", "Avaris", "Per-Ramesu",
        "Bubastis", "Sais", "Nekheb", "Buto", "Tjaru", "Akhmim",
    },
    {
        "Ugarit", "Byblos", "Sidon", "Tyre", "Arwad", "Megiddo",
        "Hazor", "Ashkelon", "Gaza", "Jaffa", "Akko", "Kadesh",
    },
};

void civ_choose(byte civ)
{
    player_civ[PLAYER_HUMAN] = civ;
    player_civ[PLAYER_AI] = (byte)((civ + 1) % CIVS);
}

const char *civ_city_name(byte civ, byte n)
{
    return city_names[civ][n % CITY_NAMES];
}
