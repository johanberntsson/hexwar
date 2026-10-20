#!/usr/bin/env python3
"""Generate a map file for Seven Ships of the Enemy.

    mapgen.py [--seed N] [--land PERCENT] [--png preview.png] out.map

The steps follow the Civilization 1 generator described in design.md (and the
civfanatics thread it links), adapted to a hex grid and to the ten terrains
the sheet has today:

1. Land masses: random walks that stamp a hexagon and its six neighbours,
   repeated until the land target is reached. **The map wraps east-west**,
   as the game's view does, so land may run across the seam; it is kept two
   cells away from the top and bottom only.
2. Relief: shorter random walks that raise mountain ranges, only on land at
   least two steps from the coast, so ranges never sit on a beach; then hills,
   as foothills round the ranges and as a few short walks of their own.
3. Rainfall: wind blows along each row, picking moisture up over water and
   dropping it over land, more of it against mountains; done west to east and
   east to west and summed. Each pass goes round the row twice and keeps the
   second lap, so that the seam gets the moisture the wrap would bring it.
   In the warm band dry land becomes desert, then plains, then grassland as
   the rain rises; elsewhere plains, grassland and, wettest, forest.
4. Temperature: land near the top and bottom of the map becomes tundra, with
   a ragged edge rather than a straight line.
5. Rivers: from mountains and hills down the coast distance to the sea.
   They are not drawn -- the sheet has no river tile yet -- but where one
   crosses desert or plains it leaves a line of floodplain, the Nile's
   arrangement and the grain the game's peoples lived on.
6. Clean-up: one-hexagon lakes are filled in, and ocean next to land becomes
   shallow ocean.

**The file layout**, which src/map.c reads; all of it little-endian:

    0   3   "HXM"
    3   1   format version, MAP_VERSION
    4   1   columns (128)
    5   1   rows (32)
    6   1   number of layers that follow
    7   1   0
    8   4   the seed that made it
    12  4   0
    16      layers, columns*rows bytes each, row-major [ay][ax]

Layer 0 is terrain. **Its values are tile numbers** -- TILE_* in src/sheet.h,
mirrored in TERRAIN below -- because for now every terrain has exactly one
tile. When they stop matching, this layer keeps meaning terrain and the game
grows a terrain-to-tile table.

The coordinates are the game's: array cell (ax, ay) is the hexagon
(ax, 2*ay + (ax & 1)) in doubled coordinates; see src/map.h.
"""

import argparse
import random
import struct
import sys
import zlib
from collections import deque

# MAP_COLUMNS and MAP_ROWS in src/map.h. Keep the two in step.
COLUMNS = 128
ROWS = 32
MAP_VERSION = 2

# TILE_* in src/sheet.h. Keep the two in step.
(HILLS, GRASS, MOUNTAIN, OCEAN, SHALLOW_OCEAN, TUNDRA,
 PLAINS, FOREST, DESERT, FLOODPLAIN) = range(10)
# Lowland: what the climate decides, and what erosion may turn into another.
LOWLAND = (GRASS, PLAINS, FOREST, DESERT, TUNDRA)

# Preview colours, one per terrain value.
COLOURS = {
    HILLS: (120, 96, 48),
    GRASS: (64, 150, 48),
    MOUNTAIN: (130, 130, 140),
    OCEAN: (40, 60, 160),
    SHALLOW_OCEAN: (70, 120, 210),
    TUNDRA: (200, 215, 220),
    PLAINS: (150, 140, 56),
    FOREST: (16, 80, 24),
    DESERT: (240, 228, 140),
    FLOODPLAIN: (150, 210, 80),
}

# Rainfall thresholds; see generate(). In the warm band, below DESERT_RAIN
# is desert and below DRY_PLAINS_RAIN plains; elsewhere below PLAINS_RAIN is
# plains and above FOREST_RAIN forest. The rest is grassland.
DESERT_RAIN = 1.3
DRY_PLAINS_RAIN = 1.9
PLAINS_RAIN = 1.0
FOREST_RAIN = 2.0
# Rivers to try for; see rivers().
RIVERS = 24

# The six neighbours of a hexagon in doubled coordinates.
HEX_STEPS = ((2, 0), (-2, 0), (1, 1), (1, -1), (-1, 1), (-1, -1))


def step(ax, ay, dx, dy):
    """The cell one hex step (dx, dy) from (ax, ay), wrapped east-west, or
    None off the top or bottom. COLUMNS is even, so wrapping keeps parity."""
    nx, ny = ax + dx, 2 * ay + (ax & 1) + dy
    nay = (ny - (nx & 1)) // 2
    if ny < 0 or not 0 <= nay < ROWS:
        return None
    return nx % COLUMNS, nay


def neighbours(ax, ay):
    """The cells next to (ax, ay): six, or fewer at the top and bottom."""
    for dx, dy in HEX_STEPS:
        cell = step(ax, ay, dx, dy)
        if cell:
            yield cell


