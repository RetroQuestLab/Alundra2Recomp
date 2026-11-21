# PS1 launch menu — design document (2026-11-21)

## Objective

Add, **before the next burnable ISO**, a native PS1 title screen and main menu:
`Continuer`, `Nouvelle partie`, `Options`, `Quitter`. The title bears the
**Alundra 2** label. The prototype remains limited to zone 011 (movement,
running, jumping, attacking, camera, 4-heart HUD): no dialogue, NPC, chest, item,
music or zone change is added.

## Design decisions

- **Full PS1** approach: `Continuer` relies on a real memory-card save.
- Title = **Alundra 2**; rendering left to our discretion → Alundra 1 presentation
  (`FONT3.TIM` font, `WIND`-style frames), A2 French text.
- `Quitter` = farewell screen (« Merci d'avoir joué — éteignez la console »), no
  soft-reset.
- `Continuer` is **grayed out** until a valid save exists.

## Current state

- The PC already has `src/title.cpp` (same labels), but it depends on SDL,
  zones, file saving and audio: not portable as-is.
- PS1 currently boots straight into `main()` → `zoneInit()` /
  `heroInit()`. There is no title screen or save.
- `pf.h` / `pf_ps1.cpp` provide opaque rectangles, 4bpp sprites, PAD and
  double buffering; `SetVideoMode(MODE_NTSC)` is currently hard-coded in `pfInit()`.
- PSn00bSDK exposes `_card_read/_card_write` (128-byte sectors), `InitCARD`,
  `StartCARD`, `_card_info`, `_card_status`, `_card_wait`, `_card_clear`.
- The A1 font is already generated as 1 bpp in `src/a1_font.h` (256 16×16 glyphs,
  index = Latin-1 byte) but this C++ header (`std::array`) cannot be compiled
  by the MIPS toolchain.

## Architecture

### State machines

`BOOT → TITLE → (GAME | OPTIONS | FAREWELL)`

- `TITLE`: background = zone 011 in a slow pan, semi-transparent dark overlay,
  « ALUNDRA 2 » title in golden A1 font, 4-item menu.
- `GAME`: current loop unchanged; **Select** saves then returns to the title
  (required so `Continuer` can be observed). **Start** saves with a temporary
  « SAUVEGARDÉ » indicator.
- `OPTIONS`: `Vidéo : NTSC (60 Hz) / PAL (50 Hz)`, `Commandes`, `Retour`.
  The video mode change is **saved and applied on next launch**
  (before `pfInit()`): no risky GPU reset mid-game.
- `FAREWELL`: night-blue screen, A1 text, loop frozen after `pfPresent()`.

### Files

| File | Role |
|---|---|
| `src/ps1/menu_ps1.h/.cpp` | TITLE/OPTIONS/FAREWELL states, up/down/Cross/Circle/Back input, rendering |
| `src/ps1/save_ps1.h/.cpp` | card init, read/write of the two records, CRC, validity detection |
| `src/ps1/a1_font_ps1.h` | generated 4bpp atlas (256 glyphs) + 4 CLUTs (gold, white, selection cyan, grayed out) |
| `tools/ps1_export_font.py` | generates the atlas from `src/a1_font.h`, frozen manifest |
| `src/platform/pf.h`, `src/ps1/pf_ps1.cpp` | `pfDrawRectSemi()`, `pfSetVideoMode()`, `pfPresent()` otherwise unchanged |
| `src/ps1/hero_ps1.h/.cpp` | `heroSetPos(x,y)` for `Continuer` |
| `src/ps1/boot.cpp` | BOOT/TITLE/GAME sequence, options loaded before `pfInit()` |
| `ps1/CMakeLists.txt` | new sources added |
| `tools/ps1_export_zone.py` + test | reserve the VRAM area for the atlas and CLUTs |
| `docs/ps1/lua/capture_menu.lua` | native 320×240 captures of each screen |
| `docs/ps1/premiere_zone_jouable.md` | scope |

### VRAM

Font atlas: `x=0..127` halfwords, `y=256..511` (256×256 px 4bpp = ~128 Ko).
CLUT: `x=128..143`, `y=256` (4 banks of 16 colors). Hero scratch unchanged
(`512,256,128,64`). These two rectangles are added to `RESERVED_RECTS` so that
`ps1_export_zone.py` does not overwrite them. Zone tiles fit in the remaining
space (~470 Ko for ~315 Ko of blobs).

### Memory-card save

A single 8 Ko block (64 sectors of 128 bytes), standard BIOS format:

- Block 0 (directory): entry `0x51`, size `0x2000`, name `ALUNDRA2PS1`,
  XOR checksum; only one free block is claimed, other saves untouched.
- Data block: sector 0 = `SC`, icon flag `0x11`, title `ALUNDRA 2`.
- Sectors 4 and 5: two copies of the game record —
  magic `A2SV`, version `u16`, zone `u8`, `x/y` `s16`, hearts `u8`, CRC16.
- Sectors 6 and 7: two copies of the options — magic `A2OP`, version, video mode,
  CRC16.

Write: data first, directory entry last. Read: any magic/version/CRC mismatch
makes the record invalid → `Continuer`
grayed out. If the card is absent or unformatted, the menu remains playable and `Continuer`
is grayed out; a discreet message indicates this in `Options`.

### Inputs

| Button | Title | Options | Game |
|---|---|---|---|
| Up/Down | move selection | move selection | — |
| Left/Right | — | adjust video | — |
| Cross | confirm | confirm / cycle | jump (unchanged) |
| Circle | back (from options) | back | — |
| Select | — | — | save + return to title |
| Start | — | — | quick save |

`pfPad()` already provides an active-high mask; no polarity change.

### Validation

1. `py -3.12 tools/ps1_export_zone_test.py` (must remain 14/14, including
   `test_font_vram_reserved`).
2. `powershell -ExecutionPolicy Bypass -File tools/build_ps1.ps1` → `PS1_BUILD: PASS`.
3. Native captures via `capture_menu.lua`: title, options, grayed-out, active save,
   return to title, farewell screen; 320×240 PNG conversion; real visual
   inspection.
4. Memory-card test: write, **emulator power-off**, restart, read →
   `Continuer` active and position restored.
5. `tools/ps1_smoke.ps1` (ELF addresses re-read), then the four canonical PC
   commands and `git diff --check`.

## Out of scope

Dialogues, dialogue font, full WIND, NPCs, chests, items, music,
multi-slot saves, zone changes, PC (which already has its own menu).
No push, no publication of assets or ISO.
