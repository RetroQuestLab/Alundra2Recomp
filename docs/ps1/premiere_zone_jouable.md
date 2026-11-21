# Design doc — first playable version on PS1 (slice 1)

Date: 2026-11-21.
Selected option: **fast playable prototype** (approach 1), not the full faithful slice.

## Objective

Provide a burnable NTSC/60 Hz ISO containing a real game start: zone 011 scenery
(Paco Village), a hero controllable with the Pad, camera, jump, attack and health HUD. The SDL3 PC
target remains unchanged.

## Scope

**Included**
- A1 scenery from `assets/maps/011.a1m`: real 24×16 tiles, stacked walls, cell heights.
- A1 hero from `assets/sprites/hero.a1s`: idle, walk, run, jump, attack (groups 0,1,3,2,16).
- 8-direction movement**, running (R1), jumping (Cross), attacking (Square), collisions identical to `canStand()`.
- Integer camera following the hero, bounded to the map.
- HUD: hearts in the top left reflecting the hero's health; minimal text « ZONE 011 ».
- ISO 2048 + BIN/CUE output, `MODE_NTSC` forced, CD boot verified in emulator.

**Excluded from this slice** (later slices of the plan)
- Dialogues/A1 font/WIND windows, NPCs, chests, items, music, save, zone changes.

## Constraints

- C++20 MIPS **without STL, without dynamic allocation**, PSn00bSDK 0.24 gcc.
- VRAM 1 Mo: 2 framebuffers 320×240×2 = 307 200 o, 4bpp tiles ≈ 157 632 o,
  4bpp hero ≈ 64 352 o, CLUT ≈ 26 ko → ~555 ko, comfortable margin.
- RAM 2 Mo: static data embedded in the EXE via `.incbin` (no CD reader to write).
- OT per scanline (64 entries), `SPRT_16` primitives for tiles/sprites, `TILE` for the HUD.
- 4bpp texel with index 0 = transparent (PS1 GPU constraint), so A1 alpha → index 0.

## Data pipeline (offline)

New tool `tools/ps1_export_zone.py` (unpublished):
1. Reads `A1MP` ( Built-in ): 821 24×16 RGBA tiles, 3 120 cells, 8 202 walls.
2. Quantizes each tile to **16 colors max** (alpha=0 → index 0), deduplicates palettes
   into 16×BGR555 CLUTs.
3. Reads `A1SP` (`hero.a1s`): keeps only groups [0 idle, 1 walk, 3 run, 6 jump, 2 sword]
   (`src/sprite.h`: `IDLE, WALK, RUN, JUMP, ATTACK`), quantizes each frame to 4bpp + deduplicated CLUT.
   The old choice [0, 1, 3, 2, 16] used the ATTACK group as jump and group 16 (empty) as sword.
   A1SP directions are `0=up, 1=left, 2=right, 3=down` (see `src/platform/pf.h`).
4. Writes `ps1/assets/zone011.dat` and `ps1/assets/hero.dat` (P1ZM / P1SP formats detailed in the plan).

## Execution (PS1)

- `src/ps1/pf_ps1.cpp` extended: `pfUpload4bpp()`, `pfTexPage()`, `pfDrawSprite16()`, OT 64.
- `src/ps1/zone011.cpp`: P1ZM loader + tile/wall drawing with integer `camX/camY`.
- `src/ps1/hero_ps1.cpp`: animation (1/60 ticks), physics entirely in 16.16 fixed-point
  (no FPU guaranteed), `canStand` ported to cells.
- `src/ps1/boot.cpp` replaced by the game loop; diagnostics still available via `__PS1_DIAG__`.
- Inputs: arrows/Pad, Cross = jump, Square = attack, R1 = run; same rising edges as PC.

## Verification

1. `powershell -File tools/build_ps1.ps1` → `PS1_BUILD: PASS`.
2. `powershell -File tools/ps1_smoke.ps1` → CD boot and hits `_start/main/pfInit/pfPresent/pfDrawRect`.
3. Native 320×240 capture via `docs/ps1/lua/capture_boot.lua`, real visual inspection.
4. Boot of the ISO 2048 in PCSX-Redux (timestamped log in `qa/ps1_logs/`).
5. PC regression: build OK, validator `ERR: 0 | WARN: 0`, CTest 81/81, `git diff --check`.
6. No binary file or image commit; only the local version in `ps1/a_graver/`.

## Deliverable

`ps1/a_graver/alundra2_ntsc.iso` regenerated, burnable, with `SHA256.txt` updated.
