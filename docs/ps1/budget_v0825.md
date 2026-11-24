# Budgets PS1 — mesures sur la version livrée

ELF mesuré : `ps1\a_graver\v0.8.25-alexia-kindra\alundra2.elf`
ISO mesurée : `ps1\a_graver\v0.8.25-alexia-kindra\alundra2_ntsc.iso`

Reproduction : `py -3.12 tools\ps1_budget_report.py ps1\a_graver\v0.8.25-alexia-kindra\alundra2.elf --iso ps1\a_graver\v0.8.25-alexia-kindra\alundra2_ntsc.iso --out docs\ps1\budget_v0825.md`

- SHA256 ELF : `66FAA491E00C3F8E5FE6D37802CC4CAC485ECD7DA4ADC7F7D211D7E579F2D897`
- SHA256 ISO : `3CE449709D14CAD439575E5B3F497F122414658F29099F98ADDF8768FD06A6F9`

Toutes les valeurs ci-dessous sont des **mesures** obtenues par les commandes
indiquées. Les quantités non instrumentées sont signalées explicitement.

## 1. RAM principale — limite 2 097 152 octets

Commande : `mipsel-none-elf-size -A <ELF>` (sections chargées uniquement).

| Section | Octets mesurés |
|---|---:|
| `.MIPS.abiflags` | 24 |
| `.bss` | 318,548 |
| `.ctors` | 8 |
| `.data` | 1,287,124 |
| `.dtors` | 8 |
| `.rodata` | 100,779 |
| `.sbss` | 468 |
| `.sdata` | 74 |
| `.text` | 91,888 |
| **Total chargé (.text → .bss)** | **1,798,921** |

- Dépassement : **aucun** ; marge **298,231 octets** (85.8 % du budget).
- Le ring de streaming `0x18000` = 98,304 octets est inclus dans `.bss` (mesure confirmée par `nm` : `s_ring`).
- **Non instrumenté dans cette exécution** : pic heap réel et profondeur de pile. Passer `--diag-log` pour obtenir des mesures ; aucune valeur n'est inventée ici.

## 2. VRAM — limite 1 048 576 octets

Méthode : rectangles de `tools/ps1_export_zone.py::RESERVED_RECTS` (synchros `src/platform/pf.h`)
+ rectangles réellement placés dans les en-têtes `P1ZM` des cinq zones embarquées.
1 demi-mot VRAM = 2 octets.

| Région | Octets mesurés |
|---|---:|
| Réservé permanent (framebuffers, police, WIND, HUD, héros, acteurs, icônes) | 465,316 |
| Tuiles + CLUT `zone011.dat` | 176,288 |
| Tuiles + CLUT `zone013.dat` | 118,368 |
| Tuiles + CLUT `zone019.dat` | 102,176 |
| Tuiles + CLUT `zone026.dat` | 69,344 |
| Tuiles + CLUT `zone119.dat` | 39,968 |
| Tuiles + CLUT `zone120.dat` | 48,992 |
| **Pic (réservé + plus grande zone `zone011`)** | **641,604** |

- Dépassement : **aucun** ; marge **406,972 octets** (61.2 % du budget).
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
- CD : 40 pistes VAG, 49,698,816 octets ; ISO livrée 53,813,248 octets (`tools/ps1_make_iso.py`). Le flux n'occupe pas la RAM SPU au-delà du double buffer.
- Le BIN MODE2/2352 du lot fait 61,801,152 octets (secteurs avec en-tête et parité) ; ce n'est **pas** l'image gravable. L'ISO 2048 livrée est convertie depuis ce BIN et constitue la taille de référence pour le mastering.

## 3b. Pic SPU pendant le streaming CD — non référencé dans ce rapport

Le calcul statique additionne la banque VAG residente et le double buffer musical, soit **394,688 octets** ; il ne mesure pas les ecritures reelles ni leurs chevauchements. Passer `--spu-log` pour joindre la capture de la table d'occupation instrumentée. Aucune valeur dynamique n'est inventée ici.

## 4. Temps d'image — mesure disponible, non référencée dans ce rapport

La sonde MIPS (`g_ps1FrameTiming`, `tools/ps1_frame_timing.ps1`) est opérationnelle depuis le lot `v0.8.6-frame-timing`. Ce rapport ne cite aucune valeur de temps d'image : passer `--timing-log <log de capture>` pour joindre une mesure sur l'ISO livrée. Aucune valeur n'est inventée ici.

## 5. Pic RAM par zone (chargement dynamique) — non référencé dans ce rapport

Passer `--zone-heap-log` pour joindre la capture des zones 011 → 013 → 026 → 011. Aucune valeur de pic n'est inventée ici.

## 6. Pic VRAM pendant le rendu — non référencé dans ce rapport

Passer `--vram-log` pour joindre la capture de la table d'allocation instrumentée. Aucune valeur de pic dynamique n'est inventée ici.

## Conclusion

Les trois budgets mesurables sont respectés : RAM 1,798,921/2,097,152, VRAM 641,604/1,048,576, SPU 394,688/524,288 octets. Le temps d'image n'est pas référencé dans ce rapport (sonde disponible depuis v0.8.6). Le pic heap par zone n'est pas référencé dans ce rapport. Le pic VRAM dynamique n'est pas référencé dans ce rapport. Le pic SPU dynamique n'est pas référencé dans ce rapport. 
