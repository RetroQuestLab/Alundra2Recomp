# Audit de portabilité — PlayStation 1 native

Date : 2026-11-21. Périmètre : audit **statique et lecture seule** de la base PC actuelle. Aucun fichier source, donnée ou build n’a été modifié ; seul ce rapport est créé. Aucune commande de build, de validation ou de CTest n’a été lancée pendant cet audit.

Cible visée : **PS-X EXE MIPS natif** + **BIN/CUE bootable**, GPU/SPU/PAD/carte mémoire réels via **PSn00bSDK v0.24**, PC SDL3 inchangé comme référence de non-régression. Budgets : **2 Mo RAM**, **1 Mo VRAM**, **512 Ko audio**, chargement par zone. Chaîne jalon 1 déjà vérifiée : template SDK → ELF/PS-X/BIN/CUE, boot DuckStation + OpenBIOS à 59,83 FPS.

## Résumé demandé (12 lignes)

1. L’architecture actuelle est **monolithique et SDL3-native** : `game.h` expose `SDL_Window`, `SDL_Renderer`, `SDL_Gamepad`, `SDL_AudioStream`, `SDL_Color` et `std::filesystem` jusqu’aux structures de gameplay.
2. Le rendu est intégralement SDL3 (textures RGBA8888, blending, tiling, rotation, `SDL_RenderDebugText`, capture BMP) ; il faut le remplacer par un renderer PS1 à primitives GPU/ordering-tables/TIM/CLUT.
3. La police A1 et les fenêtres WIND sont déjà générées en RGBA via SDL (`dialogue.cpp:59-115`) : la conversion en 4 bpp + CLUT VRAM est faisable, mais doit être centralisée.
4. Les entrées sont SDL (`SDL_Event`, scancodes, gamepad SDL) : un service PAD doit reproduire exactement les mapping clavier/manette et les tests `--auto`.
5. La boucle utilise `SDL_GetTicks` avec pas fixe 1/60 s (`main.cpp:1232-1243`) : sur PS1, il faut VSync/DrawSync et un compteur déterministe, avec attention NTSC/PAL.
6. L’audio est un **mixeur logiciel 44,1 kHz stéréo** dans un callback SDL (`audio.cpp:14-63`) : c’est le point le plus coûteux à porter sur un R3000A sans FPU.
7. Les banques WAV actuelles pèsent ~2,54 Go au total (music_a2 1,42 Go, music 662 Mo, sfx 13 Mo) : impossible dans 512 Ko audio ; VAG/ADPCM, streaming CD ou séquencement sont obligatoires.
8. Le chargement de données repose sur `std::filesystem`, `directory_iterator`, `ifstream` et des fichiers texte modifiables (`world.cpp`, `items.cpp`, `audio.cpp:176-194`) : la cible PS1 doit utiliser un manifeste CD figé et des tables compilées.
9. Les sauvegardes texte avec `.tmp`, `.bak`, `rename` et `SDL_GetPrefPath` (`items.cpp:628-695`, `title.cpp:364-375`) doivent devenir des blocs carte mémoire avec checksum, version et deux exemplaires.
10. La STL est absente de la cible MIPS (sonde réelle) : `game.h:5-13,296-397` doit être scindé ou compilé derrière un macro de compat ; tableaux fixes/arènes, vues de texte, IDs internés et pools sont obligatoires.
11. Les flottants sont partout (physics, caméra, animation, mixage audio, `std::hypot/sin/cos/fmod`) : sur PS1 sans FPU, migrer vers du fixed-point 16.16 ou des tables entières.
12. Recommandation : introduire `Platform/Renderer/Input/Audio/Storage/Time` avec implémentation SDL3 pour PC et PSn00bSDK pour PS1, garder la logique A2 et les tests `--auto` comme référence commune.

## Matrice de portabilité

