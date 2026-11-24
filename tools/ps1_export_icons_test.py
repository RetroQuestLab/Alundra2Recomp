"""Test hote (TDD) de l'exporteur d'icones A1 natives pour le menu PS1.

    py -3.12 tools/ps1_export_icons_test.py

Verification du contenu binaire, de la table POD et de la reactivite. Aucune
image n'est "jugee" ici : l'inspection visuelle des frames est rapportee a part.
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import ps1_export_icons as ex  # noqa: E402

fail = 0
checks = 0


def check(cond, label):
    global fail, checks
    checks += 1
    if not cond:
        print('FAIL : ' + label)
        fail += 1


def frames_of(data):
    """Decoupe un blob P1IC en (header_tuple, [frame_dicts])."""
    magic, count, reserved = struct.unpack_from('<4sHH', data, 0)
    o = 8
    out = []
    for i in range(count):
        w, h, ox, oy, stride, upload_h = struct.unpack_from('<6H', data, o)
        o += 12
        tex_x, tex_y, clut_x, clut_y = struct.unpack_from('<4H', data, o)
        o += 8
        res, slot = struct.unpack_from('<2H', data, o)
        o += 4
        clut = list(struct.unpack_from('<16H', data, o))
        o += 32
        payload = data[o:o + stride * 2 * upload_h]
        o += stride * 2 * upload_h
        out.append({'w': w, 'h': h, 'ox': ox, 'oy': oy, 'stride': stride,
                    'upload_h': upload_h, 'tex_x': tex_x, 'tex_y': tex_y,
                    'clut_x': clut_x, 'clut_y': clut_y, 'reserved': res,
                    'slot': slot, 'clut': clut, 'payload': payload})
    return (magic, count, reserved), out, o


def main():
    bank = ex.parse_bank()
    check(bank['nFrames'] == 84, '84 frames dans a1_icons.a1s')

    data = ex.build_blob(bank)
    head, frames, end = frames_of(data)
    check(head[0] == b'P1IC', 'magic P1IC')
    check(head[1] == 84, 'count = 84 (got %d)' % head[1])
    check(head[2] == 0, 'reserved = 0')
    check(end == len(data), 'blob exactement consomme (%d/%d)' % (end, len(data)))
    check(len(frames) == 84, '84 enregistrements')

    # Dimensions natives : 23x32 pour les six premieres frames, puis tailles
    # variables preservees telles quelles (24x31, 16x15, 24x24, 8x8...).
    check(all(frames[i]['w'] == 23 and frames[i]['h'] == 32 for i in range(5)),
          'frames 0..4 en 23x32')
    check(all((f['w'], f['h'], f['ox'], f['oy']) ==
              (bank['frames'][i]['w'], bank['frames'][i]['h'],
               bank['frames'][i]['ox'], bank['frames'][i]['oy'])
              for i, f in enumerate(frames)), 'dimensions/pivots = source A1')

    # Atlas 12x7 cellules de 24x32 px = 6 demi-mots x 32 lignes.
    for i, f in enumerate(frames):
        col, row = i % 12, i // 12
        check(f['tex_x'] == 896 + col * 6 and f['tex_y'] == 256 + row * 32,
              'placement VRAM frame %d' % i)
        check(f['clut_x'] == 896 and f['clut_y'] == 480, 'CLUT (896,480) frame %d' % i)
        check(f['slot'] == i and f['reserved'] == 0, 'slot/reserved frame %d' % i)
        check(len(f['payload']) == f['stride'] * 2 * f['upload_h'], 'payload frame %d' % i)

    # CLUT unique partagee par les 84 frames ; entree 0 = transparent.
    check(frames[0]['clut'][0] == 0, 'index 0 transparent')
    check(all(f['clut'] == frames[0]['clut'] for f in frames), 'CLUT partagee')
    check(any(v != 0 for v in frames[0]['clut'][1:]), 'CLUT non vide')

    # Table POD : ids A2 -> frames A1 reelles, six epees incluses.
    mapping = ex.build_mapping()
    check(len(mapping) >= 20, 'au moins 20 entrees mappees (got %d)' % len(mapping))
    codes = dict((code, frame) for code, frame in mapping)
    for sword in ('long_sword', 'bronze_sword', 'broad_sword',
                  'rune_sword', 'shining_sword', 'rising_sword'):
        check(sword in codes, 'epee A2 mappee : ' + sword)
    for code, frame in (('herb', 68), ('tonic', 16), ('pixie_ring', 13),
                        ('strange_key', 15), ('compass', 34)):
        check(codes.get(code) == frame, '%s -> frame %d' % (code, frame))
    check(all(0 <= frame < 84 for _code, frame in mapping), 'indices frames valides')
    check(not any(code.startswith('rm_') for code, _f in mapping),
          'aucune icone remake (rm:) dans la table A1')

    header = ex.format_header(mapping)
    check('struct PSIIconEntry' in header and 'char code[24]' in header,
          'struct POD PSIIconEntry')
    check('kPsiIcons[]' in header, 'table kPsiIcons')
    check('kPsiIconCount = %d' % len(mapping) in header, 'compteur POD')
    check('"long_sword", 8' in header, 'entree long_sword')
    check(all(b < 128 for b in header.encode('ascii')), 'header ASCII pur')
    check(header == ex.format_header(ex.build_mapping()), 'header deterministe')
    check(ex.build_blob(bank) == data, 'blob reproductible')

    # Si les fichiers generes existent, ils doivent correspondre au generateur.
    if os.path.exists(ex.ICON_DAT):
        on_disk = open(ex.ICON_DAT, 'rb').read()
        check(on_disk == data, 'icons.dat sur disque a jour')
    if os.path.exists(ex.HEADER_DST):
        on_disk = open(ex.HEADER_DST, encoding='ascii').read()
        check(on_disk == header, 'icons_data_ps1.h sur disque a jour')

    if fail == 0:
        print('PS1_EXPORT_ICONS %d/%d' % (checks, checks))
    return 1 if fail else 0


if __name__ == '__main__':
    sys.exit(main())
