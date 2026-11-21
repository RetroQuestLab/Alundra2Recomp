# Budgets PS1 — mesures réelles, conversion, chargement par zone

Statut : **mesures host effectuées le 2026-11-21 sur l’état réel du dépôt** ; budgets et pics PS1 = **cibles à valider par instrumentation**, pas encore mesurés sur console/émulateur (aucun port MIPS compilé dans ce rapport). Périmètre : cible PS1 native PSn00bSDK, sans modifier la cible PC. Aucune ressource de `extracted/`, `gp/`, `images/` ou `assets/` n’est copiée ici . Unités : `ko`/`Mo` = 10³/10⁶ octets ; `KiB`/`MiB` = 1024/1024² octets.

## 1. Mesures exactes (octets)

Méthode : PowerShell `Get-ChildItem … | Measure-Object -Property Length` et Python 3.12 stdlib (`pathlib`, `wave`, `struct`), lecture seule. Validateur exécuté : `py -3.12 tools\validate_data.py` → `dialogues 149 | sprites 44 | zones 10 | passages 19`, `ERR: 0 | WARN: 0`. Quatre fichiers B04 non commités (`data/dialogues.txt`, `data/warps.txt`, `data/world.txt`, `docs/remake/couverture_contenu.md`) étaient présents pendant les mesures et ont été **laissés intacts**.

### 1.1 `data/*.txt` — 11 fichiers, **63 572 octets** (62,1 KiB)

| Fichier | Octets | Catégorie |
|---|---:|---|
| `dialogues.txt` | 39 083 | texte/dialogues |
| `world.txt` | 7 029 | monde/zones |
| `items.txt` | 6 846 | objets |
| `item_icons.txt` | 2 174 | objets/icônes |
| `warps.txt` | 2 079 | monde/passages |
| `rm_interactions.txt` | 1 444 | ajouts remake |
| `rm_forge.txt` | 1 256 | ajouts remake |
| `sfx.txt` | 1 247 | audio (table d’événements) |
| `rm_weapons.txt` | 1 085 | ajouts remake |
| `rm_materials.txt` | 697 | ajouts remake |
| `music.txt` | 632 | audio (catégories) |

Répartition : dialogues 39 083 ; monde (world+warps) 9 108 ; objets 9 020 ; ajouts `rm_*` 4 482 ; tables audio 1 879. **Total 63 572**.

Comptage logique réel : **149 blocs** dans `data/dialogues.txt`, **339 lignes de messages**, 14 actions `! `, 2 lignes de choix `? `. Les « **553 messages** » cités dans la documentation interne viennent de `extracted/a2_text/dialogues.txt` (164 351 octets, 815 lignes non vides = 262 en-têtes `## bloc N` + 553 lignes de messages, elles-mêmes segmentées par ` | `). Ce sont deux jeux de données distincts : le port charge les 39 083 octets du remake, pas l’extraction A2. `extracted/a2_text/menus.txt` = 103 180 octets, non chargé.

### 1.2 Police / fenêtres — source vs données embarquées

| Élément | Octets | Nature |
|---|---:|---|
| `src/a1_font.h` (texte source) | 294 156 | 287,3 KiB de littéraux C++ (`0x..`) |
| `kGlyphBits` | 8 192 | 256 glyphes 16×16, 1 bit/px |
| `kAdvance` | 256 | avances |
| `kFontPalette` | 64 | 16 couleurs RGBA |
| `kWindPixels` | 65 536 | index 4bpp décompressés (1 octet/px) |
| `kWindPalette` | 64 | 16 couleurs RGBA |
| `kWindPaletteBanks` | 1 024 | 16 banques × 16 couleurs RGBA |
| **Total constant binaire PC** | **75 136** | 73,4 KiB, hors code |
| `extracted/a1/TAKI/SCREEN/FONT3.TIM` | 32 832 | TIM 4bpp 256×256 + CLUT (source native) |
| `extracted/a1/TAKI/SCREEN/WIND.TX` | 32 768 | plan 4bpp 256×256 |
| `extracted/a1/TAKI/SCREEN/WIND.CL` | 512 | 16 banques CLUT × 16 couleurs BGR555 |
| **Total textures natives utiles** | **66 112** | 64,6 KiB |