def inside(ay, margin):
    return margin <= ay < ROWS - margin


def make_land(rng, land_percent):
    """Step 1: a boolean grid, True for land."""
    land = [[False] * COLUMNS for _ in range(ROWS)]
    target = COLUMNS * ROWS * land_percent // 100
    count = 0
    while count < target:
        ax = rng.randrange(COLUMNS)
        ay = rng.randrange(3, ROWS - 3)
        for _ in range(rng.randrange(8, 64)):
            for cx, cy in [(ax, ay)] + list(neighbours(ax, ay)):
                if inside(cy, 2) and not land[cy][cx]:
                    land[cy][cx] = True
                    count += 1
            nx, ny = rng.choice(list(neighbours(ax, ay)))
            if inside(ny, 3):
                ax, ay = nx, ny
    return land


def coast_distance(land):
    """Steps from each land cell to the nearest water; 0 for water."""
    dist = [[0 if not land[y][x] else None for x in range(COLUMNS)]
            for y in range(ROWS)]
    queue = deque((x, y) for y in range(ROWS) for x in range(COLUMNS)
                  if not land[y][x])
    while queue:
        x, y = queue.popleft()
        for nx, ny in neighbours(x, y):
            if dist[ny][nx] is None:
                dist[ny][nx] = dist[y][x] + 1
                queue.append((nx, ny))
    return dist


def raise_mountains(rng, terrain, dist):
    """Step 2: mountain ranges, inland only. About one land cell in twelve."""
    inland = [(x, y) for y in range(ROWS) for x in range(COLUMNS)
              if dist[y][x] >= 2]
    if not inland:
        return
    target = len([1 for row in dist for d in row if d]) // 12
    placed = 0
    attempts = 0
    while placed < target and attempts < 1000:
        attempts += 1
        x, y = rng.choice(inland)
        # A range has a heading, so it reads as a ridge rather than a blob.
        dx, dy = rng.choice(HEX_STEPS)
        for _ in range(rng.randrange(3, 10)):
            if dist[y][x] >= 2 and terrain[y][x] != MOUNTAIN:
                terrain[y][x] = MOUNTAIN
                placed += 1
            # Mostly keep the heading, sometimes wander one step off it.
            if rng.random() < 0.3:
                dx, dy = rng.choice(HEX_STEPS)
            cell = step(x, y, dx, dy)
            if not cell:
                break
            x, y = cell


def raise_hills(rng, terrain, dist):
    """Step 2, second half: foothills beside the ranges, then short walks.
    About one land cell in twelve again, all told."""
    for y in range(ROWS):
        for x in range(COLUMNS):
            if (terrain[y][x] == GRASS and dist[y][x] >= 1
                    and any(terrain[ny][nx] == MOUNTAIN
                            for nx, ny in neighbours(x, y))
                    and rng.random() < 0.35):
                terrain[y][x] = HILLS
    lowland = [(x, y) for y in range(ROWS) for x in range(COLUMNS)
               if dist[y][x] >= 1]
    if not lowland:
        return
    target = len(lowland) // 24
    placed = 0
    while placed < target:
        x, y = rng.choice(lowland)
        for _ in range(rng.randrange(2, 6)):
            if dist[y][x] >= 1 and terrain[y][x] == GRASS:
                terrain[y][x] = HILLS
                placed += 1
            cell = rng.choice(list(neighbours(x, y)))
            x, y = cell


def rainfall(terrain, land):
    """Step 3: moisture per cell from two passes of wind along each row."""
    rain = [[0.0] * COLUMNS for _ in range(ROWS)]
    for y in range(ROWS):
        for xs in (range(COLUMNS), range(COLUMNS - 1, -1, -1)):
            moisture = 0.0
            for lap in (0, 1):
                for x in xs:
                    if not land[y][x]:
                        moisture = min(moisture + 1.0, 8.0)
                        continue
                    drop = moisture * (0.5 if terrain[y][x] == MOUNTAIN else 0.15)
                    if lap:
                        rain[y][x] += drop
                    moisture -= drop
    return rain


def rivers(rng, terrain, dist):
    """Step 5: each river runs from a mountain or hill to the sea, always
    to a neighbour nearer the coast, and turns the desert and plains on its
    way into floodplain. Only rivers that cross some of either are kept."""
    sources = [(x, y) for y in range(ROWS) for x in range(COLUMNS)
               if terrain[y][x] in (MOUNTAIN, HILLS) and dist[y][x] >= 2]
    rng.shuffle(sources)
    made = 0
    for x, y in sources:
        if made >= RIVERS:
            break
        path = []
        while dist[y][x] > 0:
            lower = [(nx, ny) for nx, ny in neighbours(x, y)
                     if dist[ny][nx] < dist[y][x]]
            x, y = rng.choice(lower)
            path.append((x, y))
        dry = [(px, py) for px, py in path if terrain[py][px] in (DESERT, PLAINS)]
        if len(dry) < 2:
            continue
        for px, py in dry:
            terrain[py][px] = FLOODPLAIN
        made += 1


