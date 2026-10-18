# Seven Ships of the Enemy

A Civilization-style hex-grid game for the **MEGA65**, in C.

![the map view](screenshots/civ-popup.png)

## Building

Needs the [Calypsi 6502 tool chain](https://www.calypsi.cc/) (5.18 or later —
`cc6502`, `as6502`, `ln6502`), Python 3, Ruby (for the bundled
`tools/diskutil.rb`), and `xemu-xmega65` to run it. Pillow only if the tile art changes.

```sh
make            # build/hexwar.prg, build/hexwar.map and build/hexwar.d81
make run        # build them and boot the disk in xemu
make release    # build the disk and copy it into release/
make clean
```

**The game is the disk, not the PRG.** `build/hexwar.d81` holds the program
(as `autoboot.c65`), the tile sheet (`tiles.fci`) and the map (`hexwar.map`,
made at build time by `tools/mapgen.py`), and the program reads the last two
from drive 8 at start-up.

The board support — the C65-mode startup, the KERNAL stubs and the linker
rules — comes from [hth313/Calypsi-6502-Commodore][bsp], which ships inside the
tool chain and is selected by `--target=mega65`. There is nothing to clone.

[bsp]: https://github.com/hth313/Calypsi-6502-Commodore

## Where things are

- `src/` — the game. documentation/techreport.md describes the layers and the display.
- `res/tiles.fci` — the tile sheet, built from `tiles/*.png` by running
  `make` in `tiles/` (`tools/mktiles.py`). Checked in.
- `alltiles/` — the full OpenCiv tile collection the art is picked from.
- `TODO.txt` — the milestones.
- `basic/` — an earlier, more complete XC=BASIC 3 version, and the screenshot
  above. Not built by anything here; kept because it is the layout this is
  aiming at.
- `documentation/techreport.md` — the technical report: program structure,
  memory allocation and the main loop.
- `design.md` — notes on hex grids and on how the original Civilization's map
  generator worked.
- `old-cc65/` — the 2022 cc65 version this replaced. Kept until the rewrite has
  everything it had.

## Status

Milestones 1-3 of `TODO.txt`: a generated, east-west wrapping map that
scrolls, under a fog of war; a minimap; a mouse or joystick pointer; a turn
loop; a settler that founds a city; a city screen (double-click a city)
where it is told to build warriors or builders, which it then produces; a
menu bar with Game/Retire and Orders/Show Map; and three computer players
that found cities, guard them, build and explore. There is no combat yet.
Controls are listed along the bottom of the screen: `0 -` / `p *` / `: =`
move a unit north-west and north-east, west and east, south-west and
south-east, and the cursor keys scroll. Runs in xemu and on a real MEGA65,
with the mouse and the joystick, and quits back to BASIC's own screen.

## Credits

Most of the terrain, city and unit tiles are from [OpenCiv](https://github.com/RyanGrieb/OpenCiv)
by Ryan Grieb, with art commissioned for it from lucasyoung988, brysia and
pratamacam. They are licensed under
[CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) (see OpenCiv's
[LICENSE](https://github.com/RyanGrieb/OpenCiv/blob/master/LICENSE) and
[issue #89](https://github.com/RyanGrieb/OpenCiv/issues/89)).

They have been modified for this game: scaled to 32×32 pixels, reduced to a
shared 240-colour palette, and widened at run time for the MEGA65's 640×200
display.

Additional graphics and tiles were created by myself for this game.

`tools/diskutil.rb`, which builds the disk image, was written by Fredrik
Ramsberg. Its sector interleave makes the disk load about five times faster
from a real floppy on a MEGA65.

## Licensing

- **Source code:** [MIT License](LICENSE).
- **Game assets:** [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/).
  Art derived from OpenCiv remains under its original CC BY 4.0 terms; see
  Credits.
