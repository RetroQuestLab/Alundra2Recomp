# PS1 budgets — actual measurements, conversion, per-zone loading

Status: RAM/VRAM/SPU are **measured on the delivered ELF/ISO**; since v0.8.19 the heap peak and MIPS stack depth are also **measured in the PCSX-Redux emulator** by reading `g_ps1Diag`. Since v0.8.22 the **SPU occupancy peak during rendering and CD streaming** is also measured from `g_ps1SpuDiag` (and the VRAM peak since v0.8.21). Frame time is **measured in raw RCnt ticks on the delivered ISO** since the v0.8.19 report (probe shipped in v0.8.6); the ms conversion is explicitly **nominal**, not a calibrated clock measurement. Physical console remains untested. Scope: native PSn00bSDK PS1 target, without changing the PC target. No resource from `extracted/`, `gp/`, `images/` or `assets/` is copied here. Units: `ko`/`Mo` = 10³/10⁶ bytes; `KiB`/`MiB` = 1024/1024² bytes.

Note : les paragraphes historiques ci-dessous reproduisent l'état **déclaré dans chaque rapport de lot**. La sonde de temps d'image existe depuis `v0.8.6-frame-timing` (`PS1_FRAME_TIMING: PASS` sur l'ISO de ce lot) ; les mentions « temps d'image toujours non instrumenté » des rapports v0.8.11 → v0.8.18 sont donc des formulations d'époque, pas l'état courant.

**Relevé livré (2026-11-24, v0.8.25-alexia-kindra)** : `docs/ps1/budget_v0825.md` généré sur l'ELF et l'ISO livrés (`PS1_BUDGET: PASS`) : RAM chargée **1 798 921 / 2 097 152** octets, pic VRAM statique **641 604 / 1 048 576**, SPU (statique) **394 688 / 524 288**. Soit **+2 316 octets RAM** vs v0.8.24 : PNJ Alexia, son sprite A1 (slot VRAM 2 déjà réservé) et son dialogue inédit. Aucune mesure dynamique rejouée sur cette ISO ; lot narratif. Console physique non testée.

**Relevé livré (2026-11-24, v0.8.24-finrod-reunion)** : `docs/ps1/budget_v0824.md` généré sur l'ELF et l'ISO livrés (`PS1_BUDGET: PASS`) : RAM chargée **1 796 605 / 2 097 152** octets, pic VRAM statique **641 604 / 1 048 576**, SPU (statique) **394 688 / 524 288**. Soit **+1 000 octets RAM** vs v0.8.23 : le nouveau dialogue (5 panneaux) et le variant de quête, sans VRAM, CLUT, texture ni audio supplémentaire. Aucune mesure dynamique n'a été rejouée sur cette ISO ; le lot est narratif, pas un lot de mesure. Console physique non testée.

**Relevé livré (2026-11-24, v0.8.23-ogre-progression)** : `docs/ps1/budget_v0823.md` a été généré sur l'ELF et l'ISO livrés (`PS1_BUDGET: PASS`) : RAM chargée **1 795 605 / 2 097 152** octets, pic VRAM **statique 641 604 / 1 048 576**, SPU (calcul statique) **394 688 / 524 288**. Soit **+456 octets RAM** vs v0.8.22 : uniquement les deux fonctions pures `ogreDuelJustWon`/`ogreDuelInteractAllowed` et le pont de quête dans `interaction_ps1.cpp` ; aucune VRAM, CLUT, texture ni audio supplémentaire. Les mesures dynamiques (heap/pile, temps d'image, pics VRAM/SPU réels) **n'ont pas été rejouées sur cette ISO** : elles restent valables pour v0.8.22 et ne sont pas revendiquées ici. Ce lot est un changement de gameplay, pas un lot de mesure. Console physique non testée.

**Relevé livré (2026-11-24, v0.8.22-ps1-spu-peak)** : `docs/ps1/budget_v0822.md` contient les **mesures** faites sur l'ELF et l'ISO livrés (`--diag-log`, `--timing-log`, `--zone-heap-log`, `--vram-log`, `--spu-log`) : RAM 1 795 149 / 2 097 152 octets, pic VRAM statique 641 604 / 1 048 576, SPU (calcul statique) 394 688 / 524 288. **Pic SPU réel pendant le rendu et le streaming CD : 394 688 octets** (75,3 % des 524 288), dont **378 304 octets** de banque VAG et **16 384 octets** de double buffer musical, avec 7 transferts acceptés, 6 chunks de flux, 32 768 octets réécrits en union, 0 rejet et 0 underrun (`qa/ps1_logs/spu_v0822/spu_capture.txt`, `PS1_CAPTURE_SPU: PASS`). `peakFrame=0` est un fait mesuré : le préfill CD synchrone du boot atteint le pic avant la première image présentée et ne le dépasse jamais ; la preuve de streaming vient de `chunks>=2`, `streamBytes==16384`, `transfers>=3` et `underruns==0`, pas de l'horodatage. Méthode : table d'un bit par bloc SPU de 8 octets (8 192 octets de BSS) mise à jour après les deux seuls `SpuWrite` du port, POD `g_ps1SpuDiag` lu passivement en phases menu/game/inventaire. Le chiffre 394 688 était déjà connu par calcul statique ; il est désormais **observé**, les deux valeurs ne se substituent pas. La table ajoute 8 192 octets de BSS (décalage `_end` de 9 156 octets) ; l'arène capture-only passe de 241 664 à **229 376 octets**, laissant 4 088 octets sous `stackPaintLo` et 4 096 au-dessus du pic de zone 225 280. **Pic heap mesuré : 6 144 octets** sur un tas de 229 376 ; **profondeur de pile max : 2 904 octets** ; **pic heap par zone : 225 280 octets** avec retour au baseline (`staticRam=1 860 760`). **Pic VRAM réel : 594 272 octets** (inchangé). **Temps d'image sur CETTE ISO** : ticks min/p50/p95/pic = **18061/18144/18208/60007**, moyenne 18 188,7 ; conversion ms nominale 17,144/17,223/17,283/56,960 ms, 1 outlier conservé à l'index 1. Rendu inchangé : 6/6 PNG natifs identiques à v0.8.21 (`PS1_RENDER_BITWISE: PASS`, `PNG_MISMATCHES=0`). Console physique toujours non testée.

**Relevé livré (2026-11-24, v0.8.21-ps1-vram-peak)** : `docs/ps1/budget_v0821.md` contient les **mesures** faites sur l'ELF et l'ISO livrés (`--diag-log`, `--timing-log`, `--zone-heap-log`, `--vram-log`) : RAM 1 786 029 / 2 097 152 octets, pic VRAM **statique** 641 604 / 1 048 576, SPU 394 688 / 524 288. **Pic VRAM réel pendant le rendu : 594 272 octets** (56,7 % de la VRAM), dont 307 200 octets de framebuffers et **287 072 octets** de texture au-dessus ; atteint à la frame 2 132, avec 4 666 uploads acceptés, 250 112 demi-mots réécrits et 0 rejet (`qa/ps1_logs/vram_v0821/vram_capture.txt`, `PS1_CAPTURE_VRAM: PASS`). Méthode : table d'un bit par demi-mot mise à jour dans `pfUpload4bpp` / `pfUpload16`, POD lu passivement en phases menu/game/inventaire ; le pic statique additionne des rectangles réservés et n'intègre ni remplacements ni uploads réels. Les deux valeurs sont distinctes. La table ajoute 65 536 octets de BSS ; l'arène capture-only passe de 262 144 à 241 664 octets pour rester sous `$sp` tout en couvrant le pic de zone de 225 280 octets. **Pic heap mesuré : 6 144 octets** sur un tas de 241 664 octets ; **profondeur de pile max : 2 904 octets** ; **pic heap par zone : 225 280 octets** avec retour au baseline (`staticRam=1 851 604`). **Temps d'image sur CETTE ISO** : ticks min/p50/p95/pic = **18060/18150/18204/59968**, moyenne 18 188,8 ; conversion ms nominale 17,143/17,228/17,280/56,923 ms, 1 outlier conservé à l'index 1. Aucune valeur FPS ni écart horloge n'est revendiqué. Rendu inchangé : 6/6 PNG natifs identiques à v0.8.20 (`PS1_RENDER_BITWISE: PASS`, `PNG_MISMATCHES=0`). Console physique toujours non testée.

**Relevé livré (2026-11-24, v0.8.19-ps1-heap-stack-diag)** : `docs/ps1/budget_v0819.md` contient les **mesures** faites sur l'ELF et l'ISO livrés (`py -3.12 tools\ps1_budget_report.py … --diag-log qa\ps1_logs\harness_v0819\diag_capture.txt`) : RAM 1 717 873 / 2 097 152 octets, pic VRAM 641 604 / 1 048 576, SPU 394 688 / 524 288. **Pic heap mesuré : 6 144 octets** (`GetHeapUsage` du SDK, champ `alloc_max`) sur un tas de 313 704 octets, soit **1,96 %** du tas et une marge de 307 560 octets. **Profondeur de pile MIPS maximale mesurée : 3 140 octets**, sommet `0x801ffffc`, plancher `0x801ff3b8`, relevée après 2 303 échantillons et plus de 200 frames de gameplay (`qa/ps1_logs/harness_v0819/diag_capture.txt`, `PS1_CAPTURE_DIAG: PASS`). Méthode : peinture `0xA5A5A5A5` d'une fenêtre bornée à 256 Kio sous `$sp`, garde de 512 octets exclue du balayage ; la profondeur rapportée peut donc sous-estimer une pile très superficielle. Ces deux valeurs sont des mesures d'émulateur, **pas** des estimations. Soit **+1 072 octets RAM** vs v0.8.18 (code du diagnostic uniquement), VRAM et SPU inchangés ; rendu inchangé (6/6 PNG bit-à-bit identiques, `qa/ps1_logs/harness_v0819/render_bitwise_proof.txt`). **Temps d'image mesuré sur CETTE ISO** (`tools\ps1_frame_timing.ps1`, `qa\ps1_logs\frame_timing_v0819\timing_stdout.txt`, `PS1_FRAME_TIMING: PASS`) : 1 000 périodes `VSync(0)`, ticks min/p50/p95/pic = **12541 / 18145 / 18206 / 60004**, moyenne 18100,0 ; conversion nominale (1,053497 MHz, non calibrée) 11,904 / 17,224 / 17,281 / 56,957 ms. Un outlier à l'index 1 est conservé sans filtrage. Les ticks bruts sont la mesure, la conversion ms reste nominale ; aucune valeur FPS n'est revendiquée. Les estimations de conversion ci-dessous restent des estimations.

**Relevé livré (2026-11-24, v0.8.18-ps1-intro-scene-sync)** : `docs/ps1/budget_v0818.md` contient les **mesures** faites sur l'ELF et l'ISO livrés (`py -3.12 tools\ps1_budget_report.py …` avec chemin ELF/ISO explicite) : RAM 1 716 801 / 2 097 152 octets, pic VRAM 641 604 / 1 048 576, SPU 394 688 / 524 288 ; temps d'image toujours non instrumenté. Valeurs **identiques à v0.8.17** : le lot ne change que trois constantes de frontière (`648/1728/1944`), sans nouvel octet de code, d'asset ni d'audio. Les estimations de conversion ci-dessous restent des estimations, pas des mesures.

**Relevé précédent (2026-11-24, v0.8.17-ps1-menu-hint-blue)** : `docs/ps1/budget_v0817.md` contient les **mesures** faites sur l'ELF et l'ISO livrés (`py -3.12 tools\ps1_budget_report.py …` avec chemin ELF/ISO explicite) : RAM 1 716 801 / 2 097 152 octets, pic VRAM 641 604 / 1 048 576, SPU 394 688 / 524 288 ; temps d'image toujours non instrumenté. Soit **+32 octets RAM et +32 octets VRAM** vs v0.8.16 (`docs/ps1/budget_v0816.md`) : uniquement la 7ᵉ CLUT FONT3 (16 demi-mots) ajoutée pour les aides de pied de page du menu, sans octet audio ni asset externe. Les estimations de conversion ci-dessous restent des estimations, pas des mesures.

**Relevé précédent (2026-11-24, v0.8.16-ps1-options-contrast)** : `docs/ps1/budget_v0816.md` contient les **mesures** faites sur l'ELF et l'ISO livrés (`py -3.12 tools\ps1_budget_report.py …` avec chemin ELF/ISO explicite) : RAM 1 716 769 / 2 097 152 octets, pic VRAM 641 572 / 1 048 576, SPU 394 688 / 524 288 ; temps d'image toujours non instrumenté. Soit **+24 octets RAM et +32 octets VRAM** vs v0.8.15 (`docs/ps1/budget_v0815.md`) : uniquement la 6ᵉ CLUT FONT3 (16 demi-mots) ajoutée pour les libellés d'options, sans octet audio ni asset externe. Les estimations de conversion ci-dessous restent des estimations, pas des mesures.

**Relevé précédent (2026-11-24, v0.8.15-remake-subtitle-orange)** : `docs/ps1/budget_v0815.md` contient les **mesures** faites sur l'ELF et l'ISO livrés (`py -3.12 tools\ps1_budget_report.py …` avec chemin ELF/ISO explicite) : RAM 1 716 745 / 2 097 152 octets, pic VRAM 641 540 / 1 048 576, SPU 394 688 / 524 288 ; temps d'image toujours non instrumenté. Soit **+32 octets RAM et +32 octets VRAM** vs v0.8.14 (`docs/ps1/budget_v0814.md`) : uniquement la 5ᵉ CLUT FONT3 (16 demi-mots) ajoutée pour le `- remake -` orange du titre, sans octet audio ni asset externe. Le script lit le lot pointé par `ps1/a_graver/DERNIERE_VERSION.txt` (plus de chemin v0.8.2 codé en dur) et précise la différence entre BIN MODE2/2352 (61 239 024 octets) et ISO 2048 gravable (53 323 776 octets). Les estimations de conversion ci-dessous restent des estimations, pas des mesures.

**Relevé précédent (2026-11-24, v0.8.14-menu-selection-parite)** : `docs/ps1/budget_v0814.md` contient les **mesures** faites sur l'ELF et l'ISO livrés (`py -3.12 tools\ps1_budget_report.py …` avec chemin ELF/ISO explicite) : RAM 1 716 713 / 2 097 152 octets, pic VRAM 641 508 / 1 048 576, SPU 394 688 / 524 288 ; temps d'image toujours non instrumenté. Ces valeurs sont **identiques à v0.8.13** (`docs/ps1/budget_v0813.md`) : le correctif ne touche que des primitives de rendu, sans nouvel asset, ni CLUT, ni octet audio. v0.8.13 avait ajouté +824 octets RAM vs v0.8.12 (validateur et garde-fous du blob HUD), zéro VRAM/audio. Le script lit le lot pointé par `ps1/a_graver/DERNIERE_VERSION.txt` (plus de chemin v0.8.2 codé en dur) et précise désormais la différence entre BIN MODE2/2352 (61 239 024 octets) et ISO 2048 gravable (53 323 776 octets). Les estimations de conversion ci-dessous restent des estimations, pas des mesures.

## 1. Exact measurements (bytes)

Method: PowerShell `Get-ChildItem … | Measure-Object -Property Length` and Python 3.12 stdlib (`pathlib`, `wave`, `struct`), read-only. Validator run: `py -3.12 tools\validate_data.py` → `dialogues 149 | sprites 44 | zones 10 | passages 19`, `ERR: 0 | WARN: 0`. Four uncommitted B04 files (`data/dialogues.txt`, `data/warps.txt`, `data/world.txt`, `docs/remake/couverture_contenu.md`) were present during the measurements and were **left untouched**.

### 1.1 `data/*.txt` — 11 files, **63 572 bytes** (62,1 KiB)

| File | Bytes | Category |
|---|---:|---|
| `dialogues.txt` | 39 083 | text/dialogues |
| `world.txt` | 7 029 | world/zones |
| `items.txt` | 6 846 | items |
| `item_icons.txt` | 2 174 | items/icons |
| `warps.txt` | 2 079 | world/passages |
| `rm_interactions.txt` | 1 444 | remake additions |
| `rm_forge.txt` | 1 256 | remake additions |
| `sfx.txt` | 1 247 | audio (event table) |
| `rm_weapons.txt` | 1 085 | remake additions |
| `rm_materials.txt` | 697 | remake additions |
| `music.txt` | 632 | audio (categories) |

Breakdown: dialogues 39 083; world (world+warps) 9 108; items 9 020; `rm_*` additions 4 482; audio tables 1 879. **Total 63 572**.

Actual logical count: **149 blocks** in `data/dialogues.txt`, **339 message lines**, 14 `! ` actions, 2 `? ` choice lines. The "**553 messages**" cited in internal documentation come from `extracted/a2_text/dialogues.txt` (164 351 bytes, 815 non-empty lines = 262 `## bloc N` headers + 553 message lines, themselves segmented by ` | `). These are two distinct datasets: the port loads the remake's 39 083 bytes, not the A2 extraction. `extracted/a2_text/menus.txt` = 103 180 bytes, not loaded.

### 1.2 Font / windows — source vs embedded data

| Item | Bytes | Nature |
|---|---:|---|
| `src/a1_font.h` (source text) | 294 156 | 287,3 KiB of C++ literals (`0x..`) |
| `kGlyphBits` | 8 192 | 256 16×16 glyphs, 1 bit/px |
| `kAdvance` | 256 | advances |
| `kFontPalette` | 64 | 16 RGBA colors |
| `kWindPixels` | 65 536 | decompressed 4bpp indices (1 byte/px) |
| `kWindPalette` | 64 | 16 RGBA colors |
| `kWindPaletteBanks` | 1 024 | 16 banks × 16 RGBA colors |
| **Total PC binary constants** | **75 136** | 73,4 KiB, excluding code |
| `extracted/a1/TAKI/SCREEN/FONT3.TIM` | 32 832 | 4bpp TIM 256×256 + CLUT (native source) |
| `extracted/a1/TAKI/SCREEN/WIND.TX` | 32 768 | 4bpp plane 256×256 |
| `extracted/a1/TAKI/SCREEN/WIND.CL` | 512 | 16 CLUT banks × 16 BGR555 colors |
| **Total useful native textures** | **66 112** | 64,6 KiB |

PS1 implication: do not embed the 294 156 bytes of text or the 65 536 decompressed indices. Store `kAdvance` (256 B) in RAM, send `FONT3.TIM` and `WIND.TX` to VRAM by DMA, keep the CLUTs in VRAM.

### 1.3 Game assets — `assets/`: **2 542 568 410 bytes** (2,37 GiB, 905 files)

| Extension | Files | Bytes | Role |
|---|---:|---:|---|
| `.wav` | 241 | 2 272 262 120 | music/sound effects (16-bit PCM source) |
| `.a1m` | 483 | 264 205 186 | 52×60 maps, pre-colored 24×16 RGBA tiles |
| `.a1s` | 44 | 6 019 748 | sprite banks (1 154 frames, RGBA + animations) |
| `.png` | 91 | 54 459 | UI previews |
| `.txt` | 6 | 26 681 | UI/SFX indexes |
| `.loop` | 40 | 216 | loop markers |

Map details: `A1MP` header, 10 bytes, then `nb_tuiles × 1 536` bytes of RGBA, then 8 bytes/cell (52×60 = 3 120 cells → 24 960 B), then a wall table in `u16`. The largest map in use, `011.a1m`, is 1 294 228 bytes (821 tiles, 1 261 056 B of RGBA, 24 960 B of cells, 8 202 walls = 16 404 B). Not all 483 maps on disk are used: the game references **13 files** (10 zones + 3 replacements `028`, `029`, `316`), for **7 812 238 bytes** in total; 4bpp conversion = **2 042 886 bytes** for these 13 maps. Converting all 483 maps to 4bpp would yield 69 321 170 bytes — unnecessary for the game.

Sprite details: 44 banks / 1 154 frames / 6 019 748 source bytes. 4bpp conversion with PS1 alignment (rows rounded to 16 bytes) = **1 054 400 bytes**, already **more than the entire VRAM** if everything were resident. `hero.a1s`: 1 247 720 B, 184 frames, 9 groups, 217 136 B aligned 4bpp; the idle/walk/attack subset (groups 0,1,2) = 60 frames, **64 352 B**. `a1_hud.a1s`: 22 312 B, 36 frames, 8 960 B converted. `a1_icons.a1s`: 234 120 B, 84 frames, 39 584 B converted. Per zone, the largest dialogue is zone `119`: 23 IDs, 5 066 UTF-8 bytes (zone `011`: 27 IDs, 6 931 B — the largest volume).

### 1.4 Source audio — 241 WAV files, **2 272 262 120 bytes**

Headers read with `wave` (actual sample rate/channels/depth):

| Folder | WAV files | Bytes | Cumulative duration | Measured format |
|---|---:|---:|---:|---|
| `assets/music_a2/` (+`tracks/`) | 109 | 1 484 549 396 | 9 818,4 s | 37 800 Hz, 2 channels, 16 bits |
| `assets/music/` | 40 | 694 047 804 | 3 934,5 s | 44 100 Hz, 2 channels, 16 bits |
| `assets/mono_a2/` | 17 | 79 979 500 | 996,9 s | 37 800 Hz, 16 mono / 1 stereo, 16 bits |
| `assets/sfx/` | 75 | 13 685 420 | 77,6 s | 44 100 Hz, 2 channels, 16 bits |

`data/music.txt` references **37 unique tracks** (village 8, plain 5, dungeon 16, boss 3, jingles 5): 658 072 700 bytes of WAV, 3 730,6 s. `data/sfx.txt` references **15 IDs**: 1 243 672 bytes, 7,05 s (the longest, ID 41, 218 904 B / 1,24 s). The `music_a2` tracks (up to 641 s, 96,9 Mo per file) are **not** referenced by `data/music.txt`: they should not be mastered initially.

### 1.5 Unpublished local references (measured, not copied)

| Location | Files | Bytes | Comment |
|---|---:|---:|---|
| `extracted/` | 236 | 884 912 910 | 844,1 MiB; films/PAK excluded from the port |
| `gp/` | 15 | 753 831 | reference A1 captures only |
| `images/` | 20 | 7 918 519 | captures/previews, not embedded |

The largest files in `extracted/`: `GRP_FR.GP` 206 905 344, `a1/MOVIE/ARAN_END.MOV` 135 608 320, `a1/DATA/DATAS.BIN` 109 103 104, `a1/MOVIE/ARAN_OP.MOV` 77 821 952, `a1/ENDING.XA` 77 725 696, `PAKVCAST.PAK` 74 354 688, `a1/MOVIE/EURO_OP.MOV` 57 753 600, `a1/DATA/NULL.NUL` 33 333 333, `PACK0.PAK` 16 516 780, `a1/ARAN_XA.XA` 14 614 528, `a1/DATA/SOUND.BIN` 12 185 600.

Subset actually useful to the port: `DATAS.BIN` (source of A1 maps/sprites), `SOUND.BIN`, `PACK0.PAK` (A2 source of the WAVs), `FONT3.TIM`/`WIND.TX`/`WIND.CL` (66 112 B), `a2_text/` (267 531 B, narrative reference) = **138 139 127 bytes**. The files `gp/images.png` (42 041 B) and `gp/images (1).png` (42 041 B), `imgres.htm` (226 023 B) are used only for visual validation. None of these files should go into the BIN/CUE.

## 2. PS1 conversion — CD / RAM / VRAM

Assumptions: 4 bpp = 4 bits/texel + 16-color CLUT (32 B per bank); SPU ADPCM 4 bits/sample; no XA/FMV video in the port; 1× 650 Mo BIN/CUE (2× drive, sustained 300 Ko/s).

| Data | Source measurement | Size on CD | Resident RAM | VRAM |
|---|---:|---:|---:|---:|
| Text `data/*.txt` | 63 572 B | 63,6 ko | 39,1 ko for full dialogues, otherwise ≤6,9 ko/zone | 0 |
| Font `FONT3.TIM` | 32 832 B | 32,8 ko | 256 B (advances) | 32 768 B + CLUT 32 B |
| Window `WIND.TX` + `WIND.CL` | 33 280 B | 33,3 ko | ~0 | 32 768 B + CLUT 512 B (16 banks) |
| Maps (13 used) | 7 812 238 B | **2 042 886 B** | cells+walls ≤41 364 B (map 011, before compaction) | ≤307 900 B for a full map; ≤160 ko working set |
| Sprites (44 banks) | 6 019 748 B | 1 054 400 B (aligned 4bpp) | animation indices ~10 ko | never everything; 64 352 B (minimal hero), 8 960 B HUD |
| Referenced music (37) | 658 072 700 B | **156,9 MiB** in 44,1 kHz stereo ADPCM; **39,2 MiB** in 22,05 kHz mono | 0 (streaming) | 0 |
| Referenced SFX (15) | 1 243 672 B | 303,6 KiB in 44,1 kHz stereo ADPCM; 75,9 KiB in 22,05 kHz mono | ID table ~1 ko | 0 |
| Unreferenced music | 1 600 504 000 B (~1,49 GiB) | **to exclude from the master** | 0 | 0 |

Total audio if everything were converted at native frequency in 4 bits: ~567 Mo (541 MiB). At 22,05 kHz mono: ~163,5 Mo (156 MiB). The recommended master contains only the 37 tracks and 15 SFX actually used: ≈165 Mo (156,9 MiB) at native quality or ≈41 Mo (39,2 MiB) at 22,05 kHz mono, well under 650 Mo.

ADPCM CD bitrate: 44 100 B/s (43,1 KiB/s) at 44,1 kHz stereo, 11 025 B/s (10,8 KiB/s) at 22,05 kHz mono — i.e. **15 %** or **3,7 %** of the sustained 300 Ko/s. The 22,05 kHz mono choice is the one that protects per-zone loading; 44,1 kHz stereo remains possible but consumes ≈165 Mo (156,9 MiB) of CD and must be validated by ear.

Tile caveat: `.a1m` stores tiles **already colored in RGBA**, without a CLUT. A naive quantization conversion yields 672 unique patterns and 659 "palettes" for map 011 (258 048 B of pixels + 21 088 B of estimated CLUT); the native A1 source (`DATAS.BIN`, sections 0/2) provides 16 palettes of 16 colors per zone. The 4bpp index + 16 CLUTs must be rebuilt from `DATAS.BIN`, otherwise the CLUT explodes and the VRAM budget is exceeded. The 659 "palettes" are a measurement artifact (color appearance order), not a game property.

## 3. Per-zone loading strategy

**Permanent resident**: MIPS code, heap/stack, `kAdvance` table (256 B), `FONT3.TIM` + CLUT in VRAM (32,8 ko), `WIND.TX` + 16 CLUTs in VRAM (33,3 ko), HUD (`a1_hud.a1s`, 8 960 B 4bpp), CD reader, PAD manager, dialogue parser, save state, small audio stream buffers (2 × 16 ko in RAM/SPU).

**Loaded on zone entry**: 4bpp map tiles (≤307,9 ko for `011`), cell/wall tables (41 364 B raw, ≤29 ko after `u16→u8` compaction of unused fields), zone icons/NPCs (≤40 ko of 4bpp sprites), zone dialogues (≤6 931 B), local passage table (2 079 B for all 19 passages), plus a short audio track/jingle if the zone requests one. The hero keeps the idle/walk/attack subset in VRAM (60 frames, 62,8 ko); the other groups (run 20,3 ko, charge 63,7 ko, etc.) are loaded on demand. **The previous zone is explicitly released before any allocation for the next one** (tiles, sprites, dialogue indices, SPU blocks); a 011 → 013 → 026 → 011 leak test must return to the same memory high-water mark. The `iso.xml` manifest is ordered according to this actual load order, not alphabetically.

**Streamed**: category music (`village`/`plaine`/`donjon`/`boss`) and jingles via CD→SPU double buffering; SFX preloaded in banks of 15 IDs ≤304 ko on CD, with an SPU working set ≤256 ko.

**Load order compatible with 300 Ko/s** (worst case zone `011`): 1) freeze the screen + fade (0,2 s); 2) read header + cells/walls (~29 ko); 3) DMA the 4bpp tiles (307,9 ko); 4) DMA zone sprites (~40 ko); 5) read local dialogues (6,9 ko); 6) start the BGM stream, lift the fade. Payload ≈384 ko → **1,28 s at 300 Ko/s**; with seek latency and margin, **2,5 s budget per transition**. Preloading neighboring zones (19 passages) must run in the background at ≤50 Ko/s during gameplay, never during rendering.

