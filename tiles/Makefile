# The tile sheet, res/tiles.fci. **A manual step**: run `make` here after
# editing or adding tile art and check the result in. The top-level build only
# links the checked-in FCI and does not need Pillow.
#
# **The order of TILES is the tile numbering** -- TILE_* in src/sheet.h, and
# the matching constants in tools/mapgen.py. Append; do not reorder.
TILES = tile_desert_hill.png tile_grass.png tile_mountain.png tile_ocean.png \
        tile_shallow_ocean.png tile_tundra.png tile_city.png tile_select.png \
        unit_settler.png unit_warrior.png unit_builder.png tile_undiscovered.png

all: ../res/tiles.fci

../res/tiles.fci: $(TILES) ../tools/mktiles.py
	python3 ../tools/mktiles.py --png tiles.png $@ $(TILES)

.PHONY: all
