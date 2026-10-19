#include "civ.h"
#include "unit.h"

#if PLAYERS != CIVS
#error "every people is a player: player_civ[] and civ_choose() assume it"
#endif
#if PLAYER_BARBARIAN != CIVS
#error "the barbarians are the line after the peoples in list_peoples.txt"
#endif

byte game_level;
byte player_civ[CIVS] = {
    CIV_MYCENAEANS, CIV_HITTITES, CIV_EGYPTIANS, CIV_CANAANITES
};

// res/text/list_cities.txt has this many a people, one after another.
#define CITY_NAMES 12

void civ_choose(byte civ)
{
    byte p;
    for (p = 0; p < CIVS; ++p) player_civ[p] = (byte)((civ + p) % CIVS);
}

const char *civ_city_name(byte civ, byte n)
{
    byte line = civ * CITY_NAMES + n % CITY_NAMES;
    return text_line(TEXT_LIST_CITIES, line);
}

// The barbarians are the line after the peoples.
static byte civ_of(byte p)
{
    return p == PLAYER_BARBARIAN ? CIVS : player_civ[p];
}

const char *player_adjective(byte p)
{
    return civ_adjective(civ_of(p));
}

const char *player_people(byte p)
{
    return civ_people(civ_of(p));
}
