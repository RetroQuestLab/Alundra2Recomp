"""Genere les donnees natives A1 pour la cible PS1 a partir de src/a1_font.h.

Sortie : src/ps1/a1_font_ps1.h, tableaux POD pour le toolchain MIPS
(pas de STL, pas d'allocation dynamique) :
  * kA1FontAtlas    : police FONT3.TIM, 4bpp 256x256 (32768 octets)
  * kA1FontCluts    : 7 CLUT or / blanc / cyan / gris / orange (titre) / bleu pale (options) / bleu aide (pied de page)
  * kA1FontAdvance  : avance reelle des 256 glyphes (une case par octet Latin-1)
  * kA1WindAtlas    : WIND.TX, 4bpp 256x256 (32768 octets, nibble faible = pixel pair)
  * kA1WindCluts    : 16 banques CLUT de la fenetre de dialogue

Source : src/a1_font.h (genere lui-meme depuis FONT3.TIM et WIND.TX, non publie).

Usage :
    py -3.12 tools/ps1_export_font.py
    py -3.12 tools/ps1_export_font.py --stdout   # brut atlas+CLUT, pour les tests
"""

import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(REPO, 'src', 'a1_font.h')
DST = os.path.join(REPO, 'src', 'ps1', 'a1_font_ps1.h')

CELL = 16
GLYPHS = 256
ATLAS_PX = CELL * 16  # 256x256 pixels, 16 glyphes par ligne
ATLAS_BYTES = ATLAS_PX * ATLAS_PX // 2  # 4bpp : deux pixels par octet
WIND_BYTES = ATLAS_BYTES


def read_numbers(text, marker, expected):
    """Extrait les valeurs numeriques d'un tableau inline constexpr.

    Accepte les notations decimales et hexagonales, les valeurs etant separees
    par des virgules dans src/a1_font.h (tableaux std::array imbriques inclus).
    """
    start = text.index(marker) + len(marker)
    end = text.index('}};', start)
    values = [int(v, 0) for v in re.findall(r'\b(?:0[xX][0-9a-fA-F]+|\d+)\b', text[start:end])]
    if len(values) != expected:
        raise ValueError(f'{marker.strip()} : {len(values)} valeurs, attendu {expected}')
    return values


