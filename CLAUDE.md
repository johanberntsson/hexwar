# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A Civilization-style hex game for the **MEGA65**, written in C for the
**Calypsi 6502** tool chain. It was a cc65 project until August 2026, and
there was an XC=BASIC 3 version as well.

**`old-cc65/` and `basic/` are gone from the tree** (18 Oct 2026) and live
only in git history: `git show 36047ba^:old-cc65/src/graphics.c`, or
`git show 737bfd8^:basic/civ.bas`. The paths into them that this file and
the source comments still cite are paths at those commits. Nothing should
need them any more.

The game is **Seven Ships of the Enemy** (`documentation/vision.md` has where the title
comes from), `sevenships` wherever a short name is needed: the repository,
the PRG, the disk and its map file `SEVENSHIPS.MAP`. Until 19 Oct 2026 it
was called Hexwar, and so is everything in git history before that.

`basic/civ.bas` was a more complete XC=BASIC 3 version of the same idea and
`basic/civ-basic.png` the layout it reached. The map window's size in
`src/view.h` comes straight from its `fc_setMergeTileMode(10, 1, 60, 23,
true)`, but **the window now starts at column 18, not 10**, as in
`civ1-reference/civ1-pic01.png`: the panel on the left is 18 columns wide so
that a minimap can go at its top. Neither the
BASIC nor the cc65 version ever scrolled correctly; see **Scrolling** below.
`documentation/design.md` holds the hex-grid and Civ-1 map-generator research, and
`documentation/techreport.md` is the technical report for readers rather
than for working on the code — program structure, memory allocation and the
main loop, in the format of Ozmoo's (pandoc markdown, `\$` in tables). **Keep
it in step** when the memory map, the startup or the loop changes; the
figures in it come from `build/sevenships.lst`. `documentation/vision.md` is the game design document — a Late Bronze Age eastern
Mediterranean 4X. Its §6 is the technical section and is written to agree
with this file; **if the two ever disagree, this file is right and §6 is
stale.** `documentation/vision-old.md` is a superseded draft. (All three
moved into `documentation/`; a bare `vision.md` or `design.md` below means
these, except hexgame's own `design.md`.)

## Build and run

```sh
make                      # build/sevenships.prg, build/sevenships.map and build/sevenships.d81
make run                  # build them and boot it in xemu (blocks until closed)
make START_OFF_X=41       # start the view one character further right; see below
make MAP_SEED=7           # another map; MAP_LAND=45 for more land
make FAKE_KEYS='{10,0,0,13},{12,0,0,13},{14,0,0,13},{16,0,0,13},{30,0,0,98}'  # see below
make NO_TITLE=1           # straight to the map, no title screen
make DEBUG=0              # without the Debug menu, as a release is
make release              # build the d81 (DEBUG=0) and copy it into release/
make clean
```

Needs `cc6502`/`as6502`/`ln6502` (Calypsi 5.18+), Python 3, Ruby (for
`tools/diskutil.rb`, which builds the d81 with an interleave that reads
faster from a real floppy) and `xemu-xmega65`. Pillow only if the tile art
changes, since `res/tiles.fci` is checked in.

**The program reads its tile sheet and its map from drive 8**, so it needs
the d81 mounted: `make run` does that, and a bare `-prg` boot stops at
`cannot read TILES.FCI`.

There are no tests. Verification is running it — see **Verifying a change**.

**`ln6502 -o` names the ELF, not the PRG.** The PRG is written beside it under
the same stem. Linking straight to a `.prg` name silently produces an ELF file
that the MEGA65 then tries to RUN as BASIC.

**`build/sevenships.lst` is the memory map and the only place the program's real
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
    -fastboot -8 $PWD/build/sevenships.d81 -screenshot $PWD/shot.png \
    -dumpmem $PWD/mem.bin -prg build/sevenships.prg
```

The screenshot is written when xemu exits, including on SIGTERM, so the
`timeout` is the run length. `-skipconfigfile` **must be the first argument**;
without it, concurrent runs race each other rewriting xemu's config template
and pop up dialogs at whoever is at the machine.

**`-dumpmem` gives 384 KB of chip RAM (`$00000-$5FFFF`)**, which covers screen
RAM at `$12000`, the whole tile store at `$40000` and the expanded tile sheet
at `$57000`.

**Nothing headless can press a key**, so `START_OFF_X` / `START_OFF_Y` (Makefile
`-D`s, defaults in `src/sevenships.h`) exist to start the view somewhere else. Two
builds one apart are how scrolling is checked: `make NO_TITLE=1` then `make
NO_TITLE=1 START_OFF_X=41` must shift the map exactly eight pixels and leave
the panel byte-identical.
Both were verified pixel-exact this way, along with one row vertically (16
screenshot pixels, since V200 is displayed doubled).

**The config stamp is what makes that work at all.** `START_OFF_X` reaches the
compiler as a `-D`, which make cannot see; without `$(STAMP)` in the Makefile a
second build finds every object up to date and hands back the *previous*
binary, which looks exactly like a change that did not take. It cost a
verification run before it went in.

**The stamp is written with make's `$(file)`, never through the shell**,
and must stay that way. It used to be `echo '$(CFLAGS)'`, and `FAKE_KEYS`
brings its own single quotes into `CFLAGS`, leaving its braces unquoted:
bash brace-expanded `{10,0,0,13},{12,0,0,13},…` into 4^n words — 268
million for a 14-entry script — which ate the machine's memory until the
kernel killed it, made the desktop crawl, and left the binary from the
*previous* script in place (18 Oct 2026). A headless run that ignores the
end of its script is the symptom; `build/config.stamp` should be a few
hundred bytes.

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
scrolling a redraw rather than a rearrangement. The one exception is a
popup (`src/popup.c`), which writes text cells over part of the window and
then has `view_restore()` put the character numbers back; the tile store
under it is never touched, so closing one needs no redraw.

### The ROM is never banked out, and that is unusual here

`~/commodore/mega65/hexgame` flattens the memory map to reach `$20000-$3FFFF`,
and pays heavily for it: a Hypervisor call to
lift the ROM's write protection, **a second one to put it back** (it is a
toggle that outlives the program, and forgetting it corrupts the *next* run), a
copy of the whole 128 KB ROM to attic RAM so the machine can still be reset, and
a rule that all disk I/O must finish first. (`~/commodore/SearchAndRescue` was
once listed here too, wrongly: it keeps the ROM and lives in banks 1, 4 and 5
plus attic RAM, as this does.)

The game needs none of it because of one number: the view is 60x23, so its tile
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
| `$2001-$9FFF` | 32 KB | program, data, BSS, C stack. About 30.2 KB used, 2.5 KB free |
| `$A000-$BFFF` | 8 KB | `highram`, under the BASIC ROM, banked out by `src/bank.s`: `map_tile`, the unit and city arrays, the sprite blocks, the AI's headings, the texts' staging and line buffers; about 2.7 KB free — `HIGH_BSS` in `src/mega65.h` moves a variable there, and **the startup does not clear it** |
| `$1600-$1EFF` | 2304 | `zpsave`: the popup's 400-byte text, the rest free — `LOW_FREE` in `src/mega65.h` moves a variable there |
| `$12000` | 4000 | screen RAM, 80x25 cells of 2 bytes |
| `$40000-$56EFF` | 94208 | the view's tile store, 23 rows of 4096 |
| `$57000-$5CFFF` | 24576 | the x-expanded tile sheet, 96x4 characters |
| `$5D000-$5DFFF` | 4096 | free: room for two more tiles of sheet |
| `$5E000-$5FFFF` | 8192 | the minimap's 16x8 full-colour characters |
| `$8000000` | ~1 KB | attic RAM: the texts, `TEXT.PAK`'s body (`src/text.h`); the rest of the 8 MB free |
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

### When bank 5 fills up

**Bank 5 is full** (18 Oct 2026): sheet, 4 KB for two more tiles, minimap.
This is chip RAM the *VIC* has to see — tiles, the minimap, anything else
drawn as full-colour characters. Data the CPU alone reads goes to attic
RAM instead and does not count. Decided, not yet done; take these in order
when a 15th tile or another full-colour surface needs room:

1. **The tile store's row padding.** Each of the 23 rows has 256 unused
   bytes after its 3840 (`VIEW_ROW_ADDR(r) + 3840`), 92 character slots
   of 64 bytes in all. A full-colour character's definition can be
   anywhere, so the minimap can take 92 of its 128 characters from there
   and keep only 36 (2304 bytes) in bank 5 — about five more tiles of
   room, for nothing but a less regular `pixel()` in `src/minimap.c` and
   its screen words. The redraw never writes past 3840 in a row.
2. **The C64 ROM, which C65 mode never runs.** A dump of the running game
   shows C64 BASIC at `$2A000` (`CBMBASIC` at `$2A004`) and the GO64
   KERNAL at `$2E000`, 16 KB together. Using them keeps the C65 KERNAL,
   CBDOS and the font, but needs hexgame's Hypervisor trap $70
   (`fc_toggle_rom_write_protect` in
   `~/commodore/mega65/hexgame/mega65-libc-modified/src/fcio_asm.s`) —
   **a toggle that outlives the program, so exactly two calls a run** —
   plus a copy of the 16 KB to attic RAM at start and back on quit, or
   GO64 breaks after the game. Check the layout against a dump of the ROM
   version actually installed, and that the font in use is not in the
   area taken.
3. **Banking the whole ROM out**, as `fc_bank_out_rom` does in hexgame,
   for all of `$20000-$3FFFF` less what the machine keeps running on.
   Last, because it costs the KERNAL: no disk I/O afterwards (Load/Save
   Game would have to put the ROM back from attic first), no `printf`, a
   raster handler of our own, and a quit that restores 128 KB and resets
   the machine, with the VIC-IV registers a real MEGA65 does not reset
   put back by hand. hexgame's `design.md` (*Putting the write protection
   back*) is the account of what went wrong there.

**The 32 KB is what to watch.** The tile sheet used to be linked in
(6.5 KB, and growing a KB a tile) and now comes off the disk, and
`map_tile`'s 4 KB has moved to `$A000`. At the last measurement (19 Oct
2026, after the rename and the blinking unit) `build/sevenships.lst`
reported the region **93.0% full (about 2.2 KB free)**, 94.3% after
the menus' keyboard control, 96.3% after the menu changes, Play 50 Turns
and the byline, 99.2% (about 260 bytes free) after save and load
(about 900 bytes, a fifth of it `_Stub_write`), and **97.4% (855 bytes
free) once the help screen became a text in attic RAM** (see **Texts**),
95.8% (1370 bytes free) once the title, setup screens and intro
followed it, and **92.2% (2549 bytes free) once `civ.c`'s names did**
(93.6% after tile select),
with milestones 1-4, the minimap, the city and help screens, the menu
bar, save and load and the quit-time register restore in it, and
`highram` 66.8% full.
Milestone 4 alone was about 2 KB, paid for in part by taking `puts` out of
`fatal()` (1.1 KB). The region was 89.9% before
`map_tile` moved, and 90.7% before the unit and city arrays and the sprite
blocks followed it. **Anything moved to `HIGH_BSS` must be set before it is
read**, since the startup clears only the ordinary BSS: `unit_reset()` and
`city_reset()` do it for the lists, `input_init()` for the sprites, and
the AI's `heading[]` is taken modulo 6. A `NO_TITLE` build is about 11 points
smaller, so measure the default build, which carries the Debug menu
(half a point more than a release).

**BASIC is banked out for the whole run** (18 Oct 2026, Search And
Rescue's arrangement, which is confirmed on a real MEGA65 with disk I/O):
`src/bank.s` clears `$D030` bit 4 in `__low_level_init` — before the
startup clears BSS, since a read of the window would otherwise come from
the ROM — and `bank_basic_in()` puts it back as the last thing before
`main()` returns, on both of its return paths. `sevenships.scm` declares
`$A000-$BFFF` as `highram` **with no `(type …)`**: `(type any)` makes it a
second content area as soon as anything initialised lands there
(*multiple program areas not allowed in prg output*), and `(type ram)`
does not work. Mark a variable `HIGH_BSS` to move it; **uninitialised data
only**, since the PRG is loaded with BASIC still mapped. `map_load()`
reads straight into it through the KERNAL. Seen in xemu and on a real
MEGA65 (18 Oct 2026): the map loads, the game plays, and quitting
reaches READY.

**Naming `zdata` in `sevenships.scm`'s `freeSpace` or `highram` would move the
whole BSS there** — which is what hexgame does — so use `LOW_FREE` and
`HIGH_BSS` per variable instead.

**Nothing that must survive a KERNAL disk call may live in `LOW_FREE`.** It is
below `$2001`, where the C65 ROM keeps its own buffers. Search And Rescue,
which uses all of it, writes it only after the last file has been read. Here
that means after `map_load()`, and again after any later load or save.

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
   straight on top of the tile store and the expanded sheet. Trim it to
   nothing: bank 5 is now the sheet, 4 KB kept for its next two tiles, and the
   minimap at `$5E000`. Leave bank 1 alone too: screen RAM is at `$12000` and BASIC 65 owns
   most of the rest. Attic RAM (`$8000000` up: the texts at the bottom,
   since the expanded sheet moved to bank 5) is the real reservoir,
   reached by DMA or `__huge`.
2. **Bank BASIC out and put BSS at `$A000-$BFFF`** — *done, see below.*
   Search And Rescue does
   exactly this and it is confirmed on a real MEGA65 (2 Oct 2026), disk I/O
   included. Copy its arrangement rather than inventing one: clear `$D030`
   bit 4 in `__low_level_init` (`src/bank.s` there); declare the window in the
   `.scm` with a name and a section and **no `(type …)`** — `(type any)` turns
   it into a second content area the moment anything initialised lands in it
   (*multiple program areas not allowed in prg output*), and `(type ram)` does
   not work; and mark what moves per object (`HIGH_BSS`), BSS only, so nothing
   loaded from the PRG has to go above `$9FFF`. The KERNAL stays in, and BASIC
   goes back before returning to it. `map_tile` is the obvious first tenant.
   TITAN Fall's C64 build does the same on the C64 (`$0801-$BFFF`).
3. **MAP-register regions plus a dispatch stub**, per part 3: 24 KB windows at
   `$2000-$7FFF`, one `#pragma clang section text="r1Text" ...` per file, and a
   resident trampoline in the `$1600-$1EFF` common area. This is the expensive
   one — it forces `--no-cross-call --no-interprocedural-cross-jump`, pushes
   shared data into a common section, and string literals do not survive a MAP
   change.
4. **Major modes reloaded from disk.** Unbounded, and not needed.

**Two rules for when step 3 arrives.** First, **the redraw path must stay
resident and undispatched** — `view_redraw` is 13.6 ms against a 19.97 ms frame
and stays ahead of the beam by about 100 lines; a trampoline in that inner loop
would spend the slack and bring the tearing back (see **Tearing**). View,
sheet, screen and DMA code stay in the common/resident region. Only per-turn
code — map generation, AI, pathfinding, diplomacy, the Crisis phase — goes in a
banked region, where a dispatch call costs nothing that matters. Second,
**give every new subsystem its own `.c` file now**, because that is what makes
the eventual split a one-line pragma per file instead of a rewrite.

**Step 2 is done** for `map_tile` (18 Oct 2026; see **Memory map**), with
about 2.7 KB of the window left for the next BSS that moves. **So is step 1
for text** (19 Oct 2026): the long texts and the names are in attic RAM
(see **Texts**), which took the region from 99.2% back to 92.2%. Before
reaching for step 3, look for more of that kind — big read-only tables
and strings that can be texts or attic data. Step 3 is the one to put off:
revisit when the region is near full again with nothing of that kind left
to move, and `highram` is full.
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
`build/sevenships-map.png`, which is the fastest way to judge a generator change.
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
readout at the top right of the screen (`draw_hover()` in `src/game.c`, via
`map_cell_at()`) names the terrain and the `ax,ay` of the hexagon under the
arrow.

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
buffer to collide over, and no assembly in the drawing path at all (the
one assembly file, `src/bank.s`, only banks BASIC).

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
  `blit_begin`/`blit` path and the padded row stride above; 13.6 ms (to
  raster line 143) once units and cities went in, measured with a settler
  and with a city in view. There is no
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

## The game

TODO.txt's milestones are the plan. Milestones 1 (pointer, turn loop, a
settler, founding a city), 2 (the city screen and building units), 3
(computer players) and 4 (combat and barbarians) are in, as of 18 Oct 2026.

