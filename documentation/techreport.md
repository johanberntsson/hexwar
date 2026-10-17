<!-- pandoc techreport.md -o techreport.pdf -->

# Program and Data Structures

## Overview

Hexwar is a turn-based strategy game on a hexagonal map for the MEGA65, written
in C for the Calypsi 6502 tool chain. It runs in the MEGA65's native C65 mode,
with the C65 ROM left in place, and returns to BASIC when the player quits. Its
main parts are:

- The display engine
This draws the map. The screen is the VIC-IV's 16-bit text mode with full
colour characters, and every character cell of the map window has a 64-byte
definition of its own, so the window behaves as a framebuffer that tiles are
copied into by DMA. Moving the view redraws the whole window, synchronised to
the raster beam.

- The map
A 128x32 hexagon map, wrapping east to west, made at build time by a Python
generator and read from disk at start-up. One byte per hexagon holds the
terrain.

- Input
The keyboard, a 1351 mouse and a joystick in either port. The mouse and the
joystick both move a pointer drawn with two hardware sprites.

- The game
Units, cities and the turn loop: each of the player's units is asked for
orders in turn, then the computer player moves, then the year advances.

The design document, `vision.md`, describes the game the program is aiming
at. This report describes the program as it is. Where the two differ, this
report and `CLAUDE.md` describe the code; `vision.md` describes the plan.

## Source files

These source files are located in the src directory. The table also shows the
most important routines included in each source file.

| **File/Routine** |  **Comment** |
| -- |  ---- |
| hexwar.c | the main routine |
| &nbsp;&nbsp;&nbsp;_main_ | where execution of Hexwar begins |
|  |  |
| title.c | the title screen and the new-game screens |
| &nbsp;&nbsp;&nbsp;_title\_run_ | the title menu, then the people and the difficulty |
| &nbsp;&nbsp;&nbsp;_menu\_run_ | one menu, picked by pointer or keyboard, once round per frame |
|  |  |
| game.c | the turn loop, the player's orders and the panel |
| &nbsp;&nbsp;&nbsp;_game\_init_ | sets up a new game with one settler |
| &nbsp;&nbsp;&nbsp;_game\_run_ | the main loop, once round per frame |
| &nbsp;&nbsp;&nbsp;_end\_turn_ | runs the computer player and starts the next turn |
|  |  |
| unit.c | units: create, move, remove, and which one wants orders |
| &nbsp;&nbsp;&nbsp;_unit\_next_ | the next unit that needs orders this turn |
| &nbsp;&nbsp;&nbsp;_unit\_mark_ | keeps the map's "occupied" bit up to date |
| &nbsp;&nbsp;&nbsp;_unit\_overlay_ | what is drawn over the terrain of an occupied hexagon |
|  |  |
| city.c | cities and their names |
| &nbsp;&nbsp;&nbsp;_city\_found_ | founds a city |
|  |  |
| civ.c | the peoples, their leaders and city names, and the difficulty levels |
| &nbsp;&nbsp;&nbsp;_civ\_choose_ | which people the human and the computer lead |
|  |  |
| popup.c | boxes of text over the map, closed with a click or a key |
| &nbsp;&nbsp;&nbsp;_popup\_show_ | word-wraps and shows the text, waits, and puts the map back |
|  |  |
| input.c | keyboard, mouse, joystick and the pointer sprites |
| &nbsp;&nbsp;&nbsp;_input\_poll_ | one frame's input, returned as a key code |
|  |  |
| map.c | the map and hexagon geometry |
| &nbsp;&nbsp;&nbsp;_map\_load_ | reads the terrain from HEXWAR.MAP |
| &nbsp;&nbsp;&nbsp;_map\_cell\_at_ | which hexagon is under a character of the map |
| &nbsp;&nbsp;&nbsp;_map\_neighbour_ | one step in one of the six directions |
|  |  |
| view.c | the map window |
| &nbsp;&nbsp;&nbsp;_view\_redraw_ | draws every tile that touches the window |
| &nbsp;&nbsp;&nbsp;_view\_wait\_for\_redraw_ | waits for the beam to leave the window |
|  |  |
| sheet.c | the tile sheet |
| &nbsp;&nbsp;&nbsp;_sheet\_load_ | reads TILES.FCI, sets the palette and builds the expanded sheet |
|  |  |
| screen.c | the VIC-IV display mode, the palette and text output |
|  |  |
| mega65.c | I/O enable and the DMA routines |
| &nbsp;&nbsp;&nbsp;_blit_ | the transparent copy the map is drawn with |
|  |  |

The tools directory holds the build-time programs: `mapgen.py` generates the
map, `mktiles.py` turns the tile PNGs in the tiles directory into the tile
sheet, `res/tiles.fci`, and `diskutil.rb` (in Ruby, by Fredrik Ramsberg)
writes the disk image with a sector interleave suited to the MEGA65's floppy
drive.

## Startup

The build produces a disk image, `hexwar.d81`, holding three files:

| **File** |  **Contents** |
| -- |  ---- |
| AUTOBOOT.C65 | the program, a C65-mode PRG loaded at \$2001 |
| TILES.FCI | the tile sheet: a palette and 11 tiles of 4x4 full colour characters |
| HEXWAR.MAP | the map: a 16-byte header and one 4096-byte layer of terrain |

