"""Exporte les gemmes de vie A1 du HUD pour la cible PS1.

Source : assets/sprites/a1_hud.a1s, banque deja utilisee par le HUD PC
(src/main.cpp::drawHud). Les indexes sont verifies par test_hud_frames_match_pc.

Usage :
    py -3.12 tools/ps1_export_hud.py <dossier_sortie>

Blob P1HU (little-endian) :
    "P1HU", u16 count=15, u16 reserved
    pour chaque frame (grosse pleine, grosse vide, petite pleine, petite vide,
    chiffres 0..9, slash) :
        u16 w, h, ox, oy, stride, upload_h, atlasY, clutY
        u16 clut[16]
        octets stride*2*upload_h
"""
import os
import struct
import sys

from ps1_export_zone import (HUD2_ATLAS, HUD2_CLUTS, HUD_ATLAS, HUD_CLUTS,
                             pad_rows, parse_a1s, quantize)

SOURCE = 'assets/sprites/a1_hud.a1s'
# Meme correspondance que kBigFull/kBigEmpty/kSmallFull/kSmallEmpty et que
# kDigit0..kDigit9/kSlash dans src/main.cpp::drawHud (le "/10" du HUD A1).
FRAMES = (1, 4, 5, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19)
DEST = 'hud.dat'
# Second blob P1HU : case d'objet (kBox), cristaux EP pleins 0..3 et vide
# (kMpFull0..+3, kMpEmpty) et bourse (kGoldBag) de src/main.cpp::drawHud.
FRAMES2 = (0, 20, 21, 22, 23, 24, 32)
OFFSETS2 = (0, 32, 48, 64, 80, 96, 112)
DEST2 = 'hud2.dat'


def build(out_dir):
    bank = parse_a1s(SOURCE)
    blobs = []
    for index in FRAMES:
        if index >= bank['nFrames']:
            raise ValueError(f'frame HUD absente : {index}/{bank["nFrames"]}')
        frame = bank['frames'][index]
        packed, clut = quantize(frame['rgba'], frame['w'], frame['h'])
        pixels, stride, upload_h = pad_rows(packed, frame['w'], frame['h'])
        blobs.append((index, frame, pixels, stride, upload_h, clut))

    atlas_x, atlas_y, atlas_w, atlas_h = HUD_ATLAS
    clut_x, clut_y, _, _ = HUD_CLUTS
    # Gemmes : offsets historiques. Chiffres/slash : 8x16 px empiles en 16
    # lignes a partir de 48, soit 11 * 16 = 176 lignes supplementaires.
    atlas_offsets = (0, 16, 32, 40) + tuple(48 + 16 * i for i in range(11))
    if len(blobs) != len(atlas_offsets):
        raise ValueError('nombre de frames HUD inattendu')

    out = bytearray(b'P1HU')
    out += struct.pack('<HH', len(blobs), 0)
    for i, (index, frame, pixels, stride, upload_h, clut) in enumerate(blobs):
        y = atlas_y + atlas_offsets[i]
        out += struct.pack('<HHhhHHHH', frame['w'], frame['h'], frame['ox'],
                           frame['oy'], stride, upload_h, y, clut_y + i)
        out += struct.pack('<16H', *clut)
        out += pixels
        if stride > atlas_w or atlas_offsets[i] + upload_h > atlas_h:
            raise ValueError(f'frame {index} depasse l atlas HUD reserve')
        print(f'HUD frame {index} : {frame["w"]}x{frame["h"]} px, stride={stride}, '
              f'ATLAS=({atlas_x},{y}) CLUT=({clut_x},{clut_y + i})')

    path = os.path.join(out_dir, DEST)
    with open(path, 'wb') as f:
        f.write(out)
    print(f'hud.dat : {len(out)} octets, {len(blobs)} gemmes -> {path}')
    return len(out)


def build2(out_dir):
    bank = parse_a1s(SOURCE)
    atlas_x, atlas_y, atlas_w, atlas_h = HUD2_ATLAS
    clut_x, clut_y, _, clut_h = HUD2_CLUTS
    if len(FRAMES2) > clut_h:
        raise ValueError('trop de CLUT HUD2')
    out = bytearray(b'P1HU')
    out += struct.pack('<HH', len(FRAMES2), 0)
    for i, index in enumerate(FRAMES2):
        frame = bank['frames'][index]
        packed, clut = quantize(frame['rgba'], frame['w'], frame['h'])
        pixels, stride, upload_h = pad_rows(packed, frame['w'], frame['h'])
        if stride > atlas_w or OFFSETS2[i] + upload_h > atlas_h:
            raise ValueError(f'frame {index} depasse l atlas HUD2 reserve')
        if i + 1 < len(OFFSETS2) and OFFSETS2[i] + upload_h > OFFSETS2[i + 1]:
            raise ValueError(f'frame {index} chevauche la suivante')
        out += struct.pack('<HHhhHHHH', frame['w'], frame['h'], frame['ox'],
                           frame['oy'], stride, upload_h, atlas_y + OFFSETS2[i], clut_y + i)
        out += struct.pack('<16H', *clut)
        out += pixels
    path = os.path.join(out_dir, DEST2)
    with open(path, 'wb') as f:
        f.write(out)
    print(f'hud2.dat : {len(out)} octets, {len(FRAMES2)} frames -> {path}')
    return len(out)


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    os.makedirs(sys.argv[1], exist_ok=True)
    build(sys.argv[1])
    build2(sys.argv[1])
    return 0


if __name__ == '__main__':
    sys.exit(main())
