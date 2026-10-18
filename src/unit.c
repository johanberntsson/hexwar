#include "unit.h"
#include "city.h"
#include "map.h"
#include "minimap.h"
#include "sheet.h"

// In HIGH_BSS, out of the 32 KB -- and so not cleared by the startup:
// unit_reset() empties the list for every game.
HIGH_BSS byte unit_kind[MAX_UNITS];
HIGH_BSS byte unit_owner[MAX_UNITS];
HIGH_BSS byte unit_ax[MAX_UNITS];
HIGH_BSS byte unit_ay[MAX_UNITS];
HIGH_BSS byte unit_moves[MAX_UNITS];
HIGH_BSS byte unit_orders[MAX_UNITS];
byte unit_active = UNIT_NONE;

const char *const unit_kind_name[] = { "", "Settler", "Warrior", "Builder" };
const byte unit_kind_moves[] = { 0, 1, 1, 1 };
const byte unit_kind_cost[] = { 0, 30, 10, 20 };
static const byte unit_kind_tile[] = {
    0, TILE_SETTLER, TILE_WARRIOR, TILE_BUILDER
};

void unit_reset(void)
{
    byte u;
    for (u = 0; u < MAX_UNITS; ++u) unit_kind[u] = KIND_NONE;
    unit_active = UNIT_NONE;
}

byte unit_create(byte kind, byte owner, byte ax, byte ay)
{
    byte u;
    for (u = 0; u < MAX_UNITS; ++u) {
        if (unit_kind[u] == KIND_NONE) {
            unit_kind[u] = kind;
            unit_owner[u] = owner;
            unit_ax[u] = ax;
            unit_ay[u] = ay;
            unit_moves[u] = unit_kind_moves[kind];
            unit_orders[u] = ORDERS_NONE;
            unit_mark(ax, ay);
            if (owner == PLAYER_HUMAN) map_explore(ax, ay);
            return u;
        }
    }
    return UNIT_NONE;
}

void unit_remove(byte u)
{
    unit_kind[u] = KIND_NONE;
    if (unit_active == u) unit_active = UNIT_NONE;
    unit_mark(unit_ax[u], unit_ay[u]);
}

void unit_move_to(byte u, byte ax, byte ay)
{
    byte old_x = unit_ax[u], old_y = unit_ay[u];
    unit_ax[u] = ax;
    unit_ay[u] = ay;
    unit_mark(old_x, old_y);
    unit_mark(ax, ay);
    if (unit_owner[u] == PLAYER_HUMAN) map_explore(ax, ay);
}

void unit_begin_turn(byte owner)
{
    byte u;
    for (u = 0; u < MAX_UNITS; ++u) {
        if (unit_kind[u] != KIND_NONE && unit_owner[u] == owner)
            unit_moves[u] = unit_kind_moves[unit_kind[u]];
    }
}

byte unit_next(byte owner, byte after)
{
    byte u = (after == UNIT_NONE) ? MAX_UNITS - 1 : after;
    byte n;
    for (n = 0; n < MAX_UNITS; ++n) {
        if (++u == MAX_UNITS) u = 0;
        if (unit_kind[u] != KIND_NONE && unit_owner[u] == owner
            && unit_moves[u] && unit_orders[u] == ORDERS_NONE)
            return u;
    }
    return UNIT_NONE;
}

byte unit_count(byte owner)
{
    byte u, n = 0;
    for (u = 0; u < MAX_UNITS; ++u)
        if (unit_kind[u] != KIND_NONE && unit_owner[u] == owner) ++n;
    for (u = 0; u < city_count; ++u)
        if (city_owner[u] == owner) ++n;
    return n;
}

byte unit_count_kind(byte owner, byte kind)
{
    byte u, n = 0;
    for (u = 0; u < MAX_UNITS; ++u)
        if (unit_kind[u] != KIND_NONE && unit_owner[u] == owner
            && (kind == KIND_NONE || unit_kind[u] == kind))
            ++n;
    return n;
}

byte unit_can_enter(byte owner, byte ax, byte ay)
{
    byte t = map_tile[ay][ax];
    byte terrain = t & MAP_TERRAIN_MASK;
    byte c, u;

    if (terrain == TILE_OCEAN || terrain == TILE_SHALLOW_OCEAN)
        return ENTER_WATER;
    if (!(t & MAP_OCCUPIED)) return ENTER_OK;
    c = city_at(ax, ay);
    if (c != CITY_NONE && city_owner[c] != owner) return ENTER_FOREIGN;
    for (u = 0; u < MAX_UNITS; ++u)
        if (unit_kind[u] != KIND_NONE && unit_ax[u] == ax && unit_ay[u] == ay
            && unit_owner[u] != owner)
            return ENTER_FOREIGN;
    return ENTER_OK;
}

byte unit_at(byte ax, byte ay)
{
    byte u, top = UNIT_NONE;
    if (unit_active != UNIT_NONE && unit_ax[unit_active] == ax
        && unit_ay[unit_active] == ay)
        return unit_active;
    for (u = 0; u < MAX_UNITS; ++u) {
        if (unit_kind[u] != KIND_NONE && unit_ax[u] == ax && unit_ay[u] == ay)
            top = u;
    }
    return top;
}

void unit_mark(byte ax, byte ay)
{
    if (unit_at(ax, ay) != UNIT_NONE || city_at(ax, ay) != CITY_NONE)
        map_tile[ay][ax] |= MAP_OCCUPIED;
    else
        map_tile[ay][ax] &= (byte)~MAP_OCCUPIED;
    minimap_plot(ax, ay);
}

byte unit_overlay(byte ax, byte ay, byte *tiles)
{
    byte n = 0;
    byte u = unit_at(ax, ay);
    if (city_at(ax, ay) != CITY_NONE) tiles[n++] = TILE_CITY;
    if (u != UNIT_NONE) {
        if (u == unit_active) tiles[n++] = TILE_SELECT;
        tiles[n++] = unit_kind_tile[unit_kind[u]];
    }
    return n;
}
