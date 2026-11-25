# OPENCODE_HANDOFF — Reprise du remake Alundra 2 par OpenCode

Rédigé le 2026-11-25 par Claude Code pour OpenCode. **Lis tout ce fichier avant d'agir.**
Tu dois **tenir ce fichier à jour au fil de ton travail** : section « Journal OpenCode » en bas (date, ce qui est fait, preuves, prochaine action). Ne présente jamais un travail partiel comme terminé.

---

## 0. Règles impératives (résumé de AGENTS.md — lis aussi AGENTS.md et CLAUDE.md)

- Remake C++20 / SDL3, **sans moteur externe**. Cibles : PC (référence de non-régression) **et PS1 native** (PSn00bSDK 0.24, `external/tools/psn00bsdk/PSn00bSDK-0.24-win32`).
- **Aucun push**, aucune publication. Commits **locaux** autorisés, message clair.
- Ne jamais committer `assets/`, `extracted/`, `gp/`, `images/`, `pb/`, `*.bin`, `*.cue`, `*.iso`, `*.chd`, `*.mp4`, `ps1/assets/license_scee.dat`, `.claude/settings.local.json`.
- Ne pas modifier : route A→Z, gates `epee_supreme`/`lampe`/`prisme`/`boss_medusa`/`cristal`, identifiants de dialogue testés, modes `--auto/--expect/--savetest`.
- Visuel = Alundra 1 ; narration = Alundra 2.
- Préserver le stash `stash@{0}` et les fichiers non suivis existants (`Alundra_2_Histoire_et_trame_detaillee.txt`, `opencode.json.bak-*`, `qa/`).
- `notes.md` est **exclu de git** (`.git/info/exclude`) : le mettre à jour quand même (section « ÉTAT ACTUEL — REPRISE »).

### Commandes canoniques (depuis `C:\Users\Liori\Alundra`)

```powershell
powershell -ExecutionPolicy Bypass -File build.ps1          # PC
py -3.12 tools\validate_data.py                              # attendu ERR: 0 | WARN: 0
ctest --test-dir build --output-on-failure                   # suite PC verte
git diff --check
powershell -ExecutionPolicy Bypass -File tools\build_ps1.ps1 # PS1 : ps1\build\alundra2.exe + alundra2_ps1.bin/.cue, doit afficher PS1_BUILD: PASS
powershell -ExecutionPolicy Bypass -File tools\ps1_cold_boot.ps1 -Lot <dossier>   # démarrage à froid émulateur (OpenBIOS), doit finir PS1_COLD_BOOT: PASS
```

