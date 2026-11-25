# Budgets PS1 — mesures sur la version livrée

ELF mesuré : `ps1\a_graver\v0.8.36-sauvegarde-fiable\alundra2.elf`
ISO mesurée : `ps1\a_graver\v0.8.36-sauvegarde-fiable\alundra2_ntsc.iso`

Reproduction : `py -3.12 tools\ps1_budget_report.py ps1\a_graver\v0.8.36-sauvegarde-fiable\alundra2.elf --iso ps1\a_graver\v0.8.36-sauvegarde-fiable\alundra2_ntsc.iso --out docs\ps1\budget_v0836.md --diag-log qa\ps1_logs\memoire_v0836\diag\diag_capture.txt --zone-heap-log qa\ps1_logs\memoire_v0836\zone_heap\zone_heap_capture.txt`

- SHA256 ELF : `575931878AFE4CFC30086CBBA2D1813AD568249DD918DC2CED117F0CDD3B35CA`
- SHA256 ISO : `0B8111BA84275D757B3D8C09EE81798E9E412BD5D79C8EE9F3C8F384B8FCBE23`

Toutes les valeurs ci-dessous sont des **mesures** obtenues par les commandes
indiquées. Les quantités non instrumentées sont signalées explicitement.

## 1. RAM principale — limite 2 097 152 octets

Commande : `mipsel-none-elf-size -A <ELF>` (sections chargées uniquement).

| Section | Octets mesurés |
|---|---:|
| `.MIPS.abiflags` | 24 |
| `.bss` | 327,860 |
| `.ctors` | 8 |
| `.data` | 656,424 |
| `.dtors` | 8 |
| `.rodata` | 115,175 |
| `.sbss` | 508 |
| `.sdata` | 78 |
| `.text` | 101,656 |
| **Total chargé (.text → .bss)** | **1,201,741** |

- Dépassement : **aucun** ; marge **895,411 octets** (57.3 % du budget).
- Le ring de streaming `0x18000` = 98,304 octets est inclus dans `.bss` (mesure confirmée par `nm` : `s_ring`).
- **Mesuré** : pic heap `HeapUsage.alloc_max` du SDK PSn00bSDK = **227,328 octets** (allocation courante finale 227,328 octets sur un tas de 797,072 octets). Un pic à 0 signifierait « aucun appel d'allocation observé », pas une estimation.
- **Mesuré** : profondeur de pile MIPS maximale = **2,888 octets** (plancher `0x801ff3c8`, sommet `0x801fff10`). Méthode : peinture `0xA5A5A5A5` d'une fenêtre de 256 Kio au plus, sous le pointeur de pile réel et au-dessus du sommet de l'arène `malloc`, garde de 512 octets, balayage toutes les 60 images. Une pile très superficielle peut être sous-estimée de cette garde.
- Relevé après **2,301** appels à `diagSamplePS1()`, le 2026-11-25T01:12:46, log `qa\ps1_logs\memoire_v0836\diag\diag_capture.txt`, ISO SHA256 `0B8111BA84275D757B3D8C09EE81798E9E412BD5D79C8EE9F3C8F384B8FCBE23`.
- Commande de capture : `C:\Users\Liori\Alundra\external\tools\pcsx-redux\pcsx-redux.main -testmode -stdout -lua_stdout -bios C:\Users\Liori\AppData\Local\DuckStation\bios\openbios.bin -iso C:\Users\Liori\Alundra\ps1\a_graver\v0.8.36-sauvegarde-fiable\alundra2_ntsc.iso -fastboot -run -debugger -interpreter -portable -noupdate -dofile C:\Users\Liori\Alundra\docs\ps1\lua\capture_diag.lua`

## 2. VRAM — limite 1 048 576 octets

Méthode : rectangles de `tools/ps1_export_zone.py::RESERVED_RECTS` (synchros `src/platform/pf.h`)
+ rectangles réellement placés dans les en-têtes `P1ZM` des cinq zones embarquées.
1 demi-mot VRAM = 2 octets.

| Région | Octets mesurés |
|---|---:|
| Réservé permanent (framebuffers, police, WIND, HUD, héros, acteurs, icônes) | 469,668 |
| Tuiles + CLUT `zone011.dat` | 176,288 |
| Tuiles + CLUT `zone013.dat` | 118,368 |
| Tuiles + CLUT `zone019.dat` | 102,176 |
| Tuiles + CLUT `zone026.dat` | 69,344 |
| Tuiles + CLUT `zone042.dat` | 104,512 |
| Tuiles + CLUT `zone050.dat` | 117,056 |
| Tuiles + CLUT `zone086.dat` | 83,264 |
| Tuiles + CLUT `zone119.dat` | 39,968 |
| Tuiles + CLUT `zone120.dat` | 48,992 |
| **Pic (réservé + plus grande zone `zone011`)** | **645,956** |

