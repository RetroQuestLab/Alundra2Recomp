"""Comparaison visuelle native : meme texte, meme echelle, cote a cote et superposition.

Le but n'est pas de produire une jolie image : c'est de fournir une preuve
Trois modes :

1. ``font`` : rend une chaine avec FONT3.TIM decode depuis le TIM natif, puis
   la compare a une coupe de capture de reference contenant **le meme texte**.
2. ``images`` : compare deux captures deja produites (reference / resultat).
3. ``window`` : compare la fenetre de dialogue de reference a une capture du
   remake, en placant les deux a la meme echelle.

Sorties dans le dossier demande (non publie) :
  <out>/cote_a_cote.png, <out>/superposition.png, <out>/reference.png,
  <out>/resultat.png

Usage :
  py -3.12 tools/capture_compare.py font --text "Gramite magique gagne !" \
      --reference <capture.png> --crop 20 340 300 40 --scale 2 --out <dossier>
  py -3.12 tools/capture_compare.py images --reference <a.png> --result <b.png> \
      --scale-reference 1 --scale-result 1 --out <dossier>
"""

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image, ImageDraw

from a1_font import SCREEN, parse_tim

ROOT = Path(__file__).resolve().parent.parent
INK = (24, 41, 8, 255)  # CLUT index 7 (#182908), observe dans FONT3.TIM
CELL = 16


def render_text(text: str, font: Image.Image, scale: int = 1) -> Image.Image:
    """Rend du texte Latin-1 avec l'atlas natif ; largeur = encre reelle + 1 px."""
    px = font.load()
    advances: list[int] = []
    for index in range(256):
        col, row = index % 16, index // 16
        inks = [x for x in range(CELL) for y in range(CELL) if px[col * CELL + x, row * CELL + y][3] > 0]
        advances.append(min(CELL, max(inks) + 2) if inks else 6)

    glyphs: list[tuple[int, int]] = []
    for ch in text:
        code = ord(ch)
        if 32 <= code <= 255:
            glyphs.append((code, advances[code]))
        elif ch == "\n":
            glyphs.append((-1, 0))
        else:
            # Caractere hors Latin-1 : on le rend explicitement en '?' plutôt
            # que de le faire disparaitre silencieusement.
            glyphs.append((ord("?"), advances[ord("?")]))

    lines: list[list[tuple[int, int]]] = [[]]
    for glyph in glyphs:
        if glyph[0] == -1:
            lines.append([])
        else:
            lines[-1].append(glyph)
    line_h = 16
    width = max((sum(w for _, w in line) for line in lines), default=0)
    out = Image.new("RGBA", (max(width, 1) * scale, len(lines) * line_h * scale), (0, 0, 0, 0))
    draw = ImageDraw.Draw(out)
    for row, line in enumerate(lines):
        x = 0
        for code, adv in line:
            col, tile_row = code % 16, code // 16
            for ty in range(CELL):
                for tx in range(CELL):
                    if px[col * CELL + tx, tile_row * CELL + ty][3] > 0:
                        draw.rectangle(
                            (x * scale + tx * scale, row * line_h * scale + ty * scale,
                             x * scale + (tx + 1) * scale - 1, row * line_h * scale + (ty + 1) * scale - 1),
                            fill=INK,
                        )
            x += adv
    return out


def crop_reference(path: Path, crop: tuple[int, int, int, int]) -> Image.Image:
    with Image.open(path) as im:
        return im.convert("RGBA").crop((crop[0], crop[1], crop[0] + crop[2], crop[1] + crop[3]))


def rescale(im: Image.Image, factor: int) -> Image.Image:
    if factor == 1:
        return im
    return im.resize((im.width * factor, im.height * factor), Image.Resampling.NEAREST)


def compose(reference: Image.Image, result: Image.Image, out: Path) -> None:
    out.mkdir(parents=True, exist_ok=True)
    w = max(reference.width, result.width)
    h = max(reference.height, result.height)
    canvas = Image.new("RGBA", (w * 2 + 16, h + 28), (28, 28, 28, 255))
    canvas.paste(reference, (0, 28), reference if reference.mode == "RGBA" else None)
    canvas.paste(result, (w + 16, 28), result if result.mode == "RGBA" else None)
    label = ImageDraw.Draw(canvas)
    label.text((4, 8), "REFERENCE", fill=(235, 235, 235, 255))
    label.text((w + 20, 8), "RESULTAT", fill=(235, 235, 235, 255))
    canvas.save(out / "cote_a_cote.png")

    overlay = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    # Vraie superposition 50 % (correction relevee par graphismes : l'ancien collage opaque ne
    # montrait que le resultat).
    padded = []
    for im in (reference, result):
        layer = Image.new("RGBA", (w, h), (0, 0, 0, 255))
        layer.paste(im.convert("RGBA"), (0, 0))
        padded.append(layer)
    overlay.paste(Image.blend(padded[0], padded[1], 0.5), (0, 0))
    overlay.save(out / "superposition.png")
    reference.save(out / "reference.png")
    result.save(out / "resultat.png")
    print(f"comparaison ecrite dans {out}")
    print(f"  cote_a_cote.png {canvas.width}x{canvas.height}")
    print(f"  superposition.png {w}x{h}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("font", "images", "window"))
    parser.add_argument("--reference", required=True, type=Path)
    parser.add_argument("--result", type=Path)
    parser.add_argument("--text", default="")
    parser.add_argument("--crop", nargs=4, type=int, metavar=("X", "Y", "W", "H"))
    parser.add_argument("--scale", type=int, default=1, help="agrandissement du rendu FONT3")
    parser.add_argument("--scale-reference", type=int, default=1)
    parser.add_argument("--scale-result", type=int, default=1)
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()

    if args.mode == "font":
        font, _, _ = parse_tim(SCREEN / "FONT3.TIM")
        result = rescale(render_text(args.text, font), args.scale)
        reference = crop_reference(args.reference, tuple(args.crop or (0, 0, result.width, result.height)))
    else:
        reference = rescale(Image.open(args.reference).convert("RGBA"), args.scale_reference)
        if args.result is None:
            parser.error("--result est obligatoire pour les modes images et window")
        result = rescale(Image.open(args.result).convert("RGBA"), args.scale_result)
        if args.crop:
            box = tuple(args.crop)
            reference = reference.crop((box[0], box[1], box[0] + box[2], box[1] + box[3]))
            result = result.crop((box[0], box[1], box[0] + box[2], box[1] + box[3]))
    compose(reference, result, args.out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
