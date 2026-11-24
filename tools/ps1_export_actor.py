"""Exporte les acteurs PS1 (jalon boss + salle du trone) en blob P1AC.

Usage :
    py -3.12 tools/ps1_export_actor.py <dossier_sortie>

L'ogre de Kindra et les coffres de data/world.txt utilisent les VRAIES frames
A1SP (aucune couleur inventee). Ils sont plus larges que les PNJ (40x49 contre
25x47), donc trois slots VRAM dedies sont reserves dans ps1_export_zone.py.

Format (little-endian, identique a P1NP) :
    "P1AC", u16 count, u16 reserved
    pour chaque acteur :
        u16 w, h, ox, oy, stride, upload_h   (pixels a l'ecran)
        u16 texX, texY, clutX, clutY         (VRAM)
        u16 zone, u16 slot                   (appartenance / emplacement)
        u16 clut[16]
        octets stride*2*upload_h
"""
import os
import struct
import sys

from ps1_export_zone import (ACTOR0_TEX, ACTOR1_TEX, ACTOR2_TEX,
                             ACTOR3_TEX, ACTOR4_TEX, ACTOR5_TEX, ACTOR_CLUTS,
                             ENEMY_CLUT, ENEMY_TEX,
                             pad_rows, parse_a1s, quantize)

# (fichier A1SP, index de frame, zone, slot VRAM)
SOURCE = {
    'ogre': ('assets/sprites/ogre.a1s', 0, 13, 0),
    'coffre_strange_key': ('assets/sprites/coffre.a1s', 0, 19, 1),
    'coffre_lettre': ('assets/sprites/coffre.a1s', 2, 13, 2),
    'mephisto': ('assets/sprites/mephisto.a1s', 0, 120, 3),
    'alexia_trone': ('assets/sprites/alexia.a1s', 0, 120, 4),
    'roi_trone': ('assets/sprites/roi.a1s', 0, 120, 5),
    # Ordres de Diaz a Toroledo (data/world.txt : chest 119 25 1 ordre). Une
    # seule zone est active : le slot 1 (coffre de l'epave, zone 19) est libre.
    'coffre_ordre': ('assets/sprites/coffre.a1s', 0, 119, 1),
}
DEST = 'actor.dat'
SLOT_RECTS = (ACTOR0_TEX, ACTOR1_TEX, ACTOR2_TEX,
              ACTOR3_TEX, ACTOR4_TEX, ACTOR5_TEX)

# Animation d'apparence de l'ogre (v0.8.8) : six vraies frames A1 de ogre.a1s,
# pixels en RAM dans un blob P1AN separe, une seule frame a la fois dans un
# scratch VRAM dedie. Aucun slot de actor.dat n'est modifie.
ENEMY_FRAME_INDICES = (0, 1, 2, 3, 4, 5)
ENEMY_DEST = 'enemy_anim.dat'


def select_frame(bank, frame_index):
    if frame_index < 0 or frame_index >= bank['nFrames']:
        raise ValueError(f'frame acteur absente : {frame_index}/{bank["nFrames"]}')
    return bank['frames'][frame_index]


def build(out_dir):
    blobs = []
    for name, (path, frame_index, zone, slot) in SOURCE.items():
        frame = select_frame(parse_a1s(path), frame_index)
        packed, clut = quantize(frame['rgba'], frame['w'], frame['h'])
        pixels, stride, upload_h = pad_rows(packed, frame['w'], frame['h'])
        blobs.append((name, frame, pixels, stride, upload_h, clut, zone, slot))

    clut_x, clut_y = ACTOR_CLUTS[0], ACTOR_CLUTS[1]
    out = bytearray(b'P1AC')
    out += struct.pack('<HH', len(blobs), 0)
    for name, frame, pixels, stride, upload_h, clut, zone, slot in blobs:
        rect = SLOT_RECTS[slot]
        tex_x, tex_y = rect[0], rect[1]
        if stride > rect[2] or upload_h > rect[3]:
            raise ValueError(f'acteur {name} trop grand pour le slot {slot}')
        out += struct.pack('<HHhhHH', frame['w'], frame['h'], frame['ox'],
                           frame['oy'], stride, upload_h)
        out += struct.pack('<HHHHHH', tex_x, tex_y, clut_x, clut_y + slot, zone, slot)
        out += struct.pack('<16H', *clut)
        out += pixels
        print(f'acteur {name} : {frame["w"]}x{frame["h"]} px, zone {zone}, '
              f'slot {slot}, VRAM=({tex_x},{tex_y})')

    path = os.path.join(out_dir, DEST)
    with open(path, 'wb') as f:
        f.write(out)
    print(f'actor.dat : {len(out)} octets, {len(blobs)} acteurs -> {path}')
    return len(out)


def build_enemy_anim(out_dir):
    """Exporte les six frames d'animation de l'ogre dans un blob P1AN.

    Format (little-endian) : "P1AN", u16 count, u16 reserved, puis par frame
    6*u16 (w,h,ox,oy,stride,uploadH), 4*u16 (texX,texY,clutX,clutY),
    clut[16] (16*u16) et stride*2*uploadH octets de pixels 4bpp.
    """
    bank = parse_a1s('assets/sprites/ogre.a1s')
    out = bytearray(b'P1AN')
    out += struct.pack('<HH', len(ENEMY_FRAME_INDICES), 0)
    for index in ENEMY_FRAME_INDICES:
        frame = select_frame(bank, index)
        packed, clut = quantize(frame['rgba'], frame['w'], frame['h'])
        pixels, stride, upload_h = pad_rows(packed, frame['w'], frame['h'])
        if stride > ENEMY_TEX[2] or upload_h > ENEMY_TEX[3]:
            raise ValueError(f'frame ogre {index} trop grande pour le scratch')
        out += struct.pack('<HHhhHH', frame['w'], frame['h'], frame['ox'],
                           frame['oy'], stride, upload_h)
        out += struct.pack('<HHHH', ENEMY_TEX[0], ENEMY_TEX[1],
                           ENEMY_CLUT[0], ENEMY_CLUT[1])
        out += struct.pack('<16H', *clut)
        out += pixels
        print(f'ogre frame {index} : {frame["w"]}x{frame["h"]} px, '
              f'stride={stride}, upload_h={upload_h}')
    path = os.path.join(out_dir, ENEMY_DEST)
    with open(path, 'wb') as f:
        f.write(out)
    print(f'{ENEMY_DEST} : {len(out)} octets, {len(ENEMY_FRAME_INDICES)} frames -> {path}')
    return len(out)


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    os.makedirs(sys.argv[1], exist_ok=True)
    build(sys.argv[1])
    build_enemy_anim(sys.argv[1])
    return 0


if __name__ == '__main__':
    sys.exit(main())
