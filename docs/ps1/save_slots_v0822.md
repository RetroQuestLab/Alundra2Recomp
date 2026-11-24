# Alternance des slots de carte mémoire PS1 sur l'ISO v0.8.22

Rapport établi le 2026-11-24. Périmètre : **harnais uniquement**. ISO, BIN, CUE, EXE, ELF, MAP, manifestes, code du jeu, `data/*`, rendu PC, route A→Z et gates restent inchangés. Aucun commit ni push.

## Cible

- ISO 2048 `/ps1/a_graver/v0.8.22-ps1-spu-peak/alundra2_ntsc.iso` SHA256 `26B2228165B70922A121FDF6C2E33F979C1BB94D45B513E0B310B4496960BE54`.
- ELF `ps1/build/alundra2.elf` SHA256 `2F8BCA0E3B8962CE6AD5F01FBDEA04031ED039322DFD63D696A9F88ADE144CA7` (identique à l'ELF du lot).
- Vocabulaire : **exécuté / émulateur** (PCSX-Redux + OpenBIOS), **mesuré / émulateur** (états POD lus en RAM), **console physique non testée**. Aucune valeur FPS revendiquée.

## Méthode

`tools/ps1_save_slots.ps1` enchaîne trois sessions distinctes de `tools/ps1_save_roundtrip.ps1` sur la carte `memcard1.mcd` :

1. `01_write_slot0` : carte réinitialisée depuis `memcard2.mcd`, écriture « Nouvelle partie » avec quantité d'objet marquée à 7 par le harnais ; exigence `seq=1 slot=0` et empreinte `46f25a9f`.
2. `02_write_slot1` : **sans réinitialisation**, garde `ExpectedCardSha256` sur la carte issue de l'étape 1, seconde écriture ; exigence `seq=2 slot=1` et même empreinte.
3. `03_load_last` : arrêt/rerun complet de l'émulateur, garde sur la carte issue de l'étape 2, lancement de « Continuer » ; exigence `hasGame=1 seq=2`, `PS1_SAVE_READ OK seq=2 zone=11 x=660 y=760` et empreinte restaurée identique.

Chaque session vérifie au préalable les SHA256 de l'ISO et de l'ELF. L'empreinte FNV-1a est définie dans `docs/ps1/lua/save_roundtrip.lua` et documentée dans `docs/ps1/save_roundtrip_v0822.md`.

Commande canonique :

```powershell
powershell -ExecutionPolicy Bypass -File tools\ps1_save_slots.ps1 -SkipBuild `
  -Cue "ps1\a_graver\v0.8.22-ps1-spu-peak\alundra2_ps1.cue" `
  -Iso "ps1\a_graver\v0.8.22-ps1-spu-peak\alundra2_ntsc.iso" `
  -ExpectedSha256 26B2228165B70922A121FDF6C2E33F979C1BB94D45B513E0B310B4496960BE54 `
  -ExpectedElfSha256 2F8BCA0E3B8962CE6AD5F01FBDEA04031ED039322DFD63D696A9F88ADE144CA7 `
  -ExpectedDigest 46f25a9f `
  -OutDir "qa\ps1_logs\save_slots_v0822"
```

## Résultats

- Écriture 1 : `PS1_SAVE_WRITE OK seq=1 slot=0`, carte `45401B16431A200E4A3EB393BE9BDC9391A26966AFE6B5898B93700AF1E49B82`.
- Écriture 2 : `PS1_SAVE_WRITE OK seq=2 slot=1`, carte `E63873EC79A55C5E286C677EF091224E3A6EADE8FEC8F47AB1FE42D35130004C` (différente de la précédente).
- Après arrêt/rerun complet : `PS1_SAVE_INIT READY hasGame=1 seq=2`, `PS1_SAVE_READ OK seq=2 zone=11 x=660 y=760`, `SAVE_STATE phase=load digest=46f25a9f`, `menu action=2` (`LOAD_GAME`).
- Verdict : `PS1_SAVE_SLOT_ALTERNATION: PASS`, exit 0. Log principal : `qa/ps1_logs/save_slots_v0822/run.txt` ; journaux par étape dans `01_write_slot0/`, `02_write_slot1/`, `03_load_last/`.

Le numéro de séquence le plus récent est donc bien retenu au démarrage, et l'écriture suivante cible l'autre bloc sans écraser la sauvegarde précédente.

## Contrôle négatif

Invocation avec `-ExpectedSlot2 0` : l'étape 2 échoue explicitement `PS1_SAVE_ROUNDTRIP: FAIL - slot attendu=0 reel=1`, puis `PS1_SAVE_SLOT_ALTERNATION: FAIL - etape 02_write_slot1 a echoue`, exit 1. Log : `qa/ps1_logs/save_slots_v0822_negative/run.txt`. Le contrôle porte sur une attente du harnais, jamais sur une valeur du jeu.

## Limites

- Deux écritures seulement : le comportement après plus de deux sauvegardes (retour au slot 0 avec `seq=3`) n'est pas encore rejoué ici.
- Aucune corruption de bloc, coupure pendant écriture ni carte pleine simulée.
- Émulateur + OpenBIOS uniquement ; carte mémoire, manette et console physiques restent à fournir par l'utilisateur.
