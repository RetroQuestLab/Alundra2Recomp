# Conformité reproductible des lots PS1 v0.8.4 → v0.8.12

Document de revue finale, état au 2026-11-24. Aucun changement gameplay, aucun commit ni push. Le matériel de console physique reste à fournir par l'utilisateur.

## Méthode

1. Pour chaque lot : relever la version, le chemin ISO, le hash annoncé et les preuves dans `notes.md` (blocs « ÉTAT ACTUEL — REPRISE » puis « BLOC HISTORIQUE »).
2. Recalculer le SHA256 de l'ISO réellement présente dans `ps1/a_graver/<lot>/` (`Get-FileHash` PowerShell) et comparer au hash annoncé.
3. Vérifier la présence des livrables `ISO`, `BUILD_INFO.txt`, `SHA256.txt`, EXE/ELF/MAP/BIN/CUE.
4. Vérifier l'existence du rapport visuel annoncé dans `docs/visuel/` et relever son verdict exact.
5. Vérifier l'existence d'un rapport de budget mesuré dans `docs/ps1/` ; l'absence est signalée, jamais comblée par une estimation.
6. Vérifier que le périmètre protégé est explicitement affirmé : aucun changement PC, `data/*`, quête, sauvegarde, warp, route A→Z, gate, boss, vraie fin ni identifiant `auto_dialogue_*`.

Vocabulaire strict : **compilé/testé hôte**, **exécuté/émulateur**, **capturé/observé**, **inspecté visuellement**, **console physique non testée**. Aucune mesure FPS/p50/p95 ni écoute subjective n'est présentée comme preuve.

## Synthèse par lot

Audit lecture seule du 2026-11-24 (agent `general`, skill `verification-before-completion`) : **aucun écart de hash** sur les huit lots. Les SHA256 sont abrégés ici (8 premiers + 8 derniers caractères) ; les valeurs complètes sont dans chaque `SHA256.txt` de lot et dans `notes.md`.

| Lot | ISO (SHA256 abrégé) | Hash cohérent | Livrables | Verdict visuel (source) | Budget | Périmètre protégé | Écarts constatés / action |
|---|---|---|---|---|---|---|---|
| v0.8.4-menu-parite | `EC0973A4…1697A802B44` | Oui (`notes.md` = `BUILD_INFO.txt`) | ISO, BIN, CUE, EXE, ELF, MAP, BUILD_INFO, **SHA256 créé** | CONFORME après correction A-08 (`notes.md` + `BUILD_INFO.txt`, synthèse `docs/visuel/ps1_lots_v084_v0811_synthese.md`) | Aucun rapport dédié | Route/gates/boss/identifiants ✓ ; sauvegarde partielle ; PC/data/quête non explicites | `SHA256.txt` absent → **créé et vérifié** (7 hashes) ; budget non régénérable fidèlement ; C-01 encore ouvert et `title.png` vide à ce lot |
| v0.8.5-c01-backdrop | `CDF1B3EE…ADA7E2970B0` | Oui (`SHA256.txt`) | Complet, 6/6 hashes recalculés OK | CONFORME après 2 passes PARTIEL (`notes.md` + synthèse) | Aucun rapport dédié (`budget_v082.md` = v0.8.2) | PC/route/gates/boss/identifiants ✓ ; data/quête/sauvegarde non explicites | Rapport visuel dédié absent → synthèse documentaire créée ; barre cyan et `title.png` vide conservés comme limites |
| v0.8.6-frame-timing | `CC80A322…F08CF7D9D9AD2` | Oui | Complet, 6/6 OK | Aucun nouveau jugement ; 7/7 PNG bit-à-bit identiques à v0.8.5 (synthèse) | Aucun rapport dédié | Route/gates/boss/identifiants ✓ ; « aucun gameplay ni rendu modifié » ; PC/data/quête/sauvegarde non explicites | Synthèse documentaire créée ; outlier de timing conservé ; conversion ms nominale, non mesurée |
| v0.8.7-audio-mix | `4D9FB918…36843BDEED757` | Oui | Complet, 6/6 OK | CONFORME, `docs/visuel/menu_ps1_v0.8.7_audio_mix.md:40` (fond animé masqué du jugement) | Aucun rapport dédié | Route/gates/boss/identifiants/data ✓ ; PC/quête/sauvegarde non explicites | **Ne pas écrire « parité octet »** : 6/7 PNG diffèrent de v0.8.6 ; duck/attaque = choix d'implémentation |
| v0.8.8-ogre-animation | `F0201D03…C9C4742CF476B8` | Oui | Complet, 6/6 OK | CONFORME, `docs/visuel/ogre_ps1_v0.8.8_animation.md:65` | Aucun rapport dédié | Route/gates/boss/identifiants/data ✓ ; quête partielle ; PC/sauvegarde non explicites | Période 12 ticks = décision de design ; deux shots `frame=3` à variantes différentes |
| v0.8.9-ogre-duel | `7B23A346…598AACA70A` | Oui | Complet, 6/6 OK | CONFORME sous limites, `docs/visuel/ogre_ps1_v0.8.9_duel.md:64` | Aucun rapport dédié | Route/gates/boss/identifiants/data/quête/sauvegarde ✓ ; PC non explicite | Harnais de l'époque : hash enregistré non comparé → **corrigé en v0.8.10** (`-ExpectedSha256`) ; budget figé v0.8.2 à cette date |
| v0.8.10-duel-sfx | `EF193607…C4DA9272` | Oui | Complet, 7/7 OK (dont BUILD_INFO) | Aucun nouveau jugement ; 3/3 PNG bit-à-bit identiques à v0.8.9 (synthèse) | Aucun rapport dédié | **Tous affirmés**, y compris PC | Synthèse documentaire créée ; non-régression par hash, pas par nouvelle inspection |
| v0.8.11-hero-vitals | `36EEF86C…3A8BAD92` | Oui | Complet, 7/7 OK | CONFORME, `docs/visuel/ps1_v0.8.11_hero_vitals.md:32` (1re passe NON CONFORME sur ISO intermédiaire, deux défauts clos) | **`docs/ps1/budget_v0811.md`** | Tous ✓ | Aucun écart ; heap/pile non instrumentés ; `/10` explicitement hors périmètre (lot v0.8.12) |
| v0.8.12-hud-max | `073A5FE1…D076D` | Oui (recalculé) | Complet, 6/6 hashes du `SHA256.txt` recalculés OK | CONFORME, `docs/visuel/ps1_hud_max_v0812.md` (inspection réelle) | **`docs/ps1/budget_v0812.md`** | Tous ✓ | Aucun écart bloquant ; une correction de test appliquée après revue (macros HUD) ; mineurs au backlog `notes.md` item 12 |

