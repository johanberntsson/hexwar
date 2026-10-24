# Seven Ships of the Enemy for the MEGA65, built with the Calypsi 6502 tool chain.
#
# This replaced a cl65/cc65 build (old-cc65/Makefile). The switch is not only a
# change of compiler: cc65 built a C64-mode program that a wrapper
# (old-cc65/c65bin/) launched out of C65 mode, and Calypsi's mega65 target
# builds a C65-mode program that the ROM boots directly. What the wrapper used
# to do, the file name autoboot.c65 now does on its own.
#
# **There is nothing to clone or vendor for the board support.**
# hth313/Calypsi-6502-Commodore ships *inside* the tool chain: its src/ is
# /usr/local/lib/calypsi-6502-5.18/contrib/MEGA65-SDK/src and its
# linker-files/ is .../linker-rules, byte for byte (checked with diff -rq
# against a fresh clone, Calypsi 5.18). --target=mega65 already links it, and
# it is what provides the C65-mode startup at $2001 and the KERNAL stubs
# behind open/read/write.

TARGET   = --target=mega65

# -O2 --speed rather than -Os. Nothing here is size-critical yet -- see
# sevenships.scm for what the 32 KB is doing -- and the one hot path, redrawing
# the map view, is address arithmetic feeding DMA jobs.
# Where the view starts, in map characters. Overriding these is the only way a
# headless run can photograph the view anywhere but its default position, since
# nothing headless can press a cursor key -- see the verification notes in
# CLAUDE.md and START_OFF_X in src/sevenships.h.
START_OFF_X ?= 40
START_OFF_Y ?= 30

CFLAGS   = $(TARGET) -O2 --speed -I src -I build \
           -DSTART_OFF_X=$(START_OFF_X) -DSTART_OFF_Y=$(START_OFF_Y)

# Keys for a headless run to "press", since nothing headless can: each entry
# is {frame, arrow x, arrow y, key}, in frame order -- see src/input.c.
#     make NO_TITLE=1 FAKE_KEYS='{30,0,0,98}'        found a city on frame 30
#     make NO_TITLE=1 FAKE_KEYS='{30,200,96,KEY_CLICK}'  click at (200,96)
ifneq ($(FAKE_KEYS),)
CFLAGS  += '-DFAKE_KEYS=$(FAKE_KEYS)'
endif
# Straight to the map, without the title screen: what a headless screenshot
# of the view wants. With the title, FAKE_KEYS has to start with four
# Returns, {10,0,0,13},{12,0,0,13},{14,0,0,13},{16,0,0,13}: Start Game, the
# first people, the first difficulty level, and closing the opening popup.
ifneq ($(NO_TITLE),)
CFLAGS  += -DNO_TITLE
endif
# The Debug menu -- Show Map and Play 50 Turns -- and everything behind it.
# On for ordinary builds, off for `make release`, which saves the bytes;
# `make DEBUG=0` builds without it otherwise.
ifneq ($(filter release,$(MAKECMDGOALS)),)
DEBUG ?= 0
endif
DEBUG ?= 1
ifeq ($(DEBUG),1)
CFLAGS  += -DDEBUG_MENU
endif
# Debug: Play 50 Turns plays this many instead (up to 255), so that a
# headless test of a long game needs fewer FAKE_KEYS, which take resident
# space: `make NO_TITLE=1 AUTOPLAY_TURNS=200 FAKE_KEYS=...`.
ifneq ($(AUTOPLAY_TURNS),)
CFLAGS  += -DAUTOPLAY_TURNS=$(AUTOPLAY_TURNS)
endif
ASFLAGS  = $(TARGET)
# Raw, one file a memory, since the windows' regions share addresses;
# tools/packprg.py makes the PRG and the region files from them. --hosted
# keeps initialised data loaded in place, as the prg format does.
LDFLAGS  = $(TARGET) --output-format=raw --raw-multiple-memories \
           --no-merge-raw-memories --hosted
LINKFILE = sevenships.scm

# The C stack. The tool chain's default is 4096, which is many times what this
# needs: return addresses go on the 6502's own stack in page 1, and this one
# holds only locals and spills. Measured 19 Oct 2026 by filling it with $AA at
# the top of main() and reading a -dumpmem after a scripted game (city screen,
# CHANGE, help, save, Play 50 Turns): about 70 bytes used, so 256 leaves a
# good margin. `make CSTACK=4096` is the thing to try if a change starts
# behaving strangely for no reason, and the same measurement is cheap to
# repeat.
CSTACK  ?= 256
# Nothing calls malloc. Kept nonzero because the startup wants a heap section.
HEAP    ?= 256

