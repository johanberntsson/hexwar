Here is the updated and expanded Game Design Document, integrating the expanded resource framework (staples vs. luxuries) alongside the mechanics and MEGA65 hardware specifications.

---

# Game Design Document: Bronze Age 4X (Working Title)

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
```[cite: 1]

1. **Sea Peoples & Migrations:** Hostile fleets spawn dynamically based on coastal wealth, raiding ports and severing shipping lanes[cite: 1].
2. **Network Fracture:** Loss of critical tin or copper hubs instantly prevents empires from reinforcing elite bronze units[cite: 1].
3. **Environmental Disasters:** Droughts and earthquakes reduce crop yields, causing widespread famine and internal rebellions[cite: 1].

---

## 5. Victory Conditions

Players compete across three victory pathways[cite: 1]:

* **A. Survival (Crisis Endurance):** Maintain a functional state (city control, non-zero treasury, active supply lines) through $N$ turns of the Collapse while rival AI empires fracture into dark-age states[cite: 1].
* **B. Lasting Legacy (Monuments & Culture):** Earn Legacy Points by constructing mega-projects (Pyramids, Palace of Knossos, Lion Gate)[cite: 1] and documenting knowledge on clay tablets (Linear B, Cuneiform law codes, the Alphabet)[cite: 1]. If your score threshold is highest, you win posthumously even if your cities fall during the Collapse[cite: 1].
* **C. Highest Score (Hegemony):** Measured by percentage of strategic trade controlled[cite: 1], total vassal/tributary states[cite: 1], and active wealth/population reserves[cite: 1].

---

## 6. Technical Architecture (MEGA65)

### Memory & Data Structures
Map hex attributes (terrain, features, resources) are packed into single-byte arrays to conserve Fast RAM:

```c
// Hex Resource ID Enum (1 byte per hex)
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

typedef struct {
    uint8_t  civ_id;
    uint16_t legacy_points;      // Accumulated through Monuments & Tech
    uint16_t trade_dominance;    // Active trade route weight
    uint8_t  is_alive;           // 1 = Active, 0 = Collapsed
    uint16_t crisis_survival_turns; // Counter during Collapse phase
} CivVictoryState;
```[cite: 1]

### Visuals & Hardware Specs
* **Hex Engine on VIC-IV:** Staggered odd/even hex rows rendered using 16-color Full-Color Mode (FCM) or custom character matrices[cite: 1]. X/Y hardware pixel scrolling handles map navigation smoothly[cite: 1].
* **CPU Utilization (40.5 MHz GS4510):** Ample processing overhead for A* hex pathfinding, AI trade route logic, and supply line state updates[cite: 1].
* **Memory Footprint (128 KB Fast RAM + Expansion):** Map arrays, unit state tables, trade vectors, and diplomacy matrices fit easily within base fast RAM[cite: 1].

```
