"""Vérification déterministe de la variante de dialogue avec portrait A1.

Le test rend `--diag 9` dans un BMP puis mesure le portrait : la référence
native observe un buste d'environ 48x56 px. Il ne remplace pas l'inspection
visuelle, mais verrouille le cadrage (plus de mini-sprite 16x24).

Le BMP est analysé sans dépendance externe (stdlib uniquement) afin de
fonctionner avec l'interpréteur Python configuré par CMake.
"""

from __future__ import annotations

import re
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
errors: list[str] = []


def check(condition: bool, message: str) -> None:
    if not condition:
        errors.append(message)


class Bmp:
    """Lecteur minimal des BMP 32 bits produits par SDL_RenderReadPixels."""

    def __init__(self, path: Path) -> None:
        data = path.read_bytes()
        if data[:2] != b"BM":
            raise ValueError("en-tête BMP invalide")
        offset = struct.unpack_from("<I", data, 10)[0]
        header = struct.unpack_from("<IiiHHIIiiII", data, 14)
        size, width, height, planes, bpp, compression = header[:6]
        if size < 40 or planes != 1 or bpp != 32 or compression != 3:
            raise ValueError(f"BMP non pris en charge: {size=} {planes=} {bpp=} {compression=}")
        masks = struct.unpack_from("<III", data, 14 + 40)
        if masks != (0x00FF0000, 0x0000FF00, 0x000000FF):
            raise ValueError(f"masques BMP inattendus: {masks}")
        self._data = data
        self._offset = offset
        self.width = width
        self.height = abs(height)
        self._bottom_up = height > 0

    def pixel(self, x: int, y: int) -> tuple[int, int, int]:
        row = self.height - 1 - y if self._bottom_up else y
        i = self._offset + (row * self.width + x) * 4
        b, g, r = self._data[i], self._data[i + 1], self._data[i + 2]
        return r, g, b


def header_window() -> tuple[list[int], list[list[int]]]:
    """kWindPixels et kWindPalette (banque 0) lus dans src/a1_font.h, sans dépendance."""
    text = (ROOT / "src" / "a1_font.h").read_text(encoding="utf-8")

    def numbers(name: str) -> list[int]:
        match = re.search(rf"k{name}\s*=\s*{{{{(.*?)}}}};", text, re.S)
        if not match:
            errors.append(f"tableau k{name} absent de a1_font.h")
            return []
        return [int(v, 0) for v in re.findall(r"0x[0-9a-fA-F]+|\b\d+\b", match.group(1))]

    pixels = numbers("WindPixels")
    flat = numbers("WindPalette")
    palette = [flat[i:i + 4] for i in range(0, len(flat), 4)]
    return pixels, palette


