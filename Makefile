# Hexwar for the MEGA65, built with the Calypsi 6502 tool chain.
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
# hexwar.scm for what the 32 KB is doing -- and the one hot path, redrawing
# the map view, is address arithmetic feeding DMA jobs.
# Where the view starts, in map characters. Overriding these is the only way a
# headless run can photograph the view anywhere but its default position, since
# nothing headless can press a cursor key -- see the verification notes in
# CLAUDE.md and START_OFF_X in src/hexwar.h.
START_OFF_X ?= 40
START_OFF_Y ?= 30

CFLAGS   = $(TARGET) -O2 --speed -I src \
           -DSTART_OFF_X=$(START_OFF_X) -DSTART_OFF_Y=$(START_OFF_Y)
ASFLAGS  = $(TARGET)
LDFLAGS  = $(TARGET) --output-format=prg
LINKFILE = hexwar.scm

# The C stack. The tool chain's default is 4096, which is many times what this
# needs: the deepest chain is main -> view_redraw -> view_draw_tile -> lcopy,
# all of them holding bytes and longs rather than structs. There is no canary
# to measure it with, so `make CSTACK=4096` is the thing to try if a change
# starts behaving strangely for no reason.
CSTACK  ?= 512
# Nothing calls malloc. Kept nonzero because the startup wants a heap section.
HEAP    ?= 256

BUILD    = build
RES      = res

SRCS     = src/hexwar.c \
           src/mega65.c \
           src/screen.c \
           src/sheet.c \
           src/view.c \
           src/map.c

OBJS     = $(patsubst src/%.c,$(BUILD)/%.o,$(SRCS))

# The tile sheet, linked in rather than loaded off a disk. See tools/bin2s.py.
SHEET_S   = $(BUILD)/tiles.s
OBJS     += $(BUILD)/tiles.o

ELF      = $(BUILD)/hexwar.elf
PRG      = $(BUILD)/hexwar.prg

# The map, made by tools/mapgen.py and read off the disk at start-up. Same seed,
# same map: `make MAP_SEED=7` is how to look at another one, and MAP_LAND is the
# percentage of it that is land.
MAP_SEED ?= 1
MAP_LAND ?= 35
MAP      = $(BUILD)/hexwar.map
D81      = $(BUILD)/hexwar.d81

all: $(PRG) $(D81)

# xemu-xmega65 blocks until the window is closed. For an automated check run it
# headless and screenshot on exit -- see the Verification section of CLAUDE.md.
# **The disk has to be mounted**: the program reads its map from drive 8, and
# without the image it stops at "cannot read HEXWAR.MAP".
run: $(PRG) $(D81)
	xemu-xmega65 -besure -8 $(D81) -prg $(PRG)

$(BUILD):
	mkdir -p $(BUILD)

# Every object depends on every header. Crude, but src/screen.h carries the
# screen geometry and src/view.h the memory map, and an object built against a
# stale one fails as a corrupt display rather than as an error.
HDRS = $(wildcard src/*.h)

# **The config stamp, and it is not optional.** START_OFF_X and friends reach
# the compiler as -D flags, which make cannot see: without this, `make
# START_OFF_X=41` after a plain build finds every object up to date and hands
# back the *previous* binary. That looks exactly like a change that did not
# take, and it wasted a verification run here before the stamp went in. The
# stamp holds the flag values, is rewritten only when they actually differ, and
# every object depends on it.
STAMP = $(BUILD)/config.stamp
.PHONY: force
$(STAMP): force | $(BUILD)
	@echo '$(CFLAGS)' | cmp -s - $@ || echo '$(CFLAGS)' > $@

# The same trick for the map's arguments, kept separate so that a new seed
# remakes the map and the disk but not every object.
MAP_STAMP = $(BUILD)/map.stamp
$(MAP_STAMP): force | $(BUILD)
	@echo '$(MAP_SEED) $(MAP_LAND)' | cmp -s - $@ || echo '$(MAP_SEED) $(MAP_LAND)' > $@

# --png leaves a preview beside it, which is much the quickest way to judge a
# change to the generator.
$(MAP): tools/mapgen.py $(MAP_STAMP) | $(BUILD)
	python3 tools/mapgen.py --seed $(MAP_SEED) --land $(MAP_LAND) \
	    --png $(BUILD)/hexwar-map.png $@

$(BUILD)/%.o: src/%.c $(HDRS) Makefile $(STAMP) | $(BUILD)
	cc6502 $(CFLAGS) -c -o $@ $<

$(SHEET_S): $(RES)/tiles.fci tools/bin2s.py | $(BUILD)
	python3 tools/bin2s.py $< $@ tiles_fci

$(BUILD)/tiles.o: $(SHEET_S) Makefile | $(BUILD)
	as6502 $(ASFLAGS) -o $@ $<

# The list file is the memory map, and the only place the program's real size
# is written down -- worth reading after any change that might have pushed it
# towards $9FFF.
$(ELF): $(OBJS) $(LINKFILE)
	ln6502 $(LDFLAGS) --cstack-size $(CSTACK) --heap-size $(HEAP) \
	    --list-file $(BUILD)/hexwar.lst -o $@ $(LINKFILE) $(OBJS)

# **ln6502 -o names the ELF, not the PRG.** The PRG is written beside it under
# the same stem, so this rule exists only to make the dependency explicit;
# linking straight to a .prg name silently produces an ELF file that the
# MEGA65 then tries to RUN as BASIC.
$(PRG): $(ELF)

# The disk image, which is how the program gets onto a real MEGA65 and where it
# finds its map. The name autoboot.c65 is what makes the ROM boot it: a C65-mode
# PRG at $2001 needs no wrapper, which is what old-cc65/c65bin/c65toc64wrapper.prg
# used to supply. c1541 writes host names as unshifted PETSCII, so hexwar.map
# here is "HEXWAR.MAP" to the program; see MAP_FILE_NAME in src/map.h.
d81: $(D81)

$(D81): $(PRG) $(MAP)
	rm -f $@
	cp $(PRG) $(BUILD)/autoboot.c65
	c1541 -format hexwar,hw d81 $@ -write $(BUILD)/autoboot.c65 autoboot.c65 \
	    -write $(MAP) hexwar.map

clean:
	rm -rf $(BUILD)

.PHONY: all run d81 clean
