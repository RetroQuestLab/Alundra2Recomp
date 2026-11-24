"""Exporte les sprites PNJ A1 des zones PS1 011/013/019/119.

Usage :
    py -3.12 tools/ps1_export_npc.py <dossier_sortie>

Une seule zone est active a la fois : chaque PNJ declare sa zone et un slot
VRAM 0..2. interactionSetZonePS1() ne televerse que les sprites de la zone
courante dans les trois emplacements PF_NPC*_TEX et PF_NPC_CLUTS.

Les frames sont choisies dans le groupe IDLE de chaque banque A1SP, dans la
direction indiquee par data/world.txt (0=haut, 1=gauche, 2=droite, 3=bas).
Aucune couleur inventee : pixels et CLUT viennent des vrais sprites A1.

Format (little-endian) :
    "P1NP", u16 count, u16 reserved
    pour chaque PNJ :
        u16 w, h, ox, oy, stride, upload_h   (pixels a l'ecran)
        u16 texX, texY, clutX, clutY         (VRAM)
        u16 zone, u16 slot                   (appartenance / emplacement)
        u16 clut[16]
        octets stride*2*upload_h
"""
import os
import struct
import sys

from ps1_export_zone import (NPC0_TEX, NPC1_TEX, NPC2_TEX, NPC_CLUTS,
                             pad_rows, parse_a1s, quantize)

# (fichier A1SP, index de frame IDLE, zone, slot VRAM)
SOURCE = {
    'prunewell': ('assets/sprites/ancien.a1s', 2, 11, 0),
    'garde': ('assets/sprites/villageois.a1s', 0, 11, 1),
    'finrod': ('assets/sprites/ancien.a1s', 4, 13, 0),
    'marchand_kindra': ('assets/sprites/villageois.a1s', 4, 13, 1),
    'alexia_epave': ('assets/sprites/alexia.a1s', 10, 19, 0),
    'belgar_epave': ('assets/sprites/belgar.a1s', 2, 19, 1),
    'natasha': ('assets/sprites/natasha.a1s', 2, 119, 0),
    'jeehan': ('assets/sprites/jeehan.a1s', 5, 119, 1),
    'alexia_kindra': ('assets/sprites/alexia.a1s', 10, 13, 2),
    # Vallee de Mithra (data/world.txt : npc 026 25 22 villageois gauche guide_vallee)
    'guide_vallee': ('assets/sprites/villageois.a1s', 0, 26, 0),
    # Ruines d'Eden (data/world.txt : npc 042 20 51 ancien / 40 44 villageoise)
    'roswell': ('assets/sprites/ancien.a1s', 2, 42, 0),
    'ome': ('assets/sprites/villageoise.a1s', 0, 42, 1),
    # Puerto Medusa : 3 emplacements VRAM, PNJ narratifs (data/world.txt zone 050).
    'ruby_puerto': ('assets/sprites/ruby.a1s', 0, 50, 0),
    # Zeppo (capitaine.a1s, 39 px) depasse l'emplacement PNJ : Albert a la place.
    'albert_puerto': ('assets/sprites/albert.a1s', 0, 50, 1),
    'zach_fort': ('assets/sprites/zach.a1s', 0, 50, 2),
    # Manoir de Milena (data/world.txt zone 086).
    'diaz_manoir': ('assets/sprites/diaz.a1s', 0, 86, 0),
    'milena': ('assets/sprites/milena.a1s', 0, 86, 1),
    'alexia_manoir': ('assets/sprites/alexia.a1s', 10, 86, 2),
}
DEST = 'npc.dat'
SLOT_RECTS = (NPC0_TEX, NPC1_TEX, NPC2_TEX)


def select_frame(bank, frame_index):
    """Verifie que la frame existe (un index vide serait un sprite invisible)."""
    if frame_index < 0 or frame_index >= bank['nFrames']:
        raise ValueError(f'frame PNJ absente : {frame_index}/{bank["nFrames"]}')
    return bank['frames'][frame_index]


def build(out_dir):
    blobs = []
    for name, (path, frame_index, zone, slot) in SOURCE.items():
        frame = select_frame(parse_a1s(path), frame_index)
        packed, clut = quantize(frame['rgba'], frame['w'], frame['h'])
        pixels, stride, upload_h = pad_rows(packed, frame['w'], frame['h'])
        blobs.append((name, frame, pixels, stride, upload_h, clut, zone, slot))

    clut_x, clut_y = NPC_CLUTS[0], NPC_CLUTS[1]
    out = bytearray(b'P1NP')
    out += struct.pack('<HH', len(blobs), 0)
    for name, frame, pixels, stride, upload_h, clut, zone, slot in blobs:
        rect = SLOT_RECTS[slot]
        tex_x, tex_y = rect[0], rect[1]
        if stride > rect[2] or upload_h > rect[3]:
            raise ValueError(f'PNJ {name} trop grand pour le slot {slot}')
        out += struct.pack('<HHhhHH', frame['w'], frame['h'], frame['ox'],
                           frame['oy'], stride, upload_h)
        out += struct.pack('<HHHHHH', tex_x, tex_y, clut_x, clut_y + slot, zone, slot)
        out += struct.pack('<16H', *clut)
        out += pixels
        print(f'PNJ {name} : {frame["w"]}x{frame["h"]} px, zone {zone}, '
              f'slot {slot}, VRAM=({tex_x},{tex_y})')

    path = os.path.join(out_dir, DEST)
    with open(path, 'wb') as f:
        f.write(out)
    print(f'npc.dat : {len(out)} octets, {len(blobs)} PNJ -> {path}')
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
