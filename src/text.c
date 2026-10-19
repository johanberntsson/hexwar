#include <fcntl.h>
#include <calypsi/stubs.h>

#include "text.h"
#include "input.h"
#include "screen.h"

// The control bytes; tools/mktext.py has the markup they come from.
#define TC_AT 1                 // x, y: move there, and x is the margin
#define TC_INK 2                // colour and attribute bits
#define TC_TAB 3                // n: to the margin plus n
#define TC_REAL 4               // to TC_EMU: a real MEGA65 only
#define TC_EMU 5                // to TC_END: xemu only
#define TC_END 6
#define TC_ARG TEXT_ARG         // n: only popup_add_text() fills these in
#define TC_NEWLINE 10

#define HEADER_SIZE 7           // "TXT", format, count, body size

// **Bytes come through here in pieces under 256**, from the disk because
// _Stub_read miscounts a request that the end of the file meets on a page
// boundary (map_load() in map.c), and back out of attic RAM a piece at a
// time as text_show() reads them. In HIGH_BSS, not LOW_FREE, since the
// disk reads go through it.
#define PIECE 128
HIGH_BSS static byte piece[PIECE];

byte text_load(void)
{
    static byte header[HEADER_SIZE];
    dword to = TEXT_ATTIC;
    word left;
    byte n, ok = 0;
    int fd = _Stub_open(TEXT_FILE_NAME, O_RDONLY | O_BINARY);

    if (fd < 0) return 0;
    if (_Stub_read(fd, header, HEADER_SIZE) == HEADER_SIZE
        && header[0] == 'T' && header[1] == 'X' && header[2] == 'T'
        && header[3] == TEXT_FORMAT && header[4] == TEXT_COUNT) {
        left = header[6];
        left = (left << 8) | header[5];
        ok = 1;
        while (left && ok) {
            n = left > PIECE ? PIECE : (byte)left;
            ok = _Stub_read(fd, piece, n) == n;
            lcopy((word)piece, to, n);
            to += n;
            left -= n;
        }
    }
    _Stub_close(fd);
    return ok;
}

// Where the next byte comes from, and how far into `piece` it is; a full
// piece means it is used up.
static dword from;
static byte at;

static void seek(dword address)
{
    from = address;
    at = PIECE;
}

byte text_next(void)
{
    if (at == PIECE) {
        lcopy(from, (word)piece, PIECE);
        from += PIECE;
        at = 0;
    }
    return piece[at++];
}

// Past the next `code`, for the half of a {real|emu} not wanted here.
static void skip_past(byte code)
{
    while (text_next() != code)
        ;
}

void text_open(byte id)
{
    byte lo, hi;
    word offset;

    seek(TEXT_ATTIC + 2 * id);
    lo = text_next();
    hi = text_next();
    offset = ((word)hi << 8) | lo;
    seek(TEXT_ATTIC + offset);
}

HIGH_BSS static char line_buffer[2][TEXT_LINE_MAX + 1];
static byte line_which;

const char *text_line(byte id, byte n)
{
    char *out = line_buffer[line_which ^= 1];
    byte c = 1, i = 0;

    text_open(id);
    while (n && c) {
        c = text_next();
        if (c == '\n') --n;
    }
    if (c)
        while ((c = text_next()) != 0 && c != '\n' && i < TEXT_LINE_MAX)
            out[i++] = c;
    out[i] = 0;
    return out;
}

void text_show(byte id)
{
    byte c, x = 0, y = 0, margin = 0, ink = COLOR_WHITE;

    text_open(id);

    while ((c = text_next()) != 0) {
        switch (c) {
        case TC_AT:
            x = margin = text_next();
            y = text_next();
            break;
        case TC_INK:
            ink = text_next();
            break;
        case TC_ARG:            // not for a screen: skip its number
            text_next();
            break;
        case TC_TAB:
            x = margin + text_next();
            break;
        case TC_NEWLINE:
            x = margin;
            ++y;
            break;
        case TC_REAL:
            if (!input_real_hw) skip_past(TC_EMU);
            break;
        case TC_EMU:
            skip_past(TC_END);  // only reached after the real half
            break;
        case TC_END:
            break;
        default:
            screen_putcxy(x++, y, c, ink);
            break;
        }
    }
}
