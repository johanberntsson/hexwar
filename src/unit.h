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

#define MAX_UNITS 64
#define UNIT_NONE 0xff

// Unit kinds. 0 is a free slot, so that a zeroed BSS is an empty list.
#define KIND_NONE 0
#define KIND_SETTLER 1
#define KIND_WARRIOR 2
#define KIND_BUILDER 3

// Standing orders: what a unit does instead of asking for any.
#define ORDERS_NONE 0
#define ORDERS_FORTIFY 1
#define ORDERS_IRRIGATE 2       // a builder at work (src/improve.h)
#define ORDERS_MINE 3

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
// meaningful only with ORDERS_IRRIGATE or ORDERS_MINE.
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

// Whether a unit of `owner` may step onto (ax, ay): not into water, and not
// onto another player's unit or city, which is an attack (combat.h).
#define ENTER_OK 0
#define ENTER_WATER 1
#define ENTER_FOREIGN 2
byte unit_can_enter(byte owner, byte ax, byte ay);

// Set or clear MAP_OCCUPIED on (ax, ay) from what is there now. Anything that
// puts a unit or a city on a hexagon, or takes one off, calls this.
void unit_mark(byte ax, byte ay);

// What view_redraw() draws over the terrain of an occupied hexagon, bottom
// first: the city, then the unit on top -- the active one if it is here,
// unless unit_hidden; in a city, never a fortified one. Writes up to two tile numbers into `tiles` and returns
// how many.
byte unit_overlay(byte ax, byte ay, byte *tiles);

#endif // SEVENSHIPS_UNIT_H
