#include <c64.h>
#include <cbm.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

#include "hexwar.h"
#include "map.h"
#include "dma.h"
#include "graphics.h"

// temp variables (much faster to keep them global)
word i, j;
byte x, y, u, v;

/*  Memory layout:
         334 -      3fb : tmp buffer for loadFCI etc. (200 bytes)
         801 -     cfff : main program
        e000 -     ff40 : screen
       12000 -    18fff
       40000 -    5ffff : screen framebuffer (bitmap)
     8000000 -  83e8000 : map bitmap (4 MB for 80*50*4*4*64 (4*4 tiles))
     8400000 -          : bitmaps (loaded from disc)
    0ff80000 - 0ff81f40 : color and attributes memory
*/

/*  Screen layout:
    $30000 - $60000 is used for a map display using tiles, which can
    be up to 64*48 characters large (on a 80*50 screen)

    $30000-$3FFFF is used for C65 ROM, so we have to stick
    to C64 mode and disable "ROM write protect" via the hypervisor.

    BMPs are scanned horizontally, so we will lay out the screen that
    way, so that the loading can go faster, and use short DMA jobs 
    where it makese sense.
*/

void go_8bit()
{
    mega65_io_enable();
    VIC3CTRL = 96;    // quit bitplane mode if set
    SCNPTR_0 = 0x00;  // screen back to 0x400
    SCNPTR_1 = 0x04;
    SCNPTR_2 = 0x00;
    SCNPTR_3 &= 0xF0;
    VIC4CTRL &= 0xFA; // clear fchi and 16bit chars
    CHRCOUNT = 40;
    CHARSTEP_LO = 40;
    CHARSTEP_HI = 0;
    HOTREG |= 0x80;   // enable hotreg
    VIC3CTRL &= 0x7f; // disable H640
    VIC3CTRL &= 0xf7; // disable V400
    POKE(0xd018,23);   // lowercase charset
    //fc_setPalette(0, 0, 0, 0);
    //fc_setPalette(1, 255, 255, 255);
    //fc_setPalette(2, 255, 0, 0);
}

void fatal(const char *format, ...)
{
    va_list args;

    go_8bit();
    va_start(args, format);
    vsprintf((char *) 0x800, format, args);
    va_end(args);
    puts("fatal error: ");
    puts((char *) 0x800);
    while (1) ;
}

void start_benchmark()
{
    POKE(0xa2, 0);
    POKE(0xa1, 0);
    POKE(0xa0, 0);
}

void stop_benchmark()
{
    long jiffies;
    jiffies = PEEK(0xa2)+256*PEEK(0xa1)+256*256*PEEK(0xa0);
    fatal("benchmark %ld jiffies", jiffies);
}

BitmapInfo tiles;
TileInfo desert_tile;
TileInfo plains_tile;
TileInfo hills_tile;
TileInfo ocean_tile;

#ifdef DEBUG
void debug_BitmapInfo(BitmapInfo *bitmap)
{
    printf("rows %d columns %d\n", bitmap->rows, bitmap->columns);
    printf("palette: size %x, address %lx\n", bitmap->palette_size, bitmap->palette_address);
    printf("bitmap: size %x, address %lx\n", bitmap->bitmap_size, bitmap->bitmap_address);
}

void debug_TileInfo(TileInfo *tile)
{
    printf("%d\n", tile->width);
}

void draw_view_frame()
{
	// draw frame around view
	g_attributes(64);
	g_xy(VIEW_LEFT - 1, VIEW_TOP - 1);
	for(x = VIEW_LEFT - 2; x <= VIEW_RIGHT; ++x) if(x < 79) g_putc(0x30);
	if(VIEW_BOTTOM <49) {
	    g_xy(VIEW_LEFT - 1, VIEW_BOTTOM + 1);
	    for(x = VIEW_LEFT - 2; x <= VIEW_RIGHT; ++x) if(x < 79) g_putc(0x30);
    }
	for(y = VIEW_TOP; y <= VIEW_BOTTOM; ++y) {
	    g_xy(VIEW_LEFT - 1, y); g_putc(0x30);
	    if(VIEW_RIGHT < 79) g_xy(VIEW_RIGHT + 1, y); g_putc(0x30);
    }
}
#endif 