**The title screen comes first** (`src/title.c`): `title_show()` puts the
title up once the texts are read and before the sheet and the map are,
and `title_run()` runs the menu — Start
Game, Load Saved Game, Quit — after
`input_init()`. Start Game goes on to two more screens, the people and the
difficulty level, and Esc on either goes back one. All three are the one
`menu_run()`: the arrow selects an item by hovering and a click picks it;
the cursor keys and Return, or a letter (`s l q`, `m h e c`, `1`-`5`), do
the same. It has its own once-a-frame loop of the same shape as
`game_run()`'s, and it runs on the plain text screen, so it must be over
before `view_init()` claims the map window's cells; `main()` clears the
screen in between. Quit returns from `main()` without the map view ever
having been set up.

**`main()` goes round for every game.** `game_run()` returns `GAME_QUIT`
(Esc, or the game was over) or `GAME_RETIRE`, and on a retire `main()`
puts the title back up and runs it again, then `view_init()`,
`game_init()` and `view_clear()` as for the first game. **`game_init()`
therefore resets everything a game leaves behind** — `map_reset()` (the
explored and occupied bits; the terrain is as loaded), `unit_reset()`,
`city_reset()` and `game.c`'s own state — and anything added to a game's
state has to be reset there too. The map is the same one each time.
`NO_TITLE` builds go straight into a new game after a retire.

