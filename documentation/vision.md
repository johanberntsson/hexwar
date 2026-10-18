Here is the updated and expanded Game Design Document, integrating the expanded resource framework (staples vs. luxuries) alongside the mechanics and MEGA65 hardware specifications.

---

# Game Design Document: Seven Ships of the Enemy (Working Title)

*The title is from the last letter of Ammurapi, king of Ugarit, to the king of
Alashiya (c. 1185 BCE): the seven ships of the enemy that came did great
damage. Ugarit fell soon after.*

**Platform Target:** MEGA65 (GS4510 CPU @ 40.5 MHz, VIC-IV)

**Genre:** Turn-based 4X Strategy (Hex-based, Civ 1 Style)

**Setting:** Eastern Mediterranean Late Bronze Age (~1600–1200 BCE)

---

## 1. High Concept & Core Pitch

A focused, hex-grid strategy game built specifically for the MEGA65. By narrowing the scope to the Late Bronze Age Eastern Mediterranean, the game bypasses thousands of years of tech tree bloat to focus deeply on resource asymmetry, maritime supply networks, palace economies, and systemic collapse.

Rather than standard world conquest, players manage fragile trade monopolies, construct lasting cultural monuments, and prepare for the inevitable late-game **Bronze Age Collapse**.

---

## 2. Resource System & Map Economy

The economy relies on a dual-tier resource model: **Staple Goods** (population growth, army logistics) and **Luxury / Strategic Goods** (metallurgy, prestige, diplomatic currency).

```
                      ┌───────────────────────────────┐
                      │    RESOURCE SYSTEM BREAKDOWN  │
                      └──────────────┬────────────────┘
                                     │
           ┌─────────────────────────┴─────────────────────────┐
           ▼                                                   ▼
┌───────────────────────────┐                       ┌───────────────────────────┐
│     STAPLE RESOURCES      │                       │  LUXURY & STRATEGIC GOODS │
│  (Manpower & Logistics)   │                       │  (Prestige, Tech & Trade) │
├───────────────────────────┤                       ├───────────────────────────┤
│ • Grain (Nile / Valleys)  │                       │ • Copper & Tin (Bronze)   │
│ • Coastal Fishing         │                       │ • Wine & Olive Oil        │
│ • Cattle / Livestock      │                       │ • Cedar Wood & Purple Dye │
└───────────────────────────┘                       └───────────────────────────┘

```

### Strategic & Luxury Goods

* **Copper & Tin (The Bronze Dependency):** Bronze is required for high-tier infrastructure, armored spearmen, and chariots. Copper is localized (Cyprus/Alashiya, Anatolia), while Tin exists in rare edge-of-map nodes (simulating imports from Central Asia or Cornwall). Without both, factions must pay steep tariffs or fight with stone/wood levies.


* **Wine & Olive Oil (Aegean & Levant):** Generated in hillside hexes. Sealed in amphorae, these serve as primary exports for Aegean powers to trade with grain-rich, oil-poor empires like Egypt.


* **Cedar Wood (Levant Coast):** Found in Levant forest hexes. Essential for building ocean-going merchant vessels, war galleys, and palace roofs.


* **Tyrian Purple Dye (Canaanite Coast):** Harvested from murex snails along specialized coastal hexes. Acts as a major trade value booster and generates passive **Legacy Points** when supplied to elite palace classes.



### Staple Resources (Food & Logistics)

* **Coastal Fishing (Universal):** Available on nearshore water hexes across the Mediterranean. Provides steady baseline food yields without land improvements, supporting island settlements and coastal outposts.


* **Grain Agriculture (Egypt & Floodplains):** Concentrated in the Nile Delta and major river valleys. Generates massive food surpluses that can be stored in Palace Granaries to supply distant military campaigns.


* **Livestock / Cattle (Inland Steppes):** Located on interior grass and hill hexes. Provides baseline food and draft animals for heavy transport and chariot production.



---

## 3. Core Gameplay Mechanics

### Maritime Trade & Coastal Emporia

* **Physical Merchant Fleets:** Trade routes are represented by physical fleet units moving across hexes rather than passive abstractions. Ships must be escorted to prevent piracy or interception by hostile coastal powers.


* **Emporia (Outposts):** Factions can establish small coastal trade outposts on islands or foreign shores without founding full cities. These outposts tap into rare resources (tin, cedar) and extend naval supply lines.



### Grain Logistics & Attrition

Armies away from friendly territory consume grain every turn. If supply fleets or land convoys are severed, units suffer severe attrition or desert.

### Asymmetric Civilizations

* **Egypt (New Kingdom):** High agricultural capacity (Nile flooding cycle) and massive manpower, but desperately lacks **Cedar** (for ships) and **Tin** (for bronze).


* **Hittites:** Superior bronze chariot technology, inland-focused heavy land forces, controls key Anatolian copper nodes.


* **Mycenaeans / Minoans:** Maritime thalassocracy. Rich in Wine, Olive Oil, and Fishing, but lacks massive inland grain fields, forcing outward naval trade.


