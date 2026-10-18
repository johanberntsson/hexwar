#include <calypsi/intrinsics6502.h>

#include "input.h"

// The MEGA65's own keyboard buffer: one key, ASCII, cleared by writing 0.
// This is the machine's scan of the real keyboard, not the KERNAL's, which is
// why a held joystick does not arrive here as phantom key presses.
#define KEYBOARD 0xd610u

// The 1351's counters, the VIC-IV's own pot registers read directly. **Which
// control port this pair belongs to is not the same in xemu as on the
// machine** (Ozmoo found the mouse on port 1 on real hardware and on port 2
// under xemu), so the button is taken from both ports below.
// What $D610 delivers for HELP (mega65-core's matrix_to_ascii.vhdl, which
// xemu copies), handed on as KEY_HELP.
#define KEYBOARD_HELP 0x1f

// Bit 5 of $D60F is set on a real MEGA65 and clear in xemu (or a VHDL
// simulator).
#define REALHW 0xd60fu
#define REALHW_BIT 0x20

#define POTX 0xd620u
#define POTY 0xd621u

// The joystick and the fire buttons. $DC01 is control port 1, $DC00 is port
// 2 -- and also the keyboard column select, which is the whole reason
// read_ports() exists. Every bit is active low.
#define JOY_PORT1 0xdc01u
#define JOY_PORT2 0xdc00u
#define JOY_COLUMNS 0xdc00u
#define JOY_UP 0x01
#define JOY_DOWN 0x02
#define JOY_LEFT 0x04
#define JOY_RIGHT 0x08
#define JOY_FIRE 0x10
#define JOY_DIRS 0x0f

#define SPR_ENABLE 0xd015u
#define SPR0_X 0xd000u
#define SPR0_Y 0xd001u
#define SPR_XMSB 0xd010u
#define SPR_YEXPAND 0xd017u
#define SPR_PRIORITY 0xd01bu    // 0 = the sprite is drawn in front
#define SPR_MULTICOLOUR 0xd01cu
#define SPR_XEXPAND 0xd01du
#define SPR0_COLOUR 0xd027u     // sprite 1's is the byte after it
#define SPRPTRADR 0xd06cu       // three bytes; bit 7 of the third: 16-bit

// Sprite coordinates of the display's top left corner. The VIC-IV keeps the
// VIC-II's 320x200 sprite space whatever the text mode, so at H640/V200 a
// sprite unit is two pixels across and one line down.
#define SPR_X_OFFSET 24
#define SPR_Y_OFFSET 50

// Ignore the button for this many frames after a click: longer than any
// bounce, shorter than anyone clicks twice on purpose.
#define CLICK_DEBOUNCE 6

// The joystick moves the arrow a unit a frame for the first JOY_SLOW frames
// it is held, so a short push can place it exactly, then JOY_FAST units a
// frame, which crosses the screen in under two seconds.
#define JOY_SLOW 10
#define JOY_FAST 4

word input_x;
word input_y;
byte input_real_hw;

// **A smaller arrow than hexgame's.** That one was 18 sprite pixels tall; at
// H640/V200 a monochrome sprite pixel is two pixels by one line, which is
// square on the glass and the same size as a pixel of the x-expanded tiles,
// so the arrow is drawn at exactly the scale of the map. This one is 11 tall.
//
// Two monochrome sprites, as in hexgame: sprite 0 is the body, sprite 1 the
// same shape grown by a pixel, behind it, for an edge that reads over any
// terrain. A multicolour sprite pixel would be twice as wide again.
//
// The tip of the body is at sprite pixel (1, 1), leaving room for the outline
// above and to the left of it; see ARROW_HOT.
#define ARROW_HOT 1
#define ARROW_BYTES 39          // 13 rows; the rest of the 63 are blank

