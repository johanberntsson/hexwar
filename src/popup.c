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
#include "view.h"

// The text, wrapped to at most TEXT_WIDTH columns and MAX_LINES lines; a
// line past that is not drawn. The box is the text with a space and a
// border round it, a blank line, and the OK line.
#define TEXT_WIDTH 40
#define MAX_LINES (VIEW_ROWS - 6)
#define TEXT_SIZE 400

#define TEXT_COLOUR COLOR_WHITE
#define BORDER_COLOUR COLOR_YELLOW

// Box-drawing screen codes, the same in the upper and the lower case sets.
#define BOX_TOP_LEFT 112
#define BOX_TOP_RIGHT 110
#define BOX_BOTTOM_LEFT 109
#define BOX_BOTTOM_RIGHT 125
#define BOX_ACROSS 64
#define BOX_DOWN 93

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

void popup_show(void)
{
    byte lines, widest, w, h, x0, y0, x, y, key;

    lines = wrap(0, 0, 0, &widest);
    if (widest < 6) widest = 6;
    w = widest + 4;
    h = lines + 5;
    x0 = VIEW_LEFT + (VIEW_COLUMNS - w) / 2;
    y0 = VIEW_TOP + (VIEW_ROWS - h) / 2;

    for (y = y0 + 1; y < y0 + h - 1; ++y) {
        screen_putcodexy(x0, y, BOX_DOWN, BORDER_COLOUR);
        for (x = x0 + 1; x < x0 + w - 1; ++x)
            screen_putcxy(x, y, ' ', TEXT_COLOUR);
        screen_putcodexy(x0 + w - 1, y, BOX_DOWN, BORDER_COLOUR);
    }
    for (x = x0 + 1; x < x0 + w - 1; ++x) {
        screen_putcodexy(x, y0, BOX_ACROSS, BORDER_COLOUR);
        screen_putcodexy(x, y0 + h - 1, BOX_ACROSS, BORDER_COLOUR);
    }
    screen_putcodexy(x0, y0, BOX_TOP_LEFT, BORDER_COLOUR);
    screen_putcodexy(x0 + w - 1, y0, BOX_TOP_RIGHT, BORDER_COLOUR);
    screen_putcodexy(x0, y0 + h - 1, BOX_BOTTOM_LEFT, BORDER_COLOUR);
    screen_putcodexy(x0 + w - 1, y0 + h - 1, BOX_BOTTOM_RIGHT, BORDER_COLOUR);

    wrap(1, x0 + 2, y0 + 1, &widest);
    screen_putsxy(x0 + (w - 6) / 2, y0 + h - 2, "[ OK ]", COLOR_YELLOW);

    // Anywhere is OK: a click need not be on the button.
    for (;;) {
        screen_wait_raster(SCREEN_ROW_LINE(SCREEN_ROWS - 1));
        key = input_poll();
        if (key == KEY_CLICK || key == KEY_RETURN || key == KEY_SPACE
            || key == KEY_ESC || key == KEY_RUNSTOP)
            break;
    }

    view_restore(x0 - VIEW_LEFT, y0 - VIEW_TOP, w, h);
}

void popup(const char *s)
{
    popup_begin();
    popup_add(s);
    popup_show();
}
