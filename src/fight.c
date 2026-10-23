// The popups that say how a fight of the human's went (24 Oct 2026, the
// user's: before them the panel said a line or two, and a city lost
// during the other players' turns said nothing anyone noticed).

// In region 2 (src/region.h): called through the stub in src/dispatch.s.
#define fight_show fight_show_r2

#include "fight.h"
#include "city.h"
#include "civ.h"
#include "combat.h"
#include "popup.h"
#include "unit.h"

#pragma clang section text="r2text" rodata="r2rodata"

// "a Hittite Warrior", "an Egyptian Spearman".
static void foe(byte i)
{
    const char *adj = player_adjective(fight_foe[i]);
    char c = adj[0];
    popup_add(c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U'
              ? "an " : "a ");
    popup_add(adj);
    popup_add(" ");
    popup_add(unit_kind_name[fight_foe_kind[i]]);
}

static void city(byte i)
{
    popup_add(civ_city_name(player_civ[fight_founder[i]], fight_name[i]));
}

// " in Mycenae", where there is a city.
static void in_city(byte i)
{
    if (fight_founder[i] == CITY_NONE) return;
    popup_add(" in ");
    city(i);
}

static void yours(byte i)
{
    popup_add("Your ");
    popup_add(unit_kind_name[fight_kind[i]]);
}

// " and 2 more", when a whole stack went with it.
static void more(byte i)
{
    if (fight_lost[i] < 2) return;
    popup_add(" and ");
    popup_add_num(fight_lost[i] - 1);
    popup_add(" more");
}

static void killed(byte kind)
{
    popup_add(KIND_IS_SHIP(kind) ? "sunk" : "destroyed");
}

void fight_show(byte i)
{
    byte what = fight_what[i];

    popup_begin();
    popup_add(what == FIGHT_WON || what == FIGHT_TOOK || what == FIGHT_HELD
              ? "Victory!\n\n" : "Defeat!\n\n");
    switch (what) {
    case FIGHT_WON:
        // Your Chariot destroyed a Barbarian Warrior and 1 more.
        yours(i);
        popup_add(" ");
        killed(fight_foe_kind[i]);
        popup_add(" ");
        foe(i);
        more(i);
        in_city(i);
        break;
    case FIGHT_LOST:
        // Your Warrior was destroyed attacking a Hittite Spearman.
        yours(i);
        popup_add(" was ");
        killed(fight_kind[i]);
        popup_add(" attacking ");
        foe(i);
        in_city(i);
        break;
    case FIGHT_TOOK:
        // Your Warrior captured Hattusa from the Hittites.
        yours(i);
        popup_add(" captured ");
        city(i);
        popup_add(" from the ");
        popup_add(player_people(fight_foe[i]));
        break;
    case FIGHT_HELD:
        // Your Spearman in Mycenae held off a Barbarian Raider.
        yours(i);
        in_city(i);
        popup_add(" held off ");
        foe(i);
        break;
    case FIGHT_FELL:
        // Your Warrior in Mycenae was destroyed by an Egyptian Chariot.
        yours(i);
        in_city(i);
        more(i);
        popup_add(fight_lost[i] < 2 ? " was " : " were ");
        killed(fight_kind[i]);
        popup_add(" by ");
        foe(i);
        break;
    default:
        // Mycenae was burnt by a Barbarian Raider.
        city(i);
        popup_add(fight_foe[i] == PLAYER_BARBARIAN
                  ? " was burnt to the ground by "
                  : " was captured by ");
        foe(i);
        break;
    }
    popup_add(".");
    popup_show();
}