static const byte arrow_body[ARROW_BYTES] = {
    0x00, 0x00, 0x00,           // ..........
    0x40, 0x00, 0x00,           // .#........
    0x60, 0x00, 0x00,           // .##.......
    0x70, 0x00, 0x00,           // .###......
    0x78, 0x00, 0x00,           // .####.....
    0x7c, 0x00, 0x00,           // .#####....
    0x7e, 0x00, 0x00,           // .######...
    0x7f, 0x00, 0x00,           // .#######..
    0x78, 0x00, 0x00,           // .####.....
    0x6c, 0x00, 0x00,           // .##.##....
    0x46, 0x00, 0x00,           // .#...##...
    0x06, 0x00, 0x00,           // .....##...
    0x00, 0x00, 0x00,           // ..........
};

static const byte arrow_outline[ARROW_BYTES] = {
    0xe0, 0x00, 0x00,           // ###.......
    0xf0, 0x00, 0x00,           // ####......
    0xf8, 0x00, 0x00,           // #####.....
    0xfc, 0x00, 0x00,           // ######....
    0xfe, 0x00, 0x00,           // #######...
    0xff, 0x00, 0x00,           // ########..
    0xff, 0x80, 0x00,           // #########.
    0xff, 0x80, 0x00,           // #########.
    0xff, 0x80, 0x00,           // #########.
    0xff, 0x00, 0x00,           // ########..
    0xff, 0x00, 0x00,           // ########..
    0xef, 0x00, 0x00,           // ###.####..
    0x0f, 0x00, 0x00,           // ....####..
};

// **Everything the sprite hardware is pointed at has to be 64-byte aligned,
// the pointer list included.** hexgame learnt the second half of that the
// hard way: given a list at $1703 the VIC-IV reads one at $1700. There is no
// way to ask C for alignment, so this is one buffer with a block of slack and
// the things are placed inside it by hand:
//
//      +0     the pointer list SPRPTRADR is given
//      +64    sprite 0, the arrow
//      +128   sprite 1, its outline
//      +192   sprite 2, free for sprite_shape() -- the minimap's view frame
//      +256   sprite 3, likewise
//
// It is HIGH_BSS rather than LOW_FREE, because $1600-$1EFF is where the
// ROM keeps disk buffers and a later load or save would scribble on it. The
// VIC-IV reads sprites from RAM whatever the CPU has mapped, so the BASIC
// ROM's being over $A000 at times makes no difference to it.
HIGH_BSS static byte sprite_ram[64 + 64 + 4 * 64];
static word sprite_list;         // the aligned start inside it

static byte saved_ptr[3];       // SPRPTRADR as the ROM left it
static byte prev_potx, prev_poty;
static byte button_held;
static byte click_timer;        // frames left of the debounce window
static byte joy_held;           // frames the stick has been held over
static byte moved;

// **Faking input for a headless run**, which can press nothing. Build with
// `make FAKE_KEYS='{60,0,0,98},{120,200,96,KEY_CLICK}'`: each entry is a
// frame number, an arrow position (0,0 to leave it alone) and a key that
// input_poll() then returns on that frame. The entries must be in frame
// order, and the script stops at the end rather than wrapping, or the
// presses would replay.
#ifdef FAKE_KEYS
struct fake_key { word frame; word x; word y; byte key; };
static const struct fake_key fake_keys[] = { FAKE_KEYS };
#define FAKE_KEY_COUNT (sizeof fake_keys / sizeof fake_keys[0])
static word frame;
static byte fake_next;
#endif

