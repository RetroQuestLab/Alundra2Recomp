#!/usr/bin/env python3
r"""Convertit les PPM P6 produits par PCSX.GPU.takeScreenShot() en PNG.

Usage : py -3.12 tools\ppm_to_png.py <input.ppm> <output.png>
Aucune dependance autre que Pillow (deja presente sur l'hote).
"""

import sys

from PIL import Image


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__)
        return 2

    source, destination = sys.argv[1], sys.argv[2]
    with Image.open(source) as image:
        image.convert("RGB").save(destination, format="PNG")
    print(f"PPM_TO_PNG: {source} -> {destination}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
