// The readout of what is under the arrow, out of game.c (22 Oct 2026, to
// make resident room for the fishing boat): it is drawn only when the arrow
// or the view has moved, never in the redraw, so it can be in region 0
// (src/region.h), which is the window's own and needs no copying. It
// shares game.c's state through gameint.h, as turn.c does.
#define game_draw_hover game_draw_hover_r0

#include "gameint.h"
#include "city.h"
#include "civ.h"
#include "growth.h"
#include "input.h"
#include "map.h"
#include "minimap.h"
#include "screen.h"
#include "sheet.h"
#include "text.h"
#include "unit.h"

#pragma clang section text="r0text" rodata="r0rodata"

// What is under the arrow, right-aligned at the end of the top row: a city's
// name or whose unit it is, the terrain (or "Unexplored", which hides both), and the cell -- on
// the map window or on the minimap. Blank when the arrow is on neither.
void game_draw_hover(void)
{
    byte ax, ay, c, u, x;

    line_clear();
    // While tile select is up the readout follows the ring, not the arrow;
    // and when the orders pass to a unit, or it moves, it shows the unit's
    // hexagon (hover_on()) until the arrow moves.
    if (selecting) {
        ax = sel_ax;
        ay = sel_ay;
    } else if (hover_unit) {
        ax = hover_ax;
        ay = hover_ay;
    }
    if (selecting || hover_unit || pointer_hex(&ax, &ay)
        || minimap_hex(input_x, input_y, &ax, &ay)) {
        if (!MAP_IS_EXPLORED(ax, ay)) {
            line_add("Unexplored");
        } else {
            // A city's name says whose it is; a unit needs telling.
            c = city_at(ax, ay);
            u = unit_at(ax, ay);
            if (c != CITY_NONE) {
                line_add(city_name_of(c));
                line_add("  ");
            } else if (u != UNIT_NONE) {
                line_add_unit(unit_owner[u], unit_kind[u]);
                line_add("  ");
            }
            // The terrain and its resource, "Hills+Copper"; with a city or
            // a unit, the resource instead of the terrain, which is what
            // there is room for -- "Canaanite Builder  Cattle+irr 127,19".
            x = map_resource_at(ax, ay);
            if (x == RES_NONE || (c == CITY_NONE && u == UNIT_NONE)) {
                line_add(text_line(TEXT_LIST_TERRAINS, MAP_TERRAIN(ax, ay)));
                if (x != RES_NONE) line_add("+");
            }
            if (x != RES_NONE) line_add(text_line(TEXT_LIST_RESOURCES, x));
            x = map_tile[ay][ax] & MAP_IMPROVEMENT;
            // Short, so that "Canaanite Builder  Mountains+mine 127,31"
            // still fits in the 40 columns.
            if (x) line_add(x == MAP_MINED ? "+mine"
                            : x == MAP_EMPORIUM ? "+emporium"
                            : MAP_TERRAIN(ax, ay) == TILE_SHALLOW_OCEAN ? "+boats"
                            : "+irr");
            // What it yields, food/shields/trade, where there is room:
            // with nothing on it.
            if (c == CITY_NONE && u == UNIT_NONE) {
                hex_yield(ax, ay);
                line_add(" ");
                line_add_num(hex_food);
                line_add("/");
                line_add_num(hex_shields);
                line_add("/");
                line_add_num(hex_trade);
            }
        }
        line_add(" ");
        line_add_num(ax);
        line_add(",");
        line_add_num(ay);
    }
    // Padded on the left to a fixed field, so a shorter text overwrites a
    // longer one. The menu line ends at column 38.
    for (x = 40; x < SCREEN_COLUMNS - line_len; ++x)
        screen_putcxy(x, 0, ' ', COLOR_WHITE);
    screen_putsxy(SCREEN_COLUMNS - line_len, 0, line, COLOR_WHITE);
}
