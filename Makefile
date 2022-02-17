# compiler flags
CC      = $(CC65_HOME)/bin/cl65 -I mega65-libc/cc65/include
CFLAGS  = -t c64 -O
LDFLAGS = -t c64 -m $(PROGRAM).map

# define the paths to suppprting programs
C1541 := c1541
XEMU := xemu-xmega65

# source and library files
SOURCES :=  $(wildcard src/*.c) mega65-libc/cc65/src/memory.c mega65-libc/cc65/src/fileio.s

# png files to convert and add to the floppy
PNGS := $(wildcard assets-img/*.png)
FCIS := $(subst assets-img, res, $(PNGS:%.png=%.fci))

# phony target is simply a target that is always out-of-date
.PHONY: civ, all, clean

# make so that no intermediate files are ever removed
.SECONDARY:

all: civ

civ: civ.d81
	$(XEMU) -8 civ.d81

bin/%.o: src/%.c
	$(CC) -c $(CFLAGS) -o $@ $<
	@$(MAKEDEPEND)

bin/%.prg: $(SOURCES:.c=.o)
	$(CC) $(LDFLAGS) -o $@ $^

# convert png to fci (MEGA65 full color mode graphics)
res/%.fci: assets-img/%.png
	python3 tools/png2fci.py -v0r $< $@

%.d81: bin/%.prg $(FCIS)
	rm -f $@
	cat c65bin/c65toc64wrapper.prg bin/$*.prg > bin/autoboot.c65
	$(C1541) -format xc-megademo,sk d81 $@
	$(C1541) $@ -write bin/autoboot.c65
	for filename in res/*; do \
	    $(C1541) $@ -write $$filename; \
	done
	#for filename in assets-other/*; do \
	#    $(C1541) $@ -write $$filename; \
	#done

clean:
	rm -rf bin/* res/* *.d81