BUILD    = build
RES      = res

SRCS     = src/sevenships.c \
           src/mega65.c \
           src/screen.c \
           src/sheet.c \
           src/view.c \
           src/map.c \
           src/mapload.c \
           src/minimap.c \
           src/input.c \
           src/unit.c \
           src/city.c \
           src/cityscreen.c \
           src/help.c \
           src/topcities.c \
           src/diplomacy.c \
           src/trade.c \
           src/path.c src/boat.c src/emporium.c \
           src/save.c \
           src/text.c \
           src/game.c \
           src/turn.c src/hover.c src/score.c src/military.c src/crisis.c src/treasury.c src/culture.c \
           src/command.c src/cities.c \
           src/ai.c \
           src/aiplace.c src/aibuild.c src/aisite.c \
           src/improve.c \
           src/growth.c \
           src/combat.c src/fight.c \
           src/civ.c \
           src/popup.c \
           src/menu.c \
           src/title.c src/picture.c \
           src/region.c

# The files whose code is in a MAP window region (src/region.h), compiled
# without the shared instruction fragments that could be called across one.
BANKED   = src/title.c src/picture.c src/turn.c src/hover.c src/score.c src/military.c src/crisis.c src/treasury.c src/culture.c src/command.c src/cities.c src/aiplace.c src/cityscreen.c src/topcities.c src/fight.c src/diplomacy.c \
           src/save.c src/sheet.c src/mapload.c src/help.c src/menu.c src/ai.c src/aibuild.c src/aisite.c \
           src/improve.c src/growth.c src/trade.c src/path.c src/boat.c src/emporium.c

# Assembly: src/bank.s, which banks BASIC out before main(), and
# src/dispatch.s, the MAP window and the calls into it.
ASRCS    = src/bank.s src/dispatch.s

OBJS     = $(patsubst src/%.c,$(BUILD)/%.o,$(SRCS)) \
           $(patsubst src/%.s,$(BUILD)/%.o,$(ASRCS))

ELF      = $(BUILD)/sevenships.elf
PRG      = $(BUILD)/sevenships.prg
# The code regions, read into attic RAM and run from bank 1, one file each
# (src/region.h).
REGION_FILES = $(BUILD)/sevenships.r1 $(BUILD)/sevenships.r2 \
               $(BUILD)/sevenships.r3 $(BUILD)/sevenships.r4 \
               $(BUILD)/sevenships.r5 $(BUILD)/sevenships.r6 \
               $(BUILD)/sevenships.r7

# The map, made by tools/mapgen.py and read off the disk at start-up. Same seed,
# same map: `make MAP_SEED=7` is how to look at another one, and MAP_LAND is the
# percentage of it that is land.
MAP_SEED ?= 1
MAP_LAND ?= 35
MAP      = $(BUILD)/sevenships.map
D81      = $(BUILD)/sevenships.d81

