// Keyboard, mouse and joystick, and the arrow sprite the last two move.
//
// Ported from ~/commodore/mega65/hexgame (src/input.c), which took the
// hardware details -- which registers a 1351 and a joystick are read through,
// and why the read of $DC00 has to be guarded -- from the Ozmoo z6 branch,
// ~/commodore/ozmoo-z6/asm/mouse.asm, the version that has run on a real
// MEGA65. **Two things are different here.** The pointer is a free position
// on the screen rather than a hexagon and an offset into it, because this map
// scrolls and the game wants to know which character is under the tip, not
// which board cell; and the joystick moves the arrow too, rather than
// stepping a cursor, so that both devices do the same thing.
//
// **input_poll() is meant to be called once a frame**, and game_run() does
// exactly that: the joystick's speed, the click debounce and the fake-key
// script below all count calls to it as frames. The KERNAL jiffy clock does
// not advance in this program, so there is no other clock to count against.
#ifndef HEXWAR_INPUT_H
#define HEXWAR_INPUT_H

#include "mega65.h"

// Key codes as the MEGA65's own keyboard buffer at $D610 delivers them: ASCII
// for the printable keys, PETSCII's control codes for the rest.
#define KEY_RUNSTOP 3
#define KEY_RETURN 13
#define KEY_DOWN 17
#define KEY_ESC 27
#define KEY_RIGHT 29
#define KEY_SPACE 32
#define KEY_UP 145
#define KEY_LEFT 157

// Not a key: the mouse button or a joystick's fire, pressed with the arrow at
// (input_x, input_y). $D610 never delivers it -- the F keys start at 241.
#define KEY_CLICK 0xf0

// Where the tip of the arrow is, in sprite coordinates relative to the top
// left of the display: 0-319 across and 0-199 down. **A unit across is two
// H640 pixels, so a character is 4 units wide and 8 tall**; see
// game.c:pointer_hex().
extern word input_x;
extern word input_y;

// Call once, after screen_init(), and after the last file has been read.
void input_init(void);

// Turn the sprites off and give the VIC back the sprite pointer address the
// ROM had, before returning to BASIC.
void input_shutdown(void);

// One frame's input: a key from the keyboard, KEY_CLICK, or 0. Moves the
// arrow for the mouse and the joystick as a side effect.
byte input_poll(void);

// True if the arrow moved since the last time this was asked.
byte input_moved(void);

// **Sprites 2 and 3 are free for the game**; 0 and 1 are the arrow, and are
// drawn in front of them. Each is monochrome, white, unexpanded, with a
// 64-byte block of its own (63 bytes of shape, 3 a row, 24x21) that
// sprite_shape() returns -- ordinary RAM, written directly.
byte *sprite_shape(byte n);
// Show sprite n with its top left at (x, y) in the arrow's coordinates.
void sprite_place(byte n, word x, word y);
void sprite_hide(byte n);

#endif // HEXWAR_INPUT_H
