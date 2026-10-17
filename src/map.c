#include <fcntl.h>
#include <string.h>
#include <calypsi/stubs.h>

#include "map.h"

byte map_tile[MAP_ROWS][MAP_COLUMNS];

// The file's header; tools/mapgen.py documents the whole layout.
#define MAP_HEADER_SIZE 16

// map_load() reads a row per call and relies on each being under 256 bytes;
// see the note there. Column numbers are bytes everywhere else anyway.
#if MAP_COLUMNS > 255
#error "map_load reads a row per _Stub_read call, which must be under 256 bytes"
#endif

void map_cell_at(word cx, word cy, byte *ax, byte *ay)
{
    // Work in half characters, so that the point is the middle of the
    // character and a tile's centre, (4*vx + 4, 3*hy + 2), is whole too.
    int px = 2 * (int)cx + 1;
    int py = 2 * (int)cy + 1;
    int q = (int)(cx / 4);
    int r = (int)(cy / 3);
    word best = 0xffff;
    int vx, hy;

    // The tiles whose rectangles hold the character start at most one column
    // left and one hexagon row up; one row down is checked as well, since its
    // centre can be nearer than a rectangle that merely contains the point.
    for (vx = q - 1; vx <= q; ++vx) {
        for (hy = r - 1; hy <= r + 1; ++hy) {
            int dx, dy;
            word d;
            if (hy < 0 || hy >= 2 * MAP_ROWS || ((vx ^ hy) & 1)) continue;
            dx = px - (8 * vx + 8);
            dy = py - (6 * hy + 4);
            // A character is 4 units wide and 8 tall on the glass, so a
            // half-character is 2 by 4, and distance squared is
            // (2dx)^2 + (4dy)^2, which is proportional to this.
            d = (word)(dx * dx + 4 * dy * dy);
            if (d < best) {
                best = d;
                // vx is -1 just left of the seam; the wrap brings it round
                // to MAP_COLUMNS - 1.
                *ax = (byte)(vx < 0 ? vx + MAP_COLUMNS : vx);
                *ay = (byte)((hy - (vx & 1)) >> 1);
            }
        }
    }
}

// Straight to the Calypsi stubs rather than through stdio: a FILE layer would
// buffer what is already being read into its final place, and costs code.
// _Stub_read is the KERNAL's CHRIN, one byte a call, so 4096 bytes is a
// moment rather than an instant -- which is fine once, at start-up.
byte map_load(void)
{
    static byte header[MAP_HEADER_SIZE];
    byte ok = 0;
    int fd = _Stub_open(MAP_FILE_NAME, O_RDONLY | O_BINARY);

    if (fd < 0) return 0;

    // "HXM" is compared as written: Calypsi does not turn literals into
    // PETSCII, and mapgen.py writes these bytes as ASCII.
    if (_Stub_read(fd, header, MAP_HEADER_SIZE) == MAP_HEADER_SIZE
        && memcmp(header, "HXM", 3) == 0
        && header[3] == MAP_VERSION
        && header[4] == MAP_COLUMNS
        && header[5] == MAP_ROWS
        && header[6] >= 1) {
        // Layer 0, terrain, is read straight into place, a row at a time.
        // Any later layers are not wanted yet and are left unread.
        //
        // **A row at a time because _Stub_read miscounts otherwise.** When
        // the file ends on the last byte of a 256-byte page of the request,
        // it sees end-of-file and returns before carrying into the count's
        // high byte: one 4096-byte read of the whole layer, which ends the
        // file, reports 3840. A request under 256 bytes takes the other path
        // in stub_read.s and counts correctly. 80x50 never showed this,
        // because 4000 bytes end mid-page.
        byte ay;
        ok = 1;
        for (ay = 0; ay < MAP_ROWS && ok; ++ay)
            ok = _Stub_read(fd, map_tile[ay], MAP_COLUMNS) == MAP_COLUMNS;
    }
    _Stub_close(fd);
    return ok;
}
