#include "combat.h"
#include "culture.h"
#include "city.h"
#include "diplomacy.h"
#include "map.h"
#include "sheet.h"
#include "unit.h"

// Bit q of combat_war[p]: p is at war with q. Kept both ways round.
byte combat_war[PLAYERS];

byte combat_owner;
byte combat_kind;
byte combat_city;
byte fight_count;
HIGH_BSS byte fight_what[FIGHTS];
HIGH_BSS byte fight_ax[FIGHTS], fight_ay[FIGHTS];
HIGH_BSS byte fight_kind[FIGHTS];
HIGH_BSS byte fight_foe[FIGHTS], fight_foe_kind[FIGHTS];
HIGH_BSS byte fight_lost[FIGHTS];
HIGH_BSS byte fight_founder[FIGHTS], fight_name[FIGHTS];

// Where the attack of the moment is, and how many died, for note().
static byte at_x, at_y, lost;

word combat_seed = 0x2a6d;

byte rnd(void)
{
    combat_seed ^= combat_seed << 7;
    combat_seed ^= combat_seed >> 9;
    combat_seed ^= combat_seed << 8;
    return (byte)combat_seed;
}

byte at_war(byte a, byte b)
{
    if (a == PLAYER_BARBARIAN || b == PLAYER_BARBARIAN) return 1;
    return combat_war[a] & (byte)(1 << b);
}

void combat_reset(void)
{
    byte p;
    for (p = 0; p < PLAYERS; ++p) combat_war[p] = 0;
    fight_count = 0;
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
    byte d = (byte)(4 * unit_kind_defence[unit_kind[u]]), c;

    if (t == TILE_HILLS || t == TILE_FOREST) d += d >> 1;
    else if (t == TILE_MOUNTAIN) d <<= 1;
    c = city_at(ax, ay);
    if (unit_orders[u] == ORDERS_FORTIFY || c != CITY_NONE)
        d += d >> 1;
    // The Lion Gate is the walls of its city (src/culture.h).
    if (c != CITY_NONE && (city_culture[c] & CITY_LION_GATE)) d <<= 1;
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

// A fight of the human's, for fight_show(): `kind` the human's unit, `foe`
// and `foe_kind` the other side. Before a city changes hands, so that it
// is still there to be named.
static void note(byte what, byte kind, byte foe, byte foe_kind)
{
    byte i = fight_count, c;
#ifdef DEBUG_MENU
    if (diplomacy_auto) return;
#endif
    if (i == FIGHTS) return;
    fight_what[i] = what;
    fight_ax[i] = at_x;
    fight_ay[i] = at_y;
    fight_kind[i] = kind;
    fight_foe[i] = foe;
    fight_foe_kind[i] = foe_kind;
    fight_lost[i] = lost;
    c = city_at(at_x, at_y);
    fight_founder[i] = c;
    if (c != CITY_NONE) {
        fight_founder[i] = city_founder[c];
        fight_name[i] = city_name[c];
    }
    fight_count = (byte)(i + 1);
}

byte combat_attack(byte u, byte ax, byte ay)
{
    byte me = unit_owner[u], kind = unit_kind[u];
    byte d = defender(ax, ay);
    byte a, v, c, o, dk;

    unit_moves[u] = 0;
    at_x = ax;
    at_y = ay;
    lost = 1;
    if (d == UNIT_NONE) {
        // Nobody in the city: in, and it is ours -- or ashes.
        c = city_at(ax, ay);
        o = city_owner[c];
        combat_owner = o;
        if (me == PLAYER_HUMAN) note(FIGHT_TOOK, kind, o, KIND_NONE);
        else if (o == PLAYER_HUMAN) note(FIGHT_CITY, KIND_NONE, me, kind);
        if (me == PLAYER_BARBARIAN) {
            city_destroy(c);
            c = CITY_NONE;
        } else {
            city_capture(c, me);
        }
        combat_city = c;
        unit_move_to(u, ax, ay);
        check_alive(o);
        return COMBAT_TOOK;
    }

    a = (byte)(4 * unit_kind_attack[kind]);
    v = defence(d);
    o = unit_owner[d];
    dk = unit_kind[d];
    if (rnd() % (byte)(a + v) >= a) {
        combat_owner = me;
        combat_kind = kind;
        if (me == PLAYER_HUMAN) note(FIGHT_LOST, kind, o, dk);
        else if (o == PLAYER_HUMAN) note(FIGHT_HELD, dk, me, kind);
        unit_remove(u);
        return COMBAT_LOST;
    }
    combat_owner = o;
    combat_kind = dk;
    if (city_at(ax, ay) == CITY_NONE) {
        lost = 0;
        for (v = 0; v < MAX_UNITS; ++v)
            if (unit_kind[v] != KIND_NONE && unit_ax[v] == ax
                && unit_ay[v] == ay)
                ++lost;
    }
    if (me == PLAYER_HUMAN) note(FIGHT_WON, kind, o, dk);
    else if (o == PLAYER_HUMAN) note(FIGHT_FELL, dk, me, kind);
    if (lost == 1) {
        unit_remove(d);
    } else {
        for (v = 0; v < MAX_UNITS; ++v)
            if (unit_kind[v] != KIND_NONE && unit_ax[v] == ax
                && unit_ay[v] == ay)
                unit_remove(v);
    }
    return COMBAT_WON;
}
