#include "screen.h"

#define VIC2CTRL (*(volatile byte *)0xd016)
#define BORDERCOL (*(volatile byte *)0xd020)
#define SCREENCOL (*(volatile byte *)0xd021)
#define VIC3CTRL (*(volatile byte *)0xd031)
#define VIC4CTRL (*(volatile byte *)0xd054)
#define CHARSTEP_LO (*(volatile byte *)0xd058)
#define CHARSTEP_HI (*(volatile byte *)0xd059)
#define CHRCOUNT (*(volatile byte *)0xd05e)
#define HOTREG (*(volatile byte *)0xd05d)
#define SCNPTR_0 (*(volatile byte *)0xd060)
#define SCNPTR_1 (*(volatile byte *)0xd061)
#define SCNPTR_2 (*(volatile byte *)0xd062)
#define SCNPTR_3 (*(volatile byte *)0xd063)
#define COLPTR_0 (*(volatile byte *)0xd064)
#define COLPTR_1 (*(volatile byte *)0xd065)
#define PALRED (0xd100u)
#define PALGREEN (0xd200u)
#define PALBLUE (0xd300u)
#define ASCIIKEY (*(volatile byte *)0xd610)

#define SCREEN_CELLS ((word)SCREEN_COLUMNS * SCREEN_ROWS)

void screen_init(void)
{
    mega65_io_enable();

    // **The hot registers have to be on for this part and off for the next.**
    // With bit 7 of $D05D set, a write to a legacy VIC-II or VIC-III register
    // makes the VIC-IV recompute its own layout registers from it -- which is
    // what turns the H640 bit below into a sensible 80-column geometry. Once
    // that is done they must go off, or the hand-written CHRCOUNT, CHARSTEP
    // and SCNPTR further down are undone by the next legacy write.
    HOTREG |= 0x80;

    VIC4CTRL |= 0x01; // 16-bit character numbers
    VIC4CTRL |= 0x04; // full-colour mode for characters numbered 256 and up

    VIC3CTRL |= 0x80; // H640
    VIC3CTRL &= 0xf7; // not V400
    VIC2CTRL |= 0x01; // shift one pixel right, working around a VIC-III bug

    // **The lower case character set, which on a normal boot the KERNAL picks
    // for you.** All it is is bit 1 of $D018, which with the hot registers
    // live moves the VIC-IV's character pointer to the second half of the
    // character ROM. Without it every capital letter comes out as a graphic.
    POKE(53272u, PEEK(53272u) | 0x02);

    HOTREG &= 0x7f;

    // Screen and colour RAM before the VIC is pointed at them, so nothing is
    // ever displayed from uninitialised memory.
    screen_clear();

    CHRCOUNT = SCREEN_COLUMNS;          // characters drawn per row
    CHARSTEP_LO = SCREEN_COLUMNS * 2;   // bytes from one row to the next
    CHARSTEP_HI = 0;

    SCNPTR_0 = SCREEN_BASE & 0xff;
    SCNPTR_1 = (SCREEN_BASE >> 8) & 0xff;
    SCNPTR_2 = (SCREEN_BASE >> 16) & 0xff;
    SCNPTR_3 = (SCNPTR_3 & 0xf0) | ((SCREEN_BASE >> 24) & 0x0f);

    COLPTR_0 = COLOR_RAM_OFFSET & 0xff;
    COLPTR_1 = (COLOR_RAM_OFFSET >> 8) & 0xff;

    BORDERCOL = COLOR_BLACK;
    SCREENCOL = COLOR_BLACK;
}

void screen_clear(void)
{
    // Screen RAM is 16-bit cells: space in the low byte of each, zero in the
    // high byte. Two fills rather than a loop over 2000 cells.
    lfill(SCREEN_BASE, 0, 2 * SCREEN_CELLS);
    lfill_skip(SCREEN_BASE, 32, SCREEN_CELLS, 2);
    lfill(COLOR_BASE, 0, 2 * SCREEN_CELLS);
}

void screen_go8bit(void)
{
    mega65_io_enable();
    VIC3CTRL &= 0x7f;  // H640 off
    VIC3CTRL &= 0xf7;  // V400 off
    VIC4CTRL &= 0xfa;  // 16-bit characters and full-colour mode off
    CHRCOUNT = 40;
    CHARSTEP_LO = 40;
    CHARSTEP_HI = 0;
    SCNPTR_0 = 0x00;   // screen back to $0800
    SCNPTR_1 = 0x08;
    SCNPTR_2 = 0x00;
    SCNPTR_3 &= 0xf0;
    COLPTR_0 = 0x00;   // colour RAM back to $FF80000
    COLPTR_1 = 0x00;
    HOTREG |= 0x80;    // and let the VIC-IV recompute the rest
}

// ASCII, as the source spells it, to a screen code. **Calypsi compiles string
// literals as written; cc65 compiled them into PETSCII.** That difference is
// the first thing to suspect when text comes out with its case inverted, and
// it is why this function exists at all -- the cc65 build simply poked the
// literal's bytes at the screen.
static byte ascii_to_screencode(char c)
{
    byte u = (byte)c;
    if (u == '_') return 100;               // the underline glyph
    if (u == '@') return 0;
    if (u >= 'a' && u <= 'z') return u - 0x60;  // 1-26
    if (u > 'Z' && u < 'a') return u - 0x40;    // [ \ ] ^ at 27-30
    return u;   // digits, punctuation, space and A-Z are screen codes already
}

void screen_putcxy(byte x, byte y, char c, byte color)
{
    dword cell = SCREEN_CELL(x, y);
    dword col = COLOR_BASE + (cell - SCREEN_BASE);
    lpoke(cell, ascii_to_screencode(c));
    lpoke(cell + 1, 0);      // high byte 0: an ordinary text character
    lpoke(col, 0);           // no extended attributes
    lpoke(col + 1, color);
}

void screen_putsxy(byte x, byte y, const char *s, byte color)
{
    while (*s && x < SCREEN_COLUMNS) {
        screen_putcxy(x, y, *s, color);
        ++x;
        ++s;
    }
}

// The VIC-IV wants each palette component's nibbles the other way round from
// how an FCI file stores them.
static byte swap_nibbles(byte v)
{
    return (byte)((v >> 4) | (v << 4));
}

void screen_set_palette_entry(byte index, byte red, byte green, byte blue)
{
    POKE(PALRED + index, swap_nibbles(red));
    POKE(PALGREEN + index, swap_nibbles(green));
    POKE(PALBLUE + index, swap_nibbles(blue));
}

byte screen_getkey(void)
{
    byte key = ASCIIKEY;
    if (key) ASCIIKEY = 0;   // writing anything acknowledges the key
    return key;
}

void screen_wait_raster(byte line)
{
    // Wait for a fresh crossing rather than returning at once if the beam
    // happens to be below the line already: otherwise a redraw that finished
    // late gets no head start at all and tears on the next frame.
    while (PEEK(0xd012u) >= line) continue;
    while (PEEK(0xd012u) < line) continue;
}
