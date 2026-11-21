"""Genere l'atlas de police A1 (FONT3.TIM) et ses CLUT pour la cible PS1.

Source : src/a1_font.h (genere lui-meme depuis FONT3.TIM, non publie).
Sortie : src/ps1/a1_font_ps1.h, tableaux POD pour le toolchain MIPS
(pas de STL, pas d'allocation dynamique).

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


def read_glyph_bits():
    text = open(SRC, encoding='utf-8').read()
    marker = 'kGlyphBits = {{'
    start = text.index(marker) + len(marker)
    end = text.index('}};', start)
    values = [int(v, 16) for v in re.findall(r'0x([0-9a-fA-F]+)', text[start:end])]
    if len(values) != GLYPHS * CELL * CELL // 8:
        raise ValueError(f'kGlyphBits : {len(values)} octets, attendu {GLYPHS * CELL * CELL // 8}')
    return values


def bgr555(r, g, b):
    return ((b >> 3) << 10) | ((g >> 3) << 5) | (r >> 3)


def make_cluts():
    # index 0 = transparent ; 1 = corps ; 2 = arete claire ; 3 = ombre.
    palettes = (
        ((255, 215, 80), (255, 246, 186), (120, 60, 8)),    # or (titre)
        ((205, 228, 226), (255, 255, 240), (70, 78, 96)),    # blanc ivoire
        ((120, 220, 220), (210, 255, 250), (16, 60, 72)),    # cyan (selection)
        ((110, 112, 124), (168, 172, 184), (38, 40, 52)),    # gris (grise)
    )
    cluts = []
    for base, hi, lo in palettes:
        clut = [0x0000, bgr555(*base), bgr555(*hi), bgr555(*lo)]
        clut += [0x0000] * (16 - len(clut))
        cluts.append(clut)
    return cluts


def build():
    bits = read_glyph_bits()
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
    return bytes(atlas), cluts


def format_rows(values, per_row, width):
    return '\n'.join(
        '\t' + ', '.join(f'0x{v:04X}' for v in values[i:i + per_row])
        for i in range(0, len(values), per_row)
    )


def format_header(atlas, cluts):
    flat = [c for clut in cluts for c in clut]
    return (
        '// GENERE par tools/ps1_export_font.py -- ne pas editer a la main.\n'
        '// Source : src/a1_font.h (FONT3.TIM, non publie). Atlas 4bpp 256x256.\n'
        '#pragma once\n'
        '#include <stdint.h>\n\n'
        f'static const uint8_t kA1FontAtlas[{len(atlas)}] = {{\n'
        + format_rows(atlas, 32, 4) + '\n};\n\n'
        f'static const uint16_t kA1FontCluts[{len(cluts)}][16] = {{\n'
        + ',\n'.join('\t{ ' + ', '.join(f'0x{c:04X}' for c in clut) + ' }' for clut in cluts)
        + '\n};\n'
    )


def main():
    atlas, cluts = build()
    if '--stdout' in sys.argv[1:]:
        sys.stdout.buffer.write(atlas)
        for clut in cluts:
            sys.stdout.buffer.write(bytes(c for half in clut for c in (half & 0xFF, half >> 8)))
        return 0
    old = open(DST, encoding='utf-8').read() if os.path.exists(DST) else ''
    text = format_header(atlas, cluts)
    if text != old:
        os.makedirs(os.path.dirname(DST), exist_ok=True)
        open(DST, 'w', encoding='utf-8', newline='\n').write(text)
        print(f'police PS1 : {len(atlas)} octets atlas, {len(cluts)} CLUT -> {DST}')
    else:
        print('police PS1 : a jour')
    return 0


if __name__ == '__main__':
    sys.exit(main())
