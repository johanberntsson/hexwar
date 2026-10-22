// Full pictures in the map window (22 Oct 2026): the title's at boot, and
// the end's before the final score -- screens/, converted by
// tools/mkpic.py into res/*.pic, files of their own on the disk.
//
// **The window's tile store is the framebuffer**: the picture is read
// straight into it, 3840 bytes a character row, through a small HIGH_BSS
// buffer, and shows as it comes in. Its 60x23 characters at a byte a
// pixel are exactly the window's, so nothing else has to make room. The
// window's cells must hold their own character numbers (view_init()).
//
// **The picture brings its own palette**, entries 26-255, which are the
// tiles' otherwise: the panel's system colours and the minimap's below
// them are left alone. picture_end() puts the tiles' back from attic RAM,
// where sheet_load() leaves a copy (PALETTE_ATTIC), and the caller redraws
// the view or writes over the window.
//
// **In region 5** (src/region.h), with the score, called through stubs
// from main() and title.c at boot.
#ifndef SEVENSHIPS_PICTURE_H
#define SEVENSHIPS_PICTURE_H

#include "mega65.h"

#define PICTURE_INTRO 0
#define PICTURE_DEFEAT 1
#define PICTURE_SURVIVAL 2
#define PICTURE_HEGEMONY 3

// The tiles' palette, entries 16-255 as the sheet has them: red, then
// green, then blue, 256 bytes each, at the entry's index. Above the texts
// (TEXT_ATTIC) and below the code regions (REGION_ATTIC).
#define PALETTE_ATTIC 0x800f000UL

// Clear the window and show picture `id` in it, palette first. 0 if its
// file is missing or not a picture: then the window is left clear, in the
// tiles' palette.
byte picture_show(byte id);

// Put the tiles' palette back.
void picture_end(void);

#endif // SEVENSHIPS_PICTURE_H
