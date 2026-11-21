# Design doc — première version jouable sur PS1 (tranche 1)

Date : 2026-11-21.
Option retenue : **prototype jouable rapide** (approche 1), pas la tranche fidèle complète.

## Objectif

Fournir une ISO NTSC/60 Hz gravable contenant un vrai début de jeu : décor de la zone 011
(Village Paco), héros contrôlable au Pad, caméra, saut, attaque et HUD de vie. La cible PC SDL3
reste inchangée.

## Périmètre

**Inclus**
- Décor A1 de `assets/maps/011.a1m` : vraies tuiles 24×16, murs empilés, hauteurs de cases.
- Héros A1 issu de `assets/sprites/hero.a1s` : repos, marche, course, saut, attaque (groupes 0,1,3,2,16).
- Déplacement 8 directions**, course (R1), saut (Croix), attaque (Carré), collisions identiques à `canStand()`.
- Caméra integer suivant le héros, bornée à la carte.
- HUD : cœurs en haut à gauche reflétant les points de vie du héros ; texte minimal « ZONE 011 ».
- Sortie ISO 2048 + BIN/CUE, `MODE_NTSC` forcé, boot CD vérifié en émulateur.

**Exclus de cette tranche** (prochaines tranches du plan)
- Dialogues/police A1/fenêtres WIND, PNJ, coffres, objets, musique, sauvegarde, changement de zone.

## Contraintes

- C++20 MIPS **sans STL, sans allocation dynamique**, gcc PSn00bSDK 0.24.
- VRAM 1 Mo : 2 framebuffers 320×240×2 = 307 200 o, tuiles 4bpp ≈ 157 632 o,
  héros 4bpp ≈ 64 352 o, CLUT ≈ 26 ko → ~555 ko, marge confortable.
- RAM 2 Mo : données statiques embarquées dans l'EXE via `.incbin` (pas de lecteur CD à écrire).
- OT par ligne de balayage (64 entrées), primitives `SPRT_16` pour tuiles/sprites, `TILE` pour le HUD.
- Texel 4bpp d'indice 0 = transparent (contrainte GPU PS1), donc alpha A1 → index 0.

## Pipeline de données (offline)

Nouvel outil `tools/ps1_export_zone.py` (non publié) :
1. Lit `A1MP` ( Built-in ) : 821 tuiles RGBA 24×16, 3 120 cases, 8 202 murs.
2. Quantifie chaque tuile sur **16 couleurs max** (alpha=0 → index 0), déduplique les palettes
   en CLUT 16×BGR555.
3. Lit `A1SP` (`hero.a1s`) : ne garde que les groupes [0 repos, 1 marche, 3 course, 6 saut, 2 épée]
   (`src/sprite.h` : `IDLE, WALK, RUN, JUMP, ATTACK`), quantifie chaque frame en 4bpp + CLUT dédupliquée.
   L'ancien choix [0, 1, 3, 2, 16] prenait le groupe ATTACK comme saut et le groupe 16 (vide) comme épée.
   Les directions A1SP sont `0=haut, 1=gauche, 2=droite, 3=bas` (voir `src/platform/pf.h`).
4. Écrit `ps1/assets/zone011.dat` et `ps1/assets/hero.dat` (formats P1ZM / P1SP détaillés dans le plan).

## Exécution (PS1)

- `src/ps1/pf_ps1.cpp` étendu : `pfUpload4bpp()`, `pfTexPage()`, `pfDrawSprite16()`, OT 64.
- `src/ps1/zone011.cpp` : chargeur P1ZM + dessin des tuiles/murs avec `camX/camY` entiers.
- `src/ps1/hero_ps1.cpp` : animation (ticks 1/60), physique entièrement en fixed-point 16.16
  (pas de FPU garanti), `canStand` porté sur les cases.
- `src/ps1/boot.cpp` remplacé par la boucle de jeu ; le diagnostic reste disponible via `__PS1_DIAG__`.
- Entrées : flèches/Pad, Croix = saut, Carré = attaque, R1 = course ; mêmes fronts montants que le PC.

## Vérification

1. `powershell -File tools/build_ps1.ps1` → `PS1_BUILD: PASS`.
2. `powershell -File tools/ps1_smoke.ps1` → boot CD et hits `_start/main/pfInit/pfPresent/pfDrawRect`.
3. Capture native 320×240 via `docs/ps1/lua/capture_boot.lua`, inspection visuelle réelle.
4. Boot de l'ISO 2048 dans PCSX-Redux (log horodaté dans `qa/ps1_logs/`).
5. Régression PC : build OK, validateur `ERR: 0 | WARN: 0`, CTest 81/81, `git diff --check`.
6. Aucun fichier binaire/commit d'image ; seule la version locale dans `ps1/a_graver/`.

## Livrable

`ps1/a_graver/alundra2_ntsc.iso` régénéré, gravable, avec `SHA256.txt` mis à jour.
