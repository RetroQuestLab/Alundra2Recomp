"""Pagination A1 de TOUTES les pages de data/dialogues.txt (stdlib uniquement).

Revue du commit 8b1396c : le rendu coupe au-delà de
kLinesPerPanel lignes, donc une régression de la pagination ou des avances de
police tronquerait du texte sans qu'aucun test ne rougisse. Ce test rejoue
fidèlement wrapUtf8 + paginate (découpe à l'ouverture) puis wrapLatin1 (découpe
au dessin) de src/dialogue.cpp, avec les avances réelles de src/a1_font.h, et
exige pour chaque panneau : au plus kLinesPerPanel lignes, aucune ligne plus
large que kTextMaxW, aucun mot perdu ni ajouté.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
errors: list[str] = []

FOLD = [("œ", "oe"), ("Œ", "OE"), ("’", "'"), ("…", "..."), ("«", '"'), ("»", '"'),
        (" ", " "), ("—", "-"), ("–", "-")]


def constants() -> tuple[int, int]:
    src = (ROOT / "src" / "dialogue.cpp").read_text(encoding="utf-8")
    lines = re.search(r"constexpr size_t kLinesPerPanel = (\d+);", src)
    width = re.search(r"constexpr float kTextMaxW = (\d+)\.f;", src)
    if not lines or not width:
        errors.append("kLinesPerPanel / kTextMaxW introuvables dans src/dialogue.cpp")
        return 3, 254
    return int(lines.group(1)), int(width.group(1))


def advances() -> list[int]:
    header = (ROOT / "src" / "a1_font.h").read_text(encoding="utf-8")
    match = re.search(r"kAdvance\s*=\s*\{\{(.*?)\}\};", header, re.S)
    values = [int(v) for v in re.findall(r"\d+", match.group(1))] if match else []
    if len(values) != 256:
        errors.append(f"kAdvance : {len(values)} valeurs, attendu 256")
        return [8] * 256
    return values


ADV = advances()
LINES, MAXW = constants()


def fold(s: str) -> bytes:
    for a, b in FOLD:
        s = s.replace(a, b)
    return bytes(ord(ch) if ord(ch) < 256 else 63 for ch in s)


def tw(b: bytes) -> int:
    return sum(ADV[c if c >= 32 else 63] for c in b)


def width(s: str) -> int:
    return tw(fold(s))


def wrap_utf8(text: str) -> list[str]:
    lines: list[str] = []
    cur = word = ""

    def push() -> None:
        nonlocal cur, word
        while word and width(word) > MAXW:
            cut = nxt = 0
            while nxt < len(word):
                n = nxt + 1
                if cut > 0 and width(word[:n]) > MAXW:
                    break
                cut = nxt = n
            if cur:
                lines.append(cur)
                cur = ""
            lines.append(word[:cut])
            word = word[cut:]
        if not word:
            return
        cand = word if not cur else cur + " " + word
        if cur and width(cand) > MAXW:
            lines.append(cur)
            cur = word
        else:
            cur = cand
        word = ""

    for c in text:
        if c == " ":
            push()
        elif c == "\n":
            push()
            lines.append(cur)
            cur = ""
        else:
            word += c
    push()
    if cur:
        lines.append(cur)
    return lines


def wrap_latin1(b: bytes) -> list[bytes]:
    lines: list[bytes] = []
    cur = word = b""

    def flush() -> None:
        nonlocal cur, word
        if cur and tw(cur + b" " + word) > MAXW:
            lines.append(cur)
            cur = b""
        while word and tw(word) > MAXW:
            part = b""
            for ch in word:
                if part and tw(part + bytes([ch])) > MAXW:
                    break
                part += bytes([ch])
            if not part:
                break
            lines.append(part)
            word = word[len(part):]
        cur += (b"" if not cur else b" ") + word
        word = b""

    for c in b:
        if c == 10:
            flush()
            lines.append(cur)
            cur = b""
        elif c == 32:
            flush()
        else:
            word += bytes([c])
    flush()
    if cur:
        lines.append(cur)
    return lines


def main() -> int:
    pages = panels = 0
    section = None
    text_path = ROOT / "data" / "dialogues.txt"
    for number, raw in enumerate(text_path.read_text(encoding="utf-8-sig").split("\n"), 1):
        line = raw.rstrip("\r")
        if not line or line[0] == "#":
            continue
        if line[0] == "[" and line[-1] == "]":
            section = line[1:-1]
            continue
        if section is None or section == "prologue":  # prologue : cartes de title.cpp, non paginées
            continue
        if line.startswith("? ") or line.startswith("! "):  # reponses et actions (B02), pas du texte
            continue
        colon = line.find(": ")
        text = line[colon + 2:] if 0 <= colon < 24 else line
        pages += 1
        wrapped = wrap_utf8(text)
        groups = [wrapped[i:i + LINES] for i in range(0, len(wrapped), LINES)] or [[text]]
        for group in groups:
            panels += 1
            shown = wrap_latin1(fold("\n".join(group)))
            if len(shown) > LINES:
                errors.append(f"l.{number} [{section}] : {len(shown)} lignes dans un panneau (max {LINES})")
            for l in shown:
                if tw(l) > MAXW:
                    errors.append(f"l.{number} [{section}] : ligne de {tw(l)} px > {MAXW}")
            if [l.decode("latin1") for l in shown] != [fold(x).decode("latin1") for x in group]:
                errors.append(f"l.{number} [{section}] : découpe au dessin différente de la pagination")
        if fold(" ".join(text.split())).replace(b" ", b"") != fold("".join(wrapped)).replace(b" ", b""):
            errors.append(f"l.{number} [{section}] : texte perdu ou ajouté par la pagination")
    if pages < 100:
        errors.append(f"seulement {pages} pages lues dans data/dialogues.txt")
    if errors:
        for e in errors[:40]:
            print(f"ERR: {e}")
        print(f"ERR: {len(errors)} | pagination des dialogues non conforme")
        return 1
    print(f"ERR: 0 | {pages} pages -> {panels} panneaux de {LINES} lignes au plus, aucun texte perdu")
    return 0


if __name__ == "__main__":
    sys.exit(main())
