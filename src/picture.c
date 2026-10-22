// In region 5 (src/region.h): called through the stubs in src/dispatch.s,
// under these names here.
#define picture_show picture_show_r5
#define picture_end picture_end_r5

#include <fcntl.h>
#include <calypsi/stubs.h>

#include "picture.h"
#include "screen.h"
#include "view.h"

#pragma clang section text="r5text" rodata="r5rodata"

// The files, by PICTURE_*. The disk has them under their host names in
// capitals (the Makefile, tools/diskutil.rb). **The name is copied out of
// the window before the open**: the KERNAL's disk code maps memory its
// own way while it reads the name, and sees region 0 at $6000, not this
// region -- which is why the loaders in region 0 can pass a literal.
static const char *const file_name[] = {
    "INTRO.PIC", "DEFEAT.PIC", "WIN_SURVIVAL.PIC", "WIN_HEGEMONY.PIC"
};
HIGH_BSS static char name[17];

// The header: "PIC", the first palette entry, how many.
#define HEADER 5

// **Every read under 256 bytes**, for _Stub_read (map_load() in
// mapload.c); 3840 is thirty of these. In HIGH_BSS, since it is in use
// between disk reads.
#define PIECE 128
HIGH_BSS static byte piece[PIECE];

static byte read_picture(int fd)
{
    byte rgb[3];
    byte i, n, r, k;
    dword to;

    if (_Stub_read(fd, piece, HEADER) != HEADER) return 0;
    if (piece[0] != 'P' || piece[1] != 'I' || piece[2] != 'C') return 0;
    i = piece[3];
    for (n = piece[4]; n; --n, ++i) {
        if (_Stub_read(fd, rgb, 3) != 3) return 0;
        screen_set_palette_entry(i, rgb[0], rgb[1], rgb[2]);
    }
    for (r = 0; r < VIEW_ROWS; ++r) {
        to = TILE_STORE + VIEW_ROW_STRIDE * r;
        for (k = 0; k < 64 * VIEW_COLUMNS / PIECE; ++k) {
            if (_Stub_read(fd, piece, PIECE) != PIECE) return 0;
            lcopy((word)piece, to, PIECE);
            to += PIECE;
        }
    }
    return 1;
}

byte picture_show(byte id)
{
    byte ok, i = 0;
    const char *s = file_name[id];
    int fd;

    view_clear();
    while ((name[i++] = *s++)) continue;
    fd = _Stub_open(name, O_RDONLY | O_BINARY);
    if (fd < 0) return 0;
    ok = read_picture(fd);
    _Stub_close(fd);
    if (!ok) {
        picture_end();
        view_clear();
    }
    return ok;
}

void picture_end(void)
{
    byte i = 16;
    do {
        screen_set_palette_entry(i, lpeek(PALETTE_ATTIC + i),
                                 lpeek(PALETTE_ATTIC + 256 + i),
                                 lpeek(PALETTE_ATTIC + 512 + i));
    } while (++i);
}
