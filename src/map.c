#include <stdio.h>
#include <stdlib.h>

#include "map.h"
#include "hexwar.h"
#include "graphics.h"

// https://www.redblobgames.com/grids/hexagons/implementation.html
// We use double coordinates for hexagons
//
//  0,0     2,0     4,0
//      1,1     3,1
//  0,2     2,2     4,2
//      1,3     3,3
//
// these can then be stored in a normal 2-dimensional array and
// translated to screen coordinates as needed
// 0,0  1,1  2,0  3,1 4,0
// 0,2  1,3  2,2  3,3 4,2


extern TileInfo bitmap_tile[4];

struct _Map {
    // The main map
    byte tile[80][50];
    // center coordinates (tile in center of the view)
    Hex center;
} map;


// Mapping from hex to storage array
arraycoord x_hex2array(hexcoord x, hexcoord) { return x; }
arraycoord y_hex2array(hexcoord, hexcoord y) { return y / 2; }

// Mapping from storage (array) to hex
hexcoord x_array2hex(arraycoord x, arraycoord) { return x; }
hexcoord y_array2hex(arraycoord x, arraycoord y) { return y * 2 + (x % 2); }

// Mapping from hex to screen coordinates
screencoord x_hex2screen(hexcoord x, hexcoord y) { return x*4 + 4*(y % 2); }
screencoord y_hex2screen(hexcoord x, hexcoord y) { return 6*(y/2) + 3*(y % 2); }

// Mapping from screen coordinates to hex
hexcoord x_screen2hex(screencoord x, screencoord y) {
    hexcoord a = 2*(x/8) + (y % 2);
    printf("%d %d -> %d\n", x, y, a);
    return a;
} 
hexcoord y_screen2hex(screencoord x, screencoord y) { 
    return 6*(y/2) + 3*(y % 2);
} 

void map_init()
{
    byte xx, yy;
    for(_y = 0; _y < 50; _y++) {
        for(_x = 0; _x < 80; _x++) {
            map.tile[_x][_y] = (rand() % 3);
        }
    }

    //if( x_screen2hex(0,0) != 0 || y_screen2hex(0,0) != 0 ||
   //     x_screen2hex(7,0) != 0 || y_screen2hex(7,2) != 0
        //x_screen2hex(8,0) != 2 || y_screen2hex(8,3) != 0 || 
        //x_screen2hex(5,5) != 1 || x_screen2hex(5,5) != 1 ||
        //x_screen2hex(13,5) != 3 || y_screen2hex(13,5) != 1
   //     ) {
   //     fatal("bad");
   // }


    //draw_tile_in_view(VIEW_LEFT,VIEW_TOP, &desert_tile);
    for(yy = 0; yy < 1; ++yy)  {
        for(xx = 0; xx < 5; xx += 2)  {
            draw_tile_in_view(
                VIEW_LEFT + x_hex2screen(xx, yy),
                VIEW_TOP + y_hex2screen(xx, yy),
                &bitmap_tile[0]);
        }
    }
}

void map_show_tile(hexcoord hx, hexcoord hy)
{
    byte x, y;
    x = x_hex2screen(hx, hy);
    x = x_hex2screen(hx, hy);

}

void map_draw_top_tiles()
{
    byte x, hx, hy;
    hx = x_screen2hex(0, 0);
    hx = y_screen2hex(0, 0);
    for(x = 0; x < VIEW_WIDTH_IN_TILES; ++x)  {
        map_show_tile(hx, hy);
        hx += 2;
    }
}
