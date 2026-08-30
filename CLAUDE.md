# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Hexwar is a Civilization-style hex-grid strategy game for the **MEGA65**, written in C for the
**cc65** compiler and targeting the C64 personality (`-t c64`) while using MEGA65-only hardware
(VIC-IV, DMAgic, 28-bit address space). It runs under `xemu-xmega65`.

`basic/civ.bas` is an earlier XC=BASIC prototype kept for reference; it is not part of the build.

## Build & run

Requires `CC65_HOME` (currently `~/commodore/cc65`), `c1541` (VICE), `xemu-xmega65`, and
`python3` with the `png` module for asset conversion.

```sh
make                      # == make debug: build, make hexwar.d81, launch xemu (blocks)
make release              # optimised build (-Osir -Cl), then launch
make bin/hexwar.prg       # compile+link only, no disk image, no emulator  <- use this to check a change compiles
make hexwar.d81           # build the disk image without launching the emulator
make c64                  # build a .d64 and run under VICE x64 instead
make clean                # rm -rf bin obj d81data *.d81 *.d64
```

There are **no tests**. `make lint` runs `splint` on `src/dma.c` only (splint is not installed here).
Verification is by building and running in the emulator.

`make debug` defines `DEBUG`, which enables `printf` diagnostics and the `draw_view_frame()` helper
in `src/hexwar.c`.

Build artifacts: `obj/*.o|*.a` → `bin/hexwar.prg` → concatenated after `c65bin/c65toc64wrapper.prg`
into `bin/autoboot.c65` → written to `hexwar.d81` together with everything in `d81data/`.
The linker map lands in `./.map` (the Makefile's `$(PROGRAM)` is never set, hence the odd name).

### Assets

`assets-fci/*.png` are converted by `tools/png2fci.py` into `d81data/*.fci` (MEGA65 Full Colour
Image) as part of the disk-image rule. `tiles/Makefile` is a **separate, manual** step: it uses
ImageMagick to append the individual `tiles/tile_*.png` into a sheet and produce
`assets-fci/tiles.png`. Run it by hand after editing tile art; the top-level build does not.

## Architecture

Four layers, bottom-up:

- **`src/dma.c`** — MEGA65 DMAgic (F018B enhanced) job lists as pre-initialised structs, one per
  operation, filled in and triggered by `do_dma()`. This is the only way the code touches memory
  above 64 K. `dma_init()` must run before anything else in `main()`.
  `dma_copy_transparent()` skips source bytes equal to 0, which is how tiles are blitted with a
  transparent colour.
- **`src/graphics.c`** — VIC-IV setup and the framebuffer abstraction (see below), FCI file
  loading, palette upload, tile blitting, view scrolling, and a tiny text output API
  (`g_xy`/`g_color`/`g_putc`).
- **`src/map.c`** — the hex world: an `80 x 50` tile array plus the coordinate conversions.
- **`src/hexwar.c`** — `main()`, the keyboard/scroll loop, `fatal()`, and the jiffy benchmark
  helpers.

`src/assembler_utilities.s` holds the two things that must be assembly: `disable_rom_protection`
(hypervisor trap to free `$2xxxx`/`$3xxxx`) and `expand_fullcolor_char_x` (pixel-doubles a
64-byte full-colour char into the two chars after it, via `$0334`/`$0374`/`$03b4`).

### The framebuffer trick

There is no bitmap mode. The screen is VIC-IV **16-bit text mode with full-colour chars**: each
screen cell holds a 16-bit pointer to a 64-byte (8x8, one byte per pixel) character definition.
`setup_view()` writes, once at startup, a *unique* character number into every cell of the map
view so that the view's character definitions form a contiguous linear framebuffer at
`$40000-$5FFFF` (or `$30000-$5FFFF` for large views, which is why ROM protection is disabled).
Drawing a tile is therefore "DMA-copy 64-byte char bitmaps into the char slots the view cells
already point at" — see `draw_tile_in_view()`. `view_row_startaddress[]` caches each view row's
framebuffer address, which makes `scroll_up/down/left/right()` a handful of long DMA copies.

Screen RAM is at `$E000`, under the KERNAL. Every direct read/write of it must be wrapped in
`ENABLE_SCREEN_ACCESS` / `DISABLE_SCREEN_ACCESS` (these `sei` and flip `$01` between `$35`/`$36`).
The memory map is documented in the comment block at the top of `src/hexwar.c`; keep it current.

### Compile-time configuration

`include/hexwar.h` is the config header: `H640`/`V400` resolution, `SCREENBASE` and its
`SCREENBASE_UNDER_RAM` / `SCREENBASE_FIRST_64K` shortcuts, `COLORBASE`, and the `VIEW_LEFT/RIGHT/
TOP/BOTTOM` bounds that everything else derives from. Changing a `VIEW_*` value changes the
framebuffer layout, the scroll routines, and `map.c`'s tile counts at once.

`src/graphics.c` derives `EXPAND_TILE_X` when `H640 && !V400`: in that mode pixels are non-square,
so 4x4-char tiles from the FCI are drawn 8 chars wide by running each char through
`expand_fullcolor_char_x()`. Tile-drawing code has two variants under this `#ifdef` — change both.

Note: several `#if (64L * VIEW_WIDTH * VIEW_HEIGHT) >= 0x20000)` guards in `hexwar.h` and
`graphics.c` carry a stray closing paren. cc65 currently tolerates it and the condition is false
for the default view size; fix the expression before relying on those branches.

### Hex coordinates

`src/map.c` uses **doubled coordinates** (see redblobgames, linked in `design.md`): odd rows are
offset, `x` steps by 2 within a row. Three spaces, with explicit converters between them:

- *hex* — game logic, `Hex{x,y}` (doubled)
- *array* — storage in `map.tile[80][50]`, `x_hex2array` / `y_hex2array` (`y/2`) and back
- *screen* — character cells, `x_hex2screen` = `x*4 + 4*(y%2)`, `y_hex2screen` = `6*(y/2) + 3*(y%2)`

Tiles are 4x4 chars and overlap vertically by half, so a hex row advances 3 character rows.

## cc65 conventions used here

These are deliberate performance choices for a 6502-class target — follow them in new code:

- Use the `byte`/`word`/`dword` typedefs from `hexwar.h`, not `int`.
- Prefer the shared globals `_i`, `_j`, `_x`, `_y` for loop/temp variables; globals are far cheaper
  than stack locals.
- Locals that must persist across a call are declared `static` (see `draw_tile_in_view`) —
  cc65 puts those in fixed storage rather than on the software stack.
- `TMPBUFFER` (`$0334`, 200 bytes) is the shared scratch area for file loading and for
  `expand_fullcolor_char_x`; both cannot be in flight at once.
- `fatal()` switches back to 8-bit text mode, prints, and hangs — it is the only error path.

## Reference

`design.md` collects the hex-grid and original-Civ-1 map-generator research the map code is
being built from.
