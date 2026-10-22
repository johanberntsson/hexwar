// Trade (milestone 9, first part): the luxury and strategic goods of
// vision.md section 2 -- olives, wine, cedar, copper, tin and purple dye --
// exchanged between the peoples by letter, as the Amarna rulers exchanged
// gifts.
//
// **A deal** is one player sending a good it owns (player_owns(), growth.h)
// to another, and usually one coming back; with nothing back it is a gift.
// **A good received counts as had** (player_has()), so tin from a trade
// with copper of one's own is bronze. Goods are not used up by sending
// them, as in Civilization IV's resource trades, and only a good a player
// owns can be sent on. One deal a pair, MAX_DEALS in all, **for as long as
// the two are at peace**: a war or a ceasefire ends it, at the next
// trade_turn() -- **and as long as merchant ships carry it** (deal_due[]).
// Partners think better of each other (diplomacy.c).
//
// The computer players ask for what they lack -- copper or tin first, the
// one that makes bronze above all -- offering what the other lacks, and one
// stronger than the other and cold to it demands the good for nothing,
// Civilization's tribute. They answer as their attitude says; the human
// answers by letter, and proposes, or ends a trade, on the Diplomacy
// screen.
//
// **In region 4** (src/region.h), slot 2 with regions 2 and 3: called once
// a turn by diplomacy_turn(), and once for a proposal or a letter.
#ifndef SEVENSHIPS_TRADE_H
#define SEVENSHIPS_TRADE_H

#include "mega65.h"
#include "unit.h"

#define MAX_DEALS 8
#define DEAL_FREE 0xff          // deal_from[] of an empty slot
// Deal i: deal_from[i] sends deal_give[i] to deal_to[i], who sends
// deal_back[i] (RES_NONE for a gift).
extern byte deal_from[MAX_DEALS];
extern byte deal_to[MAX_DEALS];
extern byte deal_give[MAX_DEALS];
extern byte deal_back[MAX_DEALS];
// **Turns until deal i lapses** unless a merchant ship arrives (vision.md's
// merchant fleets, 23 Oct 2026): DEAL_TERM when it is made, and again
// whenever a merchant ship of either partner, loaded at one of its own
// cities (unit_work[] 0), comes next to or into a city of the other --
// or every turn for neighbours, whose cities are OVERLAND_RANGE of each
// other, who trade overland. A lapsed deal ends at trade_turn(). Saved.
#define DEAL_TERM 40
extern byte deal_due[MAX_DEALS];
#define OVERLAND_RANGE 8
// Worked out from the deals: bit r of trade_got[p], p receives resource r;
// bit q of trade_partners[p], p and q trade. Saved with them.
extern word trade_got[PLAYERS];
extern byte trade_partners[PLAYERS];
// What computer player p's LETTER_TRADE or LETTER_TRIBUTE asks of the
// human: the human to send `get`, p to send `give` (RES_NONE in a tribute).
extern byte trade_ask_get[PLAYERS];
extern byte trade_ask_give[PLAYERS];

// No deals. From diplomacy_reset().
void trade_reset(void);

// Once a turn, from diplomacy_turn(): the merchant ships' voyages, deals
// between players no longer at peace or whose ships stopped coming end,
// and the computer players ask for what they want.
void trade_turn(void);

// `from` sends `give` to `to`, who sends `back`: a new deal, 1 if made (0
// if the two trade already or every slot is taken). A gift (back RES_NONE)
// pleases `to` at once.
byte trade_agree(byte from, byte to, byte give, byte back);

// On the Diplomacy screen, row `row`: the human proposes a trade or a
// gift to p -- or, if they trade already, ends it -- choosing the goods
// with the number keys. 1 if something was sent, and the answer is then a
// letter waiting (LETTER_YES or LETTER_NO); 0 if the human thought better
// of it.
byte trade_propose(byte p, byte row);

#endif // SEVENSHIPS_TRADE_H