def latitude(ay):
    """0 at the equator, 1 at either pole."""
    centre = (ROWS - 1) / 2
    return abs(ay - centre) / centre


def generate(seed, land_percent):
    rng = random.Random(seed)
    land = make_land(rng, land_percent)
    terrain = [[GRASS if land[y][x] else OCEAN for x in range(COLUMNS)]
               for y in range(ROWS)]
    dist = coast_distance(land)
    raise_mountains(rng, terrain, dist)

    raise_hills(rng, terrain, dist)

    # Steps 3 and 4. The desert band sits between the tropics and the cold,
    # and the noise on both thresholds is what makes the edges ragged.
    rain = rainfall(terrain, land)
    for y in range(ROWS):
        for x in range(COLUMNS):
            if terrain[y][x] != GRASS:
                continue
            lat = latitude(y) + rng.uniform(-0.08, 0.08)
            wet = rain[y][x] + rng.uniform(0, 1.5)
            if lat > 0.68:
                terrain[y][x] = TUNDRA
            elif 0.25 < lat < 0.55:
                if wet < DESERT_RAIN:
                    terrain[y][x] = DESERT
                elif wet < DRY_PLAINS_RAIN:
                    terrain[y][x] = PLAINS
            elif wet < PLAINS_RAIN:
                terrain[y][x] = PLAINS
            elif wet > FOREST_RAIN:
                terrain[y][x] = FOREST

    # Erosion, roughly Civ 1's "age" step: a cell most of whose neighbours
    # are one other lowland terrain becomes that terrain. Without it the
    # rainfall noise leaves the desert band speckled with single grass cells.
    for _ in range(2):
        changes = []
        for y in range(ROWS):
            for x in range(COLUMNS):
                if terrain[y][x] not in LOWLAND:
                    continue
                counts = {}
                for nx, ny in neighbours(x, y):
                    t = terrain[ny][nx]
                    counts[t] = counts.get(t, 0) + 1
                for t, n in counts.items():
                    if n >= 4 and t != terrain[y][x] and t in LOWLAND:
                        changes.append((x, y, t))
        for x, y, t in changes:
            terrain[y][x] = t

    rivers(rng, terrain, dist)

    # Step 6. Lakes first, so a filled lake does not leave a shallow ring.
    for y in range(ROWS):
        for x in range(COLUMNS):
            if terrain[y][x] == OCEAN and all(
                    land[ny][nx] for nx, ny in neighbours(x, y)):
                terrain[y][x] = GRASS
                land[y][x] = True
    for y in range(ROWS):
        for x in range(COLUMNS):
            if terrain[y][x] == OCEAN and any(
                    land[ny][nx] for nx, ny in neighbours(x, y)):
                terrain[y][x] = SHALLOW_OCEAN
    return terrain


def write_map(path, seed, terrain):
    header = b"HXM" + struct.pack("<BBBBBII", MAP_VERSION, COLUMNS, ROWS, 1, 0,
                                  seed & 0xFFFFFFFF, 0)
    assert len(header) == 16
    body = bytes(t for row in terrain for t in row)
    with open(path, "wb") as f:
        f.write(header + body)


def write_png(path, terrain):
    """A rough preview: each hexagon a 6x8 brick, odd columns half a row down,
    which is the same arrangement the game draws. The map wraps east-west, so
    the left and right edges of the picture join."""
    cw, rh = 6, 4                       # per column, per hexagon row (hy)
    width, height = COLUMNS * cw, (2 * ROWS + 1) * rh
    pixels = [[(0, 0, 0)] * width for _ in range(height)]
    for y in range(ROWS):
        for x in range(COLUMNS):
            top = (2 * y + (x & 1)) * rh
            colour = COLOURS[terrain[y][x]]
            for py in range(top, top + 2 * rh):
                for px in range(x * cw, x * cw + cw):
                    pixels[py][px] = colour
    raw = b"".join(b"\0" + bytes(c for p in row for c in p) for row in pixels)

    def chunk(kind, data):
        out = struct.pack(">I", len(data)) + kind + data
        return out + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2,
                                           0, 0, 0)))
        f.write(chunk(b"IDAT", zlib.compress(raw, 9)))
        f.write(chunk(b"IEND", b""))


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("output")
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--land", type=int, default=35,
                        help="land as a percentage of the map (default 35)")
    parser.add_argument("--png", help="also write a preview image here")
    args = parser.parse_args()
    if not 5 <= args.land <= 70:
        sys.exit("mapgen: --land must be between 5 and 70")

    terrain = generate(args.seed, args.land)
    write_map(args.output, args.seed, terrain)
    if args.png:
        write_png(args.png, terrain)


if __name__ == "__main__":
    main()
