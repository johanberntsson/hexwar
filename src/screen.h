// The VIC-IV display: 16-bit full-colour text mode, palette, text output and
// the keyboard.
//
// **The screen is a grid of characters, and there is no bitmap mode.** Each
// screen cell is *two* bytes holding a 16-bit character number. A character
// number below 256 draws the usual 8x8 monochrome glyph from the character
// ROM, which is what the panel text is; a number of 256 or more draws a
// full-colour character -- 64 bytes, one byte of palette index per pixel --
// found at `number * 64` as an absolute 28-bit address. That is what the map
// view is made of, and why the view's tile store at $40000 can be pointed at
// directly: $40000 / 64 = 0x1000, comfortably past 256.
#ifndef HEXWAR_SCREEN_H
#define HEXWAR_SCREEN_H

#include "mega65.h"

// H640 / V200: 80x25 characters over 640x200 pixels. This is the mode the
// XC=BASIC version used and what basic/civ-basic.png shows. V400 would give
// 80x50 and square pixels, at four times the tile store and half the apparent
// tile size; see the note on x-expansion in src/sheet.h.
#define SCREEN_COLUMNS 80
#define SCREEN_ROWS 25

// Screen RAM: 80 * 25 * 2 = 4000 bytes in bank 1.
//
// **Not $E000, where the cc65 build had it.** That address is under the
// KERNAL, so every read and write of a screen cell had to be bracketed by an
// `sei` and a flip of $01 -- the ENABLE_SCREEN_ACCESS / DISABLE_SCREEN_ACCESS
// macros in old-cc65/src/graphics.c. Bank 1 is plain RAM that nothing else in
// this program wants, and the whole dance goes away.
#define SCREEN_BASE 0x12000UL

// Colour RAM, 2 bytes per cell. The hardware default is $FF80000, where CBDOS
// keeps buffers of its own and corrupts the display; $D064/$D065 move it, and
// $1000 is the offset mega65-libc settled on.
#define COLOR_RAM_OFFSET 0x1000
#define COLOR_BASE (0xFF80000UL + COLOR_RAM_OFFSET)

// Colour RAM byte 1 carries the colour in its low nibble and four attribute
// bits above it. All four off is ordinary text.
#define ATTR_BLINK 0x10
#define ATTR_REVERSE 0x20
#define ATTR_BOLD 0x40
#define ATTR_UNDERLINE 0x80

#define COLOR_BLACK 0
#define COLOR_WHITE 1
#define COLOR_RED 2
#define COLOR_CYAN 3
#define COLOR_GREEN 5
#define COLOR_BLUE 6
#define COLOR_YELLOW 7
#define COLOR_LIGHTGREEN 13
#define COLOR_LIGHTGREY 15

// Bring up the display mode and clear it. Everything else here assumes it has
// run.
void screen_init(void);

// Put the machine back into the 40-column 8-bit text mode the ROM expects, so
// that returning from main() lands on a usable BASIC screen. Cheap because
// this program never banked the ROM out in the first place -- see hexwar.scm.
void screen_go8bit(void);

// Address of a cell in screen RAM. Handy for anything writing screen words
// directly, which view_init() does.
#define SCREEN_CELL(X, Y) \
    (SCREEN_BASE + 2UL * ((word)(X) + (word)(Y) * SCREEN_COLUMNS))

void screen_clear(void);
// Text is written in ASCII as it appears in the source. Calypsi, unlike cc65,
// does not compile string literals into PETSCII, so the translation to screen
// codes happens here rather than in the compiler.
void screen_putcxy(byte x, byte y, char c, byte color);
void screen_putsxy(byte x, byte y, const char *s, byte color);
// A screen code as it is, for the glyphs ASCII has no letter for.
void screen_putcodexy(byte x, byte y, byte code, byte color);

// `w` blank cells from (x, y), in `color` -- which with ATTR_REVERSE is a
// solid bar. A few DMA jobs, where w screen_putcxy calls would be 4w.
void screen_fill(byte x, byte y, byte w, byte color);

// Box-drawing screen codes, the same in the upper and the lower case sets.
#define BOX_TOP_LEFT 112
#define BOX_TOP_RIGHT 110
#define BOX_BOTTOM_LEFT 109
#define BOX_BOTTOM_RIGHT 125
#define BOX_ACROSS 64
#define BOX_DOWN 93

// A frame of box glyphs whose outside corners are (x0, y0) and (x1, y1).
void screen_box(byte x0, byte y0, byte x1, byte y1, byte color);

// A whole palette entry. `red`, `green` and `blue` are the FCI file's byte
// values; the nibble swap the VIC-IV wants is done here.
void screen_set_palette_entry(byte index, byte red, byte green, byte blue);

// The keyboard, straight off the MEGA65's ASCII key register. Returns 0 when
// nothing is waiting. Cursor keys come back as their PETSCII codes (17 down,
// 29 right, 145 up, 157 left).
byte screen_getkey(void);

// Block until the raster passes `line`, having first been above it, so the
// caller always gets a fresh crossing rather than returning immediately.
//
// $D012 is the VIC-II raster line, 0-311 on PAL; the 200 visible lines of a
// V200 screen run from about 51 to 250, so character row c is around line
// 51 + 8c. See SCREEN_ROW_LINE and view_wait_for_redraw().
void screen_wait_raster(byte line);

// The first raster line of character row `c`.
#define SCREEN_ROW_LINE(C) ((byte)(51 + 8 * (C)))

#endif // HEXWAR_SCREEN_H