| SYSTÈME | IMPLÉMENTATION ACTUELLE (fichier:fonction) | ADAPTATION PS1 NÉCESSAIRE | RISQUES | VALIDATION POSSIBLE |
|---|---|---|---|---|
| Build / cible | `CMakeLists.txt:4-37` : C++20, `find_package(SDL3)`, `add_executable(WIN32)`, `-static`, copie `SDL3.dll`. `build.ps1:4-29` : clang++ MinGW + Ninja. | Ajouter un preset PS1 séparé utilisant `cmake/sdk.cmake`, `psn00bsdk_add_executable(... GPREL ...)`, `psn00bsdk_add_cd_image` (template SDK : `CMakeLists.txt:14-20`, `CMakePresets.json:14-19`). Mutualiser la liste de sources sans modifier le preset PC. | Deux chaînes divergeantes ; erreurs de toolchain ; régression des 81 CTest. | Build PC inchangé ; build PS1 produit `*.psx` + `*.bin`/`*.cue` ; smoke DuckStation/OpenBIOS ; `ctest --test-dir build`. |
| Couplage SDL global | `src/game.h:3,81,207,214,272,286-289,296,310-312` : types SDL et `std::filesystem` dans l’état de jeu ; `src/main.cpp:78` `new Game`. | Extraire des types plateforme neutres ; `Game` ne doit contenir que POD/fixed-point et IDs. Remplacer `new Game` par une instance statique/arène. | Toute la logique dépend de SDL ; compilation MIPS impossible ou classicalisations incontrôlées. | Compilation du cœur de jeu sans en-tête SDL ; grep de contrôle `#include <SDL3` hors couche PC. |
| Fenêtre / présentation | `src/main.cpp:14-16,112-118` : callbacks SDL3, fenêtre 960x720 redimensionnable, VSync SDL, `SDL_SetRenderLogicalPresentation(INTEGER_SCALE)`. | Remplacer par mode vidéo GPU 320x240, double buffer, `PutDispEnv`/`DrawOTagEnv`, `VSync(0)`/`DrawSync(0)` (API PSn00bSDK confirmée). | Résolution/framerate différents； déchirure； logique de capture PC non transposable. | Capture native 320x240 + zoom NEAREST ; comparaison même texte/même échelle avec références A1 ; log DuckStation FPS. |
| Rendu textures / atlas | `src/map.cpp:30-63,70-98` : atlas SDL `RGBA32`, une texture par tuile dessinée ; `src/sprite.cpp:28-35,61-79` : RGBA8888 par frame. | Conversion offline en TIM 4 bpp/8 bpp + CLUT, upload VRAM, sprites assemblés en textures/page, order table par profondeur ; suppression de l’allocation par frame. | VRAM > 1 Mo ; pages/CLUT mal gérées ; ordre de dessin différent ; coût CPU de conversion. | Vue VRAM et compteur d’uploads ; capture zone 011 ; comparaison A1 au pixel (nearest) ; mesure pics VRAM. |
| UI / police / fenêtres | `src/dialogue.cpp:59-115` génère 16 textures RGBA (police + 16 banques WIND) ; `src/dialogue.cpp:123-139,193-235,376-383` dessine via SDL. | Convertir FONT3.TIM en une texture 4 bpp + CLUT ; WIND.TX/CL en TIM+CLUT ; dessiner glyphes et panneaux par quads GPU ; gérer le mot CLUT 0 transparent. | Police difference ; perte des accents ; corruption CLUT； 16 banques simultanées hors budget. | Scènes `--diag 1..9` ; comparaison glyphes/accents/panneaux avec références； tests `validate_a1_textures.py` et `validate_dialogue_layout`. |
| Primitives avancées | `src/dialogue.cpp:206-235` `RenderTextureTiled` ; `src/menu.cpp:160-167` blending alpha ; `src/sprite.cpp:72-79` et `src/title.cpp:350` rotation ; `src/fx.cpp:11-40` texture d’ombre générée ; `src/main.cpp:1164-1166` `SDL_RenderDebugText`. | Remplacer tiling par quads répétés, alpha par semi-transparence GPU, rotation par frames pré-bakées ou GTE uniquement si prouvé, ombre par sprite 4 bpp, debug par écran diagnostique compilé à part. | Rotation logicielle coûteuse ; blending incorrect ; dépendance SDL cachée dans le debug. | Captures diag ; profil temps d’image ; contrôle visuel des ombres/parchemins/curseurs. |
| Entrées | `src/main.cpp:217-297` événements SDL ; `src/main.cpp:300-327` scancodes, axes et boutons SDL_Gamepad. `title.cpp:377-381` file d’événements SDL pour quitter. | Service `IInput` : polling PAD via `psxpad.h`/BIOS, mapping D-Pad/Croix/R1/L1/Start identique, fronts montants conservés dans `Input` (`game.h:34-41`). `quit()` devient un signal plateforme. | Mapping différent sur manette PS1 ; perte des fronts ; `--auto` non reproductible. | Test de mapping manette physique/émulée ; scripts `--auto` PC et scénario PS1 zone 011 avec injection PAD déterministe. |
| Temps / boucle | `src/main.cpp:1232-1243` : `SDL_GetTicks`, accumulator `float`, pas fixe `TICK`, grain `SDL_srand` en test (`:108-110`), secousse `SDL_rand` (`:1259`). | `ITime` : VSync + compteur/root timer ; accumulator en entier (microsecondes ou ticks 60 Hz) ; PRNG maison à graine fixe ; forcer NTSC ou gérer le 50 Hz PAL explicitement. | Dérive temporelle ; tests `--auto` non déterministes ; comportement PAL divergent. | Log VPS/FPS ; replay `--auto` identique PC/PS1 ; mesure nombre de ticks par image sur 1000 frames. |
| Audio sortie / mixage | `src/audio.cpp:14-63` callback `feed` : mixage logiciel, interpolation linéaire, 16 voix, 44,1 kHz stéréo ; `src/audio.cpp:66-87` `SDL_OpenAudioDeviceStream`. | Service `IAudio` SPU : 24 canaux matériels, VAG/ADPCM, `SpuSetTransferStartAddr`/`SpuWrite` avec alignement 64 octets, pitch/volume/pan matériels (API confirmée dans PSn00bSDK). Supprimer le callback SDL et le `std::vector` alloué par callback. | CPU insuffisant ; latence ; clips ; absence de mixage logiciel équivalent. | Screenshot/log des voix SPU ; écoute émulateur ; mesure DMA et pics audio < 512 Ko. |
| Banques audio / assets | `src/audio.cpp:79-119,234-267` : 1000 WAV music + 200 WAV SFX via `SDL_LoadWAV`, `.loop` texte ; répertoires `music` 662 Mo, `music_a2` 1,42 Go, `sfx` 13 Mo, `mono_a2` 76 Mo. | Pré-conversion offline en VAG/ADPCM, table d’échantillons < 512 Ko résidente ou streaming CD par zone ; boucles et jingles stockés comme métadonnées compilées, pas en `.loop` texte. | Budget largement dépassé ; chargement trop lent ; qualité/loop altérés. | Script de conversion + rapport de tailles ; vérification audio par morceau ; pic SPU RAM mesuré. |
| Données / fichiers | `src/main.cpp:29-38,120-138` `SDL_GetBasePath` + `directory_iterator` ; `src/world.cpp:139-198`, `src/items.cpp:46`, `src/menu.cpp:137-155`, `src/audio.cpp:132-142,255-266` analysent des textes à l’exécution. | Manifeste ISO figé (`iso.xml`), accès ISO9660 via `psxcd.h`, table de fichiers compilée, IDs internés ; parser au chargement de zone uniquement ou pré-convertir en tables binaires. | `std::filesystem` indisponible/coûteux ; chemins absents ; parse CPU et allocations ; données modifiables non conformes CD. | Liste des fichiers du BIN/CUE ; `--auto` sur les mêmes données ; validateur `tools/validate_data.py` ERR 0/WARN 0. |
| Sauvegarde / options | `src/items.cpp:628-695` texte V4 avec `.tmp`, `.bak`, `rename`, `SDL_Delay`, `hasSave()` `ifstream` ; `src/title.cpp:364-375,418-428` `SDL_GetPrefPath` + `options.txt` ; `src/audio.cpp:176-194` réécrit `data/world.txt`. | `IStorage` : blocs carte mémoire via `_card_*` (`psxapi.h:280-293`) / `psxpad.h`, checksum CRC, deux blocs redondants, version V4 conservée, migration V2/V3 ; options dans un bloc dédié ; `saveZoneMusic` devient réglage en mémoire/carte, jamais écriture CD. | Pas de `rename` ni de FS writable ; corruption de bloc ; perte de sauvegarde ; F6 casse le CD. | `--savetest` en mémoire ; fixtures V2/V3/V4/V5 ; test carte mémoire émulateur ; relecture après power-off. |
| STL / allocation | `src/game.h:5-13,150-176,296-397` : `map`, `set`, `vector`, `string`, `unique_ptr`, `optional`, `function` partout ; `src/main.cpp:78` ; `src/audio.cpp:20,63,107,182-191,246-253`. Sonde MIPS : aucun en-tête ni `libstdc++` disponible. | Cœur commun **sans STL** : tableaux fixes/arènes, vues de texte, IDs internés, pools par zone, callback `Map::draw` sans `std::function` ; scinder `src/game.h` ou macro de compat. | Échec de compilation MIPS avéré (P13) ; fragmentation et pic > 2 Mo ; non-déterminisme. | Compilation cœur sans STL avec `mipsel-none-elf-g++` ; grep `std::` hors couche PC ; instrumentation heap ; pic RAM < 2 Mo. |
| Nombres / maths | `src/game.h:21,49-95,205-236,271-276,387` ; `src/entities.cpp:34,42,104-105,168,258,290` ; `src/audio.cpp:38-60,283-284` ; `src/title.cpp:52-96,127-133,205-208,282-350`. | Fixed-point 16.16 pour positions/vitesses/caméra/volume/pan ; `sqrt/hypot/sin/cos/fmod` par tables ou approximations entières ; remplacer `float`/`double` publics par un alias plateforme. | R3000A sans FPU → soft-float très lent ; différences de arrondi ; tests `--expect` sensibles. | Benchmark ticks/image ; replay identique PC/PS1 ; tests de bornes de combat/route A→Z. |
| Formats binaires / alignement | `src/map.cpp:8-10,25-26,44-53` lit des POD bruts dans `Cell`/`uint16_t` ; `src/sprite.cpp:19-29` lit des `uint16_t`/`int16_t` ; exporteurs Python écrivent en little-endian (`tools/export_assets.py:59-67`, `tools/a1_sprites.py:198-241`). | Little-endian MIPS compatible avec les fichiers actuels, mais remplacer les lectures par un sérialiseur explicite (largeurs fixes, vérification de bornes) ; structurer les transfers DMA/GPU/SPU alignés (64 octets pour SPU). | `Cell` dépend du layout C++ ; accès non alignés MIPS ; lecture hors limites ; CLUT 16 bits mal alignée. | Fuzzing des loaders ; test sur vrai BIN/CUE ; `validate_a1_textures.py` ; logs DMA/VRAM. |
| Mémoire / chargement par zone | `Game` contient maps, sprites, dialogues, musique, inventaire, forge (`src/game.h:286-397`) ; `loadZone` remplace la carte et relance musique (`src/world.cpp:62-129`) ; `assets/` totalise ~2,54 Go. | Découper par zone : carte + sprites + UI + une piste audio courte ; libérer explicitement la zone précédente ; budgets durs 2 Mo RAM/1 Mo VRAM/512 Ko audio avec instrumentation. | Pic hors budget ; CD streaming trop lent ; fuite entre zones ; `saveGame` à chaque `loadZone` (`world.cpp:125`) sollicite la carte mémoire. | Mesures réelles hors émulateur et dans DuckStation ; rapport pic par zone ; scénario 011 → 013 → 026. |
| CLI / tests déterministes | `src/main.cpp:84-211` parse `--auto/--expect/--savetest/--shot/--diag` ; `CMakeLists.txt:55-246` 81 CTest ; gates et IDs A2 inchangés. | Conserver le harnais PC intact ; sur PS1, compiler les mêmes hooks derrière une interface de test (injection PAD + storage factice), pas de CLI shell ; ne jamais modifier les IDs de dialogue ni les gates. | Régression gameplay ; tests PC verts mais build PS1 divergent ; script `--auto` non reproductible. | `ctest` complet vert ; smoke PS1 zone 011 avec même script ; vérification `mephisto_vaincu`, `epee_supreme`, `lampe`, `prisme`, `boss_medusa`, `cristal`. |
| Diagnostics / outils PC | `SDL_ShowSimpleMessageBox` (`main.cpp:126`), `SDL_Log` massifs, `SDL_RenderDebugText` (`:1164-1166`), `SDL_RenderReadPixels` + `SDL_SaveBMP` (`:1357-1360`), `SDL_Delay` (`items.cpp:686`). | Remplacer par logger plateforme, écran de diagnostic compilé à part, capture VRAM/framebuffer via outil hôte, attente VSync au lieu de sleep. | Dépendances SDL résiduelles ; capture PC non disponible sur console. | Grep SDL dans la cible PS1 ; capture émulateur convertie en PNG natif/zoom ; logs OpenBIOS/DuckStation. |
| Région / matériel | Template vérifié en NTSC-U/C à 59,83 FPS ; boucle actuelle suppose 60 Hz. | Figer NTSC pour jalon 2, puis ajouter MIDI/50 Hz ou sélection region dans `ITime` et test physique (modèle, lancement homebrew, carte, manette à fournir localement). | PAL plus lent ; test physique indisponible ; faux positif en émulateur. | Log région DuckStation/PCSX-Redux ; test console réel une fois matériel disponible. |