- Dépassement : **aucun** ; marge **402,620 octets** (61.6 % du budget).
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
- CD : 40 pistes VAG, 49,698,816 octets ; ISO livrée 53,901,312 octets (`tools/ps1_make_iso.py`). Le flux n'occupe pas la RAM SPU au-delà du double buffer.
- Le BIN MODE2/2352 du lot fait 61,902,288 octets (secteurs avec en-tête et parité) ; ce n'est **pas** l'image gravable. L'ISO 2048 livrée est convertie depuis ce BIN et constitue la taille de référence pour le mastering.

## 3b. Pic SPU pendant le streaming CD — non référencé dans ce rapport

Le calcul statique additionne la banque VAG residente et le double buffer musical, soit **394,688 octets** ; il ne mesure pas les ecritures reelles ni leurs chevauchements. Passer `--spu-log` pour joindre la capture de la table d'occupation instrumentée. Aucune valeur dynamique n'est inventée ici.

## 4. Temps d'image — mesure disponible, non référencée dans ce rapport

La sonde MIPS (`g_ps1FrameTiming`, `tools/ps1_frame_timing.ps1`) est opérationnelle depuis le lot `v0.8.6-frame-timing`. Ce rapport ne cite aucune valeur de temps d'image : passer `--timing-log <log de capture>` pour joindre une mesure sur l'ISO livrée. Aucune valeur n'est inventée ici.

## 5. Pic RAM par zone — mesuré (capture CD sur l'ISO livrée)

Méthode : le harnais capture-only lit chaque blob P1ZM depuis le CD dans l'arene `malloc`, appelle le parseur de zone existant, libere le blob avant l'allocation suivante, et releve `HeapUsage.alloc` / `alloc_max` du SDK. Le jeu normal garde ses blobs `incbin` et n'entre pas dans ce chemin.

| Etape | Zone | `alloc` mesuré (octets) | `alloc_max` SDK (octets) | Octets lus | Liberations |
|---:|---:|---:|---:|---:|---:|
| 1 | 013 | 159744 | 225280 | 154636 | 1 |
| 2 | 026 | 106496 | 225280 | 102282 | 2 |
| 3 | 011 (retour) | 225280 | 225280 | 220006 | 3 |
| 4 | liberation finale | 225280 | 225280 | 220006 | 3 |

- **Mesuré** : hauteur d'eau initiale (`baselineAlloc`) = **225280 octets** ; retour final (`finalAlloc`) = **225280 octets**. Egalité requise : aucun octet de zone n'est conservé après la séquence.
- **Mesuré** : `alloc_max` est un compteur monotone du SDK ; le pic global de la capture est **225280 octets**. La plus grande zone seule atteint **225280 octets** (zone 011, 220006 octets lus).
- **Mesuré** : `staticRam` = **1267304 octets** (adresse de fin de segments charges, limite superieure de l'arene).
- Sequence reelle : 011 (zone initiale du jeu, etat baseline) → 013 → 026 → 011 (blob CD relu) → liberation explicite. Aucune valeur n'est estimee.
- Releve le 2026-11-25T01:13:41, log `qa\ps1_logs\memoire_v0836\zone_heap\zone_heap_capture.txt`, ISO SHA256 `0B8111BA84275D757B3D8C09EE81798E9E412BD5D79C8EE9F3C8F384B8FCBE23`.
- Commande de capture : `C:\Users\Liori\Alundra\external\tools\pcsx-redux\pcsx-redux.main -testmode -stdout -lua_stdout -bios C:\Users\Liori\AppData\Local\DuckStation\bios\openbios.bin -iso C:\Users\Liori\Alundra\ps1\a_graver\v0.8.36-sauvegarde-fiable\alundra2_ntsc.iso -fastboot -run -debugger -interpreter -portable -noupdate -dofile C:\Users\Liori\Alundra\docs\ps1\lua\capture_zone_heap.lua`

## 6. Pic VRAM pendant le rendu — non référencé dans ce rapport

Passer `--vram-log` pour joindre la capture de la table d'allocation instrumentée. Aucune valeur de pic dynamique n'est inventée ici.

## Conclusion

Les trois budgets mesurables sont respectés : RAM 1,201,741/2,097,152, VRAM 645,956/1,048,576, SPU 394,688/524,288 octets. Le pic heap mesure est 227,328 octets et la profondeur de pile maximale 2,888 octets, tous deux bien dans la reserve de 797,072 octets definie par `InitHeap`. Le temps d'image n'est pas référencé dans ce rapport (sonde disponible depuis v0.8.6). Le pic heap par zone est mesuré à 225,280 octets pour la zone 011 (section 5), avec retour exact au baseline. Le pic VRAM dynamique n'est pas référencé dans ce rapport. Le pic SPU dynamique n'est pas référencé dans ce rapport. 