The C65 ROM boots AUTOBOOT.C65 automatically. `main()` then:

1. switches the VIC-IV to 80x25 16-bit text mode with full colour characters
   (`screen_init`) and puts up the title screen (`title_show`);
2. reads TILES.FCI, a character at a time. Each character is widened to twice
   its width, because a pixel of this screen mode is half as wide as it is
   tall, and copied into the expanded sheet in bank 5 (`sheet_load`);
3. reads HEXWAR.MAP into `map_tile` (`map_load`);
4. sets up the pointer sprites (`input_init`) and runs the title menu
   (`title_run`): Start Game, Load Saved Game (not implemented yet) or Quit.
   Start Game then asks for a people (Mycenaeans, Hittites, Egyptians or
   Canaanites) and one of five difficulty levels. Each menu has a
   once-a-frame loop of its own, like the main loop's, so that the mouse and
   joystick can pick an item;
5. clears the screen, writes the map window's character numbers into screen
   RAM, once (`view_init`), and places the player's settler (`game_init`);
6. clears the tile store and enters the main loop (`game_run`).

All disk access is finished by the end of step 3. The KERNAL stays available
throughout, because nothing the program does banks out the ROM.

When the player quits, from the title menu or from the game, `main()` turns
the sprites off, puts the screen back
into the 40-column text mode, and returns to BASIC.

# Memory

## The 16-bit address space

The program, its data, the BSS and the C stack all have to fit in
\$2001-\$9fff, the 32 KB that ordinary C code can address directly. This is the
scarce resource: there is plenty of RAM elsewhere, but code cannot run from
it without banking. The figures below are from the linker's listing,
`build/hexwar.lst`, for the build of 17 October 2026.

| **Address range** | **KB** |  **Usage** |
| -- |  - | ---- |
| \$0002-\$0039 | 0.1 | Calypsi's pseudo registers, in zero page |
| \$1600-\$1790 | 0.4 | the popup's text (the `zpsave` section, `LOW_FREE`) |
| \$1791-\$1eff | 1.9 | free |
| \$2001-\$298a | 2.4 | BASIC stub, startup code, constant data and text |
| \$298b-\$6836 | 15.7 | code |
| \$6837-\$6a36 | 0.5 | C stack |
| \$6a37-\$8097 | 5.6 | BSS: the map, units, cities, buffers |
| \$8098-\$9fff | 7.9 | free |

The PRG is everything up to \$6836; the C stack and the BSS are not stored in
the file, only reserved after it.

The largest item in the BSS is the terrain layer, `map_tile`, at 4096 bytes.
The units take 384 bytes (six one-byte fields for each of 64 units) and the
cities 128 (four fields for each of 32). Both are stored as one array per
field rather than as an array of structs, because the 6502 reads `unit_ax[u]`
with a single indexed instruction.

\$1600-\$1eff can hold variables that are marked `LOW_FREE`, but
nothing that has to survive a disk access may be put there, since the C65 ROM
keeps disk buffers below \$2001. The popup's 400-byte text buffer is there
because a popup's text is put together and shown with no disk access in
between.

When the free space runs low, the order of things to try is written down in
`CLAUDE.md` ("Growing past 32 KB"): move data out with `__far` first, then
bank BASIC out for BSS at \$a000-\$bfff, and only then split the code into
MAP-banked regions.

## Chip RAM and the rest of the machine

| **Address range** | **KB** |  **Usage** |
| -- |  - | ---- |
| \$12000-\$12f9f | 4 | Screen RAM, 80x25 cells of two bytes |
| \$20000-\$3ffff | 128 | The C65 ROM, left in place |
| \$40000-\$56fff | 92 | The tile store: the map window's character definitions |
| \$57000-\$5c7ff | 22 | The expanded tile sheet |
| \$5c800-\$5ffff | 14 | free |
| \$08000000- | 8192 | Attic RAM, free |
| \$0ff81000-\$0ff81f9f | 4 | Colour RAM |

**The tile store** is the heart of the display. A screen cell in this mode is a
16-bit character number, and a character number of 256 or more names a full
colour character: 64 bytes, one palette index per pixel, at the address
*number* times 64. `view_init` gives every cell of the 60x23 map window a
different number, so the window's characters lie one after another from
\$40000, and drawing a tile is copying its bytes there. The screen RAM of the
window is never written again.

Each window row is 60 characters, 3840 bytes, but rows are spaced 4096 bytes
apart. With that spacing 16 rows fill a 64 KB bank exactly, no row crosses a
bank boundary, and the bank and offset of a row are a shift and a mask of the
row number, so the drawing code never takes a 32-bit address apart. The
padding costs 5888 bytes; together with a DMA routine that sets the unchanging
fields once per redraw (`blit_begin`), it took the redraw from 32.2 ms to
12.8 ms.

**The tile store fitting below \$60000 is what keeps the ROM in.** At 60x23
the store is 94208 bytes, which leaves banks 2 and 3, the ROM, alone. A larger
window would need the ROM banked out, with everything that brings with it;
`src/view.h` stops the build if the store outgrows \$60000.

