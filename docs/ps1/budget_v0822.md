# Budgets PS1 — mesures sur la version livrée

ELF mesuré : `ps1\a_graver\v0.8.22-ps1-spu-peak\alundra2.elf`
ISO mesurée : `ps1\a_graver\v0.8.22-ps1-spu-peak\alundra2_ntsc.iso`

Reproduction : `py -3.12 tools\ps1_budget_report.py ps1\a_graver\v0.8.22-ps1-spu-peak\alundra2.elf --iso ps1\a_graver\v0.8.22-ps1-spu-peak\alundra2_ntsc.iso --out docs\ps1\budget_v0822.md --diag-log qa\ps1_logs\harness_v0822\diag\diag_capture.txt --timing-log qa\ps1_logs\harness_v0822\timing\timing_stdout.txt --zone-heap-log qa\ps1_logs\harness_v0822\zone_heap\zone_heap_capture.txt --vram-log qa\ps1_logs\harness_v0822\vram\vram_capture.txt --spu-log qa\ps1_logs\spu_v0822\spu_capture.txt`

- SHA256 ELF : `2F8BCA0E3B8962CE6AD5F01FBDEA04031ED039322DFD63D696A9F88ADE144CA7`
- SHA256 ISO : `26B2228165B70922A121FDF6C2E33F979C1BB94D45B513E0B310B4496960BE54`

Toutes les valeurs ci-dessous sont des **mesures** obtenues par les commandes
indiquées. Les quantités non instrumentées sont signalées explicitement.

## 1. RAM principale — limite 2 097 152 octets

Commande : `mipsel-none-elf-size -A <ELF>` (sections chargées uniquement).

| Section | Octets mesurés |
|---|---:|
| `.MIPS.abiflags` | 24 |
| `.bss` | 318,492 |
| `.ctors` | 8 |
| `.data` | 1,285,724 |
| `.dtors` | 8 |
| `.rodata` | 99,023 |
| `.sbss` | 468 |
| `.sdata` | 74 |
| `.text` | 91,328 |
| **Total chargé (.text → .bss)** | **1,795,149** |

- Dépassement : **aucun** ; marge **302,003 octets** (85.6 % du budget).
- Le ring de streaming `0x18000` = 98,304 octets est inclus dans `.bss` (mesure confirmée par `nm` : `s_ring`).
- **Mesuré** : pic heap `HeapUsage.alloc_max` du SDK PSn00bSDK = **6,144 octets** (allocation courante finale 6,144 octets sur un tas de 229,376 octets). Un pic à 0 signifierait « aucun appel d'allocation observé », pas une estimation.
- **Mesuré** : profondeur de pile MIPS maximale = **2,904 octets** (plancher `0x801ff3b8`, sommet `0x801fff10`). Méthode : peinture `0xA5A5A5A5` d'une fenêtre de 256 Kio au plus, sous le pointeur de pile réel et au-dessus du sommet de l'arène `malloc`, garde de 512 octets, balayage toutes les 60 images. Une pile très superficielle peut être sous-estimée de cette garde.
- Relevé après **2,315** appels à `diagSamplePS1()`, le 2026-11-24T11:26:38, log `qa\ps1_logs\harness_v0822\diag\diag_capture.txt`, ISO SHA256 `26B2228165B70922A121FDF6C2E33F979C1BB94D45B513E0B310B4496960BE54`.
- Commande de capture : `C:\Users\Liori\Alundra\external\tools\pcsx-redux\pcsx-redux.main -testmode -stdout -lua_stdout -bios C:\Users\Liori\AppData\Local\DuckStation\bios\openbios.bin -iso C:\Users\Liori\Alundra\ps1\a_graver\v0.8.22-ps1-spu-peak\alundra2_ntsc.iso -fastboot -run -debugger -interpreter -portable -noupdate -dofile C:\Users\Liori\Alundra\docs\ps1\lua\capture_diag.lua`

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
- CD : 40 pistes VAG, 49,698,816 octets ; ISO livrée 53,809,152 octets (`tools/ps1_make_iso.py`). Le flux n'occupe pas la RAM SPU au-delà du double buffer.
- Le BIN MODE2/2352 du lot fait 61,796,448 octets (secteurs avec en-tête et parité) ; ce n'est **pas** l'image gravable. L'ISO 2048 livrée est convertie depuis ce BIN et constitue la taille de référence pour le mastering.

## 3b. Pic SPU pendant le streaming CD — mesuré (table d'occupation instrumentée)

Méthode : un bit par bloc SPU de 8 octets (8 192 octets de BSS) est mis à jour après les deux seuls `SpuWrite` du port (banque VAG et double buffer musical). Le POD `g_ps1SpuDiag` est lu passivement par Lua aux phases menu, gameplay et inventaire. L'occupation est une union : réécrire un bloc ne double pas son coût.

