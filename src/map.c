#include "map.h"
#include "sheet.h"

byte map_tile[MAP_ROWS][MAP_COLUMNS];

// A placeholder world, standing in for the Civ 1 map generator that design.md
// describes. It is deliberately not random noise: bands of terrain make it
// obvious at a glance whether the view is drawing and scrolling the map it
// should be, which random tiles do not.
void map_init(void)
{
    byte ay, ax;

    for (ay = 0; ay < MAP_ROWS; ++ay) {
        for (ax = 0; ax < MAP_COLUMNS; ++ax) {
            byte t;
            if (ax < 4 || ax >= MAP_COLUMNS - 4) {
                t = TILE_OCEAN;             // an edge to scroll up against
            } else if (ay < 3 || ay >= MAP_ROWS - 3) {
                t = TILE_TUNDRA;            // and poles, likewise
            } else {
                switch ((ax / 5 + ay / 4) % 6) {
                case 0: t = TILE_GRASS; break;
                case 1: t = TILE_GRASS; break;
                case 2: t = TILE_DESERT_HILL; break;
                case 3: t = TILE_MOUNTAIN; break;
                case 4: t = TILE_SHALLOW_OCEAN; break;
                default: t = TILE_OCEAN; break;
                }
            }
            map_tile[ay][ax] = t;
        }
    }
}
