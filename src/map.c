#include <stdio.h>
#include <stdlib.h>

#include "map.h"
#include "hexwar.h"
#include "graphics.h"

// https://www.redblobgames.com/grids/hexagons/implementation.html

extern BitmapInfo tiles;

extern TileInfo desert_tile;
extern TileInfo plains_tile;
extern TileInfo hills_tile;
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

byte view_offset_x;
byte view_offset_y;

Hex hex_directions[6];

Hex hexview_top[10];
Hex hexview_left[1];
Hex hexview_right[1];
Hex hexview_center[6];
Hex hexview_bottom[9];

struct _Map {
    // The main map
    byte tile[80][50];
    // center coordinates (tile in center of the view)
    Hex center;
} map;

void map_init(void)
{
    // adjacent hexagons
    hex_init( 1,  0, &hex_directions[0]);
    hex_init( 1, -1, &hex_directions[1]);
    hex_init( 0, -1, &hex_directions[2]);
    hex_init(-1,  0, &hex_directions[3]);
    hex_init(-1,  1, &hex_directions[4]);
    hex_init( 0,  1, &hex_directions[5]);

    // hexagons in view (relative to center)
    hex_init( 1,  0, &hexview_center[0]);
    hex_init( 1, -1, &hexview_center[1]);
    hex_init( 0, -1, &hexview_center[2]);
    hex_init(-1,  0, &hexview_center[3]);
    hex_init(-1,  1, &hexview_center[4]);
    hex_init( 0,  1, &hexview_center[5]);

    hex_init( -3,  -4, &hexview_top[0]);
    hex_init( -2,  -4, &hexview_top[1]);
    hex_init( -1,  -4, &hexview_top[2]);
    hex_init(  0,  -4, &hexview_top[3]);
    hex_init(  1,  -4, &hexview_top[4]);
    hex_init(  2,  -4, &hexview_top[5]);
    hex_init(  3,  -4, &hexview_top[6]);
    hex_init(  4,  -4, &hexview_top[7]);
    hex_init(  5,  -4, &hexview_top[8]);
    hex_init(  6,  -4, &hexview_top[9]);

    hex_init( -6,  3, &hexview_bottom[0]);
    hex_init( -5,  3, &hexview_bottom[1]);
    hex_init( -4,  3, &hexview_bottom[2]);
    hex_init( -3,  3, &hexview_bottom[3]);
    hex_init( -2,  3, &hexview_bottom[4]);
    hex_init( -1,  3, &hexview_bottom[5]);
    hex_init(  0,  3, &hexview_bottom[6]);
    hex_init(  1,  3, &hexview_bottom[7]);
    hex_init(  2,  3, &hexview_bottom[8]);

    // the earth start with just water
    for(y = 0; y < 50; y++) {
        for(x = 0; x < 80; x++) {
            map.tile[x][y] = (rand() % 3);
        }
    }
}

void map_viewoffset(byte offset_x, byte offset_y)
{
    view_offset_x = offset_x;
    view_offset_y = offset_y;
}

void map_show_tile_xy(hexcoord hx, hexcoord hy)
{
    Hex offset;
    hex_init(hx, hy, &offset);
    map_show_tile(&offset);
}

void map_show_tile(Hex *offset)
{
    int xx, yy;
    Hex hex;
    TileInfo *tile;
    hex_add(&map.center, offset, &hex);
    switch(map.tile[hex.q][hex.r]) {
        case 0: tile = &plains_tile; break;
        case 1: tile = &desert_tile; break;
        case 2: tile = &hills_tile; break;
        default: tile = &ocean_tile;
    }

    xx =  VIEW_CENTER_X - view_offset_x + 8 * offset->q + 4 * offset->r;
    yy =  VIEW_CENTER_Y - view_offset_y + 3 * offset->r;
    //printf("xx %d yy %d\n", xx, yy);
    draw_tile_in_view(xx, yy, tile);
}

void map_center(hexcoord x, hexcoord y)
{
    hex_init(x, y, &map.center);
    printf("%d %d\n", VIEW_CENTER_X, VIEW_CENTER_Y);

    //for(i = 0; i < 7; i++) map_show_tile(&hexview_center[i]);
    //for(i = 0; i < 10; i++) map_show_tile&hexview_top[i]);
    //for(i = 0; i < 9; i++) map_show_tile(&hexview_bottom[i]);
}

