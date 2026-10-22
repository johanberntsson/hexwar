# The pictures, res/*.pic, from the PNGs here. **A manual step**, as
# tiles/Makefile is: run `make` here after changing a picture and check the
# results in. The top-level build only puts the checked-in files on the
# disk and does not need Pillow. `make` also leaves a preview of each, as
# the MEGA65 will show it, in build/.
PICTURES = intro defeat win_survival win_hegemony win_legacy
PICS = $(patsubst %,../res/%.pic,$(PICTURES))

all: $(PICS)

../res/%.pic: %.png ../tools/mkpic.py
	mkdir -p build
	python3 ../tools/mkpic.py --png build/$*.png $< $@

clean:
	rm -rf build

.PHONY: all clean
