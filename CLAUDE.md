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
`design.md` holds the hex-grid and Civ-1 map-generator research, and
`vision.md` is the game design document — a Late Bronze Age eastern
Mediterranean 4X. Its §6 is the technical section and is written to agree
with this file; **if the two ever disagree, this file is right and §6 is
stale.** `vision-old.md` is a superseded draft.

## Build and run

```sh
make                      # build/hexwar.prg, build/hexwar.map and build/hexwar.d81
make run                  # build them and boot it in xemu (blocks until closed)
make START_OFF_X=41       # start the view one character further right; see below
make MAP_SEED=7           # another map; MAP_LAND=45 for more land
make clean
```

Needs `cc6502`/`as6502`/`ln6502` (Calypsi 5.18+), Python 3, `c1541` and
`xemu-xmega65`. `pypng` and ImageMagick only if the tile art changes, since
`res/tiles.fci` is checked in.

**The program reads its map from drive 8**, so it needs the d81 mounted: `make
run` does that, and a bare `-prg` boot stops at `cannot read HEXWAR.MAP`.

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
    -fastboot -8 $PWD/build/hexwar.d81 -screenshot $PWD/shot.png \
    -dumpmem $PWD/mem.bin -prg build/hexwar.prg
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
| `$40000-$56EFF` | 94208 | the view's tile store, 23 rows of 4096 |
| `$8000000` | 12288 | the x-expanded tile sheet, 48x4 characters |
| `$FF81000` | 4000 | colour RAM |

**The store's rows are padded to 4096 bytes, and the padding is load bearing.**
A window row is 60 cells = 3840 bytes; end to end, rows would start at
arbitrary offsets and could straddle a 64 KB bank boundary, so every blit's
destination would have to be taken apart from a 32-bit address. At 4096 bytes
sixteen rows fit exactly in a bank, so row `r` is at bank `4 + (r >> 4)`,
offset `(r & 15) << 12` — both byte and word operations — and a 3840-byte row
can never cross into the next bank. `VIEW_ROW_BANK` / `VIEW_ROW_ADDR` in
`src/view.h`. The padding costs 5888 bytes that nothing else wanted, and buys
back two-thirds of the drawing time; see **Tearing**.

`$20000-$3FFFF` is left as the C65 ROM.

**The 32 KB is what to watch.** The linked tile sheet is 6573 bytes and
`map_tile` is 4096, so 10.5 KB is spoken for before any game code. At the last
measurement `build/hexwar.lst` reported the region 62.4% full (12331 bytes
free) with nothing in it but the display engine, the map loader and the position readout.

**Naming `zdata` in `hexwar.scm`'s `freeSpace` would move the whole BSS to
`$1600-$1EFF`** — which is what hexgame does — but `map_tile` alone does not
fit in 2304 bytes. Use `LOW_FREE` per variable instead.

## Growing past 32 KB

**The scarce thing is 16-bit address space, not RAM.** There is 384 KB of chip
RAM and 8 MB of attic, and the game will never want that much; what it cannot
have is more than 32 KB of `$2001-$9FFF` for code, initialised data, BSS and
the C stack all together. A full 4X will not fit there, so this is a known
future problem rather than a surprise.

The reference is Dan Sanderson's three-part Calypsi series, which is the only
worked account of this on the MEGA65 and matches this tool chain version:

- <https://dansanderson.com/mega65/calypsi-part-1/> — tool chain, flags,
  `--list-file`.
- <https://dansanderson.com/mega65/calypsi-part-2/> — the `.scm` files,
  memories and sections, `__far`/`__huge`, heap and C stack blocks.
- <https://dansanderson.com/mega65/calypsi-part-3/> — **the important one**:
  MAP-register regions, `#pragma clang section`, and the dispatch trampoline.

**Data and code have completely different costs here, and conflating them is
the mistake to avoid.** Data goes out of the way almost for free; code does
not. Take these in order and stop as soon as it fits:

1. **`__far` data.** Calypsi ships `mega65-banked.scm` next to `mega65-plain.scm`
   in `linker-rules/`, and it already declares `bank1` and `bank45` far
   memories. **Do not use `bank45` as shipped** — `(#x40000 . #x5ffff)` sits
   straight on top of the tile store. Trim it to `#x57000 . #x5ffff` (37120
   bytes) and leave bank 1 alone: screen RAM is at `$12000` and BASIC 65 owns
   most of the rest. Attic RAM above the expanded sheet (`$8003000` up) is the
   real reservoir, reached by DMA or `__huge`.
