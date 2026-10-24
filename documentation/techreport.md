<!-- pandoc techreport.md -o techreport.pdf -->

# Program and Data Structures

## Overview

Seven Ships of the Enemy is a turn-based strategy game on a hexagonal map for the MEGA65, written
in C for the Calypsi 6502 tool chain. It runs in the MEGA65's native C65 mode,
with the C65 KERNAL left in place (only BASIC is banked out, for the RAM
under it), and returns to BASIC when the player quits. Its
main parts are:

- The display engine
This draws the map. The screen is the VIC-IV's 16-bit text mode with full
colour characters, and every character cell of the map window has a 64-byte
definition of its own, so the window behaves as a framebuffer that tiles are
copied into by DMA. Moving the view redraws the whole window, synchronised to
the raster beam.

- The map
A 128x20 hexagon map, wrapping east to west, made at build time by a Python
generator and read from disk at start-up. One byte per hexagon holds the
terrain, whether the player has explored it, and whether anything is on it.
A minimap in the panel shows all of it that has been explored.

- Input
The keyboard, a 1351 mouse and a joystick in either port. The mouse and the
joystick both move a pointer drawn with two hardware sprites.

- The game
Units, cities, their production and the turn loop: each of the player's
units is asked for orders in turn, then the three computer players move,
then the peoples' diplomacy is settled, then every city produces, then the
year advances. War, ceasefire and peace come and go by letter, in the
formulas of the Amarna letters, and so do the goods the peoples
exchange. A city screen and a menu
bar run their own loops over the same screen.

The design document, `vision.md`, describes the game the program is aiming
at. This report describes the program as it is. Where the two differ, this
report and `CLAUDE.md` describe the code; `vision.md` describes the plan.

## Source files

These source files are located in the src directory. The table also shows the
most important routines included in each source file.

