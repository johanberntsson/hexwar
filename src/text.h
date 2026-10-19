// The game's texts, read from TEXT.PAK into attic RAM at start-up and drawn
// from there, so that none of them is in the 32 KB.
//
// They are written in res/text/, one file a text, with a little markup for
// position, colour, and what differs between a real MEGA65 and xemu;
// tools/mktext.py packs them and describes both. The Makefile lists them,
// and **the order there is the numbering**: TEXT_HELP and the rest are in
// build/textid.h, which mktext.py writes.
#ifndef SEVENSHIPS_TEXT_H
#define SEVENSHIPS_TEXT_H

#include "mega65.h"
#include "textid.h"

#define TEXT_FILE_NAME "TEXT.PAK"

// Where the pack's body goes: the bottom of attic RAM, which nothing else
// uses. **Attic is slow to DMA out of on a real MEGA65** (7x chip RAM), but
// a screen of text is a couple of KB, well under a millisecond.
#define TEXT_ATTIC 0x8000000UL

// Read TEXT_FILE_NAME into attic RAM. 0 if it is missing, or not the pack
// this build was made with. Before LOW_FREE is used, like every disk read.
byte text_load(void);

// Draw text `id` onto the screen as its markup says, over whatever is
// there: the caller clears the screen first if it wants it clear.
void text_show(byte id);

// Read text `id` a byte at a time, for something that does its own drawing
// (popup_add_text()): text_open(), then text_next() until it returns 0.
// Line breaks come as '\n', and {arg N} as TEXT_ARG followed by N.
void text_open(byte id);
byte text_next(void);
#define TEXT_ARG 7

// Line `n` of text `id`, from 0, as a string of at most TEXT_LINE_MAX
// characters; "" past the end. For the lists in res/text/list_*.txt, a name
// or a short line on each. **The string is good until the next call but
// one**: two buffers are used in turn, so that two lines can be held at
// once -- a leader and their title -- and no more.
#define TEXT_LINE_MAX 80
const char *text_line(byte id, byte n);

#endif // SEVENSHIPS_TEXT_H
