// Popups over the map.
//
// **A popup writes ordinary text characters over the map window's screen
// cells, and view_restore() puts the window's own character numbers back.**
// The window's cells are otherwise never rewritten after view_init(), and
// the tile store under the popup is not touched at all, so closing one costs
// a few small DMA jobs and no redraw. That is also why a popup has to stay
// inside the window: outside it there is panel text that nothing would put
// back.

#include "popup.h"
#include "input.h"
#include "screen.h"
#include "text.h"
#include "view.h"

// The text, wrapped to at most TEXT_WIDTH columns and MAX_LINES lines; a
// line past that is not drawn. The box is the text with a space and a
// border round it, a blank line, and the OK line.
#define TEXT_WIDTH 40
#define MAX_LINES (VIEW_ROWS - 6)
#define TEXT_SIZE 400

#define TEXT_COLOUR COLOR_WHITE
#define BORDER_COLOUR COLOR_YELLOW

// In LOW_FREE, out of the 32 KB. That memory does not survive a disk call,
// but a popup's text is put together and shown with none in between.
LOW_FREE static char text[TEXT_SIZE + 1];
static word text_len;

void popup_begin(void)
{
    text_len = 0;
    text[0] = 0;
}

void popup_add(const char *s)
{
    while (*s && text_len < TEXT_SIZE) text[text_len++] = *s++;
    text[text_len] = 0;
}

void popup_add_num(word v)
{
    char digits[6];
    byte i = sizeof digits - 1;
    digits[i] = 0;
    do { digits[--i] = (char)('0' + v % 10); v /= 10; } while (v);
    popup_add(digits + i);
}

void popup_add_text(byte id, const char *const *args)
{
    byte c;

    text_open(id);
    while ((c = text_next()) != 0) {
        if (c == TEXT_ARG) {
            popup_add(args[text_next()]);
        } else if (text_len < TEXT_SIZE) {
            text[text_len++] = c;
            text[text_len] = 0;
        }
    }
}

// Word-wrap the text, and draw it with its top left at (x, y) if `draw`.
// Returns the number of lines; the widest goes in *widest.
static byte wrap(byte draw, byte x, byte y, byte *widest)
{
    const char *p = text;
    byte lines = 0, n, brk, i;

    *widest = 0;
    while (*p && lines < MAX_LINES) {
        // The longest run that fits, cut back to the last space if that
        // would split a word. A word longer than a line is split anyway.
        n = 0;
        brk = 0;
        while (p[n] && p[n] != '\n' && n < TEXT_WIDTH) {
            if (p[n] == ' ') brk = n;
            ++n;
        }
        if (p[n] && p[n] != '\n' && p[n] != ' ' && brk) n = brk;
        if (n > *widest) *widest = n;
        if (draw)
            for (i = 0; i < n; ++i)
                screen_putcxy(x + i, y + lines, p[i], TEXT_COLOUR);
        ++lines;
        p += n;
        if (*p == ' ' || *p == '\n') ++p;
    }
    return lines;
}

// The buttons along the bottom of the box: one OK, or a pair of answers
// (POPUP_YES_NO and the rest, indexing these), the first of which is 1.
// Each answer's key is the first letter of its button.
#define OK_TEXT "[ OK ]"
#define BUTTON_GAP 4

static const char *const first_text[] = { 0, "[ Yes ]", "[ Move unit ]" };
static const char *const second_text[] = { 0, "[ No ]", "[ View city ]" };
static const byte first_len[] = { 0, 7, 13 };
static const byte second_len[] = { 0, 6, 13 };
static const byte first_key[] = { 0, 'y', 'm' };
static const byte second_key[] = { 0, 'n', 'v' };

// Whether the arrow is on row `row`, from column `x` for `w` columns.
static byte pointer_on(byte row, byte x, byte w)
{
    byte col = (byte)(input_x >> 2);
    return (byte)(input_y >> 3) == row && col >= x && col < x + w;
}

#define NO_X(BX) ((BX) + first_len[pair] + BUTTON_GAP)

