# Phase 3 — Remake additions (absent from Alundra 2)

Updated 2026-11-21. Phase 3 plan and specification archived locally.

## 1. What remains of the Alundra 2 content (unchanged)

- Story, characters, locations, the `epee_supreme`, `lampe`, `prisme`, `boss_medusa` and `cristal` gates, the A → Z route, Méphisto and the Mont Épervier boss.
- The six A2 swords (`long_sword` … `rising_sword`), their powers 1..6, and all the other A2 entries in `data/items.txt` (IDs, types, quantities and powers unchanged; only the displayed names and descriptions were rendered in French and stripped of their working notes).
- The A2 key item `hammer` remains distinct from the remake hammer (`rm_rift_hammer`); the validator forbids any collision.

## 2. Alundra 1 presentation (reused, not reinvented)

Font `FONT3.TIM`, window `WIND.TX`/`WIND.CL` (parchment at 1:1 scale), CLUT 6 ink. The forge page is drawn on this native parchment.

## 3. Remake additions

All IDs begin with `rm_`. Displayed names and descriptions are in `data/items.txt` (section "Remake additions"), statistics in `data/rm_*.txt`.

### Weapons (`data/rm_weapons.txt`)

| id | Name | Damage | Window (ticks) | Range (px) | Knockback | Charged attack EP | Effect |
|---|---|---|---|---|---|---|---|
| `rm_ember_blade` | Épée de braise | 3 | 14 | 20 | 2 | 6 | fire |
| `rm_frost_lance` | Lance de givre | 2 | 11 | 28 | 1 | 10 | ice |
| `rm_rift_hammer` | Marteau de faille | 5 | 18 | 16 | 5 | 4 | heavy |

Generic effects (no per-weapon code):
- **fire**: splash damage, 1 damage to enemies adjacent to the target; lights the brazier.
- **ice**: slows the enemy hit (90 ticks, 150 in the zone); freezes the source.
- **heavy**: shake, knockback, breaks the frontal guard of turtles; activates every puzzle object.
- **Charged strike** with a remake weapon: if energy is sufficient (EP cost), a 40 px elemental wave that hits enemies and puzzle objects.

### Materials (`data/rm_materials.txt`)

`rm_ember_shard` (Éclat de braise), `rm_frost_core` (Cœur de givre), `rm_rift_fragment` (Fragment de faille), `rm_ironwood` (Bois de fer), `rm_rune_dust` (Poussière runique). Type `Key`: never consumed by the menu, only by the forge. `"objet:n"` chests are supported.

### Orvan's forge (`data/rm_forge.txt`)

Orvan is an **original** remake character (Paco, tile 36,47). First encounter: he hands over the Marteau de faille. Afterwards, his dialogue opens the forge page.

| Recipe | Weapon | Cost | After | Trade-off |
|---|---|---|---|---|
| `rm_ember_blade_flame_1` Lame ardente | Épée de braise | 2 éclats de braise, 1 bois de fer | damage 5, range 16 | reduced range |
| `rm_ember_blade_ember_1` Braise vive | Épée de braise | 1 éclat de braise, 1 poussière runique | damage 4, EP 12 | higher EP cost |
| `rm_frost_lance_glacier_1` Lance glaciaire | Lance de givre | 2 cœurs de givre, 1 bois de fer | damage 4, EP 14 | higher EP cost |
| `rm_frost_lance_shard_1` Pointe d'éclat | Lance de givre | 1 cœur de givre, 1 poussière runique | range 34, knockback 4 | increased knockback |
| `rm_rift_hammer_quake_1` Marteau sismique | Marteau de faille | 2 fragments de faille, 1 bois de fer | damage 7, window 20 | reduced speed |
| `rm_rift_hammer_shatter_1` Marteau brise-roc | Marteau de faille | 1 fragment de faille, 1 poussière runique | damage 6, range 12 | reduced range |

Rules: two confirmations (prepare, then forge); atomic transaction (`confirmForge` checks everything before changing anything); one branch per weapon (the other closes); a second confirmation has no effect; no durability, no weapon destruction, no money economy.

### Multi-solution puzzle (`data/rm_interactions.txt`, zone 011, south bank of Paco)

| Object | Tile | Solutions |
|---|---|---|
| Brazier (`rm_demo_brazier`) | 23,50 | fire, heavy |
| Frozen path (`rm_demo_ice_path`) | 25,50 | ice, heavy |
| Cracked pillar (`rm_demo_pillar`) | 27,50 | heavy |
| Old mechanism (`rm_demo_gate`) | 30,52 | opens when 2 of 3 objects are solved |
| Lever (`rm_demo_reset`) | 29,52 | resets all 3 objects (the cache stays open) |

Opening it triggers the `rm_demo_gate` event, which spawns the cache (33,52: Épée de braise; 34,52: 2 éclats de braise). Possible solutions: hammer alone (any two objects), ember blade + frost lance, or hammer + either of the two other weapons. No existing gate is affected (test `auto_rm_no_gate_change`).

### V4 save format

New lines `ameliore <recette>` and `interaction <id> 1`. V2/V3 saves load without inventing anything (test `auto_rm_save_v3`); V4 reloads and rewrites identically (`auto_rm_save_v4`); a future version is rejected (`auto_rm_save_future`).

## 4. Limitations of this first phase

- One upgrade rank per branch, one puzzle, three weapons, five materials, six recipes.
- Puzzle objects: original sprites are being created by the art team; in the meantime, temporary markers (small coloured squares) are used — these are not final assets.
- The weapons/items/equipment menu does not yet have its native A1 layout (next task); the forge page already uses the native parchment.
- EP does not regenerate: the elemental charged strike depends on Noix de Lorien / Raisins d'Eru.

## 5. Tests

`ctest --test-dir build -R auto_rm_ --output-on-failure` (22 scenarios) and `py -3.12 tools\validate_data.py` (IDs, references, walkable tiles, no collisions and no affected gate).
