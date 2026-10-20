// In region 0 (src/region.h): called through the stubs in src/dispatch.s,
// under these names here.
#define save_write save_write_r0
#define save_read save_read_r0

#include <fcntl.h>
#include <string.h>
#include <calypsi/stubs.h>

#include "save.h"
#include "ai.h"
#include "city.h"
#include "civ.h"
#include "combat.h"
#include "game.h"
#include "map.h"
#include "unit.h"

#pragma clang section text="r0text" rodata="r0rodata"

// Everything a game is, in file order. The map goes with its explored and
// occupied bits; the terrain in it is the loaded map's, which is the same
// every game. What can be worked out again is left out: the active unit,
// the view's position (game_run() centres on whoever wants orders) and the
// panel.
struct block { void *at; word size; };

static const struct block blocks[] = {
    { map_tile, sizeof map_tile },
    { unit_kind, sizeof unit_kind },
    { unit_owner, sizeof unit_owner },
    { unit_ax, sizeof unit_ax },
    { unit_ay, sizeof unit_ay },
    { unit_moves, sizeof unit_moves },
    { unit_orders, sizeof unit_orders },
    { unit_work, sizeof unit_work },
    { ai_heading, sizeof ai_heading },
    { &city_count, sizeof city_count },
    { &city_founded, sizeof city_founded },
    { city_owner, sizeof city_owner },
    { city_ax, sizeof city_ax },
    { city_ay, sizeof city_ay },
    { city_founder, sizeof city_founder },
    { city_name, sizeof city_name },
    { city_build, sizeof city_build },
    { city_shields, sizeof city_shields },
    { city_size, sizeof city_size },
    { city_food, sizeof city_food },
    { city_worked, sizeof city_worked },
    { city_named, sizeof city_named },
    { combat_war, sizeof combat_war },
    { &combat_seed, sizeof combat_seed },
    { &game_level, sizeof game_level },
    { player_civ, sizeof player_civ },
    { &game_turn, sizeof game_turn },
    { &game_year, sizeof game_year },
};
#define BLOCKS (sizeof blocks / sizeof blocks[0])

static const char header[4] = { 'S', 'S', 'V', SAVE_VERSION };

// "@0:" replaces the file if it is there; ",S,W" makes it a SEQ file and
// opens it for writing, which a secondary address of 3 or more would not.
byte save_write(void)
{
    int fd = _Stub_open("@0:" SAVE_FILE_NAME ",S,W", O_WRONLY | O_BINARY);
    byte i, ok;

    if (fd < 0) return 0;
    ok = _Stub_write(fd, header, sizeof header) == sizeof header;
    for (i = 0; ok && i < BLOCKS; ++i)
        ok = _Stub_write(fd, blocks[i].at, blocks[i].size) == blocks[i].size;
    _Stub_close(fd);
    return ok;
}

// **In pieces under 256 bytes**, because _Stub_read miscounts a request
// that the end of the file meets on a page boundary (map_load() in map.c).
#define PIECE 128

static byte read_block(int fd, byte *at, word size)
{
    byte n;
    while (size) {
        n = size > PIECE ? PIECE : (byte)size;
        if (_Stub_read(fd, at, n) != n) return 0;
        at += n;
        size -= n;
    }
    return 1;
}

byte save_read(void)
{
    static byte got[sizeof header];
    int fd = _Stub_open(SAVE_FILE_NAME, O_RDONLY | O_BINARY);
    byte i, ok;

    if (fd < 0) return 0;
    ok = read_block(fd, got, sizeof got)
         && memcmp(got, header, sizeof header) == 0;
    for (i = 0; ok && i < BLOCKS; ++i)
        ok = read_block(fd, blocks[i].at, blocks[i].size);
    _Stub_close(fd);
    return ok;
}