# The texts, packed by tools/mktext.py into a file for the disk and a header
# of their numbers (src/text.h). **The order here is the numbering**, so add
# to the end.
TEXTS    = $(RES)/text/help.txt \
           $(RES)/text/title.txt \
           $(RES)/text/choose_people.txt \
           $(RES)/text/choose_level.txt \
           $(RES)/text/intro.txt \
           $(RES)/text/list_title_menu.txt \
           $(RES)/text/list_peoples.txt \
           $(RES)/text/list_adjectives.txt \
           $(RES)/text/list_notes.txt \
           $(RES)/text/list_leaders.txt \
           $(RES)/text/list_titles.txt \
           $(RES)/text/list_levels.txt \
           $(RES)/text/list_level_notes.txt \
           $(RES)/text/list_cities.txt \
           $(RES)/text/list_resources.txt \
           $(RES)/text/letter_greet.txt \
           $(RES)/text/letter_war.txt \
           $(RES)/text/letter_ceasefire.txt \
           $(RES)/text/letter_peace.txt \
           $(RES)/text/letter_truce_over.txt \
           $(RES)/text/letter_yes.txt \
           $(RES)/text/letter_no.txt \
           $(RES)/text/letter_trade.txt \
           $(RES)/text/letter_tribute.txt \
           $(RES)/text/list_terrains.txt \
           $(RES)/text/pedia_hills.txt \
           $(RES)/text/pedia_grassland.txt \
           $(RES)/text/pedia_mountains.txt \
           $(RES)/text/pedia_ocean.txt \
           $(RES)/text/pedia_shallows.txt \
           $(RES)/text/pedia_tundra.txt \
           $(RES)/text/pedia_plains.txt \
           $(RES)/text/pedia_forest.txt \
           $(RES)/text/pedia_desert.txt \
           $(RES)/text/pedia_floodplain.txt \
           $(RES)/text/crisis_rumour.txt \
           $(RES)/text/crisis_warning.txt \
           $(RES)/text/crisis_begins.txt \
           $(RES)/text/crisis_alone.txt \
           $(RES)/text/crisis_plea.txt \
           $(RES)/text/pedia_settler.txt \
           $(RES)/text/pedia_warrior.txt \
           $(RES)/text/pedia_builder.txt \
           $(RES)/text/pedia_spearman.txt \
           $(RES)/text/pedia_raider.txt \
           $(RES)/text/pedia_chariot.txt \
           $(RES)/text/pedia_galley.txt \
           $(RES)/text/pedia_boat.txt \
           $(RES)/text/pedia_merchant.txt \
           $(RES)/text/pedia_temple.txt \
           $(RES)/text/pedia_lion_gate.txt
TEXT_PAK = $(BUILD)/text.pak
TEXT_IDS = $(BUILD)/textid.h

all: $(PRG) $(D81)

# xemu-xmega65 blocks until the window is closed. For an automated check run it
# headless and screenshot on exit -- see the Verification section of CLAUDE.md.
# **The disk has to be mounted**: the program reads its map from drive 8, and
# without the image it stops at "cannot read SEVENSHIPS.MAP".
run: $(PRG) $(D81)
	xemu-xmega65 -besure -keymap $(CURDIR)/res/xemu-keymap.cfg -8 $(D81) -prg $(PRG)

$(BUILD):
	mkdir -p $(BUILD)

