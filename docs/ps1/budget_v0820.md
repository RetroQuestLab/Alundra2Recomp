# Budgets PS1 — mesures sur la version livrée

ELF mesuré : `ps1\a_graver\v0.8.20-ps1-zone-heap\alundra2.elf`
ISO mesurée : `ps1\a_graver\v0.8.20-ps1-zone-heap\alundra2_ntsc.iso`

Reproduction : `py -3.12 tools\ps1_budget_report.py ps1\a_graver\v0.8.20-ps1-zone-heap\alundra2.elf --iso ps1\a_graver\v0.8.20-ps1-zone-heap\alundra2_ntsc.iso --out docs\ps1\budget_v0820.md --diag-log qa\ps1_logs\diag_v0820\diag_capture.txt --timing-log qa\ps1_logs\frame_timing_v0820\timing_stdout.txt --zone-heap-log qa\ps1_logs\zone_heap_v0820\zone_heap_capture.txt`

- SHA256 ELF : `48133168F070ECB1CC4D8FD4F4EBF5C310B814DCD14A5F1421863EAF43A1A473`
- SHA256 ISO : `AE86E35F94559050F7087D74B84B10EA5FB296A1E442BBEF08A314F198FF1B76`

Toutes les valeurs ci-dessous sont des **mesures** obtenues par les commandes
indiquées. Les quantités non instrumentées sont signalées explicitement.

## 1. RAM principale — limite 2 097 152 octets

Commande : `mipsel-none-elf-size -A <ELF>` (sections chargées uniquement).

| Section | Octets mesurés |
|---|---:|
| `.MIPS.abiflags` | 24 |
| `.bss` | 244,636 |
| `.ctors` | 8 |
| `.data` | 1,285,724 |
| `.dtors` | 8 |
| `.rodata` | 98,959 |
| `.sbss` | 464 |
| `.sdata` | 74 |
| `.text` | 89,548 |
| **Total chargé (.text → .bss)** | **1,719,445** |

- Dépassement : **aucun** ; marge **377,707 octets** (82.0 % du budget).
- Le ring de streaming `0x18000` = 98,304 octets est inclus dans `.bss` (mesure confirmée par `nm` : `s_ring`).
- **Mesuré** : pic heap `HeapUsage.alloc_max` du SDK PSn00bSDK = **6,144 octets** (allocation courante finale 6,144 octets sur un tas de 262,144 octets). Un pic à 0 signifierait « aucun appel d'allocation observé », pas une estimation.
- **Mesuré** : profondeur de pile MIPS maximale = **2,904 octets** (plancher `0x801ff3b8`, sommet `0x801fff10`). Méthode : peinture `0xA5A5A5A5` d'une fenêtre de 256 Kio au plus, sous le pointeur de pile réel et au-dessus du sommet de l'arène `malloc`, garde de 512 octets, balayage toutes les 60 images. Une pile très superficielle peut être sous-estimée de cette garde.
- Relevé après **2,312** appels à `diagSamplePS1()`, le 2026-11-24T09:57:54, log `qa\ps1_logs\diag_v0820\diag_capture.txt`, ISO SHA256 `AE86E35F94559050F7087D74B84B10EA5FB296A1E442BBEF08A314F198FF1B76`.
- Commande de capture : `C:\Users\Liori\Alundra\external\tools\pcsx-redux\pcsx-redux.main -testmode -stdout -lua_stdout -bios C:\Users\Liori\AppData\Local\DuckStation\bios\openbios.bin -iso C:\Users\Liori\Alundra\ps1\a_graver\v0.8.20-ps1-zone-heap\alundra2_ntsc.iso -fastboot -run -debugger -interpreter -portable -noupdate -dofile C:\Users\Liori\Alundra\docs\ps1\lua\capture_diag.lua`

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
- CD : 40 pistes VAG, 49,698,816 octets ; ISO livrée 53,807,104 octets (`tools/ps1_make_iso.py`). Le flux n'occupe pas la RAM SPU au-delà du double buffer.
- Le BIN MODE2/2352 du lot fait 61,794,096 octets (secteurs avec en-tête et parité) ; ce n'est **pas** l'image gravable. L'ISO 2048 livrée est convertie depuis ce BIN et constitue la taille de référence pour le mastering.

## 4. Temps d'image — mesuré (ticks RCnt bruts)

- **Mesuré** : 1000 périodes entre deux `VSync(0)`, lues par accès direct au compteur racine 1 (`0x1f801110`, `src/ps1/pf_ps1.cpp`). ticks min/p50/p95/pic = **18063 / 18143 / 18216 / 60051**, moyenne 18816.4.
- **Conversion nominale** (taux RCntCNT1 1,053497 MHz, non calibré ici — jamais une mesure horloge indépendante) : ms min/p50/p95/pic = 17.146 / 17.222 / 17.291 / 57.002, moyenne 17.861. Cible : 16,67 ms (NTSC 60 Hz).
- Outlier conservé, sans filtrage : `outliers_gt_20000=16`, `max_index=958`. Sa cause précise n'est pas établie par cette mesure.
- Relevé le 2026-11-24T09:59:47, log `qa\ps1_logs\frame_timing_v0820\timing_stdout.txt`.
- Commande de capture : `C:\Users\Liori\Alundra\external\tools\pcsx-redux\pcsx-redux.main -testmode -stdout -lua_stdout -bios C:\Users\Liori\AppData\Local\DuckStation\bios\openbios.bin -iso ps1\a_graver\v0.8.20-ps1-zone-heap\alundra2_ntsc.iso -fastboot -run -debugger -interpreter -portable -noupdate -dofile C:\Users\Liori\Alundra\docs\ps1\lua\frame_timing.lua`

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
- **Mesuré** : `staticRam` = **1785044 octets** (adresse de fin de segments charges, limite superieure de l'arene).
- Sequence reelle : 011 (zone initiale du jeu, etat baseline) → 013 → 026 → 011 (blob CD relu) → liberation explicite. Aucune valeur n'est estimee.
- Releve le 2026-11-24T09:58:53, log `qa\ps1_logs\zone_heap_v0820\zone_heap_capture.txt`, ISO SHA256 `AE86E35F94559050F7087D74B84B10EA5FB296A1E442BBEF08A314F198FF1B76`.
- Commande de capture : `C:\Users\Liori\Alundra\external\tools\pcsx-redux\pcsx-redux.main -testmode -stdout -lua_stdout -bios C:\Users\Liori\AppData\Local\DuckStation\bios\openbios.bin -iso C:\Users\Liori\Alundra\ps1\a_graver\v0.8.20-ps1-zone-heap\alundra2_ntsc.iso -fastboot -run -debugger -interpreter -portable -noupdate -dofile C:\Users\Liori\Alundra\docs\ps1\lua\capture_zone_heap.lua`

## Conclusion

Les trois budgets mesurables sont respectés : RAM 1,719,445/2,097,152, VRAM 641,604/1,048,576, SPU 394,688/524,288 octets. Le pic heap mesure est 6,144 octets et la profondeur de pile maximale 2,904 octets, tous deux bien dans la reserve de 262,144 octets definie par `InitHeap`. Le temps d'image est mesuré en ticks RCnt dans la section 4. Le pic heap par zone est mesuré à 225,280 octets pour la zone 011 (section 5), avec retour exact au baseline. 
