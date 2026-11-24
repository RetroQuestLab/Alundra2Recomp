"""Verrouille l'export de l'animation ogre (blob P1AN) et sa reservation VRAM.

Usage : py -3.12 tools/ps1_export_enemy_test.py
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import ps1_export_actor as pa
import ps1_export_zone as pz


def overlaps(a, b):
    return (a[0] < b[0] + b[2] and b[0] < a[0] + a[2]
            and a[1] < b[1] + b[3] and b[1] < a[1] + a[3])


def test_enemy_vram_reserved():
    for rect in (pa.ENEMY_TEX, pa.ENEMY_CLUT):
        assert rect in pz.RESERVED_RECTS, f"rectangle animation non reserve : {rect}"
        for other in pz.RESERVED_RECTS:
            if other == rect:
                continue
            assert not overlaps(rect, other), f"VRAM animation en conflit : {rect} / {other}"
    assert pa.ENEMY_TEX[1] >= 256 and pa.ENEMY_CLUT[1] >= 256, "sous les framebuffers"
    assert pa.ENEMY_TEX[2] == 32 and pa.ENEMY_TEX[3] == 64, "scratch 32x64 demi-mots"
    print("test_enemy_vram_reserved: PASS")


def test_enemy_anim_blob_uploadable():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    out = os.path.join(root, 'ps1', 'assets')
    os.makedirs(out, exist_ok=True)
    size = pa.build_enemy_anim(out)
    path = os.path.join(out, 'enemy_anim.dat')
    b = open(path, 'rb').read()
    assert len(b) == size and b[:4] == b'P1AN', "en-tete P1AN invalide"
    count, reserved = struct.unpack_from('<HH', b, 4)
    assert reserved == 0
    assert count == len(pa.ENEMY_FRAME_INDICES) == 6, "six frames A1 attendues"
    p = 8
    pixels, cluts = [], []
    for i in range(count):
        w, h, ox, oy, stride, upload_h = struct.unpack_from('<HHhhHH', b, p)
        p += 12
        tex_x, tex_y, clut_x, clut_y = struct.unpack_from('<HHHH', b, p)
        p += 8
        assert w > 0 and h > 0 and stride >= 8 and upload_h >= h
        assert (tex_x, tex_y) == (pa.ENEMY_TEX[0], pa.ENEMY_TEX[1]), "scratch unique"
        assert (clut_x, clut_y) == (pa.ENEMY_CLUT[0], pa.ENEMY_CLUT[1]), "CLUT unique"
        assert stride <= pa.ENEMY_TEX[2] and upload_h <= pa.ENEMY_TEX[3], "depassement scratch"
        clut = struct.unpack_from('<16H', b, p)
        p += 32
        payload = b[p:p + stride * 2 * upload_h]
        assert len(payload) == stride * 2 * upload_h
        assert any(v != 0 for v in payload), f"frame {i} vide"
        cluts.append(clut)
        pixels.append(payload)
        p += stride * 2 * upload_h
    assert p == len(b), "taille P1AN incoherente"
    assert len(set(pixels)) >= 4, "l'animation doit contenir de vraies frames distinctes"
    assert len(set(cluts)) >= 2, "les palettes A1 des frames 0..5 different"
    print(f"test_enemy_anim_blob_uploadable: PASS octets={len(b)} frames={count}")


if __name__ == '__main__':
    test_enemy_vram_reserved()
    test_enemy_anim_blob_uploadable()