# Every object depends on every header. Crude, but src/screen.h carries the
# screen geometry and src/view.h the memory map, and an object built against a
# stale one fails as a corrupt display rather than as an error.
HDRS = $(wildcard src/*.h) $(TEXT_IDS)

# **The config stamp, and it is not optional.** START_OFF_X and friends reach
# the compiler as -D flags, which make cannot see: without this, `make
# START_OFF_X=41` after a plain build finds every object up to date and hands
# back the *previous* binary. That looks exactly like a change that did not
# take, and it wasted a verification run here before the stamp went in. The
# stamp holds the flag values, is rewritten only when they actually differ, and
# every object depends on it.
#
# **Written by make's own $(file), never through the shell.** It used to be
# `echo '$(CFLAGS)'`, and FAKE_KEYS brings its own single quotes into CFLAGS,
# which left its braces unquoted: bash brace-expanded {10,0,0,13},{12,0,0,13},
# ... into 4^n words -- 268 million for a 14-entry script -- until the kernel
# killed it, having made the whole desktop crawl and left the binary stale.
STAMP = $(BUILD)/config.stamp
MAP_STAMP = $(BUILD)/map.stamp
$(shell mkdir -p $(BUILD))
ifneq ($(file <$(STAMP)),$(CFLAGS))
$(file >$(STAMP),$(CFLAGS))
endif

# The same for the map's arguments, kept separate so that a new seed remakes
# the map and the disk but not every object.
ifneq ($(file <$(MAP_STAMP)),$(MAP_SEED) $(MAP_LAND))
$(file >$(MAP_STAMP),$(MAP_SEED) $(MAP_LAND))
endif

# --png leaves a preview beside it, which is much the quickest way to judge a
# change to the generator.
$(MAP): tools/mapgen.py $(MAP_STAMP) | $(BUILD)
	python3 tools/mapgen.py --seed $(MAP_SEED) --land $(MAP_LAND) \
	    --png $(BUILD)/sevenships-map.png $@

# The pack and the header come out of one run. mktext.py leaves the header
# alone when the numbering has not changed, so editing what a text says
# remakes the pack and the disk but no object.
$(TEXT_PAK) $(TEXT_IDS) &: $(TEXTS) tools/mktext.py | $(BUILD)
	python3 tools/mktext.py $(TEXT_PAK) $(TEXT_IDS) $(TEXTS)

$(patsubst src/%.c,$(BUILD)/%.o,$(BANKED)): \
    CFLAGS += --no-cross-call --no-interprocedural-cross-jump

$(BUILD)/%.o: src/%.c $(HDRS) Makefile $(STAMP) | $(BUILD)
	cc6502 $(CFLAGS) -c -o $@ $<

$(BUILD)/%.o: src/%.s Makefile | $(BUILD)
	as6502 $(ASFLAGS) -o $@ $<

# The list file is the memory map, and the only place the program's real size
# is written down -- worth reading after any change that might have pushed it
# towards $9FFF.
#
# **ln6502 -o names the ELF, not the PRG.** The raw files are written beside
# it, the first under the same stem and the rest under their memories'
# names, and tools/packprg.py makes the PRG and the region files out of
# them (src/region.h). The raw files are removed first, so that a memory
# left empty by this link cannot be packed from the last one. One grouped
# target (&:, GNU make 4.3) for all of it: it used to be a separate
# `$(PRG): $(ELF)` with no recipe, and make never looks at the time of a
# target it has no recipe for, so a relinked PRG left the disk image stale:
# `make` then `make release` shipped the Debug menu (19 Oct 2026).
$(ELF) $(PRG) $(REGION_FILES) &: $(OBJS) $(LINKFILE) tools/packprg.py
	rm -f $(BUILD)/*.raw
	ln6502 $(LDFLAGS) --cstack-size $(CSTACK) --heap-size $(HEAP) \
	    --list-file $(BUILD)/sevenships.lst -o $(ELF) $(LINKFILE) $(OBJS)
	python3 tools/packprg.py $(BUILD)/sevenships.lst $(BUILD)/sevenships \
	    $(PRG) $(BUILD)/sevenships

# The disk image, which is how the program gets onto a real MEGA65 and where it
# finds its map and its tile sheet (res/tiles.fci, which tiles/Makefile
# makes and which is checked in). The name autoboot.c65 is what makes the ROM boot it: a C65-mode
# PRG at $2001 needs no wrapper, which is what old-cc65/c65bin/c65toc64wrapper.prg
# used to supply.
#
# tools/diskutil.rb (Fredrik Ramsberg's, copied from Search And Rescue) rather
# than c1541, for its sector interleave: copied to a real floppy, the disk
# reads about five times faster on a MEGA65 than with the 1581's default.
# It names a file on the disk after the host file's basename, which is why the
# PRG is copied to autoboot.c65 first, and writes that name as unshifted
# PETSCII, so sevenships.map here is "SEVENSHIPS.MAP" to the program; see
# MAP_FILE_NAME in src/map.h. It refuses to overwrite a file already on the
# image, so the image is always built from scratch. The data files go on as
# SEQ, as Search And Rescue's do: they have no load address, and the program
# opens them by name whatever their type.
d81: $(D81)

# The pictures, made by screens/Makefile and checked in (src/picture.h).
PICTURES = $(RES)/intro.pic $(RES)/defeat.pic $(RES)/win_survival.pic \
           $(RES)/win_hegemony.pic $(RES)/win_legacy.pic

$(D81): $(PRG) $(REGION_FILES) $(MAP) $(RES)/tiles.fci $(TEXT_PAK) $(PICTURES) tools/diskutil.rb
	rm -f $@
	cp $(PRG) $(BUILD)/autoboot.c65
	ruby tools/diskutil.rb $@ -name sevenships -id ss \
	    -writeprg -copyf1 $(BUILD)/autoboot.c65 \
	    -writeseq -copyf1 $(MAP) $(RES)/tiles.fci $(TEXT_PAK) $(REGION_FILES) \
	    $(PICTURES)

# The disk image as it is handed out. Plain `make release` after a test build
# is safe, since the config stamp sees the flags change and rebuilds; what is
# refused is a release *of* a test build. `make clean` leaves release/ alone.
RELEASE  = release

release: $(D81)
ifneq ($(FAKE_KEYS)$(NO_TITLE),)
	$(error a release must not be built with FAKE_KEYS or NO_TITLE)
endif
ifeq ($(DEBUG),1)
	$(error a release must not be built with DEBUG=1)
endif
	mkdir -p $(RELEASE)
	cp $(D81) $(RELEASE)/

clean:
	rm -rf $(BUILD)

.PHONY: all run d81 release clean