Conséquence PS1 : ne pas embarquer les 294 156 octets de texte ni les 65 536 index décompressés. Stocker `kAdvance` (256 B) en RAM, envoyer `FONT3.TIM` et `WIND.TX` en VRAM par DMA, garder les CLUT en VRAM.

### 1.3 Ressources de jeu — `assets/` : **2 542 568 410 octets** (2,37 GiB, 905 fichiers)

| Extension | Fichiers | Octets | Rôle |
|---|---:|---:|---|
| `.wav` | 241 | 2 272 262 120 | musiques/bruitages (source PCM 16 bits) |
| `.a1m` | 483 | 264 205 186 | cartes 52×60, tuiles RGBA 24×16 pré-colorisées |
| `.a1s` | 44 | 6 019 748 | banques de sprites (1 154 frames, RGBA + anims) |
| `.png` | 91 | 54 459 | aperçus UI |
| `.txt` | 6 | 26 681 | index UI/SFX |
| `.loop` | 40 | 216 | marqueurs de boucle |

Détails cartes : en-tête `A1MP`, 10 octets, puis `nb_tuiles × 1 536` octets RGBA, puis 8 octets/case (52×60 = 3 120 cases → 24 960 B), puis table de murs en `u16`. La plus grosse carte utilisée, `011.a1m`, fait 1 294 228 octets (821 tuiles, 1 261 056 B de RGBA, 24 960 B de cases, 8 202 murs = 16 404 B). Les 483 cartes présentes sur disque ne sont pas toutes utilisées : le jeu référence **13 fichiers** (10 zones + 3 remplacements `028`, `029`, `316`), soit **7 812 238 octets** au total ; conversion 4bpp = **2 042 886 octets** pour ces 13 cartes. Les 483 cartes converties en 4bpp feraient 69 321 170 octets — inutile pour le jeu.

Détails sprites : 44 banques / 1 154 frames / 6 019 748 octets source. Conversion 4bpp avec alignement PS1 (lignes arrondies à 16 octets) = **1 054 400 octets**, soit déjà **plus que la VRAM entière** si tout était résident. `hero.a1s` : 1 247 720 B, 184 frames, 9 groupes, 217 136 B en 4bpp aligné ; le sous-ensemble repos/marche/attaque (groupes 0,1,2) = 60 frames, **64 352 B**. `a1_hud.a1s` : 22 312 B, 36 frames, 8 960 B converti. `a1_icons.a1s` : 234 120 B, 84 frames, 39 584 B converti. Par zone, le dialogue maximal est zone `119` : 23 identifiants, 5 066 octets UTF-8 (zone `011` : 27 identifiants, 6 931 B — le plus gros volume).

### 1.4 Audio source — 241 fichiers WAV, **2 272 262 120 octets**

En-têtes lus avec `wave` (fréquence/canaux/profondeur réels) :

| Dossier | Fichiers WAV | Octets | Durée cumulée | Format mesuré |
|---|---:|---:|---:|---|
| `assets/music_a2/` (+`tracks/`) | 109 | 1 484 549 396 | 9 818,4 s | 37 800 Hz, 2 canaux, 16 bits |
| `assets/music/` | 40 | 694 047 804 | 3 934,5 s | 44 100 Hz, 2 canaux, 16 bits |
| `assets/mono_a2/` | 17 | 79 979 500 | 996,9 s | 37 800 Hz, 16 mono / 1 stéréo, 16 bits |
| `assets/sfx/` | 75 | 13 685 420 | 77,6 s | 44 100 Hz, 2 canaux, 16 bits |

`data/music.txt` référence **37 pistes uniques** (village 8, plaine 5, donjon 16, boss 3, jingles 5) : 658 072 700 octets de WAV, 3 730,6 s. `data/sfx.txt` référence **15 identifiants** : 1 243 672 octets, 7,05 s (le plus long, ID 41, 218 904 B / 1,24 s). Les pistes `music_a2` (jusqu’à 641 s, 96,9 Mo par fichier) ne sont **pas** référencées par `data/music.txt` : elles ne doivent pas être masterisées dans un premier temps.