| **File/Routine** |  **Comment** |
| -- |  ---- |
| sevenships.c | the main routine |
| &nbsp;&nbsp;&nbsp;_main_ | where execution of the game begins |
|  |  |
| bank.s | banks BASIC out of \$a000-\$bfff before `main`, and back in after |
|  |  |
| region.c | loads the code regions into Attic RAM at start-up |
| dispatch.s | the code window: copies a region into its slot and maps it in, and the stubs that call into one |
| &nbsp;&nbsp;&nbsp;_set\_region_ | every change of the window, kept in step with the KERNAL's copy |
|  |  |
| title.c | the title screen and the new-game screens |
| &nbsp;&nbsp;&nbsp;_title\_run_ | the title menu, then the people and the difficulty |
| &nbsp;&nbsp;&nbsp;_menu\_run_ | one menu, picked by pointer or keyboard, once round per frame |
|  |  |
| game.c | the turn loop, the player's orders and the panel |
| &nbsp;&nbsp;&nbsp;_game\_run_ | the main loop, once round per frame |
|  |  |
| turn.c | the end of a turn and the start of a game, out of game.c into region 0 |
| &nbsp;&nbsp;&nbsp;_game\_init_ | sets up a new game with a settler and a warrior |
| &nbsp;&nbsp;&nbsp;_game\_end\_turn_ | runs the computer players, diplomacy and the cities, and starts the next turn |
|  |  |
| unit.c | units: create, move, remove, which one wants orders, and galleys with those aboard |
| &nbsp;&nbsp;&nbsp;_unit\_next_ | the next unit that needs orders this turn |
| &nbsp;&nbsp;&nbsp;_unit\_mark_ | keeps the map's "occupied" bit up to date |
| &nbsp;&nbsp;&nbsp;_unit\_overlay_ | what is drawn over the terrain of an occupied hexagon |
|  |  |
| city.c | cities, their names and their production |
| &nbsp;&nbsp;&nbsp;_city\_found_ | founds a city |
| &nbsp;&nbsp;&nbsp;_city\_produce_ | one turn's shields, and the unit when they are enough |
|  |  |
| civ.c | the peoples, their leaders and city names, and the difficulty levels (the names themselves are texts) |
| &nbsp;&nbsp;&nbsp;_civ\_choose_ | which people the human and the computer lead |
|  |  |
| cityscreen.c | the city screen: the city and its two rings of hexagons, and its production |
| help.c | the help screen: every key and click the map screen answers to |
| text.c | the long texts: read into Attic RAM at start-up, drawn from there |
| &nbsp;&nbsp;&nbsp;_text\_show_ | draws one text, following its markup for position and colour |
| topcities.c | the Top Five Cities screen, opened from World (empty so far) |
| &nbsp;&nbsp;&nbsp;_city\_screen_ | shows a city until the player leaves it |
|  |  |
| menu.c | the menu bar along the top, and its dropdowns |
| &nbsp;&nbsp;&nbsp;_menu\_click_ | opens the dropdown under the arrow and returns what was picked |
| &nbsp;&nbsp;&nbsp;_menu\_key_ | the same for MEGA (or CTRL) and a title's first letter |
| &nbsp;&nbsp;&nbsp;_menu\_orders_ | what the Orders menu offers the active unit |
|  |  |
| ai.c | the computer players and the barbarians |
| &nbsp;&nbsp;&nbsp;_ai\_place_ | puts each one's settler on good land, well away from the others |
| &nbsp;&nbsp;&nbsp;_ai\_turn_ | one computer player's turn |
| &nbsp;&nbsp;&nbsp;_ai\_barbarians_ | the barbarians' turn: new bands, and every barbarian's move |
|  |  |
| save.c | saving and loading a game: every state array, written out as it stands |
| &nbsp;&nbsp;&nbsp;_save\_write_ | writes SEVENSHIPS.SAV, replacing the last one (Game/Save Game) |
| &nbsp;&nbsp;&nbsp;_save\_read_ | reads it back over a freshly set-up game |
|  |  |
| diplomacy.c | contact, war, ceasefire and peace, the attitudes, the letters and the Diplomacy screen |
| &nbsp;&nbsp;&nbsp;_diplomacy\_turn_ | once a turn: who has met whom, the attitudes, and the computer players' declarations and offers |
| &nbsp;&nbsp;&nbsp;_diplomacy\_greet_ | the letters to the player, in popups, the offers answered Yes or No |
|  |  |
| path.c | routes along the shallows for galleys: a breadth-first search |
| aiplace.c | where the computer players start, out of ai.c into region 0 |
| aibuild.c | what each computer player's city builds, out of ai.c into region 2 |
| emporium.c | merchant ships and the emporia they found on foreign coasts |
| picture.c | full pictures in the map window, with their own palette: the title's and the end's |
| treasury.c | each player's gold: the cities' trade less corruption, the deals, the temples' upkeep, the computer players' buying, and Advisors/Finance |
| culture.c | the temple and the Lion Gate, finished, and the legacy points they give |
| command.c | what a menu item does, out of game.c |
| cities.c | Advisors/Cities: the player's cities in a list, and a city screen from it |
|  |  |
| trade.c | goods exchanged between the peoples, as gifts or for other goods |
| &nbsp;&nbsp;&nbsp;_trade\_turn_ | once a turn: deals end with the peace, and the computer players ask for goods or tribute |
| &nbsp;&nbsp;&nbsp;_trade\_propose_ | the player's proposal on the Diplomacy screen |
|  |  |
| combat.c | combat, and whether a player is still in the game |
| &nbsp;&nbsp;&nbsp;_combat\_attack_ | one unit attacks a neighbouring hexagon |
|  |  |
| popup.c | boxes of text over the map, closed with a click or a key |
| &nbsp;&nbsp;&nbsp;_popup\_show_ | word-wraps and shows the text, waits, and puts the map back |
|  |  |
| input.c | keyboard, mouse, joystick and the pointer sprites |
| &nbsp;&nbsp;&nbsp;_input\_poll_ | one frame's input, returned as a key code |
|  |  |
| map.c | the map and hexagon geometry |
| &nbsp;&nbsp;&nbsp;_map\_load_ | reads the terrain from SEVENSHIPS.MAP |
| &nbsp;&nbsp;&nbsp;_map\_cell\_at_ | which hexagon is under a character of the map |
| &nbsp;&nbsp;&nbsp;_map\_neighbour_ | one step in one of the six directions |
|  |  |
| minimap.c | the whole map in miniature, with the view's outline |
| &nbsp;&nbsp;&nbsp;_minimap\_plot_ | redraws one hexagon of it |
| &nbsp;&nbsp;&nbsp;_minimap\_frame_ | moves the outline sprites to the view |
|  |  |
| view.c | the map window |
| &nbsp;&nbsp;&nbsp;_view\_redraw_ | draws every tile that touches the window |
| &nbsp;&nbsp;&nbsp;_view\_wait\_for\_redraw_ | waits for the beam to leave the window |
|  |  |
| sheet.c | the tile sheet |
| &nbsp;&nbsp;&nbsp;_sheet\_load_ | reads TILES.FCI, sets the palette and builds the expanded sheet |
|  |  |
| screen.c | the VIC-IV display mode, the palette and text output, and BASIC's display kept and put back |
|  |  |
| mega65.c | I/O enable and the DMA routines |
| &nbsp;&nbsp;&nbsp;_blit_ | the transparent copy the map is drawn with |
|  |  |