* **Canaanite / Phoenician City-States:** Control Cedar Wood and Purple Dye. Rich middleman states with weak native armies that rely heavily on mercenaries.

### Diplomacy in the Voice of the Letters

All diplomacy is written as clay-tablet correspondence, in the fixed formulas
of the Amarna and Ugarit letters, so the period's voice comes from a handful of
templates rather than from much text.

* **Between Great Kings:** "Say to the King of Mycenae, my brother: thus says the King of Hatti, your brother. For me all goes well. For you, your household, your chariots and your land, may all go well." Equals call each other *brother*; what follows is the request, gift or complaint.
* **From vassals:** "To the king, my lord, say: thus says your servant. At the feet of my lord I fall, seven times and seven times." The forms of address show where each ruler stands, and change when a vassal's standing does.
* **Trade and gifts:** gift exchange is how luxury goods move between courts, and the letters haggle over it ("in my brother's land gold is as plentiful as dust"). The tone of a reply can show how relations stand before any number does.
* **The Crisis:** when the Sea Peoples arrive, the letters change: "My father, behold, the enemy's ships came; my cities were burned. Does not my father know that all my troops and chariots are in the land of Hatti?" Pleas for help that cannot come are how the player learns that the Collapse has begun, and that rival states are falling.

A message is a formula, two names and a few slots (titles, goods, places), so
it costs little memory on the MEGA65.

---

## 4. End Game: The Bronze Age Collapse

Toward the late game, global development triggers the **Crisis Phase (c. 1200 BCE)**, replacing traditional late-game monotony with a survival struggle.

```
┌─────────────────────────────────────────────────────────────────┐
│                    THE CRISIS PHASE (~1200 BCE)                 │
└────────────────────────────────┬────────────────────────────────┘
                                 │
         ┌───────────────────────┼───────────────────────┐
         ▼                       ▼                       ▼
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│  SEA PEOPLES    │     │ NETWORK FRACTURE│     │ SYSTEMIC SHOCKS │
│ Coastal raids & │     │ Tin/Copper hubs │     │ Famine, drought │
│ trade line cuts │     │ fall, halting   │     │ and domestic    │
│                 │     │ bronze builds   │     │ rebellions      │
└─────────────────┘     └─────────────────┘     └─────────────────┘
```

1. **Sea Peoples & Migrations:** Hostile fleets spawn dynamically based on coastal wealth, raiding ports and severing shipping lanes.
2. **Network Fracture:** Loss of critical tin or copper hubs instantly prevents empires from reinforcing elite bronze units.
3. **Environmental Disasters:** Droughts and earthquakes reduce crop yields, causing widespread famine and internal rebellions.

---

## 5. Victory Conditions

Players compete across three victory pathways.

* **A. Survival (Crisis Endurance):** Maintain a functional state (city control, non-zero treasury, active supply lines) through $N$ turns of the Collapse while rival AI empires fracture into dark-age states.
* **B. Lasting Legacy (Monuments & Culture):** Earn Legacy Points by constructing mega-projects (Pyramids, Palace of Knossos, Lion Gate) and documenting knowledge on clay tablets (Linear B, Cuneiform law codes, the Alphabet). If your score threshold is highest, you win posthumously even if your cities fall during the Collapse.
* **C. Highest Score (Hegemony):** Measured by percentage of strategic trade controlled, total vassal/tributary states, and active wealth/population reserves.

---

## 6. Technical Architecture (MEGA65)

The engine this design sits on already exists — `src/view.c`, `src/sheet.c`,
`src/mega65.c` — and `CLAUDE.md` is the authority on how it works. This section
records only the constraints the *game* design has to respect.

### The binding constraint is address space, not RAM

The machine has 384 KB of chip RAM and 8 MB of attic RAM, which is far more
than this game will ever need. What it does **not** have is more than **32 KB
of 16-bit addressable space** — `$2001-$9FFF`, which holds all code, all
initialised data, the BSS and the C stack. That single region is the budget
everything competes for, and it is currently 61.9% full with the display
engine and milestone 1 (input, units, cities, the turn loop) in it (`build/sevenships.lst` is where the real figure is written
down).

| Where | Size | What can live there | Cost to reach |
|---|---|---|---|
| `$2001-$9FFF` | 32 KB | code, hot data, C stack | free (normal C) |
| `$1600-$1EFF` | 2304 B | one or two hot tables (`LOW_FREE`) | free |
| `$5C800-$5FFFF` | 14 KB | warm tables, `__far` (above the expanded tile sheet) | far pointer, a few cycles |
| `$8000000-$87FFFFF` | 8 MB | cold data, save state, per-turn scratch | DMA block, or `__huge` |

`$10000-$1FFFF` is not usable — screen RAM sits at `$12000` and BASIC 65 owns
most of the rest — and `$20000-$3FFFF` is the C65 ROM, which this program
deliberately never banks out (see `sevenships.scm`).

