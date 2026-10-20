# The tile sheet, res/tiles.fci. **A manual step**: run `make` here after
# editing or adding tile art and check the result in. The top-level build only
# links the checked-in FCI and does not need Pillow.
#
# **The order of TILES is the tile numbering** -- TILE_* in src/sheet.h, and
# the matching constants in tools/mapgen.py. Terrain values are tile numbers,
# so the terrains come first and everything drawn over them after; a new
# terrain goes at the end of the first group and renumbers the second, which
# means changing all three places together.
#
# `a.png+b.png` is one tile, b drawn over a (mktiles.py): tile_forest.png is
# trees on a transparent hexagon, baked over the plains here rather than drawn
# as an overlay, which would cost the redraw a fifth DMA job a forest.
TERRAIN = tile_plains_hill.png tile_grass.png tile_mountain.png tile_ocean.png \
          tile_shallow_ocean.png tile_tundra.png tile_plains.png \
          tile_plains.png+tile_forest.png tile_desert.png tile_floodplains.png
TILES = $(TERRAIN) tile_city.png tile_select.png \
        unit_settler.png unit_warrior.png unit_builder.png tile_undiscovered.png

all: ../res/tiles.fci

../res/tiles.fci: $(subst +, ,$(TILES)) ../tools/mktiles.py
	python3 ../tools/mktiles.py --png tiles.png $@ $(TILES)

.PHONY: all
