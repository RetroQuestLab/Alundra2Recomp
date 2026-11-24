#!/usr/bin/env python3
r"""Contrôle pixel de la parité de sélection du menu PS1 (lot v0.8.14).

Compare une capture native du menu (320x240) avec l'ancien rendu et la référence
PC. Les couleurs sont des RGB 8 bits après quantification 5 bits du GPU PS1.

    py -3.12 tools\ps1_check_menu_selection_pixels.py qa\ps1_logs\menu_shots_v0814

Contrôles :
  - disparition de l'aplat cyan opaque (16,144,160) du rendu v0.8.12 ;
  - présence de la bande désaturée (40,56,56) = bgr(5,7,7) ;
  - losanges exacts (120,160,168) = bgr(15,20,21), comme la capture PC ;
  - disparition des losanges blancs saturés (240,248,248) ;
  - témoins positif/négatif sur l'ancienne capture, pour prouver que le
    harnais détecte bien l'écart qu'il est censé détecter.
"""

import argparse
from collections import Counter
from pathlib import Path

from PIL import Image

SIZE = (320, 240)
SHOT = "menu_main.png"
OLD_CYAN = (16, 144, 160)
NEW_BAND = (40, 56, 56)
REF_DIAMOND = (120, 160, 168)
OLD_DIAMOND = (240, 248, 248)
REPO = Path(__file__).resolve().parents[1]
DEFAULT_OLD = REPO / "qa" / "ps1_logs" / "menu_shots_v087_lot"
DEFAULT_PC = REPO / "qa" / "pc_logs" / "probe_c01_frame" / "titre_W400_320.png"


def counts(path: Path) -> Counter:
    image = Image.open(path).convert("RGB")
    if image.size != SIZE:
        raise ValueError(f"{path} : {image.size}, attendu {SIZE}")
    return Counter(image.get_flattened_data() if hasattr(image, "get_flattened_data")
                   else image.getdata())


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("outdir", type=Path, help="dossier de capture v0.8.14")
    parser.add_argument("--old", type=Path, default=DEFAULT_OLD)
    parser.add_argument("--pc", type=Path, default=DEFAULT_PC)
    args = parser.parse_args()

    # Les arguments relatifs sont resolus depuis la racine du depot, comme les
    # valeurs par defaut : le harnais reste independant du CWD.
    def from_repo(path: Path) -> Path:
        return path if path.is_absolute() else REPO / path

    new = counts(from_repo(args.outdir) / SHOT)
    old = counts(from_repo(args.old) / SHOT)
    pc = counts(from_repo(args.pc))

    checks = [
        ("ancien aplat cyan present dans le temoin", old[OLD_CYAN] >= 100,
         f"old={old[OLD_CYAN]} (>=-100)"),
        ("aplat cyan supprime dans la capture livree", new[OLD_CYAN] == 0,
         f"new={new[OLD_CYAN]} (==0)"),
        ("bande desaturee bgr(5,7,7) visible", new[NEW_BAND] >= 8,
         f"new={new[NEW_BAND]} (>=8)"),
        ("losanges 120/160/168 conformes a la capture PC",
         new[REF_DIAMOND] >= 8 and pc[REF_DIAMOND] >= 8,
         f"new={new[REF_DIAMOND]} pc={pc[REF_DIAMOND]} (>=8)"),
        ("losanges blancs satures supprimes", new[OLD_DIAMOND] == 0 and old[OLD_DIAMOND] >= 8,
         f"new={new[OLD_DIAMOND]} old={old[OLD_DIAMOND]}"),
    ]
    failed = False
    for label, ok, detail in checks:
        print(f"{label}: {detail} {'OK' if ok else 'FAIL'}")
        failed = failed or not ok
    print("PS1_MENU_SELECTION_PIXELS: " + ("FAIL" if failed else "PASS"))
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
