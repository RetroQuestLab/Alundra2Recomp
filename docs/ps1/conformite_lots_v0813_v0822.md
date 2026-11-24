# Conformité reproductible des lots PS1 v0.8.13 → v0.8.22

Document de revue finale, état au 2026-11-24. Aucun changement de code, de rendu, de `data/*`, de gameplay ni d'artefact de lot livré. Aucun commit ni push. Le matériel de console physique reste à fournir par l'utilisateur.

## Méthode

1. Lire `notes.md` (blocs « BLOC AUTORITAIRE » v0.8.13 → v0.8.22), `AGENTS.md` et `docs/superpowers/plans/2026-11-24-ps1-spu-peak.md`.
2. Pour chaque lot : vérifier la présence de `BUILD_INFO.txt` et `SHA256.txt`, recalculer les 7 hashes du manifeste avec `Get-FileHash`, puis comparer le hash ISO du manifeste à celui annoncé dans `notes.md`.
3. Vérifier les pointeurs `ps1/a_graver/DERNIERE_VERSION.txt` / `SHA256.txt` et la présence d'un rapport `docs/ps1/budget_v08xx.md` par lot.
4. Vérifier que chaque chemin de preuve cité dans les 10 blocs existe réellement, et que `PS1_BUILD: PASS` / `PS1_SMOKE: PASS` apparaissent dans des logs archivés portant le tag du lot.
5. Rejouer les preuves manquantes ou ambiguës sur des artefacts figés, sans réécrire les lots.
6. Contrôler l'affirmation du périmètre protégé (PC, `data/*`, quête, sauvegarde, warp, route A→Z, gates `epee_supreme`/`lampe`/`prisme`/`boss_medusa`/`cristal`, boss/vraie fin, identifiants `auto_dialogue_*`) dans les blocs et les `BUILD_INFO.txt`.
7. Relancer la chaîne canonique PC après l'ajout des outils de revue.

Vocabulaire strict : **compilé/testé hôte**, **exécuté/émulateur**, **mesuré/capturé**, **inspecté visuellement**, **console physique non testée**. Aucune valeur FPS n'est revendiquée ; les conversions ms restent nominales.

## Synthèse par lot

Audit lecture seule des artefacts : **aucun écart**. Les SHA256 sont abrégés (8 premiers + 8 derniers caractères) ; les valeurs complètes sont dans `notes.md` et dans chaque `SHA256.txt` de lot. `7/7` signifie sept artefacts recalculés sans divergence, plus le hash ISO recoupé avec `notes.md`.

