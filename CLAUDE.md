# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A Civilization-style hex game for the **MEGA65**, written in C for the
**Calypsi 6502** tool chain. It was a cc65 project until August 2026; that
version is still on disk under `old-cc65/` and will be deleted once this one
has everything it had.

`basic/civ.bas` is a more complete XC=BASIC 3 version of the same idea and
`basic/civ-basic.png` is the layout it reached — **that screenshot is the
specification for the display**, and the geometry in `src/view.h` comes
straight from its `fc_setMergeTileMode(10, 1, 60, 23, true)`. Neither the
BASIC nor the cc65 version ever scrolled correctly; see **Scrolling** below.
`design.md` holds the hex-grid and Civ-1 map-generator research.

## Build and run

```sh
make                      # build/hexwar.prg -- the whole game, one file
make run                  # build it and boot it in xemu (blocks until closed)
make d81                  # build/hexwar.d81, which is how it gets to real hardware
make START_OFF_X=41       # start the view one character further right; see below
make clean
```

Needs `cc6502`/`as6502`/`ln6502` (Calypsi 5.18+), Python 3, and
`xemu-xmega65`. `c1541` is needed only by `make d81`; `pypng` and ImageMagick
only if the tile art changes, since `res/tiles.fci` is checked in.

There are no tests. Verification is running it — see **Verifying a change**.

**`ln6502 -o` names the ELF, not the PRG.** The PRG is written beside it under
the same stem. Linking straight to a `.prg` name silently produces an ELF file
that the MEGA65 then tries to RUN as BASIC.

**`build/hexwar.lst` is the memory map and the only place the program's real
size is written down.** Worth reading after anything that might have grown it.
A change that will not link is more likely to be a few hundred bytes over than
wrong.

### The board support library is already installed

`hth313/Calypsi-6502-Commodore` — which the rewrite was meant to be based on —
ships *inside* the tool chain, byte for byte: its `src/` is
`/usr/local/lib/calypsi-6502-5.18/contrib/MEGA65-SDK/src` and its
`linker-files/` is `.../linker-rules` (`diff -rq` against a fresh clone is
clean). `--target=mega65` already links it. **There is nothing to clone or
vendor**, and no `mega65-libc` in this repo — the six things this game needs
from that library are reimplemented in `src/mega65.c` and `src/screen.c`.

### Verifying a change

xemu can be driven without a window, which is the fastest way to check that a
change still boots and draws:

```sh
timeout -s TERM 20 xemu-xmega65 -skipconfigfile -headless -sleepless -besure \
    -fastboot -screenshot $PWD/shot.png -dumpmem $PWD/mem.bin -prg build/hexwar.prg
```

The screenshot is written when xemu exits, including on SIGTERM, so the
`timeout` is the run length. `-skipconfigfile` **must be the first argument**;
without it, concurrent runs race each other rewriting xemu's config template
and pop up dialogs at whoever is at the machine.

**`-dumpmem` gives 384 KB of chip RAM (`$00000-$5FFFF`)**, which covers screen
RAM at `$12000` and the whole tile store at `$40000`. It does *not* cover the
expanded tile sheet in attic RAM.

**Nothing headless can press a key**, so `START_OFF_X` / `START_OFF_Y` (Makefile
`-D`s, defaults in `src/hexwar.h`) exist to start the view somewhere else. Two
builds one apart are how scrolling is checked: `make` then `make START_OFF_X=41`
must shift the map exactly eight pixels and leave the panel byte-identical.
Both were verified pixel-exact this way, along with one row vertically (16
screenshot pixels, since V200 is displayed doubled).

**The config stamp is what makes that work at all.** `START_OFF_X` reaches the
compiler as a `-D`, which make cannot see; without `$(STAMP)` in the Makefile a
second build finds every object up to date and hands back the *previous*
binary, which looks exactly like a change that did not take. It cost a
verification run before it went in.

## The display

**There is no bitmap mode.** The screen is VIC-IV **16-bit text mode with
full-colour characters**: each cell is two bytes holding a character *number*.
A number below 256 draws the usual 8x8 monochrome glyph from the character ROM
— that is the panel text. A number of 256 or more draws 64 bytes of palette
indices, one per pixel, found at `number * 64` as an absolute 28-bit address.

**Every cell of the map window owns 64 bytes of its own, and that is the whole
trick.** `view_init()` writes a *unique* character number into each of the
60x23 cells, once, so their definitions form one linear framebuffer at
`TILE_STORE` (`$40000`). Drawing is then nothing but copying 64-byte blocks
into it, and **the screen words never change again** — which is what makes
scrolling a redraw rather than a rearrangement.

### The ROM is never banked out, and that is unusual here