**Consequence for the design:** cheap to add, in this order — more map layers,
more unit records, more tables of any kind, since data goes far or attic almost
for free. Expensive to add: more *code*. Every new subsystem (map generator,
A* pathfinder, trade-route solver, diplomacy, the Crisis phase, save/load, the
city screen) spends the scarce 32 KB. When that region approaches ~26 KB the
project has to adopt Calypsi's MAP-register regions and a dispatch stub
(Dan Sanderson, *Calypsi part 3*): code is then split into 24 KB windows with
`#pragma clang section`, called through a resident trampoline. It works, but it
costs `--no-cross-call`, forces shared data into a common section, and breaks
string literals across regions — so subsystems should each get their own `.c`
file from the start, which is what makes that split a one-line change per file
later.

**The redraw path must never be dispatched.** `view_redraw` runs in 13.6 ms
against a 19.97 ms frame and stays ahead of the raster beam by about 100 lines;
that slack is what stops the map tearing. View, sheet, screen and DMA code stay
resident. Only per-turn code — AI, generation, pathfinding — goes in a banked
region, where a trampoline call is free.

### Map & data structures

The map is 128x32 hexagons in doubled coordinates (`src/map.h`), wrapping
east-west, so **one per-hex layer costs exactly 4096 bytes**. (Not Civ 1's
80x50: in this layout that looks like 40 hexagons across by 100 down, portrait;
128x32 is 64 by 64, about 1.33:1 landscape on screen.) Layers are separate single-byte
arrays rather than an array of structs, so that a layer can be moved far or
attic on its own and so that a scan over one attribute is a linear byte walk.

Planned layers, and where each belongs:

* **Hot** (needed by the redraw, so near or `LOW_FREE`): terrain — `map_tile`,
  which already exists. Its bit 7 already says "a city or unit is here", which
  is all the redraw needs, so a unit-id layer does not have to be hot.
* **Warm** (`__far`, bank 5): resource, feature/improvement, owner, city id,
  unit id. Bank 5 is full now -- the expanded tile sheet, then the
  minimap at `$5E000` -- so these go to attic RAM with the cold
  ones — fine for anything not read during a redraw.
* **Cold** (attic, paged in per turn): AI influence maps, supply/trade
  reachability, fog-of-war per civ.

Ten layers is 40 KB, which is why they cannot all be near, and also why they
comfortably fit once they are not.

```c
// Hex resource id, one byte per hex (one 4096-byte layer)
typedef enum {
    RES_NONE        = 0,
    RES_FISH        = 1, // Coastal fishing
    RES_GRAIN       = 2, // River valley agriculture
    RES_COPPER      = 3, // Cyprus / Anatolia
    RES_TIN         = 4, // Foreign import node
    RES_WINE_OIL    = 5, // Aegean / Hillside groves
    RES_CEDAR       = 6, // Levant timber
    RES_PURPLE_DYE  = 7  // Phoenician coast
} ResourceType;

// Per-civ state. There are at most a handful of civs, so this one stays near.
typedef struct {
    uint8_t  civ_id;
    uint16_t legacy_points;         // Accumulated through Monuments & Tech
    uint16_t trade_dominance;       // Active trade route weight
    uint8_t  is_alive;              // 1 = Active, 0 = Collapsed
    uint16_t crisis_survival_turns; // Counter during Collapse phase
} CivVictoryState;
```

### Visuals

* **Hex engine on the VIC-IV:** 16-bit text mode with **256-colour**
  full-colour characters — each cell is 64 bytes of palette indices, one per
  pixel. Staggered odd/even hexagon rows fall out of the doubled coordinates
  without a special case. Palette index 0 is transparent, which is what lets a
  hexagon be blitted over its neighbour.
* **Navigation is a full redraw, not hardware scrolling.** Every cell of the
  60x23 map window owns 64 bytes of its own in a 92 KB tile store at `$40000`,
  and the screen words never change after boot; moving the view redraws every
  tile that intersects it at the new character offset, in raster-synchronised
  painter's order. Two earlier attempts to shift the store with DMA and repair
  the exposed edge — the cc65 build and the XC=BASIC library — never came out
  correct. **This design does not assume
  smooth pixel scrolling will arrive**; a cursor step is two characters
  (16 pixels) across and one row down — the same distance on screen — and that
  is the granularity the game gets.
* **UI budget:** the map window is 60x23 of an 80x25 screen, from column
  18. The 18-column strip to its left (with the minimap at its top) and the
  two bottom rows are the panel, and it is drawn with
  ordinary ROM glyphs, so panel text is cheap. Anything the design wants to
  show that does not fit there needs a full-screen mode that reuses the tile
  store, not extra screen area.

### CPU

The GS4510 at 40.5 MHz has real headroom for A* hex pathfinding, trade-route
evaluation and supply-line updates, and — unlike the display path — per-turn
work has a whole human-scale pause to run in rather than a 20 ms frame. The
budget that is genuinely tight is the redraw, and it is DMA-bound rather than
CPU-bound. Turn processing should be allowed to take a visible moment; the
frame budget should not be spent on it.