// Draw the pair's buttons on row `by` from column `bx`, the one `yes` says
// in reverse.
static void draw_buttons(byte pair, byte bx, byte by, byte yes)
{
    screen_putsxy(bx, by, first_text[pair],
                  yes ? (ATTR_REVERSE | COLOR_YELLOW) : COLOR_YELLOW);
    screen_putsxy(NO_X(bx), by, second_text[pair],
                  yes ? COLOR_YELLOW : (ATTR_REVERSE | COLOR_YELLOW));
}

// Show the text with OK, or with the buttons of `pair` if it is not 0, and
// return 1 for OK or the first button, 0 for the second.
static byte show(byte pair)
{
    byte lines, widest, w, h, x0, y0, y, key, bx, by, answer, yes = 0;
    // OK's box is as wide as Yes and No's.
    byte b = pair ? pair : POPUP_YES_NO;
    byte ask_width = first_len[b] + BUTTON_GAP + second_len[b];

    lines = wrap(0, 0, 0, &widest);
    if (widest < ask_width) widest = ask_width;
    w = widest + 4;
    h = lines + 5;
    x0 = VIEW_LEFT + (VIEW_COLUMNS - w) / 2;
    y0 = VIEW_TOP + (VIEW_ROWS - h) / 2;

    for (y = y0 + 1; y < y0 + h - 1; ++y)
        screen_fill(x0 + 1, y, w - 2, TEXT_COLOUR);
    screen_box(x0, y0, x0 + w - 1, y0 + h - 1, BORDER_COLOUR);

    wrap(1, x0 + 2, y0 + 1, &widest);
    by = y0 + h - 2;
    if (pair) {
        bx = x0 + (w - ask_width) / 2;
        draw_buttons(pair, bx, by, yes);
        input_moved();          // only a move from here on selects
    } else {
        screen_putsxy(x0 + (w - (sizeof OK_TEXT - 1)) / 2, by, OK_TEXT,
                      COLOR_YELLOW);
    }

    for (;;) {
        screen_wait_raster(SCREEN_ROW_LINE(SCREEN_ROWS - 1));
        key = input_poll();
        if (key >= 'A' && key <= 'Z') key += 'a' - 'A';
        if (!pair) {
            // Anywhere is OK: a click need not be on the button.
            if (key == KEY_CLICK || key == KEY_RETURN || key == KEY_SPACE
                || key == KEY_ESC || key == KEY_RUNSTOP) {
                answer = 1;
                break;
            }
            continue;
        }
        // A question needs the click on a button, since either answer
        // matters. From the keyboard the cursor keys move the highlight,
        // which starts on No so that a stray Return does nothing, and
        // Return or space picks it; y and n answer at once, Esc is No.
        answer = yes;
        if (input_moved()) {
            if (pointer_on(by, bx, first_len[pair])) answer = 1;
            if (pointer_on(by, NO_X(bx), second_len[pair])) answer = 0;
        }
        if (key == KEY_LEFT) answer = 1;
        if (key == KEY_RIGHT) answer = 0;
        if (answer != yes) draw_buttons(pair, bx, by, yes = answer);
        if (key == KEY_RETURN || key == KEY_SPACE) break;
        if (key == first_key[pair]
            || (key == KEY_CLICK && pointer_on(by, bx, first_len[pair]))) {
            answer = 1;
            break;
        }
        if (key == second_key[pair] || key == KEY_ESC || key == KEY_RUNSTOP
            || (key == KEY_CLICK
                && pointer_on(by, NO_X(bx), second_len[pair]))) {
            answer = 0;
            break;
        }
    }

    view_restore(x0 - VIEW_LEFT, y0 - VIEW_TOP, w, h);
    return answer;
}

void popup_show(void)
{
    show(0);
}

byte popup_ask(const char *s)
{
    return popup_choose(s, POPUP_YES_NO);
}

byte popup_ask_shown(void)
{
    return show(POPUP_YES_NO);
}

byte popup_choose(const char *s, byte pair)
{
    popup_begin();
    popup_add(s);
    return show(pair);
}

void popup(const char *s)
{
    popup_begin();
    popup_add(s);
    popup_show();
}
