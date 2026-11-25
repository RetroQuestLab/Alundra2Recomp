"""Bustes des locuteurs pour le dialogue PS1 (comme drawPortrait() du PC).

Usage : py -3.12 tools/ps1_export_portrait.py <dossier_sortie>

Pour chaque banque de ps1_export_dialogue.PORTRAIT_BANKS : frame 0 de
l'animation groupe 0 / direction DOWN (SpriteBank::anim(0, DOWN)), haut du
sprite sur w*20/24 lignes, agrandi au plus proche voisin en 48x56 (le PC
etire le meme rectangle sans lissage). La PS1 ne met pas les SPRT a
l'echelle : l'agrandissement est fait ici, une seule fois.

Blob P1PT : "P1PT", u16 count, u16 reserved, puis par buste
u16 w, h, stride, upload_h, u16 clut[16], octets stride*2*upload_h.
"""
import os
import struct
import sys

from ps1_export_dialogue import PORTRAIT_BANKS
from ps1_export_zone import PORTRAIT_TEX, pad_rows, parse_a1s, quantize

W, H = 48, 56
DEST = 'portrait.dat'


def portrait_rgba(bank):
    anims = bank['anims']
    index = 0
    if anims and anims[0] and anims[0][0]:
        index = anims[0][0][0][0]
    f = bank['frames'][index]
    fw, fh, src = f['w'], f['h'], f['rgba']
    src_h = min(float(fh), fw * 20.0 / 24.0)
    out = bytearray(W * H * 4)
    for y in range(H):
        sy = min(fh - 1, int((y + 0.5) * src_h / H))
        for x in range(W):
            sx = min(fw - 1, int((x + 0.5) * fw / W))
            o = (sy * fw + sx) * 4
            out[(y * W + x) * 4:(y * W + x) * 4 + 4] = src[o:o + 4]
    return bytes(out)


def build(out_dir):
    out = bytearray(b'P1PT')
    out += struct.pack('<HH', len(PORTRAIT_BANKS), 0)
    for name in PORTRAIT_BANKS:
        bank = parse_a1s(f'assets/sprites/{name}.a1s')
        packed, clut = quantize(portrait_rgba(bank), W, H)
        pixels, stride, upload_h = pad_rows(packed, W, H)
        if stride > PORTRAIT_TEX[2] or upload_h > PORTRAIT_TEX[3]:
            raise ValueError(f'buste {name} trop grand pour la zone VRAM reservee')
        out += struct.pack('<HHHH', W, H, stride, upload_h)
        out += struct.pack('<16H', *clut)
        out += pixels
    path = os.path.join(out_dir, DEST)
    with open(path, 'wb') as f:
        f.write(out)
    print(f'portrait.dat : {len(out)} octets, {len(PORTRAIT_BANKS)} bustes -> {path}')
    return len(out)


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    os.makedirs(sys.argv[1], exist_ok=True)
    build(sys.argv[1])
    return 0


if __name__ == '__main__':
    sys.exit(main())
