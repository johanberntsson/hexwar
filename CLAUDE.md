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

**The program reads its texts, its code region, its tile sheet and its
map from drive 8** (`TEXT.PAK`, `SEVENSHIPS.R1`, `TILES.FCI`,
`SEVENSHIPS.MAP`, in that order), so it needs the d81 mounted: `make run`
does that, and a bare `-prg` boot stops at `cannot read TEXT.PAK`.

There are no tests. Verification is running it — see **Verifying a change**.

**`ln6502 -o` names the ELF, not the PRG.** Linking straight to a `.prg`
name silently produces an ELF file that the MEGA65 then tries to RUN as
BASIC. **The link is raw, not prg** (19 Oct 2026, the code banking under
**Growing past 32 KB**): two memories share `$6000-$7FFF`, which the prg
format refuses, so ln6502 writes one raw file a memory beside the ELF and
`tools/packprg.py` makes `build/sevenships.prg` and the region files
(`build/sevenships.r1`, `SEVENSHIPS.R1` on the disk) out of them.

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

**Comparing a build against the last one** is how the code banking and the
memory clean-ups were checked, and it catches what a screenshot cannot:
build `HEAD` in a scratch `git worktree` and the change in the tree, both
with the same `FAKE_KEYS` script (the long one used on 19 Oct 2026 goes
through the city screen, CHANGE, help, the menus, Save Game, Play 50 Turns,
Top Five, Retire, Load Saved Game and 50 more turns), `-dumpmem` both,
and compare `map_tile` and the unit and city arrays at the addresses each
build's listing gives (`unit_kind = 0000a280` and so on, at their real
lengths: 64 units, 32 cities). The game is deterministic, so they must be
byte-identical; the screenshots can differ only where a unit blinks.
**A quit is tested with a `DIM` at READY** (see **Quitting**), and BASIC
on its own with `-importbas FILE` (a BASIC 65 program as text, typed in
and RUN by xemu; `EDMA` fills memory from BASIC).

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
| `$2001-$5FFF`, `$8000-$9FFF` | 24 KB | the resident program: code, data, BSS, C stack. 500 bytes free, nearly all at `$5E0D-$5FFF` |
| `$6000-$7FFF` | 8 KB | the code window (`src/region.h`); as loaded, region 0 — the title, setup and help screens, the menus, save and load, the sheet's and the map's loaders, the owners' flags. 2.3 KB free |
| `$13000-$14FFF` | 8 KB | region 1, from `SEVENSHIPS.R1`: the computer players, the builders' improvements, the cities' yields and growth, bronze. 360 bytes free |
| `$15000-$16FFF` | 8 KB | region 2, from `SEVENSHIPS.R2`: the city screen, Top Five Cities and diplomacy. 2.2 KB free; room for region 3 up to `$18FFF` |
| `$19000-$1EFFF` | 24 KB | the tile sheet's tiles 18-29, where regions 4 to 6 would have been: the resources, mined copper and tin, the spearman; full |
| `$A000-$BFFF` | 8 KB | `highram`, under the BASIC ROM, banked out by `src/bank.s`: `map_tile`, the unit and city arrays, the sprite blocks, the AI's headings, the staging buffers of the texts and the regions, the texts' line buffers, the sheet loader's buffers, the saved VIC registers, the AI's exploration memory, the resource list; about 2.8 KB free — `HIGH_BSS` in `src/mega65.h` moves a variable there, and **the startup does not clear it** |
| `$1600-$1EFF` | 2304 | `zpsave` (56 bytes, BASIC's zero page during the run), then `lowfree`: the popup's 400-byte text and the scratch buffers of the view and the minimap, 1.6 KB free — `LOW_FREE` in `src/mega65.h` moves a variable there |
| `$12000` | 4000 | screen RAM, 80x25 cells of 2 bytes |
| `$40000-$56FFF` | 94208 | the view's tile store, 23 rows of 4096: 3840 for the window, then 256 holding four of the minimap's characters (all 80 of them, rows 0-19) or, in rows 20-22, two of the owners' flags (92 slots) |
| `$57000-$5FFFF` | 36864 | the x-expanded tile sheet's tiles 0-17, 2 KB each: all the room there is |
| `$8000000` | ~1 KB | attic RAM: the texts, `TEXT.PAK`'s body (`src/text.h`); the rest of the 8 MB free |
| `$FF81000` | 4000 | colour RAM |

**The store's rows are padded to 4096 bytes, and the padding is load bearing.**
A window row is 60 cells = 3840 bytes; end to end, rows would start at
arbitrary offsets and could straddle a 64 KB bank boundary, so every blit's
destination would have to be taken apart from a 32-bit address. At 4096 bytes
sixteen rows fit exactly in a bank, so row `r` is at bank `4 + (r >> 4)`,
offset `(r & 15) << 12` — both byte and word operations — and a 3840-byte row
can never cross into the next bank. `VIEW_ROW_BANK` / `VIEW_ROW_ADDR` in
`src/view.h`. The padding buys back two-thirds of the drawing time (see
**Tearing**), and its 5888 bytes are not wasted: they hold 92 of the
minimap's characters, so **nothing may write a row past byte 3840** —
`view_clear()` clears only the window's part of each row.

`$20000-$3FFFF` is left as the C65 ROM.

### When bank 5 fills up

Bank 5 is chip RAM the *VIC* has to see — tiles, the minimap, anything
else drawn as full-colour characters. Data the CPU alone reads goes to
attic RAM instead and does not count. It has room for 18 tiles of sheet
since the map went to 128x20 and the whole minimap into the tile store's
padding (20 Oct 2026), all used. **The sheet is not something the VIC sees, though**: it is only
ever a DMA source, copied into the tile store, so it needs chip RAM but
not bank 5 — which is what step 1b uses. Take these in order when another
tile or another full-colour surface needs room:

1. **The tile store's row padding** — *done* (19 Oct 2026). Each of the
   23 rows has 256 bytes after its 3840, 92 character slots in all, and a
   full-colour character's definition can be anywhere, so the minimap's
   first 92 characters were there and only 36 (2304 bytes) at `$5F000`
   (`char_number()` in `src/minimap.c`). That took the sheet's room from
   14 tiles to 16; and when the map shrank to 128x20 the minimap's 80
   characters fitted the padding altogether, which made it 18. Checked headlessly: pixel-identical to the old layout,
   including after a retire and with Show Map on.

   1b. **Bank 1, code regions 4 to 6** — *done* (20 Oct 2026, for
   milestone 6's overlays, and region 4 for milestone 7's resources).
   `$19000-$1EFFF` holds the tiles after bank 5's, twelve of them
   (`SHEET_HIGH` in `src/sheet.h`; 16-23 at first in `$1B000-$1EFFF`,
   18-25 since the map shrank, 18-29 since the resources), so three code
   regions are left (`REGION_LIMIT` in `src/region.h` has an `#error`). The sheet is stored a tile at a time
   (2 KB each, 512 bytes a row), so a tile's address is its base plus
   `row * 512` in either bank, and `view_draw_tile()` changes the DMA's
   source bank (`blit_source_bank()`) only when a tile is in the other one.
   More of bank 1 can go the same way, a region (four tiles) at a time,
   against room for code. **Confirmed on a real MEGA65** (20 Oct 2026): the
   overlays drew from bank 1 and scrolling was still fast (they are back in
   bank 5 since it grew to 18 tiles; the resources are bank 1's now); and
   region 2, at `$15000`, with city growth.
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

**The 32 KB is what to watch** — or since the code banking (19 Oct 2026),
**the 24 KB of it that is resident**, `$2001-$5FFF` and `$8000-$9FFF`,
with the window between: see **Growing past 32 KB**, step 3, for its
figures. What follows is the history of the whole 32 KB before that.
The tile sheet used to be linked in
(6.5 KB, and growing a KB a tile) and now comes off the disk, and
`map_tile`'s 4 KB has moved to `$A000`. At the last measurement (19 Oct
2026, after the rename and the blinking unit) `build/sevenships.lst`
reported the region **93.0% full (about 2.2 KB free)**, 94.3% after
the menus' keyboard control, 96.3% after the menu changes, Play 50 Turns
and the byline, 99.2% (about 260 bytes free) after save and load
(about 900 bytes, a fifth of it `_Stub_write`), and **97.4% (855 bytes
free) once the help screen became a text in attic RAM** (see **Texts**),
95.8% (1370 bytes free) once the title, setup screens and intro
followed it, and **92.2% (2549 bytes free) once `civ.c`'s names did**,
93.6% after tile select and **95.6% (1458 bytes free) after the
computer players' settlers**, with milestones 1-4, the minimap, the city
and help screens, the menu bar, tile select, save and load and the quit-time register restore in it, and
`highram` 66.8% full. Then **91.3% (2858 bytes free)** after a clear-out
before milestone 5 (19 Oct 2026), with `highram` 69.5%: the DMA lists
became initialised data instead of a `dma_init()` and an `if
(!initialised)` in every call (about 520 bytes), the C stack went from 512
to 256 (measured at about 70; see `CSTACK` in the Makefile), and the
scratch buffers moved out — the view's and the minimap's to `LOW_FREE`,
the sheet loader's and the saved VIC registers to `HIGH_BSS`. **A buffer
goes to `LOW_FREE` only if it is rebuilt on every use and never in use
across a disk call.** `--space` instead of `--speed` was tried for
everything but `view.c` and `mega65.c` and saved 30 bytes, so it was not
kept.
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
   nothing: bank 5 above the tile store is the sheet, to `$5FFFF`. Leave bank 1 alone too: screen RAM is at `$12000`, and `$13000-$1EFFF`
   is for code regions (step 3). Attic RAM (`$8000000` up: the texts at the bottom,
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
3. **MAP-register regions plus a dispatch stub**, per part 3 — *done, as
   an 8 KB window at `$6000-$7FFF`; see below.* The article has 24 KB
   windows at `$2000-$7FFF`, one `#pragma clang section text="r1Text" ...`
   per file, and a resident trampoline in the `$1600-$1EFF` common area.
   It forces `--no-cross-call --no-interprocedural-cross-jump`, and string
   literals do not survive a MAP change.
4. **Major modes reloaded from disk.** Unbounded, and not needed.

**Where banked code can live: bank 1, `$13000-$1F5FF`, 49.5 KB (50688 bytes)** (surveyed
19 Oct 2026, xemu, ROM 920413). MAP needs chip RAM — attic is far too slow
to run code from — and banks 0, 2-3 and 4-5 are taken, so this is the
place. It is BASIC 65's variable area, empty at READY (all zero in a dump
of a fresh boot), and BASIC is banked out for the whole run anyway. A
throwaway build filled it with `$A5` first thing in `main()` and played a
session with every disk operation the game does — map, texts and sheet at
boot, Save Game, 50 turns, Retire, Load Saved Game, 50 more — then quit to
READY: **not one byte of it was touched**, by the KERNAL, CBDOS or the
return to BASIC. Screen RAM is below it at `$12000-$12F9F`; above it,
`$1F600-$1F6FF` holds something at READY and `$1F800-$1FFFF` is the C65's
colour RAM window, so neither is to be used. MAP offsets are in 256-byte
steps, so two 24 KB regions fit (`$13000-$18FFF`, `$19000-$1EFFF`) with
1.5 KB over. (**Since milestone 7 the top 24 KB of it, `$19000-$1EFFF`, is
the tile sheet's**; see **When bank 5 fills up**, step 1b.) On a real MEGA65 (19 Oct 2026) the first try of BASIC after
quitting hung in `DIM` — but so did the plain game in xemu, with nothing
in bank 1: it was the zero page not being put back (see **Quitting**
below), fixed the same day. BASIC itself treats the area as its own and
clears it — `DIM` zeroes all of `$13000-$1F5FF` (seen in xemu) and a
`RUN` changed every byte of it on the machine — so **banked code has to
be loaded afresh on every run**, never assumed to have survived one.
**Confirmed on a real MEGA65** (19 Oct 2026) with the fixed build: the
area full of `$A5` at start, a session with save, Play 50 Turns, retire
and load, and **0 bytes changed** at the quit; then `A=1:PRINT A`,
`DIM B(1000):B(1000)=5`, a string, and `NEW` and a small program all
worked at READY. And since the resident program already fills
`$2001-$9FFF`, the 24 KB window at `$2000-$7FFF` of Dan Sanderson's
layout does not fit as it stands: deciding where the window goes, and
what moves out of its way, is the first job of the proof of concept.

**A code window works with the KERNAL kept, if every MAP change also
updates the KERNAL's copy of the map at `$011C-$011F`** (found 19 Oct
2026, xemu, ROM 920413). MAP cannot be read back, so the C65 KERNAL keeps
the map in force there and maps back to it after its own MAP changes
(`LDQ $011C : MAP` at `$F26E`; `$0110-$0113` is BASIC's map, `$0114` the
DOS's). During the game `$011C` holds `00 E0 00 83`: the lower half
identity, the upper half block 7 only, offset `$30000` — the KERNAL at
`$3E000`. A window set with MAP alone was switched off again within a
frame by the KERNAL's interrupt and by the first disk access; with
`STA/STX/STY/STZ $011C-$011F` before the `MAP` it held through 250 frames
of interrupts, `printf` and a file read (the throwaway test is
`maptest.d81`). So **the window must be in the lower half**, since the
upper half carries the KERNAL; the map routine should take Y and Z from
`$011E/$011F` rather than hard-coding them, and **leave Z at 0**, which
compiled code assumes. **Confirmed on a real MEGA65** (ROM 920413, the
same as xemu's) the same day; another ROM version may keep the copy
elsewhere.

**Two rules for banked code.** First, **the redraw path must stay
resident and undispatched** — `view_redraw` is 13.6 ms against a 19.97 ms frame
and stays ahead of the beam by about 100 lines; a trampoline in that inner loop
would spend the slack and bring the tearing back (see **Tearing**). View,
screen and DMA code stay resident, and so does the once-a-frame loop. Only per-turn
code — map generation, AI, pathfinding, diplomacy, the Crisis phase — goes in a
banked region, where a dispatch call costs nothing that matters. Second,
**give every new subsystem its own `.c` file**, because that is what makes
banking it a pragma and a few stubs instead of a rewrite.

**Step 3 is done** (19 Oct 2026), as an 8 KB window rather than the
article's 24 KB, since the KERNAL's map takes the upper half and the
resident program the rest of the lower: `src/region.h` is the scheme and
its rules, `src/dispatch.s` the MAP and the stubs, `tools/packprg.py` the
packing. `ai.c` runs from bank 1 as region 1; the screens, the menus,
save and the sheet loader are region 0. Checked headlessly against the
build before it, byte for byte: a scripted game through the city screen,
help, the menus, save, 100 turns, retire and load left every unit and
city array and the map identical, and `DIM` still works at READY after
quitting. **Confirmed on a real MEGA65** the same day: title and setup,
the city screen, the menus by mouse and MEGA+letter, HELP, Play 50 Turns,
save, retire and load, and `DIM` after quitting. **Resident space is what to watch now**: 500 bytes of it (20 Oct 2026,
after city growth, the AI pass, diplomacy and exploring, the fixes after
them, the resources, the owners' flags and bronze), against 2.3 KB
in region 0, 360 bytes in region 1 (nearly full: the next AI work wants region 3) and 2.2 KB in region 2, plus one empty region (4 to 6 are the tile sheet's
now). **Region 2** came with city growth, when the city screen's new box
left region 0 with 217 bytes: it took a `win2` memory in
`sevenships.scm`, a third entry in `dispatch.s`'s map tables (offset
`$0F000`), `REGIONS` 2 and the second file in the Makefile's
`REGION_FILES`; `packprg.py` and `region.c` needed nothing. So new
subsystems go into a region from the start, per-turn ones especially.
`--no-cross-call --no-interprocedural-cross-jump` are on for the banked
files only (the Makefile's `BANKED`).

**Step 2 is done** for `map_tile` (18 Oct 2026; see **Memory map**), with
about 2.7 KB of the window left for the next BSS that moves. **So is step 1
for text** (19 Oct 2026): the long texts and the names are in attic RAM
(see **Texts**), which took the region from 99.2% back to 92.2%. Moves of
that kind — big read-only tables and strings to texts or attic data —
are still the cheaper way to resident space, where they apply.
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
Civ 1's (design.md): land-mass random walks, inland mountain ranges and
hills, rainfall swept along each row (desert, plains, grassland and forest by
how wet a hexagon is, desert only in the warm band), latitude for tundra, an
erosion pass, rivers, then shallow ocean next to land. **Rivers are not drawn
yet** — `tile_river.png` is a 17x3 texture, not a tile — but where one
crosses desert or plains it leaves a line of floodplain, the Nile's
arrangement. Its docstring is the file format: a 16-byte header (`"HXM"`,
version, columns, rows, layer count, seed), then layer 0, a byte a cell, and
layer 1, the resources as a sorted list (see **Resources**). Layer
0 is terrain, and **its values are `TILE_*` numbers** because every
terrain has exactly one tile, which is why the ten terrain tiles come first
in the sheet (`TERRAIN_COUNT`) and the city, units and the rest after them.
A terrain that needed two tiles, or an eleventh terrain once the sheet is
full, would mean a terrain-to-tile table in the redraw instead. `make` leaves a preview at
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
`(ax, 2*ay + (ax & 1))`, which packs 128x20 = 2560 hexagons into a rectangle.

**The map is 128x20, not Civ 1's 80x50, and the reason is easy to miss.** A
visual row of hexagons is every *other* column, and an array row is *two*
hexagon rows, so 80x50 looked like 40 hexagons across by 100 down — about
1:1.9 on the glass, portrait (walking it took 11 s north to south and 7.2 s
round east to west). 128x20 is 64 across by 40 down, 512 characters by 121
rows, and a character's 1:2 aspect makes that about 2.1:1, a wide band like
the Mediterranean. **It was 128x32 (64 by 64, 4096 hexagons) until 20 Oct
2026**: Civilization's 4000 squares are for seven peoples and these are
four, a hexagon map feels the larger, and with one-hexagon-a-turn warriors
the peoples took 60 to 150 turns to meet, or never did. The width stays a
power of two, which `& (MAP_COLUMNS - 1)` in the wrap relies on; the rows
are free, but the minimap is a line a hexagon row (an `#error`), and its
characters must fit the tile store's padding (another). The
readout at the top right of the screen (`draw_hover()` in `src/game.c`, via
`map_cell_at()`) names the terrain, its yield and the `ax,ay` of the hexagon under the
arrow — or, from when the orders pass to a unit or a unit moves until the
arrow next moves, of that unit's hexagon (`hover_on()`), and of tile
select's ring while it is up.

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

**A tile row copies only the characters that have pixels.** `sheet_load()`
notes, for every row of every tile, the first expanded character with a
non-zero pixel and how many from there to the last (`sheet_row_lead`,
`sheet_row_width` in `HIGH_BSS`), and `view_draw_tile()` copies just those,
skipping a row with none. The result is the same — the characters left out
are all transparent — but a unit or an overlay, a small picture in the middle
of its tile, costs one or two short jobs instead of four of 512 bytes. **Keep
overlay art inside pixel lines 8-23** of its 32, the middle two character
rows: the mine and the irrigation are, which halved their cost.

**Overlays go straight over their own terrain, in the same pass**, not a row
behind like units: an improvement is drawn inside its hexagon, and nothing
drawn later covers the inside of a hexagon. In `view_redraw()` it is one
test of the map byte per hexagon, `t & MAP_IMPROVEMENT`.

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
  and with a city in view. **Measured again with CIA2 around the whole
  `view_redraw()` call** (20 Oct 2026, a `NO_TITLE` build at
  `START_OFF_X=90 START_OFF_Y=60` with Show Map on, not `-sleepless`),
  which takes in the raster interrupt too: 15.9 ms at HEAD before the
  overlays, 16.0 ms after them with none in view, and **20.5 ms with an
  overlay on two hexagons in three** — about 70 µs an overlay. That needed
  the row extents (see **Drawing**) and overlay art kept to the middle two
  character rows; it was 25.7 ms without either. There is no
  double buffer: a second 94 KB store does not fit in the free banks, and
  making room would mean banking out the ROM.
- **It starts when the beam leaves the window.** `view_wait_for_redraw()` waits
  for raster line 243, the bottom of the map window. Measured, the redraw runs
  from `$D012` 243 to 136 of the next frame — about 205 lines — while the beam
  needs window row *r* only at line 128 + 8*r*. It stays ahead by roughly 100
  lines throughout, and finishes with ~6 ms of slack. **So the real limit
  is about 24 ms, not 20**: a redraw spread evenly down the window stays
  ahead of the beam until the beam reaches the last row, 197 + 8 × 23 lines
  after it started; one longer than 20 ms only misses the next frame's line
  243, so the view runs at 25 frames a second there rather than tearing.
  Leave the 20% by which xemu flatters compiled C out of that.
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
(computer players) and 4 (combat and barbarians) are in, as of 18 Oct 2026,
and 5 (plains, forest, desert and floodplain) as of 20 Oct 2026. **6 is
done but for roads** (20 Oct 2026), which wait for milestone 7 since both
need tiles and roads a map bit: improvements, yields, city size and
growth, worked tiles by hand, Civilization's production rules for the
computer players and its grassland shields. Ahead of milestone 8 there is
contact between the peoples and an Advisors → Diplomacy screen (see
**Contact**), and the exploring that makes contact happen (see
**Exploring**). **Not yet tried on a real MEGA65**: the 128x20 map with its
16x5 minimap and the panel moved up, the menu bar with Advisors, the
Diplomacy screen and the greeting popup.

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

**The menu bar** (`src/menu.c`) is row 0: Game, Orders, Advisors, World,
Debug, Help. A click on a title opens its dropdown, picked by hover and
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
(`src/topcities.c`, region 2): the five largest cities the player has
seen, whoever's, with their people and size. Advisors has **Diplomacy**
(see **Contact** below). Help
has **Keyboard Control**, the help screen; all three take over the screen and
`draw_screen()` puts the map back, as after HELP. **Debug exists only
with `DEBUG_MENU`**, which the Makefile defines unless `DEBUG=0`, and
`make release` defaults to `DEBUG=0` (and refuses `DEBUG=1`). Without it
the bar is Game, Orders, Advisors, World, Help, `autoplay()` is not compiled, and
`map_show` is a constant 0 so the fog tests fold away; about 160 bytes,
95.6% against 96.1%. Anything debugging-only goes behind the same
`#ifdef`. Debug has **Play 50
Turns** (`autoplay()` in `src/game.c`): the computer plays the human's
side through the same `ai_turn()` as the others, then the usual
`end_turn()`, fifty times or until Esc or the game is over, and hands
back with the next unit asking for orders. The panel's turn and the
minimap keep up; the view is redrawn once at the end. The AI's rules
apply to the human's side meanwhile, the unit cap and what the cities
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
end above row 9**, where the panel starts (row 11 until the minimap shrank
to five rows).

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
`game_init()`, puts each one's settler — and, since 20 Oct 2026, a warrior
with it, as the human gets one, so that exploring starts on the first
turn — on grassland with at least four land
neighbours, at least 16 hexagons from every other settler (12, 8 and 4 if
200 random tries each find nothing). `ai_turn(p)` then plays TODO.txt's
five rules: a settler founds a city where it stands; warriors fortify in
a city short of guards and the rest explore (see **Exploring** below); a
builder improves the
hexagons round its nearest city — `improve_best()` on the one it stands
on, if that is next to the city, or else it makes for the first neighbour
of the city that wants something and has nobody on it — those the city
works first (`work_site()`) — and waits next to the city when none does.
**What a city builds follows Civilization 1's production rules** (milestone
6; `GameEngine.cs` in OpenCivOne has the original), decided afresh every
turn in `ai_turn()`: 1. a warrior while it has fewer fortified guards than
`guards_wanted()` — one, one more for every four citizens, and one more
with an enemy within `THREAT_RANGE` (3), Civilization's "defend" policy;
none at all before `BARB_FIRST_TURN` with no enemy near, so that the
starting warrior explores —
even over the unit cap if it has none; 2. a settler, from a city of size 2
or more with more than 1 food a turn to spare, or from the first such city
when the player has no settler, if there is a site in reach and its
cities and settlers come to fewer than `AI_CITY_CAP` (6); 3. a builder,
up to one per city, if it has hexagons to improve; 4. warriors, until
the player has `AI_UNIT_BASE` + `AI_UNITS_A_CITY` per city units (3 + 2,
at most `AI_UNIT_MAX`, 12), settlers apart, when it builds nothing
(`city_build` = `KIND_NONE`), so that three computer players and the
barbarians cannot fill the 64 unit slots everyone shares. A settler takes
a citizen, so it waits for size 2 (see **Production**). In 150-turn
autoplays this gave 18 cities on map seed 1 and 23 on seed 7, with 41 and
46 improvements and all four peoples alive. Before Civilization's
grassland shields (see **Yields and growth**) it was 14 and 22, and on
seed 1 the Egyptians died by turn 50: a capital making a shield a turn
spent 20 turns on a settler with one guard, and the new city fell to a
pair of barbarians. A site (`find_site()`) is grassland, plains, floodplain or hills,
off the two edge rows, with at least `SITE_LAND` (3) land neighbours and
at least `CITY_SPACING` (4) hexagons from every city; the settler makes
for the nearest one from where it stands and founds there, and explores
if there is none in reach. `MAX_CITIES` (32) is shared with the human.
Its random numbers are a xorshift with a fixed seed, so a
headless run is deterministic (`rnd()`, in `src/combat.c` since milestone
4). Since milestone 4 a warrior first attacks an enemy next to it if the
odds are good enough (`AI_ODDS`), then goes home if its city is short of
guards and it is within `RECALL_RANGE` (4) of it, then makes for the
nearest enemy within `AI_RANGE`, and only then explores. `approach()`,
when no neighbour is nearer, takes a step that keeps the distance, the
one nearest its heading and never straight back, which follows a coast;
only with none does it fall back on `explore_out()`.

**Exploring** (20 Oct 2026, after the peoples took 60-150 turns to meet,
or never did): every player keeps `EXPLORERS()` warriors that are not
guarding — one, and one more for every three cities — and a city builds
one after its guards. **Each player remembers where it has been**:
`ai_seen[p]` is the map in blocks of 8x4 cells, a bit each (16 bytes a
player, saved), set for every block one of its units or cities is in at
the start of its turn; `ai_land` says which blocks have land. An explorer
makes for the middle of the nearest unseen block with land, round water
with `approach()`, and gives it up as seen after `PATIENCE` (12) turns of
coming no nearer (`unit_work[]` counts them) — it is over the water. With
nothing left, or for a barbarian, `explore_out()`: the step furthest from
its nearest city, then on along its heading. Before the block memory an
explorer walked east along its strip of land and never found the neck
leading to the Canaanites 16 hexagons south; before the recall range every
explorer was called home to guard as soon as a city grew; and before the
side step a warrior paced between two hexagons for good. Measured as the
turn of first contact in 150 turns of autoplay (a throwaway build that
records it): see TODO.txt. Other players'
units and cities are drawn like the human's — only on explored hexagons —
and told apart by the hover readout ("Hittite Warrior", "Barbarian
Warrior"), on the minimap by colour — the human white, then red,
purple and orange, and the barbarians brown (`player_colour[]` in
`src/minimap.c`) — and **on the map by a flag** in the same colour at the
top left of the unit's or city's tile. Without it every people's units
looked alike, and a player told "You have met the Egyptians" took their
warrior for one of their own (20 Oct 2026). There was no tile room for a
recoloured copy of each unit per people, so the flags are two characters
each (a 7x5 pennant on a pole), drawn by `make_flags()` in `src/sheet.c`
at boot into the tile store's row padding past the minimap (rows 20-22,
`VIEW_FLAG_ROW` in `src/view.h`; the minimap has an `#error` to keep
out), and `view_draw_flag()` copies one after `unit_overlay()`'s tiles,
which leaves the owner in `unit_overlay_owner`: one 128-byte blit a unit
or city in view, from bank 5. A flag half off the window's edge is not
drawn.

**Before improving the AI, read what Civ 1's actually does.**
`documentation/civ1-civ2-ai-analysis-sources.txt` lists the write-ups,
the best being tupi's CivFanatics posts, which come from disassembling
the game: city production priorities, the per-continent policy that
switches every city on a landmass at once, wonders, government, taxes.
**OpenCivOne** (<https://codeberg.org/rhorvat/OpenCivOne>, formerly
`rajko-horvat/OpenCiv1` on GitHub, which is gone) is a rewrite of DOS
Civ 1 475.05 from its disassembly, so it is the original's rules as
code. Take ideas from it, not code: it is C# for a PC, and anything
here has to fit the byte-array, per-turn style of `src/ai.c` and its
own `.c` file for the eventual banking (see **Growing past 32 KB**).

**Combat is `src/combat.c`** (milestone 4), after Civilization: attack
against defence (`unit_kind_attack[]`, `unit_kind_defence[]`: warriors
1/1, settlers and builders 0/1), the attacker winning with probability
A/(A+D), in quarter points so it stays in bytes. Defence is ×1.5 on hills
and in forest,
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
`BARB_FIRST_TURN` (30; it was 15 until production came from worked
hexagons and halved), one turn in eight on average, two warriors appear
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
production box into a list of what can be built (`buildable[]`: warriors,
settlers and builders), picked like a menu; a settler chosen in a city of
size 1 waits at its cost, and the box says "waits for size 2". **The unit
list is where a unit fortified in the city is woken**, since the map does
not draw it (see **Bit 7** below): a click on a unit, or its number key
1-7, wakes it if it has orders and fortifies it if it is a warrior
without, and on the way out `open_city()` hands the orders on if the
turn was over or the active unit was fortified. **It is the map screen's own cells
rewritten**: text over the panel, the minimap and most of the window, and
in the middle the window's own full-colour cells, showing 24x10 characters
of the tile store that `view_draw_patch()` has cleared and drawn the seven
hexagons into, with the select ring on each hexagon the city works.
Under the patch a fourth box gives the size, the food box and the food a
turn ("grows in 13 turns", "not growing" or "starving"), the shields and
the trade, and how many citizens are idle. **Clicking a neighbour, or
pressing its moving key, takes its citizen off it or puts an idle one on
it** (`city_toggle()`, Civilization's way), which makes the city
`CITY_MANUAL`. It is in region 2 with Top Five Cities. So nothing is allocated for it, and on the way out
`draw_screen()` in `src/game.c` puts the whole map screen back —
`screen_clear()`, `view_init()`, `minimap_show()` (cells only; the pixels
were never touched) and the panel — and marks the view dirty, so the next
frame's redraw replaces the patch. That redraw needs no clear, for the
usual reason. The production box also shows the rate and the turns left.

**Yields and growth** (`src/growth.c`, milestone 6, in region 1) are
Civilization's. Every hexagon yields food, shields and trade by terrain:

| | hills | grass | mount. | ocean | shallows | tundra | plains | forest | desert | floodpl. |
|---|---|---|---|---|---|---|---|---|---|---|
| food | 1 | 2 | 0 | 1 | 1 | 1 | 1 | 1 | 0 | 3 |
| shields | 0 | 0 | 1 | 0 | 0 | 0 | 1 | 2 | 1 | 0 |
| trade | 0 | 0 | 0 | 2 | 2 | 0 | 0 | 0 | 0 | 1 |

plus 1 food for irrigation, and for a mine 3 shields on hills, 2 on
mountains, 1 on desert. **Half the grassland and floodplain makes a shield
as well**, on Civilization's fixed pattern — `((x*7 + y*11) & 2) == 0`,
from OpenCivOne's `TerrainMapCell.cs`, applied to array cells — which is
what gets a new city going; it is not drawn (no tile to spare), but the
hover readout gives every hexagon's yield as food/shields/trade
("Grassland 2/1/0 14,8") when nothing is on it, through `hex_yield()`. **A city of size n works its own hexagon (at least
one shield) and the n best of its six neighbours**, picked by
`city_work()` whenever they are wanted — 3×food + 2×shields + trade, or
food above all if that leaves the city short — so nothing about them is
stored, **unless the player has set them** on the city screen: then
`city_worked[]` holds them with `CITY_MANUAL`, growth adds the newcomer
to the best hexagon left and shrinking (or a settler) drops the worst,
and nothing else changes them; a captured city goes back to automatic.
Computer players never set them. Radius 1 is the city screen's patch, and cities are 4 apart, so
none ever share a hexagon; citizens beyond six work nothing and eat.
Every citizen eats 2 food and the rest goes into `city_food[]`; at
`GROWTH_BOX(size)` = (size + 1) × 10 the city grows and the box empties,
and empty and still short it shrinks, down to 1 (the panel: "Mycenae /
grew to 3", "famine"). **Without irrigation most cities stop at a size
where the food a turn is exactly 0** — plains and the sea give 1 — as in
Civilization; in a 150-turn autoplay they reached sizes 1 to 9, the
largest ringed by irrigated grassland. Trade is shown but nothing uses it
yet. `city_size[]`, `city_food[]` and `city_worked[]` are saved
(`SAVE_VERSION` 4).

**Production** (`src/city.c`) is the shields of the hexagons a city works
(`city_production()`), so a new grassland city makes 1 or 2 a turn: a
warrior (10) in five to ten turns. **A settler takes a citizen**, so it
waits at its cost until the city is size 2, and leaves it a size smaller.
Each city has `city_build` (a
`KIND_*`, warriors when founded) and `city_shields`. At the end of a turn,
after `ai_turn()`, `end_turn()` calls `city_grow()` and then
`city_produce()` on every city; at
the cost, the unit appears on the city's hexagon with its moves, so the
coming turn asks for its orders first like any other, and the surplus
carries over. The panel says "Mycenae / built Warrior" (the last city, if
several finish at once). With the unit list full the shields wait at the
cost.

**Improvements** (`src/improve.c`, milestone 6, in region 1) are bits 4
and 5 of the map byte: `MAP_IRRIGATED` or `MAP_MINED`, never both, drawn
as an overlay tile over the terrain (`MAP_OVERLAY_TILE`). A builder makes
one with `i` or `m`, or Orders → Irrigate or Mine: `improve_start()` gives
it `ORDERS_IRRIGATE` or `ORDERS_MINE`, so it asks for no orders, and
`improve_turn()`, from `end_turn()` after the computer players, counts its
turns in `unit_work[]` — 5 to irrigate, 10 to mine, Civilization's — and
sets the bits; the panel says "Irrigation / finished". Waking it (a click)
starts the count again. Irrigation goes on grassland, plains, desert,
floodplain or hills with the sea, a floodplain or irrigation next to it,
or on a floodplain; a mine on hills, mountains or desert. Mines add
shields now; **irrigation does nothing yet**, until there is food. Works
on a real MEGA65 (20 Oct 2026). **Builders on one hexagon with the same orders
work together**, as Civilization's settlers do: `improve_turn()` adds one
to every working builder's count first, then compares the sum on each
hexagon with the turns needed, and frees them all when it is done, so two
irrigate in three turns. One known gap, in TODO.txt: a builder may
improve any hexagon, however far from a city (Civilization allows it too). The
hover readout adds "+irr" or "+mine", short so that a unit's name and the
coordinates still fit its 40 columns. `map_reset()` clears them with the
rest. **Roads need a bit**, and the map byte has none left: terrain is
down to four bits (`MAP_TERRAIN_MASK` 0x0f, ten of sixteen values used).

**Resources** (milestone 7, vision.md §2) are nine, `RES_*` in
`src/map.h`, tiles 18-26 in bank 1, in this order:

| | where | adds |
|---|---|---|
| grain | floodplain, grassland | 2 food |
| fish | shallows | 2 food |
| cattle | grassland, plains, hills | 1 food, 1 shield |
| olives | hills | 1 food, 2 trade |
| wine | hills, plains | 3 trade |
| cedar | forest | 2 shields |
| copper | hills, mountains | 2 shields |
| tin | hills, mountains; and hills, mountains, tundra in the 3 rows at the top and bottom | 1 shield |
| purple dye | shallows | 3 trade |

`mapgen.py` places them (step 7): four tin nodes at the edges of the
world, then the rest by `RESOURCE_TERRAIN`'s chances (tin among them), **never two next to
each other**, about 80-100 a map. They never change, and add to the yield
in `yield()` in `src/growth.c` like an improvement, so the computer's
`city_work()` picks them up without being told; `find_site()` in
`src/ai.c` counts a resource in reach as half a hexagon of walking
(copper and tin a whole one).
Copper and tin are also bronze (see **Bronze**). **They are a list, not a layer**: `map_res_first[hy]` is where
hexagon row `hy`'s entries start in `map_res_ax[]`/`map_res_kind[]`, about
550 bytes in `HIGH_BSS`, saved with the game (`SAVE_VERSION` 8), and
`map_resource_at()` searches a row's two or three. A nibble a hexagon was
tried first and its test in `view_redraw()`'s inner loop cost 0.8 ms of
the frame whatever was in view; with the list, `draw_overlays()` draws a
hexagon row's resources a row behind the terrain, before its cities and
units, which is safe since the art is inside the hexagon (lines 8-23).
Measured (CIA2 round `view_redraw()`, interrupts off, Show Map, the
densest window on seed 1 with nine resources): 16.5 ms against 15.4 ms
with the list empty, about 120 µs a resource. The hover readout says
"Hills+Copper 0/3/0"; with a city or unit there, the resource replaces
the terrain to keep within 40 columns ("Canaanite Builder  Cattle+irr").
The names are a text, `list_resources.txt`. The map loader moved to
region 0 with the list (`src/mapload.c`), which more than paid for it.

**Bronze** (milestone 7b): **a player has a resource when it is on one of
its cities, or mined and next to one** (`player_has()` in
`src/growth.c`, region 1, worked out when asked and not stored), and
copper and tin together are bronze (`player_bronze()`), which the
**spearman** needs: `KIND_SPEARMAN`, Civilization's phalanx, attack 1,
defence 2, cost 20, `unit_spearman.png` from `alltiles/`. A builder may
mine copper and tin on any terrain (tin is often on tundra), and a
**mined copper or tin hexagon is drawn with its own tile**,
`alltiles/`' `tile_copper_improved` and `tile_iron_improved`, instead of
the resource (`RES_TILE()` in `src/map.h`; the mine overlay is still
drawn under it). Those are lines 2-29, all four character rows, which
costs the redraw two more jobs than lines 8-23, on the few hexagons that
have one. The city screen's CHANGE lists Spearman only with bronze, and
otherwise says which is missing ("Spearman needs bronze: tin"). **The
computer players**: any builder goes first for unmined copper or tin next
to any of its people's cities, the nearest (`strategic_site()`; before
that a builder only worked round its own nearest city, and copper next to
a city without one stayed unmined), and `improve_best()` mines them
before irrigating; `find_site()` counts copper and tin in reach double;
with bronze a city's new guards are spearmen, a city guarded only by
warriors builds one spearman even over the unit cap, and once it is in,
`retire()` disbands the fortified warriors beyond the guards wanted.
**Tin is scarce by design**: four nodes at the top and bottom rows (the
imports from the edge of the world, vision.md), where no city is founded,
and inland on hills and mountains about two thirds as often as copper,
5-12 on a map; most peoples will need trade (milestone 9). In 200-turn
autoplays the Canaanites had bronze on seed 1 (nine spearmen by turn
200) and nobody on seeds 3 and 7. Without the inland tin nobody ever
did. Not checked: a city building a
spearman when the player loses bronze still finishes it, since
`city_produce()` is resident and has no room for the test.

**Contact** (`src/diplomacy.c`, region 2): **a people is met when one of
its units or cities is next to one of the player's**, as in
Civilization. Until 20 Oct 2026 it was anywhere on a hexagon the player
had explored, and that greeted peoples whose warrior was four hexagons
from anything of the player's — confusing, and not how any Civ game
does it. `diplomacy_contact()` looks at the six neighbours of each of the
player's units and cities, only those with `MAP_OCCUPIED` set, after
each of the player's moves and at the end of every turn (a `contact`
flag in `game.c`; during Play 50 Turns only at the end of it). It returns
the peoples newly met as a bitmask and leaves in `diplomacy_ax/ay` where
one is; `show_met()` in `game.c` moves the view to put that hexagon in
window row 1, above the popup, so that the player's own unit next to it
is in view as well (or row 17, below the popup, near the bottom of the
map), and after the next redraw `diplomacy_greet()` greets each new
people with a popup: "You have met the Hittites. Their ruler is Suppiluliuma,
Great King of Hatti." `diplomacy_met`, a bit per player, is saved
(`SAVE_VERSION` 5) and never cleared during a game. **Advisors →
Diplomacy** lists every people — the player's own first, then for each
met one its ruler and title, at war or at peace (or "no more"), and its
cities, citizens and units; the rest "not yet met". It is milestone 8's
starting point.

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
that puts a unit or city on a hexagon or takes one off calls it. **A unit
fortified in a city is not drawn** (`unit_overlay()`), so that cities can
be told apart on the map; another unit there is, and the active one
always.

**Bit 6 means "the player has seen this hexagon"** (`MAP_EXPLORED`;
terrain is the low four bits, `MAP_TERRAIN_MASK`; bits 4 and 5 are the
improvement, see **Improvements**). `map_explore()` sets it on
a hexagon and its six neighbours whenever a human unit is created or moves,
and nothing clears it. An unexplored hexagon is **drawn** as
`TILE_UNDISCOVERED`, a solid black hexagon, never skipped — the redraw does
not clear, so a skipped one would show the previous frame through — and
anything on it is not drawn. The black tile's outline is a little inside
the terrain tiles', but the intersection of all seven outlines still tiles
the plane (checked by simulation), so any mixture covers the window. The
hover readout says "Unexplored" there and hides city and terrain.

**The minimap** (`src/minimap.c`) is the whole world in a frame at the top
of the panel: 16x5 full-colour characters with definitions of their own — all
in the tile store's row padding — a 128x40-pixel framebuffer in which hexagon `(hx, hy)` is pixels
`hx` and `hx + 1` of line `hy` — two pixels by one line, square on the
glass, and the odd rows offset by a pixel as the tiles are. Pixels are
palette entries: black for unexplored, cities and units in their player's
system colour, and terrain `t` in entry `16 + t`, a flat colour per
terrain that `tools/mktiles.py` writes there (`TERRAIN_COLOURS`) ahead of
the tiles' own, since the system palette has too few colours left over
from the players' for ten terrains. It is drawn in full once, by `minimap_init()`, and after that one
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
in it, so a unit fortified in a city is woken from the city screen's unit
list.
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
city with a settler; `i` and `m` irrigate and mine with a builder; MEGA or CTRL with a menu title's first letter (`g o a w d h`) opens it; space skips the unit this turn, `w` asks again
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

**READY is not enough: BASIC's zero page has to come back too.** Calypsi's
pseudo registers are `$02-$39`, where BASIC 65 keeps its pointers, and
until 19 Oct 2026 nothing saved them: the SDK's `preserve_zero_page.s`
does, but only when something asks for `__preserve_zp`, and `LOW_FREE`
had taken over the `zpsave` section it saves into. READY came up, and
the first `DIM` then hung, on a real MEGA65 and in xemu. Now `src/bank.s`
requires it — confirmed on the machine the same day, `DIM` and all — `zpsave` is the 56 bytes at `$1600`, and `LOW_FREE` is a
section of its own after it (`sevenships.scm`). **Test a quit with a
`DIM` at READY**, not by looking at READY: headlessly, a throwaway build
can put `DIMB(999)` and a Return into the KERNAL's keyboard buffer
(`$02B0`, count at `$D0`, ten characters at most) just before `main()`
returns, and the screenshot shows whether a second READY follows it.
Also seen (bank 1 survey, below): `DIM` clears BASIC's free memory in
bank 1, `$13000-$1F5FF`, to zero, and a `RUN` does something that
changes all of it as well.

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
Save Game at `{…,8,20,KEY_CLICK}` and Retire at `{…,8,28,KEY_CLICK}`, or MEGA+G is `{…,0,0,199}` (O 207, A 193, D 196,
W 215, H 200) (the
script skips `$D611`, so it cannot test the CTRL path); Debug at `{…,112,4,KEY_CLICK}` and
Show Map at `{…,116,20,KEY_CLICK}` (since 20 Oct 2026, when Advisors went
in before it; they were 52 and 56); `make NO_TITLE=1` skips all
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
the disk and compiles nothing. `text_load()`, **first of the loads
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

`res/tiles.fci` is 120x4 characters — 30 4x4 tiles side by side, in the
order `tiles/Makefile` lists the PNGs: ten terrains (hills, grass, mountain, ocean, shallow ocean, tundra, plains, forest,
desert, floodplain), then the city, a selection ring
(the tile-select cursor),
three units (settler, warrior, builder), the black hexagon of unexplored
land, the overlays, irrigation and mine, the nine resources (see
**Resources**), mined copper and tin, and the spearman (see **Bronze**);
`TILE_*` in `src/sheet.h`.
**Tiles 18 and up go in bank 1** (see **When bank 5 fills up**, step 1b):
the resources. `res_cattle`, `res_copper` and `res_tin` are `alltiles/`'
cattle, copper and iron cut to lines 8-23, `res_olives` and `res_wine` its
orange trees recoloured, and `res_grain`, `res_fish`, `res_cedar` and
`res_dye` placeholders drawn for them; `res_copper_mine`, `res_tin_mine`
and `unit_spearman` are `alltiles/`' as they are.
`overlay_mine.png` is the mine cut out of `alltiles/`' plains and tundra
hill mines — the pixels the two share and neither bare hill has — and
cropped to lines 8-23; `overlay_irrigation.png` is a placeholder drawn for
it, as `alltiles/` has no irrigation. Resources and the other improvements
of `alltiles/` can be overlays the same way: its resource tiles already
are, on transparent backgrounds. The
terrain, city and unit art is OpenCiv's, under CC BY 4.0
(`tiles/README.txt`, and **Credits** in `README.md`, which must stay in
step); `tile_select.png` is a placeholder drawn from the grass tile's
outline. **The forest is the plains with `tile_forest.png`'s trees drawn
over them at sheet-build time** — `a.png+b.png` in `tiles/Makefile` is one
tile, b composited over a by `mktiles.py` — rather than an overlay, which
would cost the redraw a fifth DMA job for every forest in view. Any other
overlay art (resources, improvements) can be baked the same way, at a tile
of sheet each.
**Only what the game uses goes into `tiles/`** — `alltiles/` is the whole
collection to pick from.

**The sheet is read off the disk** (`TILES.FCI`, by `sheet_load()`), a
character at a time, each widened and sent to bank 5 as it arrives, so none
of it is in the program. It used to be linked in through a `bin2s.py`, at 1
KB of the 32 a tile.

**`tiles/Makefile` is a separate, manual step** — run `make` in `tiles/`
after editing tile art, and check `res/tiles.fci` in. It runs
`tools/mktiles.py`, which reads the RGBA PNGs, makes every pixel with alpha
below 128 palette index 0 (transparent), writes the minimap's ten terrain
colours at entries 16-25, and quantises the rest across all tiles together
to 230 colours at entries 26-255, leaving 0-15 to the system palette the
panel text uses. The art has about 2000 colours between it, so it is
quantised; with 230 or fewer it would be exact. `tiles/tiles.png` is
left as a preview of the result. (The old route — ImageMagick `+append` then
`tools/png2fci.py` — needed a single paletted PNG with transparency at index
0, which ImageMagick gives no say over; `png2fci.py` is kept but unused.)

### Adding tiles

A new tile needs all of these, and nothing checks that they agree:

1. the PNG in `tiles/`, 32x32 pixels (4x4 characters), transparent where
   the neighbours should show through, added to `TILES` in
   `tiles/Makefile` — a terrain at the end of `TERRAIN`, anything else at
   the end. The order there *is* the tile numbering, so a new terrain
   renumbers every tile after it, in `src/sheet.h` too.
2. `make` in `tiles/`, and `res/tiles.fci` checked in.
3. `SHEET_COLUMNS` and the `TILE_*` list in `src/sheet.h` (4 columns a
   tile). `sheet_load()` refuses a sheet whose size disagrees, so a stale
   constant stops at boot with "cannot read TILES.FCI or it is not the sheet
   this build expects".
4. for terrain: the matching constants in `tools/mapgen.py`, the generator
   taught to place it, its minimap colour in `TERRAIN_COLOURS` in
   `tools/mktiles.py`, and a row each in `terrain_name[]` in `src/game.c`
   and `terrain_shields[]` in `src/city.c`, both indexed by terrain. Terrain
   values are tile numbers, so a new terrain has to come before the
   non-terrain tiles or the map layer grows a lookup table (see `map_tile`).
5. room: each tile is 2 KB expanded, and the sheet must stop at the end
   of bank 5 (an `#error` in `src/sheet.h`): from `$57000`, **18 tiles in
   bank 5 and 12 more in bank 1** (`SHEET_LOW_TILES`, `SHEET_HIGH`), all
   30 used, so **the next tile needs room first**: code region 3
   (`$17000-$18FFF`, four tiles, unused) is the obvious place. The tile store's own `#error` in `src/view.c` keeps the store
   and the sheet apart, and `src/sheet.h` has one for the total. Past 30:
   see **When bank 5 fills up**.
6. for an overlay: art inside pixel lines 8-23, or it costs the redraw
   twice as much (see **Drawing**).