**The menu bar** (`src/menu.c`) is row 0: Game, Orders, Debug, World,
Help. A click on a title opens its dropdown, picked by hover and
click or the cursor keys and Return; a click elsewhere or Esc closes it.
**MEGA and a title's first letter opens it from the keyboard**, as Alt
does on a PC (`menu_key()`), with the first item selected; cursor left
and right then go on to the next menu that has items. `$D610` delivers
MEGA+a-z as `$C1`-`$DA`, and `input_poll()` turns CTRL+a-z into the same
codes, `KEY_MEGA('g')` and so on, checking `$D611`'s CTRL bit so that
CTRL+C is not RUN/STOP's 3 or CTRL+M Return. xemu gives the PC's left
Alt to MEGA, so there it is Alt+G after all. **Compare a `KEY_MEGA` code
from a byte variable** (`menu_hotkey()`): they are all above 127, and the
first version, comparing against `(byte)(c + 0x60)`, met the sign
extension bug below and opened nothing.
Game has **Save Game** (see **Saving** below) and **Retire**, which asks "Do you want to quit this game?" with
`popup_ask()` (Yes/No buttons, `y`/`n`, Esc for No, or the cursor keys to move the highlight — No to begin with — and Return) and on Yes goes back
to the title screen. **Orders is filled in for the active unit** just
before a menu opens (`set_orders_menu()` in `src/game.c`, through
`menu_orders()`): Found City for a settler, Fortify for a warrior, and
for anything else, or no unit wanting orders, it is empty and does not
open; its items do what `b` and `f` do. World has **Top Five Cities**
(`src/topcities.c`), a screen with five empty places for now, and Help
has **Keyboard Control**, the help screen; both take over the screen and
`draw_screen()` puts the map back, as after HELP. **Debug exists only
with `DEBUG_MENU`**, which the Makefile defines unless `DEBUG=0`, and
`make release` defaults to `DEBUG=0` (and refuses `DEBUG=1`). Without it
the bar is Game, Orders, World, Help, `autoplay()` is not compiled, and
`map_show` is a constant 0 so the fog tests fold away; about 160 bytes,
95.6% against 96.1%. Anything debugging-only goes behind the same
`#ifdef`. Debug has **Play 50
Turns** (`autoplay()` in `src/game.c`): the computer plays the human's
side through the same `ai_turn()` as the others, then the usual
`end_turn()`, fifty times or until Esc or the game is over, and hands
back with the next unit asking for orders. The panel's turn and the
minimap keep up; the view is redrawn once at the end. The AI's rules
apply to the human's side meanwhile, `AI_UNIT_CAP` and what the cities
build included. Debug also has **Show Map**, a debugging toggle that
lifts the fog of war: `map_show` (`src/map.h`) is `MAP_EXPLORED` or 0, and
everything that draws ORs it into the map byte before testing the bit —
`view_redraw()` (hoisted into a local outside the loop), the city patch,
the minimap and the hover readout — so the real explored bits are never
changed and turning it off puts the fog back as it was. Toggling redraws
the whole minimap; a new game turns it off. A title with no items does not open. The
dropdown is drawn over the minimap and puts it back with
`minimap_show()` (and the map window's cells with `view_restore()` if it
reaches them); it hides the minimap's outline sprites, and the caller's
redraw puts them back. Nothing restores panel text, so **a dropdown must
end above row 11**, where the panel starts.

