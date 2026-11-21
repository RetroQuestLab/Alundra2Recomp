"""Vérification structurelle des textures A1 générées dans src/a1_font.h.

Ce test ne remplace pas une inspection visuelle : il verrouille les causes
déjà démontrées du texte corrompu (décodage 4bpp de WIND.TX, tuiles de police
vides, ordre des nibble) pour éviter qu'elles reviennent silencieusement.
"""

from __future__ import annotations

import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SCREEN = ROOT / "extracted" / "a1" / "TAKI" / "SCREEN"
HEADER = ROOT / "src" / "a1_font.h"

errors: list[str] = []


def check(condition: bool, message: str) -> None:
    if not condition:
        errors.append(message)


def numbers(name: str) -> list[int]:
    text = HEADER.read_text(encoding="utf-8")
    match = re.search(rf"k{name}\s*=\s*{{{{(.*?)}}}};", text, re.S)
    if not match:
        errors.append(f"tableau k{name} absent de {HEADER.name}")
        return []
    return [int(v, 0) for v in re.findall(r"0x[0-9a-fA-F]+|\b\d+\b", match.group(1))]


def int_const(name: str) -> int:
    text = HEADER.read_text(encoding="utf-8")
    match = re.search(rf"k{name}\s*=\s*(\d+)", text)
    if not match:
        errors.append(f"constante k{name} absente de {HEADER.name}")
        return -1
    return int(match.group(1))


def expand_4bpp(data: bytes) -> list[int]:
    """WIND.TX : 256x256 en 4bpp, pixel pair = nibble faible."""
    out: list[int] = []
    for byte in data:
        out.append(byte & 0x0F)
        out.append(byte >> 4)
    return out


def bgr555(word: int) -> tuple[int, int, int]:
    r = (word & 0x1F) << 3
    g = ((word >> 5) & 0x1F) << 3
    b = ((word >> 10) & 0x1F) << 3
    return (r | (r >> 5), g | (g >> 5), b | (b >> 5))