// Read control port 1's lines, and port 2's fire button into *fire2.
//
// $DC00 is the keyboard column select as well as control port 2, so a read of
// either port can be a key rather than a device. The columns are deselected
// across the read, with interrupts off so the KERNAL's scan cannot select one
// back in between -- but only when a line we care about reads low, because
// writing $FF there also sets the SID's paddle select, a few instructions
// from the pot reads, which an emulator taking the C64 path minds.
static byte read_ports(byte *fire2)
{
    byte p1, save;

    p1 = PEEK(JOY_PORT1);
    if ((p1 & (JOY_DIRS | JOY_FIRE)) == (JOY_DIRS | JOY_FIRE)
        && (PEEK(JOY_PORT2) & JOY_FIRE) != 0) {
        *fire2 = JOY_FIRE;
        return 0xff;
    }

    __disable_interrupts();
    save = PEEK(JOY_COLUMNS);
    POKE(JOY_COLUMNS, 0xff);
    *fire2 = PEEK(JOY_PORT2) & JOY_FIRE;
    p1 = PEEK(JOY_PORT1);
    POKE(JOY_COLUMNS, save);
    __enable_interrupts();
    return p1;
}

// The pot is a six-bit counter that wraps, so what a reading means is the
// signed change since the last one. Fold cur - prev into -32..31.
static int pot_delta(byte cur, byte prev)
{
    byte d = (byte)((byte)(cur - prev + 0x20) & 0x3f);
    return (int)d - 0x20;
}

static byte read_pot(word reg)
{
    return (byte)((PEEK(reg) >> 1) & 0x3f);
}

static void place_pointer(void)
{
    word x = input_x + SPR_X_OFFSET - ARROW_HOT;
    byte y = (byte)(input_y + SPR_Y_OFFSET - ARROW_HOT);

    POKE(SPR0_X, (byte)x);
    POKE(SPR0_Y, y);
    POKE(SPR0_X + 2, (byte)x);          // sprite 1 rides under sprite 0
    POKE(SPR0_Y + 2, y);
    POKE(SPR_XMSB, (PEEK(SPR_XMSB) & 0xfc) | ((x >> 8) ? 3 : 0));
}

// Move the arrow by (dx, dy), kept on the screen.
static void push_pointer(int dx, int dy)
{
    int x = (int)input_x + dx;
    int y = (int)input_y + dy;
    if (x < 0) x = 0;
    if (x > 319) x = 319;
    if (y < 0) y = 0;
    if (y > 199) y = 199;
    if ((word)x != input_x || (word)y != input_y) {
        input_x = (word)x;
        input_y = (word)y;
        moved = 1;
        place_pointer();
    }
}

void input_init(void)
{
    word list = ((word)sprite_ram + 63) & 0xffc0;
    word body = list + 64;
    word ptr;
    byte i;

    input_real_hw = PEEK(REALHW) & REALHW_BIT;
    sprite_list = list;
    for (ptr = 0; ptr < 4 * 64; ++ptr) POKE(body + ptr, 0);
    for (i = 0; i < ARROW_BYTES; ++i) {
        POKE(body + i, arrow_body[i]);
        POKE(body + 64 + i, arrow_outline[i]);
    }
    // Sprites 0-3 each point at their own block; the four never turned on
    // point at the arrow, so that they point somewhere valid.
    for (i = 0; i < 8; ++i) {
        ptr = body / 64 + (i < 4 ? i : 0);
        POKE(list + 2 * i, (byte)ptr);
        POKE(list + 2 * i + 1, (byte)(ptr >> 8));
    }

    for (i = 0; i < 3; ++i) saved_ptr[i] = PEEK(SPRPTRADR + i);
    POKE(SPRPTRADR, (byte)list);
    POKE(SPRPTRADR + 1, (byte)(list >> 8));
    POKE(SPRPTRADR + 2, 0x80);          // bank 0, and 16-bit pointers

    POKE(SPR0_COLOUR, 1);               // white
    POKE(SPR0_COLOUR + 1, 0);           // black
    POKE(SPR0_COLOUR + 2, 1);           // white, both
    POKE(SPR0_COLOUR + 3, 1);
    POKE(SPR_MULTICOLOUR, PEEK(SPR_MULTICOLOUR) & 0xf0);
    POKE(SPR_PRIORITY, PEEK(SPR_PRIORITY) & 0xf0);
    POKE(SPR_XEXPAND, PEEK(SPR_XEXPAND) & 0xf0);
    POKE(SPR_YEXPAND, PEEK(SPR_YEXPAND) & 0xf0);

    // Seed the counters with what they read now, so the first poll sees no
    // movement rather than the difference from zero.
    prev_potx = read_pot(POTX);
    prev_poty = read_pot(POTY);

    // The middle of the map window.
    input_x = 160;
    input_y = 100;
    place_pointer();
    POKE(SPR_ENABLE, PEEK(SPR_ENABLE) | 3);
    POKE(KEYBOARD, 0);
}

