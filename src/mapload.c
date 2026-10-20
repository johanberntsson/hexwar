// The map file's loader, map_load() (src/map.h). It runs once, at boot,
// so it is in region 0 (src/region.h) with the tile sheet's loader rather
// than in the resident program: called through the stub in src/dispatch.s,
// under this name here.
#define map_load map_load_r0

#include <fcntl.h>
#include <string.h>
#include <calypsi/stubs.h>

#include "map.h"

#pragma clang section text="r0text" rodata="r0rodata"

// The file's header; tools/mapgen.py documents the whole layout.
#define MAP_HEADER_SIZE 16

// map_load() reads a row per call and relies on each being under 256 bytes;
// see the note there. Column numbers are bytes everywhere else anyway.
#if MAP_COLUMNS > 255
#error "map_load reads a row per _Stub_read call, which must be under 256 bytes"
#endif

// Straight to the Calypsi stubs rather than through stdio: a FILE layer would
// buffer what is already being read into its final place, and costs code.
// _Stub_read is the KERNAL's CHRIN, one byte a call, so 4096 bytes is a
// moment rather than an instant -- which is fine once, at start-up.
byte map_load(void)
{
    static byte header[MAP_HEADER_SIZE];
    static byte entry[3];       // a resource: hexagon row, column, kind
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
        && header[6] >= 2) {
        // Layer 0, terrain, is read straight into place, a row at a time;
        // layer 1, the resources, an entry at a time, which gives their
        // rows' starts (map_res_first) on the way. Any later layers are
        // left unread.
        //
        // **A row at a time because _Stub_read miscounts otherwise.** When
        // the file ends on the last byte of a 256-byte page of the request,
        // it sees end-of-file and returns before carrying into the count's
        // high byte: one 4096-byte read of the whole layer, which ends the
        // file, reports 3840. A request under 256 bytes takes the other path
        // in stub_read.s and counts correctly. 80x50 never showed this,
        // because 4000 bytes end mid-page.
        byte ay, hy, i;
        ok = 1;
        for (ay = 0; ay < MAP_ROWS && ok; ++ay)
            ok = _Stub_read(fd, map_tile[ay], MAP_COLUMNS) == MAP_COLUMNS;
        ok = ok && _Stub_read(fd, &map_res_count, 1) == 1;
        map_res_first[0] = 0;
        hy = 0;
        for (i = 0; i < map_res_count && ok; ++i) {
            ok = _Stub_read(fd, entry, 3) == 3 && entry[0] < 2 * MAP_ROWS
                 && entry[0] >= hy && entry[1] < MAP_COLUMNS
                 && entry[2] < RES_COUNT;
            if (!ok) break;
            while (hy < entry[0]) map_res_first[++hy] = i;
            map_res_ax[i] = entry[1];
            map_res_kind[i] = entry[2];
        }
        while (hy < 2 * MAP_ROWS) map_res_first[++hy] = map_res_count;
    }
    _Stub_close(fd);
    return ok;
}