Légende du périmètre : PC = cible PC inchangée ; data = `data/*` ; Q = quête ; S = sauvegarde ; route A→Z ; gates ; boss/vraie fin ; identifiants `auto_dialogue_*`. Une case « non explicite » signifie que le lot ne l'a pas écrit littéralement, **pas** que le changement a eu lieu. Les lots v0.8.4–v0.8.7 employaient la formulation « identifiants de dialogue testés » plutôt que la chaîne `auto_dialogue_*`.

## Lot v0.8.12-hud-max — détail de référence

- ISO `ps1/a_graver/v0.8.12-hud-max/alundra2_ntsc.iso`, 53 323 776 octets / 26 037 secteurs, SHA256 recalculé `073A5FE1EE20982AC6A0F0C5E804D07D9E98D52DFC60F3FAA4D98E1E200D076D` (identique à `notes.md`, `BUILD_INFO.txt`, `SHA256.txt` et aux pointeurs `ps1/a_graver/DERNIERE_VERSION.txt` / `SHA256.txt`).
- Livrables présents : ISO, EXE, ELF, MAP, BIN, CUE, `BUILD_INFO.txt`, `SHA256.txt`. Les six hashes du `SHA256.txt` ont été recalculés et correspondent aux fichiers réels.
- Compilé/testé hôte : `PS1_BUILD: PASS` (HUD 36/36, vitales 69/69, interaction 144/144, enemy 3033/3033, duel 10128/10128, exportateurs 27/27) ; PC `build.ps1` OK, validateur `ERR: 0 | WARN: 0`, CTest 81/81, `git diff --check` exit 0.
- Exécuté/émulateur : `PS1_SMOKE: PASS` sur l'ISO livrée exacte (`_start=1 main=1 pfInit=1 pfPresent=1 pfDrawRect=752`, boot `cdrom:\alundra2.exe;1`).
- Capturé/observé : `PS1_CAPTURE_VITALS: PASS` sur l'ISO livrée exacte, `SHA256_MATCH=1`, 4 PNG natifs 320×240 dans `qa/ps1_logs/hero_vitals_v0812/` ; `PS1_HERO_VITALS_CAPTURE_PIXELS: PASS`, `PS1_HUD_MAX_CAPTURE_PIXELS: PASS` (slash 45 pixels, chiffres 154 pixels vs baseline v0.8.11).
- Inspecté visuellement : agent `graphismes`, verdict **CONFORME**, `docs/visuel/ps1_hud_max_v0812.md` (4 PNG natifs et 13 crops/superpositions NEAREST réellement vus, comparaison avant/après à même échelle côte à côte et superposition).
- Budget mesuré : `docs/ps1/budget_v0812.md`, RAM 1 715 889 / 2 097 152, pic VRAM 641 508 / 1 048 576, SPU 394 688 / 524 288 octets ; temps d'image non instrumenté.
- Périmètre protégé : affirmé inchangé dans `BUILD_INFO.txt` et `notes.md` ; POD vital 16 octets et POD duel 20 octets inchangés, CROIX jamais appuyée après le menu, Game Over par CARRÉ uniquement sans HUD, aucun nouvel asset importé.
- Revue finale lecture seule (agent `general`, skill `requesting-code-review`) : **aucun problème critique**. Une correction importante a été appliquée après la revue : `tools/ps1_export_zone_test.py::test_pf_constants_match_exporter()` vérifie désormais les six macros HUD de `pf.h` (`PF_HUD_TEX_X/Y/W/H`, `PF_HUD_CLUT_X/Y`), avec `27/27 tests PASS` et contrôle que les anciennes valeurs `48`/`304` ne sont plus présentes. Cette correction ne modifie ni le binaire, ni l'ISO, ni aucun résultat de capture. Les points mineurs/nits relevés sont consignés au backlog `notes.md` item 12 (garde-fou du blob, distinction `/10` vs `/01`, non-chevauchement des frames, commentaires, bornage de `hudBigGemCount`) ; aucun n'affecte le livrable. Plan TDD `docs/superpowers/plans/2026-11-24-ps1-hud-max.md` 21/21 cases cochées, ledger `.superpowers/sdd/2026-11-24-ps1-hud-max/progress.md` complet.

