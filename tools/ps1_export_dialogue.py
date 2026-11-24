"""Genere les panneaux de dialogue PS1 a partir de data/dialogues.txt.

La narration reste Alundra 2 ; seule la mise en page est Alundra 1 :
  * repli UTF-8 -> Latin-1 identique a foldAccents() de src/dialogue.cpp ;
  * coupure au mot puis a la lettre selon les avances reelles de FONT3.TIM ;
  * 254 px de large, 3 lignes par panneau, comme le rendu PC de reference.

Sortie : src/ps1/dialogue_data_ps1.h, tableaux POD sans STL (toolchain MIPS).
Plusieurs identifiants sont exportes : `kDlgLines`/`kDlgPanelCount` restent
l'alias de `prunewell` (compatibilite avec les tests et captures v0.3.0), et
`kDlgTable` donne acces a tous les dialogues de la premiere interaction.

Usage :
    py -3.12 tools/ps1_export_dialogue.py
"""

import os
import sys

from ps1_export_font import REPO, read_numbers

SOURCE = os.path.join(REPO, 'data', 'dialogues.txt')
FONT = os.path.join(REPO, 'src', 'a1_font.h')
DST = os.path.join(REPO, 'src', 'ps1', 'dialogue_data_ps1.h')

DIALOGUE_ID = 'prunewell'
DIALOGUE_IDS = ('prunewell', 'garde', 'finrod', 'marchand_kindra',
                'alexia_epave', 'belgar_epave', 'toroledo_natasha', 'jeehan',
                'objet_strange_key', 'alexia_epave_cle', 'victoire',
                'objet_lettre', 'mephisto_arrivee', 'mephisto_trone',
                'mephisto_vaincu', 'roi_trone', 'roi_trone_mephisto_vaincu',
                'alexia_finale', 'alexia_finale_mephisto_vaincu',
                 'finrod_prunewell',
                 'alexia_kindra', 'objet_ordre', 'jeehan_objet_ordre',
                 'vallee_arrivee', 'guide_vallee', 'boss_026', 'objet_lampe',
                 'eden_arrivee')
# Indications de commandes propres au clavier PC, adaptees a la manette PS1.
PS1_CONTROL_HINTS = (
    (' Touche Q pour la sortir.', ' Equipe-la avec START.'),
)
MAX_W = 254
LINES_PER_PANEL = 3
LINE_CAP = 64

# Ordre important : les sequences multi-octets avant les caracteres simples.
FOLDS = (
    (b'\xc5\x93', b'oe'), (b'\xc5\x92', b'OE'), (b'\xe2\x80\x99', b"'"),
    (b'\xe2\x80\xa6', b'...'), (b'\xc2\xab', b'"'), (b'\xc2\xbb', b'"'),
    (b'\xc2\xa0', b' '), (b'\xe2\x80\x94', b'-'), (b'\xe2\x80\x93', b'-'),
)


def advances():
    """Les 256 avances FONT3, en pixels, telles que le PC les utilise."""
    return read_numbers(open(FONT, encoding='utf-8').read(), 'kAdvance = {{', 256)


def fold(text):
    """UTF-8 -> octets Latin-1 (un octet = un index de glyphe FONT3)."""
    data = text.encode('utf-8')
    out = bytearray()
    i = 0
    while i < len(data):
        for src, dst in FOLDS:
            if data.startswith(src, i):
                out.extend(dst)
                i += len(src)
                break
        else:
            c = data[i]
            if c < 0x80:
                out.append(c)
                i += 1
            elif c == 0xC3 and i + 1 < len(data):
                out.append(0xC0 + (data[i + 1] & 0x3F))
                i += 2
            else:
                out.append(ord('?'))
                i += 1
                while i < len(data) and (data[i] & 0xC0) == 0x80:
                    i += 1
    return bytes(out)


def wrap(text, adv, max_width=MAX_W):
    """Port exact de wrapLatin1() : largeur reelle, sans reduire la police."""
    lines = []
    current = bytearray()
    word = bytearray()

    def width(bs):
        return sum(adv[c] for c in bs)

    def flush():
        nonlocal current, word
        if current and width(current + b' ' + word) > max_width:
            lines.append(bytes(current))
            current.clear()
        while word and width(word) > max_width:
            part = bytearray()
            for ch in word:
                if part and width(part + bytes((ch,))) > max_width:
                    break
                part.append(ch)
            if not part:
                break
            lines.append(bytes(part))
            word = word[len(part):]
        if word:
            if current:
                current.append(0x20)
            current.extend(word)
        word.clear()

    for c in text:
        if c == 0x0A:
            flush()
            lines.append(bytes(current))
            current.clear()
        elif c == 0x20:
            flush()
        else:
            word.append(c)
    flush()
    if current:
        lines.append(bytes(current))
    return lines


