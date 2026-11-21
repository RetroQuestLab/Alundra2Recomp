# Portability audit — native PlayStation 1

Date: 2026-11-21. Scope: **static, read-only** audit of the current PC codebase. No source file, data file or build was modified; only this report was created. No build, validation or CTest command was run during this audit.

Target: **native MIPS PS-X EXE** + **bootable BIN/CUE**, real GPU/SPU/PAD/memory card via **PSn00bSDK v0.24**, unchanged SDL3 PC as the non-regression reference. Budgets: **2 Mo RAM**, **1 Mo VRAM**, **512 Ko audio**, per-zone loading. Milestone 1 toolchain already verified: SDK template → ELF/PS-X/BIN/CUE, DuckStation + OpenBIOS boot at 59,83 FPS.

## Requested summary (12 lines)

1. The current architecture is **monolithic and SDL3-native**: `game.h` exposes `SDL_Window`, `SDL_Renderer`, `SDL_Gamepad`, `SDL_AudioStream`, `SDL_Color` and `std::filesystem` up into the gameplay structures.
2. Rendering is entirely SDL3 (RGBA8888 textures, blending, tiling, rotation, `SDL_RenderDebugText`, BMP capture); it must be replaced by a PS1 renderer using GPU primitives/ordering tables/TIM/CLUT.
3. The A1 font and WIND windows are already generated as RGBA via SDL (`dialogue.cpp:59-115`): conversion to 4 bpp + VRAM CLUT is feasible, but must be centralized.
4. Input is SDL (`SDL_Event`, scancodes, SDL gamepad): a PAD service must reproduce keyboard/controller mappings and `--auto` tests exactly.
5. The loop uses `SDL_GetTicks` with a fixed 1/60 s step (`main.cpp:1232-1243`): on PS1, VSync/DrawSync and a deterministic counter are needed, with NTSC/PAL care.
6. Audio is a **44,1 kHz stereo software mixer** in an SDL callback (`audio.cpp:14-63`): this is the most expensive part to port to an FPU-less R3000A.
7. Current WAV banks total ~2,54 Go (music_a2 1,42 Go, music 662 Mo, sfx 13 Mo): impossible within 512 Ko audio; VAG/ADPCM, CD streaming or sequencing are mandatory.
8. Data loading relies on `std::filesystem`, `directory_iterator`, `ifstream` and editable text files (`world.cpp`, `items.cpp`, `audio.cpp:176-194`): the PS1 target must use a frozen CD manifest and compiled tables.
9. Text saves with `.tmp`, `.bak`, `rename` and `SDL_GetPrefPath` (`items.cpp:628-695`, `title.cpp:364-375`) must become memory-card blocks with checksum, version and two copies.
10. The STL is absent from the MIPS target (verified probe): `game.h:5-13,296-397` must be split or compiled behind a compatibility macro; fixed arrays/arenas, text views, interned IDs and pools are mandatory.
11. Floats are everywhere (physics, camera, animation, audio mixing, `std::hypot/sin/cos/fmod`): on an FPU-less PS1, migrate to 16.16 fixed-point or integer tables.
12. Recommendation: introduce `Platform/Renderer/Input/Audio/Storage/Time` with an SDL3 implementation for PC and PSn00bSDK for PS1, keeping A2 logic and `--auto` tests as the common reference.

## Portability matrix

