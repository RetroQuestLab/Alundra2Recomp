#!/usr/bin/env python3
r"""Contrôle pixel sur TOUTES les captures natives d'un lot de menu PS1.

Le harnais v0.8.14 ne vérifiait que `menu_main.png`. Celui-ci couvre les sept
PNG du lot v0.8.15 et recherche les couleurs interdites de l'ancien rendu,
avec un témoin positif sur l'ancien lot pour prouver que le test détecte
bien l'écart.

    py -3.12 tools\ps1_check_menu_shots.py qa\ps1_logs\menu_shots_v0814

Les chemins relatifs sont résolus depuis la racine du dépôt.
"""

import argparse
from collections import Counter
from pathlib import Path

from PIL import Image

REPO = Path(__file__).resolve().parents[1]
SIZE = (320, 240)
OLD_CYAN = (16, 144, 160)
OLD_DIAMOND = (240, 248, 248)
REF_DIAMOND = (120, 160, 168)
NEW_BAND = (40, 56, 56)
LOGO_GOLD = (40, 40, 8)
SUBTITLE_ORANGE = (224, 144, 104)  # 5 bits de (226,150,110), v0.8.15
OPTIONS_PALE = (144, 160, 192)      # 5 bits de (150,164,196), v0.8.16
OPTIONS_IVORY = (208, 216, 208)     # 5 bits de (214,220,214), ligne selectionnee
FONT_OLIVE = (40, 48, 16)           # ancien libelle d'option, illisible sur WIND
HINT_BLUE = (120, 128, 168)         # 5 bits de (120,132,166), v0.8.17
HINT_CYAN = (120, 216, 216)         # ancien pied de page, banque FONT3 2
HINT_SHOTS = ("menu_main.png", "menu_options.png")
FOOTER_Y = 224
OPTION_SHOTS = ("menu_options.png", "menu_options_speed.png")
DEFAULT_OLD = REPO / "qa" / "ps1_logs" / "menu_shots_v087_lot"
DEFAULT_HINT_OLD = REPO / "qa" / "ps1_logs" / "menu_shots_v0816"


def from_repo(path: Path) -> Path:
    return path if path.is_absolute() else REPO / path


def counts(path: Path) -> Counter:
    image = Image.open(path).convert("RGB")
    if image.size != SIZE:
        raise ValueError(f"{path} : {image.size}, attendu {SIZE}")
    return Counter(image.get_flattened_data() if hasattr(image, "get_flattened_data")
                   else image.getdata())


