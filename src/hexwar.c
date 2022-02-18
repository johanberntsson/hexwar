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
#include "graphics.h"
#include "dma.h"

// temp variables (much faster to keep them global)
word i, j;
byte x, y;

/*  Memory layout:
         334 -      3fb : tmp buffer for loadFCI etc. (200 bytes)
         801 -     cfff : main program
        e000 -     ff40 : screen
       12000 -    18fff
       19000 -    5ffff : screen bitmap
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

#ifdef DEBUG
void debug_BitmapInfo(BitmapInfo *bitmap)
{
    printf("rows %d columns %d\n", bitmap->rows, bitmap->columns);
    printf("palette: size %x, address %lx\n", bitmap->palette_size, bitmap->palette_address);
    printf("bitmap: size %x, address %lx\n", bitmap->bitmap_size, bitmap->bitmap_address);
}

void debug_TileInfo(TileInfo *tile)
{
}
#endif 

void main(void) 
{
    dword next_address;
    start_benchmark();
    dma_init();
	init_graphics_mode();
    clear_map_tiles();
    HOTREG &= 0x7f;   // disable hotreg

	next_address = load_FCI("tiles.fci", 0x8400000, &tiles);
	if(next_address == 0) fatal("tiles.fci failed to load");
	set_palette(&tiles);
	prepare_tile(&desert_tile, &tiles, 0, 0, 4, 4);
#ifdef DEBUG
	//debug_BitmapInfo(&tiles);
	//debug_TileInfo(&tiles);
#endif
	draw_tile(0, 0, &desert_tile);
	draw_expanded_tile(10, 10, &desert_tile);


	//lpoke(0x19000, 1);
	//lpoke(0x30000, 1);

    //stop_benchmark();
	for(;;) {
    }
}