## Points faibles concrets et classification

Légende : **B** = `bloquant jalon 2` ; **D** = `différable` après jalon 2. Complexité : Faible / Moyenne / Élevée / Très élevée.

| ID | Point faible localisé | Criticité | Complexité | Correction minimale |
|---|---|---|---|---|
| P01 | `src/game.h:3,286-289` : SDL et `std::filesystem` dans `Game` ; `CMakeLists.txt:8-29` impose SDL3. | B | Élevée | Créer les interfaces plateforme et une couche PC qui wrap SDL3 ; cœur de jeu sans SDL. |
| P02 | `src/main.cpp:112-118` : fenêtre/VSync/logical presentation SDL. | B | Moyenne | Renderer PS1 320x240 double buffer + `VSync/DrawSync`. |
| P03 | `src/map.cpp:30-63,70-98`, `src/sprite.cpp:28-35,61-69` : textures RGBA et draw par sprite. | B | Très élevée | Pipeline offline A1MP/A1SP → TIM/CLUT, ordering tables, budget VRAM. |
| P04 | `src/dialogue.cpp:193-235`, `src/menu.cpp:160-167`, `src/fx.cpp:11-40` : tiling/alpha/textures générées. | B | Élevée | Quads GPU, semi-transparence native, ombres pré-bakées. |
| P05 | `src/dialogue.cpp:59-115,376-383` : 17 textures statiques police/WIND via SDL. | B | Moyenne | Une police 4 bpp + CLUT, WIND.TX/CLUT en VRAM, mot 0 transparent. |
| P06 | `src/sprite.cpp:72-79`, `src/title.cpp:350` : rotation logicielle SDL. | B | Moyenne | Frames pré-calculées pour l’introduction ; supprimer l’angle flottant. |
| P07 | `src/main.cpp:217-327`, `title.cpp:377-381` : entrées SDL events/gamepad. | B | Moyenne | `IInput` PAD, mapping identique, fronts montants conservés. |
| P08 | `src/main.cpp:1232-1243,1259` : `SDL_GetTicks`, accumulator float, `SDL_rand`. | B | Moyenne | `ITime` VSync + ticks entiers + PRNG à graine fixe. |
| P09 | `src/audio.cpp:14-63` : mixeur logiciel dans callback SDL. | B | Très élevée | Voix SPU, VAG/ADPCM, pas de mixage CPU. |
| P10 | `src/audio.cpp:79-119,234-267` : WAV 44,1 kHz et 300 banques. | B | Élevée | Conversion VAG, manifeste CD, <= 512 Ko audio résident. |
| P11 | `src/main.cpp:29-38,120-138`, `world.cpp:139-198` : filesystem + textes runtime. | B | Élevée | Manifeste ISO9660 figé, tables compilées, chargement par zone. |
| P12 | `src/items.cpp:628-695`, `title.cpp:364-375`, `audio.cpp:176-194` : sauvegardes FS et réécriture de `world.txt`. | B | Élevée | `IStorage` carte mémoire, CRC, deux blocs, options dédiées. |
| P13 | `src/game.h:5-13,296-397`, `main.cpp:78` : STL et `new Game` ; sonde MIPS sans STL confirmée. | B | Très élevée | **Contrainte dure** : cœur sans STL, `src/game.h` scindé ou macro de compat, arènes/vues de texte/IDs internés/pools, instance statique. |
| P14 | `src/game.h:21,49-95`, `entities.cpp:34,258`, `audio.cpp:38-60` : flottants et maths. | B | Très élevée | Fixed-point 16.16 et tables/approximations entières. |
| P15 | `src/map.cpp:8-10,44-53`, `sprite.cpp:19-29` : sérialisation POD brute. | B | Moyenne | Lecteur explicite little-endian avec bornes et padding contrôlé. |
| P16 | `CMakeLists.txt:4-37`, `build.ps1:4-29`, `main.cpp:84-211` : cible PC-only et harnais CLI. | B | Moyenne | Preset PS1 séparé + hooks de test plateforme neutres. |
| P17 | `main.cpp:126,1164-1166,1357-1360`, `items.cpp:669-691` : SDL_Log/MessageBox/DebugText/SaveBMP/Delay. | D | Faible | Logger et capture plateforme ; grep final SDL dans la cible PS1. |
| P18 | NTSC vérifié seulement ; boucle 60 Hz supposée. | D | Moyenne | Figer NTSC au jalon 2, puis gérer PAL/50 Hz et test physique. |

