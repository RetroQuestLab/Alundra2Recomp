# Alundra2Recomp

Remake C++20 d’**Alundra 2** avec une présentation visuelle inspirée d’**Alundra 1** et une narration conservée d’Alundra 2. Deux cibles coexistent dans le même dépôt :

- **PC / SDL3** : version de référence, complète et couverte par une suite de tests déterministe.
- **PlayStation 1 native** : prototype jouable de la zone 011, exécutable **PS-X EXE MIPS** produit avec **PSn00bSDK**, image disque bootable en NTSC 60 Hz.

## État du projet

| Cible | État |
|---|---|
| PC SDL3 | Jouable : dialogue, menu, équipement, forge, sauvegardes, progression A → Z, 81 tests CTest |
| PS1 native | Prototype : décor A1 de la zone 011, héros, caméra, course, saut, attaque, HUD 4 cœurs |
| ISO PS1 | Générée localement pour gravure ; non distribuée dans ce dépôt |

Le portage PS1 n’est **pas** une émulation ni un filtre « rétro » appliqué à la version PC : il utilise le GPU, le PAD et la carte mémoire de la console via PSn00bSDK.

## Avertissement important

Ce dépôt ne contient **aucune ressource commerciale** provenant des jeux Alundra : ni sprites, ni cartes, ni polices, ni musiques, ni vidéos, ni image disque. Pour compiler les cibles, il faut fournir localement ses propres copies des ressources et des outils dans les dossiers ignorés `assets/`, `extracted/`, `gp/`, `images/` et `external/`.

Les en-têtes générés à partir de ces ressources sont également exclus du dépôt et doivent être régénérés localement.

## Prérequis

### PC

- Windows avec PowerShell
- CMake 4.4 ou plus récent, Ninja, Python 3.12
- Toolchain MinGW fournie dans `external/`
- SDL3 3.4.20 fournie dans `external/`

### PlayStation 1

- PSn00bSDK 0.24, compilateur MIPS et `mkpsxiso` dans `external/tools/`
- BIOS open-source OpenBIOS pour les tests en émulateur
- Émulateur compatible pour validation (PCSX-Redux ou DuckStation)
- Console physique, manette et carte mémoire pour validation matérielle

Les outils et BIOS ne sont pas versionnés.

## Compiler la version PC

Depuis la racine du dépôt :

```powershell
powershell -ExecutionPolicy Bypass -File build.ps1
py -3.12 tools\validate_data.py
ctest --test-dir build --output-on-failure
git diff --check
```

Attendu : build réussi, validateur `ERR: 0 | WARN: 0`, suite CTest entièrement verte et aucun diff fautif.

## Compiler la cible PS1

```powershell
powershell -ExecutionPolicy Bypass -File tools\build_ps1.ps1
powershell -ExecutionPolicy Bypass -File tools\ps1_smoke.ps1 -Seconds 300
```

Le build PS1 génère un PS-X EXE, un BIN/CUE et une image ISO 2048 dans `ps1/build/`. L’image destinée à la gravure est convertie dans `ps1/a_graver/`. Aucun de ces fichiers n’est versionné.

## Commandes de jeu utiles

```text
--auto <script>        Scénario déterministe
--expect <cle=valeur>  Assertion sur l’état de jeu
--savetest             Test de sauvegarde
--zone <id>            Charge directement une zone
--at <x> <y>           Position de départ
```

## Commandes PS1

| Action | Touche |
|---|---|
| Déplacement | Croix directionnelle |
| Course | R1 |
| Saut | Croix |
| Attaque | Carré |
| Sauvegarde rapide | Start |
| Sauvegarde et retour titre | Select |

## Structure

```text
src/            Cœur C++20, backend PC SDL3 et backend PS1
data/           Dialogues, objets, cartes et données de gameplay
tools/          Convertisseurs, validateurs et scripts de build
tests/          Scénarios déterministes et fixtures
ps1/            Configuration CMake, ISO et sorties PS1
docs/ps1/       Notes techniques de la cible PS1
docs/remake/    Notes de contenu et de couverture narrative
```

## Licence

Le code et la documentation de ce dépôt sont publiés sous **GPL-3.0**. Voir `LICENSE` et `SOURCES_LICENSES.md`.

Les ressources commerciales Alundra ne sont pas incluses et restent la propriété de leurs ayants droit.