## 4. Measurable budgets and measurement points

| Metric | Target | Hard limit | Verification |
|---|---:|---:|---|
| Main RAM peak | ≤1 500 KiB | 2 048 KiB | high-water counter of the PSn00bSDK `malloc`/`free` wrapper + `mipsel-none-elf-size -A` on the ELF (`.text+.data+.bss`); log at warp and menu |
| VRAM peak | ≤896 KiB | 1 048 576 B | GPU upload instrumentation (TIM/TX rects + framebuffers); sum of allocated blocks, peak log |
| Framebuffers | 307 200 B (2 × 320×240×2 bytes, 15/16 bpp) | — | configured once, never reallocated; `DoubleBuffer` + `PutDispEnv`/`PutDrawEnv` |
| SPU RAM | ≤384 KiB | 512 KiB | tracking of ADPCM voice addresses/sizes + reverb; sum ≤393 216 B |
| Payload per zone | ≤640 KiB | 1 MiB | `CdRead` instrumentation (bytes + ticks); measured on the 13 referenced maps |
| Transition time | ≤2,5 s | 4 s | PSn00bSDK timestamps around zone loading, emulator log |
| Sustained CD bitrate | ≤60 KiB/s (outside transitions) | 300 Ko/s (307,2 KiB/s) | bytes-read counter / time, compared with the 2× bitrate |
| Frame time (60 Hz NTSC presentation) | 16,67 ms per frame; p50/p95 ≤16,67 ms, peak ≤33,3 ms (1 VBlank) over 1 000 frames | 33,3 ms outside transitions; 50 ms in transition | histogram from the R3000 timer/root counter + `VSync`; correlation with DuckStation log `VPS` |
| Presentation | 60 Hz NTSC, `VPS ≥ 50` | — | milestone 1 smoke test measured: OpenBIOS, `VPS: 59,94`; in headless mode `FPS: 0` is normal (no frame presentation) |