**The expanded sheet** is the tile art as it is drawn: 11 tiles of 8x4
characters, 2 KB each. It is in bank 5 rather than in Attic RAM because the
redraw copies about 160 KB out of it, and DMA reading Attic RAM is about seven
times slower than reading chip RAM on the real machine. All of it lies in one
bank, so a source address is 16 bits. There is room for 18 tiles before the
bank is full.

**Colour RAM** is moved from its default \$0ff80000 to \$0ff81000, because the
disk operating system keeps buffers at the default address and corrupts the
display.

**The pointer** is two monochrome sprites, a white arrow over a black outline
of the same shape grown by a pixel, 11 sprite pixels tall. Their shapes and
the sprite pointer list are in the BSS, placed by hand on 64-byte boundaries:
a sprite pointer can only name a 64-byte block, and the VIC-IV ignores the low
six bits of the list's address too.

## The map in memory

The map is 128 columns by 32 rows of array cells, one byte each. In the
"doubled" coordinates that the game uses, array cell (ax, ay) is the hexagon
(ax, 2·ay + (ax and 1)): odd columns are half a hexagon lower than even ones.
That packs 4096 hexagons, 64 across by 64 down, into a 4 KB rectangle.

Bits 0-6 of each byte are the terrain, which is also its tile number. **Bit 7
says that a city or a unit is on the hexagon.** The redraw checks that bit to
find the few hexagons that need something drawn over the terrain, which
avoids both a second 4 KB layer and a search of the unit list for every tile.
`unit_mark` sets or clears the bit whenever a unit or a city arrives or leaves.

# The Main Game Loop

## One loop, once a frame

The whole game runs in one loop, `game_run` in `src/game.c`, which goes round
exactly once per video frame:

1. Wait until the raster beam has just passed the bottom of the map window
   (line 243).
2. If anything on the map has changed, redraw the window.
3. Read one frame's input. The mouse and joystick move the pointer as a side
   effect; a key press or a click is returned as a key code.
4. If the pointer has moved onto a different hexagon, update the readout at
   the top right of the screen.
5. Act on the key code, if there is one.

There is no other place where the program waits for input. The mouse and the
joystick have to be read every frame, whatever the game is waiting for, and a
single loop makes that automatic. It also gives the input code a clock: the
KERNAL's jiffy clock does not advance in this program, so the joystick's
acceleration and the click debounce count frames, that is, trips round this
loop.

What the game is waiting for is held in one variable, `unit_active`. It is
either the unit that wants orders, which is drawn with a yellow ring, or none,
in which case the panel asks the player to end the turn.

## Redrawing in step with the beam

A PAL frame is 19.97 ms. The redraw takes about 13.6 ms (measured in xemu,
with a settler and with a city in view) and there is no second buffer to draw
into, so it has to be timed so that the beam never displays a row that is
being redrawn. This works in three ways:

- The redraw starts at line 243, as the beam leaves the window, which gives it
  the lower border and the vertical blank as a head start.
- It draws strictly from the top of the window to the bottom. Hexagons are
  drawn by hexagon row, which makes their screen position increase
  monotonically, and that is also the right order for overlapping tiles: a
  lower hexagon's top row has to be drawn over the bottom row of the one
  above.
- It never clears the window first. A clear would blank rows the beam had not
  reached yet. The hexagons cover the whole window between them, as long as
  the view stays one hexagon row inside the map at the top and bottom, and
  both scrolling and centring hold it there.

Measured, the redraw ends at about line 143 of the next frame. The beam needs
row *r* of the window at line 128 + 8*r*, so the redraw keeps about 100
lines ahead of it.

Cities, the selection ring and units are drawn over the terrain, **one
hexagon row behind it**. A unit's feet are in the bottom character row of its
tile, which is also the top row of the hexagons below, and those are opaque in
the middle. So the occupied hexagons of a row are noted while the row's
terrain is drawn, and their overlays are drawn after the next row's terrain.

## A turn

| **Step** |  **What happens** |
| -- |  ---- |
| Orders | each of the player's units that has moves left and no standing orders is selected in turn (`unit_next`) and brought into view |
| End of turn | with no unit left, the panel shows "End of turn"; Return, or a click on the panel, goes on |
| Computer player | `ai_turn`, which for now only gives the computer's units their moves back |
| New turn | the turn number goes up, the year moves on ten years from 1600 BC, and every unit gets its moves back |

The game is over when the player has neither units nor cities.

The orders a unit can be given are:

| **Key** |  **Order** |
| -- |  ---- |
| q, e, a, d, z, c | move north-west, north-east, west, east, south-west, south-east |
| b | found a city (settlers only); the settler becomes the city |
| space | no orders this turn |
| w | wait: ask again after the other units |
| f | fortify: no orders until woken |

A click on a hexagon next to the selected unit moves the unit there, and a
click on one of the player's own units selects that unit, waking it if it was
fortified. A click anywhere else on the map centres the view on that hexagon.
The cursor keys scroll the view, and ESC or RUN/STOP quits.