**Total : 16 points `bloquant jalon 2`, 2 points `différable`.**

## Contrainte confirmée par sonde réelle : pas de STL sur la cible MIPS

Sonde exécutée le 2026-11-21 : P13 n’est plus une optimisation, mais une contrainte dure de compilation.

1. `mipsel-none-elf-g++.exe -std=c++20 -c probe.cpp` avec `#include <vector>` échoue : `fatal error: vector: No such file or directory`.
2. Les en-têtes C++ du SDK existent bien sous `external/tools/psn00bsdk/PSn00bSDK-0.24-win32/mipsel-none-elf/include/c++/12.3.0/`, mais ne contiennent **pas** `vector`, `string`, `map`, `memory`, `optional`, `functional` ni `algorithm` (vérification fichier par fichier). Le sous-ensemble présent est freestanding : `cstdint`, `cstdio`, `new`, `type_traits`, etc.
3. Aucun `libstdc++*.a` n’existe dans `mipsel-none-elf/lib` ; seules les bibliothèques `libc_*`, `libpsxapi_*`, `libpsxcd_*`, `libpsxgpu_*`, `libpsxgte_*`, `libpsxpress_*`, `libpsxsio_*`, `libpsxspu_*`, `libpsxetc_*` et `liblzp_*` sont fournies.
4. Les wrappers C++ minimaux du SDK sont dans `include/libpsn00b/` : `cstdint`, `cstdio`, `cstdlib`, `cstring`, `cassert`, `cctype`, `ctype.h` ; aucun conteneur n’est proposé.

