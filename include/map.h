#ifndef __MAP_H
#define __MAP_H

typedef signed char hexcoord;

typedef struct _Hex {
    hexcoord q;
    hexcoord r;
} Hex;

void map_init(void);
void map_center(hexcoord x, hexcoord y);
#endif
