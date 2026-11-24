"""Genere l'atlas d'icones A1 natif pour le menu d'inventaire PS1.

Les 84 frames de assets/sprites/a1_icons.a1s sont quantifies en 4bpp avec UNE
CLUT partagee de 16 couleurs (index 0 = transparent), rangees dans une grille
12 colonnes x 7 lignes de cellules 24x32 px a (x=896, y=256) en demi-mots VRAM.
Les couleurs sont exclusivement les vrais pixels A1 : au-dela de 15 couleurs
opaque distinctes, on replie sur la couleur reelle la plus proche (aucune
couleur inventee).

Sorties :
    ps1/assets/icons.dat          blob P1IC (non commit)
    src/ps1/icons_data_ps1.h      table POD code A2 -> frame A1 (genere)

Usage :
    py -3.12 tools/ps1_export_icons.py
    py -3.12 tools/ps1_export_icons_test.py     # test hote TDD
"""
import os
import struct
import sys

from ps1_export_font import REPO
from ps1_export_zone import parse_a1s, to_bgr555

SOURCE = os.path.join(REPO, 'assets', 'sprites', 'a1_icons.a1s')
BANKS = os.path.join(REPO, 'assets', 'ui', 'a1_menu', 'banks.txt')
ITEM_ICONS = os.path.join(REPO, 'data', 'item_icons.txt')
ICON_DAT = os.path.join(REPO, 'ps1', 'assets', 'icons.dat')
HEADER_DST = os.path.join(REPO, 'src', 'ps1', 'icons_data_ps1.h')

ATLAS_X = 896          # demi-mots VRAM (libre apres les slots acteurs)
ATLAS_Y = 256          # lignes VRAM (sous les framebuffers)
COLS = 12
CELL_W = 6             # 24 px 4bpp = 6 demi-mots par ligne
CELL_H = 32            # hauteur maximale d'une frame
CLUT_X = 896
CLUT_Y = 480           # juste apres la dernier ligne de la grille (256+7*32)
STRIDE = CELL_W        # 6 demi-mots = 12 octets par ligne


def parse_bank(path=SOURCE):
    """Renvoie le bank A1SP avec, en plus, la table nom -> index de frame."""
    bank = dict(parse_a1s(path))
    names = {}
    if os.path.exists(BANKS):
        with open(BANKS, encoding='utf-8') as f:
            for raw in f:
                line = raw.strip()
                if not line or line.startswith('#'):
                    continue
                parts = [p.strip() for p in line.split('|')]
                if len(parts) >= 3 and parts[0] == 'a1_icons':
                    names[parts[2]] = int(parts[1])
    bank['names'] = names
    return bank


def _shared_palette(bank):
    """15 couleurs opaques reelles, triees par freq puis par valeur (deterministe)."""
    freq = {}
    for f in bank['frames']:
        rgba = f['rgba']
        for i in range(f['w'] * f['h']):
            if rgba[i * 4 + 3] < 128:
                continue
            c = (int(rgba[i * 4]), int(rgba[i * 4 + 1]), int(rgba[i * 4 + 2]))
            freq[c] = freq.get(c, 0) + 1
    colors = sorted(freq, key=lambda c: (-freq[c], c))[:15]
    clut = [0] * 16
    for i, (r, g, b) in enumerate(colors, 1):
        clut[i] = to_bgr555(r, g, b) & 0x7FFF
    return colors, clut


def build_clut(bank):
    """CLUT 16 mots partagee par toutes les frames (entree 0 = transparent)."""
    return _shared_palette(bank)[1]