def main() -> int:
    if len(sys.argv) != 3:
        print("ERR: usage: check_dialogue_layout.py <exe> <shot.bmp>")
        return 2
    exe, shot = Path(sys.argv[1]), Path(sys.argv[2])
    if shot.exists():
        shot.unlink()
    result = subprocess.run(
        [str(exe), "--auto", "W1", "--diag", "9", "--shot", str(shot)],
        cwd=ROOT, timeout=30, check=False,
    )
    check(result.returncode == 0, f"--diag 9 a retourné {result.returncode}")
    if not shot.is_file():
        print("ERR: --diag 9 n'a pas produit de capture")
        return 1

    try:
        im = Bmp(shot)
    except (ValueError, struct.error) as exc:
        print(f"ERR: lecture BMP: {exc}")
        return 1
    check((im.width, im.height) == (960, 720), f"capture {(im.width, im.height)}, attendu (960, 720)")

    bg = (48, 48, 48)
    yellow = (255, 240, 90)
    rect = (7, 117, 55, 168)  # haut du portrait, avant le panneau parchemin
    points: list[tuple[int, int]] = []
    for ny in range(rect[1], rect[3]):
        for nx in range(rect[0], rect[2]):
            color = im.pixel(nx * 3 + 1, ny * 3 + 1)
            if color not in (bg, yellow):
                points.append((nx, ny))
    check(len(points) > 200, f"portrait: seulement {len(points)} pixels non nuls dans la zone attendue")
    if points:
        x0, y0 = min(p[0] for p in points), min(p[1] for p in points)
        x1, y1 = max(p[0] for p in points), max(p[1] for p in points)
        w, h = x1 - x0 + 1, y1 - y0 + 1
        check(w >= 42, f"portrait large de {w} px, attendu >= 42 (référence 48)")
        check(h >= 48, f"portrait haut de {h} px dans la zone mesurée, attendu >= 48")
        check(x0 <= 10 and x1 <= 60, f"portrait mal cadré en x: {x0}..{x1}")
        check(y0 <= 122, f"portrait mal cadré en y: début {y0}, attendu <= 122")

    # Le panneau seul (`--diag 5`, fenêtre en (16,96) 288x64) : aucun pixel des
    # repères X de WIND.TX (quasi noirs) nulle part, y compris le bord bas où
    # l'ancien découpage les étirait en pointillés ; bord haut déchiré (le décor
    # passe entre les dents) ; texture répétée à l'échelle 1:1, jamais étirée.
    window_shot = shot.with_name("layout_window.bmp")
    if window_shot.exists():
        window_shot.unlink()
    result = subprocess.run(
        [str(exe), "--auto", "W1", "--diag", "5", "--shot", str(window_shot)],
        cwd=ROOT, timeout=30, check=False,
    )
    check(result.returncode == 0, f"--diag 5 a retourné {result.returncode}")
    pixels, palette = header_window()
    if window_shot.is_file():
        try:
            win = Bmp(window_shot)
            wx, wy, ww, wh = 16, 96, 288, 64

            def native(nx: int, ny: int) -> tuple[int, int, int]:
                return win.pixel(nx * 3 + 1, ny * 3 + 1)

            near_black = [(nx, ny) for ny in range(wy, wy + wh) for nx in range(wx, wx + ww)
                          if max(native(nx, ny)) < 32]
            check(not near_black,
                  f"repères X / pointillés sombres dans le panneau: {len(near_black)} pixels "
                  f"(ex. {near_black[:4]})")
            above = [native(nx, ny) for ny in range(wy, wy + 3) for nx in range(wx + 30, wx + ww - 30)]
            check(all(c == bg for c in above),
                  "les 3 premières lignes du centre doivent laisser voir le fond (rouleaux qui dépassent)")
            gaps = sum(1 for ny in range(wy + 3, wy + 5) for nx in range(wx + 30, wx + ww - 30)
                       if native(nx, ny) == bg)
            check(gaps >= 5, f"bord haut sans déchirure visible ({gaps} pixels de fond)")
            if pixels and palette:
                ox, oy = wx + 24, wy + 8  # origine de la texture répétée
                mismatch = []
                for v in range(0, 48, 5):
                    for u in range(0, 240, 13):
                        idx = pixels[(v % 40) * 256 + 16 + (u % 80)]
                        want = tuple(palette[idx][:3])
                        got = native(ox + u, oy + v)
                        if got != want:
                            mismatch.append(((u, v), got, want))
                check(not mismatch,
                      f"texture du panneau non répétée à 1:1: {len(mismatch)} écarts (ex. {mismatch[:2]})")
        except (ValueError, struct.error) as exc:
            check(False, f"lecture du panneau BMP: {exc}")

    # Dialogue réel de Prunewell en jeu (zone 011, page complète après 150 ticks) :
    # trois lignes au plus par panneau (relevé sur les captures A1), la première
    # page occupe bien 3 lignes, et l'indicateur de page (triangle gris de la
    # banque 8) est affiché puisque la suite existe. (`--diag 8` montrait en fait
    # le dialogue d'intro ouvert au démarrage, d'où le passage à la vraie scène.)
    page_shot = shot.with_name("layout_page.bmp")
    if page_shot.exists():
        page_shot.unlink()
    result = subprocess.run(
        [str(exe), "--auto", "R1 W2 T2 W150", "--zone", "011", "--at", "30", "47",
         "--expect", "dialogue=prunewell", "--shot", str(page_shot)],
        cwd=ROOT, timeout=60, check=False,
    )
    check(result.returncode == 0, f"dialogue Prunewell en jeu : code {result.returncode}")
    if page_shot.is_file():
        try:
            page = Bmp(page_shot)
            ink = (41, 49, 16)  # index CLUT 6 de FONT3.TIM = encre relevée sur lp11_21
            line_rows = {}
            for ny in range(170, 226):
                n = sum(1 for nx in range(30, 290) if page.pixel(nx * 3 + 1, ny * 3 + 1) == ink)
                if n:
                    line_rows[ny] = n
            third = [ny for ny in line_rows if 170 + 4 + 32 <= ny < 170 + 4 + 48]
            fourth = [ny for ny in line_rows if ny >= 170 + 4 + 48]
            check(bool(line_rows), "aucun texte visible (encre (41,49,16)) dans le panneau du dialogue complet")
            check(bool(third), "la première page de Prunewell devrait occuper 3 lignes (pagination A1 à 3 lignes)")
            check(not fourth, f"texte sur une quatrième ligne (lignes {fourth[:3]}...) : 3 lignes au plus")
            grey = (198, 198, 198)
            arrow = sum(1 for ny in range(196, 222) for nx in range(280, 304)
                        if page.pixel(nx * 3 + 1, ny * 3 + 1) == grey)
            check(arrow >= 20, f"indicateur de page absent ({arrow} pixels gris dans la zone attendue)")
        except (ValueError, struct.error) as exc:
            check(False, f"lecture du dialogue complet BMP: {exc}")

    if errors:
        for error in errors:
            print(f"ERR: {error}")
        print(f"ERR: {len(errors)} | cadrage du dialogue A2 en présentation A1 échoué")
        return 1
    print("ERR: 0 | cadrage du portrait de dialogue conforme à la référence A1")
    return 0


if __name__ == "__main__":
    sys.exit(main())