2. **Extend the `program` memory past `$9FFF`.** Part 2 says it can go to
   `$BFFF` for 40 KB. One line in `hexwar.scm`, but unverified here — it needs
   a headless boot before it is trusted, since the KERNAL stays live.
3. **MAP-register regions plus a dispatch stub**, per part 3: 24 KB windows at
   `$2000-$7FFF`, one `#pragma clang section text="r1Text" ...` per file, and a
   resident trampoline in the `$1600-$1EFF` common area. This is the expensive
   one — it forces `--no-cross-call --no-interprocedural-cross-jump`, pushes
   shared data into a common section, and string literals do not survive a MAP
   change.
4. **Major modes reloaded from disk.** Unbounded, and not needed.

**Two rules for when step 3 arrives.** First, **the redraw path must stay
resident and undispatched** — `view_redraw` is 12.8 ms against a 19.97 ms frame
and stays ahead of the beam by about 100 lines; a trampoline in that inner loop
would spend the slack and bring the tearing back (see **Tearing**). View,
sheet, screen and DMA code stay in the common/resident region. Only per-turn
code — map generation, AI, pathfinding, diplomacy, the Crisis phase — goes in a
banked region, where a dispatch call costs nothing that matters. Second,
**give every new subsystem its own `.c` file now**, because that is what makes
the eventual split a one-line pragma per file instead of a rewrite.

**Do none of this yet.** As of the last check there were 12331 free bytes in
the program region. Revisit when `hexwar.lst` shows the region past ~26 KB.
`~/commodore/mega65/hexgame/tools/mkprg.py` (exomizer, one file) is the other
known trick if the *loaded* size rather than the addressable size is what gets
tight.

## The hex grid

**The map wraps east-west, and only east-west.** It repeats every
`MAP_WIDTH_CHARS` = 512 characters, so `off_x` runs round `0..511` with no
clamp, and `view_redraw` counts columns in an unwrapped space shifted one map
width right (`vx = ax + MAP_COLUMNS`, so nothing goes negative near the seam)
and wraps only the array index. The last column's tile overlaps column 0's as
any two neighbours do, which needs `MAP_COLUMNS` even — there is an `#error`.
Verified pixel-exact: `START_OFF_X=508` shows `START_OFF_X=0`'s picture four
characters to the right. The generator wraps the same way, so land crosses
the seam.

**The map comes from `tools/mapgen.py`**, a build-time Python generator after
Civ 1's (design.md): land-mass random walks, inland mountain ranges, rainfall
swept along each row, latitude for tundra, an erosion pass, then shallow ocean
next to land. Its docstring is the file format: a 16-byte header (`"HXM"`,
version, columns, rows, layer count, seed) followed by 4096-byte layers. Layer
0 is terrain, and **its values are currently `TILE_*` numbers** because every
terrain has exactly one tile; more terrain types mean more tiles in the sheet
and, once the two diverge, a terrain-to-tile table. `make` leaves a preview at
`build/hexwar-map.png`, which is the fastest way to judge a generator change.
`map_load()` reads the file with the Calypsi `_Stub_open`/`_Stub_read` calls
directly; stdio would only add a buffer. That cost about 560 bytes. **It reads
a row per call because `_Stub_read` miscounts** when the file ends on the last
byte of a 256-byte page of the request: it returns before carrying into the
count's high byte, so one 4096-byte read of the last layer reports 3840 and
the load fails with every byte in place. Requests under 256 take a different
path in `stub_read.s` and are fine. 80x50 hid this, since 4000 ends mid-page.

**Doubled coordinates** (redblobgames): a hexagon is `(hx, hy)` with `hx` and
`hy` always the same parity, so a step east is `hx += 2` and the four diagonal
neighbours are `(hx±1, hy±1)`. Array cell `(ax, ay)` is hexagon
`(ax, 2*ay + (ax & 1))`, which packs 128x32 = 4096 hexagons into a rectangle.

**The map is 128x32, not Civ 1's 80x50, and the reason is easy to miss.** A
visual row of hexagons is every *other* column, and an array row is *two*
hexagon rows, so 80x50 looked like 40 hexagons across by 100 down — about
1:1.9 on the glass, portrait (walking it took 11 s north to south and 7.2 s
round east to west). 128x32 is 64 across by 64 down, 512 characters by 193
rows, and a character's 1:2 aspect makes that about 1.33:1, landscape. The
`ax,ay` readout at the top right of the screen (`draw_position()` in
`src/hexwar.c`, via `map_cell_at()`) is there to make this visible.

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

