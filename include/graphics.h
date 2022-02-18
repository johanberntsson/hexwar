#ifndef __GRAPHICS_H
#define __GRAPHICS_H

#include "hexwar.h"

#define VIC2CTRL (*(byte *)(0xd016))
#define BORDERCOL (*(byte *)(0xd020))
#define SCREENCOL (*(byte *)(0xd021))
#define VIC4CTRL (*(byte *)(0xd054))
#define VIC3CTRL (*(byte *)(0xd031))
#define CHARSTEP_LO (*(byte *)(0xd058))
#define CHARSTEP_HI (*(byte *)(0xd059))
#define CHRCOUNT (*(byte *)(0xd05e))
#define HOTREG (*(byte *)(0xd05d))
#define SCNPTR_0 (*(byte *)(0xd060))
#define SCNPTR_1 (*(byte *)(0xd061))
#define SCNPTR_2 (*(byte *)(0xd062))
#define SCNPTR_3 (*(byte *)(0xd063))

typedef struct _TileInfo {
    dword base_address;
    byte width;
    byte height;
    word row_skip_length_in_bytes;
} TileInfo;

typedef struct _BitmapInfo {
    byte rows;
    byte columns;
    byte options;
    byte num_colors;
    word bitmap_size;
    word palette_size;
    dword bitmap_address;
    dword palette_address;
    bool reserved_sys_palette;
} BitmapInfo;

void setup_vertical_framebuffer();
void setup_horizontal_framebuffer();
void init_graphics_mode(void);
void clear_framebuffer(void);
byte loadFromFile(word size, dword address);
dword load_FCI(char *filename, dword address, BitmapInfo *info);
void set_palette(BitmapInfo *bitmap);
void prepare_tile(TileInfo *tile, BitmapInfo *bitmap, byte x, byte y, byte w, byte h);
void draw_tile(byte x0, byte y0, TileInfo *tile);
void draw_expanded_tile(byte x0, byte y0, TileInfo *tile);

#endif