Conséquence : le code partagé ne peut **pas** utiliser `std::vector`, `std::string`, `std::map`, `std::set`, `std::unique_ptr`, `std::optional` ni `std::function` sur la cible PS1. Deux voies : (a) sous-ensemble C++ sans STL pour le cœur commun (tableaux fixes/arènes, vues de texte, IDs internés, pools) — voie recommandée ; (b) compat local écrit pour le projet, mais **aucune dépendance externe sans validation de l’Architecte**. `src/game.h` devra être scindé (types de données communs sans STL + parties PC) ou compilé derrière un macro de compatibilité. Le total reste **16 points bloquants**, P13 inclus désormais comme contrainte dure.

## Architecture recommandée

```text
src/core/        Game, Hero, Enemy, Item, Dialogue, Forge  (POD/fixed-point, sans SDL ni STL)
src/platform/    IPlatform, IRenderer, IInput, IAudio, IStorage, ITime, ILog
src/platform/pc/ SDL3Renderer, SDL3Input, SDL3Audio, SDL3Storage, SDLTime  (wrapper)
src/platform/psx/PsxRenderer, PsxPadInput, PsxSpuAudio, PsxCdStorage,
                PsxMemCardStorage, PsxTime  (PSn00bSDK)
src/game/        update.cpp, world.cpp, items.cpp, dialogue.cpp, menu.cpp (communs)
cmake/ps1/       sdk preset + iso.xml + rules de conversion A1MP/A1SP/TIM/VAG
```

