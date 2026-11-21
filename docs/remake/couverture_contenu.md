# Couverture du contenu narratif d'Alundra 2 dans le remake

Audit du 2026-11-21 (aucun code modifié). Document de planification : il compare ce que le texte A2 extrait localement atteste avec ce que le remake contient réellement, puis propose un backlog.

## 0. Sources, conventions, limites de lecture

- **Texte A2 de référence** : `extracted/a2_text/dialogues.txt` (262 blocs, 553 messages, extraction heuristique de `GRP_FR.GP`, jamais publiée). Les blocs sont cités `bloc N` ; **aucune réplique n'est recopiée**, tout est résumé avec mes mots. `extracted/a2_text/menus.txt` (listes de noms, textes systèmes) est cité `menus bloc N`.
- **Contenu du remake** lu directement : `data/world.txt`, `data/warps.txt`, `data/dialogues.txt` (131 identifiants dont 20 `rm_*`/`objet_rm_*`), `data/items.txt`, `src/title.cpp` (prologue), `CMakeLists.txt` (tests `auto_*`, gates).
- **Ajouts du remake à ne pas compter comme contenu A2** (`docs/remake/ajouts_tranche3.md`) : forgeron Orvan, forge, trois armes `rm_*`, cinq matériaux `rm_*`, énigme du brasero/pilier/mécanisme, coffres de matériaux (Paco, Kindra, épave). Ils sont listés en annexe et exclus de tous les décomptes.
- **Numéros de bloc ≠ identifiants de carte du remake.** Un bloc de `GRP_FR.GP` correspond à une carte A2 (indice dans la table d'offsets), alors que les zones du remake portent des numéros de cartes **A1** (`011`, `013`, `019`, `119`, `026`, `042`←`028`, `050`←`029`, `086`←`316`, `120`). La correspondance bloc → lieu repose uniquement sur les titres entre crochets présents dans certains blocs (ex. nom de salle) et sur les renvois entre répliques. Beaucoup de blocs n'ont pas de titre : leur lieu est inféré.
- **L'ordre de l'histoire est reconstitué** à partir d'indices internes (renvois d'un bloc à l'autre, objets échangés, consignes « va à ... ») ; il n'est pas lu dans un script d'événements, car l'extraction ne contient ni conditions, ni drapeaux, ni noms de locuteurs. Toute ligne marquée « ordre incertain » doit être confirmée par une source indépendante (vidéo, wiki A2).
- **Statuts** (par lieu) :
  - **PRÉSENT** : une zone jouable du remake reprend l'essentiel des événements, PNJ et objets attestés du lieu ;
  - **PARTIEL** : une zone, une scène ou des PNJ existent mais une partie notable de ce qui est attesté manque, est déplacée ou est contredite ;
  - **ABSENT** : ni zone, ni scène (au plus quelques PNJ qui y font allusion ailleurs).
- Les cartes A1 décors sont disponibles (483 fichiers `assets/maps/*.a1m`) ; le choix d'une carte adaptée à un lieu A2 reste à faire à chaque fois (précédents : Eden, Puerto Medusa, manoir, où la carte d'origine était noire ou trop sombre et a été remplacée).

## 1. Chapitres et lieux d'Alundra 2 dans l'ordre de l'histoire

Remake : 9 zones jouables (+ prologue narratif). Aucune n'est « PRÉSENT » : chacune est un décor A1 réutilisé pour un lieu A2, avec une trame **compressée et partiellement recomposée** (voir § 3 « divergences de trame »).

Légende des colonnes : **Blocs** = blocs du texte extrait qui attestent le lieu ; **Remake** = zone `world.txt` ou scène ; **PNJ / dialogues présents** = identifiants de `data/dialogues.txt` ; **Objets / gates / boss** = ce qui existe dans `world.txt`, `warps.txt`, `items.txt`.

### Acte I — Prologue, Paco, forêt, épave

| # | Lieu A2 | Blocs | Remake | PNJ / dialogues présents | Objets / gates / boss | Statut | Ce qui manque concrètement |
|---|---|---|---|---|---|---|---|
| 1 | **Navire volant des pirates** (cabine, salle de bain, magasin, ponts, cockpit, salle des machines, entrepôt, quartiers du capitaine, salle de contrôle de sécurité, zone de soin) | 1-10, 13, 14, 16, 18, 34 | Prologue narratif `src/title.cpp` + dialogue `[prologue]` (cartes 042, 018, « navire », « chute », 026) | `prologue`, `navire_milena` (une réplique de Milena à Zeppo, hors contexte) | aucun ; pas de combat | **PARTIEL** | Aucune zone jouable. Combat d'ouverture contre Zeppo (bloc 1) remplacé par une cinématique ; androïdes « Gi Gi » aux commandes (blocs 1, 14, 16) seulement évoqués ; salle de contrôle qui déverrouille les portes (blocs 1, 34) ; zone de soin 100 % PA/PE (bloc 13) ; clé mystérieuse trouvée à bord (blocs 3, 7) = début de la chaîne d'échange. |
| 2 | **Village Paco** (maisons, boutique d'articles, maison du chef, salle de bain, statue de Jeehan) | 19-24, 29, 31, 32 | Zone `011` « Village Paco » | `prunewell`, `prunewell_apres`, `garde`, `garde_apres`, `villageoise`, `villageoise_apres`, `enfants_paco` (+ variantes `_objet_*`) ; ajout `rm_forgeron` | coffre `herbe` ; sortie vers Kindra (non gatée) | **PARTIEL** | Scène du réveil avec Alexia et **prime sur Flint** (bloc 22 : livrer Flint ou annuler la prime) ; **maison du chef** (bloc 22) ; boutique d'articles (blocs 21, 22) ; intérieurs avec habitants, enfants, taureau Evo (blocs 20, 24) ; statue de Jeehan à prier (blocs 19, 23, 24 : menu Prier/Quitter) ; garde qui conseille de s'équiper d'une épée et d'un bouclier (bloc 19) ; marchand qui cherche sa tirelire (bloc 22). |
| 3 | **Forêt Kindra** + **Maison de Finrod** | 29-33 (30 : puzzle de blocs ; 32 : Finrod/Prunewell ; 33 : marchand) | Zone `013` « Forêt Kindra » | `finrod`, `finrod_apres`, `finrod_objet_lettre` | coffres `coeur`, `lettre` (gardé par l'ogre) ; monstres gelée/tortue/gobelin/ogre ; passes internes | **PARTIEL** | Retrouvailles Prunewell-Finrod (bloc 32) ; marchand en sous-vêtements qui échange une boussole contre une herbe (bloc 33, choix) ; puzzle de blocs de bois et de blocs violets à pousser (bloc 30) ; pièce de puzzle dans un coffre (bloc 11) ; Alexia qui rattrape Flint (blocs 30, 31) ; dans A2 la lettre de Diaz se trouve **dans l'épave** (bloc 8), pas en forêt (divergence, § 3). |
| 4 | **Mont Épervier / épave** (dirigeable écrasé) | 5, 8, 11, 31 | Zone `019` « Mont Épervier - Epave » | `epave_arrivee`, `epave_arrivee_2`, `alexia_epave` (+ variantes), `belgar_epave` | coffre `gantelet` ; 2 droïdes + gobelin ; sortie vers Toroledo (non gatée) | **PARTIEL** | Exploration à deux avec Alexia et révélation que les androïdes étaient des humains transformés (bloc 5) ; **lettre de Diaz à Milena** trouvée ici (bloc 8) ; **combat contre le chat de Méphisto transformé en monstre par une clé** (bloc 11, rappelé bloc 304) : le remake met un ogre en forêt (décision de route à préserver, § 2b) ; clé mystérieuse (blocs 3, 7) ; Belgar n'est pas attesté ici (le remake l'y place, § 3) ; le gantelet vient de l'apprenti de Jeehan (bloc 219), pas de l'épave. |
| 5 | **Paco bombardé** (ruines du village, arche détruite) | 23, 24, 235-237, 304, 310 | absent (Paco reste intact toute la route) | seulement `medusa_arrivee` qui y fait allusion | aucun | **ABSENT** | Événement : un taureau mécanique bombarde Paco sous l'emblème des pirates (blocs 23, 24, 304, 310) ; Alexia et Flint reviennent sur les décombres (bloc 23) ; villageois sinistrés, boutique détruite, échange du marteau contre la statue rouge (bloc 23, chaîne d'échange) ; ruines où les Enfants de la Couronne ont trouvé et caché l'idole (blocs 231, 237). Nécessite une variante « ruines » de la carte 011. |

### Acte II — Toroledo, Pierre, Vallée de Mithra, Fort Noir, Mont Vulcain (ordre partiellement incertain)