void draw_test_tiles()
{
    byte xx, yy, dx, ox, dy; // dx = tile width, ox = half tile width
#ifdef V400
    dx = 4;
    ox = 2;
#else
    dx = 8;
    ox = 4;
#endif
    xx = 18; yy = 1; dy = 3; 
	draw_tile_in_view(xx        , yy     , &desert_tile);
	draw_tile_in_view(xx+dx     , yy     , &plains_tile);
	draw_tile_in_view(xx+2*dx   , yy     , &ocean_tile);
	draw_tile_in_view(xx+3*dx   , yy     , &ocean_tile);
	draw_tile_in_view(xx+4*dx   , yy     , &ocean_tile);
	draw_tile_in_view(xx+5*dx   , yy     , &ocean_tile);
	draw_tile_in_view(xx+6*dx   , yy     , &ocean_tile);
	draw_tile_in_view(xx+7*dx   , yy     , &ocean_tile);
	draw_tile_in_view(xx+ox     , yy+  dy, &desert_tile);
	draw_tile_in_view(xx+ox+dx  , yy+  dy, &desert_tile);
	draw_tile_in_view(xx+ox+2*dx, yy+  dy, &desert_tile);
	draw_tile_in_view(xx        , yy+2*dy, &plains_tile);
	draw_tile_in_view(xx+ox     , yy+3*dy, &desert_tile);
	draw_tile_in_view(xx        , yy+4*dy, &plains_tile);
	draw_tile_in_view(xx+ox     , yy+5*dy, &desert_tile);
	draw_tile_in_view(xx        , yy+6*dy, &plains_tile);
	draw_tile_in_view(xx+ox     , yy+7*dy, &desert_tile);
}

void main(void) 
{
    Hex hex;
    byte key;
    dword next_address;
    byte vx, vy; // view offset (0 - 3)
    byte cx, cy; // center tile position
    byte redraw_view;
    hexcoord xx, yy; 

    start_benchmark();

    dma_init();
	init_graphics_mode();
    HOTREG &= 0x7f;   // disable hotreg

	next_address = load_FCI("tiles.fci", 0x8400000, &tiles);
	if(next_address == 0) fatal("tiles.fci failed to load");
	set_palette(&tiles);
	prepare_tile(&desert_tile, &tiles, 0, 0, 4, 4);
	prepare_tile(&plains_tile, &tiles, 4, 0, 4, 4);
	prepare_tile(&hills_tile, &tiles,  8, 0, 4, 4);
	prepare_tile(&ocean_tile, &tiles, 12, 0, 4, 4);
#ifdef DEBUG
	//debug_BitmapInfo(&tiles);
	//debug_TileInfo(&tiles);

	draw_view_frame();
	//draw_test_tiles();

	vx = 4;
	vy = 2;
	cx = 40;
	cy = 12;
	redraw_view = true;
    map_init();
	map_center(cx,cy);
    for(;;) {
        if(redraw_view) {
	        map_center(cx,cy);
            redraw_view = false;
            map_viewoffset(vx, vy);
            for(yy = -4; yy < 5; yy++) {
                for(xx = -7; xx < 7; xx++) {
                    map_show_tile_xy(xx, yy);
                }
            }
        }
        g_xy(VIEW_LEFT + VIEW_WIDTH/2, VIEW_TOP + VIEW_HEIGHT/2);
        g_putc(0x30);

        key = PEEK(0xd610);
        POKE(0xd610, 0);
        if(key != 0) redraw_view = true;
        switch(key) {
            case 97:
            case 157:
                // left
                if(vx > 0) {
                    vx = vx - 2;
                } else {
                    vx = 8;
                    cx = cx - 1;
                    redraw_view = true;
                }
                //printf("left %d\n", vx);
                break;
            case 100:
            case 29:
                // right
                if(vx < 6) {
                    vx = vx + 2;
                } else {
                    vx = 0;
                    cx = cx + 1;
                    redraw_view = true;
                }
                //printf("right %d\n", vx);
                break;
            case 119:
            case 145:
                // up
                if(vy > 0) {
                    vy = vy - 1;
                } else {
                    vy = 3;
                    cy = cy - 1;
                    redraw_view = true;
                }
                //printf("up %d\n", vy);
                break;
            case 115:
            case 17:
                // down
                if(vy < 4) {
                    vy = vy + 1;
                } else {
                    vy = 0;
                    cy = cy + 1;
                    redraw_view = true;
                }
                //printf("down %d\n", vy);
                break;
            case 13: fatal("done");
        }
    }

#endif
	//draw_tile_anywhere(6, 9, &desert_tile);


	//dma_poke(0x19000, 1);
	//dma_poke(0x19010, 1);
	//dma_poke(0x19000 + 50*64, 1);

    //stop_benchmark();
	for(;;) {
	    key = PEEK(0xd610);
	    POKE(0xd610, 0);
        if(key == 97 || key == 157) {
#ifdef V400
            scroll_left();
#else
            scroll_left();
            scroll_left();
#endif
        }
        if(key == 100 || key == 29) {
#ifdef V400
            scroll_right();
#else
            scroll_right();
            scroll_right();
#endif
        }
        if(key == 119 || key == 145) scroll_up();
        if(key == 115 || key == 17) scroll_down();
        if(key == 13) fatal("done");
    }
}