**Saving** (`src/save.c`) is one file, `SEVENSHIPS.SAV` on drive 8, and
**it is the game's memory written out as it stands**: a table of
`{address, size}` for every state array and counter — the map with its
explored and occupied bits, the unit and city arrays, the AI's headings,
the wars, `rnd()`'s seed, the peoples, the level, the turn and the year —
after a header of `"SSV"` and `SAVE_VERSION`. **Anything added to a
game's state goes in that table, and a change to it means a new
`SAVE_VERSION`**, or an old file is read into the wrong places. Left out
because they are worked out again: the active unit, the panel, and the
view, which centres on whoever wants orders. Game → Save Game writes it
with `_Stub_write`, opening `"@0:SEVENSHIPS.SAV,S,W"` — `@0:` replaces an
earlier save, `,S,W` makes a SEQ file open for writing — and says "Game
saved" or "Could not save" in the panel; a full disk is not noticed,
since that is in the error channel. Load Saved Game on the title screen
returns `TITLE_LOAD`, and `main()` runs `game_init()` as for a new game
and then `game_load()`, which reads the file over it in 128-byte pieces
(the `_Stub_read` page bug) and makes `game_run()` skip the opening
popup. If there is no file or the header is wrong, `main()` goes round to
the title, and `title_run(1)` says "No saved game could be loaded". The
statics the table needs were exported under module prefixes:
`combat_war`, `combat_seed`, `city_named`, `ai_heading`, `game_turn`,
`game_year`. Checked headlessly: the whole of `highram` is byte-identical
between a dump taken after saving and one after loading, and a second
save replaces the first (`c1541 -attach … -list` shows one file).
**Saving and loading work on a real MEGA65 too** (19 Oct 2026), so the
`@0:…,S,W` write through `_Stub_write` holds on the machine. To test
a load, run the saving build against a copy of the d81 and the loading
build against a copy of that, since a rebuild remakes `build/`'s image.

**The peoples are `src/civ.c`**: Mycenaeans, Hittites, Egyptians and
Canaanites (vision.md §3), each a name for the menu, an adjective for the
panel, a line about them, a leader and the title the diplomatic letters will
use (kept apart for that reason), and a dozen city names. **All of those
are texts** (`res/text/list_*.txt`, a name a line, by `CIV_*`), and
`civ_people(c)`, `civ_leader(c)` and the rest are macros for
`text_line()`; the barbarians are the fifth line of the people and
adjective lists. `player_civ[]` says which
people each player is — **every people plays**, `PLAYERS` = `CIVS` = 4,
the human first and the computer players the others in list order — and
`city_name_of()` goes through it. The five difficulty levels (Scribe to
Great King) are there too, in `game_level`, which nothing reads yet.
`NO_TITLE` builds play the Mycenaeans.

