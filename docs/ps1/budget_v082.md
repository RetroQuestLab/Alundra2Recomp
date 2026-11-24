# Budgets PS1 — mesures sur la version livrée

ELF mesuré : `ps1\a_graver\v0.8.2-cd-music\alundra2.elf`

Toutes les valeurs ci-dessous sont des **mesures** obtenues par les commandes
indiquées. Les quantités non instrumentées sont signalées explicitement.

## 1. RAM principale — limite 2 097 152 octets

Commande : `mipsel-none-elf-size -A <ELF>` (sections chargées uniquement).

| Section | Octets mesurés |
|---|---:|
| `.MIPS.abiflags` | 24 |
| `.bss` | 241,748 |
| `.ctors` | 8 |
| `.data` | 1,267,272 |
| `.dtors` | 8 |
| `.rodata` | 98,431 |
| `.sbss` | 452 |
| `.sdata` | 62 |
| `.text` | 78,028 |
| **Total chargé (.text → .bss)** | **1,686,033** |

- Dépassement : **aucun** ; marge **411,119 octets** (80.4 % du budget).
- Le ring de streaming `0x18000` = 98,304 octets est inclus dans `.bss` (mesure confirmée par `nm` : `s_ring`).
- **Non instrumenté** : pic heap réel et profondeur de pile. Le code de jeu n'appelle pas `malloc` (ring statique, pas de STL) ; la pile est fixée par le BIOS en haut de RAM.

## 2. VRAM — limite 1 048 576 octets

Méthode : rectangles de `tools/ps1_export_zone.py::RESERVED_RECTS` (synchros `src/platform/pf.h`)
+ rectangles réellement placés dans les en-têtes `P1ZM` des cinq zones embarquées.
1 demi-mot VRAM = 2 octets.

| Région | Octets mesurés |
|---|---:|
| Réservé permanent (framebuffers, police, WIND, HUD, héros, acteurs, icônes) | 455,076 |
| Tuiles + CLUT `zone011.dat` | 176,288 |
| Tuiles + CLUT `zone013.dat` | 118,368 |
| Tuiles + CLUT `zone019.dat` | 102,176 |
| Tuiles + CLUT `zone119.dat` | 39,968 |
| Tuiles + CLUT `zone120.dat` | 48,992 |
| **Pic (réservé + plus grande zone `zone011`)** | **631,364** |

- Dépassement : **aucun** ; marge **417,212 octets** (60.2 % du budget).
- Les zones se partagent les mêmes rectangles : le pic est `réservé + une zone`, pas la somme.

## 3. Audio SPU — limite 524 288 octets

Méthode : taille réelle de `ps1/assets/audio.vag`, constantes lues dans `src/ps1/audio_ps1.cpp`,
interleave lu dans l'en-tête VAGi de `ps1/assets/music/00.VAG`.

| Région | Adresse début | Adresse fin | Octets mesurés |
|---|---|---|---:|
| Banque VAG résidente | 0x01010 | 0x5D5D0 | 378,304 |
| Double buffer de flux (2 × interleave) | 0x5D600 | 0x61600 | 16,384 |
| **Total SPU occupé** | — | — | **394,688** |

- Dépassement : **aucun** ; marge **129,600 octets** (75.3 % du budget).
- CD : 40 pistes VAG, 49,698,816 octets ; ISO livrée 53,297,152 octets (`tools/ps1_make_iso.py`). Le flux n'occupe pas la RAM SPU au-delà du double buffer.

## 4. Temps d'image — NON MESURÉ ICI

La cible est 16,67 ms (NTSC 60 Hz, p50/p95/pic sur 1000 images). Le compteur Root Timer
n'est pas encore instrumenté dans le moteur PS1. **Aucune valeur n'est rapportée** :
un futur rapport devra corréler compteur matériel et `VPS` émulateur (jamais `FPS` en `-nogui`).

## Conclusion

Les trois budgets mesurables sont respectés : RAM 1,686,033/2,097,152, VRAM 631,364/1,048,576, SPU 394,688/524,288 octets. Le temps d'image reste à instrumenter.