`~/commodore/mega65/hexgame` and `~/commodore/SearchAndRescue` both flatten the
memory map to reach `$20000-$3FFFF`, and both pay heavily: a Hypervisor call to
lift the ROM's write protection, **a second one to put it back** (it is a
toggle that outlives the program, and forgetting it corrupts the *next* run), a
copy of the whole 128 KB ROM to attic RAM so the machine can still be reset, and
a rule that all disk I/O must finish first.

Hexwar needs none of it because of one number: the view is 60x23, so its tile
store is `60*23*64 = 88320` bytes, and that fits in banks 4 and 5
(`$40000-$559FF`) with 40 KB to spare. **So the KERNAL stays live, `printf`
works, the interrupt vectors are still the ROM's, and quitting is a return from
`main()`.** The `#error` in `src/view.h` guards the assumption. Enlarging the
view past `$60000` would drag all of that machinery in — check the arithmetic
before changing `VIEW_COLUMNS` or `VIEW_ROWS`.

Screen RAM is at `$12000`, not `$E000` where the cc65 build had it. That
retires the `ENABLE_SCREEN_ACCESS` / `$01` dance that used to bracket every
screen access in `old-cc65/src/graphics.c`.

Colour RAM is moved to `$FF81000` via `$D064`/`$D065`, because CBDOS keeps
buffers at the default `$FF80000` and corrupts the display.

### Two register habits worth keeping

- **The hot registers ($D05D bit 7) have to be on for the first half of
  `screen_init()` and off for the second.** With them on, a write to a legacy
  VIC-II/VIC-III register makes the VIC-IV recompute its layout — which is what
  turns the H640 bit into a sensible 80-column geometry. Once that is done they
  must go off, or the hand-written `CHRCOUNT`, `CHARSTEP` and `SCNPTR` are
  undone by the next legacy write.
- **`POKE(53272, PEEK(53272) | 0x02)` picks the lower case character set**, and
  on a normal boot the KERNAL does it for you. Take it away and every capital
  letter comes out as a graphic.

## Memory map

| Address | Size | Contents |
|---|---|---|
| `$2001-$9FFF` | 32 KB | program, data, BSS, C stack. About 19 KB used |
| `$1600-$1EFF` | 2304 | `zpsave`, free — `LOW_FREE` in `src/mega65.h` moves a variable there |
| `$12000` | 4000 | screen RAM, 80x25 cells of 2 bytes |
| `$40000-$559FF` | 88320 | the view's tile store, 60x23 cells of 64 bytes |
| `$8000000` | 12288 | the x-expanded tile sheet, 48x4 characters |
| `$FF81000` | 4000 | colour RAM |

`$20000-$3FFFF` is left as the C65 ROM.

**The 32 KB is what to watch.** The linked tile sheet is 6573 bytes and
`map_tile` is 4000, so 10.5 KB is spoken for before any game code. If it gets
tight the known fix is hexgame's exomizer one-file scheme
(`~/commodore/mega65/hexgame/tools/mkprg.py`); a Civ-style ten-layer map
(10 x 4000) will want attic RAM and Calypsi `__far` pointers regardless.

**Naming `zdata` in `hexwar.scm`'s `freeSpace` would move the whole BSS to
`$1600-$1EFF`** — which is what hexgame does — but `map_tile` alone does not
fit in 2304 bytes. Use `LOW_FREE` per variable instead.

## The hex grid

**Doubled coordinates** (redblobgames): a hexagon is `(hx, hy)` with `hx` and
`hy` always the same parity, so a step east is `hx += 2` and the four diagonal
neighbours are `(hx±1, hy±1)`. Array cell `(ax, ay)` is hexagon
`(ax, 2*ay + (ax & 1))`, which packs 80x50 = 4000 hexagons into a rectangle.

Two constants in `src/map.h` carry the whole view geometry:

```c
#define MAP_CELL_X(AX)     (4 * (word)(AX))
#define MAP_CELL_Y(AX, AY) (6 * (word)(AY) + 3 * ((AX) & 1))
```

A tile is **8x4 characters** once the sheet is expanded, and a hexagon column
step is `hx += 2`, so `4*hx` puts the odd, offset rows exactly half a tile
across **without a special case**. Vertically a hexagon row is 3 characters, so
tiles overlap by one row — and that overlap is what the transparent copy is for.

The cc65 build spelled these `x_hex2screen` = `x*4 + 4*(y%2)` and
`y_hex2screen` = `6*(y/2) + 3*(y%2)` in `old-cc65/src/map.c`. The second is
just `3*hy`; **the first was missing the doubling that x-expansion needs**,
which is one reason that build never drew a correct map.

## Drawing