**The computer players are `src/ai.c`** (milestone 3). `ai_place()`, from
`game_init()`, puts each one's settler on grassland with at least four land
neighbours, at least 16 hexagons from every other settler (12, 8 and 4 if
200 random tries each find nothing). `ai_turn(p)` then plays TODO.txt's
five rules: a settler founds a city where it stands; the first warrior in
an unguarded city fortifies there and the rest explore (on along a heading
kept in `heading[]`, turning now and then); a builder goes to a hexagon
next to its nearest city and stays — **there are no improvements yet**, so
that is where rule 5 stops. Each city builds a warrior if the player has
none, then a builder if it has none, then warriors, until the player has
`AI_UNIT_CAP` (6) units, when it builds nothing (`city_build` =
`KIND_NONE`), so that three computer players cannot fill the 64 unit slots
everyone shares. Its random numbers are a xorshift with a fixed seed, so a
headless run is deterministic (`rnd()`, in `src/combat.c` since milestone
4). Since milestone 4 a warrior first attacks an enemy next to it if the
odds are good enough (`AI_ODDS`), then goes home if its city has nobody
fortified in it, then makes for the nearest enemy within `AI_RANGE`, and
only then explores. `approach()` takes an exploring step when no
neighbour is nearer, which is what gets units round water. Other players'
units and cities are drawn like the human's — only on explored hexagons —
and told apart by the hover readout ("Hittite Warrior", "Barbarian
Warrior") and on the minimap by colour: the human white, then red,
purple and orange, and the barbarians brown.

**Combat is `src/combat.c`** (milestone 4), after Civilization: attack
against defence (`unit_kind_attack[]`, `unit_kind_defence[]`: warriors
1/1, settlers and builders 0/1), the attacker winning with probability
A/(A+D), in quarter points so it stays in bytes. Defence is ×1.5 on hills,
×2 on mountains, and ×1.5 again for a unit that is fortified or in a city
(not both). The best defender on the hexagon fights; outside a city the
whole stack dies with it, in a city only the defender. An attack uses all
the attacker's moves and the winner stays put; **a city is taken by moving
into it once nobody is left in it** — captured by a people, burnt by
barbarians (`city_destroy()`, which moves the last city into the hole).
`unit_can_enter()` still says `ENTER_FOREIGN` for another player's
hexagon, and the caller decides that it is an attack: `attack()` in
`src/game.c` for the human, `attack_near()` in `src/ai.c` otherwise.
**War** is a bitmask per player in `combat.c`; everyone is at war with the
barbarians, the peoples start at peace, and only the human declares war,
by attacking ("Declare war on the Hittites?", `popup_ask_shown()`). The
computer players never do yet. **A player is out** (`player_alive()`) once
the last city is gone if it ever founded one (`city_founded`, a bitmask),
or when its units are gone if not: a computer player's remaining units
are removed with it, and for the human the game is over. A city keeps its
name when captured, which is why `city_founder[]` and not `city_owner[]`
picks the name list.

**The barbarians are `PLAYER_BARBARIAN`** (= `PLAYERS`, 4), an owner with
units but no cities, outside `PLAYERS` so the peoples' arrays stay four
long; anything indexed by owner (the minimap's colours,
`player_adjective()`/`player_people()` in `src/civ.c`) has a fifth entry
or a test. `ai_barbarians()` runs after the computer players: from turn
`BARB_FIRST_TURN` (15), one turn in eight on average, two warriors appear
on empty land at least `BARB_SPACING` (6) hexagons from every unit and
city, up to `BARB_CAP` (8) — a band can take it to 9. Every barbarian
attacks whatever is next to it, whatever the odds, or makes for the
nearest unit or city within 24. What happened to the human during the
other turns goes into `news` and is shown in the panel at the end of
`end_turn()` ("Barbarian attack / a city lost").

**The city screen** (`src/cityscreen.c`, after
`civ1-reference/civ1-pic02.png`) takes over the whole screen: the city's
name on top, three boxes — the units in the city, the city's hexagon and
its six neighbours, and what it is producing with a CHANGE button — and
EXIT at the bottom right (`c` and Esc/Return do the same). CHANGE turns the
production box into a list of what can be built (`buildable[]`: warriors
and builders), picked like a menu. **It is the map screen's own cells
rewritten**: text over the panel, the minimap and most of the window, and
in the middle the window's own full-colour cells, showing 24x10 characters
of the tile store that `view_draw_patch()` has cleared and drawn the seven
hexagons into. So nothing is allocated for it, and on the way out
`draw_screen()` in `src/game.c` puts the whole map screen back —
`screen_clear()`, `view_init()`, `minimap_show()` (cells only; the pixels
were never touched) and the panel — and marks the view dirty, so the next
frame's redraw replaces the patch. That redraw needs no clear, for the
usual reason. The production box also shows the rate and the turns left.

**Production** (`src/city.c`) stands in for population and food until they
exist: a city makes the shields of its own hexagon, at least 1, plus those
of its best neighbour — what a size-1 city working one tile would make.
`terrain_shields[]` there is the table (hills 2, grassland and mountains 1,
water and tundra 0), so a grassland city by hills makes 3 a turn: a warrior
(10) in four turns, a builder (20) in seven. Each city has `city_build` (a
`KIND_*`, warriors when founded) and `city_shields`. At the end of a turn,
after `ai_turn()`, `end_turn()` calls `city_produce()` on every city; at
the cost, the unit appears on the city's hexagon with its moves, so the
coming turn asks for its orders first like any other, and the surplus
carries over. The panel says "Mycenae / built Warrior" (the last city, if
several finish at once). With the unit list full the shields wait at the
cost.

**Popups** (`src/popup.c`) are a box of word-wrapped text over the middle
of the map window, closed by a click anywhere, fire, Return, space or Esc.
Build the text with `popup_begin()`, `popup_add()` and `popup_add_num()`
and show it with `popup_show()`, or `popup("...")` for a fixed one; `\n`
breaks a line. Like the menus it has its own once-a-frame loop, so the
input is still read every frame. **It must stay inside the map window**:
closing it restores the window's cells with `view_restore()`, and nothing
would put panel text back. A new game opens with one (`intro()` in
`src/game.c`), drawn over the map, which `NO_TITLE` builds skip.

**One loop, once a frame** — `game_run()` in `src/game.c`: wait for raster
line 243, redraw if `dirty`, then one `input_poll()` and act on what it
returns. There is no nested wait-for-a-key anywhere, because the mouse and
joystick must be read every frame whatever the game is waiting for, and
`input_poll()` counts its calls as frames (the joystick's acceleration, the
click debounce, `FAKE_KEYS`) because **the KERNAL jiffy clock does not
advance in this program**. What the game is waiting for is only
`unit_active`: a unit wanting orders, or `UNIT_NONE` and the end-of-turn
prompt in the panel, whose "Press Return" blinks — `ATTR_BLINK`, colour
RAM's attribute bit, so the VIC does it and no code runs for it. Seen
blinking in xemu and on a real MEGA65 (19 Oct 2026). A turn is: every human unit with moves and no standing
orders gets asked, in list order (`unit_next()`); then the prompt; then
`ai_turn()` for each computer player; then every city's production; then
the year moves on. The game is over
when the human has neither units nor cities.

**Units and cities are arrays per field** (`unit_kind[]`, `unit_ax[]`, …),
not arrays of structs: a byte array indexed by a byte is `lda abs,x` in
Calypsi, where a struct field is a multiply and a pointer. Kind 0 is a free
slot, so zeroed BSS is an empty list.

**Bit 7 of `map_tile` means "a city or unit is here"** (`MAP_OCCUPIED`; read
terrain through `MAP_TERRAIN`). That is how `view_redraw()` finds the few
cells that need anything drawn on them without another 4 KB layer or a search
of the unit list for every tile. `unit_mark()` keeps it right, and everything
that puts a unit or city on a hexagon or takes one off calls it.

**Bit 6 means "the player has seen this hexagon"** (`MAP_EXPLORED`;
terrain is the low six bits, `MAP_TERRAIN_MASK`). `map_explore()` sets it on
a hexagon and its six neighbours whenever a human unit is created or moves,
and nothing clears it. An unexplored hexagon is **drawn** as
`TILE_UNDISCOVERED`, a solid black hexagon, never skipped — the redraw does
not clear, so a skipped one would show the previous frame through — and
anything on it is not drawn. The black tile's outline is a little inside
the terrain tiles', but the intersection of all seven outlines still tiles
the plane (checked by simulation), so any mixture covers the window. The
hover readout says "Unexplored" there and hides city and terrain.

**The minimap** (`src/minimap.c`) is the whole world in a frame at the top
of the panel: 16x8 full-colour characters with definitions of their own at
`$5E000`, a 128x64-pixel framebuffer in which hexagon `(hx, hy)` is pixels
`hx` and `hx + 1` of line `hy` — two pixels by one line, square on the
glass, and the odd rows offset by a pixel as the tiles are. Pixels are
system palette entries 0-15 (black for unexplored, white a city, red a
unit). It is drawn in full once, by `minimap_init()`, and after that one
hexagon at a time by `minimap_plot()`, which `map_explore()` calls for each
newly explored hexagon and `unit_mark()` for every hexagon it looks at.
**The outline of the view is sprites 2 and 3, not pixels**, so scrolling
costs nothing but `minimap_frame()` rebuilding 54 bytes of sprite shape after
each redraw; sprite 3 carries the part of the outline that has wrapped round
the seam. A click on it centres the view, and the hover readout works over
it. `sprite_shape()`/`sprite_place()` in `src/input.c` hand sprites 2 and 3
out; 0 and 1 stay the arrow and are drawn in front.

**Overlays are drawn one hexagon row behind the terrain.** A unit's feet are
in its tile's bottom character row, which is also the top row of the
hexagons below, which are drawn later and are opaque in the middle. So the
occupied cells of row `hy` are noted while its terrain goes down, and their
city and unit are drawn once row `hy + 1`'s terrain is in.
Still top to bottom, at most three character rows behind, so the beam race
is unaffected.

**The active unit blinks**, half a second drawn and half a second not
(`unit_hidden`, flipped by `game_run()` every `BLINK_FRAMES`), and each
change is a whole `view_redraw()`. Redrawing only its hexagon was
considered and is not worth it: a tile overlaps five neighbours, so putting
the terrain back under the unit means redrawing parts of all of them in
painter's order, clipped, or keeping a snapshot — and the full redraw
already runs every frame while a cursor key is held. Anything else that
dirties the view shows the unit at once and restarts the blink, so a unit
is visible the moment it moves or is selected. The city screen's patch
always shows it.

**The pointer** is sprite 0 (white body) over sprite 1 (black outline), as
in hexgame, but 11 sprite pixels tall rather than 18. At H640/V200 a
monochrome sprite pixel is two pixels by one line — square on the glass and
exactly the size of a pixel of the x-expanded tiles. `input_x`/`input_y` are
sprite units from the top left of the display, 0-319 by 0-199, so a
character is 4 units across and 8 down; `pointer_hex()` in `src/game.c`
turns that into a hexagon. The sprite pointer list is relocated with
`SPRPTRADR` and 16-bit pointers, and **the list must be 64-byte aligned as
well as the shapes** — hexgame found that the VIC-IV drops the low bits of
the list address. Both are placed by hand in an over-sized BSS buffer;
`LOW_FREE` would do but is under the ROM's disk buffers. `input_shutdown()`
puts `SPRPTRADR` back before returning to BASIC.

Controls: the mouse or a joystick in either port moves the arrow, and the
button or fire is `KEY_CLICK`. A click next to the active unit moves it
there (or attacks, if another player holds the hexagon), on one of the
player's cities opens the city screen, on one of the player's units hands
it the orders, elsewhere on the map centres the view, on the minimap
centres the view there, and on the panel at the end of a turn ends it
(`click()` and `click_hex()` in `src/game.c`). A city comes before a unit
in it, so a fortified unit in a city can no longer be woken by a click.
**Everything must be possible from the keyboard alone**, since not every
MEGA65 has a mouse and the joystick is slow: **Tab is tile select**, as in
Civilization — a yellow ring (`TILE_SELECT`) on the active unit or the
middle of the view, moved by the cursor keys or the unit's moving keys,
and Return does what a click on its hexagon would; Tab or Esc puts it
away. The cursor keys go up and down by zigzagging between a column and
the one east of it, so all four reach every hexagon. While it is up the
ring blinks instead of the active unit, the hover readout follows the
ring, and other keys (`b`, `f`, the menus, HELP) work as usual.
`view_redraw()` draws the ring with the cities and units of its hexagon
row (`view_mark_hy`/`view_mark_x`, set by `place_mark()`), so it costs
the beam race nothing. Keys:
`0 - p * : =` move the active unit NW, NE, W, E, SW, SE — the `q e a d z c`
pattern on the right of the keyboard, by the cursor keys, so that those
letters are free for other orders (`:` is also taken as `[`, its shifted
form); the tiles are pointy-topped, so there is no N or S; a move onto
another player's unit or city is an attack; `b` founds a
city with a settler; MEGA or CTRL with a menu title's first letter (`g o d w h`) opens it; space skips the unit this turn, `w` asks again
after the others, `f` fortifies; Return ends the turn; the cursor keys
scroll; ESC or RUN/STOP quits to BASIC, after asking with `popup_ask()`
(Esc again is No). `w a s d` do not scroll.
**HELP or F1 opens the help screen** (`src/help.c`, and what it says is
`res/text/help.txt`; see **Texts**), which lists all of this; the map screen's bottom row only says "HELP for the keys". It takes
over the screen like the city screen, and `draw_screen()` puts the map
back. HELP arrives in `$D610` as `$1F` (mega65-core's
`matrix_to_ascii.vhdl`, which xemu copies), and `input_poll()` returns it
as `KEY_HELP`, F1's code, so `FAKE_KEYS` presses it as 241.

**The game knows when it is in xemu**: bit 5 of `$D60F` is set on a real
MEGA65 and clear in xemu, read into `input_real_hw` by `input_init()`. In
xemu, which has no HELP key and a PC keypad, the hint and the help screen
say F1, and **keypad 7 9 4 6 1 3 move the unit** — but only with
`res/xemu-keymap.cfg`, which `make run` passes with `-keymap`. Stock xemu
gives keypad 2 4 6 8 to its fake joystick and 7 9 1 3 to nothing; the
keymap turns them into F3-F8 (`$F3`-`$F8`, `KEY_PAD_FIRST`), which
`unit_key()` takes only when `input_real_hw` is 0. F3-F8 because the game
uses none of them and xemu keeps F9-F11 for hot keys. `0 - p * : =` still
work in xemu too.

**Tried on a real MEGA65 and it works** (18 Oct 2026, milestones 1-3):
the sheet and map load off the disk with the hot registers off, BASIC
banked out with `map_tile`, the unit and city lists and the sprite blocks
under it, the sheet and minimap in bank 5, the mouse and the joystick, and
the redraw, with nothing seen that differs from xemu. If something does
turn up there later, the places xemu is known to be looser are the mouse
port (Ozmoo saw port 1 on hardware, port 2 in xemu — the button is read
from both), colour RAM attribute bits, and timing, which xemu reports
optimistically for compiled C.

**Quitting puts BASIC's display back by value.** `screen_save()`, first
thing in `main()`, reads the VIC registers that make the screen's geometry
and colours, and `screen_restore()` writes them back before the return —
VIC-II/III ones first with the hot registers on, then the VIC-IV ones
exactly, `$D05D` last (Search And Rescue's `vic4_save_rom`/`vic4_leave`).
It used to force a fixed 40-column mode, but BASIC 65 boots in 80
columns, so after quitting it printed 80-column lines into a 40-column
screen in black, worse on the machine than in xemu. Anything new the game
does to the VIC has to be in `saved_regs[]` in `src/screen.c`. Seen in
xemu, from the title and from the game, and on a real MEGA65 (18 Oct
2026): READY on the blue 80-column screen.

## What the other Calypsi projects already found out

Two later projects use the same tool chain and have worked through problems
this one will meet: `~/commodore/SearchAndRescue` (MEGA65, a full game, its
`CLAUDE.md` sections **Memory map**, **Performance** and **Gotchas**) and
`~/commodore/titanfall` on the `calypsi` branch (a C64 game ported from
assembly to C; `c/README.md` and `c/COMPARISON.md`). Read those before
solving something they cover. The points that bear on this game:

**Attic RAM is slow to DMA *out of*, and xemu does not show it.** Measured on
a real MEGA65 by Search And Rescue: a DMA copy chip to chip is 2.2 cycles a
byte, **attic to chip 16.1** (7.3x), chip to attic 9.5. xemu runs attic at
chip speed. The expanded tile sheet used to be in attic RAM (`$8000000`), and
a redraw reads about 160 KB of it — roughly 9 ms from chip RAM and **about 65
ms from attic**, three frames, so the tearing fix would not have held on the
machine. **It is now in bank 5 at `$57000`**, just above the tile store, which
also makes every source address 16 bits in one bank. Nothing hot may go back
into attic RAM for the same reason. CPU access to attic is a flat ~15 cycles
slower than chip, and writes into it are nearly free.

**Timing in xemu:** never with `-sleepless` (about 19x real speed); time
inside the machine against the raster or a CIA. xemu's figures are accurate
for tight assembly and **optimistic by up to ~20% for compiled C**, so leave
margin on the 20 ms budget.

**Calypsi 5.18 code generation bugs, each of which cost someone a session.**
Check the `--list-file` listing when arithmetic that looks unarguable comes
out wrong:
- a narrowing cast to `uint8_t` inside a wider expression is
  **sign-extended**: `ptr + (uint8_t)(a + b)` lands 256 bytes low for
  offsets 128-255. Put the byte in a variable first, or widen and mask. The
  listing shows `ora #127`.
- `int8_t` coordinates and small `static const` arrays inside a function have
  produced a function that stored nothing at all. Use 16-bit coordinates.
- `(uint16_t)p[i] | (uint16_t)p[i + 1] << 8` with a byte `i` loads only one
  byte. Load the two halves into locals.
- a constant-false term in `&&` can be dropped (`if (f < 40 && X == 1)` with
  `X` 2 compiled to `if (f < 40)`). Use `#if` for compile-time switches.
- a function call inside a 32-bit expression emits `_FillZPQ`, which no
  library has. Hoist the call into a variable.
- `int` is 16 bits: constant expressions overflow silently, and a call with
  no prototype in scope truncates a 32-bit return (that one warns).
- this project's own: `_Stub_read` miscounts when the file ends on a 256-byte
  page boundary of the request (see the map loader under **The hex grid**).

**Speed in compiled C:**
- `--no-cross-call` is a per-file decision. Calypsi turns shared instruction
  runs into `jsr` fragments, even two-instruction loop bodies. Turning that
  off was worth 16% to a C hot loop in Search And Rescue, and TITAN Fall
  builds with it everywhere (+3% size). Worth measuring on `view.c`.
- `--strong-inline` gave **wrong answers** around the hardware multiplier. Do
  not use it.
- library `memcpy`/`memset` cost ~80 cycles a byte; a plain loop is ~20, and
  DMA is better still.
- integer promotion turns `(x + 6) / 12` into a call to `_UDivMod16`, and a
  32-bit C multiply is ~2200 cycles against 85 on the hardware multiplier at
  `$D770`. Keep hot arithmetic in bytes or shifts.
- `(base + n)[i]` compiles to `sta abs,x`; `base[n + i]` builds a 16-bit
  pointer every iteration.
- a function prologue that saves pseudo-registers (`_AllocStackSave`) is often
  the largest single item in a profile, so keep per-tile code flat.

**Size:** `printf` costs 6.5 KB, and Search And Rescue got it out of the link
entirely. `fatal()` used `puts`, which pulled in `__lib_fwrite`, `__lib_fflush`,
`__lib_rdflush` and `_Stub_write`, about 1.1 KB, for one error message;
it writes with `screen_putsxy` now, and nothing else should bring them back.

**Display:**
- **colour RAM's second byte has attribute bits 4-7 even on full-colour
  cells.** Blink, reverse, bold and underline apply to tiles too. An ink
  above 15 put a blinking underline on every tile on a real MEGA65, and xemu
  shows nothing. Map cells here get 0, which is right; keep it that way.
- the VIC-IV latches `SCRNPTR`, `LINESTEP`, `CHRCOUNT` and `CHRXSCL` once a
  frame. Changing them mid-screen does nothing, so a per-row geometry needs
  the raster rewrite buffer. Flipping `SCRNPTR` between two screen tables is
  how Search And Rescue double-buffers.
- a raster interrupt through `$0314` on the C65 ROM is entered with **five**
  bytes pushed (A, X, Y, Z, B), not the C64's three.
- Search And Rescue inferred, by elimination and never run on its own, that
  turning the hot registers off stopped the KERNAL opening files, so it loads
  everything first. The game loads its map *after* `screen_init()` has turned
  them off, and that works in xemu and on a real MEGA65 (18 Oct 2026), so
  the inference does not hold here.

**Input and quitting:** the joysticks share CIA1 with the keyboard (port 1
reads as keys and has to be masked; the fire button needs a debounce, and the
scan wants to run once a frame) — all of which `src/input.c` now does, ported
from hexgame. A reset re-runs
`AUTOBOOT.C65`, so quitting is a return through the `SYS`, which works here
because nothing is banked out. Search And Rescue's **The joystick** and
**Quitting** sections have the details.

**Headless testing:** the way to test anything that needs a key is a build
that fakes it, the same idea as `START_OFF_X`. **`make FAKE_KEYS=...` does
that here** (`src/input.c`): a list of `{frame, x, y, key}` entries, in frame
order, each moving the arrow to sprite position `(x, y)` (0,0 leaves it) and
returning `key` from `input_poll()` on that frame. `KEY_CLICK` is a mouse
click. **The title and new-game screens come first and count frames
too**, and so does the opening popup, so a script starts with four
Returns — Start Game, Mycenaeans, Scribe, and closing the popup: `make
FAKE_KEYS='{10,0,0,13},{12,0,0,13},{14,0,0,13},{16,0,0,13},{30,0,0,42},{60,0,0,13},{90,0,0,98}'`
starts the game, moves the settler east, ends the turn and founds a city,
and a screenshot shows the result. Menu item *i* is at sprite y `92 + 16i`
(`{40,160,108,KEY_CLICK}` clicks the second), and **an item the arrow
has hovered over stays selected after it leaves**, so a script that clicks
somewhere and then comes back to a menu has to select with the cursor keys
before pressing Return. The Game menu's title is at `{…,4,4,KEY_CLICK}`,
Save Game at `{…,8,20,KEY_CLICK}` and Retire at `{…,8,28,KEY_CLICK}`, or MEGA+G is `{…,0,0,199}` (O 207, D 196,
W 215, H 200) (the
script skips `$D611`, so it cannot test the CTRL path); Debug at `{…,52,4,KEY_CLICK}` and
Show Map at `{…,56,20,KEY_CLICK}`; `make NO_TITLE=1` skips all
three screens and the popup. Frames are counted in both loops, so a `-sleepless`
run is as deterministic as a real one. The script stops rather than wrapping, or the
presses would replay.

**Testing combat and the AI headlessly** takes two more tricks, both used
for milestone 4. **A long game is Debug → Play 50 Turns**:
`{60,0,0,196},{65,0,0,17},{70,0,0,13}` (MEGA+D, down, Return) after the
four opening Returns, and again for 100. `autoplay()` calls
`input_poll()` once a turn to see Esc, so **each turn played uses up one
`FAKE_KEYS` frame**: put the next key at least 50 frames later. Before
it, a long game was a script of `{f, 0,0,'f'},{f+3,0,0,13}` pairs,
generated by a shell loop. What happened is then read out of
`-dumpmem`: the unit and city arrays are in `highram`, and their
addresses are in `build/sevenships.lst` (`unit_kind = 0000a280`); **take them
from the listing of the build that ran**, since a relink moves the small
ones like `city_count`. A situation that would take hundreds of turns to
arise — an enemy next to the settler, an empty city beside a barbarian —
is quicker made by a few throwaway `unit_create()` calls in `game_init()`,
reverted afterwards, as are any counters added to see whether battles
happen at all.

End xemu with `SIGTERM` (a background job from a script
ignores `SIGINT`), and remember that the screenshot is taken when xemu
finally exits, which can be well after the signal.

**Map generation:** Search And Rescue's `tools/genmap.py` does its arithmetic
in fixed-point integers so that a seed reproduces a map byte for byte, and
routes roads with a Dijkstra in which high ground is a price, not a wall. Its
notes on rivers (a river cut against the map it is changing digs a canyon)
are worth reading before rivers go into `mapgen.py`.

## The things cc65 was doing silently

Four differences account for most of the port, and are the first things to
suspect when something behaves oddly:

- **cc65 compiled string literals into PETSCII. Calypsi compiles them as
  written.** Anything compared byte for byte against the outside world has to
  be spelled the way the bytes really are — the FCI marker `sheet.c` looks for
  is `"IMG"`, upper case, because that is what `mktiles.py` writes.
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

## Texts

**Long texts live in attic RAM, not in the 32 KB.** Each is a file in
`res/text/`, plain ASCII with a little markup in braces — `{at X Y}`,
`{tab N}`, a colour such as `{key}` or `{rev red}`, and `{A|B}` for
what differs between a real MEGA65 (A) and xemu (B), such as `{HELP|F1}`
— all described in `tools/mktext.py`'s docstring. The Makefile's `TEXTS`
lists them, **and the order there is the numbering**: `mktext.py` packs
them into `build/text.pak` (`TEXT.PAK` on the disk) and writes
`build/textid.h` with `TEXT_HELP` and the rest, rewriting it only when
the numbering changes, so editing what a text says remakes the pack and
the disk and compiles nothing. `text_load()`, **first of the three loads
at boot because the title screen is a text**, reads the pack's body into attic RAM at `$8000000` through a 128-byte
`HIGH_BSS` staging buffer (pieces under 256 for the `_Stub_read` bug; not
`LOW_FREE`, which the KERNAL's disk buffers share), and refuses a pack
whose count differs from the build's. `text_show(id)` draws one over the
screen as its markup says, reading it back out of attic a piece at a
time. A text is a whole screen or part of one; the caller clears first
and runs its own exit loop. `{centre Y}` places the rest of the line in
the middle of the screen, which is how `title.txt`, `people.txt` and
`level.txt` are laid out round `title.c`'s menus (they keep clear of
`MENU_ROW` and `NOTE_ROW`). **A popup's text** comes through
`popup_add_text(id, args)`, which reads it with `text_open()` and
`text_next()` and replaces each `{arg N}` with `args[N]` — `intro.txt`
gets the people and the leader that way — so a popup text is plain text,
line breaks and arguments, and the popup word-wraps it. **A list** is a
text with an entry a line (`list_*.txt`): `text_line(id, n)` copies line
`n` into one of two `HIGH_BSS` buffers, used in turn, so **its string is
good until the next call but one** — enough for a leader and a title at
once, and no more; copy it (`line_add()`, `popup_add()`) to keep it.
`civ.c`'s names, the levels and the title menu's items are lists, and
`menu_run()` in `title.c` takes a list's number rather than an array.
**All of it works on a real MEGA65** (19 Oct 2026): the pack loading
into attic RAM first thing at boot, and every screen, popup and name
drawn back out of it.

The machinery cost 661 bytes and moving the help screen saved 1342; the
title, setup screens and intro saved 515 more, and `civ.c`'s names 1179.
Each text moved from now on saves its full size. **Next**: the
Civilopedia, a text an entry. What is left in the program is short
messages built round numbers and names — the panel's, the city screen's,
the menus' — where the call costs about as much as the string. If the pack grows
large enough to slow the boot, the same index allows reading one text
from the disk when it is opened. A real-hardware screen can be checked
in xemu by building once with `input_real_hw = 1` forced in
`input_init()`, as was done for the help screen.

## Assets

`res/tiles.fci` is 48x4 characters — twelve 4x4 tiles side by side, in the
order `tiles/Makefile` lists the PNGs: six terrains (desert hill, grass,
mountain, ocean, shallow ocean, tundra), then the city, a selection ring
(the tile-select cursor),
three units (settler, warrior, builder) and the black hexagon of unexplored
land; `TILE_*` in `src/sheet.h`. The
terrain, city and unit art is OpenCiv's, under CC BY 4.0
(`tiles/README.txt`, and **Credits** in `README.md`, which must stay in
step); `tile_select.png` is a placeholder drawn from the grass tile's
outline.
**Only what the game uses goes into `tiles/`** — `alltiles/` is the whole
collection to pick from.

**The sheet is read off the disk** (`TILES.FCI`, by `sheet_load()`), a
character at a time, each widened and sent to bank 5 as it arrives, so none
of it is in the program. It used to be linked in through a `bin2s.py`, at 1
KB of the 32 a tile.

**`tiles/Makefile` is a separate, manual step** — run `make` in `tiles/`
after editing tile art, and check `res/tiles.fci` in. It runs
`tools/mktiles.py`, which reads the RGBA PNGs, makes every pixel with alpha
below 128 palette index 0 (transparent), and quantises the rest across all
tiles together to 240 colours at entries 16-255, leaving 0-15 to the system
palette the panel text uses. The art has about 1450 colours between it, so
it is quantised; with 240 or fewer it would be exact. `tiles/tiles.png` is
left as a preview of the result. (The old route — ImageMagick `+append` then
`tools/png2fci.py` — needed a single paletted PNG with transparency at index
0, which ImageMagick gives no say over; `png2fci.py` is kept but unused.)

### Adding tiles

A new tile needs all of these, and nothing checks that they agree:

1. the PNG in `tiles/`, 32x32 pixels (4x4 characters), transparent where
   the neighbours should show through, **appended** to `TILES` in
   `tiles/Makefile`. The order there *is* the tile numbering, so do not
   reorder.
2. `make` in `tiles/`, and `res/tiles.fci` checked in.
3. `SHEET_COLUMNS` and the `TILE_*` list in `src/sheet.h` (4 columns a
   tile). `sheet_load()` refuses a sheet whose size disagrees, so a stale
   constant stops at boot with "cannot read TILES.FCI or it is not the sheet
   this build expects".
4. for terrain: the matching constants in `tools/mapgen.py`, the generator
   taught to place it, and `terrain_name[]` in `src/game.c`. Terrain values
   are still tile numbers, so a new terrain has to come before the
   non-terrain tiles or the map layer grows a lookup table (see `map_tile`).
5. room: each tile is 2 KB expanded, and the sheet must stop below the
   minimap at `$5E000` (an `#error` in `src/minimap.c`): from `$57000`,
   **14 tiles, of which 12 are used**. The tile store's own `#error` in
   `src/view.c` keeps the store and the sheet apart. Past 14 something has
   to move: see **When bank 5 fills up**.