### 1.5 Références locales non publiées (mesurées, non copiées)

| Emplacement | Fichiers | Octets | Commentaire |
|---|---:|---:|---|
| `extracted/` | 236 | 884 912 910 | 844,1 MiB ; films/PAK exclus du port |
| `gp/` | 15 | 753 831 | captures A1 de référence uniquement |
| `images/` | 20 | 7 918 519 | captures/aperçus, non embarquées |

Les plus gros fichiers de `extracted/` : `GRP_FR.GP` 206 905 344, `a1/MOVIE/ARAN_END.MOV` 135 608 320, `a1/DATA/DATAS.BIN` 109 103 104, `a1/MOVIE/ARAN_OP.MOV` 77 821 952, `a1/ENDING.XA` 77 725 696, `PAKVCAST.PAK` 74 354 688, `a1/MOVIE/EURO_OP.MOV` 57 753 600, `a1/DATA/NULL.NUL` 33 333 333, `PACK0.PAK` 16 516 780, `a1/ARAN_XA.XA` 14 614 528, `a1/DATA/SOUND.BIN` 12 185 600.

Sous-ensemble réellement utile au port : `DATAS.BIN` (source des cartes/sprites A1), `SOUND.BIN`, `PACK0.PAK` (source A2 des WAV), `FONT3.TIM`/`WIND.TX`/`WIND.CL` (66 112 B), `a2_text/` (267 531 B, référence narrative) = **138 139 127 octets**. Les fichiers `gp/images.png` (42 041 B) et `gp/images (1).png` (42 041 B), `imgres.htm` (226 023 B) servent uniquement à la validation visuelle. Aucun de ces fichiers ne doit entrer dans le BIN/CUE.

## 2. Conversion PS1 — CD / RAM / VRAM

Hypothèses : 4 bpp = 4 bits/texel + CLUT 16 couleurs (32 B par banque) ; ADPCM SPU 4 bits/échantillon ; pas de vidéo XA/FMV dans le port ; BIN/CUE 1× 650 Mo (2× lecteur, débit soutenu 300 Ko/s).

| Donnée | Mesure source | Taille sur CD | RAM résidente | VRAM |
|---|---:|---:|---:|---:|
| Textes `data/*.txt` | 63 572 B | 63,6 ko | 39,1 ko si dialogues entiers, sinon ≤6,9 ko/zone | 0 |
| Police `FONT3.TIM` | 32 832 B | 32,8 ko | 256 B (avances) | 32 768 B + CLUT 32 B |
| Fenêtre `WIND.TX` + `WIND.CL` | 33 280 B | 33,3 ko | ~0 | 32 768 B + CLUT 512 B (16 banques) |
| Cartes (13 utilisées) | 7 812 238 B | **2 042 886 B** | cases+murs ≤41 364 B (carte 011, avant compactage) | ≤307 900 B si carte entière ; ≤160 ko en working set |
| Sprites (44 banques) | 6 019 748 B | 1 054 400 B (4bpp aligné) | index anims ~10 ko | jamais tout ; 64 352 B (héros minimal), 8 960 B HUD |
| Musiques référencées (37) | 658 072 700 B | **156,9 MiB** en ADPCM 44,1 kHz stéréo ; **39,2 MiB** en 22,05 kHz mono | 0 (streaming) | 0 |
| SFX référencés (15) | 1 243 672 B | 303,6 KiB en ADPCM 44,1 kHz stéréo ; 75,9 KiB en 22,05 kHz mono | table d’IDs ~1 ko | 0 |
| Musiques non référencées | 1 600 504 000 B (~1,49 GiB) | **à exclure du master** | 0 | 0 |

Audio total si tout était converti à la fréquence native en 4 bits : ~567 Mo (541 MiB). En 22,05 kHz mono : ~163,5 Mo (156 MiB). Le master recommandé ne contient que les 37 pistes et 15 SFX utilisés : ≈165 Mo (156,9 MiB) en qualité native ou ≈41 Mo (39,2 MiB) en 22,05 kHz mono, largement sous 650 Mo.

