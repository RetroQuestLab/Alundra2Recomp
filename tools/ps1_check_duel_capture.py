#!/usr/bin/env python3
r"""Vérifie que les captures du duel PS1 contiennent réellement les flashes.

Le harnais de capture lit l'état au début du VSync, avant que le jeu n'ait
exécuté la liste GPU de l'image courante. Une capture trop précoce peut donc
afficher un état de quête correct mais aucun flash. Ce contrôle porte sur les
pixels natifs 320x240 convertis depuis le PPM, pas sur les statistiques de
couleur globales.

Usage :
    py -3.12 tools\ps1_check_duel_capture.py <dossier_captures>

Sortie : "PS1_DUEL_CAPTURE_PIXELS: PASS" (exit 0) ou FAIL (exit 1).
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from PIL import Image

SIZE = (320, 240)
# Bande où pfDrawRect dessine les trois tires (x=148/158/168, y=72, 4x1).
# La marge couvre les arrondis de conversion 5 bits -> 8 bits.
REGION = (140, 68, 180, 80)
GRAY = (128, 128, 128)   # 0x4210 -> (16,16,16) en 5 bits
WHITE = (248, 248, 248)  # 0x7FFF -> (31,31,31) en 5 bits
EXPECTED = 12            # 3 tires * 4 pixels


def count_colors(path: Path) -> tuple[int, int, tuple[int, int]]:
    with Image.open(path) as image:
        rgb = image.convert("RGB")
        if rgb.size != SIZE:
            raise ValueError(f"{path.name} : {rgb.size}, attendu {SIZE}")
        crop = rgb.crop(REGION)
        pixels = (list(crop.get_flattened_data()) if hasattr(crop, "get_flattened_data")
                  else list(crop.getdata()))
    return pixels.count(GRAY), pixels.count(WHITE), rgb.size


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("outdir", type=Path, help="dossier des PNG natifs")
    args = parser.parse_args()

    report: list[str] = []
    failed = False
    for name, want_gray, want_white in (
        ("duel_guard_hit", EXPECTED, 0),
        ("duel_open_hit", 0, EXPECTED),
        ("duel_down", 0, 0),
    ):
        path = args.outdir / f"{name}.png"
        if not path.is_file():
            print(f"PS1_DUEL_CAPTURE_PIXELS: FAIL - {path} absent")
            return 1
        gray, white, size = count_colors(path)
        ok = gray == want_gray and white == want_white
        failed = failed or not ok
        report.append(
            f"{name}: gray={gray} white={white} attendu gray={want_gray} "
            f"white={want_white} {'OK' if ok else 'FAIL'}"
        )
        if size != SIZE:
            failed = True

    for line in report:
        print(line)
    if failed:
        print("PS1_DUEL_CAPTURE_PIXELS: FAIL")
        return 1
    print("PS1_DUEL_CAPTURE_PIXELS: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
