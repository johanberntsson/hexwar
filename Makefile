# compiler flags
CC      = $(CC65_HOME)/bin/cl65 -I include
CFLAGS  = -t c64 -O
LDFLAGS = -t c64 -m $(PROGRAM).map

# define the paths to suppprting programs
C1541 := c1541
XEMU := xemu-xmega65

# source and library files
SOURCES :=  $(wildcard src/*.c) 
OBJS := $(subst src, obj, $(SOURCES:%.c=%.o))

# png files to convert and add to the floppy
PNGS := $(wildcard assets-fci/*.png)
FCIS := $(subst assets-fci, d81data, $(PNGS:%.png=%.fci))

# phony target is simply a target that is always out-of-date
.PHONY: debug, release, all, clean

# make so that no intermediate files are ever removed
.SECONDARY:

all: debug

release: CC += -Osir -Cl
release: hexwar

debug: CC += -DDEBUG
debug: hexwar

hexwar: prepare_dirs hexwar.d81
	$(XEMU) -8 hexwar.d81 > /dev/null

c64: prepare_dirs hexwar.d64
	x64 hexwar.d64 > /dev/null

prepare_dirs:
	mkdir -p bin d81data obj

obj/%.o: src/%.c
	$(CC) -c $(CFLAGS) -o $@ $<
	#@$(MAKEDEPEND)

bin/%.prg: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

# convert png to fci (MEGA65 full color mode graphics)
d81data/%.fci: assets-fci/%.png
	python3 tools/png2fci.py -v0r $< $@

%.d81: bin/%.prg $(FCIS)
	rm -f $@
	cat c65bin/c65toc64wrapper.prg bin/$*.prg > bin/autoboot.c65
	$(C1541) -format xc-megademo,sk d81 $@
	$(C1541) $@ -write bin/autoboot.c65
	for filename in d81data/*; do \
	    $(C1541) $@ -write $$filename; \
	done

%.d64: bin/%.prg $(FCIS)
	rm -f $@
	$(C1541) -format xc-megademo,sk d64 $@
	$(C1541) $@ -write bin/$*.prg
	for filename in d81data/*; do \
	    $(C1541) $@ -write $$filename; \
	done

clean:
	rm -rf bin obj d81data *.d81 *.d64

# TODO: make this automatic
DEPS    :=  $(wildcard include/*.h) 

obj/dma.o: $(DEPS)
obj/graphics.o: $(DEPS)
obj/hexwar.o: $(DEPS)