def main() -> int:
    wind = (SCREEN / "WIND.TX").read_bytes()
    clut = (SCREEN / "WIND.CL").read_bytes()
    tim = (SCREEN / "FONT3.TIM").read_bytes()

    width, height = 256, 256
    check(len(wind) == width * height // 2,
          f"WIND.TX fait {len(wind)} octets, attendu {width * height // 2} pour du 4bpp 256x256")
    check(int_const("WindW") == width, f"kWindW={int_const('WindW')}, attendu {width}")
    check(int_const("WindH") == height, f"kWindH={int_const('WindH')}, attendu {height}")
    check(int_const("WindClutBank") == 0,
          f"kWindClutBank={int_const('WindClutBank')}, attendu 0 (parchemin A1)")
    pixels = numbers("WindPixels")
    expected = expand_4bpp(wind)
    check(len(pixels) == len(expected),
          f"kWindPixels contient {len(pixels)} octets, attendu {len(expected)} (nibble étalé)")
    if pixels:
        check(pixels[:64] == expected[:64],
              "kWindPixels ne correspond pas à l'ordre low_first de WIND.TX")

    clut_colors = [bgr555(struct.unpack_from("<H", clut, i * 2)[0]) for i in range(256)]
    palette = numbers("WindPalette")
    check(len(palette) == 64, f"kWindPalette contient {len(palette)} composantes, attendu 64 (16 couleurs)")
    if len(palette) >= 8:
        check(palette[3] == 0, "l'index 0 de la banque WIND doit rester transparent")
        check(palette[7] == 255, "les autres index de la banque WIND doivent être opaques")
    check(clut_colors[0] == (0, 0, 0), "première couleur WIND.CL inattendue")
    clut_words = [struct.unpack_from("<H", clut, i * 2)[0] for i in range(256)]
    expected_colors: list[int] = []
    for i, (r, g, b) in enumerate(clut_colors[:16]):
        # Règle PS1 : un mot CLUT 0x0000 est transparent (banque 0 : index 0 et 7).
        expected_colors.extend((r, g, b, 0 if i == 0 or clut_words[i] == 0 else 255))
    check(palette[:64] == expected_colors,
          "kWindPalette ne correspond pas à la banque 0 de WIND.CL")

    # Structure de la banderole WIND.TX (relevée indice par indice, 2026-11-21) :
    # rouleaux x96..119 et x200..223 (40 lignes), centre x120..199 avec bord
    # déchiré haut (lignes 0..7, dont 0..2 vides), intérieur y8..29 couvert de
    # tuiles REPÈRES « X » (index 1, motif 11...11.) qui marquent la zone à remplir,
    # lignes 30..31 vides, bord déchiré bas y32..39. Les repères ne doivent jamais
    # être dessinés : l'ancien kWindBottomY=29 les étirait en pointillés parasites.
    def wind_index(x: int, y: int) -> int:
        byte = wind[y * 128 + x // 2]
        return byte & 0x0F if x % 2 == 0 else byte >> 4

    def region(x: int, y: int, w: int, h: int) -> list[int]:
        return [wind_index(px, py) for py in range(y, y + h) for px in range(x, x + w)]

    marker = 1  # index des repères X (16,16,16)
    near_black = {i for i, c in enumerate(clut_colors[:16]) if i and max(c) < 32}
    check(marker in near_black, "l'index des repères X devrait être quasi noir dans la banque 0")
    check(region(120, 8, 80, 1).count(marker) >= 16,
          "la ligne 8 du centre de banderole devrait porter les repères X (structure attendue)")

    fill = (int_const("WindFillX"), int_const("WindFillY"),
            int_const("WindFillW"), int_const("WindFillH"))
    check(fill == (16, 0, 80, 40), f"rectangle de remplissage WIND inattendu: {fill}")
    if -1 not in fill:
        indices = region(*fill)
        dark = {i for i, c in enumerate(clut_colors[:16]) if i and max(c) < 96}
        check(all(i not in (0, 7) for i in indices), "la texture parchemin contient des pixels transparents")
        check(not any(i in dark for i in indices), "la texture parchemin contient des pixels sombres")

    roll = (int_const("WindRollLeftX"), int_const("WindRollRightX"), int_const("WindRollW"),
            int_const("WindRollH"), int_const("WindRollCap"), int_const("WindRollMidY"),
            int_const("WindRollMidH"))
    check(roll == (96, 200, 24, 40, 16, 16, 8), f"géométrie des rouleaux inattendue: {roll}")
    edge = (int_const("WindEdgeX"), int_const("WindEdgeW"), int_const("WindTopY"),
            int_const("WindTopH"), int_const("WindBottomY"), int_const("WindBottomH"))
    check(edge == (120, 80, 0, 8, 32, 8), f"géométrie des bords déchirés inattendue: {edge}")
    if -1 not in roll and -1 not in edge:
        drawn = {
            "rouleau gauche": (roll[0], 0, roll[2], roll[3]),
            "rouleau droit": (roll[1], 0, roll[2], roll[3]),
            "bord haut": (edge[0], edge[2], edge[1], edge[3]),
            "bord bas": (edge[0], edge[4], edge[1], edge[5]),
        }
        for name, rect in drawn.items():
            check(not any(i in near_black for i in region(*rect)),
                  f"{name} : contient des pixels de repère X / quasi noirs (motif parasite)")
        top = region(edge[0], edge[2], edge[1], edge[3])
        check(top[:3 * edge[1]].count(0) == 3 * edge[1],
              "les lignes 0..2 du bord haut doivent être vides (rouleaux qui dépassent)")
        check(0 in region(edge[0], 3, edge[1], 2),
              "le bord haut doit garder sa déchirure (pixels transparents ligne 3..4)")
        check(region(edge[0], 32, edge[1], 1).count(0) == 0,
              "la première ligne du bord bas doit être pleine (raccord avec la texture)")
        check(0 in region(edge[0], 36, edge[1], 4), "le bord bas doit garder sa déchirure")

    # Indicateur de page : triangle bas de WIND.TX, banque CLUT 8 (gris + contour),
    # couleurs vérifiées sur la capture A1 lp01_7 (197,197,197) et (74,83,58).
    arrow = (int_const("WindArrowX"), int_const("WindArrowY"), int_const("WindArrowSize"),
             int_const("WindArrowBank"))
    check(arrow == (176, 56, 16, 8), f"indicateur de page inattendu: {arrow}")
    if -1 not in arrow:
        cell = region(arrow[0], arrow[1], arrow[2], arrow[2])
        check(sum(1 for i in cell if i) >= 60, "la tuile de l'indicateur de page est presque vide")
        bank8 = clut_colors[arrow[3] * 16:(arrow[3] + 1) * 16]
        check(bank8[12] == (198, 198, 198), f"banque 8 index 12 = {bank8[12]}, attendu (198,198,198)")
        check(bank8[4] == (74, 82, 57), f"banque 8 index 4 = {bank8[4]}, attendu (74,82,57)")
    banks = numbers("WindPaletteBanks")
    check(len(banks) == 16 * 16 * 4, f"kWindPaletteBanks contient {len(banks)} valeurs, attendu 1024")
    if len(banks) == 1024 and len(palette) == 64:
        check(banks[:64] == palette[:64],
              "la banque 0 de kWindPaletteBanks doit correspondre à kWindPalette")
        check(banks[7 * 4 + 3] == 0, "banque 0 index 7 (mot 0x0000) doit être transparent (règle PS1)")

    glyphs = numbers("GlyphBits")
    advances = numbers("Advance")
    # Pixels isolés débordant d'une tuile voisine (FONT3.TIM) : retirés par
    # tools/a1_font.py. Sans cela, « é » avançait de 16 px (« hé  ritière »).
    if len(advances) == 256:
        check(advances[233] == advances[232] == advances[101],
              f"avances é/è/e = {advances[233]}/{advances[232]}/{advances[101]}, attendues égales")
        for tile in (123, 193, 221, 233, 253):
            check(advances[tile] <= 10, f"avance de la tuile {tile} = {advances[tile]} (pixel isolé non retiré ?)")
    if len(glyphs) == 8192:
        for tile in (193, 221, 233, 253):
            right_column = sum(1 for y in range(16) if glyphs[tile * 32 + y * 2 + 1] & 0x01)
            check(right_column == 0, f"tuile {tile} : pixel isolé encore présent en x=15")
    check(len(glyphs) == 8192, f"kGlyphBits contient {len(glyphs)} octets, attendu 8192")
    check(len(advances) == 256, f"kAdvance contient {len(advances)} valeurs, attendu 256")
    # Comptes vérifiés directement sur FONT3.TIM avec le pas de ligne correct
    # (octet = y*128 + x//2 dans l'atlas 256x256, pas y*128 + tuile*16 + x//2).
    for tile, expected_bits, expected_advance in ((48, 40, 7), (49, 24, 5), (65, 43, 8), (97, 33, 8)):
        bits = sum(byte.bit_count() for byte in glyphs[tile * 32:(tile + 1) * 32])
        check(bits == expected_bits, f"glyphe {tile!r} : {bits} bits, attendu {expected_bits}")
        check(advances[tile] == expected_advance,
              f"avance {tile!r} : {advances[tile]}, attendu {expected_advance}")
    check(len(tim) == 32832, f"FONT3.TIM fait {len(tim)} octets, attendu 32832")

    if errors:
        for error in errors:
            print(f"ERR: {error}")
        print(f"ERR: {len(errors)} | contrôle textures A1 échoué")
        return 1
    print("ERR: 0 | textures A1 conformes aux données natives")
    return 0


if __name__ == "__main__":
    sys.exit(main())
