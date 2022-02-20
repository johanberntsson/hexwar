#include "map.h"
#include "hexwar.h"
#include "graphics.h"


// https://www.redblobgames.com/grids/hexagons/implementation.html

// Convention:
// hx, hy is cubic coordonates for tiles (hexagons)
// ax, ay is array coordinates for tile storage
// vx, vy is screen coordintes for the view

extern BitmapInfo tiles;
extern TileInfo desert_tile;
extern TileInfo plains_tile;
extern TileInfo ocean_tile;

void hex_init(hexcoord q, hexcoord r, Hex *hex)
{
    hex->q = q;
    hex->r = r;
}

void hex_add(Hex *a, Hex *b, Hex *result)
{
    result->q = a->q + b->q;
    result->r = a->r + b->r;
}

Hex hex_directions[6];

struct _Map {
    // The main map
    byte tile[80][50];
    // center coordinates (tile in center of the view)
    Hex center;
} map;

void map_init(void)
{
    hex_init( 1,  0, &hex_directions[0]);
    hex_init( 1, -1, &hex_directions[1]);
    hex_init( 0, -1, &hex_directions[2]);
    hex_init(-1,  0, &hex_directions[3]);
    hex_init(-1,  1, &hex_directions[4]);
    hex_init( 0,  1, &hex_directions[5]);

    // the earth start with just water
    for(y = 0; y < 50; y++) {
        for(x = 0; x < 80; x++) {
            map.tile[x][y] = 0;
        }
    }
}

void map_show_tile(Hex *tile)
{
    draw_tile_in_view(VIEW_CENTER_X, VIEW_CENTER_Y, &desert_tile);
}

void map_center(hexcoord x, hexcoord y)
{
    Hex result;
    hex_init(x, y, &map.center);

    map_show_tile(&map.center);
    for(i = 0; i < 6; i++) {
        hex_add(&map.center, &hex_directions[i], &result);
        map_show_tile(&result);
    }
}

