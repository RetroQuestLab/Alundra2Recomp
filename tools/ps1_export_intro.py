"""Genere les panneaux d'introduction PS1 a partir de data/dialogues.txt [prologue].

La narration reste Alundra 2 ; seule la mise en page est Alundra 1 :
  * repli UTF-8 -> Latin-1 identique a foldAccents() de src/dialogue.cpp ;
  * coupure au mot puis a la lettre selon les avances reelles de FONT3.TIM ;
  * 254 px de large, 3 lignes par panneau, comme le rendu PC de reference.

Sortie : src/ps1/intro_data_ps1.h, tableau POD sans STL (toolchain MIPS).

Usage :
    py -3.12 tools/ps1_export_intro.py
"""

import os
import sys

from ps1_export_font import REPO, read_numbers

SOURCE = os.path.join(REPO, 'data', 'dialogues.txt')
FONT = os.path.join(REPO, 'src', 'a1_font.h')
DST = os.path.join(REPO, 'src', 'ps1', 'intro_data_ps1.h')

DIALOGUE_ID = 'prologue'
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


def source_lines():
    """Recupere les repliques du dialogue A2 voulu, sans le nom du locuteur."""
    text = open(SOURCE, encoding='utf-8').read().splitlines()
    inside = False
    out = []
    for raw in text:
        line = raw.strip()
        if line.startswith('['):
            inside = line == '[' + DIALOGUE_ID + ']'
            continue
        if inside and line and not line.startswith('#'):
            out.append(line.split(': ', 1)[-1] if ': ' in line else line)
    return out


def build_panels():
    adv = advances()
    lines = []
    for line in source_lines():
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


def format_header(panels):
    rows = []
    for panel in panels:
        cells = ', '.join(cstr(line) for line in panel)
        rows.append('\t{ ' + cells + ' },')
    return (
        '// GENERE par tools/ps1_export_intro.py -- ne pas editer a la main.\n'
        '// Source : data/dialogues.txt [' + DIALOGUE_ID + '], narration A2,\n'
        '// pagination A1 : 254 px, 3 lignes par panneau, police FONT3.TIM.\n'
        '#pragma once\n\n'
        'static const char kIntroLines[][3][' + str(LINE_CAP) + '] = {\n'
        + '\n'.join(rows) + '\n'
        '};\n\n'
        'static const int kIntroPanelCount = ' + str(len(panels)) + ';\n'
    )


def main():
    panels = build_panels()
    text = format_header(panels)
    if '--stdout' in sys.argv[1:]:
        sys.stdout.write(text)
        return 0
    old = open(DST, encoding='utf-8').read() if os.path.exists(DST) else ''
    if text != old:
        os.makedirs(os.path.dirname(DST), exist_ok=True)
        open(DST, 'w', encoding='utf-8', newline='\n').write(text)
        print(f'intro PS1 : {len(panels)} panneaux, {DIALOGUE_ID} -> {DST}')
    else:
        print('intro PS1 : a jour')
    return 0


if __name__ == '__main__':
    sys.exit(main())
