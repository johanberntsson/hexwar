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

// The display as BASIC left it, read back by value rather than set to a
// fixed list, because BASIC 65 boots in 80 columns and a fixed 40-column
// mode left it printing 80-column lines into a 40-column screen, in black.
// This is Search And Rescue's vic4_save_rom/vic4_leave.
//
// The VIC-II and VIC-III registers come first, because with the hot registers
// on a write to one of those recomputes the VIC-IV side; the VIC-IV ones then
// go back exactly as they were, and $D05D -- the hot register bit itself --
// last. File scope, not a static const in a function: see the Calypsi bugs in
// CLAUDE.md. Only the first sixteen colours are kept, since BASIC draws in
// nothing else, and sheet_load() leaves them alone anyway.
static const byte saved_regs[] = {
    0x11, 0x16, 0x18, 0x20, 0x21, 0x31,                    // VIC-II, VIC-III
    0x15, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b,  // VIC-IV
    0x5e, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x68, 0x69,
    0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x70, 0x5d,
};
#define SAVED_REGS sizeof saved_regs
#define SAVED_REGS_LEGACY 6

// HIGH_BSS: written first thing in main() and read last, with every disk
// call in between, and BASIC is still banked out at both ends.
HIGH_BSS static byte saved_values[SAVED_REGS];

void screen_save(void)
{
    byte i;

    mega65_io_enable();
    for (i = 0; i < SAVED_REGS; i++)
        saved_values[i] = PEEK(0xd000u + saved_regs[i]);
}

void screen_restore(void)
{
    byte i;

    mega65_io_enable();
    HOTREG |= 0x80;
    for (i = 0; i < SAVED_REGS; i++) {
        if (i == SAVED_REGS_LEGACY)
            mega65_io_enable();  // a VIC-II write can relock the VIC-IV
        POKE(0xd000u + saved_regs[i], saved_values[i]);
    }
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
    screen_putcodexy(x, y, ascii_to_screencode(c), color);
}

void screen_putcodexy(byte x, byte y, byte code, byte color)
{
    dword cell = SCREEN_CELL(x, y);
    dword col = COLOR_BASE + (cell - SCREEN_BASE);
    lpoke(cell, code);
    lpoke(cell + 1, 0);      // high byte 0: an ordinary text character
    lpoke(col, 0);           // no extended attributes
    lpoke(col + 1, color);
}

void screen_fill(byte x, byte y, byte w, byte color)
{
    dword cell = SCREEN_CELL(x, y);
    dword col = COLOR_BASE + (cell - SCREEN_BASE);
    lfill(cell, 0, 2 * w);
    lfill_skip(cell, 32, w, 2);
    lfill(col, 0, 2 * w);
    lfill_skip(col + 1, color, w, 2);
}

void screen_box(byte x0, byte y0, byte x1, byte y1, byte color)
{
    byte i;
    for (i = y0 + 1; i < y1; ++i) {
        screen_putcodexy(x0, i, BOX_DOWN, color);
        screen_putcodexy(x1, i, BOX_DOWN, color);
    }
    for (i = x0 + 1; i < x1; ++i) {
        screen_putcodexy(i, y0, BOX_ACROSS, color);
        screen_putcodexy(i, y1, BOX_ACROSS, color);
    }
    screen_putcodexy(x0, y0, BOX_TOP_LEFT, color);
    screen_putcodexy(x1, y0, BOX_TOP_RIGHT, color);
    screen_putcodexy(x0, y1, BOX_BOTTOM_LEFT, color);
    screen_putcodexy(x1, y1, BOX_BOTTOM_RIGHT, color);
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
