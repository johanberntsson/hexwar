#!/usr/bin/env python3
"""Make the PRG and the region files from a raw, multiple-memory link.

    packprg.py LIST RAWSTEM PRG REGIONSTEM

The program is linked raw (sevenships.scm, src/region.h): ln6502
--output-format=raw --raw-multiple-memories --no-merge-raw-memories
writes one file a memory, or a part of one, and LIST is the linker's
listing, whose "Memories summary" says where each starts: the files are
RAWSTEM's directory plus NAME.raw, NAME being the memory or the part of
one (resHi-const). The exception is RAWSTEM.raw itself, which holds the
sections placed at fixed addresses in resLo -- the BASIC line and the
startup from $2001 -- and is not listed as a part of its own.

**The PRG** is every memory that is not a window region from 1 up,
placed at its address in bank 0 with zeros in between, from $2001 and
with the two-byte load address in front, as ln6502's own prg format
writes it. That is resLo, win0 (region 0) and resHi.

**Region N >= 1** (memory winN) goes to REGIONSTEM.rN: "RGN", N, a
16-bit stamp and the 16-bit length, then the bytes from $6000. The same
stamp is written into the PRG at region_stamp[N - 1], found in the
listing, so that src/region.c refuses a region file from another build.
"""

import os
import re
import sys

PRG_START = 0x2001
WINDOW = 0x6000


def memories(listing):
    """(name, start, end) for each memory and part, in listing order."""
    out = []
    inside = False
    for line in listing.splitlines():
        if 'Memories summary' in line:
            inside = True
            continue
        if inside and 'Sections summary' in line:
            break
        if not inside:
            continue
        m = re.match(r'\s*>?\s*([A-Za-z_][\w-]*)\s+([0-9a-f]{8})-([0-9a-f]{8})',
                     line)
        if m:
            out.append((m.group(1), int(m.group(2), 16), int(m.group(3), 16)))
    return out


def symbol(listing, name):
    m = re.search(r'^\s+' + re.escape(name) + r' = ([0-9a-f]{8})\s*$',
                  listing, re.M)
    if not m:
        sys.exit('packprg: %s is not in the listing' % name)
    return int(m.group(1), 16)


def stamp(data):
    """CRC-16/CCITT of the region's bytes; never 0xffff, which is what
    region_stamp[] holds before it is patched."""
    crc = 0xffff
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021 if crc & 0x8000 else crc << 1) & 0xffff
    return crc if crc != 0xffff else 0xfffe


def main():
    list_file, raw_stem, prg_file, region_stem = sys.argv[1:5]
    listing = open(list_file).read()
    raw_dir = os.path.dirname(raw_stem)

    bank0 = {PRG_START: open(raw_stem + '.raw', 'rb').read()}
    regions = {}
    mems = memories(listing)
    names = [m[0] for m in mems]
    for name, start, end in mems:
        # Only the program's own memories, and of one listed in parts
        # (resHi-const, resHi-text, ...) only the parts.
        if start < PRG_START or start >= 0xa000:
            continue
        if any(other.startswith(name + '-') for other in names):
            continue
        path = os.path.join(raw_dir, name + '.raw')
        if not os.path.exists(path):
            continue                # nothing but BSS, or nothing at all
        data = open(path, 'rb').read()
        if len(data) > end - start + 1:
            sys.exit('packprg: %s is larger than %s' % (path, name))
        m = re.match(r'win(\d+)$', name)
        if m and int(m.group(1)) >= 1:
            if start != WINDOW:
                sys.exit('packprg: %s does not start at $6000' % name)
            regions[int(m.group(1))] = data
        else:
            bank0[start] = data

    end = max(start + len(data) for start, data in bank0.items())
    if min(bank0) != PRG_START or end > 0xa000:
        sys.exit('packprg: the program is not within $2001-$9FFF')
    image = bytearray(end - PRG_START)
    for start, data in sorted(bank0.items()):
        at = start - PRG_START
        if any(image[at:at + len(data)]):
            sys.exit('packprg: two memories overlap at $%04X' % start)
        image[at:at + len(data)] = data

    stamps_at = symbol(listing, 'region_stamp') - PRG_START
    for n, data in sorted(regions.items()):
        s = stamp(data)
        at = stamps_at + 2 * (n - 1)
        image[at] = s & 0xff
        image[at + 1] = s >> 8
        header = b'RGN' + bytes([n, s & 0xff, s >> 8,
                                 len(data) & 0xff, len(data) >> 8])
        with open('%s.r%d' % (region_stem, n), 'wb') as f:
            f.write(header + data)
        print('region %d: %d bytes, stamp %04x' % (n, len(data), s))

    with open(prg_file, 'wb') as f:
        f.write(bytes([PRG_START & 0xff, PRG_START >> 8]) + image)
    print('%s: $%04X-$%04X, %d bytes' % (prg_file, PRG_START, end - 1,
                                        len(image)))


main()
