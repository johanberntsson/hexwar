#!/usr/bin/env python3
"""Pack the game's texts into one file for drive 8, and number them.

    mktext.py OUT.PAK OUT_IDS.H TEXT.txt ...

Each source is plain text, one screen of it, with a little markup in
braces. A text's number is its place on the command line, and its name in
the header is TEXT_ and the file's stem in capitals: res/text/help.txt is
TEXT_HELP. The program reads the whole pack into attic RAM at start-up
(src/text.c) and draws a text from there.

Markup:

    {at X Y}      move to column X, row Y; X is the margin a new line
                  goes back to
    {centre Y}    the same, with X putting the rest of the line in the
                  middle of the screen
    {tab N}       move to the margin plus N
    {COLOUR}      ink: white red cyan green blue yellow lightgreen grey,
                  or key (yellow), text (lightgreen), head (white);
                  {rev COLOUR} is the same in reverse
    {A|B}         A on a real MEGA65, B in xemu: {HELP|F1}
    {arg N}       the Nth string the caller passes; only popup_add_text()
                  fills these in, so they are for popups, which take plain
                  text, line breaks and arguments and nothing else
    a line break  the next row, at the margin

A line ending in a backslash runs on into the next without a break, and
a line starting with # is a comment. Every other character has to be
printable ASCII.

The file: "TXT", FORMAT_VERSION, the number of texts, and the size of
the body (16 bits, little-endian) -- seven bytes -- and then the body,
which is what goes into attic RAM: a table of count + 1 16-bit offsets
from the start of the body, then the texts, each ending in a 0 byte.
The control bytes are the TC_* constants below and in src/text.c.
"""

import os
import re
import sys

FORMAT_VERSION = 1

TC_AT = 1        # x, y
TC_INK = 2       # colour, with the attribute bits
TC_TAB = 3       # column after the margin
TC_REAL = 4      # what follows, to TC_EMU, is for a real MEGA65 only
TC_EMU = 5       # what follows, to TC_END, is for xemu only
TC_END = 6
TC_ARG = 7       # n
TC_NEWLINE = 10

COLOURS = {
    'white': 1, 'red': 2, 'cyan': 3, 'green': 5, 'blue': 6, 'yellow': 7,
    'lightgreen': 13, 'grey': 15,
    'key': 7, 'text': 13, 'head': 1,
}
REVERSE = 0x20

COLUMNS, ROWS = 80, 25


def fail(path, line, message):
    sys.exit(f"{path}:{line}: {message}")


def visible_length(rest):
    """How many columns the rest of a line takes, markup left out; of the
    two halves of an {A|B}, the longer."""
    rest = rest.split('\n', 1)[0]
    n = 0
    for part in re.split(r'(\{[^}]*\})', rest):
        if part.startswith('{'):
            if '|' in part:
                n += max(len(h) for h in part[1:-1].split('|', 1))
        else:
            n += len(part)
    return n


def compile_text(path):
    out = bytearray()
    with open(path, encoding='ascii') as f:
        source = f.read()
    # Comments, then continuation lines.
    lines = [l for l in source.split('\n') if not l.startswith('#')]
    text = '\n'.join(lines).replace('\\\n', '')
    text = text.rstrip('\n')
    line = 1
    pos = 0
    while pos < len(text):
        c = text[pos]
        if c == '\n':
            out.append(TC_NEWLINE)
            line += 1
            pos += 1
            continue
        if c == '{':
            end = text.find('}', pos)
            if end < 0:
                fail(path, line, "a { with no }")
            body = text[pos + 1:end]
            pos = end + 1
            words = body.split()
            if '|' in body:
                real, emu = body.split('|', 1)
                out.append(TC_REAL)
                out += real.encode('ascii')
                out.append(TC_EMU)
                out += emu.encode('ascii')
                out.append(TC_END)
            elif words and words[0] == 'at' and len(words) == 3:
                x, y = int(words[1]), int(words[2])
                if not (0 <= x < COLUMNS and 0 <= y < ROWS):
                    fail(path, line, f"{{at {x} {y}}} is off the screen")
                out += bytes([TC_AT, x, y])
            elif words and words[0] == 'centre' and len(words) == 2:
                y = int(words[1])
                x = (COLUMNS - visible_length(text[pos:])) // 2
                if not (0 <= y < ROWS) or x < 0:
                    fail(path, line, f"{{centre {y}}} does not fit")
                out += bytes([TC_AT, x, y])
            elif words and words[0] == 'arg' and len(words) == 2:
                out += bytes([TC_ARG, int(words[1])])
            elif words and words[0] == 'tab' and len(words) == 2:
                out += bytes([TC_TAB, int(words[1])])
            elif words and words[0] == 'rev' and len(words) == 2 \
                    and words[1] in COLOURS:
                out += bytes([TC_INK, COLOURS[words[1]] | REVERSE])
            elif len(words) == 1 and words[0] in COLOURS:
                out += bytes([TC_INK, COLOURS[words[0]]])
            else:
                fail(path, line, f"unknown markup {{{body}}}")
            continue
        if not (32 <= ord(c) < 127):
            fail(path, line, f"{c!r} is not printable ASCII")
        out.append(ord(c))
        pos += 1
    out.append(0)
    return bytes(out)


def main():
    if len(sys.argv) < 4:
        sys.exit(__doc__)
    pak, ids, sources = sys.argv[1], sys.argv[2], sys.argv[3:]
    texts = [compile_text(s) for s in sources]

    table = 2 * (len(texts) + 1)
    offsets, at = [], table
    for t in texts:
        offsets.append(at)
        at += len(t)
    offsets.append(at)
    body = b''.join(o.to_bytes(2, 'little') for o in offsets) + b''.join(texts)
    if len(body) > 0xffff:
        sys.exit("the texts are over 64 KB, which the header cannot say")

    with open(pak, 'wb') as f:
        f.write(b'TXT' + bytes([FORMAT_VERSION, len(texts)])
                + len(body).to_bytes(2, 'little') + body)

    names = [re.sub(r'\W', '_', os.path.splitext(os.path.basename(s))[0])
             .upper() for s in sources]
    header = ("// Made by tools/mktext.py from the texts the Makefile lists; "
              "do not edit.\n"
              "#ifndef SEVENSHIPS_TEXTID_H\n#define SEVENSHIPS_TEXTID_H\n\n"
              f"#define TEXT_FORMAT {FORMAT_VERSION}\n"
              f"#define TEXT_COUNT {len(texts)}\n\n"
              + ''.join(f"#define TEXT_{n} {i}\n" for i, n in enumerate(names))
              + "\n#endif\n")
    # Only when the numbering changes, since every object depends on it:
    # editing what a text says remakes the pack and nothing else.
    try:
        with open(ids) as f:
            same = f.read() == header
    except OSError:
        same = False
    if not same:
        with open(ids, 'w') as f:
            f.write(header)


if __name__ == '__main__':
    main()
