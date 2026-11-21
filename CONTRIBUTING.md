# Contributing

Thanks for contributing to Alundra2Recomp.

## General rules

- The game core is C++20 with no external engine.
- The SDL3 PC target remains the reference for non-regression.
- The PlayStation 1 target uses PSn00bSDK and a C++ subset without a heavy standard library.
- No commercial game assets are accepted in the repository.
- No generated files, binaries, disc images or working captures are versioned.

## Before every change

Create a dedicated branch, then run from the repository root:

```powershell
powershell -ExecutionPolicy Bypass -File build.ps1
py -3.12 tools\validate_data.py
ctest --test-dir build --output-on-failure
git diff --check
```

For a PlayStation 1 change, also run:

```powershell
py -3.12 tools\ps1_export_zone_test.py
powershell -ExecutionPolicy Bypass -File tools\build_ps1.ps1
powershell -ExecutionPolicy Bypass -File tools\ps1_smoke.ps1 -Seconds 300
```

## Tests

- `--auto` scenarios are deterministic: prefer them over subjective checks.
- Every gameplay change must keep or add a deterministic scenario.
- The PS1 conversion tests lock the data formats and the VRAM layout.

## Commits and pull requests

- One commit = one coherent change.
- Do not include `assets/`, `extracted/`, `gp/`, `images/`, `external/`, `build/`, `*.bin`,
  `*.cue`, `*.iso` or `*.mp4`.
- Do not include email addresses, personal information or absolute local paths in tracked files.
- Describe what changed and which of the commands above you ran.
