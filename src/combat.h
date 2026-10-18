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
#ifndef HEXWAR_COMBAT_H
#define HEXWAR_COMBAT_H

#include "mega65.h"

// Every player is at war with the barbarians and they with everyone. Between
// the peoples it is peace until somebody declares war, which for now only
// the human does, by attacking.
byte at_war(byte a, byte b);
void declare_war(byte a, byte b);

// Peace all round, for a new game.
void combat_reset(void);

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

// The last time the human's units or cities were attacked during the other
// players' turns, for the panel: who attacked, and what happened.
#define NEWS_NONE 0
#define NEWS_HELD 1             // the attack failed
#define NEWS_LOST_UNIT 2
#define NEWS_LOST_CITY 3
extern byte news;
extern byte news_owner;

// Whether player p is still in the game: a player who has had a city is out
// once the last one is gone, and one who never founded any once its units
// are. The barbarians are never out.
byte player_alive(byte p);

// A 16-bit xorshift with a fixed seed, which keeps a headless run
// deterministic; nothing here needs better.
byte rnd(void);

#endif // HEXWAR_COMBAT_H
