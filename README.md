# Seven Ships of the Enemy

A Civilization-style hex-grid game for the **MEGA65**, in C.

![the map view](screenshots/civ-popup.png)

## Building

Needs the [Calypsi 6502 tool chain](https://www.calypsi.cc/) (5.18 or later —
`cc6502`, `as6502`, `ln6502`), Python 3, Ruby (for the bundled
`tools/diskutil.rb`), and `xemu-xmega65` to run it. Pillow only if the tile art
or the pictures change.

```sh
make            # build/sevenships.prg, build/sevenships.map and build/sevenships.d81
make run        # build them and boot the disk in xemu
make release    # build the disk and copy it into release/
make clean
```

**The game is the disk, not the PRG.** `build/sevenships.d81` holds the program
(as `autoboot.c65`), the texts (`text.pak`), six regions of code that run
from bank 1 by way of attic RAM (`sevenships.r1` to `.r6`), the title
picture and the end pictures (`*.pic`), the tile sheet (`tiles.fci`) and
the map (`sevenships.map`, made at build time by `tools/mapgen.py`), and the
program reads them from drive 8 — all but the end pictures at start-up.

The board support — the C65-mode startup, the KERNAL stubs and the linker
rules — comes from [hth313/Calypsi-6502-Commodore][bsp], which ships inside the
tool chain and is selected by `--target=mega65`. There is nothing to clone.

[bsp]: https://github.com/hth313/Calypsi-6502-Commodore

## Where things are

- `src/` — the game. documentation/techreport.md describes the layers and the display.
- `res/tiles.fci` — the tile sheet, built from `tiles/*.png` by running
  `make` in `tiles/` (`tools/mktiles.py`). Checked in.
- `res/*.pic` — the title and end pictures, built from `screens/*.png` by
  running `make` in `screens/` (`tools/mkpic.py`). Checked in.
- `documentation/techreport.md` — the technical report: program structure,
  memory allocation, the main loop and how combat works.
- `res/text/` — the game's texts: the title, help and new-game screens, the
  opening popup, the letters, the Crisis, the Civilopedia's pages, and the
  lists of names. `tools/mktext.py` packs them into
  `TEXT.PAK` on the disk, and the game reads them into attic RAM at start-up.
- `res/xemu-keymap.cfg` — gives xemu's keypad to the game; `make run` loads it.
- `documentation/design.md` — notes on hex grids and on how the original Civilization's map
  generator worked.
- `documentation/vision.md` — the game design: a Late Bronze Age eastern
  Mediterranean 4X, ending in the Bronze Age Collapse.
- `TODO.txt` — the milestones, what is done, and what is next.

## Status

Milestones 1-10 of `TODO.txt` are in, in a first form: a whole game from
1600 BC to the Bronze Age Collapse and its end in 1026 BC.

- **The world**: a generated map of the eastern Mediterranean's kind that
  wraps east-west and scrolls, under a fog of war, with a minimap; ten
  terrains, nine resources (grain, fish, cattle, olives, wine, cedar,
  copper, tin and purple dye), and a title picture at start-up.
- **Four peoples** — Mycenaeans, Hittites, Egyptians and Canaanites — the
  human one of them and the computer the others, at five difficulty
  levels.
- **Cities** that grow on their food, work the hexagons round them (by
  hand on the city screen, or by themselves), and build settlers,
  warriors, builders, spearmen, galleys, fishing boats, merchant ships,
  temples and the Lion Gate; builders irrigate and mine; copper and tin
  together make bronze, which the spearman needs.
- **Combat** after Civilization's rules, barbarians, and a people that
  loses its capital breaking up into rebel cities.
- **Diplomacy** in the voice of the Amarna letters: contact, war,
  ceasefire and peace, attitudes, and letters from the other rulers.
- **Trade**: goods exchanged by letter, kept alive by merchant ships (or
  overland between neighbours), emporia on foreign coasts, and a treasury
  of the cities' trade that buys production and pays for temples.
- **Ships**: galleys that ferry settlers and armies, fishing boats that
  explore the coasts and work the fish, merchant ships.
- **The Crisis** from 1200 BC: omens, the Sea Peoples' raiding fleets,
  drought, earthquake, plague and revolt, and pleas from rulers losing
  their cities.
- **The end**: a score of cities, citizens, goods, deals, gold and legacy,
  and survival, legacy or hegemony victory — or defeat — each with its
  picture.
- **Advisors**: Cities, Diplomacy and Finance; World: Top Five Cities and
  the Score; Help: the keys, and the Civilopedia's pages on terrains,
  units and buildings.
- Saving and loading; mouse, joystick or keyboard alone (Tab puts a
  cursor on the map, the cursor keys move it and Return clicks; MEGA and
  a menu title's first letter opens it, Alt in xemu). Development builds
  have a Debug menu (Show Map, Play 50 Turns) which `make release` leaves
  out.

HELP (F1 in xemu) shows a screen of every key and click. `0 -` / `p *` /
`: =` move a unit north-west and north-east, west and east, south-west and
south-east (keypad 7 9 / 4 6 / 1 3 in xemu, with `make run`'s keymap), and
the cursor keys scroll. Esc asks before quitting, and the game quits back
to BASIC's own screen. It is played and tested on a real MEGA65 as it goes.
Much of the balance — the Crisis above all — is still to be tuned from
play, and the Civilopedia's pages are placeholders.

## Credits

Most of the terrain, city, unit and resource tiles are from [OpenCiv](https://github.com/RyanGrieb/OpenCiv)
by Ryan Grieb, with art commissioned for it from lucasyoung988, brysia and
pratamacam. They are licensed under
[CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) (see OpenCiv's
[LICENSE](https://github.com/RyanGrieb/OpenCiv/blob/master/LICENSE) and
[issue #89](https://github.com/RyanGrieb/OpenCiv/issues/89)).

They have been modified for this game: scaled to 32×32 pixels, reduced to a
shared 240-colour palette, and widened at run time for the MEGA65's 640×200
display. The cattle, copper and tin (OpenCiv's iron) resources are cut down
to fit the middle of a hexagon, and the olives and wine, and their improved
groves, are OpenCiv's orange trees and orchard recoloured. The Sea
Peoples' raider is OpenCiv's swordsman, the merchant ship its cargo ship,
and the emporium its bazaar, scaled down to the middle of a hexagon.

Additional graphics and tiles were created by myself for this game.

`tools/diskutil.rb`, which builds the disk image, was written by Fredrik
Ramsberg. Its sector interleave makes the disk load about five times faster
from a real floppy on a MEGA65.

## Licensing

- **Source code:** [MIT License](LICENSE).
- **Game assets:** [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/).
  Art derived from OpenCiv remains under its original CC BY 4.0 terms; see
  Credits.
