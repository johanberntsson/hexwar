#include <stdio.h>

#include <memory.h>

// temp variables (much faster to keep them global)
unsigned short i, j;
unsigned char a, b;

#define SCREENBASE 0xe000

#define VIC2CTRL (*(unsigned char *)(0xd016))
#define BORDERCOL (*(unsigned char *)(0xd020))
#define SCREENCOL (*(unsigned char *)(0xd021))
#define VIC4CTRL (*(unsigned char *)(0xd054))
#define VIC3CTRL (*(unsigned char *)(0xd031))
#define CHARSTEP_LO (*(unsigned char *)(0xd058))
#define CHARSTEP_HI (*(unsigned char *)(0xd059))
#define CHRCOUNT (*(unsigned char *)(0xd05e))
#define HOTREG (*(unsigned char *)(0xd05d))
#define SCNPTR_0 (*(unsigned char *)(0xd060))
#define SCNPTR_1 (*(unsigned char *)(0xd061))
#define SCNPTR_2 (*(unsigned char *)(0xd062))
#define SCNPTR_3 (*(unsigned char *)(0xd063))

/*  Memory layout:
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

void go8bit()
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

    mega65_io_enable();
    go8bit();

    va_start(args, format);
    vsprintf((char *) 0x800, format, args);
    va_end(args);
    puts("fatal error: ");
    puts(0x800);
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

void setup_main_screen()
{
    // Layout screen so that map tile data comes from $19000 -- $5FFFF
    i = 0x19000 / 0x40;
    j = SCREENBASE;
    for (b = 0; b < 50; ++b) {
        for (a = 0; a < 80; ++a) {
            POKE(j, i & 0xff);
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
    // 16-bit text mode, full-colour text for high chars
    VIC4CTRL = 0x05;
    // H640, fast CPU, V400
    VIC3CTRL = 0xC8;
    // Adjust D016 smooth scrolling for VIC-III H640 offset
    VIC2CTRL = 0xC9;
    // 640x400 16bits per char, 8 pixels wide per char
    // = 640/16 x 8 bits = 160 bytes per row
    CHARSTEP_LO = 160;
    CHARSTEP_HI = 0;
    // Draw 80 chars per row
    CHRCOUNT = 80;
    // Put 80x50x2 = 8KB screen at SCREENBASE
    SCNPTR_0 = 0x00;
    SCNPTR_1 = 0xe0;
    SCNPTR_2 = 0x00;
    SCNPTR_3 = 0x00;

    setup_main_screen();

    // Clear colour RAM
    lfill(0xff80000L, 0x00, 80 * 50 * 2);

    BORDERCOL = 0;
    SCREENCOL = 0;
}

void clear_map_tiles(void)
{
    lfill(0x19000, 0x00, 0x6800);
    //  skip DOS ($1f800 - $24000)
    lfill(0x24000, 0x00, 0x8000);
    // skip C64 kernal ($2c000 - $30000)
    lfill(0x30000, 0x00, 0x8000);
    lfill(0x38000, 0x00, 0x8000);
    lfill(0x40000, 0x00, 0x8000);
    lfill(0x48000, 0x00, 0x8000);
    lfill(0x50000, 0x00, 0x8000);
    lfill(0x58000, 0x00, 0x8000);
}

unsigned char assembler[] = {
    // since cc65 doesn't allow 4510 opcodes I have
    // written the assembler in acme, made a hexdump and
    // stored it here
    0xa9, 0x00,       // lda #$00
    0xaa,             // tax
    0xa8,             // tay
    0x4b,             // taz
    0x5c,             // map
    0xa9, 0x36,       // lda #$36 (no basic)
    0x85, 0x01,       // sta $01
    0xa9, 0x47,       // lda #$47
    0x8d, 0x2f, 0xd0, // sta $d02f
    0xa9, 0x53,       // lda #$53
    0x8d, 0x2f, 0xd0, // sta $d02f
    0xea,             // eom
    0xa9, 0x70,       // lda #$70
    0x8d, 0x40, 0xd6, // sta $d640
    0xea,             // nop
    //0xee, 0x20, 0xd0, // inc $d020
    0x60              // rts
};

void main(void) 
{
    //puts("\n");       // cancel leftover quote mode from wrapper or whatever
    //cbm_k_bsout(14);  // lowercase
    //cbm_k_bsout(147); // clr
    start_benchmark();

    // Fast CPU, M65 IO
    mega65_io_enable();

    // bank out the C64/C65 ROM, freeing $2xxxx and $3xxxx.
    // But, assuming that the program is started from C64
    // mode, we still need the kernal, so avoid writing on
    // 2c000 - 2ffff.
    // See the MEGA65 book, page F-11
    asm("jsr %v", assembler);

    clear_map_tiles();
	init_graphics_mode();
	//lpoke(0x19000, 1);
	//lpoke(0x30000, 1);

    //stop_benchmark();
	for(;;) {
    }
}
