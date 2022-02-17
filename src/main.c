#include <stdio.h>

#include <hal.h>
#include <memory.h>
#include <dirent.h>
#include <fileio.h>

void graphics_mode(void) {
  // 16-bit text mode, full-colour text for high chars
  POKE(0xD054, 0x05);
  // H640, fast CPU, V400
  POKE(0xD031, 0xC8);
  // Adjust D016 smooth scrolling for VIC-III H640 offset
  POKE(0xD016, 0xC9);
  // 640x400 16bits per char, 8 pixels wide per char
  // = 640/16 x 8 bits = 160 bytes per row
  POKE(0xD058, 160);
  POKE(0xD059, 160 / 256);
  // Draw 80 chars per row
  POKE(0xD05E, 80);
  // Put 80x50x2 = 8KB screen at $E000
  POKE(0xD060, 0x00);
  POKE(0xD061, 0xE0);
  POKE(0xD062, 0x00);

  // Clear colour RAM
  //lfill(0xff80000L, 0x00, 80 * 50 * 2);

  POKE(0xD020, 0);
  POKE(0xD021, 0);
}

void graphics_clear_screen(void) {
  lfill(0x40000, 0x00, 0x8000);
  lfill(0x48000, 0x00, 0x8000);
  lfill(0x50000, 0x00, 0x8000);
  lfill(0x58000, 0x00, 0x8000);
  lfill(0x30000, 0x00, 0x8000);
  lfill(0x38000, 0x00, 0x8000);
  lfill(0x12000, 0x00, 0x8000);
  lfill(0x1A000, 0x00, 0x5800);
  lfill(0x0A000, 0x00, 0x4000);
}

void main(void) {
    unsigned char ch;

    // Fast CPU, M65 IO
    POKE(0, 65);
    POKE(0xD02F, 0x47);
    POKE(0xD02F, 0x53);

    // Make sure ROM area is writeable
    ch = lpeek(0x30000);
    lpoke(0x30000, ch + 1);
    if (lpeek(0x30000) == ch)
        toggle_rom_write_protect();
    lpoke(0x30000, ch);

    graphics_clear_screen();
	graphics_mode();
	printf("Hello, world!\n");
}
