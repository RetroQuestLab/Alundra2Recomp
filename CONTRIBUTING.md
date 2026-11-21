# Contribuer

Merci de contribuer à Alundra2Recomp.

## Règles générales

- Le cœur du jeu est du C++20 sans moteur externe.
- La cible PC SDL3 reste la référence de non-régression.
- La cible PS1 utilise PSn00bSDK et un sous-ensemble C++ sans bibliothèque standard lourde.
- Aucune ressource commerciale n’est acceptée dans le dépôt.
- Aucun fichier généré, binaire, image disque ou capture de travail n’est versionné.

## Avant chaque modification

Créer une branche dédiée, puis exécuter depuis la racine :

```powershell
powershell -ExecutionPolicy Bypass -File build.ps1
py -3.12 tools\validate_data.py
ctest --test-dir build --output-on-failure
git diff --check
```

Pour une modification PS1, ajouter :

```powershell
py -3.12 tools\ps1_export_zone_test.py
powershell -ExecutionPolicy Bypass -File tools\build_ps1.ps1
powershell -ExecutionPolicy Bypass -File tools\ps1_smoke.ps1 -Seconds 300
```

## Tests

- Les scénarios `--auto` sont déterministes : ils doivent être préférés à une vérification subjective.
- Tout changement de gameplay doit conserver ou ajouter un scénario déterministe.
- Les tests de conversion PS1 verrouillent les formats de données et la disposition VRAM.

## Commits

- Un commit = une modification cohérente.
- Ne pas inclure `assets/`, `extracted/`, `gp/`, `images/`, `external/`, `build/`, `*.bin`, `*.cue`, `*.iso`, `*.mp4`.
- Ne pas inclure d’adresse email, d’information personnelle ou de chemin local absolu dans les fichiers versionnés.