def source_lines(dialogue_id=DIALOGUE_ID):
    """Recupere les repliques du dialogue A2 voulu, sans le nom du locuteur.

    Les directives PC (`! si`, `? choix`) ne sont pas des repliques : la cible
    PS1 n'a pas encore de systeme de choix, elles sont donc ignorees.
    """
    text = open(SOURCE, encoding='utf-8').read().splitlines()
    inside = False
    out = []
    for raw in text:
        line = raw.strip()
        if line.startswith('['):
            inside = line == '[' + dialogue_id + ']'
            continue
        if inside and line and not line.startswith(('#', '!', '?')):
            line = line.split(': ', 1)[-1] if ': ' in line else line
            for pc, ps1 in PS1_CONTROL_HINTS:
                line = line.replace(pc, ps1)
            out.append(line)
    return out


def build_panels(dialogue_id=DIALOGUE_ID):
    adv = advances()
    lines = []
    for line in source_lines(dialogue_id):
        lines.extend(wrap(fold(line), adv))
    panels = []
    for i in range(0, len(lines), LINES_PER_PANEL):
        panel = lines[i:i + LINES_PER_PANEL]
        panel += [b''] * (LINES_PER_PANEL - len(panel))
        panels.append(tuple(panel))
    return panels


def cstr(bs):
    r"""Chaine C ASCII : octets Latin-1 en echappement octal (jamais \x ambigu)."""
    out = []
    for b in bs:
        if b == 0x22:
            out.append('\\"')
        elif b == 0x5C:
            out.append('\\\\')
        elif 32 <= b <= 126:
            out.append(chr(b))
        else:
            out.append('\\%03o' % b)
    return '"' + ''.join(out) + '"'


def format_array(name, panels):
    rows = []
    for panel in panels:
        cells = ', '.join(cstr(line) for line in panel)
        rows.append('\t{ ' + cells + ' },')
    return ('static const char ' + name + '[][3][' + str(LINE_CAP) + '] = {\n'
            + '\n'.join(rows) + '\n};\n\n')


def format_header(panels_by_id):
    out = [
        '// GENERE par tools/ps1_export_dialogue.py -- ne pas editer a la main.\n'
        '// Source : data/dialogues.txt ' + ', '.join('[' + i + ']' for i in DIALOGUE_IDS)
        + ', narration A2,\n'
        '// pagination A1 : 254 px, 3 lignes par panneau, police FONT3.TIM.\n'
        '#pragma once\n\n'
    ]
    # Compatibilite v0.3.0 : kDlgLines / kDlgPanelCount = le dialogue prunewell.
    out.append(format_array('kDlgLines', panels_by_id[DIALOGUE_ID]))
    out.append('static const int kDlgPanelCount = '
               + str(len(panels_by_id[DIALOGUE_ID])) + ';\n\n')
    names = []
    for dialogue_id in DIALOGUE_IDS:
        suffix = '' if dialogue_id == DIALOGUE_ID else '_' + dialogue_id
        array = 'kDlgLines' + suffix
        count = 'kDlgPanelCount' + suffix
        names.append(array)
        if suffix:
            out.append(format_array(array, panels_by_id[dialogue_id]))
            out.append('static const int ' + count + ' = '
                       + str(len(panels_by_id[dialogue_id])) + ';\n\n')
        else:
            count = 'kDlgPanelCount'
    out.append('struct DlgTableEntry {\n'
               '\tconst char (*lines)[3][' + str(LINE_CAP) + '];\n'
               '\tint panels;\n'
               '};\n\n')
    out.append('static const DlgTableEntry kDlgTable[] = {\n'
               + '\n'.join('\t{ ' + array + ', kDlgPanelCount'
                           + ('' if array == 'kDlgLines' else array[len('kDlgLines'):])
                           + ' },' for array in names)
               + '\n};\n\n')
    out.append('static const char* const kDlgNames[] = {\n'
               + '\n'.join('\t"' + i + '",' for i in DIALOGUE_IDS)
               + '\n};\n\n')
    out.append('static const int kDlgDialogueCount = ' + str(len(DIALOGUE_IDS)) + ';\n')
    return ''.join(out)


def main():
    panels_by_id = {i: build_panels(i) for i in DIALOGUE_IDS}
    text = format_header(panels_by_id)
    if '--stdout' in sys.argv[1:]:
        sys.stdout.write(text)
        return 0
    old = open(DST, encoding='utf-8').read() if os.path.exists(DST) else ''
    if text != old:
        os.makedirs(os.path.dirname(DST), exist_ok=True)
        open(DST, 'w', encoding='utf-8', newline='\n').write(text)
        print('dialogue PS1 : ' + ', '.join(
            i + '=' + str(len(panels_by_id[i])) for i in DIALOGUE_IDS) + ' -> ' + DST)
    else:
        print('dialogue PS1 : a jour')
    return 0


if __name__ == '__main__':
    sys.exit(main())


