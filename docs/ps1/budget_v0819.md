# Budgets PS1 — mesures sur la version livrée

ELF mesuré : `ps1\a_graver\v0.8.19-ps1-heap-stack-diag\alundra2.elf`
ISO mesurée : `ps1\a_graver\v0.8.19-ps1-heap-stack-diag\alundra2_ntsc.iso`

Reproduction : `py -3.12 tools\ps1_budget_report.py ps1\a_graver\v0.8.19-ps1-heap-stack-diag\alundra2.elf --iso ps1\a_graver\v0.8.19-ps1-heap-stack-diag\alundra2_ntsc.iso --out docs\ps1\budget_v0819.md --diag-log qa\ps1_logs\harness_v0819\diag_capture.txt --timing-log qa\ps1_logs\frame_timing_v0819\timing_stdout.txt`

- SHA256 ELF : `77094EEEB049764F356178BAF0594F3C3229E3389C13B4299107D3E6B80B534F`
- SHA256 ISO : `5AC2E4D4AD76052E530889D19C205C7FEBD09E894D1EBBF6EF7D346A0BBCC07D`

Toutes les valeurs ci-dessous sont des **mesures** obtenues par les commandes
indiquées. Les quantités non instrumentées sont signalées explicitement.

## 1. RAM principale — limite 2 097 152 octets

Commande : `mipsel-none-elf-size -A <ELF>` (sections chargées uniquement).

| Section | Octets mesurés |
|---|---:|
| `.MIPS.abiflags` | 24 |
| `.bss` | 244,572 |
| `.ctors` | 8 |
| `.data` | 1,285,724 |
| `.dtors` | 8 |
| `.rodata` | 98,899 |
| `.sbss` | 460 |
| `.sdata` | 74 |
| `.text` | 88,104 |
| **Total chargé (.text → .bss)** | **1,717,873** |

- Dépassement : **aucun** ; marge **379,279 octets** (81.9 % du budget).
- Le ring de streaming `0x18000` = 98,304 octets est inclus dans `.bss` (mesure confirmée par `nm` : `s_ring`).
- **Mesuré** : pic heap `HeapUsage.alloc_max` du SDK PSn00bSDK = **6,144 octets** (allocation courante finale 6,144 octets sur un tas de 313,704 octets). Un pic à 0 signifierait « aucun appel d'allocation observé », pas une estimation.
- **Mesuré** : profondeur de pile MIPS maximale = **3,140 octets** (plancher `0x801ff3b8`, sommet `0x801ffffc`). Méthode : peinture `0xA5A5A5A5` d'une fenêtre de 256 Kio sous le pointeur de pile initial, garde de 512 octets, balayage toutes les 60 images. Une pile très superficielle peut être sous-estimée de cette garde.
- Relevé après **2,303** appels à `diagSamplePS1()`, le 2026-11-24T09:03:12, log `qa\ps1_logs\harness_v0819\diag_capture.txt`, ISO SHA256 `5AC2E4D4AD76052E530889D19C205C7FEBD09E894D1EBBF6EF7D346A0BBCC07D`.
- Commande de capture : `C:\Users\Liori\Alundra\external\tools\pcsx-redux\pcsx-redux.main -testmode -stdout -lua_stdout -bios C:\Users\Liori\AppData\Local\DuckStation\bios\openbios.bin -iso C:\Users\Liori\Alundra\ps1\a_graver\v0.8.19-ps1-heap-stack-diag\alundra2_ntsc.iso -fastboot -run -debugger -interpreter -portable -noupdate -dofile C:\Users\Liori\Alundra\docs\ps1\lua\capture_diag.lua`

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
- CD : 40 pistes VAG, 49,698,816 octets ; ISO livrée 53,325,824 octets (`tools/ps1_make_iso.py`). Le flux n'occupe pas la RAM SPU au-delà du double buffer.
- Le BIN MODE2/2352 du lot fait 61,241,376 octets (secteurs avec en-tête et parité) ; ce n'est **pas** l'image gravable. L'ISO 2048 livrée est convertie depuis ce BIN et constitue la taille de référence pour le mastering.

## 4. Temps d'image — mesuré (ticks RCnt bruts)

- **Mesuré** : 1000 périodes entre deux `VSync(0)`, lues par accès direct au compteur racine 1 (`0x1f801110`, `src/ps1/pf_ps1.cpp`). ticks min/p50/p95/pic = **12541 / 18145 / 18206 / 60004**, moyenne 18100.0.
- **Conversion nominale** (taux RCntCNT1 1,053497 MHz, non calibré ici — jamais une mesure horloge indépendante) : ms min/p50/p95/pic = 11.904 / 17.224 / 17.281 / 56.957, moyenne 17.181. Cible : 16,67 ms (NTSC 60 Hz).
- Outlier conservé, sans filtrage : `outliers_gt_20000=1`, `max_index=1`. Sa cause précise n'est pas établie par cette mesure.
- Relevé le 2026-11-24T09:19:39, log `qa\ps1_logs\frame_timing_v0819\timing_stdout.txt`.
- Commande de capture : `C:\Users\Liori\Alundra\external\tools\pcsx-redux\pcsx-redux.main -testmode -stdout -lua_stdout -bios C:\Users\Liori\AppData\Local\DuckStation\bios\openbios.bin -iso ps1\a_graver\v0.8.19-ps1-heap-stack-diag\alundra2_ntsc.iso -fastboot -run -debugger -interpreter -portable -noupdate -dofile C:\Users\Liori\Alundra\docs\ps1\lua\frame_timing.lua`

## Conclusion

Les trois budgets mesurables sont respectés : RAM 1,717,873/2,097,152, VRAM 641,604/1,048,576, SPU 394,688/524,288 octets. Le pic heap mesure est 6,144 octets et la profondeur de pile maximale 3,140 octets, tous deux bien dans la reserve de 313,704 octets definie par `InitHeap`. Le temps d'image est mesuré en ticks RCnt dans la section 4. 