**Choice of 60 Hz presentation, 60 Hz logic**: the milestone 1 smoke test measures 59,83 frames/s in NTSC and the PS1 budget sets frame time at 16,67 ms. Ticks remain at 1/60 s (`src/sprite.h` animations and `--auto` scenarios depend on them) and input is sampled at 60 Hz. If actual measurements show p95 exceeding 16,67 ms, falling back to **30 Hz rendering** (33,3 ms per frame, logic and input still at 60 Hz) is possible only after the variance has been approved architecturally — never by reducing resolution or changing colors. This report does not adopt 30 Hz by default: there is no MIPS measurement justifying it.

Concrete measurement points: `mipsel-none-elf-size -A <elf>` (tool in `external/tools/psn00bsdk/PSn00bSDK-0.24-win32/bin`) for `.text`/`.data`/`.bss`, allocation and `CdRead` wrappers with high-water mark and fragmentation, GPU/SPU upload counting, p50/p95/peak histogram over **1 000 frames** via root counter + `VSync`, and an `.a1m`/`.a1s` header analysis script (formats documented in `tools/export_assets.py` and `src/sprite.cpp`) checking the thresholds above. Every budget report must name the command, the measurement moment, and the observed value, then the variance from budget; an estimate will never be presented as a measurement.

DuckStation measurement pitfall: in `-nogui` there is no frame presentation, so `FPS: 0` is **normal**; it is `V/PerfMon: … VPS: ~60` that validates emulation. Do not conclude a crash from `FPS: 0`. Since DuckStation rewrites its log on each launch, read the whole file and take the **last** occurrence of each marker. The reproducible smoke test is `powershell -ExecutionPolicy Bypass -File tools\ps1_smoke.ps1 -Seconds 15` with `PS1_SMOKE: PASS` and `VPS ≥ 50` (test tool, unchanged here). On the PC side, `py -3.12 tools\validate_data.py` must stay at `dialogues 149 | sprites 44 | zones 10 | passages 19`, `ERR: 0 | WARN: 0` after any data change, and `ctest --test-dir build -N` must stay green. These PS1 checks are to be created in milestone 2; this report modifies no tool.

