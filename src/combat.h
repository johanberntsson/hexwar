// Combat, war and peace, and the end of a player (milestone 4).
//
// **Civilization's rule**: an attack is the attacker's attack strength
// against the defender's defence, and the attacker wins with probability
// attack / (attack + defence). The loser dies -- outside a city the whole
// stack on the defender's hexagon with it, inside one only the defender.
// Defence is worth half as much again on hills, twice on mountains, and half
// as much again to a unit that is fortified or in a city. An attack takes
// the attacker's whole turn, and a winner stays where it is; it takes an
// enemy city by walking into it once nobody is left to defend it.
#ifndef SEVENSHIPS_COMBAT_H
#define SEVENSHIPS_COMBAT_H

#include "mega65.h"
#include "unit.h"

// Every player is at war with the barbarians and they with everyone. Between
// the peoples it is peace until somebody declares war: declare_war() and
// the rest of diplomacy are in diplomacy.h.
byte at_war(byte a, byte b);

// Peace all round, for a new game.
void combat_reset(void);

// Bit q of combat_war[p]: p is at war with q. And the state of rnd(). For
// save.c; everything else goes through at_war() and rnd().
extern byte combat_war[PLAYERS];
extern word combat_seed;

// Who holds (ax, ay): the owner of the unit or city there.
byte combat_holder(byte ax, byte ay);

// Unit u attacks (ax, ay), a neighbour held by an enemy: a battle with the
// best defender there, or, with nobody left in an enemy city, u walks in and
// takes it -- or burns it, for a barbarian. Uses all of u's moves.
#define COMBAT_LOST 0
#define COMBAT_WON 1
#define COMBAT_TOOK 2           // the city at (ax, ay); combat_city says which
byte combat_attack(byte u, byte ax, byte ay);

// The other side of the last combat_attack(): the loser's owner and kind
// when it was a battle, the city's previous owner when it was taken, and
// the city (CITY_NONE if it was burnt).
extern byte combat_owner;
extern byte combat_kind;
extern byte combat_city;

// Whether the defence of (ax, ay) is worth attacking for a unit of `kind`:
// odds of at least `ratio` / 4 attack to defence.
byte combat_worth(byte kind, byte ax, byte ay, byte ratio);

// **The fights the human's units and cities were in**, since they were
// last shown, for fight_show() (fight.h), which game.c calls for each in
// turn with the view on the hexagon. Up to FIGHTS of them; not noted
// during Play 50 Turns. fight_kind is the human's unit, fight_foe and
// fight_foe_kind the other side's, fight_lost how many of the losing side
// died (a stack outside a city), and fight_founder and fight_name the city
// on the hexagon (fight_founder CITY_NONE for none), kept by name since a
// burnt one is gone.
#define FIGHTS 8
#define FIGHT_WON 0             // the human attacked and won
#define FIGHT_LOST 1            // ... and lost
#define FIGHT_TOOK 2            // ... and took an empty city
#define FIGHT_HELD 3            // the human was attacked and won
#define FIGHT_FELL 4            // ... and lost
#define FIGHT_CITY 5            // ... and an empty city was taken or burnt
extern byte fight_count;
extern byte fight_what[FIGHTS];
extern byte fight_ax[FIGHTS], fight_ay[FIGHTS];
extern byte fight_kind[FIGHTS];
extern byte fight_foe[FIGHTS], fight_foe_kind[FIGHTS];
extern byte fight_lost[FIGHTS];
extern byte fight_founder[FIGHTS], fight_name[FIGHTS];

// Whether player p is still in the game: a player who has had a city is out
// once the last one is gone, and one who never founded any once its units
// are. The barbarians are never out.
byte player_alive(byte p);

// A 16-bit xorshift with a fixed seed, which keeps a headless run
// deterministic; nothing here needs better.
byte rnd(void);

#endif // SEVENSHIPS_COMBAT_H
