#include <c64.h>
#include <stdio.h>

#include "dma.h"
#include "graphics.h"
#include "assembler_utilities.h"

#ifdef H640
    #define SCREEN_WIDTH 80
#else
    //TODO
#endif

#ifdef V400
    #define SCREEN_HEIGHT 50
#else
    #define SCREEN_HEIGHT 25
    #ifdef H640
        #define EXPAND_TILE_X
    #endif
#endif

#ifdef SCREENBASE_UNDER_RAM
#define ENABLE_SCREEN_ACCESS asm("sei"); POKE(1,0x35);
#define DISABLE_SCREEN_ACCESS POKE(1,0x36); asm("cli");
#else
#define ENABLE_SCREEN_ACCESS
#define DISABLE_SCREEN_ACCESS
#endif

byte text_x;
byte text_y;
byte text_color;
byte text_attributes;
word text_screen_offset;

void load_from_file(FILE *f, word size, dword address);
unsigned char swap_nibbles(unsigned char in);

void setup_horizontal_framebuffer()
{
    // byte x, y;
    // word i, j;

    // Layout screen so that map tile data comes from $19000 -- $5FFFF
    i = 0x19000 / 0x40;
    j = SCREENBASE;
    for (y = 0; y < SCREEN_WIDTH; ++y) {
        for (x = 0; x < SCREEN_HEIGHT; ++x) {
            POKE(j, i & 0xff);
            //if(x == 0 && y == 0) fatal("%x %x %x\n", j, i & 0xff, i >> 8);
            ++j;
            POKE(j, i >> 8);
            ++j;
            ++i;
            // skip DOS
            if (i == (0x1f800 / 0x40)) i = 0x24000 / 0x40;
            // skip C64 kernal
            if (i == (0x2c000 / 0x40)) i = 0x30000 / 0x40;
        }
    }
}

void init_graphics_mode(void)
{
    g_xy(0,0);
    g_attributes(0);
    g_color(COLOR_RED);

    // Fast CPU, M65 IO
    mega65_io_enable();

    // bank out the C64/C65 ROM, freeing $2xxxx and $3xxxx.
    // But, assuming that the program is started from C64
    // mode, we still need the kernal, so avoid writing on
    // 2c000 - 2ffff.
    // See the MEGA65 book, page F-11
    disable_rom_protection();
    clear_framebuffer();
    setup_horizontal_framebuffer();
    // Clear colour RAM
    dma_fill(0xff80000L, 0x00, 2 * SCREEN_WIDTH * SCREEN_HEIGHT);

    // 16-bit text mode, full-colour text for high chars
    VIC4CTRL = 0x05;
#ifdef V400
    // H640, fast CPU, V400
    VIC3CTRL = 0xC8;
#else
    // H640, fast CPU, V200
    VIC3CTRL = 0xC0;
#endif
    // Adjust D016 smooth scrolling for VIC-III H640 offset
    VIC2CTRL = 0xC9;
    // 640x400 16bits per char, 8 pixels wide per char
    // = 640/16 x 8 bits = 160 bytes per row
    CHARSTEP_LO = SCREEN_WIDTH * 2;
    CHARSTEP_HI = 0;
    // Draw SCREEN_WIDTH chars per row
    CHRCOUNT = SCREEN_WIDTH;
    // Put max 80x50x2 = 8KB screen at SCREENBASE
    SCNPTR_0 = 0x00;
    SCNPTR_1 = 0xe0;
    SCNPTR_2 = 0x00;
    SCNPTR_3 = 0x00;

    BORDERCOL = 0;
    SCREENCOL = 0;
}

void clear_framebuffer(void)
{
    dma_fill(0x19000, 0x00, 0x6800);
    //  skip DOS ($1f800 - $24000)
    dma_fill(0x24000, 0x00, 0x8000);
    // skip C64 kernal ($2c000 - $30000)
    dma_fill(0x30000, 0x00, 0x8000);
    dma_fill(0x38000, 0x00, 0x8000);
    dma_fill(0x40000, 0x00, 0x8000);
    dma_fill(0x48000, 0x00, 0x8000);
    dma_fill(0x50000, 0x00, 0x8000);
    dma_fill(0x58000, 0x00, 0x8000);
}