### Actual PS1 peaks — heap, pile, pics par zone/VRAM/SPU mesurés sur les lots livrés

| Metric | Command / moment | Observed value | Variance from budget |
|---|---|---|---|
| Pic heap émulateur (v0.8.20) | `GetHeapUsage().alloc_max` via `g_ps1Diag`, après >200 frames de jeu (`tools/ps1_capture_diag.ps1`) | **6 144 octets** | 2,34 % du tas `InitHeap` de 262 144 octets ; marge 256 000 |
| Profondeur pile MIPS max (v0.8.20) | peinture `0xA5A5A5A5`, garde 512 octets, fenêtre au-dessus de l'arène, balayage toutes les 60 images, 2 312 échantillons | **2 904 octets** (`0x801fff10` → `0x801ff3b8`) | sous-estimation possible de la garde ; la fenêtre est bornée par l'arène `malloc` élargie |
| RAM chargée (ELF livré) | `mipsel-none-elf-size -A`, sections chargées | 1 719 445 octets (v0.8.20) | 82,0 % de 2 097 152 ; marge 377 707 |
| Peak RAM (zone 011, blob CD) | `HeapUsage.alloc` pendant `loadZone`, harnais `tools/ps1_capture_zone_heap.ps1` | **225 280 octets** d'allocation courante, 220 006 octets lus | mesure capture-only ; le jeu normal garde son blob `incbin` et n'alloue rien |
| Pic global heap (séquence 011→013→026→011) | `HeapUsage.alloc_max` sur 4 commandes, `baselineAlloc=finalAlloc=4096` | **225 280 octets**, 3 libérations explicites (`qa/ps1_logs/zone_heap_v0820/zone_heap_capture.txt`) | retour exact au baseline ; aucun octet de zone conservé |
| Peak VRAM | GPU allocation table, during rendering | not measured (pic statique 641 604 octets mesuré sur l'ELF/ISO) | — |
| Pic SPU (v0.8.22, ISO livrée) | table d'occupation `g_ps1SpuDiag`, 1 bit / bloc de 8 octets, 3 phases + preuve streaming (`tools/ps1_capture_spu.ps1`) | **394 688 octets mesurés** = 378 304 banque + 16 384 double buffer, 7 transferts, 6 chunks, 32 768 réécrits, 0 rejet/underrun (`qa/ps1_logs/spu_v0822/spu_capture.txt`) | 75,3 % de 524 288 ; marge 129 600 ; le calcul statique coïncide mais n'est plus la seule preuve |
| Temps d'image (v0.8.20, ISO livrée) | `tools\ps1_frame_timing.ps1`, 1 000 périodes `VSync(0)`, MMIO `0x1f801110` | **ticks min/p50/p95/pic 18063/18143/18216/60051**, moyenne 18816,4 (`qa/ps1_logs/frame_timing_v0820/timing_stdout.txt`) | p50/p95 ~17,2 ms **nominaux**, au-dessus de la cible nominale 16,67 ms ; conversion non calibrée, aucun écart horloge revendiqué ; 16 outliers conservés (pic à l'index 958) |
| `VPS` DuckStation | `tools/ps1_smoke.ps1 -Seconds 15`, last occurrence of `V/PerfMon` | 59,83 measured during the SDK template smoke test (the log may vary around 59,9) | compliant (`≥50`) |