| Phase | `transfers` | `chunks` | Chevauchements (octets) | Occupation (octets) | Pic (octets) |
|---|---:|---:|---:|---:|---:|
| menu | 3 | 0 | 0 | 394688 | 394688 |
| game | 7 | 6 | 32768 | 394688 | 394688 |
| inventory | 7 | 6 | 32768 | 394688 | 394688 |

- **Mesuré** : pic SPU global = **394,688 octets** sur 524 288 (75.3 %). `peakFrame=0` : le préfill CD synchrone du boot atteint ce pic avant la première image présentée ; il n'est jamais dépassé pendant le gameplay.
- **Mesuré** : banque VAG = **378,304 octets**, double buffer musical = **16,384 octets**, soit exactement le pic ; 7 transferts acceptés, 6 chunks de flux, 0 underrun, 0 rejet, 32,768 octets réécrits en union.
- **Preuve de streaming** : `chunks>=2`, `streamBytes==16384`, `transfers>=3` et `underruns==0` sont vérifiés sur la phase finale ; la mesure couvre donc le rendu **et** le streaming CD, pas seulement le chargement initial.
- Le calcul statique additionne la banque VAG residente et le double buffer musical, soit **394,688 octets** ; il ne mesure pas les ecritures reelles ni leurs chevauchements. Ici le pic mesuré coïncide avec le calcul statique, mais il est désormais **observé** sur l'ISO livrée : les deux valeurs ne se substituent pas.
- Relevé le 2026-11-24T11:24:56, log `qa\ps1_logs\spu_v0822\spu_capture.txt`, ISO SHA256 `26B2228165B70922A121FDF6C2E33F979C1BB94D45B513E0B310B4496960BE54`.
- Commande de capture : `C:\Users\Liori\Alundra\external\tools\pcsx-redux\pcsx-redux.main -testmode -stdout -lua_stdout -bios C:\Users\Liori\AppData\Local\DuckStation\bios\openbios.bin -iso C:\Users\Liori\Alundra\ps1\a_graver\v0.8.22-ps1-spu-peak\alundra2_ntsc.iso -fastboot -run -debugger -interpreter -portable -noupdate -dofile C:\Users\Liori\Alundra\docs\ps1\lua\capture_spu.lua`

## 4. Temps d'image — mesuré (ticks RCnt bruts)

- **Mesuré** : 1000 périodes entre deux `VSync(0)`, lues par accès direct au compteur racine 1 (`0x1f801110`, `src/ps1/pf_ps1.cpp`). ticks min/p50/p95/pic = **18061 / 18144 / 18208 / 60007**, moyenne 18188.7.
- **Conversion nominale** (taux RCntCNT1 1,053497 MHz, non calibré ici — jamais une mesure horloge indépendante) : ms min/p50/p95/pic = 17.144 / 17.223 / 17.283 / 56.960, moyenne 17.265. Cible : 16,67 ms (NTSC 60 Hz).
- Outlier conservé, sans filtrage : `outliers_gt_20000=1`, `max_index=1`. Sa cause précise n'est pas établie par cette mesure.
- Relevé le 2026-11-24T11:29:23, log `qa\ps1_logs\harness_v0822\timing\timing_stdout.txt`.
- Commande de capture : `C:\Users\Liori\Alundra\external\tools\pcsx-redux\pcsx-redux.main -testmode -stdout -lua_stdout -bios C:\Users\Liori\AppData\Local\DuckStation\bios\openbios.bin -iso qa\ps1_logs\harness_v0822\timing\alundra2_ntsc.iso -fastboot -run -debugger -interpreter -portable -noupdate -dofile C:\Users\Liori\Alundra\docs\ps1\lua\frame_timing.lua`

## 5. Pic RAM par zone — mesuré (capture CD sur l'ISO livrée)

Méthode : le harnais capture-only lit chaque blob P1ZM depuis le CD dans l'arene `malloc`, appelle le parseur de zone existant, libere le blob avant l'allocation suivante, et releve `HeapUsage.alloc` / `alloc_max` du SDK. Le jeu normal garde ses blobs `incbin` et n'entre pas dans ce chemin.

| Etape | Zone | `alloc` mesuré (octets) | `alloc_max` SDK (octets) | Octets lus | Liberations |
|---:|---:|---:|---:|---:|---:|
| 1 | 013 | 159744 | 159744 | 154636 | 1 |
| 2 | 026 | 106496 | 161792 | 102282 | 2 |
| 3 | 011 (retour) | 225280 | 225280 | 220006 | 3 |
| 4 | liberation finale | 225280 | 225280 | 220006 | 3 |