**The drawing path uses `blit_begin`/`blit`, not `lcopy_transparent`, and the
difference is most of the frame.** A 512-byte `lcopy` measured 84.8 µs of
which the DMAgic itself is only 27.2 — the rest was this code taking two
32-bit addresses apart into megabyte, bank and offset, which Calypsi has no
cheap way to do and a redraw does 320 times. `blit_begin()` sets everything
that stays put for a run (both megabytes, the source bank, the transparent
byte) and `blit()` then stores four fields, all 8 or 16 bit. That plus the
padded row stride took a redraw from **32.2 ms to 12.8 ms**.

Two things that were tried and are not worth revisiting: **chaining DMA jobs**
(bit 2 of the command byte) measured 25.8 µs a job against 27.2 for separate
triggers — 5%, for a much more complicated inner loop. And the `$D02F` I/O
knock that `do_dma()` performs on every job costs 1.3 µs, so removing it from
`blit()` was worth almost nothing on its own.

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

**A cursor step is two characters across and one row down**, because at
H640/V200 a character is twice as tall as it is wide, so that is the same
distance on the glass either way. A step of one character across moved the view
at visibly half the speed of a step down. A hexagon is 8 characters wide and 3
rows apart, so that is four sub-steps per column and three per row. The view
itself can still sit at any character offset — `START_OFF_X=41` still works —
and the left/right handlers clamp so an odd start cannot step past an even limit.

## Tearing

The first version of the redraw tore badly when a cursor key was held. It took
**32.2 ms against a 19.97 ms frame** (PAL, measured), so it was always painting
into the middle of a frame the beam was displaying. Three things fixed it, and
all three have to stay:

- **The redraw is fast enough to fit in a frame** — 12.8 ms, from the
  `blit_begin`/`blit` path and the padded row stride above. There is no
  double buffer: a second 94 KB store does not fit in the free banks, and
  making room would mean banking out the ROM.
- **It starts when the beam leaves the window.** `view_wait_for_redraw()` waits
  for raster line 243, the bottom of the map window. Measured, the redraw runs
  from `$D012` 243 to 136 of the next frame — about 205 lines — while the beam
  needs window row *r* only at line 128 + 8*r*. It stays ahead by roughly 100
  lines throughout, and finishes with ~6 ms of slack.
- **It draws strictly top to bottom and never clears.** The `hy` loop makes the
  destination row monotonically increasing, which is what "ahead of the beam"
  means. A `view_clear()` first would blank rows the beam has not reached yet —
  the flash it is trying to avoid — so the store is cleared **once**, in
  `main()`, and never again.

**The no-clear rule depends on the hexagons covering every byte between them,
and they only do so where every neighbour exists.** A redraw at offset (0,0)
leaves **4256 bytes** the transparent copies never wrote, because the first
hexagon row has no row above it to fill the notches between its tiles, so those
bytes keep the previous frame's picture. `VIEW_MIN_OFF_Y`/`VIEW_MAX_OFF_Y` in
`src/view.h` hold the view one hexagon inside the map at the top and bottom,
which makes coverage total for nothing at run time. Across there is no edge to
guard, since the map wraps. Verified at 0 uncovered bytes at all four corners
of the reachable range, the middle, and an odd-parity offset, and again
(after the wrap went in, and again at 128x32) at `off_x` 0, 508 and 510 either
side of the seam, and at `off_y` 4 and 166, the top and bottom limits.

**Anything that makes the redraw slower risks bringing the tearing back**, and
the budget is about 20 ms. Measure before and after: build a throwaway `main`
that times `view_redraw` with CIA2 timer A, which ticks at ~1 MHz whatever the
CPU is doing, so a reading is microseconds and 16 bits of it is 65 ms.

```c
POKE(0xdd0e, 0x00); POKE(0xdd04, 0xff); POKE(0xdd05, 0xff); POKE(0xdd0e, 0x11);
// read $DD05/$DD04 hi,lo,hi until hi agrees; it counts *down*
```

The KERNAL jiffy clock at `$A0-$A2` does not advance here, so it is no use for
this. And note `$D012` is 8 bits of a 0-311 raster, so it repeats for lines
256-311; any line below 56 is ambiguous, and 243 is not.

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
