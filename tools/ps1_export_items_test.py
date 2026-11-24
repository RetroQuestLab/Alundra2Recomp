"""Test hote (TDD) de l'exporteur d'objets PS1.

    py -3.12 tools/ps1_export_items_test.py
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import ps1_export_items as ex

fail = 0
checks = 0


def check(cond, label):
    global fail, checks
    checks += 1
    if not cond:
        print('FAIL : ' + label)
        fail += 1


def main():
    items = ex.parse_items()

    # Le contenu A2 complet est present, avec ses six epees et puissances 1..6.
    check(len(items) >= 55, 'au moins 55 objets A2')
    swords = [(i['code'], i['power']) for i in items if i['kind'] == 'Weapon'
              and not i['code'].startswith('rm_') and i['code'] != 'epee_supreme']
    check(len(swords) == 6, 'six epees A2 (got %d)' % len(swords))
    check(swords == [
        ('long_sword', 1), ('bronze_sword', 2), ('broad_sword', 3),
        ('rune_sword', 4), ('shining_sword', 5), ('rising_sword', 6),
    ], 'puissances A2 1..6')
    codes = {i['code'] for i in items}
    for gate in ('epee_supreme', 'lampe', 'prisme', 'cristal'):
        check(gate in codes, 'objet gate conserve : ' + gate)

    # Aucune arme d'Alundra 1 ne doit entrer dans le contenu A2.
    forbidden = {'dagger', 'fiend_blade', 'iron_flail', 'great_sword',
                 'silver_sword', 'flail', 'bow', 'a1_sword'}
    check(not (codes & forbidden), 'aucune arme A1')
    check(not any(i['name'].lower().startswith('dagger') for i in items), 'aucun nom A1')

    # Regles de jeu : consommables empilables, cles non consommables.
    herb = next(i for i in items if i['code'] == 'herb')
    check(herb['kind'] == 'Consumable' and herb['maxQty'] == 9
          and herb['consumable'] == 1, 'herbe : consommable max 9')
    key = next(i for i in items if i['code'] == 'strange_key')
    check(key['kind'] == 'Key' and key['maxQty'] == 1
          and key['consumable'] == 0, 'cle etrange : cle non consommable')
    ring = next(i for i in items if i['code'] == 'pixie_ring')
    check(ring['kind'] == 'Ring', 'anneau de Pixie : Ring')

    # Les champs POD doivent contenir le texte repli plus le terminating nul.
    import ps1_export_dialogue as dlg
    check(max(len(dlg.fold(i['code'])) for i in items) < 24, 'code dans PSI_MAX_CODE')
    check(max(len(dlg.fold(i['name'])) for i in items) < 36, 'nom dans PSI_MAX_NAME')
    check(max(len(dlg.fold(i['desc'])) for i in items) < 96, 'description dans PSI_MAX_DESC')

    # L'header genere est un POD pur, sans octet UTF-8 brut (police FONT3 = Latin-1).
    header = ex.format_header(items)
    check('PSIItemDb kPsiDb' in header, 'table kPsiDb present')
    check('#include "inventory_ps1_logic.h"' in header, 'include du header POD')
    check(all(b < 128 for b in header.encode('utf-8')), 'header ASCII / echappements octaux')
    check(header == ex.format_header(items), 'generation deterministe')

    if fail == 0:
        print('PS1_EXPORT_ITEMS %d/%d' % (checks, checks))
    return 1 if fail else 0


if __name__ == '__main__':
    sys.exit(main())
