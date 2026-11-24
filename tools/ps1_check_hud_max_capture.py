#!/usr/bin/env python3
r"""Vérifie que les captures PS1 contiennent le suffixe HUD natif "/MM".

Contrôle sur les pixels natifs 320x240 : le slash (160,16) et les deux chiffres
(168,16)/(176,16) doivent différer d'une capture sans suffixe (lot v0.8.11),
et la sonde doit rapporter hp=10. Aucune mesure de FPS ni d'écoute subjective.

Usage :
    py -3.12 tools\ps1_check_hud_max_capture.py <dossier_captures>
        [--baseline <dossier_captures_sans_suffixe>]

Sortie : "PS1_HUD_MAX_CAPTURE_PIXELS: PASS" (exit 0) ou FAIL (exit 1).
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

from PIL import Image

import ps1_export_hud as hud_export

SIZE = (320, 240)
HUD_FRAME = (8, 16)  # contrat A1S des frames /10 et du slash, verifie ci-dessous
SLASH_BOX = (160, 16, 168, 32)
DIGITS_BOX = (168, 16, 184, 32)
SHOT = "hero_vitals_full"
REPO = Path(__file__).resolve().parents[1]
DEFAULT_BASELINE = REPO / "qa" / "ps1_logs" / "hero_vitals_v0811"


def load(outdir: Path) -> Image.Image:
    path = outdir / f"{SHOT}.png"
    if not path.is_file():
        raise ValueError(f"{path} absent")
    image = Image.open(path).convert("RGB")
    if image.size != SIZE:
        raise ValueError(f"{path.name} : {image.size}, attendu {SIZE}")
    return image


def pixels(image: Image.Image) -> list[tuple[int, int, int]]:
    return list(image.get_flattened_data()) if hasattr(image, "get_flattened_data") else list(image.getdata())


def diff_count(a: Image.Image, b: Image.Image, box: tuple[int, int, int, int]) -> int:
    pa = pixels(a.crop(box))
    pb = pixels(b.crop(box))
    return sum(1 for x, y in zip(pa, pb) if x != y)


def probe_max_hp(outdir: Path) -> int:
    stdout = outdir / "capture_stdout.txt"
    if not stdout.is_file():
        return -1
    text = stdout.read_text(encoding="utf-8", errors="replace")
    match = re.search(r"VITALS_CAPTURE: PASS full=(\d+)", text)
    return int(match.group(1)) if match else -1


def a1s_mask(frame_index: int) -> list[list[bool]]:
    bank = hud_export.parse_a1s(REPO / hud_export.SOURCE)
    frame = bank["frames"][frame_index]
    w, h = frame["w"], frame["h"]
    if (w, h) != HUD_FRAME:
        raise ValueError(f"frame A1S {frame_index} : {w}x{h}, attendu {HUD_FRAME}")
    rgba = frame["rgba"]
    return [[rgba[(y * w + x) * 4 + 3] > 96 for x in range(w)] for y in range(h)]


def mask_matches(new: Image.Image, old: Image.Image, x: int,
                 mask: list[list[bool]]) -> tuple[int, int]:
    """Faux positifs / faux negatifs du masque modifie vs masque alpha A1S."""
    false_pos = false_neg = 0
    for y in range(16):
        for dx in range(8):
            changed = new.getpixel((x + dx, 16 + y)) != old.getpixel((x + dx, 16 + y))
            if changed and not mask[y][dx]:
                false_pos += 1
            if mask[y][dx] and not changed:
                false_neg += 1
    return false_pos, false_neg


def max_digits(max_hp: int) -> tuple[int, int]:
    value = min(99, max(0, max_hp))
    return value // 10, value % 10


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("outdir", type=Path, help="dossier des PNG natifs avec suffixe")
    parser.add_argument("--baseline", type=Path, default=DEFAULT_BASELINE,
                        help="dossier de capture sans suffixe ( défaut : v0.8.11)")
    args = parser.parse_args()

    try:
        new = load(args.outdir)
        old = load(args.baseline)
    except (ValueError, OSError) as exc:
        print(f"PS1_HUD_MAX_CAPTURE_PIXELS: FAIL - {exc}")
        return 1

    slash_diff = diff_count(old, new, SLASH_BOX)
    digits_diff = diff_count(old, new, DIGITS_BOX)
    max_hp = probe_max_hp(args.outdir)
    tens, ones = max_digits(max_hp)
    glyphs = (("slash A1S frame 19", 160, 19),
              (f"dizaine A1S frame {9 + tens}", 168, 9 + tens),
              (f"unite A1S frame {9 + ones}", 176, 9 + ones))
    mask_results = [(label, mask_matches(new, old, x, a1s_mask(frame)))
                    for label, x, frame in glyphs]

    checks = [
        ("sonde PV maximaux", max_hp == 10, f"capture_stdout: full={max_hp} (attendu 10)"),
        ("slash natif en (160,16)", slash_diff >= 8, f"pixels differents={slash_diff} (>=8)"),
        ("deux chiffres natifs en (168,16)", digits_diff >= 8,
         f"pixels differents={digits_diff} (>=8)"),
    ]
    for label, (false_pos, false_neg) in mask_results:
        checks.append((label, false_pos == 0 and false_neg == 0,
                       f"masque exact : faux positifs={false_pos} faux negatifs={false_neg}"))
    failed = False
    for label, ok, detail in checks:
        print(f"{label}: {detail} {'OK' if ok else 'FAIL'}")
        failed = failed or not ok
    if failed:
        print("PS1_HUD_MAX_CAPTURE_PIXELS: FAIL")
        return 1
    print("PS1_HUD_MAX_CAPTURE_PIXELS: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
