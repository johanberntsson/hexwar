// Code banking: a window at $6000-$7FFF that the 4510's MAP points at one
// region of code at a time, so that there can be more code than the 32 KB
// at $2001 holds.
//
// **Region 0 is the PRG's own $6000-$7FFF**, and it is what the window
// shows whenever nothing else is called: the map is the KERNAL's own then.
// **Regions 1 and up are in bank 1**, region r at REGION_BASE(r), linked
// to run at $6000 but loaded there from SEVENSHIPS.Rr at start-up by
// region_load(). Bank 1 from $13000 is BASIC 65's variable area, which BASIC
// clears for itself (DIM, RUN), so they are loaded afresh on every run.
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

// The regions in bank 1: region r of them is at REGION_BASE(r). Six would
// fit below $1F600, where BASIC 65 keeps something of its own, but **regions
// 5 and 6 are the tile sheet's** since milestone 6 (SHEET_HIGH in
// src/sheet.h), so four.
#define REGIONS 2
#define REGION_BASE(R) (0x13000UL + ((dword)(R) - 1) * REGION_SIZE)
#define REGION_LIMIT 0x1B000UL
#if 0x13000UL + REGIONS * 0x2000UL > REGION_LIMIT
#error "the code regions run into the tile sheet in bank 1"
#endif

// "SEVENSHIPS.R1" and so on; the digit is the last character.
#define REGION_FILE_NAME "SEVENSHIPS.R1"

// Take the KERNAL's map as region 0's. First thing in main().
void region_init(void);

// Read regions 1 to REGIONS into bank 1. 0 if a file is missing, or not
// the one this build was made with (tools/packprg.py stamps both).
byte region_load(void);

#endif // SEVENSHIPS_REGION_H
