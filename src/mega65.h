// The bottom layer: MEGA65 I/O enable and the DMAgic primitives.
//
// Everything above 64 KB is reached through these. The names match the ones
// mega65-libc uses (lpeek/lpoke/lcopy/lfill), because the other two MEGA65
// projects in this tree use that library and it is easier to read across them
// than to invent a second vocabulary -- but this is a fresh, much smaller
// implementation. mega65-libc looks abandoned, and sevenships uses six of its
// functions.
#ifndef SEVENSHIPS_MEGA65_H
#define SEVENSHIPS_MEGA65_H

#include <stdint.h>

typedef uint8_t byte;
typedef uint16_t word;
typedef uint32_t dword;

#define POKE(A, V) (*(volatile uint8_t *)(A) = (V))
#define PEEK(A) (*(volatile uint8_t *)(A))

// Put a variable in the chip RAM at $1600-$1EFF, after the 56 bytes of
// `zpsave` where BASIC's zero page is kept during the run (sevenships.scm). It is
// ordinary near-addressable RAM, so this costs nothing per access -- it is
// simply room that is not part of the 32 KB at $2001. See sevenships.scm.
//
// It sits below $2001 where the C65 ROM keeps its own buffers, so nothing that
// has to survive a KERNAL call belongs in it.
#define LOW_FREE __attribute__((section("lowfree")))

// **Put a variable in the 8 KB of RAM under the BASIC ROM, $A000-$BFFF**,
// which src/bank.s banks in before main() and sevenships.scm hands to a section
// called `highbss`. Ordinary 16-bit addresses, so it costs nothing per access
// either. **Uninitialised data only**: the PRG is loaded with BASIC still
// mapped, so nothing that has to arrive from the disk can live up there, and
// the linker script is what keeps it out. **Nor is it cleared by the
// startup**: whatever is put there must be set before it is read, as
// unit_reset() and city_reset() do for a new game.
#define HIGH_BSS __attribute__((section("highbss")))

// BASIC back over $A000-$BFFF, as the ROM had it. The last thing before
// main() returns; HIGH_BSS is gone after it.
void bank_basic_in(void);

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

// **The fast path, and the reason it exists.**
//
// A 512-byte lcopy measured 84.8 us, of which the DMAgic itself is 27.2 --
// the other 57.6 was this file taking two 32-bit addresses apart into
// megabyte, bank and offset. Calypsi has no cheap way to do `>> 20` on a
// dword, and a redraw does it 320 times.
//
// So the hot path splits the addresses *once*. blit_begin() sets everything
// that stays put for a run -- both megabytes, the source bank, the transparent
// byte -- and blit() then stores only four fields, all of them 8 or 16 bit.
// The caller is responsible for keeping a blit inside one 64 KB bank; see the
// padded row stride in src/view.h, which is arranged so it always is.
void blit_begin(byte source_mb, byte source_bank, byte dest_mb,
                byte transparent);
void blit(word source_addr, byte dest_bank, word dest_addr, word count);

#endif // SEVENSHIPS_MEGA65_H
