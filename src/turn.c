// The turn's end and a game's start, out of game.c (21 Oct 2026, to make
// resident room for the galley): code that runs once a turn or once a
// game, in region 0 (src/region.h), which is the window's own and needs no
// copying. It shares game.c's state through gameint.h.
#define game_init game_init_r0
#define game_end_turn game_end_turn_r0
#define game_autoplay game_autoplay_r0

#include "game.h"
#include "gameint.h"
#include "ai.h"
#include "city.h"
#include "civ.h"
#include "combat.h"
#include "crisis.h"
#include "diplomacy.h"
#include "growth.h"
#include "improve.h"
#include "input.h"
#include "map.h"
#include "sheet.h"
#include "unit.h"
#include "view.h"

#pragma clang section text="r0text" rodata="r0rodata"

#define START_YEAR 1600         // BC; the calendar is in game.h

// The end of a turn: the computer players and the barbarians move, the
// diplomacy and the trade are settled, the builders work, and every city
// grows and produces, the computer's included, so that a unit finished
// now is ready for the turn about to begin. With more than one
// city finishing, the panel names the last.
void game_end_turn(void)
{
    byte c, kind;

    message("", "");
    news = NEWS_NONE;
    for (c = PLAYER_FIRST_AI; c < PLAYERS; ++c) ai_turn(c);
    ai_barbarians(game_turn);
    diplomacy_turn();
    kind = improve_turn();
    if (kind != ORDERS_NONE)
        message(kind == ORDERS_IRRIGATE ? "Irrigation" : "Mine", "finished");
    crisis_turn();              // before the cities grow, for the drought
    for (c = 0; c < city_count; ++c) {
        // Food before shields, so a city that grows works its new hexagon
        // at once; and a city built up to a settler can let it go.
        kind = city_grow(c);
        if (kind != GROWTH_NONE && city_owner[c] == PLAYER_HUMAN) {
            line_clear();
            if (kind == GROWTH_GREW) {
                line_add("grew to ");
                line_add_num(city_size[c]);
            } else {
                line_add("famine");
            }
            message(city_name_of(c), line);
        }
        kind = city_produce(c);
        if (kind != KIND_NONE && city_owner[c] == PLAYER_HUMAN) {
            line_clear();
            line_add("built ");
            line_add(unit_kind_name[kind]);
            message(city_name_of(c), line);
            dirty = 1;
        }
    }
    // Being attacked matters more than what was built.
    if (news != NEWS_NONE) {
        static const char *const what[] = {
            "", "repelled", "a unit lost", "a city lost"
        };
        line_clear();
        line_add(player_adjective(news_owner));
        line_add(" attack");
        message(line, what[news]);
    }
    dirty = 1;
    hover_dirty = 1;
    contact = 1;
    // **The end** (milestone 10): after the last turn, or once the human's
    // people is the only one left -- and, as before, once it is out, which
    // select_unit() sees. game_run() then shows the score (score.h).
    for (c = PLAYER_FIRST_AI; c < PLAYERS && !player_alive(c); ++c)
        ;
    if (game_turn == GAME_LAST_TURN || c == PLAYERS) {
        game_over = 1;
    } else {
        game_year -= game_turn < GAME_SLOW_TURN ? 2 : 1;
        ++game_turn;
    }
    unit_begin_turn(PLAYER_HUMAN);
    draw_status();
    select_unit(UNIT_NONE);
}

#ifdef DEBUG_MENU
// Debug: the computer plays the human's side for `n` turns, through the
// same ai_turn() as everybody else, or until Esc or the game is over. The
// panel's turn and the minimap keep up as it goes; the view is redrawn
// once, at the end, by the caller.
void game_autoplay(byte n)
{
    diplomacy_auto = 1;
    unit_active = UNIT_NONE;    // so that handing back does not end a turn
    while (n-- && !game_over) {
        ai_turn(PLAYER_HUMAN);
        game_end_turn();
        if (input_poll() == KEY_ESC) break;
    }
    diplomacy_auto = 0;
}
#else
// Not called without the Debug menu, but its stub in dispatch.s is there.
void game_autoplay(byte n)
{
    (void)n;
}
#endif

// The land cell nearest (cx, cy) of the given terrain, searching outwards in
// square rings of the array. Not hexagon distance, but this only has to find
// somewhere sensible to start.
static byte find_terrain(byte cx, byte cy, byte want, byte *rx, byte *ry)
{
    int r, dx, dy, y;
    byte x;

    for (r = 0; r < MAP_COLUMNS / 2; ++r) {
        for (dy = -r; dy <= r; ++dy) {
            y = (int)cy + dy;
            if (y < 0 || y >= MAP_ROWS) continue;
            for (dx = -r; dx <= r; ++dx) {
                if (dx != -r && dx != r && dy != -r && dy != r) continue;
                x = (byte)((int)cx + dx + MAP_COLUMNS) & (MAP_COLUMNS - 1);
                if (MAP_TERRAIN(x, y) == want) {
                    *rx = x;
                    *ry = (byte)y;
                    return 1;
                }
            }
        }
    }
    return 0;
}

void game_init(void)
{
    byte cx, cy, ax, ay;

    // Nothing left over from a game before this one.
    map_reset();
    unit_reset();
    city_reset();
    combat_reset();
    game_over = quit = retired = loaded = 0;
    diplomacy_reset();
    contact = 1;
    greet = 0;
    hover_unit = 0;
    selecting = 0;

    // START_OFF_X / START_OFF_Y are what let a headless run check
    // scrolling: `make START_OFF_X=41` against a plain build must shift the
    // map exactly eight pixels and leave the panel alone. The settler goes
    // on the grassland nearest the middle of that view, so it is the same
    // settler in both builds and neither has to recentre on it.
    off_x = START_OFF_X;
    off_y = START_OFF_Y;
    map_cell_at(off_x + VIEW_COLUMNS / 2, off_y + VIEW_ROWS / 2, &cx, &cy);
    if (!find_terrain(cx, cy, TILE_GRASS, &ax, &ay)
        && !find_terrain(cx, cy, TILE_HILLS, &ax, &ay)) {
        ax = cx;
        ay = cy;
    }
    // Every people starts with a settler and a warrior on its hexagon, so
    // that there is somebody to explore from the first turn.
    unit_create(KIND_SETTLER, PLAYER_HUMAN, ax, ay);
    unit_create(KIND_WARRIOR, PLAYER_HUMAN, ax, ay);
    ai_place();

    game_turn = 1;
    game_year = START_YEAR;
    crisis_drought = 0;
    for (ax = 0; ax < PLAYERS; ++ax) crisis_cities[ax] = 0;
    dirty = 1;
    hover_dirty = 1;
}
