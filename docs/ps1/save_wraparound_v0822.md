# Retournement des slots de carte mémoire PS1 — écriture 3 (ISO v0.8.22)

Rapport établi le 2026-11-24. Périmètre : **harnais uniquement**. Aucun octet du jeu, de `data/*`, des lots figés, du rendu PC ni de la route A→Z n'a été modifié. Aucun commit ni push.

## Cible et vocabulaire

- ISO 2048 `/ps1/a_graver/v0.8.22-ps1-spu-peak/alundra2_ntsc.iso`, SHA256 `26B2228165B70922A121FDF6C2E33F979C1BB94D45B513E0B310B4496960BE54`.
- ELF `ps1/build/alundra2.elf` SHA256 `2F8BCA0E3B8962CE6AD5F01FBDEA04031ED039322DFD63D696A9F88ADE144CA7` (identique à l'ELF du lot).
- **Exécuté / émulateur** : PCSX-Redux + OpenBIOS, carte `memcard1.mcd`. **Mesuré / émulateur** : états POD lus en RAM. **Console physique non testée**. Aucune valeur FPS revendiquée.

## Hypothèse validée

`ps1SaveWriteSlot(best)` (`src/ps1/save_ps1_logic.h:208-210`) cible toujours l'autre slot du meilleur, et `ps1SaveNextSeq(3) = 4` (dez 213-215 : repli `0xFFFF → 1`). Au troisième save, `best=1`, donc le slot écrit devrait être `0` avec `seq=3`. Le harnais le vérifie.

## Méthode reproductible

```powershell
powershell -ExecutionPolicy Bypass -File tools\ps1_save_slots.ps1 -SkipBuild `
  -SaveCount 3 `
  -Cue "ps1\a_graver\v0.8.22-ps1-spu-peak\alundra2_ps1.cue" `
  -Iso "ps1\a_graver\v0.8.22-ps1-spu-peak\alundra2_ntsc.iso" `
  -ExpectedSha256 26B2228165B70922A121FDF6C2E33F979C1BB94D45B513E0B310B4496960BE54 `
  -ExpectedElfSha256 2F8BCA0E3B8962CE6AD5F01FBDEA04031ED039322DFD63D696A9F88ADE144CA7 `
  -ExpectedDigest 46f25a9f `
  -OutDir "qa\ps1_logs\save_wraparound_v0822"
```

`tools/ps1_save_slots.ps1` effectue trois sessions PCSX-Redux distinctes (`01_write_slot0`, `02_write_slot1`, `03_write_slot0`, `04_load_last`) : chaque session vérifie les SHA256 de l'ISO et de l'ELF ; la carte est reset par `memcard2.mcd` à la première ; le harnais exige `seq=1/2/3`, `slot=0/1/0` et échange `ExpectedCardSha256` entre étapes ; le verdict final exige `PS1_SAVE_INIT READY hasGame=1 seq=3` lu dans `04_load_last\02_load_after_poweroff\stdout.txt` (arrêt/rerun émulateur complet avant).

## Résultats (positif, `qa/ps1_logs/save_wraparound_v0822/run.txt`)

- `ECRITURE_1 seq=1 slot=0 carte=45401B16431A200E4A3EB393BE9BDC9391A26966AFE6B5898B93700AF1E49B82`
- `ECRITURE_2 seq=2 slot=1 carte=E63873EC79A55C5E286C677EF091224E3A6EADE8FEC8F47AB1FE42D35130004C`
- `ECRITURE_3 seq=3 slot=0 carte=7E1F67DBC9130CD2A9834EBB58DB5A8F7C0FD9A5F56C42C3FD5100491B6EA59B`
- `RETOURNEMENT slot=0 apres 3 ecritures (slot initial=0)`
- `SEQUENCE_FINALE seq=3 (superieur a 1)`, `ETAT_RESTAURE digest=46f25a9f`
- Enfant `04_load_last\02_load_after_poweroff\stdout.txt` : `PS1_SAVE_INIT READY hasGame=1 seq=3`, `PS1_SAVE_READ OK seq=3 zone=11 x=660 y=760`, `SAVE_STATE phase=load digest=46f25a9f`, `menu action=2` (`LOAD_GAME`).
- Verdict : `PS1_SAVE_SLOT_ALTERNATION: PASS`, exit 0.

## Contrôles négatifs

| Contrôle | Résultat (exit 1 attendu) |
|---|---|
| `-SaveCount 4` | Rejet par le paramètre : « L'argument 4 est supérieur à la plage maximale autorisée de 3 ». |
| `-ExpectedSlot3 1` avec `-SaveCount 3` | `FAIL - attente invalide: slot3=1 different de slot1=0` (invariant wraparound côté harnais, échec immédiat). |
| `-ExpectedSeq3 4` avec `-SaveCount 3` | Ecriture 3 observée `seq=3 slot=0`, `PS1_SAVE_ROUNDTRIP: FAIL - sequence attendue=4 reelle=3` → `etape 03_write_slot0 a echoue` (`qa/ps1_logs/save_wraparound_v0822_negative/run_seq.txt`). |

## Limites

- Un seul retournement à `seq=3 → slot=0` rejoué (le log de plus de trois écritures ne concerne pas ce harnais, mais la classe `ps1SaveWriteSlot` rend la période de 2 implicite).
- Pas de corruption, coupure pendant écriture ni carte pleine.
- Émulateur + OpenBIOS uniquement ; matériel PS1 à fournir par l'utilisateur.

