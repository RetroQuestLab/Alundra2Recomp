# Coverage of Alundra 2 narrative content in the remake

Audit dated 2026-11-21 (no code changed). This planning document compares what the locally extracted A2 text attests with what the remake actually contains, then proposes a backlog.

## 0. Sources, conventions and reading limitations

- **Reference A2 text**: `extracted/a2_text/dialogues.txt` (262 blocks, 553 messages, heuristic extraction from `GRP_FR.GP`, never published). Blocks are cited as `bloc N`; **no line of dialogue is reproduced**, everything is summarized in my own words. `extracted/a2_text/menus.txt` (name lists, system text) is cited as `menus bloc N`.
- **Remake content** read directly: `data/world.txt`, `data/warps.txt`, `data/dialogues.txt` (131 IDs, including 20 `rm_*`/`objet_rm_*`), `data/items.txt`, `src/title.cpp` (prologue), `CMakeLists.txt` (tests `auto_*`, gates).
- **Remake additions not to be counted as A2 content** (`docs/remake/ajouts_tranche3.md`): blacksmith Orvan, the forge, three `rm_*` weapons, five `rm_*` materials, the brazier/pillar/mechanism puzzle, material chests (Paco, Kindra, wreck). They are listed in the appendix and excluded from all counts.
- **Block numbers ≠ remake map IDs.** A `GRP_FR.GP` block corresponds to an A2 map (index in the offset table), whereas remake zones use **A1** map numbers (`011`, `013`, `019`, `119`, `026`, `042`←`028`, `050`←`029`, `086`←`316`, `120`). The block → location mapping is based solely on bracketed titles present in some blocks (e.g. a room name) and on cross-references between lines. Many blocks have no title: their location is inferred.
- **The story order is reconstructed** from internal clues (cross-references between blocks, exchanged items, instructions such as "va à ..."); it is not read from an event script, since the extraction contains no conditions, flags or speaker names. Any row marked "uncertain order" must be confirmed by an independent source (video, A2 wiki).
- **Statuses** (per location):
  - **PRESENT**: a playable remake area covers the main events, NPCs and items attested for the location;
  - **PARTIAL**: an area, a scene or NPCs exist, but a notable part of what is attested is missing, relocated or contradicted;
  - **ABSENT**: neither an area nor a scene (at most a few NPCs alluding to it elsewhere).
- A1 background maps are available (483 `assets/maps/*.a1m` files); choosing a map suited to an A2 location remains to be done each time (precedents: Eden, Puerto Medusa, the manor, where the original map was black or too dark and was replaced).

## 1. Alundra 2 chapters and locations in story order

Remake: 9 playable zones (+ a narrative prologue). None is "PRESENT": each reuses an A1 background for an A2 location, with a **compressed and partly recomposed** plot (see § 3 "plot divergences").

Column key: **Blocks** = blocks of the extracted text that attest to the location; **Remake** = `world.txt` zone or scene; **NPCs / dialogue present** = IDs from `data/dialogues.txt`; **Items / gates / bosses** = what exists in `world.txt`, `warps.txt`, `items.txt`.

### Act I — Prologue, Paco, forest, wreck

