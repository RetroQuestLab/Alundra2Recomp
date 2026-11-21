# Tranche 3 — ajouts du remake (absents d'Alundra 2)

Mis à jour le 2026-11-21. Plan et spécification de la tranche 3 archivés localement.

## 1. Ce qui reste du contenu Alundra 2 (inchangé)

- Histoire, personnages, lieux, gates `epee_supreme`, `lampe`, `prisme`, `boss_medusa`, `cristal`, route A → Z, Méphisto, boss du Mont Épervier.
- Les six épées A2 (`long_sword` … `rising_sword`), leurs puissances 1..6 et toutes les autres entrées A2 de `data/items.txt` (ids, types, quantités et puissances inchangés ; seuls les noms et descriptions affichés ont été francisés et nettoyés de leurs notes de travail).
- L'objet clé A2 `hammer` reste distinct du marteau du remake (`rm_rift_hammer`) ; le validateur interdit toute collision.

## 2. Présentation Alundra 1 (réutilisée, pas réinventée)

Police `FONT3.TIM`, fenêtre `WIND.TX`/`WIND.CL` (parchemin à l'échelle 1:1), encre CLUT 6. La page de forge est dessinée sur ce parchemin natif.

## 3. Ajouts du remake

Tous les identifiants commencent par `rm_`. Les noms et descriptions affichés sont dans `data/items.txt` (section « Ajouts du remake »), les statistiques dans `data/rm_*.txt`.

### Armes (`data/rm_weapons.txt`)

| id | Nom | Dégâts | Fenêtre (ticks) | Portée (px) | Recul | EP coup chargé | Effet |
|---|---|---|---|---|---|---|---|
| `rm_ember_blade` | Épée de braise | 3 | 14 | 20 | 2 | 6 | fire |
| `rm_frost_lance` | Lance de givre | 2 | 11 | 28 | 1 | 10 | ice |
| `rm_rift_hammer` | Marteau de faille | 5 | 18 | 16 | 5 | 4 | heavy |

Effets génériques (aucun code par arme) :
- **fire** : éclaboussure, 1 dégât aux ennemis voisins de la cible ; allume le brasero.
- **ice** : ralentit l'ennemi touché (90 ticks, 150 en zone) ; gèle la source.
- **heavy** : tremblement, poussée, brise la garde frontale des tortues ; actionne tout objet de l'énigme.
- **Coup chargé** avec une arme du remake : si l'énergie suffit (coût EP), onde élémentaire de 40 px qui touche les ennemis et les objets de l'énigme.

### Matériaux (`data/rm_materials.txt`)

`rm_ember_shard` (Éclat de braise), `rm_frost_core` (Cœur de givre), `rm_rift_fragment` (Fragment de faille), `rm_ironwood` (Bois de fer), `rm_rune_dust` (Poussière runique). Type `Key` : jamais consommés par le menu, seulement par la forge. Coffres « objet:n » possibles.

### Forge d'Orvan (`data/rm_forge.txt`)

Orvan est un personnage **original** du remake (Paco, case 36,47). Première rencontre : il confie le Marteau de faille. Ensuite, sa réplique ouvre la page de forge.

| Recette | Arme | Coût | Après | Contrepartie |
|---|---|---|---|---|
| `rm_ember_blade_flame_1` Lame ardente | Épée de braise | 2 éclats de braise, 1 bois de fer | dégâts 5, portée 16 | portée réduite |
| `rm_ember_blade_ember_1` Braise vive | Épée de braise | 1 éclat de braise, 1 poussière runique | dégâts 4, EP 12 | coût EP accru |
| `rm_frost_lance_glacier_1` Lance glaciaire | Lance de givre | 2 cœurs de givre, 1 bois de fer | dégâts 4, EP 14 | coût EP accru |
| `rm_frost_lance_shard_1` Pointe d'éclat | Lance de givre | 1 cœur de givre, 1 poussière runique | portée 34, recul 4 | recul accru |
| `rm_rift_hammer_quake_1` Marteau sismique | Marteau de faille | 2 fragments de faille, 1 bois de fer | dégâts 7, fenêtre 20 | vitesse réduite |
| `rm_rift_hammer_shatter_1` Marteau brise-roc | Marteau de faille | 1 fragment de faille, 1 poussière runique | dégâts 6, portée 12 | portée réduite |

Règles : deux appuis (préparer puis forger) ; transaction atomique (`confirmForge` vérifie tout avant de modifier quoi que ce soit) ; une seule branche par arme (l'autre se ferme) ; double validation sans effet ; pas de durabilité, pas de destruction d'arme, pas d'économie d'argent.

### Énigme à solutions multiples (`data/rm_interactions.txt`, zone 011, rive sud de Paco)

| Objet | Case | Solutions |
|---|---|---|
| Brasero (`rm_demo_brazier`) | 23,50 | fire, heavy |
| Chemin gelé (`rm_demo_ice_path`) | 25,50 | ice, heavy |
| Pilier fêlé (`rm_demo_pillar`) | 27,50 | heavy |
| Vieux mécanisme (`rm_demo_gate`) | 30,52 | s'ouvre à 2 objets résolus sur 3 |
| Levier (`rm_demo_reset`) | 29,52 | remet les 3 objets à zéro (la cache reste ouverte) |

L'ouverture déclenche l'événement `rm_demo_gate`, qui fait apparaître la cache (33,52 : Épée de braise ; 34,52 : 2 éclats de braise). Solutions possibles : marteau seul (deux objets quelconques), épée de braise + lance de givre, marteau + l'une des deux autres armes. Aucun gate historique n'est touché (test `auto_rm_no_gate_change`).

### Sauvegarde V4

Nouvelles lignes `ameliore <recette>` et `interaction <id> 1`. Les sauvegardes V2/V3 se chargent sans rien inventer (test `auto_rm_save_v3`) ; V4 se relit et se réécrit à l'identique (`auto_rm_save_v4`) ; une version future est refusée (`auto_rm_save_future`).

## 4. Limites de cette première tranche

- Un rang par branche, une énigme, trois armes, cinq matériaux, six recettes.
- Objets de l'énigme : sprites originaux en cours de création (`graphismes`) ; en attendant, repères provisoires (petits carrés colorés) — ce ne sont pas des assets finaux.
- Le menu armes/objets/équipement n'a pas encore sa mise en page A1 native (travail suivant) ; la page de forge utilise déjà le parchemin natif.
- L'EP ne se régénère pas : le coup chargé élémentaire dépend des Noix de Lorien / Raisins d'Eru.

## 5. Tests

`ctest --test-dir build -R auto_rm_ --output-on-failure` (22 scénarios) et `py -3.12 tools\validate_data.py` (ids, références, cases marchables, absence de collision et de gate touché).