| SYSTEM | CURRENT IMPLEMENTATION (file:function) | REQUIRED PS1 ADAPTATION | RISKS | POSSIBLE VALIDATION |
|---|---|---|---|---|
| Build / target | `CMakeLists.txt:4-37`: C++20, `find_package(SDL3)`, `add_executable(WIN32)`, `-static`, copy `SDL3.dll`. `build.ps1:4-29`: MinGW clang++ + Ninja. | Add a separate PS1 preset using `cmake/sdk.cmake`, `psn00bsdk_add_executable(... GPREL ...)`, `psn00bsdk_add_cd_image` (SDK template: `CMakeLists.txt:14-20`, `CMakePresets.json:14-19`). Share the source list without changing the PC preset. | Two diverging toolchains; toolchain errors; regression of the 81 CTest cases. | Unchanged PC build; PS1 build produces `*.psx` + `*.bin`/`*.cue`; DuckStation/OpenBIOS smoke test; `ctest --test-dir build`. |
| Global SDL coupling | `src/game.h:3,81,207,214,272,286-289,296,310-312`: SDL types and `std::filesystem` in game state; `src/main.cpp:78` `new Game`. | Extract platform-neutral types; `Game` should contain only POD/fixed-point and IDs. Replace `new Game` with a static/arena instance. | All logic depends on SDL; MIPS compilation impossible or uncontrolled conversions. | Compile the game core without SDL headers; control grep for `#include <SDL3` outside the PC layer. |
| Window / presentation | `src/main.cpp:14-16,112-118`: SDL3 callbacks, resizable 960x720 window, SDL VSync, `SDL_SetRenderLogicalPresentation(INTEGER_SCALE)`. | Replace with a 320x240 GPU video mode, double buffering, `PutDispEnv`/`DrawOTagEnv`, `VSync(0)`/`DrawSync(0)` (PSn00bSDK API confirmed). | Different resolution/framerate; tearing; PC capture logic not transferable. | Native 320x240 capture + NEAREST zoom; same-text/same-scale comparison with A1 references; DuckStation FPS log. |
| Texture / atlas rendering | `src/map.cpp:30-63,70-98`: SDL `RGBA32` atlas, one texture per tile drawn; `src/sprite.cpp:28-35,61-79`: RGBA8888 per frame. | Offline conversion to 4 bpp/8 bpp TIM + CLUT, VRAM upload, sprites packed into textures/pages, order table by depth; remove per-frame allocation. | VRAM > 1 Mo; poorly managed pages/CLUTs; different draw order; conversion CPU cost. | VRAM view and upload counter; zone 011 capture; pixel-wise A1 comparison (nearest); VRAM peak measurement. |
| UI / font / windows | `src/dialogue.cpp:59-115` generates 16 RGBA textures (font + 16 WIND banks); `src/dialogue.cpp:123-139,193-235,376-383` draws via SDL. | Convert FONT3.TIM to one 4 bpp texture + CLUT; WIND.TX/CL to TIM+CLUT; draw glyphs and panels with GPU quads; handle transparent CLUT word 0. | Font differences; lost accents; CLUT corruption; 16 simultaneous banks over budget. | `--diag 1..9` scenes; glyph/accent/panel comparison with references; `validate_a1_textures.py` and `validate_dialogue_layout` tests. |
| Advanced primitives | `src/dialogue.cpp:206-235` `RenderTextureTiled`; `src/menu.cpp:160-167` alpha blending; `src/sprite.cpp:72-79` and `src/title.cpp:350` rotation; `src/fx.cpp:11-40` generated shadow texture; `src/main.cpp:1164-1166` `SDL_RenderDebugText`. | Replace tiling with repeated quads, alpha with GPU semi-transparency, rotation with pre-baked frames or GTE only if proven, shadow with a 4 bpp sprite, debug with a separately compiled diagnostic screen. | Costly software rotation; incorrect blending; hidden SDL dependency in debug. | Diagnostic captures; frame-time profiling; visual check of shadows/scrolls/cursors. |
| Input | `src/main.cpp:217-297` SDL events; `src/main.cpp:300-327` SDL scancodes, axes and buttons. `title.cpp:377-381` SDL event queue for quitting. | `IInput` service: PAD polling via `psxpad.h`/BIOS, identical D-Pad/Cross/R1/L1/Start mapping, rising edges preserved in `Input` (`game.h:34-41`). `quit()` becomes a platform signal. | Different mapping on PS1 controller; lost edges; non-reproducible `--auto`. | Physical/emulated controller mapping test; PC `--auto` scripts and PS1 zone 011 scenario with deterministic PAD injection. |
| Time / loop | `src/main.cpp:1232-1243`: `SDL_GetTicks`, `float` accumulator, fixed `TICK` step, `SDL_srand` seed in test (`:108-110`), `SDL_rand` shake (`:1259`). | `ITime`: VSync + counter/root timer; integer accumulator (microseconds or 60 Hz ticks); custom fixed-seed PRNG; force NTSC or handle 50 Hz PAL explicitly. | Time drift; non-deterministic `--auto` tests; divergent PAL behavior. | VPS/FPS log; identical PC/PS1 `--auto` replay; ticks-per-frame measurement over 1000 frames. |
| Audio output / mixing | `src/audio.cpp:14-63` `feed` callback: software mixing, linear interpolation, 16 voices, 44,1 kHz stereo; `src/audio.cpp:66-87` `SDL_OpenAudioDeviceStream`. | SPU `IAudio` service: 24 hardware channels, VAG/ADPCM, `SpuSetTransferStartAddr`/`SpuWrite` with 64-byte alignment, hardware pitch/volume/pan (API confirmed in PSn00bSDK). Remove the SDL callback and the callback-allocated `std::vector`. | Insufficient CPU; latency; clipping; no equivalent software mixing. | Screenshot/log of SPU voices; emulator listening; DMA measurement and audio peaks < 512 Ko. |
| Audio banks / assets | `src/audio.cpp:79-119,234-267`: 1000 music WAV + 200 SFX WAV via `SDL_LoadWAV`, `.loop` text; directories `music` 662 Mo, `music_a2` 1,42 Go, `sfx` 13 Mo, `mono_a2` 76 Mo. | Offline pre-conversion to VAG/ADPCM, resident sample table < 512 Ko or per-zone CD streaming; loops and jingles stored as compiled metadata, not `.loop` text. | Budget largely exceeded; loading too slow; altered quality/loops. | Conversion script + size report; audio check per track; measured SPU RAM peak. |
| Data / files | `src/main.cpp:29-38,120-138` `SDL_GetBasePath` + `directory_iterator`; `src/world.cpp:139-198`, `src/items.cpp:46`, `src/menu.cpp:137-155`, `src/audio.cpp:132-142,255-266` parse text at runtime. | Frozen ISO manifest (`iso.xml`), ISO9660 access via `psxcd.h`, compiled file table, interned IDs; parse only at zone load or pre-convert to binary tables. | `std::filesystem` unavailable/costly; missing paths; CPU parsing and allocations; editable data non-compliant with CD. | BIN/CUE file list; `--auto` on the same data; `tools/validate_data.py` validator ERR 0/WARN 0. |
| Save / options | `src/items.cpp:628-695` V4 text with `.tmp`, `.bak`, `rename`, `SDL_Delay`, `hasSave()` `ifstream`; `src/title.cpp:364-375,418-428` `SDL_GetPrefPath` + `options.txt`; `src/audio.cpp:176-194` rewrites `data/world.txt`. | `IStorage`: memory-card blocks via `_card_*` (`psxapi.h:280-293`) / `psxpad.h`, CRC checksum, two redundant blocks, V4 version kept, V2/V3 migration; options in a dedicated block; `saveZoneMusic` becomes an in-memory/card setting, never a CD write. | No `rename` or writable FS; block corruption; save loss; F6 breaks the CD. | In-memory `--savetest`; V2/V3/V4/V5 fixtures; emulator memory-card test; re-read after power-off. |
| STL / allocation | `src/game.h:5-13,150-176,296-397`: `map`, `set`, `vector`, `string`, `unique_ptr`, `optional`, `function` everywhere; `src/main.cpp:78`; `src/audio.cpp:20,63,107,182-191,246-253`. MIPS probe: no header or `libstdc++` available. | Common core **without STL**: fixed arrays/arenas, text views, interned IDs, per-zone pools, `Map::draw` callback without `std::function`; split `src/game.h` or compatibility macro. | Proven MIPS compile failure (P13); fragmentation and peak > 2 Mo; non-determinism. | Compile core without STL with `mipsel-none-elf-g++`; grep `std::` outside PC layer; heap instrumentation; RAM peak < 2 Mo. |
| Numbers / math | `src/game.h:21,49-95,205-236,271-276,387`; `src/entities.cpp:34,42,104-105,168,258,290`; `src/audio.cpp:38-60,283-284`; `src/title.cpp:52-96,127-133,205-208,282-350`. | 16.16 fixed-point for positions/velocities/camera/volume/pan; `sqrt/hypot/sin/cos/fmod` via tables or integer approximations; replace public `float`/`double` with a platform alias. | FPU-less R3000A → very slow soft-float; rounding differences; sensitive `--expect` tests. | Ticks/frame benchmark; identical PC/PS1 replay; combat/route A→Z boundary tests. |
| Binary formats / alignment | `src/map.cpp:8-10,25-26,44-53` reads raw PODs into `Cell`/`uint16_t`; `src/sprite.cpp:19-29` reads `uint16_t`/`int16_t`; Python exporters write little-endian (`tools/export_assets.py:59-67`, `tools/a1_sprites.py:198-241`). | MIPS little-endian is compatible with current files, but replace reads with an explicit serializer (fixed widths, bounds checks); structure aligned DMA/GPU/SPU transfers (64 bytes for SPU). | `Cell` depends on C++ layout; unaligned MIPS accesses; out-of-bounds reads; misaligned 16-bit CLUT. | Loader fuzzing; test on a real BIN/CUE; `validate_a1_textures.py`; DMA/VRAM logs. |
| Memory / per-zone loading | `Game` holds maps, sprites, dialogues, music, inventory, forge (`src/game.h:286-397`); `loadZone` replaces the map and restarts music (`src/world.cpp:62-129`); `assets/` totals ~2,54 Go. | Split per zone: map + sprites + UI + one short audio track; explicitly release the previous zone; hard budgets 2 Mo RAM/1 Mo VRAM/512 Ko audio with instrumentation. | Peak over budget; CD streaming too slow; leak between zones; `saveGame` on every `loadZone` (`world.cpp:125`) stresses the memory card. | Real measurements outside the emulator and in DuckStation; per-zone peak report; 011 → 013 → 026 scenario. |
| CLI / deterministic tests | `src/main.cpp:84-211` parses `--auto/--expect/--savetest/--shot/--diag`; `CMakeLists.txt:55-246` 81 CTest; gates and A2 IDs unchanged. | Keep the PC harness intact; on PS1, compile the same hooks behind a test interface (PAD injection + fake storage), no shell CLI; never change dialogue IDs or gates. | Gameplay regression; green PC tests but divergent PS1 build; non-reproducible `--auto` script. | Full green `ctest`; PS1 zone 011 smoke test with the same script; check `mephisto_vaincu`, `epee_supreme`, `lampe`, `prisme`, `boss_medusa`, `cristal`. |
| Diagnostics / PC tools | `SDL_ShowSimpleMessageBox` (`main.cpp:126`), heavy `SDL_Log`, `SDL_RenderDebugText` (`:1164-1166`), `SDL_RenderReadPixels` + `SDL_SaveBMP` (`:1357-1360`), `SDL_Delay` (`items.cpp:686`). | Replace with a platform logger, separately compiled diagnostic screen, VRAM/framebuffer capture via a host tool, VSync wait instead of sleep. | Residual SDL dependencies; PC capture unavailable on console. | Grep SDL in the PS1 target; emulator capture converted to native/zoom PNG; OpenBIOS/DuckStation logs. |
| Region / hardware | Template verified in NTSC-U/C at 59,83 FPS; current loop assumes 60 Hz. | Fix NTSC for milestone 2, then add MIDI/50 Hz or region selection in `ITime` and physical testing (model, homebrew launch method, card, controller to be provided locally). | Slower PAL; physical test unavailable; false positive in emulator. | DuckStation/PCSX-Redux region log; real console test once hardware is available. |

