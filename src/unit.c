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
HIGH_BSS byte unit_work[MAX_UNITS];
byte unit_active = UNIT_NONE;
byte unit_hidden;

// The spearman is Civilization's phalanx, the galley its trireme, the
// fishing boat Civilization IV's work boat.
const char *const unit_kind_name[] = {
    "", "Settler", "Warrior", "Builder", "Spearman", "Galley", "Fishing Boat"
};
const byte unit_kind_moves[] = { 0, 1, 1, 1, 1, 3, 2 };
const byte unit_kind_cost[] = { 0, 30, 10, 20, 20, 40, 20 };
const byte unit_kind_attack[] = { 0, 0, 1, 0, 1, 1, 0 };
const byte unit_kind_defence[] = { 0, 1, 1, 1, 2, 1, 1 };
static const byte unit_kind_tile[] = {
    0, TILE_SETTLER, TILE_WARRIOR, TILE_BUILDER, TILE_SPEARMAN, TILE_GALLEY,
    TILE_FISHING_BOAT
};

#define IS_WATER(T) ((T) == TILE_OCEAN || (T) == TILE_SHALLOW_OCEAN)

static byte water(byte ax, byte ay)
{
    byte t = MAP_TERRAIN(ax, ay);
    return IS_WATER(t);
}

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
            unit_work[u] = 0;
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
    byte old_x = unit_ax[u], old_y = unit_ay[u], v, n = 0;

    // A galley takes those aboard with it, from the water; in a city they
    // stay. A land unit stepping onto the water is aboard, and sentried.
    if (unit_kind[u] == KIND_GALLEY) {
        if (water(old_x, old_y))
            for (v = 0; v < MAX_UNITS && n < GALLEY_HOLD; ++v)
                if (unit_kind[v] != KIND_NONE && !KIND_IS_SHIP(unit_kind[v])
                    && unit_ax[v] == old_x && unit_ay[v] == old_y
                    && unit_owner[v] == unit_owner[u]) {
                    unit_ax[v] = ax;
                    unit_ay[v] = ay;
                    ++n;
                }
    } else if (!KIND_IS_SHIP(unit_kind[u]) && water(ax, ay)) {
        unit_orders[u] = ORDERS_FORTIFY;
    }
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

byte unit_count_kind(byte owner, byte kind)
{
    byte u, n = 0;
    for (u = 0; u < MAX_UNITS; ++u)
        if (unit_kind[u] != KIND_NONE && unit_owner[u] == owner
            && (kind == KIND_NONE || unit_kind[u] == kind))
            ++n;
    return n;
}

void unit_wake_cargo(byte u)
{
    byte v, dir, nx, ny, x = unit_ax[u], y = unit_ay[u];

    if (unit_kind[u] != KIND_GALLEY || !water(x, y)) return;
    for (dir = 0; dir < 6; ++dir)
        if (map_neighbour(x, y, dir, &nx, &ny) && !water(nx, ny)) break;
    if (dir == 6) return;
    for (v = 0; v < MAX_UNITS; ++v)
        if (unit_kind[v] != KIND_NONE && !KIND_IS_SHIP(unit_kind[v])
            && unit_ax[v] == x && unit_ay[v] == y
            && unit_orders[v] == ORDERS_FORTIFY)
            unit_orders[v] = ORDERS_NONE;
}

byte unit_can_enter(byte owner, byte kind, byte ax, byte ay)
{
    byte t = map_tile[ay][ax];
    byte terrain = t & MAP_TERRAIN_MASK;
    byte c, u, galleys = 0, aboard = 0;

    c = city_at(ax, ay);
    if (KIND_IS_SHIP(kind)) {
        if (terrain != TILE_SHALLOW_OCEAN
            && (c == CITY_NONE || city_owner[c] != owner))
            return ENTER_LAND;
    } else if (IS_WATER(terrain)) {
        // Aboard its own galley, if there is room; never onto a foreign
        // ship, which a land unit cannot attack.
        for (u = 0; u < MAX_UNITS; ++u)
            if (unit_kind[u] != KIND_NONE && unit_ax[u] == ax
                && unit_ay[u] == ay) {
                if (unit_owner[u] != owner) return ENTER_WATER;
                if (unit_kind[u] == KIND_GALLEY) galleys += GALLEY_HOLD;
                else if (!KIND_IS_SHIP(unit_kind[u])) ++aboard;
            }
        return aboard < galleys ? ENTER_BOARD : ENTER_WATER;
    }
    if (!(t & MAP_OCCUPIED)) return ENTER_OK;
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

byte unit_overlay_owner;

byte unit_overlay(byte ax, byte ay, byte *tiles)
{
    byte n = 0, v;
    byte u = unit_at(ax, ay);
    byte c = city_at(ax, ay);
    // In a city a fortified unit is not drawn, so that the city can be
    // seen; the city screen lists it. The active unit always is.
    if (c != CITY_NONE) {
        tiles[n++] = TILE_CITY;
        unit_overlay_owner = city_owner[c];
        if (u != unit_active) {
            u = UNIT_NONE;
            for (v = 0; v < MAX_UNITS; ++v)
                if (unit_kind[v] != KIND_NONE && unit_ax[v] == ax
                    && unit_ay[v] == ay && unit_orders[v] != ORDERS_FORTIFY)
                    u = v;
        }
    }
    // At sea the galley is drawn, not those aboard, unless one of them
    // has the orders.
    if (c == CITY_NONE && u != UNIT_NONE && u != unit_active
        && !KIND_IS_SHIP(unit_kind[u]) && water(ax, ay))
        for (v = 0; v < MAX_UNITS; ++v)
            if (unit_kind[v] == KIND_GALLEY && unit_ax[v] == ax
                && unit_ay[v] == ay)
                u = v;
    if (u != UNIT_NONE && !(u == unit_active && unit_hidden)) {
        tiles[n++] = unit_kind_tile[unit_kind[u]];
        unit_overlay_owner = unit_owner[u];
    }
    return n;
}