Règles d’interface :

- Le cœur commun est **sans STL** : `FixedVec<T,N>`, `TextSpan`, `InternId` et pools côté PS1 ; les conteneurs STL éventuels restent confinés à la couche PC derrière les interfaces. `src/game.h` est scindé ou protégé par un macro de compatibilité ; aucune dépendance externe n’est ajoutée sans validation Architecte.
- `IRenderer` expose uniquement `drawSprite(id, x, y, ...)`, `drawText(latin1, ...)`, `drawPanel(...)`, `present()` ; les textures, CLUT et order tables restent privées à l’implémentation PS1.
- `IInput` retourne `Input` (`game.h:34-41`) avec fronts déjà calculés ; PC lit SDL, PS1 lit PAD. Le script `--auto` injecte le même `Input`.
- `ITime` fournit `tick()` et le temps entier ; PC utilise SDL_GetTicks en référence, PS1 utilise VSync/root counters. Le mode test force un tick par image des deux côtés.
- `IAudio` reçoit des IDs d’événements (`playSfx`, `playMusic`, `playJingle`) ; le mixage 44,1 kHz disparaît du code commun.
- `IStorage` expose `hasSave/load/save/options` avec version V4 ; PC conserve `.tmp/.bak` pour CTest, PS1 écrit des blocs carte mémoire CRCés. Le CD est read-only.
- Les IDs de dialogue, la route A→Z, les gates `epee_supreme`/`lampe`/`prisme`/`boss_medusa`/`cristal`, Méphisto, le boss du Mont Épervier et `--auto`/`--expect`/`--savetest` restent des invariants non négociables.