void load_from_file(FILE *f, word size, dword address)
{
    // Read <size> bytes from file; return 1 if ok, 0 if error
    static byte num_read, n;
    while(size > 0) {
        n = size < TMPBUFFER_SIZE ? size : TMPBUFFER_SIZE;
        num_read = fread((char *) TMPBUFFER, 1, n, f);
        if(num_read != n) {
            fclose(f);
            fatal("file error");
        }
        dma_copy(TMPBUFFER, address, num_read);
        address += num_read;
        size -= num_read;
    }
}

dword load_FCI(char *filename, dword address, BitmapInfo *bitmap)
{
    // updates info and return next address if ok, 0 if error
    //
    // Note: cannot print while file is open as channel 1
    // (add printout after cbm_close(1); below)
    //
    FILE *f;

    // read header
    f = fopen(filename, "r");
    fread((char *) TMPBUFFER, 1, 9, f);
    if(((char *) TMPBUFFER)[0] != 0x66) return 0;
    bitmap->rows = ((char *) TMPBUFFER)[5];
    bitmap->columns = ((char *) TMPBUFFER)[6];
    bitmap->options = ((char *) TMPBUFFER)[7];
    bitmap->num_colors = ((char *) TMPBUFFER)[8];
    bitmap->reserved_sys_palette = bitmap->options & 2;

    // read palette
    bitmap->palette_size = (bitmap->num_colors + 1) * 3;
    bitmap->palette_address = address;
    load_from_file(f, bitmap->palette_size, bitmap->palette_address);

    // read "IMG"
    fread((char *) TMPBUFFER, 1, 3, f);

    // read bitmap
    bitmap->bitmap_size = bitmap->rows * bitmap->columns * 64;
    bitmap->bitmap_address = address + bitmap->palette_size;
    load_from_file(f, bitmap->bitmap_size, bitmap->bitmap_address);

    fclose(f);
    return address + bitmap->bitmap_size + bitmap->palette_size;
}

void prepare_tile(TileInfo *tile, BitmapInfo *bitmap, byte x, byte y, byte w, byte h)
{
    tile->width = w;
    tile->height = h;
    tile->row_skip_length_in_bytes = 64 * (bitmap->columns - w);
    tile->base_address = bitmap->bitmap_address + 64*(x + y * bitmap->columns);
}

static unsigned char swp;
unsigned char swap_nibbles(unsigned char in) // oh why?!
{
    swp = in;
    asm("lda %v", swp);
    asm("asl  a");
    asm("adc  #$80");
    asm("rol a");
    asm("asl a");
    asm("adc  #$80");
    asm("rol  a");
    asm("sta %v", swp);
    return swp;
}

void set_palette(BitmapInfo *bitmap)
{
    dword address;
    x  = bitmap->reserved_sys_palette ? 16 : 0;
    address = bitmap->palette_address + 3 * x;
    for(y = x; y <= bitmap->num_colors; ++y) {
        POKE(0xd100u + y, swap_nibbles(dma_peek(address)));
        ++address;
        POKE(0xd200u + y, swap_nibbles(dma_peek(address)));
        ++address;
        POKE(0xd300u + y, swap_nibbles(dma_peek(address)));
        ++address;
    }
}