def footer_counts(path: Path) -> Counter:
    image = Image.open(path).convert("RGB")
    if image.size != SIZE:
        raise ValueError(f"{path} : {image.size}, attendu {SIZE}")
    crop = image.crop((0, FOOTER_Y, SIZE[0], SIZE[1]))
    return Counter(crop.get_flattened_data() if hasattr(crop, "get_flattened_data")
                   else crop.getdata())


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("outdir", type=Path, help="dossier de captures PS1")
    parser.add_argument("--old", type=Path, default=DEFAULT_OLD)
    parser.add_argument("--hint-old", type=Path, default=DEFAULT_HINT_OLD)
    args = parser.parse_args()

    shots = sorted(from_repo(args.outdir).glob("*.png"))
    if len(shots) < 7:
        print(f"attendu au moins 7 PNG, trouve {len(shots)}")
        print("PS1_MENU_SHOTS_PIXELS: FAIL")
        return 1

    table = {}
    options = {}
    hints = {}
    title_counter = None
    for shot in shots:
        counter = counts(shot)
        if shot.name == "title.png":
            title_counter = counter
        if shot.name in OPTION_SHOTS:
            options[shot.name] = (
                counter[OPTIONS_PALE], counter[OPTIONS_IVORY], counter[FONT_OLIVE],
            )
        table[shot.name] = (
            counter[OLD_CYAN], counter[OLD_DIAMOND],
            counter[REF_DIAMOND], counter[NEW_BAND],
        )
        if shot.name in HINT_SHOTS:
            footer = footer_counts(shot)
            hints[shot.name] = (footer[HINT_BLUE], footer[HINT_CYAN])
        detail = (f" pale={options[shot.name][0]:<4} ivoire={options[shot.name][1]:<4}"
                  if shot.name in OPTION_SHOTS else "")
        print(f"{shot.name:<26} cyan={table[shot.name][0]:<5} "
              f"blanc={table[shot.name][1]:<4} "
              f"losange={table[shot.name][2]:<4} bande={table[shot.name][3]}{detail}")
    if title_counter is None:
        print("title.png absent du lot")
        print("PS1_MENU_SHOTS_PIXELS: FAIL")
        return 1
    title_distinct = len(title_counter)
    title_gold = title_counter[LOGO_GOLD]
    title_orange = title_counter[SUBTITLE_ORANGE]
    print(f"title.png                  couleurs={title_distinct:<5} "
          f"or_logo={title_gold:<5} orange_sous_titre={title_orange}")
    for name in HINT_SHOTS:
        print(f"{name:<26} pied_bleu={hints.get(name, (0, 0))[0]:<4} "
              f"pied_cyan={hints.get(name, (0, 0))[1]}")

    old = counts(from_repo(args.old) / "menu_main.png")
    old_hints = {}
    for name in HINT_SHOTS:
        path = from_repo(args.hint_old) / name
        if path.exists():
            footer = footer_counts(path)
            old_hints[name] = (footer[HINT_BLUE], footer[HINT_CYAN])
    checks = [
        ("témoin positif : ancien aplat cyan présent", old[OLD_CYAN] >= 100,
         f"old={old[OLD_CYAN]}"),
        ("témoin positif : anciens losanges blancs présents", old[OLD_DIAMOND] >= 8,
         f"old={old[OLD_DIAMOND]}"),
        ("aucun aplat cyan dans les 7 captures",
         all(row[0] == 0 for row in table.values()), ""),
        ("aucun losange blanc saturé dans les 7 captures",
         all(row[1] == 0 for row in table.values()), ""),
        ("sélection visible au moins une fois",
         any(row[2] >= 8 and row[3] >= 8 for row in table.values()), ""),
        ("écrans sans sélection sans losange parasite",
         any(row[2] == 0 for row in table.values()), ""),
        ("titre réellement rendu (pas un aplat 8,8,32)",
         title_distinct >= 16, f"couleurs={title_distinct}"),
        ("logo doré du titre visible",
         title_gold >= 1000, f"or={title_gold}"),
        ("sous-titre '- remake -' orange visible",
         title_orange >= 100, f"orange={title_orange}"),
        ("lignes d'options normales en bleu pale",
         all(options.get(name, (0, 0, 0))[0] >= 20 for name in OPTION_SHOTS),
         ", ".join(f"{name}={options.get(name, (0, 0, 0))[0]}" for name in OPTION_SHOTS)),
        ("ligne d'options selectionnee en ivoire",
         all(options.get(name, (0, 0, 0))[1] >= 20 for name in OPTION_SHOTS),
         ", ".join(f"{name}={options.get(name, (0, 0, 0))[1]}" for name in OPTION_SHOTS)),
        ("ancien olive d'option remplace par le pale",
         all(pale > olive for pale, _ivory, olive in options.values())
         and len(options) == len(OPTION_SHOTS), ""),
        ("témoin négatif : ancien pied de page cyan au lot v0.8.16",
         all(old_hints.get(name, (0, 0))[1] >= 100 for name in HINT_SHOTS),
         ", ".join(f"{name}={old_hints.get(name, (0, 0))[1]}" for name in HINT_SHOTS)),
        ("témoin négatif : nouveau bleu absent de l'ancien lot",
         all(old_hints.get(name, (0, 0))[0] == 0 for name in HINT_SHOTS), ""),
        ("aides de pied de page en bleu PC (120,128,168)",
         all(hints.get(name, (0, 0))[0] >= 100 for name in HINT_SHOTS),
         ", ".join(f"{name}={hints.get(name, (0, 0))[0]}" for name in HINT_SHOTS)),
        ("aucun cyan (120,216,216) residuel dans le pied de page",
         all(hints.get(name, (0, 0))[1] == 0 for name in HINT_SHOTS), ""),
        ("masque de glyphes egal a la reference PC (837 px sur menu_main)",
         hints.get("menu_main.png", (0, 0))[0] == 837,
         f"bleu={hints.get('menu_main.png', (0, 0))[0]}"),
    ]
    failed = False
    for label, ok, detail in checks:
        print(f"{label} {detail}: {'OK' if ok else 'FAIL'}")
        failed = failed or not ok
    print("PS1_MENU_SHOTS_PIXELS: " + ("FAIL" if failed else "PASS"))
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