| Lot | ISO (SHA256 abrégé) | Artefacts / hash notes | Build + smoke archivés | Preuve complémentaire | Budget | Périmètre protégé | Écart / action |
|---|---|---|---|---|---|---|---|
| v0.8.13-hud-guard | `1270AA36…C7DC57E1` | 7/7 + cohérent | `PS1_BUILD: PASS`, smoke rejoué à la revue | 4 PNG bit-à-bit identiques à v0.8.12 ; `graphismes` CONFORME | `budget_v0813.md` | affirmé dans `notes.md` et `BUILD_INFO.txt` | Log smoke initial non archivé sous un chemin identifiable → **rejoué** sur l'ISO figée |
| v0.8.14-menu-selection-parite | `271A466F…A2B6214D` | 7/7 + cohérent | `PS1_BUILD: PASS`, `PS1_SMOKE: PASS` | `PS1_CAPTURE_MENU`, 7 PNG, `graphismes` CONFORME (4 lus) | `budget_v0814.md` | affirmé | Aucun ; réserve visuelle historique sur 3 PNG non lus conservée |
| v0.8.15-remake-subtitle-orange | `980E54B0…05086D93` | 7/7 + cohérent | `PS1_BUILD: PASS`, `PS1_SMOKE: PASS` | `PS1_CAPTURE_MENU`, 7 PNG, `graphismes` CONFORME | `budget_v0815.md` | affirmé | Aucun ; réserve historique sur le contraste olive des options, corrigée au lot suivant |
| v0.8.16-ps1-options-contrast | `64E36404…BACF52D2` | 7/7 + cohérent | `PS1_BUILD: PASS`, `PS1_SMOKE: PASS` | `PS1_CAPTURE_MENU`, 7 PNG, `graphismes` CONFORME | `budget_v0816.md` | affirmé | Aucun ; comparaison PC indirecte documentée |
| v0.8.17-ps1-menu-hint-blue | `5B0DAE8E…D6742816` | 7/7 + cohérent | `PS1_BUILD: PASS`, `PS1_SMOKE: PASS` | `PS1_CAPTURE_MENU`, 7 PNG, `graphismes` CONFORME (10 lus) | `budget_v0817.md` | affirmé | Aucun ; quantization 5 bits documentée |
| v0.8.18-ps1-intro-scene-sync | `D6CE7F21…2405565D` | 7/7 + cohérent | `PS1_BUILD: PASS`, `PS1_SMOKE: PASS` | `PS1_CAPTURE_INTRO`, 6 PNG, preuve pixel, `graphismes` CONFORME avec réserves | `budget_v0818.md` | affirmé | Aucun ; réserves sur police dense et invite `Ç : passer` conservées |
| v0.8.19-ps1-heap-stack-diag | `5AC2E4D4…0BBCC07D` | 7/7 + cohérent | `PS1_BUILD: PASS`, `PS1_SMOKE: PASS` | `PS1_CAPTURE_DIAG`, `PS1_RENDER_BITWISE` 6/6 | `budget_v0819.md` | affirmé | Aucun ; deux défauts du **test** corrigés avant livraison, documentés |
| v0.8.20-ps1-zone-heap | `AE86E35F…98FF1B76` | 7/7 + cohérent | `PS1_BUILD: PASS`, `PS1_SMOKE: PASS` | `PS1_CAPTURE_ZONE_HEAP`, `PS1_CAPTURE_DIAG`, `PS1_RENDER_BITWISE` 6/6 | `budget_v0820.md` | affirmé | Aucun ; arène capture-only et sonde de pile documentées |
| v0.8.21-ps1-vram-peak | `9C1F24EC…5DAC899A` | 7/7 + cohérent | `PS1_BUILD: PASS`, `PS1_SMOKE: PASS` | `PS1_CAPTURE_VRAM`, `PS1_CAPTURE_ZONE_HEAP`, `PS1_RENDER_BITWISE` 6/6 | `budget_v0821.md` | affirmé | Aucun ; pic VRAM réel distinct du pic statique |
| v0.8.22-ps1-spu-peak | `26B22281…6960BE54` | 7/7 + cohérent | `PS1_BUILD: PASS`, `PS1_SMOKE: PASS` | `PS1_CAPTURE_SPU`, `PS1_CAPTURE_VRAM`, `PS1_RENDER_BITWISE` 6/6 | `budget_v0822.md` | affirmé dans `notes.md`; non énuméré dans `BUILD_INFO.txt` | Mineur documentaire : ajouter la liste au prochain lot, **sans réécrire** celui-ci (manifeste figé) |

Légende : « affirmé » signifie que le bloc et/ou le `BUILD_INFO.txt` déclarent explicitement l'absence de changement. Cette revue vérifie la cohérence documentaire et les preuves sur disque ; elle ne produit pas un diff VCS des lots historiques (aucun commit n'existe, par consigne explicite).

## Résultats des contrôles automatiques

Outils créés pour cette revue (aucun binaire, aucune ISO ni ressource modifiée) :