`ps1_cold_boot.ps1` attend dans `ps1\a_graver\<lot>\` : `alundra2_ps1.bin`, `alundra2_ps1.cue` **et** `alundra2.elf` (copier `ps1\build\alundra2.elf`).

---

## 1. Situation au 2026-11-25 (faits vérifiés sur la console réelle de l'utilisateur)

| Image | Résultat sur PS1 réelle |
|---|---|
| v0.1.0 … premières versions (sans licence) | démarraient |
| v0.8.40-trame-a2 (sans licence) | **1er démarrage à froid : bloque sur écran bleu** ; **2e essai : démarre**, titre visible, jeu jouable mais « moche, plein de bugs » |
| v0.8.41-licence (secteurs 0-15 du disque original SLES-02601) | **reste sur le logo « Sony Computer Entertainment Europe »**, ne démarre pas |
| v0.8.42-diag-couleurs (sans licence + repères couleur) | quand ça bloque : **écran bleu** |
| Toutes | démarrent dans l'émulateur de l'utilisateur et dans DuckStation + OpenBIOS (cold boot PASS) |

Conclusions :
1. **La licence empêche le démarrage sur cette console** → la retirer de toutes les livraisons (tâche 2).
2. **Blocage intermittent au démarrage à froid**, invisible en émulateur. L'écran bleu foncé = couleur de fond `setRGB0(&g_draw[i], 8, 12, 32)` posée par `PutDrawEnv` (isbg=1) à la fin de `pfInit()` (`src/ps1/pf_ps1.cpp:126`).
3. Sur v0.8.42, le premier repère (`bootMark(255,0,0)` ROUGE, juste après `pfInit()` dans `src/ps1/boot.cpp`) **n'apparaît jamais** quand ça bloque. Donc le blocage est **entre `PutDrawEnv` dans `pfInit()` et la fin du premier `bootMark`** (ou le bleu observé est un autre bleu — à confirmer avec l'utilisateur : bleu foncé/nuit ou bleu vif ?). Le repère MAGENTA a remplacé l'ancien BLEU, il n'y a plus de repère bleu.

Disque témoin livré : `ps1\a_graver\v0.8.42-diag-couleurs\alundra2_ps1.cue` (SHA256 du bin `2ba5c0cb9dc94990b1ef66e581c16bac6bd454884d591a2077eae77a84be243e`), commit `4675ab3`.
Table des couleurs (dans l'ordre) : ROUGE affichage prêt → JAUNE carte mémoire init → VERT SPU init → CYAN avant CdInit/lecture musique CD → MAGENTA lecture CD OK → BLANC tout initialisé → dans la boucle (120 premières images) ORANGE carte mémoire, GRIS flux CD, VERT FONCÉ menu.

---

## 2. Tâches, dans l'ordre (ne pas sauter)

### Tâche 1 — Corriger le blocage intermittent au démarrage (PRIORITÉ ABSOLUE)

Suspects dans `pfInit()` (`src/ps1/pf_ps1.cpp:81-129`) et `bootMark()` (`src/ps1/boot.cpp`) :
- `VSyncHaltFunction(&ps1VsyncHalt)` installé **avant** tout le reste ; `ps1VsyncHalt` (`pf_ps1.cpp:39`) appelle `saveCardOpPendingPS1()` et `ChangeClearPAD` — dépend de la carte mémoire alors que `saveEarlyInitPS1()` n'a pas encore tourné.
- `InitPAD/StartPAD/ChangeClearPAD(0)` avant le premier `DrawSync` : état d'IRQ différent au froid sur vrai BIOS Sony.
- `SetRCnt(RCntCNT1, 0xffff, RCntMdFR)` : reprogramme un compteur que le BIOS Sony utilise peut-être.
- `DrawSync(0)` dans `bootMark` peut attendre indéfiniment si la file GPU/IRQ n'est pas dans l'état attendu.
- `SetVideoMode(MODE_NTSC)` forcé sur console PAL (europe) : à vérifier, mais ce n'est pas un blocage normalement.

Méthode (systematic-debugging, une variable à la fois, **chaque essai = un disque que l'utilisateur grave**, donc minimiser le nombre d'essais) :
1. Comparer l'ordre d'initialisation avec le template du SDK qui marche sur matériel (`external/tools/witness_clean/` et les exemples `PSn00bSDK…/share/psn00bsdk/examples/`, notamment `hellocpp`) : ResetGraph → envs → affichage → seulement ensuite PAD/carte/CD.
2. Produire **un seul** disque de bissection plus fin : ajouter des repères couleur **à l'intérieur** de `pfInit()` (après ResetGraph, après VSyncHaltFunction, après SetRCnt, après InitPAD/StartPAD/ChangeClearPAD, après PutDrawEnv) avec des couleurs distinctes, PAS de bleu. Pour peindre avant `PutDrawEnv`, utiliser une primitive `FILL` + `DrawPrim` (voir `bootMark`, `ClearImage` n'existe pas dans PSn00bSDK 0.24) et `SetDispMask(1)` tôt.
3. Ajouter des **délais maximaux** à toute attente (`DrawSync`, `CdReadSync(0,0)` dans `src/ps1/audio_ps1.cpp` startStreamNow/prefill/feed et `src/ps1/zone_heap_ps1.cpp`, boucles carte mémoire de `src/ps1/save_ps1.cpp`) : on préfère une musique absente à une console figée.
4. Candidat de correctif probable : réordonner `pfInit()` comme le template (affichage d'abord, `VSyncHaltFunction` et PAD après le premier `DrawSync`), et ne lancer la musique CD (`audioStreamMusicPS1(1)`) qu'après la première image du menu.
5. Après le correctif : retirer (ou garder derrière `#ifdef PS1_BOOTMARK`) les repères couleur de `boot.cpp`.
6. Livrer `ps1\a_graver\v0.8.43-boot-froid\` (bin+cue+elf, **sans licence**), `ps1_cold_boot.ps1` PASS, et demander à l'utilisateur **5 démarrages à froid consécutifs** (éteindre 10 s entre chaque). Critère de réussite : 5/5 jusqu'au titre.

### Tâche 2 — Retirer la licence des livraisons

- `ps1/iso.xml:18` : supprimer `<license file=…license_scee.dat/>`.
- `tools/build_ps1.ps1` : supprimer le bloc « Licence du disque » (lecture `ALUNDRA_LICENSE_DISC` / `*SLES*.bin`, écriture `ps1/assets/license_scee.dat`) et son `throw` ; mettre à jour le commentaire qui affirme qu'une vraie PS1 se bloque sans licence (c'est **faux** pour cette console : elle se bloque **avec**).
- Vérifier : secteurs 0-15 du bin = formulaire 2 vides (sous-en-tête `00002000`), comme v0.8.40. Script de contrôle d'exemple : lire 16×2352 octets, octets 16-23 de chaque secteur.
- Le v0.8.42 a été fabriqué en recopiant les 16 premiers secteurs de v0.8.40 ; après cette tâche le build doit le produire directement.

### Tâche 3 — Recueillir et corriger les bugs visibles sur console

L'utilisateur dit « c'est moche, plein de bugs ». **Ne pas deviner** : lui demander en une seule fois une liste (écran, ce qu'il voit, photo si possible). Points connus à vérifier en priorité, car l'émulateur peut masquer des différences matérielles :
- mode vidéo NTSC forcé sur console PAL (image qui défile/noir et blanc selon le câble) → envisager `MODE_PAL` automatique (`GetVideoMode()`), en vérifiant `timing_ps1.h` et le cadencement 50/60 Hz ;
- transferts VRAM non alignés (voir `loadImageAligned` dans `pf_ps1.cpp`) ;
- tests dans DuckStation avec options « précises » (GPU logiciel, timing CD précis, pas de fast boot) avant de livrer.

### Tâche 4 — Reprendre le backlog normal

Seulement une fois le démarrage 5/5 confirmé : reprendre la « prochaine action » de `notes.md` § « ÉTAT ACTUEL — REPRISE » (scénario A2, zones, visuel A1). À chaque livraison : nouveau dossier `ps1\a_graver\v0.8.N-nom\`, PS1_BUILD PASS, cold boot PASS, quatre commandes PC vertes, commit local.

---

## 3. Livraison à l'utilisateur (format)

- Toujours indiquer le **chemin complet du `.cue`** à graver et le SHA256 du `.bin`.
- Gravure : mode **DAO / disc-at-once**, vitesse lente, fichier sélectionné = le `.cue`.
- Dire exactement quoi observer et quoi rapporter (couleur, écran, étape).
- Pas de question finale « veux-tu que je continue ? » : enchaîner sur la tâche suivante.

---

## 4. Journal OpenCode (à compléter à chaque étape)

| Date | Tâche | Fait | Preuves (commandes, logs, hash) | Prochaine action |
|---|---|---|---|---|
| 2026-11-25 | (Claude Code) | Diagnostic console, disque v0.8.42-diag-couleurs, ce fichier | commit 4675ab3, cold boot PASS | OpenCode : Tâche 1 étape 1 |
