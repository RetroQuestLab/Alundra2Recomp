"""Genere la table d'objets PS1 a partir de data/items.txt.

Le contenu reste Alundra 2 (six epees, consommables, anneaux, objets de poche,
cles) ; seuls les octets de texte sont replies en Latin-1 pour la police FONT3.
Aucune arme d'Alundra 1 n'est importee.

Sortie : src/ps1/items_data_ps1.h, POD sans STL (toolchain MIPS PSn00bSDK).

Usage :
    py -3.12 tools/ps1_export_items.py
    py -3.12 tools/ps1_export_items.py --stdout
"""
import os
import sys

from ps1_export_dialogue import cstr, fold
from ps1_export_font import REPO

SOURCE = os.path.join(REPO, 'data', 'items.txt')
DST = os.path.join(REPO, 'src', 'ps1', 'items_data_ps1.h')

KINDS = {
    'Consumable': 0,  # PSI_KIND_CONSUMABLE
    'Key': 1,         # PSI_KIND_KEY
    'Weapon': 2,      # PSI_KIND_WEAPON
    'Ring': 3,        # PSI_KIND_RING
    'Pocket': 4,      # PSI_KIND_POCKET
}


def parse_items(path=SOURCE):
    """Renvoie les objets A2 dans l'ordre exact de data/items.txt."""
    out = []
    with open(path, encoding='utf-8') as f:
        for raw in f:
            line = raw.strip()
            if not line or line.startswith('#'):
                continue
            parts = [p.strip() for p in line.split('|')]
            if len(parts) != 7:
                raise ValueError('ligne objet invalide : ' + line)
            code, name, desc, kind, max_qty, consumable, power = parts
            if kind not in KINDS:
                raise ValueError('kind inconnu : ' + kind)
            out.append({
                'code': code,
                'name': name,
                'desc': desc,
                'kind': kind,
                'maxQty': int(max_qty),
                'consumable': int(consumable),
                'power': int(power),
            })
    return out


def format_header(items):
    rows = []
    for i in items:
        rows.append('\t\t{ ' + ', '.join((
            cstr(fold(i['code'])),
            cstr(fold(i['name'])),
            cstr(fold(i['desc'])),
            str(KINDS[i['kind']]),
            str(i['consumable']),
            str(i['maxQty']),
            str(i['power']),
        )) + ' },')
    return (
        '// GENERE par tools/ps1_export_items.py -- ne pas editer a la main.\n'
        '// Source : data/items.txt (contenu A2), texte repli en Latin-1 pour FONT3.\n'
        '#pragma once\n\n'
        '#include "inventory_ps1_logic.h"\n\n'
        'static const PSIItemDb kPsiDb = {\n'
        '\t{\n' + '\n'.join(rows) + '\n\t},\n'
        '\t' + str(len(items)) + '\n'
        '};\n'
    )


def main():
    text = format_header(parse_items())
    if '--stdout' in sys.argv[1:]:
        sys.stdout.write(text)
        return 0
    old = open(DST, encoding='ascii').read() if os.path.exists(DST) else ''
    if text != old:
        with open(DST, 'w', encoding='ascii', newline='\n') as f:
            f.write(text)
        print('objets PS1 : %d entrees -> %s' % (len(parse_items()), DST))
    return 0


if __name__ == '__main__':
    sys.exit(main())