| # | Lieu A2 | Blocs | Remake | PNJ / dialogues présents | Objets / gates / boss | Statut | Ce qui manque concrètement |
|---|---|---|---|---|---|---|---|
| 6 | **Toroledo** (arène des corridas, auberge + réserve + étage, boutiques, grange, salle d'attente des taureaux, entrepôt, maison de Jeehan, maisons) | 41, 42, 49-64, 219 | Zone `119` « Toroledo » | `garde_toroledo`, `alexia_toroledo`, `jeehan`, `jeehan_apres`, `jeehan_epee_supreme`, `dame_toroledo`, `toroledo_natasha`, `enfants_toroledo`, `prisonnier_toroledo`, `nunugi_gwaba`, `berger_toroledo` (+ variantes `_objet_ordre`) | coffre `ordre` ; gate `epee_supreme` vers la Vallée ; 2 droïdes | **PARTIEL** | Arène, paris, billet d'entrée (blocs 49-55) ; auberge (10 O la nuit) et boutiques (blocs 54, 55) ; **Maison de Jeehan** : maître enfermé par un sort, 4 puzzles à terminer avec des pièces de puzzle, enseignement d'enchaînements puis de l'attaque Éclair, remise du gantelet (bloc 219) ; le remake donne à la place l'épée suprême contre des « ordres du baron » (§ 3) ; **Pierre le joueur, sa femme Emma et sa fille Luca** (blocs 49, 50, 59, 291) ; Miss Vachette (blocs 53, 55, 416) ; collectionneur qui troque la clé mystérieuse contre une vache sculptée (bloc 54) ; taureaux Lao et Evo (blocs 58, 61-64) ; accès à la Vallée de Mithra **par le jardin de la maison de Pierre** (blocs 50, 59, 62, 64). |
| 7 | **Maison de Pierre / Grotte Hurlante / Maison du Bonheur** | 49, 50, 59, 291, 292 | absent | — | — | **ABSENT** | Quête de rapatriement de Pierre pour Emma et Luca (blocs 49, 50, 59, 291) ; la « Maison du Bonheur », piège qui attire les joueurs sous Fort Noir (blocs 25, 37, 38) ; Grotte Hurlante (blocs 291, 292). |
| 8 | **Fort Noir** (+ Port Yagin, puits de mine, passages souterrains, ascenseurs en panne, chariots de minerai) ; Île Noire ? | 25-28, 37-39, 45, 48, 269, 294, 299-302, 305, 307, 310-312, 319 | absent (allusions seulement) | `zach_fort`, `medusa_minerai` (placés à Puerto Medusa) | aucun | **ABSENT** | Antagoniste au rire reconnaissable et ses serviteurs qui piègent Flint (blocs 25, 28, 37) ; minerai transporté en chariots de la mine vers Puerto Medusa (blocs 294, 302, 305) ; infiltration d'un navire pirate (bloc 294) ; marchand rescapé de la baleine sur l'Île Noire (bloc 269 ; identité Île Noire / Fort Noir incertaine). |
| 9 | **Vallée de Mithra** (énorme véhicule-taureau « ToroTank », base souterraine) | 304, 328, 332-334 | Zone `026` « Vallée de Mithra » | `vallee_arrivee`, `guide_vallee`, `guide_vallee_objet_lampe`, `boss_026`, `objet_lampe`, `objet_idole` | gate `lampe` (coffre derrière le golem `boss_026`) ; coffre `idole` ; golem, gelée, tortue, gobelin | **PARTIEL** | Le lieu attesté : Zeppo, Ruby et Albert voient puis explorent un engin gigantesque (bloc 328) ; combat contre le monstre-taureau, qui redevient taureau normal (bloc 304) ; base secrète de jeunes gens qui connaissent Flint (bloc 333, non titré, rattachement inféré) ; scène où Diaz et Méphisto font placer les reliques sur un mur (blocs 334, 365, non titrés, lieu inféré : voir ligne 20) ; marchand d'une grotte souterraine (bloc 332, titré « Grotte souterraine », rattachement inféré). Le golem gardien et la Lampe comme récompense ne sont **pas** attestés (inventions du remake). |
| 10 | **Mont Vulcain / Rivière souterraine / Dragonia / Prison du Dragon** | 220, 222, 227, 229, 231, 272, 274, 282, 283, 287 | absent (allusion `objet_idole`) | — | coffre `idole` placé en Vallée de Mithra | **ABSENT** | Passage secret des androïdes à l'est du Mont Épervier, bague de triton pour marcher sur la lave (blocs 222, 229) ; cachette de l'idole à trois yeux (blocs 231, 237) ; le dragon Tirion prisonnier et son remède (bloc 282) ; **Croc rouge, le Gardien** (bloc 282) ; plan d'enfoncer une clé dans le dragon (bloc 274) ; toit de Dragonia (bloc 283). |

### Acte III — Puerto Medusa, manoir de Milena, palais, Gwaba (ordre reconstitué : infiltration, enlèvement d'Alexia, rançon des reliques)

| # | Lieu A2 | Blocs | Remake | PNJ / dialogues présents | Objets / gates / boss | Statut | Ce qui manque concrètement |
|---|---|---|---|---|---|---|---|
| 11 | **Puerto Medusa** (île-base des pirates : port, restaurant avec spectacle, auberge, boutique, entrepôt, usine navale, phare, centre de test des badges d'or, chemin du manoir) | 90, 99-109, 112, 120, 122-126, 129-133, 206-211, 253, 255-259, 261-266, 290, 297 | Zone `050` « Puerto Medusa » (carte `029`) | `medusa_arrivee`, `marin_puerto`, `medusa_baleine`, `zach_fort`, `medusa_minerai`, `ruby_puerto`, `albert_puerto`, `diseuse_puerto`, `zeppo_puerto`, `georges_puerto` (+ variantes `_objet_cristal`) | gate `boss_medusa` (le capitaine, boss du remake) vers le manoir ; coffre `cristal` ; capitaine, droïde, gobelin | **PARTIEL** | **Système des badges d'or** (course d'obstacles vers le manoir, barrière réservée aux détenteurs : blocs 107, 123, 126, 132) à la place du capitaine ; déguisements : Alexia chante sous le nom d'Axelya et Flint se fait passer pour son petit frère Georges (blocs 99, 262, 263) ; auberge (bloc 103), diseuse de bonne aventure (bloc 103), forgeron du bouclier Valar (bloc 102, lieu inféré), phare et chaîne d'informateurs (blocs 106, 261) ; arrivée de Méphisto et de ses prêtres, Alexia emmenée (blocs 264-266) ; mini-jeux « Faites la bombe », « Attaque du chien », circuit RC, vases, pierre-papier-ciseaux (blocs 90, 109, 112, 130, 131 ; lieu de certains inféré) ; porte à mot de passe devinée par Alexia (bloc 178, rattachement incertain). Le Cristal d'Étoile est trouvé **dans la baleine** (bloc 433), pas ici. |
| 12 | **Manoir de Lady Milena** (entrée, jardin, hall, annexe, cuisine, dressing + toit, salle des punitions, sous-sol, chemin secret) | 120, 135-137, 240-251, 253-259, 264, 267, 288 | Zone `086` « Manoir de Milena » (carte `316`) | `manoir_arrivee`, `garde_manoir`, `milena`, `belgar_manoir`, `natasha_manoir`, `diaz_manoir`, `alexia_manoir`, `gwaba_habitant` (+ variantes `_objet_cristal`, `_boss_medusa`) | gate `cristal` vers le trône ; coffre `coeur` ; droïde, gobelin | **PARTIEL** | Infiltration furtive par la cheminée et le dressing (bloc 251, indices de la fan Kate) ; salle des punitions et file d'attente comique (blocs 243-250) ; **confrontation Milena - Méphisto** devant Belgar (bloc 242) ; remise de la cape rouge à Belgar contre l'épée (bloc 242) ; couple Milena-Belgar et passé de Belgar avec Flint (blocs 242, 267, 288) ; alerte rouge et visiteur annoncé (blocs 136, 137, 243) ; Diaz et Natasha ne sont **pas** attestés ici (palais, bloc 417). |
| 13 | **Palais royal de Varuna** (cellules, salle du trône) | 414-419 | Zone `120` « Salle du trône » | `mephisto_arrivee`, `mephisto_trone`, `alexia_finale`, `roi_trone` (+ variantes `_mephisto_vaincu`) ; `enfants_paco_mephisto_vaincu`, `belgar_manoir_mephisto_vaincu` | boss Méphisto + droïde ; vraie victoire testée par `auto_boss_victory` | **PARTIEL** | Répétition de mariage forcée avec Alexia otage, négociation d'échange des reliques avec **choix** (bloc 414) ; Flint emprisonné, Natasha qui le fait évader (blocs 415-417) ; Prunewell et jeunes Enfants en cellule (bloc 415) ; accord provisoire de Diaz et demande de l'idole (bloc 419). L'affrontement final avec Méphisto **n'est pas attesté** par l'extraction (voir § 5) : c'est une reconstitution du remake. |
| 14 | **Gwaba** (mairie à ascenseur, auberge, boutique, maisons de Jessica/Po/Sue/José/Linda/du maire, maison d'Oméga, maison de Roswell, port) | 179-196, 198, 200-207 | absent | `nunugi_gwaba` (à Toroledo), `gwaba_habitant` (au manoir) | aucun | **ABSENT** | Toute la ville : enquête sur l'homme à la main-crochet menée par Ruby et Albert (blocs 179-194, 200, 201) ; fausse piste de l'auberge (bloc 183) ; Piaf Doudou et cape rouge (bloc 187) ; marchand qui vend une pièce de puzzle contre tout l'argent (bloc 186) ; **maison d'Oméga** et questionnaire de la « femme idéale » (blocs 195-198) ; **maison de Roswell** : remise des ordres du baron, rappel des trois reliques, **bague de la Sirène** (blocs 202, 205) ; départ en navire (blocs 206, 207). |
| 15 | **Église de la Clé / sous-sol / passage souterrain / digue de la clé** | 203, 204, 463, 472, 473, 475-477, 479 | absent (allusions : `roswell`, `mephisto_trone`) | — | — | **ABSENT** | Secte vouée à la Clé (bloc 204) ; passage secret par lequel elle livre le minerai (bloc 204) ; intérieur de la digue (blocs 473, 475, 476) ; tour des observateurs face à la digue, atteignable par l'ascenseur (blocs 477, 479). |

### Acte IV — Les reliques : ruines des mouettes, Eden, baleine, chambre des reliques

| # | Lieu A2 | Blocs | Remake | PNJ / dialogues présents | Objets / gates / boss | Statut | Ce qui manque concrètement |
|---|---|---|---|---|---|---|---|
| 16 | **Ruines des mouettes** + épave sous-marine (clé des ruines) | 69, 73-79, 126, 139, 140, 289, 374 | absent | — | — | **ABSENT** | Plongée depuis le navire de Zeppo avec la bague de Sirène (blocs 139, 140) ; clé des ruines des mouettes récupérée dans l'épave (blocs 139, 140, 374) ; entrée des ruines et pièges (sol glissant, piques : blocs 73-79, 289). |
| 17 | **Île d'Eden** (village, puits, cottage, capture du Piaf Doudou) | 404-406 | absent (la zone `042` ne montre que les ruines) | `ome`, `roswell` (placés dans les ruines) | — | **ABSENT** | Jeu de capture du Piaf Doudou contre la bague de Dryade (bloc 404) ; clé des ruines d'Eden dans un coffre derrière une statue jaune à détruire avec la Dryade (bloc 404) ; villageois, puits dangereux, auberge (blocs 404-406). |
| 18 | **Ruines d'Eden** (portes à clé ronde et triangulaire, leviers) | 160, 165, 170, 178, 208, 354 | Zone `042` « Ruines d'Eden » (carte `028`) | `eden_arrivee`, `roswell`, `roswell_objet_*`, `ome`, `ome_objet_idole`, `boss_042`, `objet_prisme` | gate `prisme` vers Puerto Medusa ; coffre `prisme` (golem `boss_042`) ; golem, droïde, gobelin | **PARTIEL** | Portes qui demandent une clé ronde puis une clé triangulaire (bloc 208) ; levier à actionner autrement qu'à l'épée (bloc 170) ; déverrouillage par clés et leviers seulement attesté par fragments (blocs 165, 170, rattachement inféré par proximité de numéro) ; pose du Prisme d'Étoile (bloc 354) ; Roswell et Oméga sont attestés à **Gwaba**, pas ici. Le golem gardien n'est pas attesté. |
| 19 | **Baleine démon** (bouche, tube d'air, zones du cerveau, des poumons, du cœur, du tube d'alimentation, de l'estomac, des intestins, salle de contrôle) | 423-434, 437-439, 441, 443, 449-452, 454, 460-462, 481 | absent (une seule scène de PNJ : `medusa_baleine`) | `medusa_baleine`, `ruby_puerto`, `albert_puerto` (récit de leur passage) | — | **ABSENT** | Donjon complet : séparation de Flint, Ruby, Albert, Zeppo (blocs 433, 443) ; énigmes de clés colorées et de ponts de particules (blocs 433, 449-451) ; plongée du monstre et alertes d'intrusion (bloc 426) ; Cristal d'Étoile trouvé ici (bloc 433) ; fluides gastriques et sortie par la bouche (blocs 443, 461, 462). |
| 20 | **Chambre des reliques** (mur aux emplacements, tour vieille de plus de deux mille ans ; lieu non titré) | 333-334, 354, 364, 365, 371 | absent (remplacée par l'escalier du manoir et le trône) | `roswell`, `ome`, `mephisto_trone` (explications) | gates `lampe`, `prisme`, `cristal` portent le rôle des reliques | **ABSENT** | Pose successive des reliques (idole, prisme, cristal : blocs 334, 354, 364) ; Méphisto révèle qu'il a bâti la tour pour sa clé d'étoile et que Diaz l'a activée sans le savoir (bloc 334) ; lecture du Livre des Rois et des « instructions pour desceller les trésors » (bloc 365). La composition exacte du trio de reliques est **ambiguë** (§ 5). |

### Acte V — Digressions et fin d'aventure

| # | Lieu A2 | Blocs | Remake | PNJ / dialogues présents | Objets / gates / boss | Statut | Ce qui manque concrètement |
|---|---|---|---|---|---|---|---|
| 21 | **Île de Gamar** (salle de jeu à médailles, bar, aire de jeu) | 290, 407-409, 486-488 | absent | — | — | **ABSENT** | Achat de médailles (5 O pièce), mini-jeux « Tireur », « Rallye », « La Course » (blocs 408, 486-488). Pur contenu optionnel. |
| 22 | **QG des Enfants de la Couronne** (navire ou dirigeable ? lieu non titré) | 388 (+ 415, 477, 479) | absent (seulement `enfants_paco`, `enfants_toroledo`, `prunewell_couronne`) | messager générique | — | **ABSENT** | Scène où les Enfants apprennent l'enlèvement d'Alexia et décident de la libérer ; serment de défendre la Couronne ; cap sur le Mont Vulcain (bloc 388). |

**Décompte des 22 lieux** : PRÉSENT 0 · PARTIEL 10 (lignes 1, 2, 3, 4, 6, 9, 11, 12, 13, 18) · ABSENT 12 (lignes 5, 7, 8, 10, 14, 15, 16, 17, 19, 20, 21, 22).
Les 9 zones jouables du remake sont toutes des lieux PARTIELS : `011`→2, `013`→3, `019`→4, `119`→6, `026`→9, `042`→18, `050`→11, `086`→12, `120`→13. Le prologue narratif couvre la ligne 1.

## 2. Personnages et boss

### 2a. Personnages

Liste de référence : `menus bloc 15/17` donne quatorze noms de portraits/locuteurs d'A2 (Alexia, Ruby, Zeppo, Albert, Milena, Belgar, Diaz, Méphisto, Oméga, Prunewell, Natasha, le 139e roi de Varuna, Nunugi, Axelya). Les autres personnages viennent des blocs de dialogues.

Sprites du remake (`assets/sprites`) : **nommés** (un fichier par personnage ; leur origine graphique n'a pas été vérifiée ici) pour Flint, Alexia, Jeehan, Milena, Méphisto, Ruby, Albert, Natasha, Belgar, Diaz, Roi, Nunugi, Zach ; **génériques réutilisés d'A1** pour tout le reste (`villageois`, `villageoise`, `ancien`, `capitaine`, `droide`, monstres). Zeppo, Pierre, Prunewell, Finrod, Roswell, Oméga empruntent un sprite générique.

| Personnage | Attesté (blocs) | Dans le remake (sprite · dialogues · zone) | Statut | Manquant / divergent |
|---|---|---|---|---|
| Flint | tous | sprites héros · `intro`, `victoire`, répliques de héros · toutes zones | PRÉSENT | portrait d'A1 absent (buste du sprite A2 utilisé, décision documentée) |
| Gi Gi (androïdes, humains transformés par une clé) | 1, 5, 14, 16, 305, 310 | sprite `droide` · quelques lignes (`medusa_minerai`, `zeppo_puerto`) · spawns 019, 119, 042, 050, 086, 120 | PRÉSENT (en tant qu'ennemi) | les Gi Gi « civils » qui parlent par onomatopées (blocs 59, 305, 310) |
| Alexia | 5, 8, 11, 22, 30, 31, 41-42, 48-50, 263-265, 288, 414 | sprite nommé · `alexia_epave`, `alexia_toroledo`, `alexia_manoir`, `alexia_finale` · zones 019, 119, 086, 120 | PARTIEL | rôle de **compagnon qui suit Flint** (blocs 11, 29, 31, 41, 42, 49), scène du réveil à Paco et prime (bloc 22), déguisement en Axelya (bloc 263), captivité chez Diaz (blocs 264, 414) |
| Prunewell | 22, 32, 415 | sprite générique `ancien` · `prunewell`, `prunewell_apres`, `prunewell_couronne` · zone 011 | PARTIEL | scène avec Finrod (bloc 32), emprisonnement (bloc 415) |
| Finrod | 19, 31, 32 | `ancien` · `finrod*` · zone 013 | PARTIEL | Maison de Finrod comme intérieur ; ses retrouvailles avec Prunewell |
| Zeppo | 1, 14, 139, 140, 180, 289, 290, 328, 334, 419, 425 | sprite `capitaine` (substitut) · `zeppo_puerto`, `navire_milena`, prologue · zone 050 | PARTIEL | combat d'ouverture, navire qu'il pilote vers Gwaba/Puerto/Gamar (blocs 206, 290) |
| Ruby | 1, 14, 139, 180, 183, 209, 289, 334, 433 | sprite nommé · `ruby_puerto` · zone 050 | PARTIEL | enquête à Gwaba, hostage chez Diaz (bloc 334), baleine |
| Albert | mêmes blocs que Ruby | sprite nommé · `albert_puerto` · zone 050 | PARTIEL | idem ; gaffes et fausse piste à l'auberge (bloc 183) |
| Milena | 14, 242, 267 | sprite nommé · `milena`, `milena_objet_*`, `milena_boss_medusa` · zone 086 | PARTIEL | confrontation avec Méphisto (bloc 242), couple avec Belgar (bloc 267) ; le remake lui fait dire qu'elle se détache de Diaz au profit de Méphisto (non attesté) |
| Belgar | 242, 264, 267, 288 (+ allusions 123, 297) | sprite nommé · `belgar_epave`, `belgar_manoir` · zones 019, 086 | PARTIEL | échange cape rouge / épée suprême (bloc 242), lien avec Flint (§ 5), présence à l'épave non attestée |
| Diaz | 8 (lettre), 334, 365, 414, 419 | sprite nommé · `diaz_manoir` · zone 086 | PARTIEL | apparaît au palais, pas au manoir ; mariage forcé, marché des reliques |
| Méphisto | 242, 264-266, 288, 334, 365 (+ chat 11, 304) | sprite nommé · `mephisto_*` · zone 120 (boss) | PARTIEL | son rôle de bâtisseur de la tour et de sorcier de plus de deux millénaires (bloc 334) ; sa fuite (blocs 242, 484) |
| Natasha | 417, 419 | sprite nommé · `toroledo_natasha`, `natasha_manoir` · zones 119, 086 | PARTIEL | sa vraie scène (palais, évasion de Flint) manque ; elle est placée ailleurs |
| Nunugi | 202 (ordres), 414, 419, 365 | sprite nommé · `nunugi_gwaba*` · zone 119 | PARTIEL | scène de remise des reliques (bloc 414) ; est à Toroledo au lieu de Gwaba |
| Oméga | 48, 103, 198, 219 | sprite générique `villageoise` · `ome`, `ome_objet_idole` · zone 042 | PARTIEL | maison de prophétie et questionnaire (bloc 198) ; prophétie qui annonce Flint (blocs 48, 219) |
| Roswell le Sage | 139, 202, 205 | sprite générique `ancien` · `roswell*` · zone 042 | PARTIEL | sa maison sous le pont de Gwaba, bague de la Sirène (bloc 205) |
| Jeehan | 19, 23, 24, 159, 203, 219, 289 | sprite nommé · `jeehan*` · zone 119 | PARTIEL | enfermé par un sort et délivré par les pièces de puzzle (bloc 219) ; statue vénérée dans plusieurs villes |
| Roi (139e roi de Varuna) | menus 15/17, 22, 188, 415 | sprite nommé · `roi_trone*` · zone 120 | PARTIEL | disparu / emprisonné par Diaz (blocs 22, 188, 415) ; au trône dans le remake |
| Pierre (joueur de corrida, mari d'Emma, père de Luca) | 49, 50, 59, 62, 64, 291 | sprite générique `villageois` · `guide_vallee` (reconverti en guide) · zone 026 | PARTIEL | rôle de joueur ruiné et rapatriement ; le remake en fait un guide |
| Zach le Borgne | 213, 215, 217 | sprite nommé · `zach_fort` · zone 050 | PARTIEL | rôle exact incertain (§ 5) |
| Enfants de la Couronne (groupe) | 32, 388, 415, 477, 479 | `enfants_paco`, `enfants_toroledo`, `prunewell_couronne` · sprite générique | PARTIEL | scène du QG (bloc 388), serment, jeunes princes et princesses nommés |
| Axelya / Georges (Alexia et Flint déguisés) | 99, 262, 263 | **contredit** par `georges_puerto` (un matelot qui cherche Axelya) · zone 050 | ABSENT | la scène d'infiltration ; la réplique du remake est incompatible avec les faits attestés |
| Emma, Luca (femme et fille de Pierre) | 49, 50, 59, 291 | — | ABSENT | quête de Pierre |
| Miss Vachette | 53, 55, 416 | — | ABSENT | autographe de la chaîne d'échange |
| Lao et Evo (taureaux) | 20, 58, 61-64 | allusion dans `berger_toroledo` (qui confond la disparition de Lao avec celle de Pierre) | ABSENT | combat de taureaux, Evo de Paco |
| Tirion (dragon prisonnier) | 282 | — | ABSENT | remède de dragon |
| Croc rouge, le Gardien | 282 | — | ABSENT | voir boss |
| Lumière / Magicien du palais | 219, 334 | — | ABSENT | rôle de la tour et du sort qui enferme Jeehan (identité du « magicien » incertaine) |
| Kate (domestique fan de Flint) | 251 | — | ABSENT | indices pour infiltrer le manoir |
| Chat de Méphisto | 11, 304 | — | ABSENT | transformé en monstre par une clé |
| Homme à la main-crochet | 179-194, 202 | seulement cité par `nunugi_gwaba`, `gwaba_habitant` | ABSENT | identité inconnue de l'extraction ; fil narratif des ordres du baron |
| Antagoniste de Fort Noir (rire reconnaissable, serviteurs) | 25, 28, 37 | — | ABSENT | nom et rôle inconnus |
| Prêtres de l'Église de la Clé | 123, 137, 180, 204 | allusions dans `roswell`, `mephisto_trone` | ABSENT | personnages visibles |

**Décompte (33 lignes ; certaines regroupent deux personnages)** : PRÉSENT 2 · PARTIEL 19 · ABSENT 12. Aucun personnage « PRÉSENT » ne l'est dans toute sa trame : Flint (héros) et les androïdes (ennemis) sont complets au sens où l'attesté est jouable.

### 2b. Boss

| Boss attesté (blocs) | Remake | Statut | Notes |
|---|---|---|---|
| Zeppo, combat d'ouverture « écrase-crâne » (bloc 1) | cinématique du prologue seulement | ABSENT | pas de combat jouable ; dialogue `prologue` |
| Chat de Méphisto transformé par une clé, au Mont Épervier (blocs 11, 304) | **ogre** de la forêt Kindra (sprite A1, coffre de la lettre) | PARTIEL (substitut) | **décision de route à préserver** (route validée : « boss du Mont Épervier ») ; seule l'identité diffère : on peut ajouter des lignes mentionnant la clé sans toucher au gameplay |
| Monstre-taureau / « ToroTank » (blocs 304, 305, 328) | aucun | ABSENT | nécessite un sprite original ; redevient taureau normal une fois vaincu (bloc 304) |
| Antagoniste de Fort Noir (blocs 25, 28, 37) | aucun | ABSENT | |
| Croc rouge, Gardien du dragon (bloc 282) | aucun | ABSENT | décrit comme plus rapide que n'importe quel homme |
| Baleine démon (blocs 423, 426) | PNJ `medusa_baleine` seulement | ABSENT | donjon-boss complet |
| Belgar (blocs 264, 267 ; le duel n'est pas confirmé) | aucun combat | ABSENT | incertain |
| Méphisto, affrontement final | boss de la salle du trône (`auto_boss_victory`) | PARTIEL (reconstitution) | l'extraction n'atteste ni le lieu ni la forme du combat final (blocs 371, 484 : fuite et envol seulement) |

Boss du remake **non attestés** par l'extraction (inventions de route à préserver, sprites A1 réutilisés) : golem de la Vallée de Mithra (`boss_026`, gate `lampe`), golem d'Eden (`boss_042`, gate `prisme`), capitaine de Puerto Medusa (`boss_medusa` : ouvre le passage du manoir et le coffre du Cristal). Le capitaine ne doit pas être présenté comme un boss d'A2 ; « Capitaine Zeppo » est un PNJ attesté (bloc 297).

**Décompte des 8 boss attestés** : PRÉSENT 0 · PARTIEL 2 · ABSENT 6.

## 3. Mécaniques et faits narratifs attestés mais non implémentés (ou divergents)

### 3a. Divergences de trame entre le remake et l'extraction

Ces écarts sont des **choix du remake** (compression de la route A → Z). Aucun n'est à corriger sans arbitrage : les gates `epee_supreme`, `lampe`, `prisme`, `boss_medusa`, `cristal`, la route, Méphisto final et les identifiants des tests `auto_dialogue_*` doivent rester. Ils doivent en revanche être **documentés comme tels** et ne pas être présentés comme de la trame A2.

| # | Remake | Ce que l'extraction atteste | Action possible sans toucher aux gates |
|---|---|---|---|
| D1 | Route linéaire Paco → Kindra → épave → Toroledo → Vallée → Eden → Puerto → manoir → trône | Après l'épave, Paco est bombardé ; Toroledo et Pierre ; Vallée de Mithra ; Fort Noir ; infiltration à Puerto Medusa ; Alexia enlevée ; remise des reliques à Diaz ; Gwaba ; plongée ; baleine ; idole ; tour des reliques (§ 1) | présenter la route comme « version resserrée » ; ajouter les chapitres manquants en branches optionnelles ou en étapes intercalées (backlog) |
| D2 | Lettre de Diaz dans un coffre en forêt, gardé par l'ogre | Lettre trouvée dans l'épave par Alexia (bloc 8) | garder le coffre ; ajouter une scène d'Alexia qui commente la lettre (déjà `alexia_epave_objet_lettre`) |
| D3 | Gantelet d'éclair dans l'épave | Gantelet remis par l'apprenti de Jeehan après l'enseignement de l'attaque Éclair (bloc 219) | garder le coffre ; faire parler Jeehan de son apprenti |
| D4 | `epee_supreme` offerte par Jeehan contre les ordres du baron | Épée suprême reçue de Belgar en échange de la cape rouge, au bout d'une chaîne d'échanges (blocs 187, 242) ; `items.txt` modélise la chaîne mais le remake ne la joue pas | documenter ; implémenter la chaîne comme source **alternative** (backlog) |
| D5 | Lampe (golem), Prisme (golem), Cristal (capitaine) ; idole en coffre dans la Vallée | Trois reliques à rapporter à Diaz (blocs 205, 377) ; Cristal pris dans la baleine (bloc 433) ; idole cachée au Mont Vulcain (blocs 231, 237) ; l'énumération des reliques n'est pas cohérente d'un bloc à l'autre (§ 5) | dans les dialogues, éviter d'affirmer les lieux de récupération comme A2 |
| D6 | Objet `ordre` = « ordres du baron » dans un coffre de Toroledo | Les ordres du baron sont la lettre de mission de Flint, présentée à Roswell et aux amis d'Alexia (blocs 183, 202) | renommer la description si nécessaire |
| D7 | Roswell et Oméga à Eden ; Nunugi à Toroledo ; Natasha à Toroledo et au manoir ; Diaz au manoir ; Georges à Puerto ; Belgar à l'épave | Roswell, Oméga, Nunugi à Gwaba/palais ; Natasha et Diaz au palais ; Georges est le déguisement de Flint ; Belgar au manoir | corriger les répliques les plus contradictoires (`georges_puerto`, `berger_toroledo`) ; laisser les positions |
| D8 | Pierre guide de la Vallée | Pierre joueur invétéré, Emma, Luca (blocs 49, 50, 59, 291) | réécrire sans contredire |
| D9 | Ogre, golems, capitaine comme boss | boss attestés : § 2b | documenter |
| D10 | Paco n'est jamais bombardé | Paco bombardé (blocs 23, 24, 235-237) | backlog |
| D11 | Argent absent (or = 0) | Monnaie « O », boutiques, auberges, paris, mini-jeux | arbitrage requis (§ 3b M3) |

### 3b. Mécaniques et événements attestés mais absents

| # | Mécanique / événement | Attesté (blocs) | État du remake | Remarque |
|---|---|---|---|---|
| M1 | **Prime sur Flint** : le héros est « recherché mort ou vif », Alexia peut le livrer ou annuler la prime ; affiches placardées en ville | 22, 212, 247, 251, 262 | absent | amorce du chapitre Paco, voulue par la tranche 1 (liste « manquant ») |
| M2 | **Maison du chef du village** (scène d'Alexia avec Prunewell) | 22 | absent | intérieur de Paco à ajouter |
| M3 | **Monnaie « O » et commerce** : boutiques d'articles, auberges à 10 O, marchands ambulants, pots-de-vin, paris | 21, 54, 55, 100, 103, 183, 187, 239, 269, 332, 425, 461 | absent (menu : or = 0, pas d'économie) | tout mini-jeu payant en dépend ; arbitrage Architecte requis |
| M4 | **Choix dans les dialogues** (menus de réponse : prier ou quitter, donner ou refuser, choix de destination, questionnaire d'Oméga, réponses à Diaz) | 23, 33, 41, 50, 51, 90, 107, 130, 131, 180-182, 198, 206, 290, 414, 477 | absent (dialogues linéaires `Nom: texte`) | **prérequis** de plusieurs entrées du backlog |
| M5 | **Alexia compagnon** : elle suit Flint, annonce qu'elle attend, râle si on la distance | 11, 29, 31, 41, 42, 49, 50 | PNJ immobile | gameplay + niveaux |
| M6 | **Statues de Jeehan** (prier / quitter) dans plusieurs lieux | 19, 23, 24, 51, 159, 203, 289 | absent | fonction exacte (sauvegarde ? soin ?) non établie par le texte |
| M7 | **Chaîne d'échange** : clé étrange (blocs 3, 7) → vache sculptée (54) → marteau (294) → statue rouge (23) → boucle d'oreille de pirate (101) → autographe de Miss Vachette (53) → gaufre à la vanille (416) → Piaf Doudou (404) → cape rouge (187) → épée à Belgar (242) | cités | les 9 objets existent dans `items.txt` mais **aucune source ni aucun PNJ ne les distribue** | contenu A2 pur, sans économie ; très bon candidat de quête |
| M8 | **Pièces de puzzle** (4 puzzles de Jeehan, compteur, techniques enseignées une à une) | 11, 186, 219, 274 | absent (pas d'objet `puzzle_piece` dans `items.txt`) | trame de Jeehan et progression de combat |
| M9 | **Éléments** : bagues de Pixie, Sirène, Dryade, Triton ; tir d'élément 10 PE, Seigneur 200 PE ; cibles colorées ; respirer sous l'eau, marcher sur la lave | 26, 205, 213-218, 222, 229, 404 | quatre bagues présentes dans `items.txt`, **aucun effet** (aucune référence dans `src/`) | contenu A2 cœur de gameplay |
| M10 | **Boucliers** (Mini, Moyen, Titan, Mithril, Suprême, Valar) et quête du bouclier Valar (3 matériaux bruts) | 19, 102, 361, 480 | absents de `items.txt` et du menu | à arbitrer avec l'Architecte (périmètre menu) ; ne pas confondre avec la forge d'Orvan (ajout) |
| M11 | **Noms des épées** dans le script FR : Longue, Mithril, Large, Runes, Brillante, Suprême | 478 | `items.txt` : Longue, bronze, large, runique, brillante, ascendante | aligner les libellés sur le script FR ou documenter (§ 5) ; l'id `epee_supreme` existe à part avec une puissance 0 |
| M12 | **Enchaînements enseignés par Jeehan**, attaque Éclair par charge, « combinaison spéciale » finale | 219 | combo à trois coups + coup chargé présents, mais pas la leçon | lier à M8 |
| M13 | **Zone de soin** (100 % PA/PE) | 13 | absent | peut servir de point de repos dans la Vallée / le manoir |
| M14 | **Mini-jeux** : bombes (90), attaque du chien (109), circuit RC (112), vases (130), pierre-papier-ciseaux jusqu'à 999 999 O (131), fléchettes (214), corrida (49-55), parcours des badges d'or (132), salle de jeu de Gamar (486-488) | cités | absents | la plupart dépendent de M3 |
| M15 | **Déguisement et infiltration** (cheminée, dressing, vêtements volés, navires pirates) | 251, 262, 263, 293, 294 | absent | niveaux + gameplay |
| M16 | **Voyages en navire à choix** (Puerto Medusa, Gwaba, Gamar, Port Yagin) | 206, 290, 383, 409 | absent (passages par warps fixes) | dépend de M4 |
| M17 | **Aides de jeu** (courir, sauter, caméra, poche, coffres, glissade) | 212 | partiellement dans `garde` | |
| M18 | **Énigmes de décor** : blocs à pousser/tirer, interrupteurs au pied, pièges de couleur, levier non-épée, clés/serrures colorées, tapis roulant, ascenseurs en panne, bombes en chaîne | 26, 30, 48, 90, 170, 208, 212, 307, 311, 312, 319, 449-451 | une énigme originale du remake (`rm_*`) ; aucune énigme A2 | éviter de confondre avec l'énigme d'Orvan |
| M19 | **Plongée et profondeur** (bague de Sirène, tube d'air) | 139, 205, 424, 426 | absent | liée à M9 |
| M20 | **Fin d'aventure et collections** (écran « Fin », félicitations des épées/boucliers complets) | menus 14/16, 478, 480 | fin de route présente (`auto_ending`) ; pas d'écran de collection | |

Mécanismes déjà présents et à **ne pas recompter** : variantes de dialogue `_apres` / `_objet_*` / `_<événement>_vaincu` (remake), prologue narratif, menu d'objets A1/A2, sauvegarde V4, forge et énigme d'Orvan (ajouts).

## 4. Backlog priorisé pour les prochaines tranches

### Règles communes à toutes les entrées

- Chaque entrée doit laisser **verts** les tests de route : `auto_gate_*` (5 gates), `auto_boss_victory`, `auto_savetest`/`auto_save_ending`, `auto_dialogue_*` / `auto_zeppo*` / `auto_georges` / `auto_berger*` / `auto_prunewell`, plus `validate_data` (`ERR: 0 | WARN: 0`). Les identifiants de dialogue existants ne changent pas ; les ajouts utilisent de nouveaux identifiants.
- Les nouvelles zones sont des cartes A1 déclarées par `map`/`name` dans `data/world.txt` (pôle niveaux). 483 cartes A1 sont disponibles ; **aucune carte n'est vérifiée pour les lieux ci-dessous** : le choix passe par un rendu PNG (`tools/a1_map.py`) et une vérification des cartes sombres ou vides (précédents : Eden, Puerto, manoir).
- Répliques écrites avec mes propres mots à partir des faits ci-dessus (pas de recopie du script). Tout élément absent d'A2 reste préfixé `rm_*` et documenté comme ajout (`docs/remake/`).
- Aucun sprite existant n'est redessiné ; les nouveaux personnages ou boss (ToroTank, Croc rouge, Tirion, baleine, chat de Méphisto, Emma, Luca, Kate, taureaux) demandent des sprites **originaux** (pôle graphismes) ou le réemploi d'un sprite générique, à décider au cas par cas.
- Effort relatif (une session ciblée) : **S** = quelques fichiers de données/dialogues ; **M** = données + code limité + 2-4 tests ; **L** = nouvelle zone ou nouveau système + sprites + plusieurs tests ; **XL** = donjon complet ou plusieurs zones.
- Les pôles `architecte` (arbitrage de `src/game.h`) et `qa` (tests, `validate_data`) interviennent dès qu'une structure de données ou un test change ; ils ne sont répétés ci-dessous que si leur rôle est central.

### Vague 0 — Pré-requis et corrections (aucune carte nouvelle)

| ID | Zone / thème | Objectif jouable | Pôles | Dépendances | Test déterministe | Effort |
|---|---|---|---|---|---|---|
| B01 | Transversal : fidélité des répliques existantes | Réécrire les répliques qui contredisent le texte A2 (`georges_puerto`, `berger_toroledo`, `guide_vallee`, `zach_fort`, `belgar_epave`, `milena`) sans changer leurs identifiants ni les gates ; documenter les divergences D1-D11 dans `docs/remake/` | narration-ui | aucune | `auto_georges`, `auto_berger`, `auto_berger_ordre`, `auto_zeppo*`, `auto_prunewell` inchangés + `validate_data` | S |
| B02 | Transversal : **choix de dialogue** (M4) | Dialogues à réponses (au moins deux choix, curseur sur le parchemin A1 natif, validation clavier/manette) qui positionnent un drapeau ou donnent/retirent un objet | architecte (`game.h`), narration-ui (`dialogue.cpp`, format de `dialogues.txt`), gameplay (`main.cpp`, commande `--auto`), qa (validateur) | aucune ; prérequis de B03, B04, B07, B10, B11, B12, B14… | nouveau jeton `--auto` de choix (`D1`/`U1` + `T`) et `--expect choice=…` : `auto_choice_yes`, `auto_choice_no`, `auto_choice_cancel` | M |
| B03 | Transversal : **arbitrage de l'économie « O »** (M3) | Décision écrite : garder « or = 0 » ou ajouter une monnaie A2 (affichage, gains, boutiques, auberges) ; si oui, créer le sous-système | architecte, narration-ui (menu), gameplay | décision | `auto_money_*` (gain, achat, refus faute d'argent, sauvegarde) si accepté | S pour décider ; M pour implémenter |

### Vague 1 — Prolonge directement la route (zones 011 → 019)

| ID | Zone / thème | Objectif jouable | Pôles | Dépendances | Test déterministe | Effort |
|---|---|---|---|---|---|---|
| B04 | **Paco (011)** : réveil avec Alexia, prime sur Flint, maison du chef (M1, M2) | Une maison du chef accessible depuis le village : scène d'Alexia, choix « alliance » ou « prime », événement `alliance_alexia` ; chemin principal inchangé (scène facultative) | niveaux (intérieur + warp), narration-ui (`chef_paco`, `alexia_paco`), gameplay (événement) | B02 ; carte A1 d'intérieur à choisir ; sprite Alexia existant | `auto_paco_chef` (entrer, valider, `--expect dialogue=alexia_paco`) ; route inchangée | M |
| B05 | **Forêt Kindra (013)** : marchand en sous-vêtements et retrouvailles Prunewell-Finrod (blocs 32, 33) | Échange herbe → boussole par choix ; scène de Finrod qui évoque Prunewell ; pièce de puzzle dans un coffre (prépare B10) | niveaux (PNJ, coffre), narration-ui, gameplay (don/reprise d'objet) | B02 ; objet `puzzle_piece` à ajouter à `items.txt` | `auto_kindra_marchand` (`--give herbe:1 --expect qty:compass=1,qty:herbe=0`), `auto_kindra_refus` | M |
| B06 | **Mont Épervier (019)** : androïdes humains transformés, clé mystérieuse, histoire du chat de Méphisto (blocs 3, 5, 7, 11) | Ajouter un coffre de la clé étrange (début de la chaîne B12) et des dialogues d'Alexia/Flint sur l'origine des Gi Gi et sur la clé qui transforme les créatures ; le combat reste contre l'ogre | niveaux (coffre), narration-ui | aucune ; ne touche pas au boss | `auto_epave_cle` (`--expect qty:strange_key=1` après ouverture) | S |
| B07 | **Paco bombardé** : variante « ruines » de la carte 011 (blocs 23, 24, 235-237) | Après un événement de route (ex. retour de l'épave), Paco apparaît en ruines avec ses habitants sinistrés et le troc marteau → statue rouge ; **facultatif et réversible**, la route reste jouable sans y repasser | niveaux (variante de carte), graphismes (décor de ruines si nécessaire), narration-ui, gameplay (événement `paco_bombarde`) | B02 ; carte 011 + calque de ruines (nouveau) | `auto_paco_ruines` (zone + dialogues), route inchangée | L |

### Vague 2 — Toroledo et Vallée de Mithra (zones 119, 026)

| ID | Zone / thème | Objectif jouable | Pôles | Dépendances | Test déterministe | Effort |
|---|---|---|---|---|---|---|
| B08 | **Toroledo (119)** : Pierre, Emma, Luca, maison de Pierre (blocs 49, 50, 59, 291) | Quête : parler à Emma et Luca, retrouver Pierre, le ramener ; la maison de Pierre devient un passage supplémentaire vers la Vallée **sans retirer** le gate `epee_supreme` (la quête ouvre une variante cohérente avec A2) | niveaux (maison + warp), narration-ui (~15 dialogues), gameplay (événement), graphismes (sprites génériques pour Emma/Luca ou originaux) | B02 ; B03 si les pourboires/pertes d'argent sont conservés ; carte d'intérieur | `auto_pierre_quete` (`--expect dialogue=…` puis événement `pierre_rentre`) ; `auto_gate_epee_*` inchangés | M |
| B09 | **Toroledo (119)** : arène, auberge, billets (M3, M14) | Arène visitable, billet, auberge (repos) | niveaux, narration-ui, gameplay | B03 accepté | `auto_arene_billet`, `auto_auberge_repos` | L |
| B10 | **Toroledo (119)** : maison de Jeehan, pièces de puzzle, enchaînements, Éclair (M8, M12, D3) | Jeehan enseigne chaque technique contre des pièces de puzzle ; l'attaque Éclair est apprise **ou** gagnée via le gantelet déjà présent ; progression sauvegardée | niveaux (maison), narration-ui, gameplay (système de pièces + techniques), graphismes (icône de pièce) | B02, B05 (pièces), sauvegarde V5 à prévoir | `auto_jeehan_puzzle_1…4`, `auto_save_puzzle` | L |
| B11 | **Vallée de Mithra (026)** : ToroTank (blocs 304, 328, 333) | Boss original en forme de taureau mécanique après le golem, base secrète des jeunes gens ; événement `torotank_vaincu` | gameplay (boss), graphismes (sprite **original**), niveaux (arène), narration-ui | sprite à créer ; carte A1 de grande salle à choisir | `auto_torotank_victory` (`--godmode`, `--expect` événement) ; `auto_gate_lampe_*` inchangés | L |

### Vague 3 — Puerto Medusa, manoir, palais (zones 050, 086, 120)

| ID | Zone / thème | Objectif jouable | Pôles | Dépendances | Test déterministe | Effort |
|---|---|---|---|---|---|---|
| B12 | **Chaîne d'échange partielle** (M7, D4) | Dans les lieux existants : collectionneur de Toroledo (clé → vache), Miss Vachette (boucle d'oreille → autographe), Puerto Medusa (statue rouge → boucle d'oreille), manoir (cape rouge → épée à Belgar) ; les maillons manquants restent à fournir par d'autres entrées : marteau (B21), Piaf Doudou (B18), cape rouge (B16), gaufre (lieu non établi) | niveaux (PNJ), narration-ui (troc + dialogues), gameplay (`trade`) | B02 ; sprites : Miss Vachette (générique `villageoise`) | `auto_troc_cle_vache`, `auto_troc_cape_epee` avec `--give` | M |
| B13 | **Puerto Medusa (050)** : badges d'or, déguisements Axelya/Georges, raid de Méphisto (M14, M15 ; blocs 99, 107, 126, 132, 262-266) | Parcours d'obstacles vers l'entrée du manoir qui donne un badge ; scène d'infiltration (Alexia chanteuse, Flint en frère) ; arrivée de Méphisto qui emmène Alexia ; **ajoutés** à côté du gate `boss_medusa`, pas à sa place | niveaux (parcours, restaurant), gameplay (badge, sprites de déguisement), narration-ui (~20 dialogues), graphismes | B02 ; carte du restaurant et du centre de test ; sprites de déguisement | `auto_badge_or`, `auto_deguisement_dialogue` | L |
| B14 | **Manoir (086)** : infiltration (cheminée, dressing), salle des punitions, confrontation Milena-Méphisto (M15 ; blocs 242-251) | Route furtive alternative vers la chambre de Milena ; scène avec Belgar | niveaux (3-4 pièces), narration-ui, gameplay (furtivité simple), graphismes (Kate) | B12, B13 ; cartes d'intérieur | `auto_manoir_cheminee` | L |
| B15 | **Palais (120)** : répétition de mariage, négociation des reliques, cellules, évasion avec Natasha (blocs 414-419) | Scène jouable avant le boss de Méphisto : choix (donner les reliques ou exiger la libération), cellule, évasion ; **n'altère pas** le boss final (décision de route) | niveaux (cellules), narration-ui, gameplay (choix) | B02, B13 ; carte de cellules | `auto_palais_choix`, `auto_boss_victory` inchangé | M |

### Vague 4 — Nouveaux chapitres hors de la route actuelle

| ID | Zone / thème | Objectif jouable | Pôles | Dépendances | Test déterministe | Effort |
|---|---|---|---|---|---|---|
| B16 | **Gwaba** (blocs 179-207) | Ville hub : mairie à ascenseur, auberge, Roswell (bague de la Sirène, trois reliques), Oméga (questionnaire), enquête sur l'homme à la main-crochet ; déplacer les PNJ du remake en conservant leurs identifiants | niveaux (3-5 cartes), narration-ui (50+ répliques), gameplay (ascenseur, choix), graphismes (génériques) | B02, B03 ; sprites (Maire, Jessica…) génériques | `auto_gwaba_roswell` (`--expect dialogue=…`), `auto_gwaba_ascenseur` | XL |
| B17 | **Ruines des mouettes + plongée** (M19 ; blocs 69, 73-79, 139, 140, 289, 374) | Navire de Zeppo, plongée avec la bague de Sirène, clé des ruines, pièges | gameplay (eau, bague), niveaux, graphismes, narration-ui | B16 (bague de Sirène), B20 (Éléments) | `auto_plongee_cle` | L |
| B18 | **Île d'Eden** (village, Piaf Doudou, bague de Dryade ; blocs 404-406) | Mini-jeu de capture du Piaf Doudou, bague de Dryade, clé de la statue jaune | niveaux, gameplay (IA de l'oiseau), narration-ui, graphismes (oiseau original) | B20 | `auto_piaf_capture`, `auto_dryade` | L |
| B19 | **Baleine démon** (blocs 423-462) | Donjon en six zones (bouche, poumons, cœur, estomac, intestins, cerveau), énigmes de clés colorées et de ponts, Cristal d'Étoile | niveaux (6 cartes), gameplay (énigmes), graphismes (baleine), narration-ui | B16, B17 ; décision sur la place du Cristal dans la route actuelle | `auto_baleine_*` par zone | XL |
| B20 | **Éléments** : bagues et tirs (M9) | Quatre bagues avec effet (projectile, nage, piétinement, feu), coût 10 PE, Seigneurs, cibles colorées | gameplay, graphismes (effets), narration-ui (descriptions), niveaux (cibles) | items déjà présents ; arbitrage EP/HUD | `auto_ring_pixie`, `auto_ring_siren`, `auto_ring_dryad`, `auto_ring_newt` | L |
| B21 | **Fort Noir + Maison du Bonheur + Port Yagin** (blocs 25-28, 37-39, 291-302) | Donjon minier avec chariots, ascenseurs en panne, piège de la Maison du Bonheur, antagoniste | niveaux, gameplay, graphismes, narration-ui | B08 (Pierre), B02 | `auto_fortnoir_*` | XL |
| B22 | **Mont Vulcain, Dragonia, Tirion, Croc rouge** (blocs 220-231, 272-283) | Passage à l'est de l'épave, lave (bague de Triton), prison du dragon, boss Croc rouge, remède | niveaux, gameplay, graphismes (dragon, boss), narration-ui | B20 (Triton), B21 | `auto_vulcain_*`, `auto_croc_rouge_victoire` | XL |
| B23 | **Église de la Clé, digue, QG des Enfants** (blocs 203-205, 388, 463-479) | Souterrains, observatoire, QG ; relie les enquêtes de B16 | niveaux, narration-ui, gameplay | B16 | `auto_eglise_*` | L |
| B24 | **Île de Gamar et mini-jeux à médailles** (M14) | Salle de jeu, médailles, trois jeux | niveaux, gameplay, narration-ui | B03 (économie) | `auto_gamar_*` | L |

### Entrées transversales à placer selon la capacité

| ID | Thème | Objectif jouable | Pôles | Dépendances | Test | Effort |
|---|---|---|---|---|---|---|
| B25 | **Alexia compagnon** (M5) | Alexia suit Flint, attend à l'entrée des zones, commente les événements | gameplay, niveaux (pathing), narration-ui | B04 | `auto_compagnon_suit` (`--expect` position relative) | L |
| B26 | **Boucliers A2** (M10) | Six boucliers dans le menu, effets défensifs, quête du bouclier Valar | architecte (arbitrage menu), gameplay, narration-ui (`items.txt`), graphismes (icônes) | B03 | `auto_menu_shield`, `auto_valar` | M |
| B27 | **Alignement des noms d'épées** (M11) | Libellés FR identiques au script (Mithril, Suprême…) ou note de divergence ; `rising_sword` / `epee_supreme` clarifiés | narration-ui (`items.txt`), qa | aucune | `validate_data`, `auto_menu_*` | S |
| B28 | **Statues de Jeehan, zone de soin, écran « Fin » et collections** (M6, M13, M20) | Statues interactives (prier/quitter), repos 100 % PA/PE, écran de fin | gameplay, narration-ui | B02 ; fonction exacte des statues à confirmer | `auto_statue_prier`, `auto_zone_soin` | M |

### Ordre recommandé

1. B01 (correctifs) et B02 (choix) : sans carte nouvelle, débloquent le reste. 2. B04, B05, B06 : prolongent directement Paco → épave. 3. B08, B10 puis B11 : Toroledo et Vallée. 4. B12, B13, B14, B15 : Puerto, manoir, palais. 5. B03 à trancher dès que possible : conditionne plusieurs entrées. 6. Vague 4 par dépendances (B16 → B17/B18 → B19 ; B20 transversal).

## 5. Ce que le remake ne doit pas prétendre

Cet audit repose sur une **extraction heuristique** (`tools/a2_text.py`) : elle retient les suites de mots-caractères contenant au moins trois lettres d'affilée, regroupées en « messages » par fin de message. Elle n'est pas déclarée exhaustive, et ses limites imposent les réserves suivantes.

1. **Pas de couverture complète d'A2.** 262 blocs / 553 messages ; les numéros de bloc vont jusqu'à 502, donc beaucoup d'indices sont absents de l'extraction. L'écart avec les « 815 blocs » de l'historique n'est pas résolu (815 = 262 en-têtes + 553 messages en lignes non vides). Les blocs contenant un texte provisoire du jeu (86, 87, 149, 170, 502) ne portent pas d'histoire. Les messages très courts ou composés de moins de trois lettres consécutives peuvent manquer.
2. **Aucune information sur qui parle.** L'extraction ne contient ni noms de locuteurs, ni portraits, ni conditions d'apparition, ni drapeaux d'événement. Quand ce document attribue une réplique à un personnage, c'est par recoupement de contenu : par exemple, l'échange de la cape rouge contre l'épée suprême (bloc 242) est rattaché à Belgar par le commentaire d'`items.txt` (page de l'épée sur le wiki A2) et par le contexte, pas par le texte.
3. **Aucune carte, aucun ennemi, aucune position de coffre ou de PNJ n'est attesté.** Le texte ne dit rien de la topologie des lieux, du nombre ou de la forme des boss, des récompenses de coffres, ni des énigmes (seules des consignes verbales apparaissent). La place des objets dans les zones du remake est donc **toujours** une décision du remake.
4. **Lieux inférés.** Seuls les blocs qui contiennent un titre entre crochets nomment un lieu (Paco, Mont Épervier, Toroledo, Vallée de Mithra, Ruines d'Eden, Gwaba, Eden, Gamar, etc.). Les rattachements des blocs 25-28, 37-39, 178, 332-334, 354, 364, 365, 388, 407 et d'autres sont inférés par proximité ou par le contenu. L'ordre des chapitres l'est aussi, en particulier le milieu de l'aventure (Fort Noir, Mont Vulcain, Vallée de Mithra).
5. **Composition des reliques ambiguë.** Un bloc cite Lampe, Prisme, Cristal (blocs 205, 377) ; un autre demande d'ajouter l'idole à trois yeux à un trio dont deux membres sont le Prisme et le Cristal (blocs 334, 365). Le remake a retenu quatre objets (Lampe, Prisme, Cristal, idole). Ne pas affirmer que la liste est « Lampe + Prisme + Cristal + idole » comme un fait A2 sans seconde source.
6. **Identités non établies** : l'homme à la main-crochet (blocs 179-194, 202), l'antagoniste de Fort Noir, le « magicien du palais » et Lumière, le lien exact entre Belgar et Flint (blocs 242, 267, 288), la fonction des statues de Jeehan, le rôle exact de Zach le Borgne. Les répliques du remake qui y répondent (par exemple celle qui prête à Belgar une connaissance du père de Flint) sont des **hypothèses**, pas des faits attestés.
7. **Final non attesté.** L'extraction ne contient aucun épilogue ni combat final explicite (seulement une fuite de Méphisto, bloc 484, et un message d'envol, bloc 371). Le boss de la salle du trône, la victoire testée par `auto_boss_victory` et les dialogues `*_mephisto_vaincu` sont des reconstitutions du remake.
8. **Version française seule.** `GRP_FR.GP` est lu ; les autres langues n'ont pas été comparées. Plusieurs noms FR diffèrent de ceux du wiki (Mithril/bronze, Suprême/ascendante, Piaf Doudou/Goo Goo Bird). Aucun nom n'a été « corrigé » ici.
9. **Les boss golem, capitaine et ogre, la Lampe gardée, la forge, l'énigme et Orvan ne sont pas de l'A2.** Les trois premiers sont des choix de route à préserver, les trois autres sont des ajouts `rm_*` (§ Annexe). Aucun texte de communication ne doit les présenter comme de l'histoire d'Alundra 2.
10. **Les répliques du remake sont originales.** Elles s'inspirent des faits résumés ici et ne reproduisent pas le script du jeu ; elles ne prouvent donc pas que les propos des personnages A2 sont identiques.
11. **Le prologue du remake** suit l'ouverture attestée (bloc 1 et bloc 14) en cinématique ; il ne rejoue ni le combat, ni les Gi Gi aux commandes.
12. **Les statistiques de ce document** (22 lieux, 33 personnages, 8 boss, 20 mécaniques) découpent l'extraction à ma façon ; un autre découpage peut changer les totaux sans changer les conclusions.

## Annexe — Ajouts du remake exclus des décomptes

Voir `docs/remake/ajouts_tranche3.md`. Résumé : Orvan (forgeron original, Paco 36,47), forge atomique à deux branches (6 recettes), `rm_ember_blade`, `rm_frost_lance`, `rm_rift_hammer`, cinq matériaux `rm_*`, énigme brasero / chemin gelé / pilier / mécanisme / levier, coffres de matériaux (Paco, Kindra, épave), sauvegarde V4 (`ameliore`, `interaction`), sprites originaux (`tools/rm_assets.py`). Dialogues concernés : `rm_forgeron`, `rm_forgeron_forge`, `objet_rm_*`, `rm_brasero_allume`, `rm_chemin_gele`, `rm_pilier_brise`, `rm_mecanisme_*`, `rm_interaction_sans_effet`, `rm_forge_*` (20 identifiants).

## Annexe — Zones jouables du remake (rappel)

| Zone | Nom affiché | Carte A1 | PNJ | Coffres | Monstres | Passages |
|---|---|---|---|---|---|---|
| 011 | Village Paco | 011 | 4 + Orvan | herbe (+ 3 `rm_*`) | aucun | 013 |
| 013 | Forêt Kindra | 013 | Finrod | cœur, lettre (+ 3 `rm_*`) | gelée ×2, tortue ×2, gobelin ×2, ogre | 011, 019 |
| 019 | Mont Épervier - Épave | 019 | Alexia, Belgar | gantelet (+ 3 `rm_*`) | droïde ×2, gobelin | 013, 119 |
| 119 | Toroledo | 119 | 9 | ordre | droïde ×2 | 019, 026 (gate `epee_supreme`) |
| 026 | Vallée de Mithra | 026 | Pierre | idole (gate `ordre`), lampe (gate `boss_026`) | golem, gelée, tortue, gobelin | 119, 042 (gate `lampe`) |
| 042 | Ruines d'Eden | 028 | Roswell, Oméga | prisme (gate `boss_042`) | golem, droïde, gobelin | 026, 050 (gate `prisme`) |
| 050 | Puerto Medusa | 029 | 9 | cristal (gate `boss_medusa`) | capitaine, droïde, gobelin | 042, 086 (gate `boss_medusa`) |
| 086 | Manoir de Milena | 316 | 7 | cœur | droïde, gobelin | 050, 120 (gate `cristal`) |
| 120 | Salle du trône | 120 | Alexia, Roi | aucun | Méphisto, droïde | 086 |

(38 PNJ, 19 coffres, 26 apparitions de monstres dans `data/world.txt`, dont dix éléments `rm_*` : un PNJ et neuf coffres.)

## B01 — corrections de fidélité

Corrections de répliques de `data/dialogues.txt` qui contredisaient un fait attesté d'Alundra 2 (numéros de bloc de l'extraction locale `extracted/a2_text/dialogues.txt`). Identifiants, locuteurs à portrait, variantes `_apres` / `_objet_*`, gates et nombre de pages inchangés. Les choix de route documentés (D1-D11) restent tels quels. Réécriture avec les mots du remake, sans citation du script.

| Identifiant | Ce qui contredisait A2 | Bloc(s) | Nouvelle intention |
|---|---|---|---|
| `georges_puerto` | « Georges » était un matelot qui cherchait « Axelya » ; dans A2 ce sont les noms d'emprunt de Flint et d'Alexia, et Axelya est une chanteuse mise à prix. Le locuteur devient `Habitant` (sans portrait, comme avant). | 262, 263 (aussi 235, 99) | un habitant parle de la chanteuse Axelya et de la prime sur sa tête, évoque son petit frère Georges (clin d'œil au déguisement, sans le révéler) et avertit que les gardes traquent un chasseur de pirates |
| `berger_toroledo` (réplique 1-2) | Lao est un taureau de corrida de Toroledo qui disparaît puis revient ; il n'est pas lié à la Grotte Hurlante, c'est Pierre, le joueur, qui y est parti. Les bêtes « transformées en machines » ne sont pas attestées. | 58, 61, 49, 59 | Lao a disparu, bêtes et gens disparaissent, l'argent aussi ; Pierre le parieur a été vu partant vers la Grotte Hurlante. Réplique 3 (Jeehan) inchangée |
| `guide_vallee` (réplique 1) | Pierre n'est pas un ancien meneur de taureaux reconverti en guide : c'est un joueur ruiné, marié à Emma, père de Luca. Le rôle de passeur vers la Lampe (choix de route D8) est conservé. | 49, 50, 59, 291 | Pierre se présente comme un joueur sans argent que sa femme Emma cherche ; la suite (golem, Lampe) inchangée |
| `belgar_epave` (répliques 1-2) | Belgar n'est pas le « second » de Milena : il est son compagnon et a laissé le garçon s'enfuir. Sa présence à l'épave reste un choix de route non attesté. | 267, 264, 242 | il se présente comme le compagnon de Milena et dit ne pas venir combattre Flint |
| `milena` (réplique 2) | Milena ne « travaille plus pour Diaz » et n'est pas ralliée à Méphisto : elle reste liée au Baron et s'oppose au sorcier (reproches, intention d'en référer au Baron). | 242, 267, 14 | elle méprise le « partenaire » du Baron et ne le tient pas pour un allié |
| `milena_objet_ordre` (répliques 1-2) | même contradiction (« ce n'est plus mon problème », promesse de Méphisto) | 242, 267 | elle redoute le soulèvement du peuple et garde ses comptes à régler pour le sorcier du Baron, pas pour Flint |
| `zach_fort` (réplique 2) | l'anecdote « Port Yagin / chercher du travail » appartient à un autre personnage de Fort Noir ; Zach est déjà entré une fois et on l'a éconduit. | 115, 213, 215, 217 | Zach dit avoir déjà poussé une porte sans être accueilli et s'être trompé de couloir |
| `medusa_arrivee` (réplique 2) | Flint affirme que les hommes de Milena ont bombardé Paco ; A2 présente le bombardement comme une attaque sous faux drapeau (symbole pirate sur le ToroTank), que Flint et Alexia jugent étrangère aux pirates. | 23, 50, 242 | Paco a été bombardé sous l'emblème pirate, mais Flint doute que Milena en soit l'auteur ; Méphisto tire les ficelles |

Laissées telles quelles (choix de route ou hypothèses déjà documentés, non corrigés ici) : lettre en forêt (D2), gantelet dans l'épave (D3), épée suprême (D4), reliques et lieux de récupération (D5), ordres du baron (D6), positions des PNJ (D7), boss (D9), Paco intact (D10), argent (D11), `belgar_manoir` (« j'ai connu ton père », hypothèse § 6), « main de crochet » (identité non établie). Coquilles repérées mais hors périmètre de fidélité : « roulettez » (`guide_vallee`), « te attend » (`milena_boss_medusa`).

Vérification : `tools\validate_data.py` → `ERR: 0 | WARN: 0` ; `tests\check_dialogue_pagination.py` → `ERR: 0`.

## B04 — Maison du chef de Paco et prime sur Flint (partie narration)

Scène **facultative** : la route A → Z, ses gates et les identifiants testés (`prunewell`, `auto_dialogue_*`) ne changent pas. Le PNJ chef utilise le dialogue `chef_paco` (placement : pôle niveaux). Réécriture avec les mots du remake, sans citation du script.

| Élément | Statut | Détail |
|---|---|---|
| Maison du chef du village de Paco ; le chef peut livrer Flint pour toucher la récompense ou annuler la prime | **attesté A2** (bloc 22 de l'extraction locale) | fait de départ de `chef_paco` |
| Auteur de la prime : emblème des pirates de Milena, sceau du baron Diaz juste dessous | **propre au remake** | cohérent avec la trame (Diaz et les pirates alliés ; `prunewell` inchangé, Alexia part vers le Mont Épervier) |
| Question à trois réponses (`Annuler` / `Laisser` / `Plus tard`) et issues `chef_paco_annulee`, `chef_paco_maintenue`, `chef_paco_plus_tard` | **ajout du remake** (outil de choix B02) | événements `prime_annulee` / `prime_maintenue`, aucun effet sur la route ; « Plus tard » ne pose rien |
| Visites suivantes `chef_paco_annulee_suite`, `chef_paco_maintenue_suite` | **ajout du remake** | variantes `! si` en tête de `chef_paco` |
| Réactions de `villageoise` et `garde` (`villageoise_prime_*`, `garde_prime_*`) | **ajout du remake** | `! si` en tête des dialogues existants ; répliques d'origine inchangées, conseils de combat repris dans les variantes |

Limite connue : une fois la lettre trouvée, `villageoise` et `garde` passent sur leurs variantes `_objet_lettre` / `_apres` (priorité du moteur de variantes) ; les réactions à la prime ne s'affichent donc qu'avant la lettre.

Vérification : `tools\validate_data.py` → `ERR: 0 | WARN: 0` ; `tests\check_dialogue_pagination.py` → `ERR: 0`.
