#!/usr/bin/env python3
r"""Vérifie que les captures PS1 contiennent réellement HUD dynamique et Game Over.

Contrôle sur les pixels natifs 320x240 : gemmes pleines/vides dans la zone HUD,
voile assombri et glyphes dans la fenêtre de Game Over. Aucune mesure de FPS ni
d'écoute subjective.

Usage :
    py -3.12 tools\ps1_check_hero_vitals_capture.py <dossier_captures>

Sortie : "PS1_HERO_VITALS_CAPTURE_PIXELS: PASS" (exit 0) ou FAIL (exit 1).
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from PIL import Image

SIZE = (320, 240)
HUD = (120, 16, 160, 32)      # dix petites gemmes, deux rangées de cinq
WINDOW = (16, 92, 304, 146)   # fenêtre A1 du Game Over local
SCREEN = (0, 0, 320, 240)


def load(outdir: Path, name: str) -> Image.Image:
    path = outdir / f"{name}.png"
    if not path.is_file():
        raise ValueError(f"{path} absent")
    image = Image.open(path).convert("RGB")
    if image.size != SIZE:
        raise ValueError(f"{path.name} : {image.size}, attendu {SIZE}")
    return image


def pixels(region: Image.Image) -> list[tuple[int, int, int]]:
    return list(region.get_flattened_data()) if hasattr(region, "get_flattened_data") else list(region.getdata())


def diff_count(a: Image.Image, b: Image.Image, box: tuple[int, int, int, int]) -> int:
    pa = pixels(a.crop(box))
    pb = pixels(b.crop(box))
    return sum(1 for x, y in zip(pa, pb) if x != y)


def darker_count(a: Image.Image, b: Image.Image, box: tuple[int, int, int, int]) -> int:
    pa = pixels(a.crop(box))
    pb = pixels(b.crop(box))
    return sum(1 for x, y in zip(pa, pb) if sum(y) + 30 < sum(x))


def gem_box(i: int) -> tuple[int, int, int, int]:
    x = 120 + 8 * (i % 5)
    y = 16 + 8 * (i // 5)
    return x, y, x + 8, y + 8


def game_over_text_source_ok() -> bool:
    """Le rendu PS1 utilise un atlas Latin-1, pas l'UTF-8 du source PC."""
    source = Path(__file__).resolve().parents[1] / "src" / "ps1" / "dialogue_ps1.cpp"
    data = source.read_bytes()
    latin1 = b"Flint est tomb\\351..."
    utf8 = "Flint est tombé...".encode("utf-8")
    return latin1 in data and utf8 not in data


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("outdir", type=Path, help="dossier des PNG natifs")
    args = parser.parse_args()

    try:
        full = load(args.outdir, "hero_vitals_full")
        damaged = load(args.outdir, "hero_vitals_damaged")
        over = load(args.outdir, "hero_vitals_game_over")
        respawn = load(args.outdir, "hero_vitals_respawn")
    except (ValueError, OSError) as exc:
        print(f"PS1_HERO_VITALS_CAPTURE_PIXELS: FAIL - {exc}")
        return 1

    hud_diff = diff_count(full, damaged, HUD)
    # plein et respawn sont dans deux zones differentes : le fond derriere les
    # gemmes transparents change. On compare donc respawn a damaged, pris dans
    # la meme zone 013 a la meme position : seul l'etat des gemmes differe.
    hud_restore = diff_count(damaged, respawn, HUD)
    cell_diffs = [diff_count(damaged, respawn, gem_box(i)) for i in range(10)]
    empty_cells = [i for i, n in enumerate(cell_diffs) if n > 4]
    unstable_cells = [i for i, n in enumerate(cell_diffs[:8]) if n != 0]
    veil = darker_count(full, over, SCREEN)
    over_window = pixels(over.crop(WINDOW))
    dark_glyphs = sum(1 for r, g, b in over_window if r + g + b < 100)
    text_ok = game_over_text_source_ok()

    checks = (
        ("HUD plein -> endommage", hud_diff > 32, f"pixels differents={hud_diff} (>32)"),
        ("HUD respawn redessine", hud_restore > 16, f"pixels differents={hud_restore} (>16)"),
        ("HUD 8 PV : 2 gemmes eteintees", empty_cells == [8, 9],
         f"cellules={empty_cells} ([8, 9])"),
        ("HUD 8 PV : gemmes 0..7 stables", not unstable_cells,
         f"cellules={unstable_cells} ([])"),
        ("voile Game Over", veil > 2000, f"pixels assombris={veil} (>2000)"),
        ("glyphes Game Over", dark_glyphs > 50, f"pixels sombres dans fenetre={dark_glyphs} (>50)"),
        ("texte Game Over accentue (Latin-1)", text_ok, "source=src/ps1/dialogue_ps1.cpp"),
    )
    failed = False
    for label, ok, detail in checks:
        print(f"{label}: {detail} {'OK' if ok else 'FAIL'}")
        failed = failed or not ok
    if failed:
        print("PS1_HERO_VITALS_CAPTURE_PIXELS: FAIL")
        return 1
    print("PS1_HERO_VITALS_CAPTURE_PIXELS: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
