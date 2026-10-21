// Diplomacy (milestone 8): contact between the peoples, war, ceasefire and
// peace, the attitudes behind them, the letters, and the Diplomacy screen
// (Advisors on the menu bar).
//
// **A people is met when one of its units or cities is next to one of the
// other's units or cities**, as in Civilization: for the human by
// diplomacy_contact() after each of its moves and at the end of every
// turn, and between everybody once a turn by diplomacy_turn(). Met stays
// met.
//
// **War is combat_war** (combat.h), which at_war() reads; a ceasefire is
// peace with diplomacy_truce counting down to war again, and peace is
// peace with it 0. The computer players declare war, ask for ceasefires
// and offer peace by Civilization's rules (diplomacy_turn() has them),
// and the human hears of it, and answers, by letter in the formulas of
// the Amarna letters (vision.md section 3, res/text/letter_*.txt), in
// popups over the map. The human proposes on the Diplomacy screen.
//
// **In region 3** (src/region.h), in slot 2 with region 2, which does not
// call it.
#ifndef SEVENSHIPS_DIPLOMACY_H
#define SEVENSHIPS_DIPLOMACY_H

#include "mega65.h"
#include "unit.h"

// Bit q of diplomacy_known[p]: p has met q, and q p. The human's row is
// who it has met.
extern byte diplomacy_known[PLAYERS];
// Between p and q, the same both ways round: at war, the turns of the war
// (to 255); in a ceasefire, the turns left; at peace, 0.
extern byte diplomacy_truce[PLAYERS][PLAYERS];
// How p feels about q, around 100; diplomacy.c has the rules.
extern byte diplomacy_mood[PLAYERS][PLAYERS];

// Nobody has met anybody, everybody at their temper. game_init(), after
// combat_reset() and once the peoples are chosen.
void diplomacy_reset(void);

#define RELATION_WAR 0
#define RELATION_CEASEFIRE 1
#define RELATION_PEACE 2
byte diplomacy_relation(byte p, byte q);

// Note any people newly next to the human's units or cities, or met by
// them during diplomacy_turn(), and return them as a bitmask (bit p), 0 if
// none. diplomacy_ax/ay are then where one of them is, for the caller to
// bring into view before diplomacy_greet() says who they are.
byte diplomacy_contact(void);
extern byte diplomacy_ax, diplomacy_ay;

// Once a turn, after the computer players: contact between everybody, the
// attitudes, the ceasefires, and the computer players' declarations and
// offers. Leaves letters for the human, and diplomacy_waiting set.
void diplomacy_turn(void);

// Player a declares war on player b: war, and b and everybody who knows a
// think the worse of it. A computer player tells the human by letter.
void declare_war(byte a, byte b);

// Letters are waiting for diplomacy_greet().
extern byte diplomacy_waiting;

// A popup over the map for each people in `met`, a diplomacy_contact()
// bitmask: "You have met the Hittites ...", and then the letters waiting,
// the offers among them answered Yes or No.
void diplomacy_greet(byte met);

// The Diplomacy screen until the player leaves. Writes over the whole
// screen and hides sprites 2 and 3, like the help screen, so **the caller
// puts the map screen back afterwards**, and then diplomacy_greet() reads
// the answer to whatever was proposed.
void diplomacy_screen(void);

#ifdef DEBUG_MENU
// Set while the computer plays the human's side (Play 50 Turns): offers to
// the human are answered for it, and nothing is written to it.
extern byte diplomacy_auto;
#endif

#endif // SEVENSHIPS_DIPLOMACY_H