## Concrete weak points and classification

Legend: **B** = `milestone 2 blocker`; **D** = `deferrable` after milestone 2. Complexity: Low / Medium / High / Very high.

| ID | Located weak point | Criticality | Complexity | Minimal fix |
|---|---|---|---|---|
| P01 | `src/game.h:3,286-289`: SDL and `std::filesystem` in `Game`; `CMakeLists.txt:8-29` requires SDL3. | B | High | Create platform interfaces and a PC layer wrapping SDL3; game core without SDL. |
| P02 | `src/main.cpp:112-118`: SDL window/VSync/logical presentation. | B | Medium | PS1 renderer 320x240 double buffer + `VSync/DrawSync`. |
| P03 | `src/map.cpp:30-63,70-98`, `src/sprite.cpp:28-35,61-69`: RGBA textures and per-sprite draws. | B | Very high | Offline A1MP/A1SP → TIM/CLUT pipeline, ordering tables, VRAM budget. |
| P04 | `src/dialogue.cpp:193-235`, `src/menu.cpp:160-167`, `src/fx.cpp:11-40`: tiling/alpha/generated textures. | B | High | GPU quads, native semi-transparency, pre-baked shadows. |
| P05 | `src/dialogue.cpp:59-115,376-383`: 17 static font/WIND textures via SDL. | B | Medium | One 4 bpp font + CLUT, WIND.TX/CLUT in VRAM, transparent word 0. |
| P06 | `src/sprite.cpp:72-79`, `src/title.cpp:350`: SDL software rotation. | B | Medium | Pre-computed frames for the intro; remove the floating angle. |
| P07 | `src/main.cpp:217-327`, `title.cpp:377-381`: SDL events/gamepad input. | B | Medium | `IInput` PAD, identical mapping, rising edges preserved. |
| P08 | `src/main.cpp:1232-1243,1259`: `SDL_GetTicks`, `float` accumulator, `SDL_rand`. | B | Medium | `ITime` VSync + integer ticks + fixed-seed PRNG. |
| P09 | `src/audio.cpp:14-63`: software mixer in SDL callback. | B | Very high | SPU voices, VAG/ADPCM, no CPU mixing. |
| P10 | `src/audio.cpp:79-119,234-267`: 44,1 kHz WAV and 300 banks. | B | High | VAG conversion, CD manifest, <= 512 Ko resident audio. |
| P11 | `src/main.cpp:29-38,120-138`, `world.cpp:139-198`: filesystem + runtime text. | B | High | Frozen ISO9660 manifest, compiled tables, per-zone loading. |
| P12 | `src/items.cpp:628-695`, `title.cpp:364-375`, `audio.cpp:176-194`: FS saves and `world.txt` rewriting. | B | High | `IStorage` memory card, CRC, two blocks, dedicated options. |
| P13 | `src/game.h:5-13,296-397`, `main.cpp:78`: STL and `new Game`; MIPS probe without STL confirmed. | B | Very high | **Hard constraint**: STL-free core, `src/game.h` split or compatibility macro, arenas/text views/interned IDs/pools, static instance. |
| P14 | `src/game.h:21,49-95`, `entities.cpp:34,258`, `audio.cpp:38-60`: floats and math. | B | Very high | 16.16 fixed-point and integer tables/approximations. |
| P15 | `src/map.cpp:8-10,44-53`, `sprite.cpp:19-29`: raw POD serialization. | B | Medium | Explicit little-endian reader with bounds and controlled padding. |
| P16 | `CMakeLists.txt:4-37`, `build.ps1:4-29`, `main.cpp:84-211`: PC-only target and CLI harness. | B | Medium | Separate PS1 preset + platform-neutral test hooks. |
| P17 | `main.cpp:126,1164-1166,1357-1360`, `items.cpp:669-691`: SDL_Log/MessageBox/DebugText/SaveBMP/Delay. | D | Low | Platform logger and capture; final SDL grep in the PS1 target. |
| P18 | NTSC only verified; 60 Hz loop assumed. | D | Medium | Fix NTSC at milestone 2, then handle PAL/50 Hz and physical testing. |