The tools directory holds the build-time programs: `mapgen.py` generates the
map, `mktiles.py` turns the tile PNGs in the tiles directory into the tile
sheet, `res/tiles.fci`, `mkpic.py` the pictures in the screens directory
into `res/*.pic`, and `diskutil.rb` (in Ruby, by Fredrik Ramsberg)
writes the disk image with a sector interleave suited to the MEGA65's floppy
drive.

## Startup

The build produces a disk image, `sevenships.d81` (`make release` copies it
to `release/`), holding these files:

| **File** |  **Contents** |
| -- |  ---- |
| AUTOBOOT.C65 | the program, a C65-mode PRG loaded at \$2001 |
| TILES.FCI | the tile sheet: a palette and 39 tiles of 4x4 full colour characters |
| SEVENSHIPS.MAP | the map: a 16-byte header and one 4096-byte layer of terrain |
| TEXT.PAK | the texts -- the title, help and new-game screens, the opening popup, and the lists of names -- with their markup |
| SEVENSHIPS.R1 | code region 1, the computer players and the cities' growth: a header and the code, kept in Attic RAM and run at \$6000 from bank 1 |
| SEVENSHIPS.R2 | code region 2, Top Five Cities, the builders' improvements and what the computer players' cities build, the same way |
| SEVENSHIPS.R3 | code region 3, diplomacy, the same way |
| SEVENSHIPS.R4 | code region 4, trade, the ships' routes, exploring and fishing, merchant ships and emporia, and the menus, the same way |
| SEVENSHIPS.R5 | code region 5, the Crisis and the peoples' collapse, the score and the end of the game, and the pictures, the same way |
| SEVENSHIPS.R6 | code region 6, the menus and their commands, the treasury, and buildings and wonders, the same way |
| SEVENSHIPS.R7 | code region 7, the city screen, the same way |
| INTRO.PIC, DEFEAT.PIC, WIN_SURVIVAL.PIC, WIN_HEGEMONY.PIC, WIN_LEGACY.PIC | the pictures shown at boot and at the end: a palette and 60x23 full colour characters, 87 KB each |

The C65 ROM boots AUTOBOOT.C65 automatically. `main()` then:

1. keeps the VIC registers as BASIC left them (`screen_save`), switches the
   VIC-IV to 80x25 16-bit text mode with full colour characters
   (`screen_init`), reads TEXT.PAK into Attic RAM (`text_load`) and
   SEVENSHIPS.R1 to .R7 into Attic RAM (`region_load`), and reads
   INTRO.PIC into the map window's tile store, palette first, so that the
   title picture shows while the rest loads (`title_splash`);
2. reads TILES.FCI, a character at a time. Each character is widened to twice
   its width, because a pixel of this screen mode is half as wide as it is
   tall, and each tile row, once its four characters are in, packed into
   the expanded sheet in bank 5, and bank 1 when that is full
   (`sheet_load`). A row is staged in code slot 1, not in the tile store,
   which holds the picture, and the sheet's palette goes to Attic RAM
   rather than to the VIC, for `picture_end` to put in place once the
   picture has gone;
3. reads SEVENSHIPS.MAP: the terrain into `map_tile`, and the resources,
   a list sorted by hexagon row, into `map_res_*` (`map_load`);
