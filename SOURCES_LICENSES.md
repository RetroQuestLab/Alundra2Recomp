# Origines et licences

## Code et documentation du dépôt

L’ensemble du code source et de la documentation de ce dépôt est publié sous **GPL-3.0**. Voir `LICENSE`.

## Ressources non incluses

Les éléments suivants ne sont pas distribués dans ce dépôt :

- sprites, cartes, polices, fenêtres de dialogue, musiques et effets provenant des jeux Alundra ;
- images de référence, vidéos et captures de travail ;
- blobs PS1 générés à partir de ces ressources ;
- exécutables, bibliothèques, BIOS, images BIN/CUE et ISO.

Ces fichiers doivent être fournis localement par chaque personne disposant des droits nécessaires. Ils restent la propriété de leurs ayants droit respectifs.

## Dépendances externes

| Composant | Usage | Licence |
|---|---|---|
| SDL3 3.4.20 | Backend PC | zlib |
| PSn00bSDK 0.24 | Chaîne d’outils PS1 | Voir la licence du SDK |
| OpenBIOS | BIOS open-source pour émulation | Voir la licence du projet |

Aucune de ces dépendances n’est copiée dans le dépôt.

## Génération locale

Le blob de police PS1 et les blobs de zone sont générés par les scripts présents dans `tools/`, à partir de ressources locales non versionnées. Les commandes de génération sont documentées dans `README.md` et `CONTRIBUTING.md`.