void draw_tile_anywhere(byte x0, byte y0, TileInfo *tile)
{
    // byte x, y;
    // word i, j;
    static word screen;         // screen RAM points to...
    static dword framebuffer;  // the framebuffer, which has data from...
    static dword tile_bitmap;   // the tile with data loaded from the FCI file
    static byte screen_skip;

    screen = SCREENBASE + 2 * SCREEN_WIDTH * y0 + 2 * x0;
    screen_skip = 2 * (SCREEN_WIDTH - tile->width);
    tile_bitmap = tile->base_address;
    ENABLE_SCREEN_ACCESS
    for(y = 0; y < tile->height; ++y) {
        if(y + y0 >= SCREEN_HEIGHT) continue;
        for(x = 0; x < tile->width; ++x) {
            if(x + x0 < SCREEN_WIDTH) {
#ifdef SCREENBASE_FIRST_64K
                framebuffer = *((word *) screen);
#else
                framebuffer = (dma_peek(screen) + 256 * dma_peek(screen + 1));
#endif
                dma_copy_transparent(tile_bitmap, framebuffer * 64, 64);
            }
            screen += 2;
            tile_bitmap += 64;
        }
        tile_bitmap += tile->row_skip_length_in_bytes;
        screen += screen_skip;
    }
    DISABLE_SCREEN_ACCESS
}


void g_xy(byte x, byte y)
{
    text_x = x;
    text_y = y;
}

void g_color(byte color)
{
    text_color = color;
}

void g_attributes(byte attributes)
{
    text_attributes = attributes;
}

void g_putc(byte c)
{
    dword colorbase;

    text_screen_offset = 2 * (text_x + SCREEN_WIDTH * text_y);
    i = SCREENBASE + text_screen_offset;
    colorbase = COLORBASE + text_screen_offset;
    //fatal("%lx %lx %x ", COLORBASE, colorbase, text_screen_offset);

    ENABLE_SCREEN_ACCESS
    POKE(i, c);
    POKE(i + 1, 0);
    DISABLE_SCREEN_ACCESS
    dma_poke(colorbase, 0);
    dma_poke(colorbase + 1, text_color | 0x20);

    if(++text_x >= SCREEN_WIDTH) {
        text_x = 0;
        if(++text_y >= SCREEN_HEIGHT) --text_y;
    }
}

void draw_tile_in_view(byte x0, byte y0, TileInfo *tile)
{
    static word screen;         // screen RAM points to...
    static dword framebuffer;  // the framebuffer, which has data from...
    static dword tile_bitmap;   // the tile with data loaded from the FCI file
    static byte screen_skip;

    screen = SCREENBASE + 2 * (SCREEN_WIDTH * y0 + x0);
#ifdef EXPAND_TILE_X
    screen_skip = 2 * (SCREEN_WIDTH - 2 * tile->width);
#else
    screen_skip = 2 * (SCREEN_WIDTH - tile->width);
#endif
    tile_bitmap = tile->base_address;
    ENABLE_SCREEN_ACCESS
    for(y = 0; y < tile->height; ++y) {
        u = y + y0;
        if(u > VIEW_BOTTOM) continue;
        if(u >= VIEW_TOP) {
            for(x = 0; x < tile->width; ++x) {
#ifdef EXPAND_TILE_X
                v = 2*x + x0;
#else
                v = x + x0;
#endif
                if(v >= VIEW_LEFT && v <= VIEW_RIGHT) {
#ifdef SCREENBASE_FIRST_64K
                    framebuffer = *((word *) screen);
#else
                    framebuffer = (dma_peek(screen) + 256 * dma_peek(screen + 1));
#endif
#ifdef EXPAND_TILE_X
                    dma_copy(tile_bitmap, TMPBUFFER, 64);
                    expand_fullcolor_char_x();
                    dma_copy_transparent(TMPBUFFER + 64, framebuffer * 64, 64);
                    dma_copy_transparent(TMPBUFFER + 128, (framebuffer + 1) * 64, 64);
                }
                screen += 4;
#else
                    dma_copy_transparent(tile_bitmap, framebuffer * 64, 64);
                }
                screen += 2;
#endif
                tile_bitmap += 64;
            }
        }
        tile_bitmap += tile->row_skip_length_in_bytes;
        screen += screen_skip;
    }
    DISABLE_SCREEN_ACCESS
}

