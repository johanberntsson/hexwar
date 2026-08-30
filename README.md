# Hexwar

A Civilization-style hex-grid game for the **MEGA65**, in C.

![the map view](basic/civ-basic.png)

## Building

Needs the [Calypsi 6502 tool chain](https://www.calypsi.cc/) (5.18 or later —
`cc6502`, `as6502`, `ln6502`), Python 3, and `xemu-xmega65` to run it. `c1541`
(from VICE) is needed only by `make d81`.

```sh
make            # build/hexwar.prg
make run        # build it and boot it in xemu
make d81        # build/hexwar.d81, for a real MEGA65
make clean
```

Nothing is loaded at run time: the tile sheet (`res/tiles.fci`) is linked into
the program, so the PRG is the whole game.

The board support — the C65-mode startup, the KERNAL stubs and the linker
rules — comes from [hth313/Calypsi-6502-Commodore][bsp], which ships inside the
tool chain and is selected by `--target=mega65`. There is nothing to clone.

[bsp]: https://github.com/hth313/Calypsi-6502-Commodore

## Where things are

- `src/` — the game. `CLAUDE.md` describes the layers and the display.
- `res/tiles.fci` — the tile sheet, built from `tiles/*.png` by
  `tiles/Makefile` and `tools/png2fci.py`.
- `basic/` — an earlier, more complete XC=BASIC 3 version, and the screenshot
  above. Not built by anything here; kept because it is the layout this is
  aiming at.
- `design.md` — notes on hex grids and on how the original Civilization's map
  generator worked.
- `old-cc65/` — the 2022 cc65 version this replaced. Kept until the rewrite has
  everything it had.

## Status

The display, the map and scrolling work. There is no game yet: the map is
placeholder terrain from `map_init()`, and the panel is static text.