**Total: 16 `milestone 2 blocker` points, 2 `deferrable` points.**

## Constraint confirmed by a real probe: no STL on the MIPS target

Probe run on 2026-11-21: P13 is no longer an optimization, but a hard compilation constraint.

1. `mipsel-none-elf-g++.exe -std=c++20 -c probe.cpp` with `#include <vector>` fails: `fatal error: vector: No such file or directory`.
2. The SDK's C++ headers do exist under `external/tools/psn00bsdk/PSn00bSDK-0.24-win32/mipsel-none-elf/include/c++/12.3.0/`, but **do not** contain `vector`, `string`, `map`, `memory`, `optional`, `functional` or `algorithm` (file-by-file check). The subset present is freestanding: `cstdint`, `cstdio`, `new`, `type_traits`, etc.
3. No `libstdc++*.a` exists in `mipsel-none-elf/lib`; only the libraries `libc_*`, `libpsxapi_*`, `libpsxcd_*`, `libpsxgpu_*`, `libpsxgte_*`, `libpsxpress_*`, `libpsxsio_*`, `libpsxspu_*`, `libpsxetc_*` and `liblzp_*` are provided.
4. The SDK's minimal C++ wrappers are in `include/libpsn00b/`: `cstdint`, `cstdio`, `cstdlib`, `cstring`, `cassert`, `cctype`, `ctype.h`; no container is provided.

