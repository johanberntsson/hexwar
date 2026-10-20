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
# The overlays come after: drawn over a terrain, never instead of one, and
# in bank 5 up to tile 17 and bank 1 after it (src/sheet.h). overlay_mine.png is the mine cut
# out of alltiles' plains and tundra hill mines (what the two have in common
# and neither bare hill has); overlay_irrigation.png is a placeholder.
OVERLAYS = overlay_irrigation.png overlay_mine.png
# The resources, overlays too, in RES_* order (src/map.h) from tile 18: all
# in bank 1. res_cattle, res_copper and res_tin are alltiles' cattle, copper
# and iron cut to pixel lines 8-23; res_olives and res_wine its orange trees
# recoloured; res_grain, res_fish, res_cedar and res_dye placeholders.
RESOURCES = res_grain.png res_fish.png res_cattle.png res_olives.png \
            res_wine.png res_cedar.png res_copper.png res_tin.png res_dye.png
# Copper and tin mined: alltiles' tile_copper_improved and tile_iron_improved,
# drawn instead of the resource when its hexagon has a mine (RES_TILE in
# src/map.h). Then the bronze unit, alltiles' spearman. Then cattle, olives
# and wine irrigated, in bank 1's code region 3: alltiles'
# tile_cattle_improved, and its tile_oranges_improved recoloured as
# res_olives and res_wine are.
MINES = res_copper_mine.png res_tin_mine.png
IRRIGATED = res_cattle_pasture.png res_olives_grove.png res_wine_vineyard.png
TILES = $(TERRAIN) tile_city.png tile_select.png \
        unit_settler.png unit_warrior.png unit_builder.png tile_undiscovered.png \
        $(OVERLAYS) $(RESOURCES) $(MINES) unit_spearman.png $(IRRIGATED)

all: ../res/tiles.fci

../res/tiles.fci: $(subst +, ,$(TILES)) ../tools/mktiles.py
	python3 ../tools/mktiles.py --png tiles.png $@ $(TILES)

.PHONY: all