Débit CD de l’ADPCM : 44 100 B/s (43,1 KiB/s) en 44,1 kHz stéréo, 11 025 B/s (10,8 KiB/s) en 22,05 kHz mono — soit **15 %** ou **3,7 %** des 300 Ko/s soutenus. Le choix 22,05 kHz mono est celui qui protège le chargement par zone ; le choix 44,1 kHz stéréo reste possible mais consomme ≈165 Mo (156,9 MiB) de CD et doit être validé à l’oreille (`game-audio-custom`).

Attention tuiles : `.a1m` stocke des tuiles **déjà colorisées en RGBA**, sans CLUT. Une conversion naïve par quantification donne 672 motifs uniques et 659 « palettes » pour la carte 011 (258 048 B de pixels + 21 088 B de CLUT estimés) ; la source A1 native (`DATAS.BIN`, sections 0/2) fournit 16 palettes de 16 couleurs par zone. Il faut reconstruire l’index 4bpp + 16 CLUT depuis `DATAS.BIN`, sinon la CLUT explose et le budget VRAM est dépassé. Les 659 « palettes » sont un artefact de la mesure (ordre d’apparition des couleurs), pas une propriété du jeu.

## 3. Stratégie de chargement par zone

**Résident permanent** : code MIPS, heap/stack, table `kAdvance` (256 B), `FONT3.TIM` + CLUT en VRAM (32,8 ko), `WIND.TX` + 16 CLUT en VRAM (33,3 ko), HUD (`a1_hud.a1s`, 8 960 B 4bpp), lecteur CD, gestionnaire PAD, parseur de dialogues, état de sauvegarde, petits buffers de stream audio (2 × 16 ko en RAM/SPU).

**Chargé à l’entrée de zone** : tuiles 4bpp de la carte (≤307,9 ko pour `011`), tables cases/murs (41 364 B bruts, ≤29 ko après compactage `u16→u8` des champs inutilisés), icônes/PNJ de la zone (≤40 ko de sprites 4bpp), dialogues de la zone (≤6 931 B), table des passages locaux (2 079 B pour les 19 passages au total), plus une piste audio courte/jingle si la zone le demande. Le héros garde en VRAM le sous-ensemble repos/marche/attaque (60 frames, 62,8 ko) ; les autres groupes (course 20,3 ko, charge 63,7 ko, etc.) sont chargés à la demande. **La zone précédente est libérée explicitement avant toute allocation de la suivante** (tuiles, sprites, index de dialogues, blocs SPU) ; un test de fuite 011 → 013 → 026 → 011 doit revenir au même high-water mémoire. Le manifeste `iso.xml` est ordonné selon cet ordre de chargement réel, pas par ordre alphabétique.

**Streamé** : musique de catégorie (`village`/`plaine`/`donjon`/`boss`) et jingles via double buffer CD→SPU ; SFX préchargés par banque de 15 IDs ≤304 ko sur CD, avec working set SPU ≤256 ko.

**Ordre de chargement compatible 300 Ko/s** (pire cas zone `011`) : 1) figer l’écran + fade (0,2 s) ; 2) lire en-tête + cases/murs (~29 ko) ; 3) DMA des tuiles 4bpp (307,9 ko) ; 4) DMA des sprites de zone (~40 ko) ; 5) lire les dialogues locaux (6,9 ko) ; 6) démarrer le stream BGM, lever le fade. Charge utile ≈384 ko → **1,28 s à 300 Ko/s** ; avec latence de seek et marge, **budget 2,5 s par transition**. Le préchargement des zones voisines (19 passages) doit se faire en tâche de fond à ≤50 Ko/s pendant le jeu, jamais pendant le rendu.

## 4. Budgets mesurables et points de mesure

