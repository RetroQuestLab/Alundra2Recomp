"""Extract the real Alundra 1 bitmap font and dialogue window graphics.

Sources (local, non publiees) :
  extracted/a1/TAKI/SCREEN/FONT3.TIM  -- TIM 4bpp + CLUT 16 couleurs
  extracted/a1/TAKI/SCREEN/WIND.TX    -- texture de fenetre 4 bits/pixel, 256x256
  extracted/a1/TAKI/SCREEN/WIND.CL    -- 16 palettes de 16 couleurs (BGR555 PS1)

Sorties dans extracted/preview/ (non publie) :
  a1_font3_raw.png   -- atlas de police a l'echelle native (1 pixel = 1 pixel)
  a1_font3_4x.png    -- meme atlas agrandi x4 (inspection seulement)
  a1_wind_4bpp_bank0.png -- fenetre 256x256 4bpp, banque CLUT 0 (parchemin)
  a1_wind_4bpp_bank8.png -- meme texture avec la banque d'encre verte (diagnostic)
"""

from __future__ import annotations

import struct
import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
SCREEN = ROOT / "extracted" / "a1" / "TAKI" / "SCREEN"
PREVIEW = ROOT / "extracted" / "preview"


def bgr555_to_rgb(word: int) -> tuple[int, int, int]:
    """PS1 15-bit BGR555 -> RGB888 (les 5 bits sont etires vers 8 bits)."""
    r = (word & 0x1F) << 3
    g = ((word >> 5) & 0x1F) << 3
    b = ((word >> 10) & 0x1F) << 3
    # etirage 5 -> 8 bits pour restituer la valeur PS1 d'origine
    return (r | (r >> 5), g | (g >> 5), b | (b >> 5))


