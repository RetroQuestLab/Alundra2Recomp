# Round-trip de sauvegarde PS1 sur l'ISO v0.8.22 exacte

Rapport établi le 2026-11-24. Périmètre : **harnais uniquement**. Aucun octet du jeu, de `data/*`, des lots figés, du rendu PC ni de la route A→Z n'a été modifié. Aucun commit ni push.

## Cible et vocabulaire

- ISO 2048 `/ps1/a_graver/v0.8.22-ps1-spu-peak/alundra2_ntsc.iso`, SHA256 `26B2228165B70922A121FDF6C2E33F979C1BB94D45B513E0B310B4496960BE54`.
- ELF courant `ps1/build/alundra2.elf`, SHA256 `2F8BCA0E3B8962CE6AD5F01FBDEA04031ED039322DFD63D696A9F88ADE144CA7` (identique à l'ELF du lot).
- CUE SHA256 `14AC4495BD8B4F53496E0322C8E83AA5ECC5B1BA1683719946949115BF5DEE33`.
- **Exécuté / émulateur** : PCSX-Redux, OpenBIOS, carte `memcard1.mcd` port 1. **Mesuré / émulateur** : empreinte des états POD en RAM. **Console physique** : non testée (matériel à fournir par l'utilisateur). Aucune valeur FPS ni écoute subjective revendiquée.

## Méthode reproductible

Commande canonique (hachages attendus inclus) :

```powershell
powershell -ExecutionPolicy Bypass -File tools\ps1_save_roundtrip.ps1 -SkipBuild -ResetCard -Seconds 180 `
  -Cue "ps1\a_graver\v0.8.22-ps1-spu-peak\alundra2_ps1.cue" `
  -Iso "ps1\a_graver\v0.8.22-ps1-spu-peak\alundra2_ntsc.iso" `
  -ExpectedSha256 26B2228165B70922A121FDF6C2E33F979C1BB94D45B513E0B310B4496960BE54 `
  -ExpectedElfSha256 2F8BCA0E3B8962CE6AD5F01FBDEA04031ED039322DFD63D696A9F88ADE144CA7 `
  -ExpectedDigest 46f25a9f `
  -OutDir "qa\ps1_logs\save_roundtrip_v0822"
```

1. Garde `-ExpectedSha256` / `-ExpectedElfSha256` : les deux fichiers sont hachés avant tout lancement ; un écart arrête immédiatement le harnais.
2. Session **write** (`docs/ps1/lua/save_roundtrip.lua`) : navigation pilotée par l'état réel (rendus du menu/intro), START → CROIX « Nouvelle partie » → START saute l'intro → SELECT déclenche `saveWriteGamePS1()` dans `src/ps1/boot.cpp:295-298`. Une quantité possédée est marquée à `7` **côté harnais** avant SELECT (`g_inv`, capture-only) pour que la comparaison ne soit pas triviale. Empreinte de l'état imprimée juste avant l'appui.
3. **Arrêt complet du processus PCSX-Redux**, puis redémarrage à froid sur le même CUE, carte mémoire persistée sur disque (`memcard1.mcd`, hash avant/après consigné).
4. Session **load** : attente de la détection asynchrone `hasSave==1` (`src/ps1/boot.cpp:139`), START, CROIX sur « Continuer », puis empreinte de l'état restauré. Le harnais exige `PS1_SAVE_INIT READY hasGame=1 seq=1`, `PS1_SAVE_READ OK seq=1 zone=11 x=660 y=760` et `action=2` (`MENU_ACT_LOAD_GAME`, pas `MENU_ACT_NEW_GAME`).

## Définition de l'empreinte

`stateDigest()` (Lua) calcule un FNV-1a 32 bits sur les octets des champs réellement sérialisés :

- `QuestState` (`src/ps1/quest_ps1_logic.h`) : 22 entiers aux offsets `0,4,…,56` puis `68,…,92`. `prevPad` (60) et `gates` (64) sont exclus : le premier est remis à zéro par `saveWriteGamePS1`/`saveReadGamePS1`, le second est réservé au harnais de capture.
- `PSIInventory` (`src/ps1/inventory_ps1_logic.h`) : `def` et les deux octets de `qty` des 64 entrées, puis `weapon`, `ring`, `pocket` aux offsets 256/257/258. `page`, `cursor`, `open` et `prevPad` sont exclus (état d'interface, remis à zéro au chargement).

Les deux `QuestState` et inventaires sont lus en RAM sans aucune écriture du harnais ; seule la mutation capture-only décrite au pas 2 est appliquée.

## Résultats

- `PS1_SAVE_INIT READY hasGame=0 seq=0` au premier boot ; `PS1_SAVE_WRITE OK seq=1 slot=0`.
- Carte mémoire : `AC0DBCB8…` avant écriture, `45401B16…` après (fichier copié dans `qa/ps1_logs/save_roundtrip_v0822/memcard1_after_write.mcd`).
- Après redémarrage complet : `PS1_SAVE_INIT READY hasGame=1 seq=1`, `PS1_SAVE_READ OK seq=1 zone=11 x=660 y=760`, `menu action=2`.
- **Empreinte à l'écriture = empreinte après relecture = `46f25a9f`** : quête, position et inventaire (quantité marquée incluse) survivent à l'arrêt/rerun.
- Verdict : `PS1_SAVE_ROUNDTRIP: PASS`, exit 0. Log : `qa/ps1_logs/save_roundtrip_v0822/save_roundtrip.txt`.

## Contrôles négatifs

| Contrôle | Résultat |
|---|---|
| `-ExpectedSha256 00…00` | `FAIL - ISO_SHA256 attendu=… reel=26B22281…`, avant tout lancement (`negative_control.txt`). |
| `-ExpectedDigest deadbeef` | `FAIL - digest attendu=deadbeef reel=46f25a9f` (`qa/ps1_logs/save_roundtrip_v0822_negative_digest/run.txt`). |
| `-ExpectedX 661` | `FAIL - relecture seq=1 zone/x/y attendus 11/661/760 absente` (`…_negative_fields/run.txt`). |

Cause d'un échec intermédiaire identifiée par débogage systématique : le harnais écrivait la quantité marquée à l'adresse MIPS complète (`0x801c40ac`) au lieu de l'offset `addr - 0x80000000` attendu par le pointeur Lua ; l'exception interrompait silencieusement le listener VSync, d'où l'absence de `PS1_SAVE_WRITE OK`. Correction dans `docs/ps1/lua/save_roundtrip.lua` uniquement ; aucune valeur du jeu n'a été adaptée à un test faux. Logs de diagnostic : `qa/ps1_logs/save_roundtrip_v0822_debug{3,4,5}/`.

## Limites

- Un seul cycle écriture → arrêt → relecture, et un seul slot (0). L'alternance des deux slots et le choix du numéro de séquence le plus récent font l'objet de la tâche suivante.
- Émulateur + OpenBIOS uniquement. Le comportement d'une carte mémoire physique, d'une manette réelle et du lancement homebrew sur console reste à valider avec le matériel fourni par l'utilisateur.
- Aucune preuve visuelle ou subjective n'est substituée aux marqueurs déterministes du log.