| Métrique | Cible | Limite dure | Vérification |
|---|---:|---:|---|
| Pic RAM principale | ≤1 500 KiB | 2 048 KiB | compteur high-water du wrapper `malloc`/`free` PSn00bSDK + `mipsel-none-elf-size -A` sur l’ELF (`.text+.data+.bss`) ; log au warp et au menu |
| Pic VRAM | ≤896 KiB | 1 048 576 B | instrumentation des uploads GPU (rects TIM/TX + framebuffers) ; somme des blocs alloués, log pic |
| Framebuffers | 307 200 B (2 × 320×240×2 octets, 15/16 bpp) | — | configurés une fois, jamais réalloués ; `DoubleBuffer` + `PutDispEnv`/`PutDrawEnv` |
| SPU RAM | ≤384 KiB | 512 KiB | suivi des adresses/tailles de voix ADPCM + reverb ; somme ≤393 216 B |
| Charge utile par zone | ≤640 KiB | 1 MiB | instrumentation `CdRead` (octets + ticks) ; mesure sur les 13 cartes référencées |
| Temps de transition | ≤2,5 s | 4 s | horodatage PSn00bSDK autour du chargement de zone, log émulateur |
| Débit CD soutenu | ≤60 KiB/s (hors transition) | 300 Ko/s (307,2 KiB/s) | compteur d’octets lus / temps, comparé au débit 2× |
| Temps d’image (présentation 60 Hz NTSC) | 16,67 ms par image ; p50/p95 ≤16,67 ms, pic ≤33,3 ms (1 VBlank) sur 1 000 images | 33,3 ms hors transition ; 50 ms en transition | histogramme obtenu via timer/root counter R3000 + `VSync` ; corrélation avec `VPS` du log DuckStation |
| Présentation | 60 Hz NTSC, `VPS ≥ 50` | — | smoke test jalon 1 mesuré : OpenBIOS, `VPS: 59,94` ; en headless `FPS: 0` est normal (pas de présentation d'image) |

**Choix 60 Hz de présentation, 60 Hz de logique** : le smoke test jalon 1 mesure 59,83 images/s en NTSC et le budget PS1 fixe le temps d’image à 16,67 ms. Les ticks restent à 1/60 s (les animations de `src/sprite.h` et les scénarios `--auto` en dépendent) et l’entrée est échantillonnée à 60 Hz. Si les mesures réelles montrent que p95 dépasse 16,67 ms, un repli à **30 Hz de rendu** (33,3 ms par image, logique et entrée toujours à 60 Hz) est envisageable uniquement après validation de l’écart par l’Architecte — jamais par réduction de résolution ou changement de couleur. Ce rapport ne retient pas le 30 Hz par défaut : il n’existe aucune mesure MIPS justifiant de l’adopter.

Points de mesure concrets : `mipsel-none-elf-size -A <elf>` (outil dans `external/tools/psn00bsdk/PSn00bSDK-0.24-win32/bin`) pour `.text`/`.data`/`.bss`, wrappers d’allocation et de `CdRead` avec high-water et fragmentation, comptage des uploads GPU/SPU, histogramme p50/p95/pic sur **1 000 images** via root counter + `VSync`, et un script d’analyse des en-têtes `.a1m`/`.a1s` (formats documentés dans `tools/export_assets.py` et `src/sprite.cpp`) vérifiant les seuils ci-dessus. Chaque rapport de budget devra nommer la commande, l’instant de mesure et la valeur observée, puis l’écart au budget ; une estimation ne sera jamais présentée comme une mesure.

Piège de mesure DuckStation : en `-nogui`, il n’y a pas de présentation d’image, donc `FPS: 0` est **normal** ; c’est `V/PerfMon: … VPS: ~60` qui valide l’émulation. Ne pas conclure à un plantage sur `FPS: 0`. DuckStation réécrivant son log à chaque lancement, lire le fichier entier et prendre la **dernière** occurrence de chaque marqueur. Le smoke reproductible est `powershell -ExecutionPolicy Bypass -File tools\ps1_smoke.ps1 -Seconds 15` avec `PS1_SMOKE: PASS` et `VPS ≥ 50` (outil possédé par `qa`, non modifié ici). Côté PC, `py -3.12 tools\validate_data.py` doit rester à `dialogues 149 | sprites 44 | zones 10 | passages 19`, `ERR: 0 | WARN: 0` après tout changement de données, et `ctest --test-dir build -N` doit rester vert. Ces contrôles PS1 sont à créer dans le jalon 2 ; ce rapport ne modifie aucun outil.

### Pics PS1 réels — à renseigner par instrumentation (aucune mesure MIPS à ce jour)

| Métrique | Commande / moment | Valeur observée | Écart au budget |
|---|---|---|---|
| RAM pic (zone 011) | wrapper `malloc` + `size -A`, au fade | non mesuré | — |
| RAM pic (chaîne 011→013→026→011) | high-water après libération explicite | non mesuré | — |
| VRAM pic | table d’allocation GPU, pendant le rendu | non mesuré | — |
| SPU pic | octets transférés + occupation max | non mesuré | — |
| Temps d’image p50/p95/pic | root counter + `VSync`, 1 000 images | non mesuré | — |
| `VPS` DuckStation | `tools/ps1_smoke.ps1 -Seconds 15`, dernière occurrence de `V/PerfMon` | 59,83 mesuré lors du smoke du modèle SDK (le log peut varier autour de 59,9) | conforme (`≥50`) |

Le `59,83` ci-dessus vient du smoke du **modèle SDK**, pas du jeu porté ; il ne valide aucun budget de rendu du remake.

## 5. Gros consommateurs et risques de dépassement

1. **Audio PCM (2 272 262 120 B, 89,4 % de `assets/`)** : risque n°1. Convertir les 241 fichiers en ADPCM natif donne ~567 Mo sur CD et exige un stream permanent ; inclure les pistes `music_a2` non référencées gaspille 1 484 549 396 B (~1,38 GiB) de source. Mitigation retenue : 37 pistes + 15 SFX, 22,05 kHz mono par défaut (39,2 Mo + 76 KiB), avec option 44,1 kHz stéréo à valider à l’oreille.
2. **Cartes A1 (264 205 186 B, 483 fichiers)** : conversion naïve RGBA→VRAM = 1263,9 KiB pour la seule carte 011 ; avec framebuffers (300 KiB), police+fenêtre (65,6 KiB), héros complet (212,1 KiB) et HUD (8,8 KiB), on atteint ~893 KiB avant les ennemis. Mitigation : 13 cartes seulement (7,45 MiB source → 1,95 MiB CD), working set tuiles ≤160 KiB, cases/murs compactés, héros minimal 62,8 KiB.
3. **Palettes des tuiles** : `.a1m` pré-colorisé n’a pas de CLUT ; la mesure heuristique donne jusqu’à 659 palettes pour une carte, contre 16 palettes natives dans `DATAS.BIN`. Sans récupération des palettes natives, la CLUT et le code de conversion explosent.
4. **Sprites (6 019 748 B → 1 054 400 B 4bpp)** : les 44 banques ne tiennent pas seules dans 1 MiB de VRAM. Résidence obligatoire du seul sous-ensemble zone + héros minimal ; charger les autres banques à la demande.
5. **Police (294 156 B de source, 75 136 B de constantes PC)** : embarquer le header C++ ou les 65 536 index décompressés gaspille 287+64 KiB sur CD/RAM. Envoyer les TIM en VRAM, ne garder que 256 B d’avances.
6. **CD et transitions** : 19 passages, carte max 1 294 228 B source ; lire tout le jeu au démarrage est impossible (2,37 GiB de `assets/`). Le préchargement voisin doit rester ≤50 Ko/s pour ne pas perturber le stream audio.
7. **Comptage des dialogues** : 149 blocs / 339 messages dans le remake contre 262 blocs / 553 lignes dans `extracted/a2_text/dialogues.txt`. Ne pas dimensionner le chargeur ni la RAM sur 553 : la valeur utile est 6 931 B pour la zone la plus bavarde.

**Risque principal de dépassement** : la VRAM, via la somme « carte entière + héros complet + double framebuffer » (~893 KiB avant ennemis/effets), aggravée par l’absence de CLUT dans les `.a1m` pré-colorisés. Le second risque est le SPU/CD audio si les 241 WAV sont masterisés au lieu des 37 pistes référencées.

---
Mesures reproductibles sans écrire dans le dépôt : taille des fichiers via `Get-ChildItem … | Measure-Object -Property Length`, en-têtes WAV via `python -c "import wave …"`, en-têtes `.a1m` (`<HHH` à l’offset 4) et `.a1s` (`A1SP`, `u16` frames puis `HHhh` par frame) via `struct`, tableaux `a1_font.h` via regex sur `std::array`. Aucune autre ressource n’a été lue en écriture, aucun commit, aucun push.