**The sheet is x-expanded once at boot, not once per character drawn.** At
H640/V200 a screen pixel is half as wide as it is tall, so a tile drawn as it
comes out of the file looks squashed. The cc65 build fixed that at draw time —
`draw_tile_in_view` ran an assembly routine (`expand_fullcolor_char_x`) over
*every character of every tile*, through a shared 200-byte scratch buffer at
`$0334`, then issued two DMA jobs per source character. `sheet_expand()` does
it once, in 96 iterations, leaving a sheet in which every tile is natively 8x4.
After that there is no expansion step in the drawing path, no shared scratch
buffer to collide over, and no assembly file at all.

**A tile row is one DMA job, not eight.** The eight characters of a tile row
are contiguous in the expanded sheet *and* the cells they land on are
contiguous in the store, so the row is a single `lcopy_transparent`. The same
change was worth four times the speed in hexgame's `fc_displayTile`.

**Palette index 0 is transparent** — the DMAgic is told to skip source bytes of
0 — which is what lets a hexagon be blitted over its neighbour without a
rectangle of background around it.

`src/mega65.c` keeps **one pre-initialised DMA list per operation**, so a call
stores only the four or five fields that actually change. mega65-libc rebuilds
all fourteen every time; `old-cc65/src/dma.c` already had the better
arrangement. It matters because a redraw is hundreds of these in a row.

## Scrolling

**This is the part that never worked, twice, and the answer is not to scroll.**

`view_redraw(off_x, off_y)` blanks the store and redraws every tile that
intersects the window at that character offset. About eighty tiles are visible,
four DMA jobs each, plus two fills — a few milliseconds. It is unconditionally
correct.

Both earlier attempts tried to *shift* the store with DMA row copies and repair
the newly exposed edge: `scroll_up`/`scroll_down`/`scroll_left`/`scroll_right`
in `old-cc65/src/graphics.c`, and `fc_scrollMerge*` in the XC=BASIC library.
Both have to work out which tiles overlap a one-character strip and redraw them
in the right order, against a lattice where a tile is eight characters wide and
three tall. Neither ever came out right.

**`view_redraw` iterates by hexagon row `hy`, not by array row `ay`.** Tiles
overlap by one character row, so a later draw must be the lower one — and
within one array row the odd columns sit three character rows *below* the even
ones. Stepping `hy` makes the screen y strictly increasing, so painter's order
falls out without sorting anything.

One character a step is eight pixels across and one row down; a hexagon is 8
characters wide and 3 rows apart, so that is eight sub-steps per column and
three per row. That is the smoothness the `vx`/`vy` pair in
`old-cc65/src/hexwar.c` was reaching for.

## The things cc65 was doing silently

Four differences account for most of the port, and are the first things to
suspect when something behaves oddly:

- **cc65 compiled string literals into PETSCII. Calypsi compiles them as
  written.** Anything compared byte for byte against the outside world has to
  be spelled the way the bytes really are — the FCI marker `sheet.c` looks for
  is `"IMG"`, upper case, because that is what `png2fci.py` writes.
- **The same in reverse for output.** `ascii_to_screencode()` in `src/screen.c`
  exists only because of this; the cc65 build poked literal bytes at the
  screen. Note it returns A–Z *unchanged*: with the lower case character set
  selected, screen codes 1–26 are `a`–`z` and 65–90 are `A`–`Z`.
- **The heap was whatever RAM was left. Calypsi's is what `--heap-size` says**,
  and a `malloc` returning NULL here gets written to anyway. Nothing in this
  program calls `malloc`.
- **The linker picks the smallest `printf` the format strings it can see need.**
  A runtime format passed through to `vsprintf` makes it pick `reduced`, which
  has no field widths. Add `--rtattr printf=medium` to `LDFLAGS` if that
  happens.

## Assets

`res/tiles.fci` is 24x4 characters — six 4x4 hex tiles side by side, in the
order `tiles/Makefile` appends the PNGs (desert hill, grass, mountain, ocean,
shallow ocean, tundra; `TILE_*` in `src/sheet.h`). It is checked in and
identical to `~/commodore/xc-basic3.xxx/xc-megademo/res/civ.fci`, which is what
the BASIC version used.

**`tiles/Makefile` is a separate, manual step** — run it by hand after editing
tile art. It appends the individual PNGs with ImageMagick and produces
`assets-fci/tiles.png`; `tools/png2fci.py` turns that into an FCI. The top-level
build does neither, since the FCI is checked in.

`tools/bin2s.py` turns the FCI into `build/tiles.s`. It emits a **`code`**
section, not `data`: Calypsi's `data` is *initialised* data, kept as an image
the startup copies into RAM, so a 6.5 KB blob there would cost 13 KB of the 32
and be copied for nothing.