| # | A2 location | Blocks | Remake | NPCs / dialogue present | Items / gates / bosses | Status | What is concretely missing |
|---|---|---|---|---|---|---|---|
| 1 | **Pirate flying ship** (cabin, bathroom, shop, decks, cockpit, engine room, hold, captain's quarters, security control room, healing area) | 1-10, 13, 14, 16, 18, 34 | Narrative prologue `src/title.cpp` + dialogue `[prologue]` (maps 042, 018, "ship", "fall", 026) | `prologue`, `navire_milena` (one line from Milena to Zeppo, out of context) | none; no combat | **PARTIAL** | No playable area. The opening fight against Zeppo (block 1) is replaced by a cutscene; the "Gi Gi" androids at the controls (blocks 1, 14, 16) are only alluded to; the control room that unlocks the doors (blocks 1, 34); the 100% HP/EP healing area (block 13); the mysterious key found aboard (blocks 3, 7) = the start of the trading chain. |
| 2 | **Village Paco** (houses, general store, chief's house, bathroom, Jeehan statue) | 19-24, 29, 31, 32 | Zone `011` "Village Paco" | `prunewell`, `prunewell_apres`, `garde`, `garde_apres`, `villageoise`, `villageoise_apres`, `enfants_paco` (+ `_objet_*` variants); addition `rm_forgeron` | `herbe` chest; exit to Kindra (not gated) | **PARTIAL** | Waking-up scene with Alexia and **bounty on Flint** (block 22: deliver Flint or cancel the bounty); **chief's house** (block 22); general store (blocks 21, 22); interiors with residents, children and the Evo bull (blocks 20, 24); Jeehan statue to pray at (blocks 19, 23, 24: `Prier/Quitter` menu); guard advising to equip a sword and shield (block 19); merchant looking for his piggy bank (block 22). |
| 3 | **Forêt Kindra** + **Finrod's House** | 29-33 (30: block puzzle; 32: Finrod/Prunewell; 33: merchant) | Zone `013` "Forêt Kindra" | `finrod`, `finrod_apres`, `finrod_objet_lettre` | `coeur`, `lettre` chests (guarded by the ogre); jelly/turtle/goblin/ogre monsters; internal passages | **PARTIAL** | Prunewell–Finrod reunion (block 32); merchant in his underwear trading a compass for an herb (block 33, choice); puzzle of wooden and purple blocks to push (block 30); puzzle piece in a chest (block 11); Alexia catching up with Flint (blocks 30, 31); in A2 Diaz's letter is found **in the wreck** (block 8), not in the forest (divergence, § 3). |
| 4 | **Mont Épervier / wreck** (crashed airship) | 5, 8, 11, 31 | Zone `019` "Mont Épervier - Epave" | `epave_arrivee`, `epave_arrivee_2`, `alexia_epave` (+ variants), `belgar_epave` | `gantelet` chest; 2 droids + goblin; exit to Toroledo (not gated) | **PARTIAL** | Two-player exploration with Alexia and the revelation that the androids were transformed humans (block 5); **Diaz's letter to Milena** found here (block 8); **fight against Méphisto's cat transformed into a monster by a key** (block 11, recalled in block 304): the remake places an ogre in the forest (route decision to preserve, § 2b); mysterious key (blocks 3, 7); Belgar is not attested here (the remake places him here, § 3); the gauntlet comes from Jeehan's apprentice (block 219), not from the wreck. |
| 5 | **Bombarded Paco** (village ruins, destroyed arch) | 23, 24, 235-237, 304, 310 | absent (Paco stays intact throughout the route) | only `medusa_arrivee` alluding to it | none | **ABSENT** | Event: a mechanical bull bombards Paco under the pirates' emblem (blocks 23, 24, 304, 310); Alexia and Flint return to the rubble (block 23); displaced villagers, destroyed shop, hammer traded for the red statue (block 23, trading chain); ruins where the Children of the Crown found and hid the idol (blocks 231, 237). Requires a "ruins" variant of map 011. |

### Act II — Toroledo, Pierre, Vallée de Mithra, Fort Noir, Mont Vulcain (order partly uncertain)

| # | A2 location | Blocks | Remake | NPCs / dialogue present | Items / gates / bosses | Status | What is concretely missing |
|---|---|---|---|---|---|---|---|
| 6 | **Toroledo** (bullfighting arena, inn + storeroom + upper floor, shops, barn, bull waiting room, warehouse, Jeehan's house, houses) | 41, 42, 49-64, 219 | Zone `119` "Toroledo" | `garde_toroledo`, `alexia_toroledo`, `jeehan`, `jeehan_apres`, `jeehan_epee_supreme`, `dame_toroledo`, `toroledo_natasha`, `enfants_toroledo`, `prisonnier_toroledo`, `nunugi_gwaba`, `berger_toroledo` (+ `_objet_ordre` variants) | `ordre` chest; `epee_supreme` gate to the Valley; 2 droids | **PARTIAL** | Arena, betting, entry ticket (blocks 49-55); inn (10 O per night) and shops (blocks 54, 55); **Jeehan's house**: master sealed by a spell, 4 puzzles to complete with puzzle pieces, teaching of combos then of the Lightning attack, handing over the gauntlet (block 219); the remake instead gives the supreme sword in exchange for "baron's orders" (§ 3); **Pierre the bullfighter, his wife Emma and their daughter Luca** (blocks 49, 50, 59, 291); Miss Vachette (blocks 53, 55, 416); collector trading the mysterious key for a carved cow (block 54); bulls Lao and Evo (blocks 58, 61-64); access to Vallée de Mithra **through the garden of Pierre's house** (blocks 50, 59, 62, 64). |
| 7 | **Pierre's House / Howling Cave / House of Happiness** | 49, 50, 59, 291, 292 | absent | — | — | **ABSENT** | Emma and Luca's quest to bring Pierre home (blocks 49, 50, 59, 291); the "House of Happiness", a trap luring players under Fort Noir (blocks 25, 37, 38); Howling Cave (blocks 291, 292). |
| 8 | **Fort Noir** (+ Port Yagin, mine shaft, underground passages, broken lifts, ore carts); Black Island? | 25-28, 37-39, 45, 48, 269, 294, 299-302, 305, 307, 310-312, 319 | absent (allusions only) | `zach_fort`, `medusa_minerai` (placed at Puerto Medusa) | none | **ABSENT** | Antagonist with a distinctive laugh and his servants trapping Flint (blocks 25, 28, 37); ore transported by carts from the mine to Puerto Medusa (blocks 294, 302, 305); infiltration of a pirate ship (block 294); merchant who survived the whale on Black Island (block 269; identification of Black Island / Fort Noir uncertain). |
| 9 | **Vallée de Mithra** (huge bull-shaped vehicle "ToroTank", underground base) | 304, 328, 332-334 | Zone `026` "Vallée de Mithra" | `vallee_arrivee`, `guide_vallee`, `guide_vallee_objet_lampe`, `boss_026`, `objet_lampe`, `objet_idole` | `lampe` gate (chest behind the golem `boss_026`); `idole` chest; golem, jelly, turtle, goblin | **PARTIAL** | The attested location: Zeppo, Ruby and Albert see and then explore a gigantic machine (block 328); fight against the bull monster, which turns back into a normal bull (block 304); secret base of young people who know Flint (block 333, untitled, inferred attribution); scene where Diaz and Méphisto have relics placed on a wall (blocks 334, 365, untitled, inferred location: see line 20); merchant in an underground cave (block 332, titled "Grotte souterraine", inferred attribution). The guardian golem and the Lamp as a reward are **not** attested (remake inventions). |
| 10 | **Mont Vulcain / Underground River / Dragonia / Dragon's Prison** | 220, 222, 227, 229, 231, 272, 274, 282, 283, 287 | absent (allusion `objet_idole`) | — | `idole` chest placed in Vallée de Mithra | **ABSENT** | Androids' secret passage east of Mont Épervier, triton ring to walk on lava (blocks 222, 229); hiding place of the three-eyed idol (blocks 231, 237); the dragon Tirion imprisoned and its remedy (block 282); **Croc rouge, le Gardien** (block 282); plan to drive a key into the dragon (block 274); roof of Dragonia (block 283). |

### Act III — Puerto Medusa, Milena's manor, palace, Gwaba (reconstructed order: infiltration, abduction of Alexia, ransom of the relics)

| # | A2 location | Blocks | Remake | NPCs / dialogue present | Items / gates / bosses | Status | What is concretely missing |
|---|---|---|---|---|---|---|---|
| 11 | **Puerto Medusa** (pirate base island: harbour, restaurant with a show, inn, shop, warehouse, shipyard, lighthouse, gold badge testing centre, path to the manor) | 90, 99-109, 112, 120, 122-126, 129-133, 206-211, 253, 255-259, 261-266, 290, 297 | Zone `050` "Puerto Medusa" (map `029`) | `medusa_arrivee`, `marin_puerto`, `medusa_baleine`, `zach_fort`, `medusa_minerai`, `ruby_puerto`, `albert_puerto`, `diseuse_puerto`, `zeppo_puerto`, `georges_puerto` (+ `_objet_cristal` variants) | `boss_medusa` gate (the captain, remake boss) to the manor; `cristal` chest; captain, droid, goblin | **PARTIAL** | **Gold badge system** (obstacle course to the manor, barrier reserved for badge holders: blocks 107, 123, 126, 132) instead of the captain; disguises: Alexia sings as Axelya and Flint passes himself off as her little brother Georges (blocks 99, 262, 263); inn (block 103), fortune teller (block 103), Valar shield blacksmith (block 102, inferred location), lighthouse and chain of informants (blocks 106, 261); arrival of Méphisto and his priests, Alexia taken away (blocks 264-266); mini-games "Faites la bombe", "Attaque du chien", RC circuit, vases, rock-paper-scissors (blocks 90, 109, 112, 130, 131; location inferred for some); password door guessed by Alexia (block 178, uncertain attribution). The Star Crystal is found **inside the whale** (block 433), not here. |
| 12 | **Lady Milena's manor** (entrance, garden, hall, annex, kitchen, dressing room + roof, punishment room, basement, secret path) | 120, 135-137, 240-251, 253-259, 264, 267, 288 | Zone `086` "Manoir de Milena" (map `316`) | `manoir_arrivee`, `garde_manoir`, `milena`, `belgar_manoir`, `natasha_manoir`, `diaz_manoir`, `alexia_manoir`, `gwaba_habitant` (+ `_objet_cristal`, `_boss_medusa` variants) | `cristal` gate to the throne; `coeur` chest; droid, goblin | **PARTIAL** | Stealth infiltration through the chimney and dressing room (block 251, clues from Kate the fan); punishment room and comic queue (blocks 243-250); **Milena – Méphisto confrontation** in front of Belgar (block 242); handing the red cape to Belgar in exchange for the sword (block 242); Milena–Belgar relationship and Belgar's past with Flint (blocks 242, 267, 288); red alert and an announced visitor (blocks 136, 137, 243); Diaz and Natasha are **not** attested here (palace, block 417). |
| 13 | **Royal Palace of Varuna** (cells, throne room) | 414-419 | Zone `120` "Salle du trône" | `mephisto_arrivee`, `mephisto_trone`, `alexia_finale`, `roi_trone` (+ `_mephisto_vaincu` variants); `enfants_paco_mephisto_vaincu`, `belgar_manoir_mephisto_vaincu` | Méphisto boss + droid; true victory tested by `auto_boss_victory` | **PARTIAL** | Forced wedding rehearsal with Alexia hostage, negotiation to exchange the relics with a **choice** (block 414); Flint imprisoned, Natasha helping him escape (blocks 415-417); Prunewell and young Children in cells (block 415); Diaz's provisional agreement and request for the idol (block 419). The final confrontation with Méphisto **is not attested** by the extraction (see § 5): it is a remake reconstruction. |
| 14 | **Gwaba** (town hall with lift, inn, shop, houses of Jessica/Po/Sue/José/Linda/the mayor, Omega's house, Roswell's house, harbour) | 179-196, 198, 200-207 | absent | `nunugi_gwaba` (at Toroledo), `gwaba_habitant` (at the manor) | none | **ABSENT** | The whole town: investigation of the hook-handed man by Ruby and Albert (blocks 179-194, 200, 201); false lead at the inn (block 183); Piaf Doudou and red cape (block 187); merchant selling a puzzle piece for all the money (block 186); **Omega's house** and the "ideal woman" questionnaire (blocks 195-198); **Roswell's house**: handing over the baron's orders, reminder of the three relics, **Siren's Ring** (blocks 202, 205); departure by ship (blocks 206, 207). |
| 15 | **Key Church / basement / underground passage / key dam** | 203, 204, 463, 472, 473, 475-477, 479 | absent (allusions: `roswell`, `mephisto_trone`) | — | — | **ABSENT** | Cult devoted to the Key (block 204); secret passage through which it delivers ore (block 204); interior of the dam (blocks 473, 475, 476); observers' tower facing the dam, reachable by lift (blocks 477, 479). |

### Act IV — The relics: seagull ruins, Eden, whale, relic chamber

| # | A2 location | Blocks | Remake | NPCs / dialogue present | Items / gates / bosses | Status | What is concretely missing |
|---|---|---|---|---|---|---|---|
| 16 | **Seagull ruins** + underwater wreck (ruins key) | 69, 73-79, 126, 139, 140, 289, 374 | absent | — | — | **ABSENT** | Diving from Zeppo's ship with the Siren's Ring (blocks 139, 140); ruins key recovered from the wreck (blocks 139, 140, 374); ruins entrance and traps (slippery floor, spikes: blocks 73-79, 289). |
| 17 | **Île d'Eden** (village, well, cottage, capture of Piaf Doudou) | 404-406 | absent (zone `042` shows only the ruins) | `ome`, `roswell` (placed in the ruins) | — | **ABSENT** | Piaf Doudou capture game in exchange for the Dryad's Ring (block 404); Eden ruins key in a chest behind a yellow statue to destroy with the Dryad (block 404); villagers, dangerous well, inn (blocks 404-406). |
| 18 | **Ruines d'Eden** (round-key and triangular-key doors, levers) | 160, 165, 170, 178, 208, 354 | Zone `042` "Ruines d'Eden" (map `028`) | `eden_arrivee`, `roswell`, `roswell_objet_*`, `ome`, `ome_objet_idole`, `boss_042`, `objet_prisme` | `prisme` gate to Puerto Medusa; `prisme` chest (golem `boss_042`); golem, droid, goblin | **PARTIAL** | Doors requiring a round key then a triangular key (block 208); lever to operate with something other than the sword (block 170); unlocking by keys and levers attested only in fragments (blocks 165, 170, inferred attribution by proximity of numbers); placing the Star Prism (block 354); Roswell and Omega are attested at **Gwaba**, not here. The guardian golem is not attested. |
| 19 | **Demon whale** (mouth, air tube, and areas of the brain, lungs, heart, feeding tube, stomach, intestines and control room) | 423-434, 437-439, 441, 443, 449-452, 454, 460-462, 481 | absent (one NPC scene only: `medusa_baleine`) | `medusa_baleine`, `ruby_puerto`, `albert_puerto` (account of their visit) | — | **ABSENT** | Complete dungeon: separation of Flint, Ruby, Albert and Zeppo (blocks 433, 443); coloured-key and particle-bridge puzzles (blocks 433, 449-451); monster diving and intrusion alerts (block 426); Star Crystal found here (block 433); gastric fluids and exit through the mouth (blocks 443, 461, 462). |
| 20 | **Relic chamber** (wall with slots, tower over two thousand years old; untitled location) | 333-334, 354, 364, 365, 371 | absent (replaced by the manor staircase and the throne) | `roswell`, `ome`, `mephisto_trone` (explanations) | `lampe`, `prisme`, `cristal` gates carry the role of the relics | **ABSENT** | Successive placement of the relics (idol, prism, crystal: blocks 334, 354, 364); Méphisto reveals that he built the tower for his star key and that Diaz activated it unwittingly (block 334); reading of the Book of Kings and "instructions for unsealing the treasures" (block 365). The exact composition of the relic trio is **ambiguous** (§ 5). |

### Act V — Side stories and end of the adventure

| # | A2 location | Blocks | Remake | NPCs / dialogue present | Items / gates / bosses | Status | What is concretely missing |
|---|---|---|---|---|---|---|---|
| 21 | **Gamar Island** (medal game room, bar, play area) | 290, 407-409, 486-488 | absent | — | — | **ABSENT** | Buying medals (5 O each), "Tireur", "Rallye" and "La Course" mini-games (blocks 408, 486-488). Purely optional content. |
| 22 | **Children of the Crown HQ** (ship or airship? untitled location) | 388 (+ 415, 477, 479) | absent (only `enfants_paco`, `enfants_toroledo`, `prunewell_couronne`) | generic messenger | — | **ABSENT** | Scene where the Children learn of Alexia's abduction and decide to free her; oath to defend the Crown; setting sail for Mont Vulcain (block 388). |

**Count for the 22 locations**: PRESENT 0 · PARTIAL 10 (rows 1, 2, 3, 4, 6, 9, 11, 12, 13, 18) · ABSENT 12 (rows 5, 7, 8, 10, 14, 15, 16, 17, 19, 20, 21, 22).
All 9 playable zones of the remake are PARTIAL locations: `011`→2, `013`→3, `019`→4, `119`→6, `026`→9, `042`→18, `050`→11, `086`→12, `120`→13. The narrative prologue covers row 1.

## 2. Characters and bosses

### 2a. Characters

Reference list: `menus bloc 15/17` gives fourteen A2 portrait/speaker names (Alexia, Ruby, Zeppo, Albert, Milena, Belgar, Diaz, Méphisto, Omega, Prunewell, Natasha, the 139th king of Varuna, Nunugi, Axelya). Other characters come from the dialogue blocks.

Remake sprites (`assets/sprites`): **named** (one file per character; their graphical origin was not verified here) for Flint, Alexia, Jeehan, Milena, Méphisto, Ruby, Albert, Natasha, Belgar, Diaz, Roi, Nunugi, Zach; **generic A1 sprites reused** for everything else (`villageois`, `villageoise`, `ancien`, `capitaine`, `droide`, monsters). Zeppo, Pierre, Prunewell, Finrod, Roswell and Omega borrow a generic sprite.

| Character | Attested (blocks) | In the remake (sprite · dialogue · zone) | Status | Missing / divergent |
|---|---|---|---|---|
| Flint | all | hero sprites · `intro`, `victoire`, hero lines · all zones | PRESENT | A1 portrait absent (the A2 sprite bust is used, documented decision) |
| Gi Gi (androids, humans transformed by a key) | 1, 5, 14, 16, 305, 310 | `droide` sprite · a few lines (`medusa_minerai`, `zeppo_puerto`) · spawns in 019, 119, 042, 050, 086, 120 | PRESENT (as enemies) | the "civilian" Gi Gi who speak in onomatopoeia (blocks 59, 305, 310) |
| Alexia | 5, 8, 11, 22, 30, 31, 41-42, 48-50, 263-265, 288, 414 | named sprite · `alexia_epave`, `alexia_toroledo`, `alexia_manoir`, `alexia_finale` · zones 019, 119, 086, 120 | PARTIAL | role as **companion who follows Flint** (blocks 11, 29, 31, 41, 42, 49), waking scene at Paco and bounty (block 22), Axelya disguise (block 263), captivity at Diaz's (blocks 264, 414) |
| Prunewell | 22, 32, 415 | generic `ancien` sprite · `prunewell`, `prunewell_apres`, `prunewell_couronne` · zone 011 | PARTIAL | scene with Finrod (block 32), imprisonment (block 415) |
| Finrod | 19, 31, 32 | `ancien` · `finrod*` · zone 013 | PARTIAL | Finrod's House as an interior; his reunion with Prunewell |
| Zeppo | 1, 14, 139, 140, 180, 289, 290, 328, 334, 419, 425 | `capitaine` sprite (substitute) · `zeppo_puerto`, `navire_milena`, prologue · zone 050 | PARTIAL | opening fight, ship he pilots to Gwaba/Puerto/Gamar (blocks 206, 290) |
| Ruby | 1, 14, 139, 180, 183, 209, 289, 334, 433 | named sprite · `ruby_puerto` · zone 050 | PARTIAL | investigation in Gwaba, hostage at Diaz's (block 334), whale |
| Albert | same blocks as Ruby | named sprite · `albert_puerto` · zone 050 | PARTIAL | same; blunders and false lead at the inn (block 183) |
| Milena | 14, 242, 267 | named sprite · `milena`, `milena_objet_*`, `milena_boss_medusa` · zone 086 | PARTIAL | confrontation with Méphisto (block 242), relationship with Belgar (block 267); the remake has her say she is breaking away from Diaz in favour of Méphisto (unattested) |
| Belgar | 242, 264, 267, 288 (+ allusions 123, 297) | named sprite · `belgar_epave`, `belgar_manoir` · zones 019, 086 | PARTIAL | red cape / supreme sword exchange (block 242), link with Flint (§ 5), unattested presence at the wreck |
| Diaz | 8 (letter), 334, 365, 414, 419 | named sprite · `diaz_manoir` · zone 086 | PARTIAL | appears at the palace, not the manor; forced marriage, relic trade |
| Méphisto | 242, 264-266, 288, 334, 365 (+ cat 11, 304) | named sprite · `mephisto_*` · zone 120 (boss) | PARTIAL | his role as builder of the tower and sorcerer over two millennia old (block 334); his escape (blocks 242, 484) |
| Natasha | 417, 419 | named sprite · `toroledo_natasha`, `natasha_manoir` · zones 119, 086 | PARTIAL | her real scene (palace, Flint's escape) is missing; she is placed elsewhere |
| Nunugi | 202 (orders), 414, 419, 365 | named sprite · `nunugi_gwaba*` · zone 119 | PARTIAL | relic handover scene (block 414); is at Toroledo instead of Gwaba |
| Omega | 48, 103, 198, 219 | generic `villageoise` sprite · `ome`, `ome_objet_idole` · zone 042 | PARTIAL | prophecy house and questionnaire (block 198); prophecy announcing Flint (blocks 48, 219) |
| Roswell the Sage | 139, 202, 205 | generic `ancien` sprite · `roswell*` · zone 042 | PARTIAL | his house under Gwaba's bridge, Siren's Ring (block 205) |
| Jeehan | 19, 23, 24, 159, 203, 219, 289 | named sprite · `jeehan*` · zone 119 | PARTIAL | sealed by a spell and freed by puzzle pieces (block 219); statue worshipped in several towns |
| Roi (139th king of Varuna) | menus 15/17, 22, 188, 415 | named sprite · `roi_trone*` · zone 120 | PARTIAL | missing / imprisoned by Diaz (blocks 22, 188, 415); on the throne in the remake |
| Pierre (bullfighter, Emma's husband, Luca's father) | 49, 50, 59, 62, 64, 291 | generic `villageois` sprite · `guide_vallee` (repurposed as a guide) · zone 026 | PARTIAL | role as a ruined gambler and being brought home; the remake makes him a guide |
| Zach the One-Eyed | 213, 215, 217 | named sprite · `zach_fort` · zone 050 | PARTIAL | exact role uncertain (§ 5) |
| Children of the Crown (group) | 32, 388, 415, 477, 479 | `enfants_paco`, `enfants_toroledo`, `prunewell_couronne` · generic sprite | PARTIAL | HQ scene (block 388), oath, named young princes and princesses |
| Axelya / Georges (Alexia and Flint in disguise) | 99, 262, 263 | **contradicted** by `georges_puerto` (a sailor looking for Axelya) · zone 050 | ABSENT | the infiltration scene; the remake line is incompatible with the attested facts |
| Emma, Luca (Pierre's wife and daughter) | 49, 50, 59, 291 | — | ABSENT | quest to bring Pierre home |
| Miss Vachette | 53, 55, 416 | — | ABSENT | autograph in the trading chain |
| Lao and Evo (bulls) | 20, 58, 61-64 | allusion in `berger_toroledo` (who confuses Lao's disappearance with Pierre's) | ABSENT | bullfighting, Evo of Paco |
| Tirion (imprisoned dragon) | 282 | — | ABSENT | dragon remedy |
| Croc rouge, le Gardien | 282 | — | ABSENT | see bosses |
| Lumière / Palace Magician | 219, 334 | — | ABSENT | role of the tower and of the spell sealing Jeehan (identity of the "magician" uncertain) |
| Kate (housekeeper, Flint fan) | 251 | — | ABSENT | clues for infiltrating the manor |
| Méphisto's cat | 11, 304 | — | ABSENT | transformed into a monster by a key |
| Hook-handed man | 179-194, 202 | only mentioned by `nunugi_gwaba`, `gwaba_habitant` | ABSENT | identity unknown from the extraction; baron's orders storyline |
| Fort Noir antagonist (distinctive laugh, servants) | 25, 28, 37 | — | ABSENT | name and role unknown |
| Key Church priests | 123, 137, 180, 204 | allusions in `roswell`, `mephisto_trone` | ABSENT | visible characters |

**Count (33 rows; some group two characters)**: PRESENT 2 · PARTIAL 19 · ABSENT 12. No "PRESENT" character is complete across their whole storyline: Flint (hero) and the androids (enemies) are complete only in the sense that what is attested is playable.

### 2b. Bosses

| Attested boss (blocks) | Remake | Status | Notes |
|---|---|---|---|
| Zeppo, opening "skull-crusher" fight (block 1) | prologue cutscene only | ABSENT | no playable fight; `prologue` dialogue |
| Méphisto's cat transformed by a key, at Mont Épervier (blocks 11, 304) | **ogre** in Forêt Kindra (A1 sprite, letter chest) | PARTIAL (substitute) | **route decision to preserve** (validated route: "Mont Épervier boss"); only the identity differs: lines mentioning the key can be added without changing gameplay |
| Bull monster / "ToroTank" (blocks 304, 305, 328) | none | ABSENT | requires an original sprite; turns back into a normal bull once defeated (block 304) |
| Fort Noir antagonist (blocks 25, 28, 37) | none | ABSENT | |
| Croc rouge, Guardian of the dragon (block 282) | none | ABSENT | described as faster than any man |
| Demon whale (blocks 423, 426) | NPC `medusa_baleine` only | ABSENT | complete boss dungeon |
| Belgar (blocks 264, 267; the duel is not confirmed) | no fight | ABSENT | uncertain |
| Méphisto, final confrontation | throne room boss (`auto_boss_victory`) | PARTIAL (reconstruction) | the extraction attests neither the location nor the form of the final fight (blocks 371, 484: only escape and flight) |

Remake bosses **not attested** by the extraction (route inventions to preserve, reusing A1 sprites): golem of Vallée de Mithra (`boss_026`, `lampe` gate), Eden golem (`boss_042`, `prisme` gate), captain of Puerto Medusa (`boss_medusa`: opens the manor passage and the Crystal chest). The captain must not be presented as an A2 boss; "Capitaine Zeppo" is an attested NPC (block 297).

**Count of the 8 attested bosses**: PRESENT 0 · PARTIAL 2 · ABSENT 6.

## 3. Attested mechanics and narrative facts not implemented (or divergent)

### 3a. Plot divergences between the remake and the extraction

These discrepancies are **remake choices** (compression of the A → Z route). None should be corrected without a decision: the `epee_supreme`, `lampe`, `prisme`, `boss_medusa` and `cristal` gates, the route, the final Méphisto and the `auto_dialogue_*` test IDs must remain. They must, however, be **documented as such** and not presented as A2 plot.

| # | Remake | What the extraction attests | Possible action without touching the gates |
|---|---|---|---|
| D1 | Linear route Paco → Kindra → wreck → Toroledo → Valley → Eden → Puerto → manor → throne | After the wreck, Paco is bombarded; Toroledo and Pierre; Vallée de Mithra; Fort Noir; infiltration at Puerto Medusa; Alexia abducted; relics handed to Diaz; Gwaba; diving; whale; idol; relic tower (§ 1) | present the route as a "condensed version"; add the missing chapters as optional branches or interleaved steps (backlog) |
| D2 | Diaz's letter in a chest in the forest, guarded by the ogre | Letter found in the wreck by Alexia (block 8) | keep the chest; add a scene where Alexia comments on the letter (already `alexia_epave_objet_lettre`) |
| D3 | Lightning gauntlet in the wreck | Gauntlet handed over by Jeehan's apprentice after teaching the Lightning attack (block 219) | keep the chest; have Jeehan speak about his apprentice |
| D4 | `epee_supreme` offered by Jeehan in exchange for the baron's orders | Supreme sword received from Belgar in exchange for the red cape, at the end of a trading chain (blocks 187, 242); `items.txt` models the chain but the remake does not play it | document; implement the chain as an **alternative** source (backlog) |
| D5 | Lampe (golem), Prisme (golem), Cristal (captain); idol in a chest in the Valley | Three relics to bring to Diaz (blocks 205, 377); Crystal taken from the whale (block 433); idol hidden at Mont Vulcain (blocks 231, 237); the list of relics is not consistent from one block to another (§ 5) | in dialogue, avoid asserting pickup locations as A2 |
| D6 | `ordre` item = "baron's orders" in a chest at Toroledo | The baron's orders are Flint's mission letter, shown to Roswell and Alexia's friends (blocks 183, 202) | rename the description if necessary |
| D7 | Roswell and Omega at Eden; Nunugi at Toroledo; Natasha at Toroledo and the manor; Diaz at the manor; Georges at Puerto; Belgar at the wreck | Roswell, Omega and Nunugi at Gwaba/palace; Natasha and Diaz at the palace; Georges is Flint's disguise; Belgar at the manor | correct the most contradictory lines (`georges_puerto`, `berger_toroledo`); leave the positions as they are |
| D8 | Pierre as Valley guide | Pierre is an inveterate gambler, Emma's husband and Luca's father (blocks 49, 50, 59, 291) | rewrite without contradicting |
| D9 | Ogre, golems and captain as bosses | attested bosses: § 2b | document |
| D10 | Paco is never bombarded | Paco bombarded (blocks 23, 24, 235-237) | backlog |
| D11 | No money (gold = 0) | "O" currency, shops, inns, betting, mini-games | decision required (§ 3b M3) |

### 3b. Attested but absent mechanics and events

| # | Mechanic / event | Attested (blocks) | Remake state | Note |
|---|---|---|---|---|
| M1 | **Bounty on Flint**: the hero is "wanted dead or alive", Alexia can turn him in or cancel the bounty; posters put up around town | 22, 212, 247, 251, 262 | absent | opening of the Paco chapter, intended in phase 1 ("missing" list) |
| M2 | **Village chief's house** (scene of Alexia with Prunewell) | 22 | absent | Paco interior to add |
| M3 | **"O" currency and commerce**: general stores, inns at 10 O, wandering merchants, bribes, betting | 21, 54, 55, 100, 103, 183, 187, 239, 269, 332, 425, 461 | absent (menu: gold = 0, no economy) | every paid mini-game depends on it; architectural decision required |
| M4 | **Dialogue choices** (response menus: pray or quit, give or refuse, destination choice, Omega's questionnaire, replies to Diaz) | 23, 33, 41, 50, 51, 90, 107, 130, 131, 180-182, 198, 206, 290, 414, 477 | absent (linear `Nom: texte` dialogues) | **prerequisite** for several backlog entries |
| M5 | **Alexia companion**: she follows Flint, announces that she will wait, complains if left behind | 11, 29, 31, 41, 42, 49, 50 | immobile NPC | gameplay + levels |
| M6 | **Jeehan statues** (pray / quit) in several locations | 19, 23, 24, 51, 159, 203, 289 | absent | exact function (save? healing?) not established by the text |
| M7 | **Trading chain**: strange key (blocks 3, 7) → carved cow (54) → hammer (294) → red statue (23) → pirate earring (101) → Miss Vachette's autograph (53) → vanilla waffle (416) → Piaf Doudou (404) → red cape (187) → sword to Belgar (242) | cited | the 9 items exist in `items.txt` but **no source or NPC gives them out** | pure A2 content, without an economy; very good quest candidate |
| M8 | **Puzzle pieces** (Jeehan's 4 puzzles, counter, techniques taught one at a time) | 11, 186, 219, 274 | absent (no `puzzle_piece` item in `items.txt`) | Jeehan's storyline and combat progression |
| M9 | **Elements**: Pixie, Siren, Dryad and Triton rings; elemental shot 10 EP, Lord 200 EP; coloured targets; breathing underwater, walking on lava | 26, 205, 213-218, 222, 229, 404 | four rings present in `items.txt`, **no effect** (no reference in `src/`) | core A2 gameplay content |
| M10 | **Shields** (Mini, Medium, Titan, Mithril, Supreme, Valar) and the Valar shield quest (3 raw materials) | 19, 102, 361, 480 | absent from `items.txt` and the menu | requires an architectural decision (menu scope); do not confuse with Orvan's forge (an addition) |
| M11 | **Sword names** in the FR script: Longue, Mithril, Large, Runes, Brillante, Suprême | 478 | `items.txt`: Longue, bronze, large, runique, brillante, ascendante | align the labels with the FR script or document (§ 5); the `epee_supreme` ID exists separately with power 0 |
| M12 | **Combos taught by Jeehan**, Lightning attack by charging, final "special combination" | 219 | three-hit combo + charged strike present, but not the lesson | link to M8 |
| M13 | **Healing area** (100% HP/EP) | 13 | absent | can serve as a rest point in the Valley / the manor |
| M14 | **Mini-games**: bombs (90), dog attack (109), RC circuit (112), vases (130), rock-paper-scissors up to 999 999 O (131), darts (214), bullfighting (49-55), gold badge course (132), Gamar game room (486-488) | cited | absent | most depend on M3 |
| M15 | **Disguise and infiltration** (chimney, dressing room, stolen clothes, pirate ships) | 251, 262, 263, 293, 294 | absent | levels + gameplay |
| M16 | **Ship travel with choices** (Puerto Medusa, Gwaba, Gamar, Port Yagin) | 206, 290, 383, 409 | absent (fixed warp transitions) | depends on M4 |
| M17 | **Tutorials** (running, jumping, camera, pocket, chests, sliding) | 212 | partially in `garde` | |
| M18 | **Environment puzzles**: blocks to push/pull, foot switches, colour traps, non-sword lever, coloured keys/locks, conveyor belts, broken lifts, chain bombs | 26, 30, 48, 90, 170, 208, 212, 307, 311, 312, 319, 449-451 | one original remake puzzle (`rm_*`); no A2 puzzle | avoid confusing with Orvan's puzzle |
| M19 | **Diving and depth** (Siren's Ring, air tube) | 139, 205, 424, 426 | absent | linked to M9 |
| M20 | **End of adventure and collections** ("End" screen, congratulations for complete swords/shields) | menus 14/16, 478, 480 | route ending present (`auto_ending`); no collection screen | |

Mechanics already present and **not to be counted again**: `_apres` / `_objet_*` / `_<événement>_vaincu` dialogue variants (remake), narrative prologue, A1/A2 item menu, V4 save, Orvan's forge and puzzle (additions).

## 4. Prioritized backlog for upcoming phases

### Rules common to all entries

- Each entry must keep the route tests **green**: `auto_gate_*` (5 gates), `auto_boss_victory`, `auto_savetest`/`auto_save_ending`, `auto_dialogue_*` / `auto_zeppo*` / `auto_georges` / `auto_berger*` / `auto_prunewell`, plus `validate_data` (`ERR: 0 | WARN: 0`). Existing dialogue IDs do not change; additions use new IDs.
- New zones are A1 maps declared with `map`/`name` in `data/world.txt` (level design). 483 A1 maps are available; **no map has been verified for the locations below**: selection requires a PNG render (`tools/a1_map.py`) and a check for dark or empty maps (precedents: Eden, Puerto, manor).
- Lines written in original wording based on the facts above (no copying of the script). Anything absent from A2 keeps the `rm_*` prefix and is documented as an addition (`docs/remake/`).
- No existing sprite is redrawn; new characters or bosses (ToroTank, Croc rouge, Tirion, whale, Méphisto's cat, Emma, Luca, Kate, bulls) require **original** sprites (art team) or the reuse of a generic sprite, to be decided case by case.
- Relative effort (one focused session): **S** = a few data/dialogue files; **M** = data + limited code + 2-4 tests; **L** = new zone or system + sprites + several tests; **XL** = a complete dungeon or several zones.
- The architecture team (`src/game.h` decisions) and the QA team (tests, `validate_data`) are involved whenever a data structure or a test changes; they are only repeated below when their role is central.

### Wave 0 — Prerequisites and fixes (no new maps)

| ID | Zone / theme | Playable objective | Teams | Dependencies | Deterministic test | Effort |
|---|---|---|---|---|---|---|
| B01 | Cross-cutting: fidelity of existing lines | Rewrite the lines that contradict the A2 text (`georges_puerto`, `berger_toroledo`, `guide_vallee`, `zach_fort`, `belgar_epave`, `milena`) without changing their IDs or the gates; document divergences D1-D11 in `docs/remake/` | narrative/UI | none | `auto_georges`, `auto_berger`, `auto_berger_ordre`, `auto_zeppo*`, `auto_prunewell` unchanged + `validate_data` | S |
| B02 | Cross-cutting: **dialogue choices** (M4) | Response dialogues (at least two choices, cursor on the native A1 parchment, keyboard/controller confirmation) that set a flag or give/take an item | architecture (`game.h`), narrative/UI (`dialogue.cpp`, `dialogues.txt` format), gameplay (`main.cpp`, `--auto` command), QA (validator) | none; prerequisite for B03, B04, B07, B10, B11, B12, B14… | new `--auto` choice token (`D1`/`U1` + `T`) and `--expect choice=…`: `auto_choice_yes`, `auto_choice_no`, `auto_choice_cancel` | M |
| B03 | Cross-cutting: **"O" economy decision** (M3) | Written decision: keep "gold = 0" or add an A2 currency (display, earnings, shops, inns); if so, create the subsystem | architecture, narrative/UI (menu), gameplay | decision | `auto_money_*` (earning, purchase, refusal when broke, save) if approved | S to decide; M to implement |

### Wave 1 — Directly extends the route (zones 011 → 019)

| ID | Zone / theme | Playable objective | Teams | Dependencies | Deterministic test | Effort |
|---|---|---|---|---|---|---|
| B04 | **Village Paco (011)**: waking with Alexia, bounty on Flint, chief's house (M1, M2) | An accessible chief's house in the village: Alexia scene, "alliance" or "bounty" choice, `alliance_alexia` event; main route unchanged (optional scene) | level design (interior + warp), narrative/UI (`chef_paco`, `alexia_paco`), gameplay (event) | B02; A1 interior map to choose; existing Alexia sprite | `auto_paco_chef` (enter, confirm, `--expect dialogue=alexia_paco`); route unchanged | M |
| B05 | **Forêt Kindra (013)**: merchant in his underwear and Prunewell–Finrod reunion (blocks 32, 33) | herb → compass exchange by choice; Finrod scene referring to Prunewell; puzzle piece in a chest (prepares B10) | level design (NPC, chest), narrative/UI, gameplay (item give/take) | B02; `puzzle_piece` item to add to `items.txt` | `auto_kindra_marchand` (`--give herbe:1 --expect qty:compass=1,qty:herbe=0`), `auto_kindra_refus` | M |
| B06 | **Mont Épervier (019)**: transformed-human androids, mysterious key, Méphisto's cat story (blocks 3, 5, 7, 11) | Add a strange-key chest (start of the B12 chain) and Alexia/Flint dialogue on the origin of the Gi Gi and of the key that transforms creatures; the fight remains against the ogre | level design (chest), narrative/UI | none; does not touch the boss | `auto_epave_cle` (`--expect qty:strange_key=1` after opening) | S |
| B07 | **Bombarded Paco**: "ruins" variant of map 011 (blocks 23, 24, 235-237) | After a route event (e.g. returning from the wreck), Paco appears in ruins with displaced residents and a hammer → red statue trade; **optional and reversible**, the route stays playable without revisiting | level design (map variant), art (ruins scenery if needed), narrative/UI, gameplay (`paco_bombarde` event) | B02; map 011 + new ruins overlay | `auto_paco_ruines` (zone + dialogue), route unchanged | L |

### Wave 2 — Toroledo and Vallée de Mithra (zones 119, 026)

| ID | Zone / theme | Playable objective | Teams | Dependencies | Deterministic test | Effort |
|---|---|---|---|---|---|---|
| B08 | **Toroledo (119)**: Pierre, Emma, Luca, Pierre's house (blocks 49, 50, 59, 291) | Quest: talk to Emma and Luca, find Pierre, bring him home; Pierre's house becomes an additional passage to the Valley **without removing** the `epee_supreme` gate (the quest opens a variant consistent with A2) | level design (house + warp), narrative/UI (~15 dialogues), gameplay (event), art (generic or original sprites for Emma/Luca) | B02; B03 if tips/money losses are kept; interior map | `auto_pierre_quete` (`--expect dialogue=…` then `pierre_rentre` event); `auto_gate_epee_*` unchanged | M |
| B09 | **Toroledo (119)**: arena, inn, tickets (M3, M14) | Visitable arena, ticket, inn (rest) | level design, narrative/UI, gameplay | B03 approved | `auto_arene_billet`, `auto_auberge_repos` | L |
| B10 | **Toroledo (119)**: Jeehan's house, puzzle pieces, combos, Lightning (M8, M12, D3) | Jeehan teaches each technique in exchange for puzzle pieces; the Lightning attack is **learned or obtained** via the gauntlet already present; progress saved | level design (house), narrative/UI, gameplay (piece + technique system), art (piece icon) | B02, B05 (pieces), V5 save to plan | `auto_jeehan_puzzle_1…4`, `auto_save_puzzle` | L |
| B11 | **Vallée de Mithra (026)**: ToroTank (blocks 304, 328, 333) | Original mechanical-bull boss after the golem, secret base of the young people; `torotank_vaincu` event | gameplay (boss), art (**original** sprite), level design (arena), narrative/UI | sprite to create; large-hall A1 map to choose | `auto_torotank_victory` (`--godmode`, `--expect` event); `auto_gate_lampe_*` unchanged | L |

### Wave 3 — Puerto Medusa, manor, palace (zones 050, 086, 120)

| ID | Zone / theme | Playable objective | Teams | Dependencies | Deterministic test | Effort |
|---|---|---|---|---|---|---|
| B12 | **Partial trading chain** (M7, D4) | In existing locations: Toroledo collector (key → cow), Miss Vachette (earring → autograph), Puerto Medusa (red statue → earring), manor (red cape → sword to Belgar); the missing links remain to be supplied by other entries: hammer (B21), Piaf Doudou (B18), red cape (B16), waffle (location not established) | level design (NPCs), narrative/UI (trade + dialogue), gameplay (`trade`) | B02; sprites: Miss Vachette (generic `villageoise`) | `auto_troc_cle_vache`, `auto_troc_cape_epee` with `--give` | M |
| B13 | **Puerto Medusa (050)**: gold badges, Axelya/Georges disguises, Méphisto's raid (M14, M15; blocks 99, 107, 126, 132, 262-266) | Obstacle course to the manor entrance awarding a badge; infiltration scene (Alexia the singer, Flint as her brother); arrival of Méphisto who takes Alexia away; **added** alongside the `boss_medusa` gate, not in its place | level design (course, restaurant), gameplay (badge, disguise sprites), narrative/UI (~20 dialogues), art | B02; restaurant and testing centre maps; disguise sprites | `auto_badge_or`, `auto_deguisement_dialogue` | L |
| B14 | **Manor (086)**: infiltration (chimney, dressing room), punishment room, Milena–Méphisto confrontation (M15; blocks 242-251) | Alternative stealth route to Milena's room; scene with Belgar | level design (3-4 rooms), narrative/UI, gameplay (simple stealth), art (Kate) | B12, B13; interior maps | `auto_manoir_cheminee` | L |
| B15 | **Palace (120)**: wedding rehearsal, relic negotiation, cells, escape with Natasha (blocks 414-419) | Playable scene before the Méphisto boss: choice (give the relics or demand release), cell, escape; **does not alter** the final boss (route decision) | level design (cells), narrative/UI, gameplay (choice) | B02, B13; cell map | `auto_palais_choix`, `auto_boss_victory` unchanged | M |

### Wave 4 — New chapters outside the current route

| ID | Zone / theme | Playable objective | Teams | Dependencies | Deterministic test | Effort |
|---|---|---|---|---|---|---|
| B16 | **Gwaba** (blocks 179-207) | Hub town: town hall with lift, inn, Roswell (Siren's Ring, three relics), Omega (questionnaire), hook-handed man investigation; move the remake NPCs while keeping their IDs | level design (3-5 maps), narrative/UI (50+ lines), gameplay (lift, choices), art (generic) | B02, B03; generic sprites (Mayor, Jessica…) | `auto_gwaba_roswell` (`--expect dialogue=…`), `auto_gwaba_ascenseur` | XL |
| B17 | **Seagull ruins + diving** (M19; blocks 69, 73-79, 139, 140, 289, 374) | Zeppo's ship, diving with the Siren's Ring, ruins key, traps | gameplay (water, ring), level design, art, narrative/UI | B16 (Siren's Ring), B20 (Elements) | `auto_plongee_cle` | L |
| B18 | **Île d'Eden** (village, Piaf Doudou, Dryad's Ring; blocks 404-406) | Piaf Doudou capture mini-game, Dryad's Ring, yellow-statue key | level design, gameplay (bird AI), narrative/UI, art (original bird) | B20 | `auto_piaf_capture`, `auto_dryade` | L |
| B19 | **Demon whale** (blocks 423-462) | Six-zone dungeon (mouth, lungs, heart, stomach, intestines, brain), coloured-key and bridge puzzles, Star Crystal | level design (6 maps), gameplay (puzzles), art (whale), narrative/UI | B16, B17; decision on the Crystal's place in the current route | `auto_baleine_*` per zone | XL |
| B20 | **Elements**: rings and shots (M9) | Four rings with effects (projectile, swimming, stomping, fire), 10 EP cost, Lords, coloured targets | gameplay, art (effects), narrative/UI (descriptions), level design (targets) | items already present; EP/HUD decision | `auto_ring_pixie`, `auto_ring_siren`, `auto_ring_dryad`, `auto_ring_newt` | L |
| B21 | **Fort Noir + House of Happiness + Port Yagin** (blocks 25-28, 37-39, 291-302) | Mining dungeon with carts, broken lifts, House of Happiness trap, antagonist | level design, gameplay, art, narrative/UI | B08 (Pierre), B02 | `auto_fortnoir_*` | XL |
| B22 | **Mont Vulcain, Dragonia, Tirion, Croc rouge** (blocks 220-231, 272-283) | Passage east of the wreck, lava (Triton's Ring), dragon's prison, Croc rouge boss, remedy | level design, gameplay, art (dragon, boss), narrative/UI | B20 (Triton), B21 | `auto_vulcain_*`, `auto_croc_rouge_victoire` | XL |
| B23 | **Key Church, dam, Children's HQ** (blocks 203-205, 388, 463-479) | Underground areas, observatory, HQ; links the B16 investigations | level design, narrative/UI, gameplay | B16 | `auto_eglise_*` | L |
| B24 | **Gamar Island and medal mini-games** (M14) | Game room, medals, three games | level design, gameplay, narrative/UI | B03 (economy) | `auto_gamar_*` | L |

### Cross-cutting entries to schedule as capacity allows

| ID | Theme | Playable objective | Teams | Dependencies | Test | Effort |
|---|---|---|---|---|---|---|
| B25 | **Alexia companion** (M5) | Alexia follows Flint, waits at zone entrances, comments on events | gameplay, level design (pathing), narrative/UI | B04 | `auto_compagnon_suit` (`--expect` relative position) | L |
| B26 | **A2 shields** (M10) | Six shields in the menu, defensive effects, Valar shield quest | architecture (menu decision), gameplay, narrative/UI (`items.txt`), art (icons) | B03 | `auto_menu_shield`, `auto_valar` | M |
| B27 | **Sword name alignment** (M11) | FR labels identical to the script (Mithril, Suprême…) or a divergence note; `rising_sword` / `epee_supreme` clarified | narrative/UI (`items.txt`), QA | none | `validate_data`, `auto_menu_*` | S |
| B28 | **Jeehan statues, healing area, "End" screen and collections** (M6, M13, M20) | Interactive statues (pray/quit), 100% HP/EP rest, ending screen | gameplay, narrative/UI | B02; exact function of the statues to confirm | `auto_statue_prier`, `auto_zone_soin` | M |

### Recommended order

1. B01 (fixes) and B02 (choices): no new maps, unlock the rest. 2. B04, B05, B06: directly extend Paco → wreck. 3. B08, B10 then B11: Toroledo and the Valley. 4. B12, B13, B14, B15: Puerto, manor, palace. 5. B03 to be decided as soon as possible: it conditions several entries. 6. Wave 4 by dependencies (B16 → B17/B18 → B19; B20 cross-cutting).

## 5. What the remake must not claim

This audit is based on a **heuristic extraction** (`tools/a2_text.py`): it keeps sequences of character-words containing at least three consecutive letters, grouped into "messages" by message end markers. It is not claimed to be exhaustive, and its limitations impose the following caveats.

1. **No complete coverage of A2.** 262 blocks / 553 messages; block numbers go up to 502, so many clues are absent from the extraction. The discrepancy with the historical "815 blocks" is unresolved (815 = 262 headers + 553 messages in non-empty lines). Blocks containing placeholder game text (86, 87, 149, 170, 502) carry no story. Very short messages or those made of fewer than three consecutive letters may be missing.
2. **No information about who speaks.** The extraction contains no speaker names, portraits, appearance conditions or event flags. When this document attributes a line to a character, it is by cross-checking content: for example, the exchange of the red cape for the supreme sword (block 242) is attributed to Belgar via the `items.txt` comment (the sword's page on the A2 wiki) and the context, not by the text itself.
3. **No maps, enemies, chest or NPC positions are attested.** The text says nothing about the topology of locations, the number or appearance of bosses, chest rewards or puzzles (only verbal instructions appear). The placement of items in remake zones is therefore **always** a remake decision.
4. **Inferred locations.** Only blocks containing a bracketed title name a location (Paco, Mont Épervier, Toroledo, Vallée de Mithra, Ruines d'Eden, Gwaba, Eden, Gamar, etc.). The attribution of blocks 25-28, 37-39, 178, 332-334, 354, 364, 365, 388, 407 and others is inferred by proximity or by content. The same applies to the order of chapters, particularly the middle of the adventure (Fort Noir, Mont Vulcain, Vallée de Mithra).
5. **Ambiguous relic composition.** One block cites Lampe, Prisme and Cristal (blocks 205, 377); another asks to add the three-eyed idol to a trio of which two members are the Prism and the Crystal (blocks 334, 365). The remake chose four items (Lampe, Prisme, Cristal, idole). Do not state that the list is "Lampe + Prisme + Cristal + idole" as an A2 fact without a second source.
6. **Unestablished identities**: the hook-handed man (blocks 179-194, 202), the Fort Noir antagonist, the "magicien du palais" and Lumière, the exact link between Belgar and Flint (blocks 242, 267, 288), the function of Jeehan's statues, and the exact role of Zach le Borgne. Remake lines that answer these (for example the one attributing knowledge of Flint's father to Belgar) are **hypotheses**, not attested facts.
7. **Unattested ending.** The extraction contains no epilogue or explicit final battle (only Méphisto's escape, block 484, and a flight message, block 371). The throne room boss, the victory tested by `auto_boss_victory` and the `*_mephisto_vaincu` dialogues are remake reconstructions.
8. **French version only.** `GRP_FR.GP` is read; other languages were not compared. Several FR names differ from those on the wiki (Mithril/bronze, Suprême/ascendante, Piaf Doudou/Goo Goo Bird). No name has been "corrected" here.
9. **The golem, captain and ogre bosses, the guarded Lampe, the forge, the puzzle and Orvan are not from A2.** The first three are route choices to preserve, the other three are `rm_*` additions (§ Appendix). No communication text should present them as Alundra 2 story.
10. **Remake lines are original.** They are inspired by the facts summarized here and do not reproduce the game script; they therefore do not prove that A2 characters say the same things.
11. **The remake prologue** follows the attested opening (blocks 1 and 14) as a cutscene; it replays neither the fight nor the Gi Gi at the controls.
12. **The statistics in this document** (22 locations, 33 characters, 8 bosses, 20 mechanics) slice the extraction in my own way; a different slicing may change the totals without changing the conclusions.

## Appendix — Remake additions excluded from the counts

See `docs/remake/ajouts_tranche3.md`. Summary: Orvan (original blacksmith, Paco 36,47), two-branch atomic forge (6 recipes), `rm_ember_blade`, `rm_frost_lance`, `rm_rift_hammer`, five `rm_*` materials, the brazier / frozen path / pillar / mechanism / lever puzzle, material chests (Paco, Kindra, wreck), V4 save (`ameliore`, `interaction`), original sprites (`tools/rm_assets.py`). Dialogues concerned: `rm_forgeron`, `rm_forgeron_forge`, `objet_rm_*`, `rm_brasero_allume`, `rm_chemin_gele`, `rm_pilier_brise`, `rm_mecanisme_*`, `rm_interaction_sans_effet`, `rm_forge_*` (20 IDs).

## Appendix — Remake playable zones (reminder)

| Zone | Displayed name | A1 map | NPCs | Chests | Monsters | Passages |
|---|---|---|---|---|---|---|
| 011 | Village Paco | 011 | 4 + Orvan | herbe (+ 3 `rm_*`) | none | 013 |
| 013 | Forêt Kindra | 013 | Finrod | cœur, lettre (+ 3 `rm_*`) | jelly ×2, turtle ×2, goblin ×2, ogre | 011, 019 |
| 019 | Mont Épervier - Epave | 019 | Alexia, Belgar | gantelet (+ 3 `rm_*`) | droid ×2, goblin | 013, 119 |
| 119 | Toroledo | 119 | 9 | ordre | droid ×2 | 019, 026 (gate `epee_supreme`) |
| 026 | Vallée de Mithra | 026 | Pierre | idole (gate `ordre`), lampe (gate `boss_026`) | golem, jelly, turtle, goblin | 119, 042 (gate `lampe`) |
| 042 | Ruines d'Eden | 028 | Roswell, Omega | prisme (gate `boss_042`) | golem, droid, goblin | 026, 050 (gate `prisme`) |
| 050 | Puerto Medusa | 029 | 9 | cristal (gate `boss_medusa`) | captain, droid, goblin | 042, 086 (gate `boss_medusa`) |
| 086 | Manoir de Milena | 316 | 7 | cœur | droid, goblin | 050, 120 (gate `cristal`) |
| 120 | Salle du trône | 120 | Alexia, Roi | none | Méphisto, droid | 086 |

(38 NPCs, 19 chests, 26 monster spawns in `data/world.txt`, including ten `rm_*` entries: one NPC and nine chests.)

## B01 — Fidelity fixes

Fixes to lines in `data/dialogues.txt` that contradicted an attested Alundra 2 fact (block numbers from the local extraction `extracted/a2_text/dialogues.txt`). IDs, portrait speakers, `_apres` / `_objet_*` variants, gates and page counts unchanged. The documented route choices (D1-D11) remain as they are. Rewritten in the remake's own words, without quoting the script.

| ID | What contradicted A2 | Block(s) | New intent |
|---|---|---|---|
| `georges_puerto` | "Georges" was a sailor looking for "Axelya"; in A2 these are the aliases of Flint and Alexia, and Axelya is a singer with a bounty on her head. The speaker becomes `Habitant` (no portrait, as before). | 262, 263 (also 235, 99) | a resident talks about the singer Axelya and the bounty on her head, alludes to her little brother Georges (a nod to the disguise, without revealing it) and warns that the guards are hunting a pirate hunter |
| `berger_toroledo` (lines 1-2) | Lao is a bullfighting bull from Toroledo who disappears and comes back; he is not linked to the Howling Cave — it is Pierre the gambler who went there. Beasts "transformed into machines" are not attested. | 58, 61, 49, 59 | Lao has disappeared, beasts and people are disappearing, and so is money; Pierre the gambler was seen heading for the Howling Cave. Line 3 (Jeehan) unchanged |
| `guide_vallee` (line 1) | Pierre is not a former bull leader turned guide: he is a ruined gambler, married to Emma and father of Luca. His role as ferryman to the Lampe (route choice D8) is kept. | 49, 50, 59, 291 | Pierre introduces himself as a gambler with no money whom his wife Emma is looking for; the rest (golem, Lampe) unchanged |
| `belgar_epave` (lines 1-2) | Belgar is not Milena's "second": he is her companion and let the boy escape. His presence at the wreck remains an unattested route choice. | 267, 264, 242 | he introduces himself as Milena's companion and says he has not come to fight Flint |
| `milena` (line 2) | Milena no longer "works for Diaz" and has not joined Méphisto: she remains tied to the Baron and opposes the sorcerer (reproaches, intention to refer the matter to the Baron). | 242, 267, 14 | she scorns the Baron's "partner" and does not consider him an ally |
| `milena_objet_ordre` (lines 1-2) | same contradiction ("it's no longer my problem", Méphisto's promise) | 242, 267 | she fears an uprising of the people and still has scores to settle with the Baron's sorcerer, not with Flint |
| `zach_fort` (line 2) | the anecdote "Port Yagin / looking for work" belongs to another Fort Noir character; Zach has already entered once and was turned away. | 115, 213, 215, 217 | Zach says he once pushed open a door without being welcomed and took the wrong corridor |
| `medusa_arrivee` (line 2) | Flint states that Milena's men bombarded Paco; A2 presents the bombing as a false-flag attack (pirate symbol on the ToroTank), which Flint and Alexia judge to be unlike the pirates. | 23, 50, 242 | Paco was bombarded under the pirate emblem, but Flint doubts Milena was behind it; Méphisto is pulling the strings |

Left unchanged (route choices or already documented hypotheses, not fixed here): letter in the forest (D2), gauntlet in the wreck (D3), supreme sword (D4), relics and pickup locations (D5), baron's orders (D6), NPC positions (D7), bosses (D9), Paco intact (D10), money (D11), `belgar_manoir` ("j'ai connu ton père", hypothesis § 6), "hook hand" (unestablished identity). Typos noted but outside the fidelity scope: "roulettez" (`guide_vallee`), "te attend" (`milena_boss_medusa`).

Verification: `tools\validate_data.py` → `ERR: 0 | WARN: 0` ; `tests\check_dialogue_pagination.py` → `ERR: 0`.

## B04 — Paco chief's house and bounty on Flint (narrative part)

**Optional** scene: the A → Z route, its gates and the tested IDs (`prunewell`, `auto_dialogue_*`) do not change. The chief NPC uses the `chef_paco` dialogue (placement: level design). Rewritten in the remake's own words, without quoting the script.

| Element | Status | Detail |
|---|---|---|
| Paco village chief's house; the chief can hand Flint over to collect the reward or cancel the bounty | **attested A2** (block 22 of the local extraction) | starting fact for `chef_paco` |
| Author of the bounty: the pirates' emblem of Milena, Diaz's baron seal just below | **remake-specific** | consistent with the plot (Diaz and the pirates allied; `prunewell` unchanged, Alexia leaves for Mont Épervier) |
| Three-answer question (`Annuler` / `Laisser` / `Plus tard`) with outcomes `chef_paco_annulee`, `chef_paco_maintenue`, `chef_paco_plus_tard` | **remake addition** (B02 choice tool) | events `prime_annulee` / `prime_maintenue`, no effect on the route; "Plus tard" sets nothing |
| Follow-up visits `chef_paco_annulee_suite`, `chef_paco_maintenue_suite` | **remake addition** | `! si` variants at the top of `chef_paco` |
| Reactions from `villageoise` and `garde` (`villageoise_prime_*`, `garde_prime_*`) | **remake addition** | `! si` at the top of existing dialogues; original lines unchanged, combat advice repeated in the variants |

Known limitation: once the letter is found, `villageoise` and `garde` switch to their `_objet_lettre` / `_apres` variants (variant engine priority); the bounty reactions therefore only appear before the letter.

Verification: `tools\validate_data.py` → `ERR: 0 | WARN: 0` ; `tests\check_dialogue_pagination.py` → `ERR: 0`.