- **Mesuré** : hauteur d'eau initiale (`baselineAlloc`) = **4096 octets** ; retour final (`finalAlloc`) = **4096 octets**. Egalité requise : aucun octet de zone n'est conservé après la séquence.
- **Mesuré** : `alloc_max` est un compteur monotone du SDK ; le pic global de la capture est **225280 octets**. La plus grande zone seule atteint **225280 octets** (zone 011, 220006 octets lus).
- **Mesuré** : `staticRam` = **1860760 octets** (adresse de fin de segments charges, limite superieure de l'arene).
- Sequence reelle : 011 (zone initiale du jeu, etat baseline) → 013 → 026 → 011 (blob CD relu) → liberation explicite. Aucune valeur n'est estimee.
- Releve le 2026-11-24T11:27:33, log `qa\ps1_logs\harness_v0822\zone_heap\zone_heap_capture.txt`, ISO SHA256 `26B2228165B70922A121FDF6C2E33F979C1BB94D45B513E0B310B4496960BE54`.
- Commande de capture : `C:\Users\Liori\Alundra\external\tools\pcsx-redux\pcsx-redux.main -testmode -stdout -lua_stdout -bios C:\Users\Liori\AppData\Local\DuckStation\bios\openbios.bin -iso C:\Users\Liori\Alundra\ps1\a_graver\v0.8.22-ps1-spu-peak\alundra2_ntsc.iso -fastboot -run -debugger -interpreter -portable -noupdate -dofile C:\Users\Liori\Alundra\docs\ps1\lua\capture_zone_heap.lua`

## 6. Pic VRAM pendant le rendu — mesuré (table d'allocation instrumentée)

Méthode : un bit par demi-mot VRAM (65 536 octets de BSS) est mis à jour dans `pfUpload4bpp` / `pfUpload16`, avec les deux framebuffers réservés dans `pfInit`. Le POD `g_ps1VramDiag` est lu passivement par Lua aux phases menu, gameplay et inventaire. L'occupation est une union : réécrire un rectangle ne double pas son coût.

| Phase | `uploads` | Chevauchements (demi-mots) | Occupation (octets) | Pic (octets) |
|---|---:|---:|---:|---:|
| menu | 1541 | 0 | 564896 | 564896 |
| game | 4615 | 216512 | 592928 | 592928 |
| inventory | 4666 | 250112 | 594272 | 594272 |

- **Mesuré** : pic VRAM global = **594,272 octets** sur 1 048 576 (56.7 %), atteint à la frame 2132.
- **Mesuré** : framebuffers = **307,200 octets** ; pic texture au-dessus des framebuffers = **287,072 octets**.
- **Mesuré** : 4666 uploads acceptés, 250112 demi-mots réécrits, 0 rejet ; somme de contrôle du POD vérifiée à chaque phase.
- Le pic statique de la section 2 additionne des rectangles réservés et le plus grand blob P1ZM : il ne mesure pas les remplacements ni les uploads réels. Les deux valeurs sont distinctes et ne se substituent pas.
- Relevé le 2026-11-24T11:28:34, log `qa\ps1_logs\harness_v0822\vram\vram_capture.txt`, ISO SHA256 `26B2228165B70922A121FDF6C2E33F979C1BB94D45B513E0B310B4496960BE54`.
- Commande de capture : `C:\Users\Liori\Alundra\external\tools\pcsx-redux\pcsx-redux.main -testmode -stdout -lua_stdout -bios C:\Users\Liori\AppData\Local\DuckStation\bios\openbios.bin -iso C:\Users\Liori\Alundra\ps1\a_graver\v0.8.22-ps1-spu-peak\alundra2_ntsc.iso -fastboot -run -debugger -interpreter -portable -noupdate -dofile C:\Users\Liori\Alundra\docs\ps1\lua\capture_vram.lua`

## Conclusion

Les trois budgets mesurables sont respectés : RAM 1,795,149/2,097,152, VRAM 641,604/1,048,576, SPU 394,688/524,288 octets. Le pic heap mesure est 6,144 octets et la profondeur de pile maximale 2,904 octets, tous deux bien dans la reserve de 229,376 octets definie par `InitHeap`. Le temps d'image est mesuré en ticks RCnt dans la section 4. Le pic heap par zone est mesuré à 225,280 octets pour la zone 011 (section 5), avec retour exact au baseline. Le pic VRAM réel pendant le rendu est mesuré à 594,272 octets (section 6), distinct du pic statique. Le pic SPU réel pendant le streaming CD est mesuré à 394,688 octets (section 3b), avec preuve de streaming. 