def _pack_frame(frame, colors):
    """Quantifie une frame sur la palette partagee, stride=6 demi-mots/ligne."""
    w, h = frame['w'], frame['h']
    rgba = frame['rgba']
    upload_h = ((h + 15) // 16) * 16  # multiple de 16 pour la regle LoadImage
    out = bytearray(STRIDE * 2 * upload_h)
    for y in range(h):
        row = y * STRIDE * 2
        for x in range(w):
            i = (y * w + x) * 4
            if rgba[i + 3] < 128:
                idx = 0
            else:
                c = (int(rgba[i]), int(rgba[i + 1]), int(rgba[i + 2]))
                idx = 1 + min(range(len(colors)),
                             key=lambda k: sum((c[j] - colors[k][j]) ** 2 for j in range(3)))
            o = row + x // 2
            if (x & 1) == 0:
                out[o] = (out[o] & 0xF0) | idx
            else:
                out[o] = (out[o] & 0x0F) | (idx << 4)
    return bytes(out), STRIDE, upload_h


def build_blob(bank):
    """Blob P1IC : 8 octets d'en-tete, puis 84 enregistrements 56 octets + pixels."""
    colors, clut = _shared_palette(bank)
    head = b'P1IC' + struct.pack('<HH', bank['nFrames'], 0)
    body = bytearray()
    for i, f in enumerate(bank['frames']):
        pixels, stride, upload_h = _pack_frame(f, colors)
        col, row = i % COLS, i // COLS
        tex_x = ATLAS_X + col * CELL_W
        tex_y = ATLAS_Y + row * CELL_H
        # Header : dims/pivots, stride/hauture de transfert, placement VRAM,
        # reserved/slot, puis la CLUT partagee (32 octets).
        body += struct.pack('<6H', f['w'], f['h'], f['ox'], f['oy'], stride, upload_h)
        body += struct.pack('<4H', tex_x, tex_y, CLUT_X, CLUT_Y)
        body += struct.pack('<2H', 0, i)
        body += struct.pack('<16H', *clut)
        body += pixels
    return bytes(head) + bytes(body)


def build_mapping():
    """Table code A2 -> index de frame A1, issue de banks.txt + item_icons.txt."""
    names = parse_bank()['names']
    out = []
    seen = set()
    with open(ITEM_ICONS, encoding='utf-8') as f:
        for raw in f:
            line = raw.strip()
            if not line or line.startswith('#'):
                continue
            code, icon = (p.strip() for p in line.split('|'))
            if code.startswith('rm_') or icon.startswith('rm:'):
                continue  # icones du bank remake : hors atlas A1
            frame = names.get(icon)
            if frame is None or not (0 <= frame < 84):
                continue
            if code in seen:
                continue
            seen.add(code)
            out.append((code, frame))
    return out


def _cstr(s):
    """Chaine C ASCII : octets non ASCII echappes en octal."""
    r = []
    for b in s.encode('latin-1'):
        if b == 0x22:
            r.append('\\"')
        elif b == 0x5C:
            r.append('\\\\')
        elif 32 <= b <= 126:
            r.append(chr(b))
        else:
            r.append('\\%03o' % b)
    return '"' + ''.join(r) + '"'


def format_header(mapping):
    rows = ['\t{ ' + _cstr(code) + ', ' + str(frame) + ' },' for code, frame in mapping]
    return (
        '// GENERE par tools/ps1_export_icons.py -- ne pas editer a la main.\n'
        '// Source : a1_icons.a1s + banks.txt + item_icons.txt (A1 non publie).\n'
        '#pragma once\n\n'
        '#include <stdint.h>\n\n'
        'struct PSIIconEntry {\n'
        '\tchar code[24];\n'
        '\tuint8_t frame;\n'
        '};\n\n'
        'static const PSIIconEntry kPsiIcons[] = {\n'
        + '\n'.join(rows) + '\n'
        '};\n\n'
        'static const int kPsiIconCount = ' + str(len(mapping)) + ';\n'
    )


def main():
    bank = parse_bank()
    blob = build_blob(bank)
    header = format_header(build_mapping())
    os.makedirs(os.path.dirname(ICON_DAT), exist_ok=True)
    old_blob = open(ICON_DAT, 'rb').read() if os.path.exists(ICON_DAT) else None
    if old_blob != blob:
        with open(ICON_DAT, 'wb') as f:
            f.write(blob)
    old_header = open(HEADER_DST, encoding='ascii').read() if os.path.exists(HEADER_DST) else ''
    if old_header != header:
        with open(HEADER_DST, 'w', encoding='ascii', newline='\n') as f:
            f.write(header)
    print('icones PS1 : %d frames, %d entrees -> %s' %
          (bank['nFrames'], len(build_mapping()), ICON_DAT))
    return 0


if __name__ == '__main__':
    sys.exit(main())
