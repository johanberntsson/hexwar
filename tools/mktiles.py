#!/usr/bin/python3
"""Build res/tiles.fci from the individual tile PNGs in tiles/.

    mktiles.py [--png preview.png] out.fci tile0.png tile1.png ...

**The order of the PNGs on the command line is the tile numbering** -- TILE_*
in src/sheet.h -- and tiles/Makefile is where that order is written down.
A tile given as `a.png+b.png` is a drawn first and b over it, which is how an
overlay such as the forest's trees becomes a terrain tile of its own.

This replaced an ImageMagick `convert +append` followed by tools/png2fci.py,
which needed a single paletted PNG whose palette index 0 happened to be the
transparent colour. The tile art is RGBA with far more than 256 colours
between them (1443 for the first ten), so something has to quantise, and
ImageMagick gives no say over which index transparency lands on. Here:

* a pixel with alpha below 128 is transparent and becomes **palette index 0**,
  which is what the DMAgic's transparent copy skips (src/mega65.h);
* entries 16 up hold TERRAIN_COLOURS, one flat colour per terrain, which is
  what the minimap draws a hexagon in (PAL_TERRAIN in src/minimap.c). The
  system palette has too few colours left for ten terrains beside the five
  players';
* every other pixel is quantised, across all the tiles together, to what is
  left -- 230 colours -- which are written after them. Entries 0-15 are
  the system colours the panel text uses, so the FCI carries the "reserved"
  flag and src/sheet.c leaves them alone. If the art has few enough colours
  it is used exactly.

The FCI layout is png2fci's: "fciP", version 1, rows, columns (in 8x8
characters), options (bit 1: system palette reserved), the index of the last
palette entry, RGB triples, "IMG", then 64 bytes a character, row-major.
"""

import argparse
import sys

from PIL import Image

TILE = 32               # pixels; a tile is 4x4 characters in the file
RESERVED = 16

# The minimap's colour for each terrain, in terrain (TILE_*) order: entry
# 16 + terrain. Keep in step with TILE_* in src/sheet.h.
TERRAIN_COLOURS = [
    (120, 96, 48),      # hills
    (48, 140, 40),      # grassland
    (136, 136, 144),    # mountains
    (32, 56, 160),      # ocean
    (72, 120, 216),     # shallows
    (208, 216, 224),    # tundra
    (150, 140, 56),     # plains
    (16, 80, 24),       # forest
    (240, 228, 140),    # desert
    (150, 210, 80),     # floodplain
]
FIRST = RESERVED + len(TERRAIN_COLOURS)
MAX_COLOURS = 256 - FIRST


def load(spec):
    im = None
    for path in spec.split("+"):
        layer = Image.open(path).convert("RGBA")
        if layer.size != (TILE, TILE):
            sys.exit("%s: tiles are %dx%d, this is %dx%d"
                     % (path, TILE, TILE, layer.size[0], layer.size[1]))
        if im is None:
            im = layer
        else:
            im.alpha_composite(layer)
    return im


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--png", help="also write the sheet as a PNG preview")
    ap.add_argument("out")
    ap.add_argument("tiles", nargs="+")
    args = ap.parse_args()

    tiles = [load(p) for p in args.tiles]
    width = TILE * len(tiles)
    sheet = Image.new("RGBA", (width, TILE))
    for i, im in enumerate(tiles):
        sheet.paste(im, (TILE * i, 0))

    pixels = list(sheet.get_flattened_data()
                  if hasattr(sheet, "get_flattened_data") else sheet.getdata())
    opaque = [p[:3] for p in pixels if p[3] >= 128]
    distinct = sorted(set(opaque))

    if len(distinct) <= MAX_COLOURS:
        palette = distinct
        lookup = {c: i for i, c in enumerate(palette)}
        mapped = [lookup[c] for c in opaque]
    else:
        # One row of every opaque pixel, quantised as a whole, so that the
        # transparent corners do not take up palette entries.
        strip = Image.new("RGB", (len(opaque), 1))
        strip.putdata(opaque)
        q = strip.quantize(colors=MAX_COLOURS, method=Image.Quantize.MEDIANCUT,
                           dither=Image.Dither.NONE)
        flat = q.getpalette()[:3 * MAX_COLOURS]
        palette = [tuple(flat[3 * i:3 * i + 3]) for i in range(len(flat) // 3)]
        mapped = list(q.get_flattened_data()
                      if hasattr(q, "get_flattened_data") else q.getdata())
        used = max(mapped) + 1
        palette = palette[:used]

    # Back into a width x TILE grid of FCI palette indices.
    index = []
    it = iter(mapped)
    for p in pixels:
        index.append(FIRST + next(it) if p[3] >= 128 else 0)

    cols = width // 8
    rows = TILE // 8
    image = bytearray()
    for cr in range(rows):
        for cc in range(cols):
            for y in range(8):
                start = (8 * cr + y) * width + 8 * cc
                image.extend(index[start:start + 8])

    out = bytearray(b"fciP")
    out += bytes([1, rows, cols, 0x02, FIRST + len(palette) - 1])
    out += bytes(3 * RESERVED)
    for c in TERRAIN_COLOURS:
        out += bytes(c)
    for c in palette:
        out += bytes(c)
    out += b"IMG"
    out += image
    with open(args.out, "wb") as f:
        f.write(out)

    if args.png:
        preview = Image.new("RGBA", (width, TILE))
        full = [(0, 0, 0)] * RESERVED + TERRAIN_COLOURS + palette
        preview.putdata([(0, 0, 0, 0) if i == 0 else full[i] + (255,)
                         for i in index])
        preview.save(args.png)

    print("%s: %d tiles, %d colours (%d distinct in the art), %d bytes"
          % (args.out, len(tiles), len(palette), len(distinct), len(out)))


if __name__ == "__main__":
    main()
