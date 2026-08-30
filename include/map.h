#ifndef __MAP_H
#define __MAP_H

#include "hexwar.h"

typedef unsigned char hexcoord;
typedef unsigned char arraycoord;
typedef unsigned int screencoord;

typedef struct _Hex {
    hexcoord x;
    hexcoord y;
} Hex;

void hex_init(hexcoord q, hexcoord r, Hex *hex);

void map_init(void);
void map_center(hexcoord x, hexcoord y);
void map_draw_top_tiles();
void map_draw_tile(hexcoord x, hexcoord y);
#endif
