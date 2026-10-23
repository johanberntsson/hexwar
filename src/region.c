#include <fcntl.h>
#include <calypsi/stubs.h>

#include "region.h"

// A region file is a header and the region's bytes, from $6000:
// "RGN", the region's number, a 16-bit stamp and the 16-bit length.
#define HEADER_SIZE 8

// The stamp each region file must carry, one a region from 1. **Written
// into the PRG by tools/packprg.py**, which finds it by name in the
// listing, computes it from the region's bytes and puts the same stamp in
// the file's header -- so a disk with region files from another build is
// refused rather than run. Initialised, so that it is in the PRG at all.
word region_stamp[REGIONS] = {
    0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff
};

// Each region's length, for set_region's copy into its slot; by region
// from 0, in src/dispatch.s.
extern byte region_length_lo[], region_length_hi[];

// In pieces under 256 for _Stub_read (map_load() in map.c), and in
// HIGH_BSS since the disk reads go through it, as text_load() does.
#define PIECE 128
HIGH_BSS static byte piece[PIECE];

static byte load_one(byte r)
{
    static char name[] = REGION_FILE_NAME;
    byte header[HEADER_SIZE];
    dword to = REGION_ATTIC(r);
    word left, stamp;
    byte n, ok = 0;
    int fd;

    name[sizeof name - 2] = (char)('0' + r);
    fd = _Stub_open(name, O_RDONLY | O_BINARY);
    if (fd < 0) return 0;
    if (_Stub_read(fd, header, HEADER_SIZE) == HEADER_SIZE
        && header[0] == 'R' && header[1] == 'G' && header[2] == 'N'
        && header[3] == r) {
        stamp = header[5];
        stamp = (stamp << 8) | header[4];
        left = header[7];
        left = (left << 8) | header[6];
        ok = stamp == region_stamp[r - 1] && left <= REGION_SIZE;
        region_length_lo[r] = header[6];
        region_length_hi[r] = header[7];
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

byte region_load(void)
{
    byte r;

    for (r = 1; r <= REGIONS; ++r)
        if (!load_one(r)) return 0;
    // Nothing is in the slots yet: the first call copies each region in.
    region_init();
    return 1;
}