## Actions de la revue finale

1. `ps1/a_graver/v0.8.4-menu-parite/SHA256.txt` créé : sept hashes calculés sur les artefacts réellement présents ; ISO et BIN recoupés avec `BUILD_INFO.txt`. Aucun artefact modifié.
2. `docs/visuel/ps1_lots_v084_v0811_synthese.md` créé : synthèse **documentaire** des verdicts déjà consignés pour v0.8.4, v0.8.5, v0.8.6 et v0.8.10, avec identité bit-à-bit des PNG recalculée par l'audit. Aucune nouvelle inspection visuelle revendiquée.
3. `tools/ps1_export_zone_test.py` durci : six macros HUD de `pf.h` désormais synchronisées avec l'exporteur ; `27/27 tests PASS`. Aucun binaire ni ISO modifié.
4. Chaine canonique relancée après ces changements : `build.ps1` OK (`ninja: no work to do`), `validate_data.py ERR: 0 | WARN: 0`, `ctest` **81/81** (175,70 s), `git diff --check` exit 0 (2 avertissements CRLF préexistants sur `ps1/iso.xml` et `tools/ps1_export_font.py`).

## Décision sur les budgets historiques

Aucun rapport `budget_v084` à `budget_v0810` n'a été fabriqué. `tools/ps1_budget_report.py` lit les réservations VRAM actuelles de `src/platform/pf.h` et les constantes audio actuelles ; l'appliquer à un ELF ancien mélangerait l'état du lot avec l'état courant (HUD 4 frames contre 15, par exemple) et produirait un chiffre trompeur. Les seuls rapports de mesure reproductibles sont `budget_v082.md` (lot v0.8.2), `budget_v0811.md` et `budget_v0812.md`. L'absence est signalée lot par lot, sans estimation de remplacement.

## Non vérifiable dans cette revue

- **Exécution émulateur, builds hôtes et sondes des lots antérieurs** : les preuves sont annoncées dans `notes.md`/`BUILD_INFO.txt` et leurs traces sont présentes, mais aucun build ni lancement PCSX-Redux n'a été rejoué par l'audit. Seuls les fichiers et les hashes ont été vérifiés de façon indépendante.
- **Rapports visuels dédiés de v0.8.4/5/6/10** : aucun fichier historique n'existait ; la synthèse documentaire ne remplace pas une inspection réelle.
- **Budgets v0.8.4 → v0.8.10** : non vérifiables par absence de rapport généré pour ces lots.
- **Console physique, matériel et lecture CD audio subjective** : non vérifiables pour tous les lots ; matériel à fournir par l'utilisateur.

## Limites transversales

- Console physique : non testée pour tous les lots ; matériel (modèle/région, homebrew, manette, carte mémoire) à fournir par l'utilisateur.
- Temps d'image : non instrumenté ; aucune valeur FPS/p50/p95 revendiquée.
- Aucune écoute subjective présentée comme preuve ; seul l'identifiant SPU engagé est observé dans les sondes.
- Arbre non commité : aucun commit ni push n'a été effectué, conformément à `AGENTS.md`.