The `59,83` above comes from the **SDK template** smoke test, not from the ported game; it validates no rendering budget of the remake. Les valeurs heap/pile/pic par zone v0.8.20 viennent du **jeu porté** sur l'ISO livrée, pas du template. Les valeurs v0.8.19 restent dans `docs/ps1/budget_v0819.md` (tas alors de 313 704 octets, pile 3 140 octets, ticks p50 18145) et ne sont pas mélangées au lot v0.8.20.

## 5. Major consumers and overrun risks

1. **PCM audio (2 272 262 120 B, 89,4 % of `assets/`)**: risk #1. Converting the 241 files to native ADPCM yields ~567 Mo on CD and requires permanent streaming; including unreferenced `music_a2` tracks wastes 1 484 549 396 B (~1,38 GiB) of source. Chosen mitigation: 37 tracks + 15 SFX, 22,05 kHz mono by default (39,2 Mo + 76 KiB), with a 44,1 kHz stereo option to be validated by ear.
2. **A1 maps (264 205 186 B, 483 files)**: naive RGBA→VRAM conversion = 1263,9 KiB for map 011 alone; with framebuffers (300 KiB), font+window (65,6 KiB), full hero (212,1 KiB) and HUD (8,8 KiB), this reaches ~893 KiB before enemies. Mitigation: only 13 maps (7,45 MiB source → 1,95 MiB CD), tile working set ≤160 KiB, compacted cells/walls, minimal hero 62,8 KiB.
3. **Tile palettes**: pre-colored `.a1m` has no CLUT; heuristic measurement yields up to 659 palettes for one map, versus 16 native palettes in `DATAS.BIN`. Without recovering the native palettes, the CLUT and conversion code explode.
4. **Sprites (6 019 748 B → 1 054 400 B 4bpp)**: the 44 banks do not fit in 1 MiB of VRAM on their own. Only the zone subset + minimal hero may reside; load the other banks on demand.
5. **Font (294 156 B source, 75 136 B of PC constants)**: embedding the C++ header or the 65 536 decompressed indices wastes 287+64 KiB on CD/RAM. Send TIMs to VRAM, keep only 256 B of advances.
6. **CD and transitions**: 19 passages, largest map 1 294 228 B source; reading the whole game at startup is impossible (2,37 GiB of `assets/`). Neighbor preloading must stay ≤50 Ko/s so as not to disturb audio streaming.
7. **Dialogue count**: 149 blocks / 339 messages in the remake versus 262 blocks / 553 lines in `extracted/a2_text/dialogues.txt`. Do not size the loader or RAM around 553: the useful value is 6 931 B for the most talkative zone.

**Main overrun risk**: VRAM, via the sum "full map + full hero + double framebuffer" (~893 KiB before enemies/effects), worsened by the absence of CLUTs in pre-colored `.a1m` files. The second risk is audio SPU/CD if the 241 WAVs are mastered instead of the 37 referenced tracks.

---
Reproducible measurements without writing to the repository: file sizes via `Get-ChildItem … | Measure-Object -Property Length`, WAV headers via `python -c "import wave …"`, `.a1m` headers (`<HHH` at offset 4) and `.a1s` (`A1SP`, `u16` frames then `HHhh` per frame) via `struct`, `a1_font.h` arrays via regex on `std::array`. No other resource was opened for writing, no commit, no push.
