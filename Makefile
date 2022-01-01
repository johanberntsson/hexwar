TARGETS := hexwar
C1541 := c1541
X64 := x64
X128 := x128
XMEGA := xemu-xmega65 -autoload -8

.PRECIOUS: %.d81

all: $(TARGETS)

%.prg: asm/%.asm
	mkdir temp
	acme --setpc 0x2000 --cpu m65 --format cbm -l temp/labels --outfile temp/$@ $<

%.d81: %.prg
	$(C1541) -format foo,id d81 $@ -write temp/$<

%: %.d81
	$(XMEGA) $<

clean:
	rm -f $(TARGETS) *.prg *.d81 temp/*