def parse_tim(path: Path) -> tuple[Image.Image, list[tuple[int, int, int]], tuple[int, int]]:
    data = path.read_bytes()
    if data[0] != 0x10:
        raise ValueError(f"{path.name}: pas un TIM (magic {data[0]:#x})")
    flag = struct.unpack_from("<I", data, 4)[0]
    bpp = 4 if (flag & 0x03) == 0 else 8
    # Format TIM : en 4bpp, le champ largeur est en unites de 4 pixels.
    # Source : http://fileformats.archiveteam.org/wiki/TIM_(PlayStation_graphics)
    width_scale = 4 if bpp == 4 else 1
    has_clut = bool(flag & 0x08)
    print(f"{path.name}: bpp={bpp}, clut={has_clut}, flag={flag:#x}")

    pos = 8
    clut: list[tuple[int, int, int]] = []
    if has_clut:
        length = struct.unpack_from("<I", data, pos)[0]
        cx, cy, cw, ch = struct.unpack_from("<HHHH", data, pos + 4)
        count = length - 12
        clut = [
            bgr555_to_rgb(struct.unpack_from("<H", data, pos + 12 + i * 2)[0])
            for i in range(count // 2)
        ]
        print(f"  CLUT: {cw}x{ch} a ({cx},{cy}) -> {len(clut)} couleurs")
        pos += length

    length = struct.unpack_from("<I", data, pos)[0]
    px, py, stored_w, ph = struct.unpack_from("<HHHH", data, pos + 4)
    pw = stored_w * width_scale
    print(f"  pixels: {pw}x{ph} (champ largeur {stored_w}) a ({px},{py}), {length} octets")
    body = data[pos + 12 : pos + length]
    expected = pw * ph * bpp // 8
    if len(body) < expected:
        raise ValueError(f"donnees pixel trop courtes: {len(body)} < {expected}")

    img = Image.new("RGBA", (pw, ph), (0, 0, 0, 0))
    out = img.load()
    for y in range(ph):
        for x in range(pw):
            if bpp == 4:
                byte = body[y * (pw // 2) + x // 2]
                # TIM 4bpp : le pixel PAIR est dans le nibble FAIBLE. L'ordre
                # inverse produisait des glyphes parasités (verifie sur 0/1/B/a).
                idx = (byte & 0x0F) if (x % 2 == 0) else (byte >> 4)
            else:
                idx = body[y * pw + x]
            # TIM 4bpp : index 0 = transparent
            if idx < len(clut) and not (bpp == 4 and idx == 0):
                out[x, y] = clut[idx] + (255,)
    return img, clut, (pw, ph)


def raw_texture_4bpp(path: Path, clut_path: Path, bank: int, name: str) -> Image.Image:
    """Decode WIND.TX en 256x256 4bpp avec une banque de 16 couleurs de WIND.CL."""
    data = path.read_bytes()
    clut_data = clut_path.read_bytes()
    if len(data) < 256 * 128:
        raise ValueError(f"{path.name}: trop court pour du 4bpp 256x256")
    colors = [
        bgr555_to_rgb(struct.unpack_from("<H", clut_data, (bank * 16 + i) * 2)[0])
        for i in range(16)
    ]
    img = Image.new("RGBA", (256, 256), (0, 0, 0, 0))
    out = img.load()
    for y in range(256):
        for x in range(256):
            byte = data[y * 128 + x // 2]
            # Convention PSX verifiee visuellement : pixel pair = nibble faible.
            idx = (byte & 0x0F) if x % 2 == 0 else (byte >> 4)
            if idx:
                out[x, y] = colors[idx] + (255,)
    img.save(PREVIEW / name)
    print(f"  ecrit {PREVIEW / name}")
    return img


def _cpp_bytes(values: list[int], fmt: str = "0x{:02x}", per_line: int = 16) -> str:
    """Formate une liste d'octets en lignes C++ lisibles."""
    return "\n".join(
        "        " + ", ".join(fmt.format(v) for v in values[i : i + per_line]) + ","
        for i in range(0, len(values), per_line)
    )


def _cpp_rgba(colors: list[tuple[int, int, int]], transparent_zero: bool = True,
              transparent: set[int] | None = None) -> str:
    rows = []
    for i, (r, g, b) in enumerate(colors):
        hidden = (transparent_zero and i == 0) or (transparent is not None and i in transparent)
        a = 0 if hidden else 255
        rows.append(f"        {{{{{r}, {g}, {b}, {a}}}}},")
    return "\n".join(rows)


def strip_stray_pixels(ink: set[tuple[int, int]]) -> tuple[set[tuple[int, int]], set[tuple[int, int]]]:
    """Retire les pixels isoles qui debordent d'une tuile voisine de l'atlas.

    Constat sur FONT3.TIM (2026-11-21) : les tuiles 123, 193, 221, 233 (e accent aigu)
    et 253 portent 1 a 2 pixels isoles en x=15 (ou x=8 pour 123), a plus de 3 colonnes
    du corps du glyphe. Ils gonflaient l'avance a 16 px (blanc visible apres chaque
    'e accent aigu' : « he  ritiere ») et etaient dessines comme un point parasite.
    Regle : une composante 8-connexe de 3 pixels au plus, separee du corps principal
    par au moins 3 colonnes vides, n'appartient pas au glyphe. Les accents (proches
    du corps) sont conserves.
    """
    comps: list[set[tuple[int, int]]] = []
    seen: set[tuple[int, int]] = set()
    for start in ink:
        if start in seen:
            continue
        stack, comp = [start], set()
        while stack:
            p = stack.pop()
            if p in seen:
                continue
            seen.add(p)
            comp.add(p)
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    q = (p[0] + dx, p[1] + dy)
                    if q in ink and q not in seen:
                        stack.append(q)
        comps.append(comp)
    if len(comps) < 2:
        return set(ink), set()
    body = max(comps, key=len)
    bx0 = min(x for x, _ in body)
    bx1 = max(x for x, _ in body)
    kept, dropped = set(body), set()
    for comp in comps:
        if comp is body:
            continue
        cx0 = min(x for x, _ in comp)
        cx1 = max(x for x, _ in comp)
        gap = max(cx0 - bx1, bx0 - cx1)
        (dropped if len(comp) <= 3 and gap >= 3 else kept).update(comp)
    return kept, dropped


def generate_header(font: Image.Image, clut: list[tuple[int, int, int]]) -> Path:
    """Genere src/a1_font.h depuis les donnees natives (jamais de glyphe code a la main).

    L'index de tuile 16x16 est l'octet Latin-1 : verification structurelle faite
    dans extracted/preview/a1_font3_raw.png (32=espace, 65=A, 97=a, 233=e-accent-aigu).
    Les tuiles 0..31 sont des symboles, pas des lettres : on les conserve pour
   completude, mais drawLatin1 ne les utilise pas.
    """
    cell = 16
    px = font.load()
    glyphs: list[int] = []
    advances: list[int] = []
    stray_report: list[str] = []
    for tile in range(256):
        col, row = tile % 16, tile // 16
        # 16x16 pixels a 1 bit = 2 octets par ligne x 16 lignes = 32 octets.
        # Une tuile ne contient PAS deux octets au total : c'etait la cause
        # du texte invisible (les tuiles 16..255 du tableau etaient a zero).
        ink = {(x, y) for y in range(cell) for x in range(cell)
               if px[col * cell + x, row * cell + y][3] > 0}
        ink, dropped = strip_stray_pixels(ink)
        if dropped:
            stray_report.append(f"{tile}:{sorted(dropped)}")
        bits = [0] * (cell * cell // 8)
        max_x = -1
        for x, y in ink:
            bits[y * 2 + x // 8] |= 1 << (7 - (x % 8))
            max_x = max(max_x, x)
        glyphs.extend(bits)
        # Avance observee : encre reelle + 2 px de respiration, bornee a la tuile.
        advances.append(min(cell, max_x + 2) if max_x >= 0 else 6)

    wind_bytes = (SCREEN / "WIND.TX").read_bytes()
    wind_clut_bytes = (SCREEN / "WIND.CL").read_bytes()
    # WIND.TX est un plan 4bpp de 256x256 : 32768 octets, pixel pair = nibble
    # faible (ordre verifie visuellement). WIND.CL contient 16 palettes de 16
    # couleurs ; la banque 0 est le parchemin sepia des dialogues A1.
    if len(wind_bytes) < 256 * 128:
        raise ValueError("WIND.TX doit contenir 32768 octets pour du 4bpp 256x256")
    wind_pixels: list[int] = []
    for byte in wind_bytes[: 256 * 128]:
        wind_pixels.append(byte & 0x0F)
        wind_pixels.append(byte >> 4)
    wind_words = [struct.unpack_from("<H", wind_clut_bytes, i * 2)[0] for i in range(256)]
    wind_colors = [bgr555_to_rgb(w) for w in wind_words]
    # Regle PS1 : un mot CLUT 0x0000 est transparent quel que soit l'index
    # (banque 0 : index 0 et 7). Les autres couleurs sont opaques.
    wind_hidden = [{i for i in range(16) if wind_words[bank * 16 + i] == 0} for bank in range(16)]
    wind_banks = "\n".join(
        "        {{\n" + _cpp_rgba(wind_colors[bank * 16:(bank + 1) * 16], True, wind_hidden[bank])
        .replace("        {{{", "            {{{") + "\n        }},"
        for bank in range(16)
    )
    if stray_report:
        print("  pixels isoles retires des tuiles : " + " ; ".join(stray_report))

    font_colors = clut + [(0, 0, 0)] * (16 - len(clut))
    out = ROOT / "src" / "a1_font.h"
    out.write_text(
        "// GENERE par tools/a1_font.py -- ne pas editer a la main.\n"
        "// Source : extracted/a1/TAKI/SCREEN/FONT3.TIM + WIND.TX/WIND.CL (non publies).\n"
        "// Preuve : index de tuile 16x16 = octet Latin-1 (voir docs/visuel/a1_police_cadres_reference.md).\n"
        "#pragma once\n"
        "#include <array>\n"
        "#include <cstdint>\n"
        "namespace a1font {\n"
        "inline constexpr int kCell = 16;\n"
        "inline constexpr int kGlyphCount = 256;\n"
        "// 1 bit/pixel, 16x16 par glyphe, octet fort en premier (x=0 -> bit 7).\n"
        "inline constexpr std::array<uint8_t, 8192> kGlyphBits = {{\n"
        + _cpp_bytes(glyphs)
        + "\n}};\n"
        "// Avance reelle : max_x + 2, bornee a 16 ; 6 pour un glyphe vide.\n"
        "inline constexpr std::array<uint8_t, 256> kAdvance = {{\n"
        + _cpp_bytes(advances, "{}", 16)
        + "\n}};\n"
        "// CLUT 4bpp de FONT3.TIM (index 0 transparent).\n"
        "inline constexpr std::array<std::array<uint8_t, 4>, 16> kFontPalette = {{\n"
        + _cpp_rgba(font_colors[:16])
        + "\n}};\n"
        "inline constexpr int kWindW = 256, kWindH = 256;\n"
        "inline constexpr int kWindClutBank = 0; // parchemin sepia des dialogues A1\n"
        # Structure de la banderole WIND.TX (x96..223, y0..39), relevee indice par
        # indice le 2026-11-21 : rouleau gauche x96..119, centre x120..199, rouleau
        # droit x200..223. Au centre : bord dechire haut y3..7, interieur y8..29
        # rempli de tuiles REPERES « X » (index 1, motif 11...11.) qui marquent la
        # zone a remplir par la texture, lignes 30..31 vides, bord dechire bas
        # y32..39. Les X ne doivent jamais etre dessines : c'etaient les pointilles
        # parasites (ancien kWindBottomY=29 = derniere ligne de X).
        # Le jeu A1 compose le panneau a l'echelle 1:1 (texture repetee, pas etiree).
        "// Texture parchemin repetee a 1:1 (aucun pixel transparent ni repere X).\n"
        "inline constexpr int kWindFillX = 16, kWindFillY = 0;\n"
        "inline constexpr int kWindFillW = 80, kWindFillH = 40;\n"
        "// Rouleaux : 24 px de large, 40 lignes ; chapeaux haut/bas de 16 lignes\n"
        "// gardes intacts, corps du cylindre (lignes 16..23) repete en hauteur.\n"
        "inline constexpr int kWindRollLeftX = 96, kWindRollRightX = 200;\n"
        "inline constexpr int kWindRollW = 24, kWindRollH = 40, kWindRollCap = 16;\n"
        "inline constexpr int kWindRollMidY = 16, kWindRollMidH = 8;\n"
        "// Chapeaux ARRONDIS des rouleaux (WIND.TX 224,0 gauche ; 224,24 droit ; 24x16), suivis des\n"
        "// H-16 DERNIERES lignes de la colonne du rouleau : composition mesuree au pixel sur l'ecran\n"
        "// d'inventaire A1 (gp/images.png, docs/visuel/a1_menu_layout.md), meme parchemin que les dialogues.\n"
        "inline constexpr int kWindCapLeftX = 224, kWindCapLeftY = 0;\n"
        "inline constexpr int kWindCapRightX = 224, kWindCapRightY = 24;\n"
        "inline constexpr int kWindCapW = 24, kWindCapH = 16;\n"
        "// Bords dechires du centre, repetes en largeur (periode 80 px).\n"
        "inline constexpr int kWindEdgeX = 120, kWindEdgeW = 80;\n"
        "// Bord haut pris des lignes 0..7 : les lignes 0..2 du centre sont vides dans la\n"
        "// source, ce qui laisse depasser les rouleaux de 3 px comme sur la capture A1.\n"
        "inline constexpr int kWindTopY = 0, kWindTopH = 8;\n"
        "inline constexpr int kWindBottomY = 32, kWindBottomH = 8;\n"
        "// Indicateur de page (triangle bas) : premiere des 4 tuiles x176..239 y56..71,\n"
        "// banque CLUT 8 (gris 24/31 + contour (72,80,56)), couleurs verifiees sur la\n"
        "// capture A1 lp01_7 (197,197,197) / (74,83,58) apres etirage 5->8 bits.\n"
        "inline constexpr int kWindArrowX = 176, kWindArrowY = 56, kWindArrowSize = 16;\n"
        "inline constexpr int kWindArrowBank = 8;\n"
        "// WIND.TX : 4 bits/pixel, nibble faible = pixel pair, index 0 = fond.\n"
        "inline constexpr std::array<uint8_t, 65536> kWindPixels = {{\n"
        + _cpp_bytes(wind_pixels, "{}", 32)
        + "\n}};\n"
        "inline constexpr std::array<std::array<uint8_t, 4>, 16> kWindPalette = {{\n"
        + _cpp_rgba(wind_colors[:16], True, wind_hidden[0])
        + "\n}};\n"
        "// Les 16 banques de WIND.CL (regle PS1 : mot 0x0000 transparent).\n"
        "inline constexpr std::array<std::array<std::array<uint8_t, 4>, 16>, 16> kWindPaletteBanks = {{\n"
        + wind_banks
        + "\n}};\n"
        "} // namespace a1font\n",
        encoding="utf-8",
    )
    print(f"  ecrit {out.relative_to(ROOT)} (8192 octets de glyphes, 65536 indices de fenetre 4bpp)")
    return out


def main() -> int:
    PREVIEW.mkdir(parents=True, exist_ok=True)
    font, clut, size = parse_tim(SCREEN / "FONT3.TIM")
    print(f"  CLUT police: {[f'#{r:02x}{g:02x}{b:02x}' for r, g, b in clut]}")
    font.save(PREVIEW / "a1_font3_raw.png")
    font.resize((size[0] * 4, size[1] * 4), Image.Resampling.NEAREST).save(PREVIEW / "a1_font3_4x.png")
    print(f"  ecrit a1_font3_raw.png ({size[0]}x{size[1]}) et a1_font3_4x.png")

    # Planche annote : 16x16 tuiles de 16 px, index decimal en dessous.
    cell = 16
    scale = 3
    label_h = 14
    cols = size[0] // cell
    rows = size[1] // cell
    sheet = Image.new("RGBA", (cols * cell * scale, rows * (cell * scale + label_h)), (24, 24, 24, 255))
    from PIL import ImageDraw
    draw = ImageDraw.Draw(sheet)
    for row in range(rows):
        for col in range(cols):
            tile = font.crop((col * cell, row * cell, (col + 1) * cell, (row + 1) * cell))
            x = col * cell * scale
            y = row * (cell * scale + label_h)
            sheet.paste(tile.resize((cell * scale, cell * scale), Image.Resampling.NEAREST), (x, y))
            draw.text((x + 2, y + cell * scale), f"{row * cols + col:03d}", fill=(255, 255, 255, 255))
    sheet.save(PREVIEW / "a1_font3_indexed.png")
    print(f"  ecrit a1_font3_indexed.png ({cols}x{rows} tuiles de {cell} px)")

    print("WIND:")
    raw_texture_4bpp(SCREEN / "WIND.TX", SCREEN / "WIND.CL", 0, "a1_wind_4bpp_bank0.png")
    raw_texture_4bpp(SCREEN / "WIND.TX", SCREEN / "WIND.CL", 8, "a1_wind_4bpp_bank8.png")
    generate_header(font, clut)
    return 0


if __name__ == "__main__":
    sys.exit(main())
