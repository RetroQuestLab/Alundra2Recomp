#!/usr/bin/env python3
r"""Convertit une image PS1 brute (secteurs MODE2/2352) en ISO 2048 gravable.

Le BIN/CUE produit par mkpsxiso stocke chaque secteur avec 24 octets
d'en-tete (sync + header) et des octets de parite. Le champ utilisateur
utilisable est exactement [24:24+2048] : c'est une piste MODE1 de 2048 octets.

Usage :
    py -3.12 tools\ps1_make_iso.py <input.bin> <output.iso>

Aucune dependance. Ne modifie pas le BIN source.
"""

import os
import sys

SECTOR = 2352
DATA = 2048
HEADER = 24


def convert(source: str, destination: str) -> tuple[int, int]:
    size = os.path.getsize(source)
    if size % SECTOR != 0:
        raise ValueError(f"{source} : {size} octets, multiple de {SECTOR} attendu")

    sectors = size // SECTOR
    with open(source, "rb") as src, open(destination, "wb") as dst:
        for lba in range(sectors):
            src.seek(lba * SECTOR + HEADER)
            chunk = src.read(DATA)
            if len(chunk) != DATA:
                raise ValueError(f"secteur {lba} tronque : {len(chunk)} octets")
            dst.write(chunk)
    return sectors, sectors * DATA


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    sectors, total = convert(sys.argv[1], sys.argv[2])
    print(f"PS1_ISO: {sectors} secteurs, {total} octets -> {sys.argv[2]}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