void input_shutdown(void)
{
    byte i;
    POKE(SPR_ENABLE, PEEK(SPR_ENABLE) & 0xf0);
    for (i = 0; i < 3; ++i) POKE(SPRPTRADR + i, saved_ptr[i]);
}

byte *sprite_shape(byte n)
{
    return (byte *)(sprite_list + 64 * (word)(n + 1));
}

void sprite_place(byte n, word x, word y)
{
    byte bit = (byte)(1 << n);
    x += SPR_X_OFFSET;
    POKE(SPR0_X + 2 * n, (byte)x);
    POKE(SPR0_Y + 2 * n, (byte)(y + SPR_Y_OFFSET));
    if (x >> 8) POKE(SPR_XMSB, PEEK(SPR_XMSB) | bit);
    else POKE(SPR_XMSB, PEEK(SPR_XMSB) & (byte)~bit);
    POKE(SPR_ENABLE, PEEK(SPR_ENABLE) | bit);
}

void sprite_hide(byte n)
{
    POKE(SPR_ENABLE, PEEK(SPR_ENABLE) & (byte)~(1 << n));
}

byte input_moved(void)
{
    byte m = moved;
    moved = 0;
    return m;
}

byte input_poll(void)
{
    byte pot, joy, fire2, key, pressed;
    int dx, dy;

#ifdef FAKE_KEYS
    ++frame;
    if (fake_next < FAKE_KEY_COUNT && fake_keys[fake_next].frame == frame) {
        const struct fake_key *f = &fake_keys[fake_next++];
        if (f->x || f->y) push_pointer((int)f->x - (int)input_x,
                                       (int)f->y - (int)input_y);
        return f->key;
    }
#endif

    // --- the mouse: difference the pots and push the arrow ---
    pot = read_pot(POTX);
    dx = pot_delta(pot, prev_potx);
    prev_potx = pot;
    pot = read_pot(POTY);
    dy = pot_delta(pot, prev_poty);
    prev_poty = pot;

    // --- the joystick pushes it as well ---
    joy = read_ports(&fire2);
    if ((joy & JOY_DIRS) != JOY_DIRS) {
        byte step = (joy_held < JOY_SLOW) ? 1 : JOY_FAST;
        if (joy_held < 255) ++joy_held;
        if (!(joy & JOY_LEFT)) dx -= step;
        if (!(joy & JOY_RIGHT)) dx += step;
        if (!(joy & JOY_UP)) dy += step;    // the pot's y runs upwards
        if (!(joy & JOY_DOWN)) dy -= step;
    } else {
        joy_held = 0;
    }
    if (dx || dy) push_pointer(dx, -dy);

    // --- the keyboard ---
    key = PEEK(KEYBOARD);
    if (key) {
        POKE(KEYBOARD, 0);
        return key == KEYBOARD_HELP ? KEY_HELP : key;
    }

    // --- either fire button, on either port, is the mouse button ---
    if (click_timer) --click_timer;
    pressed = !(joy & JOY_FIRE) || !fire2;
    if (pressed) {
        if (!button_held) {
            button_held = 1;
            if (!click_timer) {
                click_timer = CLICK_DEBOUNCE;
                return KEY_CLICK;       // a press edge is a click
            }
        }
    } else {
        button_held = 0;
    }
    return 0;
}
