TARGET := bin/hexwar.prg
SRC_FILES := $(wildcard src/*.asm)
ACME_FILES := $(patsubst %.asm, %.a, $(SRC_FILES))
ASM_FILES := $(patsubst src/%, asm/%, $(ACME_FILES))
C1541 := c1541
XMEGA := xemu-xmega65

all: $(TARGET) test

$(TARGET): $(ASM_FILES)
	mkdir -p temp
	acme --setpc 0x2000 --cpu m65 --format cbm -l temp/labels --report temp/repot --outfile $@ $<

asm/%.a: src/%.asm
	mkdir -p asm
	acme-preprocessor.rb $< $@

test:
	$(XMEGA) -prg bin/hexwar.prg

d81:
	cat bin/c65toc64wrapper.prg $(TARGET) > bin/autoboot.c65
	$(C1541) -format foo,id d81 bin/hexwar.d81 -write bin/autoboot.c65
	$(XMEGA) -8 bin/hexwar.d81

clean:
	rm -f temp/* bin/*
