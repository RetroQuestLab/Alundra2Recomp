"""Textes francais d'Alundra 2 (dialogues, noms de lieux, menus).

Les textes ne sont PAS compresses : ils sont a la fin de chaque bloc de DATA/GRP_FR.GP (un bloc = une
carte, table de 512 offsets u32 en tete du fichier) et dans DATA/PACK0.PAK (menus, objets ; 768 offsets).
Un caractere = un mot de 16 bits 0xFFcc (cc = Latin-1). Les mots < 0x20 sont des codes :
    0000 fin du message            0001 fin de page (attente du bouton, puis message suivant)
    0002 attente puis suite        0003 saut de ligne
    0008 titre centre (nom de lieu, message systeme)      000A nom du heros (Flint)
Avant le texte d'un bloc : une table d'offsets u32 (0xFFFFFFFF = message absent), relatifs au 1er message.

    python tools/a2_text.py <GRP_FR.GP ou PACK0.PAK> <sortie.txt> [nb_offsets]

Sortie : une section "## bloc N" par bloc, un message par ligne ; "/" = saut de ligne, "//" = suite,
"|" = page suivante, {FLINT} = nom du heros, [..] = titre, {xx} = autre code (07 choix, 0d nombre,
0e/0f icone de bouton...).
Les textes extraits restent en local (extracted/ n'est jamais publie).
"""
import re
import struct
import sys

CODES = {1: " | ", 2: " // ", 3: " / ", 10: "{FLINT}"}


def is_text(w):
    return (w >> 8 == 0xFF and (w & 0xFF) >= 0x20 and (w & 0xFF) != 0xFF) or w < 0x20


def messages(chunk):
    """Messages d'un bloc : on prend les suites de mots 'texte' contenant au moins 3 lettres d'affilee."""
    words = struct.unpack_from("<%dH" % (len(chunk) // 2), chunk)
    out, cur, letters, best = [], [], 0, 0

    def flush():
        nonlocal cur, letters, best
        if best >= 3:
            text = "".join(cur).strip(" |/")
            if text:
                out.append(text)
        cur, letters, best = [], 0, 0

    title = False
    for w in words:
        if not is_text(w):
            flush()
            title = False
            continue
        if w >> 8 == 0xFF:
            cur.append(chr(w & 0xFF))
            letters += 1
            best = max(best, letters)
            continue
        letters = 0
        if title and w in (0, 1):
            cur.append("]")
            title = False
        if w == 0:
            flush()
        elif w == 8:
            cur.append("[")
            title = True
        elif w in CODES:
            cur.append(CODES[w])
        else:
            cur.append("{%02x}" % w)
    flush()
    return out


def tail(chunk):
    """Fin du bloc faite uniquement de mots 'texte' : c'est la zone des messages d'une carte."""
    n = len(chunk) // 2
    words = struct.unpack_from("<%dH" % n, chunk)
    i = n
    while i > 0 and is_text(words[i - 1]):
        i -= 1
    return chunk[i * 2:n * 2]


def readable(msg):
    """Ecarte les faux positifs (donnees graphiques qui ressemblent a du texte)."""
    plain = re.sub(r"\{[0-9a-f]{2}\}", "", msg.replace("{FLINT}", "Flint"))  # codes : nombre, icone de bouton, choix...
    good = sum(1 for c in plain if c.isalnum() and (c.isascii() or c in COMMON) or c in " .,;:!?'\"-/|[]()")
    return len(plain) >= 2 and good >= 0.9 * len(plain)


COMMON = "àâäçèéêëîïôöùûüÀÇÈÉÊÎÔ"


def blocks(data, count):
    offs = [o for o in struct.unpack_from("<%dI" % count, data) if 0 < o <= len(data)]
    offs = sorted(set(offs)) + [len(data)]
    return [(i, data[a:b]) for i, (a, b) in enumerate(zip(offs, offs[1:]))]


def main():
    data = open(sys.argv[1], "rb").read()
    count = int(sys.argv[3]) if len(sys.argv) > 3 else (768 if sys.argv[1].upper().endswith(".PAK") else 512)
    total = 0
    with open(sys.argv[2], "w", encoding="utf-8") as f:
        for i, chunk in blocks(data, count):
            msgs = [m for m in messages(tail(chunk) if count == 512 else chunk) if readable(m)]
            if not msgs:
                continue
            f.write("## bloc %d\n" % i)
            for m in msgs:
                f.write(m + "\n")
            f.write("\n")
            total += len(msgs)
    print(total, "messages")


if __name__ == "__main__":
    main()
