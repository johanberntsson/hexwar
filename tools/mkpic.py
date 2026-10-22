#!/usr/bin/python3
"""Turn a picture from screens/ into a file the game shows in its map window.

    mkpic.py [--png preview.png] in.png out.pic

**The picture fills the map window**, 60x23 characters, 480x184 pixels at
H640/V200, where a pixel is twice as tall as it is wide: so the source is
cropped round its middle to the window's shape on the glass, 480:368, and
scaled to 480x184. The window's tile store is the framebuffer
(src/picture.c), and nothing else is needed while a picture is up.

The colours are quantised, with dithering, to the 230 palette entries the
tiles use, 26-255: entries 0-15 are the system's, which the panel text
uses, and 16-25 the minimap's (tools/mktiles.py), which stays on screen.
Index 0 is never used, so nothing is transparent.

The layout, all of it read in pieces under 256 bytes (src/picture.c):
"PIC", the first palette entry, the number of entries, RGB triples, then
the window's 23 character rows of 3840 bytes, each 60 characters of 64
bytes (eight lines of eight pixels), as the tile store holds them.
"""

import argparse

from PIL import Image

COLUMNS, ROWS = 60, 23
WIDTH, HEIGHT = 8 * COLUMNS, 8 * ROWS
FIRST, COLOURS = 26, 230


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--png")
    ap.add_argument("src")
    ap.add_argument("out")
    args = ap.parse_args()

    im = Image.open(args.src).convert("RGB")
    w, h = im.size
    # The window on the glass is WIDTH wide by 2 * HEIGHT tall.
    if w * 2 * HEIGHT > h * WIDTH:
        cw = h * WIDTH // (2 * HEIGHT)
        im = im.crop(((w - cw) // 2, 0, (w - cw) // 2 + cw, h))
    else:
        ch = w * 2 * HEIGHT // WIDTH
        im = im.crop((0, (h - ch) // 2, w, (h - ch) // 2 + ch))
    im = im.resize((WIDTH, HEIGHT), Image.LANCZOS)
    q = im.quantize(colors=COLOURS, method=Image.Quantize.MEDIANCUT,
                    dither=Image.Dither.FLOYDSTEINBERG)
    flat = q.getpalette()[:3 * COLOURS]
    flat += [0] * (3 * COLOURS - len(flat))
    px = q.load()

    out = bytearray(b"PIC") + bytes([FIRST, COLOURS]) + bytes(flat)
    for row in range(ROWS):
        for col in range(COLUMNS):
            for y in range(8):
                for x in range(8):
                    out.append(FIRST + px[8 * col + x, 8 * row + y])
    with open(args.out, "wb") as f:
        f.write(out)

    if args.png:
        # As it looks on the glass: every line twice.
        q.convert("RGB").resize((WIDTH, 2 * HEIGHT), Image.NEAREST).save(args.png)
    print("%s: %d bytes" % (args.out, len(out)))


if __name__ == "__main__":
    main()