Consequence: shared code **cannot** use `std::vector`, `std::string`, `std::map`, `std::set`, `std::unique_ptr`, `std::optional` or `std::function` on the PS1 target. Two paths: (a) STL-free C++ subset for the common core (fixed arrays/arenas, text views, interned IDs, pools) — recommended; (b) a project-local compatibility layer, but **no external dependency without architectural approval**. `src/game.h` must be split (common STL-free data types + PC parts) or compiled behind a compatibility macro. The total remains **16 blocking points**, with P13 now included as a hard constraint.

## Recommended architecture

```text
src/core/        Game, Hero, Enemy, Item, Dialogue, Forge  (POD/fixed-point, no SDL or STL)
src/platform/    IPlatform, IRenderer, IInput, IAudio, IStorage, ITime, ILog
src/platform/pc/ SDL3Renderer, SDL3Input, SDL3Audio, SDL3Storage, SDLTime  (wrapper)
src/platform/psx/PsxRenderer, PsxPadInput, PsxSpuAudio, PsxCdStorage,
                PsxMemCardStorage, PsxTime  (PSn00bSDK)
src/game/        update.cpp, world.cpp, items.cpp, dialogue.cpp, menu.cpp (common)
cmake/ps1/       sdk preset + iso.xml + A1MP/A1SP/TIM/VAG conversion rules
```