def read_glyph_bits(text):
    values = read_numbers(text, 'kGlyphBits = {{', GLYPHS * CELL * CELL // 8)
    return values


def read_advance(text):
    return read_numbers(text, 'kAdvance = {{', GLYPHS)


def read_wind_pixels(text):
    pixels = read_numbers(text, 'kWindPixels = {{', ATLAS_PX * ATLAS_PX)
    atlas = bytearray(WIND_BYTES)
    for p, value in enumerate(pixels):
        idx = value & 0x0F
        if idx == 0:
            continue
        off = p // 2
        if p % 2 == 0:
            atlas[off] = (atlas[off] & 0xF0) | idx
        else:
            atlas[off] = (atlas[off] & 0x0F) | (idx << 4)
    return bytes(atlas)


def bgr555(r, g, b):
    return ((b >> 3) << 10) | ((g >> 3) << 5) | (r >> 3)


def make_cluts():
    # index 0 = transparent ; 1 = corps ; 2 = arete claire ; 3 = ombre.
    # L'encre des dialogues A1 est l'index 6 de FONT3.TIM : (41,49,16).
    # Toutes les variations d'index du relief sont donc neutralisees dans la
    # CLUT 0 : le texte reste plat, comme dans le rendu PC de reference.
    palettes = (
        ((41, 49, 16), (41, 49, 16), (41, 49, 16)),          # encre A1 (dialogues)
        ((214, 220, 214), (214, 220, 214), (214, 220, 214)), # ivoire
        ((120, 220, 220), (120, 220, 220), (120, 220, 220)), # cyan (selection)
        ((110, 112, 124), (110, 112, 124), (110, 112, 124)), # gris (grise)
        # v0.8.15 : "- remake -" du titre PC (src/title.cpp:616) est (226,150,110).
        # La banque 0 olive (41,49,16) le rendait invisible sur le fond dore.
        ((226, 150, 110), (226, 150, 110), (226, 150, 110)), # orange (titre)
        # v0.8.16 : les options PC ecrivent les lignes normales en (150,164,196)
        # (src/title.cpp:565). La banque 0 olive etait illegible sur le WIND.
        ((150, 164, 196), (150, 164, 196), (150, 164, 196)), # bleu pale (options PC)
        # v0.8.17 : l'aide de pied de page PC est (120,132,166) (src/title.cpp:568).
        # bgr555() tronque : 166>>3 = 20, mais 170>>3 = 21 donne (120,128,168),
        # plus proche en 5 bits (ecart 0/4/2). Ne pas arrondir toutes les CLUT.
        ((120, 132, 170), (120, 132, 170), (120, 132, 170)), # bleu aide (pied de page PC)
        # v0.8.37 : libelle selectionne du menu PC (210,232,225) (title.cpp:565) ;
        # la banque 0 olive le rendait invisible sur le bandeau.
        ((210, 232, 225), (210, 232, 225), (210, 232, 225)), # selection menu PC
        # v0.8.37 : sous-titre PC "UNE LEGENDE EST NEE" (190,208,244) (title.cpp:612).
        ((190, 208, 244), (190, 208, 244), (190, 208, 244)), # sous-titre PC
        # v0.8.37 : prologue PC (src/title.cpp::drawPrologue).
        ((225, 238, 232), (225, 238, 232), (225, 238, 232)), # texte du prologue
        ((180, 225, 225), (180, 225, 225), (180, 225, 225)), # nom du locuteur
        ((8, 8, 8), (8, 8, 8), (8, 8, 8)),                   # ombre noire
        ((90, 98, 124), (90, 98, 124), (90, 98, 124)),       # invite de passage
    )
    cluts = []
    for base, hi, lo in palettes:
        clut = [0x0000, bgr555(*base), bgr555(*hi), bgr555(*lo)]
        clut += [0x0000] * (16 - len(clut))
        cluts.append(clut)
    return cluts


def make_wind_cluts(text):
    rgba = read_numbers(text, 'kWindPaletteBanks = {{', 16 * 16 * 4)
    cluts = []
    for bank in range(16):
        clut = []
        for color in range(16):
            o = (bank * 16 + color) * 4
            r, g, b, a = rgba[o:o + 4]
            clut.append(0x0000 if a == 0 else bgr555(r, g, b) & 0x7FFF)
        cluts.append(clut)
    return cluts


def build():
    text = open(SRC, encoding='utf-8').read()
    bits = read_glyph_bits(text)
    advance = read_advance(text)
    wind_pixels = read_wind_pixels(text)
    wind_cluts = make_wind_cluts(text)

    atlas = bytearray(ATLAS_BYTES)
    for g in range(GLYPHS):
        gx, gy = (g % 16) * CELL, (g // 16) * CELL
        for y in range(CELL):
            row = bits[g * 32 + y * 2:g * 32 + y * 2 + 2]
            for x in range(CELL):
                byte = row[x // 8]
                if not (byte & (0x80 >> (x % 8))):
                    continue
                # Leger relief A1 : arete en haut/gauche, ombre en bas/droite.
                idx = 2 if y == 0 or x == 0 else 3 if y == CELL - 1 or x == CELL - 1 else 1
                px = gx + x
                py = gy + y
                off = (py * ATLAS_PX + px) // 2
                if px % 2 == 0:
                    atlas[off] = (atlas[off] & 0xF0) | idx
                else:
                    atlas[off] = (atlas[off] & 0x0F) | (idx << 4)
    cluts = make_cluts()
    return bytes(atlas), cluts, advance, wind_pixels, wind_cluts


def format_rows(values, per_row, fmt):
    # La virgule de fin de ligne est obligatoire : sans elle, deux lignes
    # consecutives forment une expression invalide pour le compilateur MIPS.
    return '\n'.join(
        '\t' + ', '.join(fmt(v) for v in values[i:i + per_row]) + ','
        for i in range(0, len(values), per_row)
    )


def format_table(name, ctype, values, per_row, fmt):
    return f'static const {ctype} {name}[{len(values)}] = {{\n' + format_rows(values, per_row, fmt) + '\n};\n\n'


def format_cluts(name, cluts):
    return (
        f'static const uint16_t {name}[{len(cluts)}][16] = {{\n'
        + ',\n'.join('\t{ ' + ', '.join(f'0x{c:04X}' for c in clut) + ' }' for clut in cluts)
        + '\n};\n\n'
    )


def format_header(atlas, cluts, advance, wind_pixels, wind_cluts):
    return (
        '// GENERE par tools/ps1_export_font.py -- ne pas editer a la main.\n'
        '// Source : src/a1_font.h (FONT3.TIM / WIND.TX, non publie). Atlas 4bpp 256x256.\n'
        '#pragma once\n'
        '#include <stdint.h>\n\n'
        + format_table('kA1FontAtlas', 'uint8_t', atlas, 32, lambda v: f'0x{v:04X}')
        + format_cluts('kA1FontCluts', cluts)
        + format_table('kA1FontAdvance', 'uint8_t', advance, 32, lambda v: f'{v}')
        + format_table('kA1WindAtlas', 'uint8_t', wind_pixels, 32, lambda v: f'0x{v:04X}')
        + format_cluts('kA1WindCluts', wind_cluts)
    )


def main():
    atlas, cluts, advance, wind_pixels, wind_cluts = build()
    if '--stdout' in sys.argv[1:]:
        sys.stdout.buffer.write(atlas)
        for clut in cluts:
            sys.stdout.buffer.write(bytes(c for half in clut for c in (half & 0xFF, half >> 8)))
        return 0
    old = open(DST, encoding='utf-8').read() if os.path.exists(DST) else ''
    text = format_header(atlas, cluts, advance, wind_pixels, wind_cluts)
    if text != old:
        os.makedirs(os.path.dirname(DST), exist_ok=True)
        open(DST, 'w', encoding='utf-8', newline='\n').write(text)
        print(f'police/fenetre PS1 : {len(atlas)} octets police, {len(advance)} avances, '
              f'{len(wind_pixels)} octets WIND, {len(wind_cluts)} CLUT -> {DST}')
    else:
        print('police/fenetre PS1 : a jour')
    return 0


if __name__ == '__main__':
    sys.exit(main())