## Ordre de portage suggéré

1. Créer les interfaces, bannir la STL du cœur commun (`src/game.h` scindé/macro) et faire passer le PC SDL3 derrière elles **sans changer un seul test** ; c’est le filet de non-régression.
2. Remplacer les flottants du cœur par un type fixed-point et figer le PRNG ; valider avec les 81 CTest.
3. Implémenter le preset PS1 minimal : boot, VSync, une carte 011 en TIM, PAD, affichage HUD ; mesurer RAM/VRAM/temps d’image.
4. Brancher dialogues A1/textes A2, menu, forge, sauvegarde carte mémoire et audio SPU par tranches, avec captures comparées au même texte et à la même échelle.
5. Mastering BIN/CUE, test émulateur approfondi, puis test console physique avec le matériel à fournir localement.

## Validation globale

- PC : `powershell -ExecutionPolicy Bypass -File build.ps1`, `py -3.12 tools\validate_data.py`, `ctest --test-dir build --output-on-failure`, `git diff --check`.
- PS1 : build preset dédié → PS-X EXE + BIN/CUE ; boot avec OpenBIOS ; log `FPS/VPS` ; scénario `--auto` équivalent sur la zone 011 ; instrumentation RAM/VRAM/audio ; captures natives 320x240 + zoom NEAREST comparées avec les références A1.
- Critère visuel : inspection réelle côte à côte et superposition, même texte/même échelle ; aucune validation par statistiques de couleurs seules.