Interface rules:

- The common core is **STL-free**: `FixedVec<T,N>`, `TextSpan`, `InternId` and pools on the PS1 side; any STL containers remain confined to the PC layer behind the interfaces. `src/game.h` is split or guarded by a compatibility macro; no external dependency is added without architectural approval.
- `IRenderer` exposes only `drawSprite(id, x, y, ...)`, `drawText(latin1, ...)`, `drawPanel(...)`, `present()`; textures, CLUTs and order tables remain private to the PS1 implementation.
- `IInput` returns `Input` (`game.h:34-41`) with edges already computed; PC reads SDL, PS1 reads PAD. The `--auto` script injects the same `Input`.
- `ITime` provides `tick()` and integer time; PC uses SDL_GetTicks as reference, PS1 uses VSync/root counters. Test mode forces one tick per frame on both sides.
- `IAudio` receives event IDs (`playSfx`, `playMusic`, `playJingle`); 44,1 kHz mixing disappears from common code.
- `IStorage` exposes `hasSave/load/save/options` with V4 version; PC keeps `.tmp/.bak` for CTest, PS1 writes CRC-protected memory-card blocks. The CD is read-only.
- Dialogue IDs, the A→Z route, the gates `epee_supreme`/`lampe`/`prisme`/`boss_medusa`/`cristal`, Méphisto, the Mont Épervier boss and `--auto`/`--expect`/`--savetest` remain non-negotiable invariants.

## Suggested porting order

1. Create the interfaces, ban the STL from the common core (`src/game.h` split/macro) and move the SDL3 PC behind them **without changing a single test**; this is the non-regression safety net.
2. Replace core floats with a fixed-point type and freeze the PRNG; validate with the 81 CTest cases.
3. Implement the minimal PS1 preset: boot, VSync, one map 011 as TIM, PAD, HUD display; measure RAM/VRAM/frame time.
4. Connect A1 dialogues/A2 text, menu, forge, memory-card save and SPU audio in stages, with captures compared at the same text and scale.
5. BIN/CUE mastering, in-depth emulator testing, then physical console testing with hardware to be provided locally.

## Overall validation

- PC: `powershell -ExecutionPolicy Bypass -File build.ps1`, `py -3.12 tools\validate_data.py`, `ctest --test-dir build --output-on-failure`, `git diff --check`.
- PS1: dedicated preset build → PS-X EXE + BIN/CUE; boot with OpenBIOS; `FPS/VPS` log; equivalent `--auto` scenario on zone 011; RAM/VRAM/audio instrumentation; native 320x240 captures + NEAREST zoom compared with A1 references.
- Visual criterion: real side-by-side and overlay inspection, same text/same scale; no validation by color statistics alone.
