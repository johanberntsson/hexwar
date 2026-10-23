// Units: settlers, warriors and builders, and which one is waiting for
// orders.
//
// **One array per field, not an array of structs.** Calypsi indexes a byte
// array with a byte index as `lda abs,x`, and every field here is a byte, so
// unit_ax[u] is one instruction where units[u].ax would be a multiply and a
// pointer. It is also the shape vision.md §6 wants for the map layers.
#ifndef SEVENSHIPS_UNIT_H
#define SEVENSHIPS_UNIT_H

#include "mega65.h"

// 96, not 64, since milestone 10 (22 Oct 2026): by the Crisis three
// computer players, the human and the barbarians had filled 64, and the
// Sea Peoples could not come. The redraw's unit_at() walks the list for
// every occupied hexagon in view: 0.13 ms more in a busy view.
#define MAX_UNITS 96
#define UNIT_NONE 0xff

// Unit kinds. 0 is a free slot, so that a zeroed BSS is an empty list.
#define KIND_NONE 0
#define KIND_SETTLER 1
#define KIND_WARRIOR 2
#define KIND_BUILDER 3
#define KIND_SPEARMAN 4         // bronze: needs copper and tin (growth.h)
#define KIND_RAIDER 5           // the Sea Peoples' only, never built (crisis.c)
#define KIND_CHARIOT 6          // horses; two moves, never onto mountains
#define KIND_GALLEY 7           // cedar and a coastal city; carries GALLEY_HOLD
#define KIND_BOAT 8             // a fishing boat: a coastal city; carries nobody
#define KIND_MERCHANT 9         // a coastal city; founds an emporium (emporium.h)
#define KINDS 10
// **Buildings follow the units** (src/culture.h): what a city can build
// is a kind, so city_build[] and the CHANGE list take them as they are,
// with a name and a cost in unit_kind_name[] and unit_kind_cost[] -- and
// nothing else of the units' tables, since nothing ever creates one.
#define KIND_TEMPLE 10
#define KIND_LION_GATE 11       // a wonder: one in the world
#define BUILD_KINDS 12
#define KIND_IS_BUILDING(K) ((K) >= KIND_TEMPLE)

// **The ships are the kinds from KIND_GALLEY on**: they sail the shallows
// and their own cities, and everything else is a land unit, which on the
// water is aboard a galley. So a new land unit goes before KIND_GALLEY.
#define KIND_IS_SHIP(K) ((K) >= KIND_GALLEY)

// **A galley** (milestone 9, Civilization's trireme) sails the shallows
// and its own cities, never the open sea or land, and carries GALLEY_HOLD
// land units: a land unit on a water hexagon is aboard the galley there.
// It boards by stepping onto the galley, is sentried (ORDERS_FORTIFY) as
// it does, goes wherever the galley goes from the water, and is woken by
// unit_wake_cargo() when the galley stops next to land.
#define GALLEY_HOLD 2

// **A fishing boat** (milestone 9; Civilization IV's work boat) sails as a
// galley does but carries nobody: it needs no cedar, and the computer
// players build one early to explore the coasts (boat_explore(), boat.h),
// which is how distant peoples meet before anyone has a galley.

// Standing orders: what a unit does instead of asking for any.
#define ORDERS_NONE 0
#define ORDERS_FORTIFY 1
#define ORDERS_IRRIGATE 2       // a builder at work (src/improve.h)
#define ORDERS_MINE 3
#define ORDERS_PASTURE 4        // MAP_IRRIGATED on cattle or horses

// The players: the human, and a computer player for each of the other
// peoples (src/ai.c). Player p leads people player_civ[p] (civ.h).
#define PLAYER_HUMAN 0
#define PLAYER_FIRST_AI 1
#define PLAYERS 4

// The barbarians own units like a player, but no cities, and are at war
// with everybody (combat.h). Not counted in PLAYERS, which is the peoples.
#define PLAYER_BARBARIAN PLAYERS

extern byte unit_kind[MAX_UNITS];
extern byte unit_owner[MAX_UNITS];
extern byte unit_ax[MAX_UNITS];
extern byte unit_ay[MAX_UNITS];
extern byte unit_moves[MAX_UNITS];      // left this turn
extern byte unit_orders[MAX_UNITS];
// Turns a builder has worked on its improvement; set by improve_start(),
// meaningful only with ORDERS_IRRIGATE, ORDERS_MINE or ORDERS_PASTURE.
extern byte unit_work[MAX_UNITS];

// The unit waiting for orders, which view_redraw() draws on top of anything
// else on its hexagon. UNIT_NONE between units and between turns.
extern byte unit_active;

// Set for the half of the blink in which the active unit is not drawn: its
// hexagon shows the terrain and any city. game_run() flips it twice a second
// and clears it whenever anything else redraws the view.
extern byte unit_hidden;

// Per kind, indexed by KIND_*.
extern const char *const unit_kind_name[];
extern const byte unit_kind_moves[];
extern const byte unit_kind_cost[];     // shields to build one
extern const byte unit_kind_attack[];   // 0: cannot attack
extern const byte unit_kind_defence[];
extern const byte unit_kind_tile[];      // TILE_* (help.c draws it too)

// No units at all, for a new game.
void unit_reset(void);

// A new unit, with a full turn's moves. UNIT_NONE if the list is full.
byte unit_create(byte kind, byte owner, byte ax, byte ay);
void unit_remove(byte u);
void unit_move_to(byte u, byte ax, byte ay);

// Give every unit of `owner` its moves back. Fortified units keep their
// orders and so are never offered for new ones.
void unit_begin_turn(byte owner);

// The next unit of `owner` after `after` (cyclically, and `after` itself
// last) that has moves left and no standing orders, or UNIT_NONE. Pass
// UNIT_NONE to start from the beginning.
byte unit_next(byte owner, byte after);

// How many units of `kind` `owner` has; KIND_NONE counts all of them.
byte unit_count_kind(byte owner, byte kind);

// The unit drawn on (ax, ay) -- the active one if it is there, otherwise the
// last in the list -- or UNIT_NONE.
byte unit_at(byte ax, byte ay);

// Whether a unit of `kind` and `owner` may step onto (ax, ay): a land unit
// not into water but onto its own galley with room (ENTER_BOARD), a ship
// only onto the shallows or its own city, a chariot never onto mountains,
// not even to attack; and not onto another player's unit or city, which
// is an attack (combat.h) -- except a land unit's onto a ship.
#define ENTER_OK 0
#define ENTER_WATER 1           // a land unit, and no galley to board
#define ENTER_FOREIGN 2
#define ENTER_BOARD 3
#define ENTER_LAND 4            // a ship, and land or the open sea
#define ENTER_MOUNTAIN 5        // a chariot, and mountains
byte unit_can_enter(byte owner, byte kind, byte ax, byte ay);

// Galley u has stopped: wake the units aboard if it is next to land.
void unit_wake_cargo(byte u);

// Set or clear MAP_OCCUPIED on (ax, ay) from what is there now. Anything that
// puts a unit or a city on a hexagon, or takes one off, calls this.
void unit_mark(byte ax, byte ay);

// What view_redraw() draws over the terrain of an occupied hexagon, bottom
// first: the city, then the unit on top -- the active one if it is here,
// unless unit_hidden; in a city, never a fortified one. Writes up to two tile numbers into `tiles` and returns
// how many, and leaves whose they are in unit_overlay_owner for the flag
// (view_draw_flag()).
byte unit_overlay(byte ax, byte ay, byte *tiles);
extern byte unit_overlay_owner;

#endif // SEVENSHIPS_UNIT_H
