#include "combat.h"
#include "city.h"
#include "map.h"
#include "sheet.h"
#include "unit.h"

// Bit q of war[p]: p is at war with q. Kept both ways round.
static byte war[PLAYERS];

byte combat_owner;
byte combat_kind;
byte combat_city;
byte news;
byte news_owner;

static word seed = 0x2a6d;

byte rnd(void)
{
    seed ^= seed << 7;
    seed ^= seed >> 9;
    seed ^= seed << 8;
    return (byte)seed;
}

byte at_war(byte a, byte b)
{
    if (a == PLAYER_BARBARIAN || b == PLAYER_BARBARIAN) return 1;
    return war[a] & (byte)(1 << b);
}

void declare_war(byte a, byte b)
{
    war[a] |= (byte)(1 << b);
    war[b] |= (byte)(1 << a);
}

void combat_reset(void)
{
    byte p;
    for (p = 0; p < PLAYERS; ++p) war[p] = 0;
    news = NEWS_NONE;
}

byte combat_holder(byte ax, byte ay)
{
    byte u = unit_at(ax, ay);
    return u != UNIT_NONE ? unit_owner[u] : city_owner[city_at(ax, ay)];
}

// Unit u's defence where it stands, in quarters of a point.
static byte defence(byte u)
{
    byte ax = unit_ax[u], ay = unit_ay[u];
    byte t = MAP_TERRAIN(ax, ay);
    byte d = (byte)(4 * unit_kind_defence[unit_kind[u]]);

    if (t == TILE_DESERT_HILL) d += d >> 1;
    else if (t == TILE_MOUNTAIN) d <<= 1;
    if (unit_orders[u] == ORDERS_FORTIFY || city_at(ax, ay) != CITY_NONE)
        d += d >> 1;
    return d;
}

// The unit on (ax, ay) that defends best, or UNIT_NONE.
static byte defender(byte ax, byte ay)
{
    byte u, d, best = UNIT_NONE, best_d = 0;
    for (u = 0; u < MAX_UNITS; ++u) {
        if (unit_kind[u] == KIND_NONE || unit_ax[u] != ax || unit_ay[u] != ay)
            continue;
        d = defence(u);
        if (best == UNIT_NONE || d > best_d) {
            best = u;
            best_d = d;
        }
    }
    return best;
}

byte combat_worth(byte kind, byte ax, byte ay, byte ratio)
{
    byte d = defender(ax, ay);
    if (d == UNIT_NONE) return 1;               // an empty city
    return (word)16 * unit_kind_attack[kind] >= (word)ratio * defence(d);
}

byte player_alive(byte p)
{
    byte c;
    if (p == PLAYER_BARBARIAN) return 1;
    for (c = 0; c < city_count; ++c)
        if (city_owner[c] == p) return 1;
    if (city_founded & (byte)(1 << p)) return 0;
    return unit_count_kind(p, KIND_NONE) != 0;
}

// Out of the game: a computer player's last units go with its last city.
// The human's stay, and game.c ends the game.
static void check_alive(byte p)
{
    byte u;
    if (p == PLAYER_HUMAN || player_alive(p)) return;
    for (u = 0; u < MAX_UNITS; ++u)
        if (unit_kind[u] != KIND_NONE && unit_owner[u] == p) unit_remove(u);
}

static void tell(byte victim, byte what, byte attacker)
{
    if (victim != PLAYER_HUMAN) return;
    news = what;
    news_owner = attacker;
}

byte combat_attack(byte u, byte ax, byte ay)
{
    byte me = unit_owner[u];
    byte d = defender(ax, ay);
    byte a, v, c;

    unit_moves[u] = 0;
    if (d == UNIT_NONE) {
        // Nobody in the city: in, and it is ours -- or ashes.
        c = city_at(ax, ay);
        combat_owner = city_owner[c];
        tell(combat_owner, NEWS_LOST_CITY, me);
        if (me == PLAYER_BARBARIAN) {
            city_destroy(c);
            c = CITY_NONE;
        } else {
            city_capture(c, me);
        }
        combat_city = c;
        unit_move_to(u, ax, ay);
        check_alive(combat_owner);
        return COMBAT_TOOK;
    }

    a = (byte)(4 * unit_kind_attack[unit_kind[u]]);
    v = defence(d);
    if (rnd() % (byte)(a + v) >= a) {
        combat_owner = me;
        combat_kind = unit_kind[u];
        tell(unit_owner[d], NEWS_HELD, me);
        unit_remove(u);
        return COMBAT_LOST;
    }
    combat_owner = unit_owner[d];
    combat_kind = unit_kind[d];
    tell(combat_owner, NEWS_LOST_UNIT, me);
    if (city_at(ax, ay) != CITY_NONE) {
        unit_remove(d);
    } else {
        for (v = 0; v < MAX_UNITS; ++v)
            if (unit_kind[v] != KIND_NONE && unit_ax[v] == ax
                && unit_ay[v] == ay)
                unit_remove(v);
    }
    return COMBAT_WON;
}