4. sets up the pointer sprites (`input_init`), waits for a key over the
   picture and puts the tiles' palette in place, and runs the title menu
   (`title_run`): Start Game, Load Saved Game or Quit.
   Start Game then asks for a people (Mycenaeans, Hittites, Egyptians or
   Canaanites) and one of five difficulty levels. Each menu has a
   once-a-frame loop of its own, like the main loop's, so that the mouse and
   joystick can pick an item;
5. clears the screen, writes the map window's character numbers into screen
   RAM, once (`view_init`), and places the player's settler (`game_init`) --
   or, for Load Saved Game, reads SEVENSHIPS.SAV over what `game_init` set
   up (`game_load`), going back to step 4 if there is none;
6. clears the tile store and enters the main loop (`game_run`).

Apart from saving and loading a game, and the picture before the final
score, all disk access is finished by the end of step 3. The KERNAL stays available
throughout, because the only part of the ROM the program banks out is BASIC,
at \$a000-\$bfff, and that before `main()` starts.

Game/Retire on the menu bar asks for confirmation and then goes back to
step 4 for a new game; `game_init` clears everything the old one left on the
map and in the unit and city lists. When the player quits, from the title
menu or from the game, `main()` turns
the sprites off, writes back the VIC registers `screen_save()` read before
the game touched them, so that BASIC gets its own 80-column screen and
colours, banks BASIC back in, and returns to it.

# Memory

## The 16-bit address space

The program, its data, the BSS and the C stack all have to fit in
\$2001-\$9fff, the 32 KB that ordinary C code can address directly. This is the
scarce resource: there is plenty of RAM elsewhere, but code cannot run from
it without banking -- plus the 8 KB at \$a000-\$bfff, which is RAM under the
BASIC ROM and is the program's once `src/bank.s` has banked BASIC out, before
`main()` runs. **\$6000-\$7fff of the 32 KB is a code window** (see
*Code banking* below): resident code lives on either side of it, and the
window shows one region of code at a time. The figures below are from the linker's listing,
`build/sevenships.lst`, for the build of 24 October 2026.

