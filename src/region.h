// Code banking: a window at $6000-$7FFF that the 4510's MAP points at one
// region of code at a time, so that there can be more code than the 32 KB
// at $2001 holds.
//
// **Region 0 is the PRG's own $6000-$7FFF**, and it is what the window
// shows whenever nothing else is called: the map is the KERNAL's own then.
// **Regions 1 and up are kept in attic RAM and run from a slot in bank 1**
// (21 Oct 2026). Each is linked to run at $6000, read from SEVENSHIPS.Rr at
// start-up by region_load() into attic at REGION_ATTIC(r), and copied by DMA
// into its slot (slot_of in src/dispatch.s) at SLOT_BASE(), when it is called and the slot
// holds another region -- by set_region in src/dispatch.s, which the window
// is then mapped onto. **Attic RAM cannot be run from** (far too slow for
// MAP), which is why there are slots at all. Two slots fit between screen
// RAM and the tile sheet in bank 1: slot 1 is region 1's alone, and slot 2
// is shared by regions 2 and up, so **there can be as many regions as
// attic and the tables in dispatch.s allow** (eight in attic's bank 1 as
// they stand). A copy is about 8 KB at 16 cycles a byte on a real MEGA65,
// 3.3 ms at most, so it is nothing for per-turn code and per-click
// screens, and **two regions in one slot must not call each other in a
// loop**, or every call is two copies. Bank 1 from $13000 is BASIC 65's
// variable area, which BASIC clears for itself (DIM, RUN), and the slots
// are filled afresh on every run anyway.
//
// A file goes into region N with
//
//     #pragma clang section text="rNtext" rodata="rNrodata"
//
// after its includes, and is compiled with --no-cross-call
// --no-interprocedural-cross-jump (the Makefile's BANKED list), since
// shared instruction fragments could otherwise be called across a window.
// **Every function of it that is called from outside** is renamed inside
// the file -- `#define ai_turn ai_turn_r1` above the includes -- and gets
// a stub of the plain name in src/dispatch.s, which maps the region in,
// calls it and maps the caller's region back. So callers do not change.
//
// **What a region may not do.** Data it hands to resident code -- a string
// literal, a const table -- is only there while the region is mapped, so
// it must not be kept and read afterwards (region 0 is the exception, as
// it is mapped by default). It calls another region's functions only
// through their stubs, and resident code never calls into a window
// directly. Its static and global variables are ordinary BSS and data,
// resident and shared.
//
// **Every MAP change goes through set_region in src/dispatch.s**, which
// also writes the map to $011C-$011F, the KERNAL's copy of the map in
// force: the KERNAL maps back to that after its own MAP changes (an
// interrupt, a disk call), and MAP itself cannot be read. CLAUDE.md,
// "Growing past 32 KB", has how that was found.
#ifndef SEVENSHIPS_REGION_H
#define SEVENSHIPS_REGION_H

#include "mega65.h"

#define REGION_WINDOW 0x6000u
#define REGION_SIZE 0x2000u

// The regions from 1, and where each is kept. **slot_of, attic_bank and
// attic_hi in src/dispatch.s are per region and have to agree**, as do
// REGION_FILES in the Makefile and the winN memories in sevenships.scm.
#define REGIONS 4
#define REGION_ATTIC(R) (0x8010000UL + ((dword)(R) - 1) * REGION_SIZE)
#if REGIONS > 4
#error "add the new region to the tables in src/dispatch.s"
#endif

// The slots in bank 1. Six 8 KB ones would fit below $1F600, where BASIC 65
// keeps something of its own, but **$17000 up is the tile sheet's**
// (SHEET_HIGH in src/sheet.h), so two: slot s is at SLOT_BASE(s).
#define SLOTS 2
#define SLOT_BASE(S) (0x13000UL + ((dword)(S) - 1) * REGION_SIZE)
#define REGION_LIMIT 0x17000UL
#if 0x13000UL + SLOTS * 0x2000UL > REGION_LIMIT
#error "the code slots run into the tile sheet in bank 1"
#endif

// "SEVENSHIPS.R1" and so on; the digit is the last character.
#define REGION_FILE_NAME "SEVENSHIPS.R1"

// Take the KERNAL's map as region 0's. First thing in main().
void region_init(void);

// Read regions 1 to REGIONS into attic RAM, leaving the slots empty. 0 if a file is missing, or not
// the one this build was made with (tools/packprg.py stamps both).
byte region_load(void);

#endif // SEVENSHIPS_REGION_H