- `tools/ps1_check_lots.ps1` : `PS1_LOT_CONFORMITY: PASS`, `ARTEFACTS_VERIFIES=70`, `NOTES_HASHES_VERIFIES=10`, `POINTEUR_OK=1`, `ECHECS=0`. Log : `qa/ps1_logs/conformite_v0813_v0822/lot_hashes.txt`.
- `tools/ps1_check_evidence.py` : `PS1_EVIDENCE: PASS`, `BLOCS_AUDITES=10`, `CHEMINS_UNIQUES_VERIFIES=55`, `ECHECS=0`, verdicts `PS1_BUILD: PASS` et `PS1_SMOKE: PASS` retrouvés pour **10/10** lots (`20/20` au total). Log : `qa/ps1_logs/conformite_v0813_v0822/evidence_paths.txt`.
- Contrôle négatif du vérificateur de lots (hash d'artefact altéré dans un lot factice) : `PS1_LOT_CONFORMITY: FAIL`, `ECHECS=2` ; log `lot_hashes_negative_control.txt`.
- Contrôle négatif ciblé sur le recoupage `notes.md` (hash ISO remplacé par zéro dans un manifeste factice) : `FAIL ISO SHA256 notes.md incoherent` ; log `lot_hashes_notes_negative_control.txt`.

La cause d'un premier échec uniforme de `ps1_check_evidence.py` a été identifiée par débogage systématique : les logs `Tee-Object` sont en UTF-16LE avec BOM et étaient lus comme UTF-8 ; le script lisait en outre son propre fichier de sortie truncé. Lecture UTF-16 détectée par BOM et exclusion des trois rapports d'audit → **vert**, sans adaptation d'aucune preuve à un contrôle faux.

## Rejeu du smoke v0.8.13

Le verdict `PS1_SMOKE: PASS` du lot v0.8.13 était annoncé dans `notes.md` et `BUILD_INFO.txt`, mais aucun log archivé ne portait l'ISO dans un chemin identifiable (recherche des 12 verdicts `PS1_SMOKE: PASS` présents dans `qa/ps1_logs`). Un premier rejeu avec l'ELF courant v0.8.22 a échoué à juste titre (`_start hits=0`) : les adresses symboliques du build courant ne correspondent pas à l'ISO figée. Le harnais `tools/ps1_smoke_lot.ps1` copie alors temporairement l'ELF du lot dans `ps1/build/alundra2.elf`, exécute `tools/ps1_smoke.ps1` sur le CUE du lot, puis restaure l'ELF courant et vérifie son hash.

Résultat : `PS1_SMOKE: PASS`, boot `cdrom:\alundra2.exe;1`, `_start=1 main=1 pfInit=1 pfPresent=1 pfDrawRect=752`, `_start=0x80024bb4`, `main=0x8001f54c`, `ELF_RESTORE_SHA256=2F8BCA0E…` (ELF v0.8.22 restauré). Log : `qa/ps1_logs/conformite_v0813_v0822/ps1_smoke_v0813.txt`. Aucun artefact du lot v0.8.13 n'a été modifié.

## Revue indépendante

La revue `requesting-code-review` a été invoquée et un agent `general` en lecture seule a été lancé, mais il n'a pas démarré : limite de quota du provider (`Free usage limit reached`). **Aucun verdict de revue indépendante n'est revendiqué.** À la place, les contrôles déterministes ci-dessus ont été exécutés localement ; ils ne constituent pas une inspection visuelle ni une relecture humaine du code des lots historiques.

## Chaîne canonique après revue

Commandes canoniques relancées depuis `C:\Users\Liori\Alundra` après l'ajout des outils :

- `powershell -ExecutionPolicy Bypass -File build.ps1` → exit 0, `ninja: no work to do`, `OK : build\alundra2.exe`.
- `py -3.12 tools\validate_data.py` → exit 0, `dialogues 150 | sprites 44 | zones 10 | passages 19`, `ERR: 0 | WARN: 0`.
- `ctest --test-dir build --output-on-failure` → exit 0, `100% tests passed out of 81`, 183,10 s.
- `git diff --check` → exit 0, deux avertissements CRLF préexistants (`ps1/iso.xml`, `tools/ps1_export_font.py`).

Logs archivés dans `qa/ps1_logs/conformite_v0813_v0822/` (`pc_build.txt`, `pc_validate.txt`, `pc_ctest.txt`, `pc_diff_check.txt`).

## Non vérifiable dans cette revue

- **Console physique** : non testée pour les dix lots. Modèle/région, lancement homebrew, manette et carte mémoire à fournir par l'utilisateur.
- **Rapports visuels v0.8.13 → v0.8.18** : les verdicts `graphismes` sont ceux des lots, vérifiés comme existants et cités ; aucune nouvelle inspection d'image n'a été réalisée par cette revue. Les lots v0.8.19 → v0.8.22 s'appuient sur des preuves bit-à-bit et des captures instrumentées, pas sur un nouveau jugement esthétique.
- **Diff VCS des dix lots** : impossible sans commits ; la conformité est établie sur les artefacts figés, les manifestes, les logs et les affirmations de périmètre.
- **Écoute subjective** : aucune présentée comme preuve ; seul l'identifiant SPU observé est documenté au lot v0.8.22.

## Décision

Aucune issue Critical ni Important affectant un livrable. Deux points mineurs documentaires sont consignés ci-dessus (smoke v0.8.13 rejoué et archivé ; périmètre non énuméré dans le `BUILD_INFO.txt` v0.8.22). Les dix lots restent figés ; le pointeur courant reste `v0.8.22-ps1-spu-peak`. Aucun commit ni push.
