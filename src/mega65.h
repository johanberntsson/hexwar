// The bottom layer: MEGA65 I/O enable and the DMAgic primitives.
//
// Everything above 64 KB is reached through these. The names match the ones
// mega65-libc uses (lpeek/lpoke/lcopy/lfill), because the other two MEGA65
// projects in this tree use that library and it is easier to read across them
// than to invent a second vocabulary -- but this is a fresh, much smaller
// implementation. mega65-libc looks abandoned, and hexwar uses six of its
// functions.
#ifndef HEXWAR_MEGA65_H
#define HEXWAR_MEGA65_H

#include <stdint.h>

typedef uint8_t byte;
typedef uint16_t word;
typedef uint32_t dword;

#define POKE(A, V) (*(volatile uint8_t *)(A) = (V))
#define PEEK(A) (*(volatile uint8_t *)(A))

// Put a variable in the 2304 bytes of chip RAM at $1600-$1EFF that the linker
// rules hand to a section called `zpsave`, which nothing here uses. It is
// ordinary near-addressable RAM, so this costs nothing per access -- it is
// simply room that is not part of the 32 KB at $2001. See hexwar.scm.
//
// It sits below $2001 where the C65 ROM keeps its own buffers, so nothing that
// has to survive a KERNAL call belongs in it.
#define LOW_FREE __attribute__((section("zpsave")))

// Unlock the MEGA65 registers ($D02F knock) and switch the CPU to full speed.
// Cheap, and every DMA job does it anyway.
void mega65_io_enable(void);

// 28-bit address space. `count` is 16 bits, which is the DMAgic's own limit --
// a copy or fill of more than 65535 bytes has to be split by the caller, and
// view_clear() in src/view.c is the one place that matters.
byte lpeek(dword address);
void lpoke(dword address, byte value);
void lcopy(dword source, dword destination, word count);
void lfill(dword destination, byte value, word count);
// Fill writing every `skip`th byte, leaving the ones between untouched. Used
// to lay a character down the low byte of a 16-bit screen without disturbing
// the high byte.
void lfill_skip(dword destination, byte value, word count, byte skip);
// Copy, skipping source bytes equal to `transparent`. This is how overlapping
// hex tiles are drawn: palette index 0 is the transparent colour in the sheet,
// so a hexagon blitted over its neighbour leaves the neighbour's corners
// showing rather than a rectangle of background.
void lcopy_transparent(dword source, dword destination, word count,
                       byte transparent);

#endif // HEXWAR_MEGA65_H