| **Address range** | **KB** |  **Usage** |
| -- |  - | ---- |
| \$0002-\$0039 | 0.1 | Calypsi's pseudo registers, in zero page |
| \$1600-\$1637 | 0.05 | `zpsave`: BASIC's zero page \$02-\$39, saved at start-up and put back on exit |
| \$1638-\$1b20 | 1.2 | the `lowfree` section (`LOW_FREE`): the popup's text, the scratch buffers of the view and the minimap, and the galleys' route search |
| \$1b21-\$1eff | 1.0 | free |
| \$2001-\$5ad1 | 14.7 | BASIC stub, startup and resident code |
| \$5ad2-\$5b42 | 0.1 | initialised data: the DMA lists |
| \$5b43-\$5c42 | 0.25 | C stack |
| \$5c43-\$5e1a | 0.5 | BSS: small state |
| \$5e1b-\$5fff | 0.5 | free |
| \$6000-\$7fff | 8 | the code window; region 0 as loaded (7.6 KB used) |
| \$8000-\$9fff | 8 | resident constant data and code |
| \$a000-\$bb99 | 6.9 | `highbss` (`HIGH_BSS`): the map `map_tile` (terrain, improvements, explored and occupied bits), the unit and city arrays (the cities' size, food box and worked hexagons among them), what `city_work()` found, the peoples' relations and attitudes, the trade deals and their ships' due turns, the emporia, the treasuries, the buildings and the legacy points, the fights waiting to be shown, the sprite blocks, the AI's headings, the staging buffers of the texts, the regions and the pictures, the texts' line buffers, the sheet loader's buffers and its rows' extents and addresses, the saved VIC registers, and the resources, a list by hexagon row |
| \$bb9a-\$bfff | 1.1 | free |

The long texts and the names are not in this table at all: they are in
TEXT.PAK, read into Attic RAM at start-up, which is most of why the
constant data went from 3.7 KB on 18 October to 1.8 KB.

The PRG is everything from \$2001 to \$9ffe, gaps and all. Nothing initialised may go above \$9fff,
because the PRG is loaded while BASIC is still mapped there, so only BSS
marked `HIGH_BSS` is put in the window. BASIC is banked back in as the last
thing `main()` does before it returns.

The largest item in the BSS is the terrain layer, `map_tile`, at 4096 bytes,
which is why it was the first to move up. The units take 384 bytes (six
one-byte fields for each of 64 units) and the cities 256 (six byte fields
and a word for each of 32), and they followed it. The startup does not
clear the window, so a new game empties both lists itself. Both are stored as one array per
field rather than as an array of structs, because the 6502 reads `unit_ax[u]`
with a single indexed instruction.

\$1600-\$1eff can hold variables that are marked `LOW_FREE`, but
nothing that has to survive a disk access may be put there, since the C65 ROM
keeps disk buffers below \$2001. The popup's 400-byte text buffer is there
because a popup's text is put together and shown with no disk access in
between.

When the free space runs low, the order of things to try is written down in
`CLAUDE.md` ("Growing past 32 KB"): move data out first -- more texts and
tables into Attic RAM, as the long texts already are, or with `__far` --
then more BSS into the window at \$a000-\$bfff, and only then split the
code into MAP-banked regions.

## Code banking

There is more code than resident space, so part of it runs through a
window. The 45GS02's MAP instruction can make the CPU's \$6000-\$7fff
show any 8 KB of the first megabyte, and the program keeps several
*regions* of code there, all linked to run at \$6000: region 0 is the
PRG's own \$6000-\$7fff, and regions 1 and up are read from their own
files (SEVENSHIPS.R1, ...) into Attic RAM at start-up. Code cannot run
from Attic RAM at any useful speed, so a region is copied by DMA into a
*slot* in bank 1 when it is called and the slot holds another: slot 1, at
\$13000, is region 1's alone, and slot 2, at \$15000, is shared by
regions 2 and up. That is BASIC 65's variable area, unused while the game
runs, and two slots are all there is room for: above them,
\$17000-\$1efff holds the second part of the tile sheet. A copy
of a whole region takes about 3.3 ms on the real machine, which is
nothing for code that runs once a turn or once a click, so the number of
regions is limited only by the tables in `src/dispatch.s`; what must not
happen is two regions of one slot calling each other in a loop. Region 0
holds the screens that are not the map -- title, setup, help -- with
save and load, the loaders of the tile sheet and the map, the end of a
turn and the readout of what is under the arrow;
region 1 holds the computer players and the cities' yields and growth;
region 2 Top Five Cities, the builders'
improvements and what the computer players' cities build, which calls
the improvements for every city and so is beside them; region 3
diplomacy; region 4 trade, the ships' routes, exploring and fishing,
merchant ships and emporia; region 5 the Crisis and the peoples'
collapse, the score and the end of the game, and the pictures; region 6
the menus and their commands, the treasury, and buildings and wonders;
region 7 the city screen.

Resident code calls a function in a region through a stub of the same
name (`src/dispatch.s`), which maps the region in (copying it into its
slot first if need be), makes the call, and maps the caller's region
back (copying that back in if the call displaced it), so the callers are unchanged; inside its
own file the function is renamed. The window has to be in the lower
half of the address space, since the KERNAL is mapped into the upper
half, and every change of it is also written to \$011c-\$011f, where the
C65 KERNAL keeps its copy of the map in force: the KERNAL maps back to
that after its own MAP changes, in interrupts and disk calls, and
without it the window was switched off within a frame. The redraw path
and the main loop stay resident, where a call costs nothing.

## Chip RAM and the rest of the machine

| **Address range** | **KB** |  **Usage** |
| -- |  - | ---- |
| \$12000-\$12f9f | 4 | Screen RAM, 80x25 cells of two bytes |
| \$13000-\$14fff | 8 | Code slot 1: region 1 |
| \$15000-\$16fff | 8 | Code slot 2: whichever of regions 2 and up was called last |
| \$17000-\$1efff | 32 | The expanded tile sheet, second area: the rows that did not fit bank 5 |
| \$20000-\$3ffff | 128 | The C65 ROM, left in place |
| \$40000-\$56fff | 92 | The tile store: the map window's character definitions, and in the row padding the minimap's 80 and the owners' flags |
| \$57000-\$5ffff | 36 | The expanded tile sheet, first area |
| \$08000000- | 8192 | Attic RAM: the texts from TEXT.PAK at the bottom, the tiles' palette at \$0800f000 (for after a picture), the code regions from \$08010000, 8 KB each, the rest free |
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
padding is 5888 bytes, and the minimap keeps most of its characters there; together with a DMA routine that sets the unchanging
fields once per redraw (`blit_begin`), it took the redraw from 32.2 ms to
12.8 ms.

**The tile store fitting below \$60000 is what keeps the ROM in.** At 60x23
the store is 94208 bytes, which leaves banks 2 and 3, the ROM, alone. A larger
window would need the ROM banked out, with everything that brings with it;
`src/view.h` stops the build if the store outgrows \$60000.

**The expanded sheet** is the tile art as it is drawn: 39 tiles of 8x4
characters, 2 KB each as drawn, but packed. It is in chip RAM rather
than in Attic RAM because the redraw copies about 160 KB out of it, and DMA
reading Attic RAM is about seven times slower than reading chip RAM on the
real machine. The VIC never reads it -- it is only a DMA source -- so it
need not be in bank 5: its rows fill bank 5 to its end, and the rest go
in bank 1, where four code regions would have been. While loading it, the
program notes which characters of each tile row have any pixels, keeps
only those, and records where each row went, its bank and address: a
unit or an improvement, a small picture in the middle of its tile, then
takes a fraction of a whole tile's room and costs a fraction of its
copying. The 39 tiles take 56 KB of the 68 KB the two parts hold. No
row crosses a bank, so a source address is 16 bits, and the redraw
changes the DMA's source bank only when a row is in the other part.

**The minimap** is the same trick at a smaller scale: 16x5 full-colour
characters, each with a definition of its own, all of them in the tile
store's row padding, which nothing else uses -- making a 128x40
framebuffer with a two-pixel hexagon per position. It is drawn in full once
and then a hexagon at a time as the map is explored or units move; the
outline of the view on it is two sprites, so scrolling never touches it.

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
That packs 2560 hexagons, 64 across by 40 down, into a 2.5 KB rectangle.

Bits 0-5 of each byte are the terrain, which is also its tile number. **Bit 6
says that the player has seen the hexagon**; until then the redraw draws a
black hexagon there instead, and nothing on it. **Bit 7
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
either the unit that wants orders, which blinks twice a second, or none,
in which case the panel asks the player to end the turn.

## Redrawing in step with the beam

A PAL frame is 19.97 ms. The redraw takes about 13.6 ms (measured in xemu,
with a settler and with a city in view; 16 ms measured round the whole call,
interrupts included, and 20.5 ms with an improvement on two hexagons in
three) and there is no second buffer to draw
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

Cities and units are drawn over the terrain, **one
hexagon row behind it**. A unit's feet are in the bottom character row of its
tile, which is also the top row of the hexagons below, and those are opaque in
the middle. So the occupied hexagons of a row are noted while the row's
terrain is drawn, and their overlays are drawn after the next row's terrain.

## A turn

| **Step** |  **What happens** |
| -- |  ---- |
| Orders | each of the player's units that has moves left and no standing orders is selected in turn (`unit_next`) and brought into view |
| End of turn | with no unit left, the panel shows "End of turn"; Return, or a click on the panel, goes on |
| Computer players | `ai_turn` for each of the three: what each city builds, and every unit's move. A player with two warriors, one fortified, and a builder has a city build a settler, which founds a new city on the nearest good site within six hexagons, at least four from any other city, up to four cities |
| Barbarians | `ai_barbarians`: perhaps a new band, then every barbarian's move |
| Diplomacy | `diplomacy_turn`, and `trade_turn` at its end: contact between all the peoples, the merchant ships' voyages renewing deals, deals ended by war or for want of ships, goods asked for, the attitudes, ceasefires running out, and each computer player's war, ceasefire or peace with each other player one turn in four; what concerns the player waits as letters, shown over the map once it is redrawn |
| The Crisis | `crisis_turn`: a people that has lost its capital breaks up, its furthest cities going over to the rebels (the barbarians), and from 1200 BC the omens, the Sea Peoples' fleets and the shocks |
| Treasury | `treasury_turn`: every city's trade, less corruption (more the further it is from the capital), into its owner's gold, two gold a deal, a gold a temple paid out (or the temple sold), a gold a unit beyond the free ones (or a unit disbanded) and k² gold for k cities beyond ten, and the computer players' gold turned into shields for soldiers and galleys |
| Production | every city adds its shields (its own hexagon's, at least 1, plus its best neighbour's); one that reaches the cost of what it is building puts the unit on its hexagon, or finishes the temple or the wonder, and keeps the surplus |
| New turn | the turn number goes up, the year moves on ten years from 1600 BC, and every unit gets its moves back |

Every fight the player's units and cities are in, whether the player's
own attack or one during the other players' turns, is brought into view
and told in a popup ("Mycenae was burnt to the ground by a Barbarian
Raider."),
and what the other rulers have to say in letters ("Say to Agamemnon,
Wanax of Mycenae: thus says Suppiluliuma, Great King of Hatti. …").
The game is over when the player has lost the last city, or, before
founding one, the last unit.

The orders a unit can be given are:

| **Key** |  **Order** |
| -- |  ---- |
| 0, -, p, \*, :, = | move north-west, north-east, west, east, south-west, south-east |
| b | found a city (settlers); the settler becomes the city. A merchant ship founds an emporium on the coast next to it instead |
| space | no orders this turn |
| w | wait: ask again after the other units |
| f | fortify: no orders until woken |

A click on a hexagon next to the selected unit moves the unit there, a click
on one of the player's cities opens the city screen, and a click on one of
the player's own units selects that unit, waking it if it was fortified. A
click anywhere else on the map centres the view on that hexagon.

A galley sails the shallows. A land unit boards it by moving onto it, two
to a galley, and goes where the galley goes; when the galley stops next
to land, those aboard are asked for orders and can step ashore.

Everything can be done from the keyboard alone, since not every MEGA65 has
a mouse. Tab starts tile select, as in Civilization: a blinking yellow ring
on a hexagon, which the cursor keys move, and Return
does what a click on that hexagon would. Tab or ESC ends it. MEGA (Alt in
xemu) and a menu title's first letter opens that menu.

Otherwise the cursor keys scroll the view, HELP (PageUp in xemu) lists the keys, F1 to F4 open the advisors (cities, military, diplomacy and finance), and ESC
or RUN/STOP quits. In xemu, keypad 7, 9, 4, 6, 1 and 3 move a unit as well.

## Combat

Moving onto a hexagon held by another player is an attack. Only warriors
can attack; settlers and builders can only defend. As in Civilization,
each kind of unit has an attack and a defence strength, and the attacker
wins with probability *attack* / (*attack* + *defence*):

| **Unit** | **Attack** | **Defence** |
| -- | - | - |
| Warrior | 1 | 1 |
| Spearman (bronze) | 1 | 2 |
| Raider (the Sea Peoples only) | 3 | 1 |
| Settler, builder | 0 | 1 |
| Galley | 1 | 1 |
| Fishing boat, merchant ship | 0 | 1 |

The defence is worth half as much again on hills and twice as much on
mountains, and half as much again to a unit that is fortified or in a city.
The strengths are kept in quarter points so that the arithmetic stays in
bytes: a warrior fortified in a city on hills defends with 9 against an
attack of 4.

The unit on the hexagon that defends best fights. If it loses, outside a
city every unit on the hexagon dies with it; in a city only the defender
does. A losing attacker dies. An attack uses all of the attacker's moves,
and the winner stays where it is. A city is taken by moving into it once
nobody is left to defend it: a people keeps the city, and the barbarians
burn it.

The peoples are at peace until one attacks another, which the game asks the
player to confirm as a declaration of war. For now only the player
declares war. The barbarians are at war with everybody. They appear from
the fifteenth turn, two at a time, on empty land well away from everyone's
units and cities. A barbarian attacks whatever is next to it, whatever the
odds. A computer player's warrior attacks only when the odds are good.

A player who loses the last city is out of the game. For a computer player
that means its remaining units go too. For the human player the game is
over. The barbarians have no cities and are never out.
