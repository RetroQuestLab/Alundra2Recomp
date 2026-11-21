# Menu de lancement PS1 — document de conception (2026-11-21)

## Objectif

Ajouter, **avant la prochaine ISO gravable**, un écran titre et un menu principal
native PS1 : `Continuer`, `Nouvelle partie`, `Options`, `Quitter`. Le titre porte
la mention **Alundra 2**. Le prototype reste limité à la zone 011 (déplacement,
course, saut, attaque, caméra, HUD 4 cœurs) : aucun dialogue, PNJ, coffre, objet,
musique ni changement de zone n'est ajouté.

## Décisions de conception

- Approche **PS1 complète** : `Continuer` repose sur une vraie sauvegarde carte mémoire.
- Titre = **Alundra 2** ; rendu laissé à notre choix → présentation Alundra 1
  (police `FONT3.TIM`, cadres façon `WIND`), textes français A2.
- `Quitter` = écran d'adieu (« Merci d'avoir joué — éteignez la console »), pas de
  soft-reset.
- `Continuer` est **grisé** tant qu'aucune sauvegarde valide n'existe.

## État des lieux

- Le PC possède déjà `src/title.cpp` (mêmes libellés), mais il dépend de SDL,
  zones, save fichier et audio : non portable tel quel.
- Le PS1 démarre actuellement directement dans `main()` → `zoneInit()` /
  `heroInit()`. Il n'existe ni écran titre ni sauvegarde.
- `pf.h` / `pf_ps1.cpp` fournissent rectangles opaques, sprites 4bpp, manette et
  double buffer ; `SetVideoMode(MODE_NTSC)` est aujourd'hui figé dans `pfInit()`.
- PSn00bSDK expose `_card_read/_card_write` (secteurs de 128 octets), `InitCARD`,
  `StartCARD`, `_card_info`, `_card_status`, `_card_wait`, `_card_clear`.
- La police A1 est déjà générée en 1 bpp dans `src/a1_font.h` (256 glyphes 16×16,
  index = octet Latin-1) mais ce header C++ (`std::array`) n'est pas compilable
  par le toolchain MIPS.

## Architecture

### Machines d'états

`BOOT → TITLE → (GAME | OPTIONS | FAREWELL)`

- `TITLE` : fond = zone 011 en lent travelling, voile sombre semi-transparent,
  titre « ALUNDRA 2 » en police A1 dorée, menu en 4 entrées.
- `GAME` : boucle actuelle inchangée ; **Select** sauvegarde puis revient au titre
  (indispensable pour que `Continuer` soit observable). **Start** sauvegarde avec
  un indicateur « SAUVEGARDÉ » temporaire.
- `OPTIONS` : `Vidéo : NTSC (60 Hz) / PAL (50 Hz)`, `Commandes`, `Retour`.
  Le changement de mode vidéo est **enregistré et appliqué au prochain lancement**
  (avant `pfInit()`) : pas de réinitialisation GPU risquée en pleine partie.
- `FAREWELL` : écran bleu nuit, texte A1, boucle figée après `pfPresent()`.

### Fichiers

| Fichier | Rôle |
|---|---|
| `src/ps1/menu_ps1.h/.cpp` | états TITLE/OPTIONS/FAREWELL, saisie haut/bas/Croix/Cercle/Retour, rendu |
| `src/ps1/save_ps1.h/.cpp` | init carte, lecture/écriture des deux enregistrements, CRC, détection validité |
| `src/ps1/a1_font_ps1.h` | atlas 4bpp généré (256 glyphes) + 4 CLUT (or, blanc, cyan sélection, grisé) |
| `tools/ps1_export_font.py` | génère l'atlas depuis `src/a1_font.h`, manifeste figé |
| `src/platform/pf.h`, `src/ps1/pf_ps1.cpp` | `pfDrawRectSemi()`, `pfSetVideoMode()`, `pfPresent()` inchangé sinon |
| `src/ps1/hero_ps1.h/.cpp` | `heroSetPos(x,y)` pour `Continuer` |
| `src/ps1/boot.cpp` | séquence BOOT/TITLE/GAME, chargement options avant `pfInit()` |
| `ps1/CMakeLists.txt` | ajout des nouvelles sources |
| `tools/ps1_export_zone.py` + test | réserver la zone VRAM de l'atlas et des CLUT |
| `docs/ps1/lua/capture_menu.lua` | captures natives 320×240 de chaque écran |
| `docs/ps1/premiere_zone_jouable.md` | périmètre |

### VRAM

Atlas police : `x=0..127` demi-mots, `y=256..511` (256×256 px 4bpp = ~128 Ko).
CLUT : `x=128..143`, `y=256` (4 banques 16 couleurs). Scratch héros inchangé
(`512,256,128,64`). Ces deux rectangles sont ajoutés à `RESERVED_RECTS` pour que
`ps1_export_zone.py` ne les écrase pas. Les tuiles de la zone tiennent dans
l'espace restant (~470 Ko pour ~315 Ko de blobs).

### Sauvegarde carte mémoire

Un seul bloc de 8 Ko (64 secteurs de 128 octets), format BIOS standard :

- Bloc 0 (répertoire) : entrée `0x51`, taille `0x2000`, nom `ALUNDRA2PS1`,
  checksum XOR ; on ne réclame qu'un bloc libre, sans toucher aux autres saves.
- Bloc de données : secteur 0 = `SC`, drapeau icône `0x11`, titre `ALUNDRA 2`.
- Secteurs 4 et 5 : deux copies de l'enregistrement de partie —
  magic `A2SV`, version `u16`, zone `u8`, `x/y` `s16`, cœurs `u8`, CRC16.
- Secteurs 6 et 7 : deux copies des options — magic `A2OP`, version, mode vidéo,
  CRC16.

Écriture : données d'abord, entrée de répertoire en dernier. Lecture : toute
incohérence de magic/version/CRC rend l'enregistrement invalide → `Continuer`
grisé. Sur carte absente ou non formatée, le menu reste jouable et `Continuer`
est grisé ; un message discret l'indique dans `Options`.

### Entrées

| Touche | Titre | Options | Jeu |
|---|---|---|---|
| Haut/Bas | déplacer sélection | déplacer sélection | — |
| Gauche/Droite | — | régler vidéo | — |
| Croix | valider | valider / cycle | saut (inchangé) |
| Cercle | retour (depuis options) | retour | — |
| Select | — | — | sauvegarder + retour titre |
| Start | — | — | sauvegarde rapide |

`pfPad()` fournit déjà un masque actif en haut ; aucun changement de polarité.

### Validation

1. `py -3.12 tools/ps1_export_zone_test.py` (doit rester 14/14, dont
   `test_font_vram_reserved`).
2. `powershell -ExecutionPolicy Bypass -File tools/build_ps1.ps1` → `PS1_BUILD: PASS`.
3. Captures natives via `capture_menu.lua` : titre, options, grisé, sauvegarde
   active, retour titre, écran d'adieu ; conversion PNG 320×240 ; inspection
   visuelle réelle.
4. Test carte mémoire : écriture, **power-off de l'émulateur**, relance, lecture →
   `Continuer` actif et position restaurée.
5. `tools/ps1_smoke.ps1` (adresses ELF relues), puis les quatre commandes PC
   canoniques et `git diff --check`.

## Hors périmètre

Dialogues, police de dialogue, WIND complet, PNJ, coffres, objets, musique,
sauvegarde multi-slots, changement de zone, PC (déjà pourvu de son propre menu).
Aucun push, aucune publication de ressources ou d'ISO.
