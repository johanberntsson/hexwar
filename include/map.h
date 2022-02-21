#ifndef __MAP_H
#define __MAP_H

#include "hexwar.h"

typedef signed char hexcoord;

typedef struct _Hex {
    hexcoord q;
    hexcoord r;
} Hex;

void hex_init(hexcoord q, hexcoord r, Hex *hex);

void map_init(void);
void map_center(hexcoord x, hexcoord y);
void map_show_tile(Hex *offset);
void map_show_tile_xy(hexcoord hx, hexcoord hy);
void map_viewoffset(byte offset_x, byte offset_y);
#endif
