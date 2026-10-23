// What a menu item asks for, out of game.c (23 Oct 2026, when resident
// room ran out for the buildings): once a pick, in region 6 with the menus
// (src/region.h), sharing game.c's state through gameint.h as turn.c does.
#define game_menu_command game_menu_command_r6

#include "game.h"
#include "gameint.h"
#include "diplomacy.h"
#include "input.h"
#include "map.h"
#include "menu.h"
#include "minimap.h"
#include "popup.h"
#include "save.h"
#include "score.h"
#include "text.h"
#include "treasury.h"
#include "unit.h"

#pragma clang section text="r6text" rodata="r6rodata"

void game_menu_command(byte command)
{
    switch (command) {
    case MENU_RETIRE:
        if (popup_ask("Do you want to quit this game?")) {
            score_screen(SCORE_RETIRED);
            retired = 1;
            quit = 1;
        }
        break;
    case MENU_SCORE:
        score_screen(SCORE_NOW);
        draw_screen();
        break;
    case MENU_CITIES:
        cities_screen();
        break;
    case MENU_FINANCE:
        finance_screen();
        draw_screen();
        break;
#ifdef DEBUG_MENU
    case MENU_SHOW_MAP:
        // The whole minimap changes, so it is drawn again; the view is
        // redrawn below anyway.
        map_show ^= MAP_EXPLORED;
        minimap_init();
        message(map_show ? "Map shown" : "Map hidden", "");
        hover_dirty = 1;
        break;
    case MENU_PLAY_50:
        game_autoplay(AUTOPLAY_TURNS);
        break;
#endif
    case MENU_END_OF_TURN:
        game_end_wait ^= 1;
        message("End of Turn", game_end_wait ? "waits" : "automatic");
        break;
    case MENU_SAVE:
        message("Saving", "");
        message(save_write() ? "Game saved" : "Could not save", "");
        break;
    case MENU_FOUND_CITY:
    case MENU_EMPORIUM:
        unit_key('b');
        break;
    case MENU_FORTIFY:
        unit_key('f');
        break;
    case MENU_IRRIGATE:
        unit_key('i');
        break;
    case MENU_MINE:
        unit_key('m');
        break;
    case MENU_PASTURE:
        unit_key('p');
        break;
    case MENU_FISH:
        unit_key('i');
        break;
    case MENU_TOP_CITIES:
        open_top_cities();
        break;
    case MENU_DIPLOMACY:
        // Anybody in sight is met before the list, and greeted after it.
        greet |= diplomacy_contact();
        if (greet) show_met();
        diplomacy_screen();
        draw_screen();
        break;
    case MENU_KEYS:
        open_help(TEXT_HELP);
        break;
    default:
        if (command >= MENU_UNIT)
            open_help(TEXT_PEDIA_SETTLER + (command - MENU_UNIT - KIND_SETTLER));
        else if (command >= MENU_TERRAIN)
            open_help(TEXT_PEDIA_HILLS + (command - MENU_TERRAIN));
        break;
    }
    dirty = 1;
}
