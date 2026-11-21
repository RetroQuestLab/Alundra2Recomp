"""Flint = sprites d'Alundra 1 retouches a la main, image par image.

Chaque image du heros A1 est recoloree par matiere (regles par bandes de hauteur), puis
une retouche dessinee a la main (RETOUCHES) remplace des lignes entieres : epis,
echarpe, epauliere... La resolution et les animations d'A1 sont conservees.

    python tools/flint_retouch.py <DATAS.BIN> assets/sprites/flint.a1s [apercu.png]
"""
import os
import struct
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(__file__))
from a1_sprites import HERO_GROUPS, WEAPON_A2_COLORS, SpriteBank, recolor_palette  # noqa: E402

LETTERS = ".0123456789abcdefghijklmnopqrstuvwxyz"

FLINT = {
    "1": (8, 8, 16),
    # cheveux roux
    "q": (60, 10, 14), "r": (128, 26, 24), "R": (198, 58, 38), "H": (244, 112, 62), "h": (255, 186, 134),
    # haut orange
    "o": (82, 30, 10), "O": (170, 70, 18), "Y": (236, 128, 34), "y": (255, 196, 110),
    # echarpe bleue
    "j": (20, 26, 92), "k": (42, 70, 168), "l": (86, 128, 230), "m": (164, 198, 252),
    # pantalon bleu
    "n": (26, 30, 88), "N": (46, 60, 156), "M": (98, 120, 224), "Z": (166, 182, 246),
    # cuir
    "t": (42, 20, 10), "T": (92, 54, 28), "u": (142, 92, 52), "U": (194, 142, 92),
    "G": (240, 196, 70),  # boucle doree
    # semelles et epauliere violettes
    "v": (72, 52, 150), "V": (128, 100, 222),
    "p": (64, 42, 122), "P": (114, 88, 202), "Q": (170, 148, 244), "X": (222, 210, 255),
}

# Retouches dessinees a la main sur une image de reference par direction :
# {image: {ligne: texte}} (numeros de ligne de l'image A1 d'origine, negatif = au-dessus).
# Elles sont reportees automatiquement sur les autres images de la meme direction quand
# la tete (ou la ligne d'origine) correspond exactement.
SPIKES_SIDE = {
    2: "............q...q........",
    3: "........q..qRq.qRq.......",
    4: "........qRqRHRqRRq.......",
    5: "......qRRRHHRHHRRq.......",
}
RETOUCHES = {
    0: {
        -3: "..........q....q.........",
        -2: "......q..qRq..qHq..q.....",
        -1: "........qRRqRHHqRRq......",
        0:  ".......qRRRHHRHHRRRq.....",
        12: "....1kllk148998lkk1pp1...",
        13: "...1jkllmllllkkkj1pQXP1..",
        14: "...1yYOjklkjjjjjj1PQQP1..",
        15: "...1OYOjklOYYYOOo1pPPp1..",
        16: "..41oOOjkjOYOOOooo1pp14..",
    },
    3: {
        -3: "..........q....q.........",
        -2: "......q..qRq..qHq..q.....",
        -1: "........qRRqRHHqRRq......",
        0:  ".......qRRRHHRHHRRRq.....",
        11: "......qrRRrRRRRRrRRq.....",
        12: ".....1jkllkllllkkkkj1....",
        13: "....1yY1jkkkkkkkkj1YO1...",
    },
    6: dict(SPIKES_SIDE),
    9: {y: (line[::-1][2:] + "..") for y, line in SPIKES_SIDE.items()},
}
BASE = {0: 0, 1: 3, 2: 6, 3: 9}
# Epauliere : collee partout ou la tete de reference est retrouvee ('.' = on garde le pixel)
PADS = {
    0: {12: "...................pp....", 13: ".................1pQXP1..",
        14: ".................1PQQP1..", 15: ".................1pPPp1..", 16: "..................1pp1..."},
    1: {13: "...1pp1..................", 14: "..1pQXP1.................",
        15: "..1PQQP1.................", 16: "..1pPPp1.................", 17: "...1pp1.................."},
}
HEAD = 9  # lignes de cheveux comparees pour retrouver la tete


def material(i, y, h, white_eye, top, d):
    """Couleur Flint d'un index de palette A1 selon la hauteur dans l'image."""
    if i == 1:
        return "1"
    y -= top
    h -= top
    if y <= 11:  # tete
        if i in (4, 5, 6, 7):
            return "qrRH"[i - 4]
        if i == 15:
            return None if white_eye else "h"
        return None
    if y >= h - 8:  # bottes
        sole = y >= h - 4
        if i in (4, 5, 6, 7, 8, 9, 10, 15):
            if sole and i in (9, 10, 15, 7):
                return "V"
            if sole and i in (6, 8):
                return "v"
            return {4: "t", 5: "T", 6: "T", 7: "u", 8: "T", 9: "u", 10: "U", 15: "U"}[i]
        return {2: "N", 3: "M", 11: "n", 12: "N", 13: "M", 14: "Z"}.get(i)
    if y < 21:  # torse
        if d == 1 and i in (4, 5, 6, 7):
            # de dos : le sac devient le dos du haut, sans les coutures du sac
            return {4: "O", 5: "O", 6: "Y", 7: "Y", 8: "O", 9: "Y", 10: "y"}[i]
        if i in (4, 5, 6, 7):
            # meches et sac sur le torse -> haut orange
            return "oOYy"[i - 4]
        return {11: "o", 12: "O", 13: "Y", 14: "y", 2: "O", 3: "Y", 15: "y"}.get(i)
    if y <= 23:  # ceinture
        if i in (4, 5, 6):
            return "tTu"[i - 4]
        if i == 7:
            return "G"
    return {11: "n", 12: "N", 13: "M", 14: "Z", 2: "N", 3: "M", 15: "Z"}.get(i)  # jambes


def grid(img, index):
    px = img.load()
    return [[index.get(px[x, y][:3], 0) if px[x, y][3] else -1 for x in range(img.width)]
            for y in range(img.height)]


def hair_top(g):
    for y, row in enumerate(g):
        if any(i in (4, 5, 6, 7) for i in row):
            return y
    return 0


def find_head(base, g):
    """Decalage (dx, dy) qui superpose la tete de l'image de reference sur g, ou None."""
    t0 = hair_top(base)
    best = None
    for dy in range(-6, 7):
        for dx in range(-4, 5):
            bad = 0
            for y in range(t0, t0 + HEAD):
                for x, i in enumerate(base[y]):
                    yy, xx = y + dy, x + dx
                    j = g[yy][xx] if 0 <= yy < len(g) and 0 <= xx < len(g[0]) else -1
                    bad += i != j
            if best is None or bad < best[0]:
                best = (bad, dx, dy)
    if os.environ.get("DBG"):
        print("head", best)
    return (best[1], best[2]) if best[0] <= 12 else None


# Sac a dos de profil : rectangles effaces a la main (x0, y0, x1, y1 inclus) ;
# side = cote du corps (-1 : a gauche du rectangle, 1 : a droite) pour refermer le contour.
BACKPACK = {
    6: [(19, 16, 23, 23), (20, 24, 23, 26)], 7: [(19, 16, 23, 23), (20, 24, 23, 26)],
    8: [(19, 16, 23, 24), (20, 25, 23, 27)],
    9: [(0, 16, 4, 24), (0, 25, 3, 26)], 10: [(0, 16, 4, 24), (0, 25, 3, 26)],
    11: [(0, 16, 4, 25), (0, 26, 3, 27)],
    24: [(24, 16, 31, 27)], 25: [(19, 16, 29, 28)], 26: [(17, 16, 24, 28)],
    27: [(19, 16, 27, 18), (22, 19, 27, 23)], 28: [(19, 17, 28, 19), (23, 20, 28, 24)],
    29: [(16, 16, 23, 18), (19, 19, 25, 22)],
    30: [(0, 15, 6, 27)], 31: [(3, 16, 12, 28)], 32: [(0, 16, 6, 28)],
    33: [(6, 16, 13, 18), (4, 19, 8, 22)], 34: [(6, 17, 12, 19), (3, 20, 7, 22)],
    35: [(0, 16, 8, 18), (0, 19, 5, 23)],
}
BACKPACK_SIDE = {k: (-1 if k < 9 or 24 <= k < 30 else 1) for k in BACKPACK}


# Coupe courte de profil : remplace le chignon d'Alundra par des epis vers l'arriere.
# (colonne de depart, {ligne: texte}) ; '.' = garder, '_' = effacer.
_CUT = {7: "....qq..", 8: "..rRHRq.", 9: "..RHHRq_", 10: "..RRRq__", 11: "..RRq___",
        12: "..rq____", 13: "..q_____", 14: "...q____"}
HAIRCUT = {2: (15, _CUT), 3: (0, {y: s[::-1] for y, s in _CUT.items()})}


def erase_backpack(g, k):
    w = len(g[0])
    side = BACKPACK_SIDE.get(k, 0)
    for x0, y0, x1, y1 in BACKPACK.get(k, []):
        for y in range(y0, y1 + 1):
            for x in range(x0, min(w, x1 + 1)):
                g[y][x] = -1
    for x0, y0, x1, y1 in BACKPACK.get(k, []):
        for y in range(y0, y1 + 1):
            bx = x0 - 1 if side < 0 else x1 + 1
            if 0 <= bx < w and g[y][bx] > 1:
                g[y][bx] = 1

def retouch(img, pal, k, d, base_grid):
    index = {tuple(c[:3]): i for i, c in enumerate(pal) if i}
    w, h = img.size
    g = grid(img, index)
    rows = RETOUCHES.get(BASE[d], {})
    shift = (0, 0) if k == BASE[d] else find_head(base_grid, g) if rows else None
    lines = {}
    place = shift
    if rows and shift is None:
        # tete trop differente : on ne pose que les epis, alignes sur le haut des cheveux
        t0, t1 = hair_top(base_grid), hair_top(g)
        cx = lambda gr, t: (lambda xs: sum(xs) / len(xs) if xs else w / 2)(
            [x for y in range(t, t + 3) for x, i in enumerate(gr[y]) if i in (4, 5, 6, 7)])
        dx, dy = round(cx(g, t1) - cx(base_grid, t0)), t1 - t0
        place = (dx, dy)
        for r, line in rows.items():
            if r < t0:
                lines[r + dy] = (line, dx)
    if shift:
        dx, dy = shift
        for r, line in rows.items():
            if r < 12 or (0 <= r < len(base_grid) and 0 <= r + dy < h and
                          base_grid[r][max(0, -dx):w - max(0, dx)] == g[r + dy][max(0, dx):w - max(0, -dx)]):
                lines[r + dy] = (line, dx)
    pads = {}
    if shift is not None and d in PADS:
        dx, dy = shift
        pads = {r + dy: (line, dx) for r, line in PADS[d].items()}
    erase_backpack(g, k)
    if d == 1 and place is not None:
        # de dos : le beige du sac (entre les bras) devient du tissu orange, pas de la peau
        dx, dy = place
        for y in range(13 + dy, min(h, 23 + dy)):
            for x in range(max(0, 7 + dx), min(w, 20 + dx)):
                if g[y][x] in (8, 9, 10):
                    g[y][x] = {8: 5, 9: 6, 10: 7}[g[y][x]]
    add = max([0] + [-r for r in lines])
    out = Image.new("RGBA", (w, h + add), (0, 0, 0, 0))
    op = out.load()
    px = img.load()
    top = hair_top(g)
    for y in range(h):
        for x in range(w):
            p = px[x, y]
            i = g[y][x]
            if not p[3] or i == -1:
                continue
            m = material(i, y, h, 9 <= y - top <= 11, top, d) if i > 0 else None
            op[x, y + add] = (*FLINT[m], 255) if m else p
    for y, (line, dx) in lines.items():
        for x, ch in enumerate(line):
            if 0 <= x + dx < w:
                op[x + dx, y + add] = (*FLINT[ch], 255) if ch in FLINT else (0, 0, 0, 0)
    for y, (line, dx) in pads.items():
        for x, ch in enumerate(line):
            if ch in FLINT and 0 <= x + dx < w and 0 <= y + add < h + add:
                op[x + dx, y + add] = (*FLINT[ch], 255)
    if place is not None and d in HAIRCUT:
        col0, cut = HAIRCUT[d]
        dx, dy = place
        for r, line in cut.items():
            y = r + dy + add
            for x, ch in enumerate(line):
                xx = col0 + x + dx
                if 0 <= xx < w and 0 <= y < h + add:
                    if ch == "_":
                        op[xx, y] = (0, 0, 0, 0)
                    elif ch in FLINT:
                        op[xx, y] = (*FLINT[ch], 255)
    if k < 36:  # immobile et marche : on retire les pixels isoles herites d'A1
        remove_specks(out)
    return out, add


def remove_specks(img, size=4):
    w, h = img.size
    px = img.load()
    seen = set()
    for y0 in range(h):
        for x0 in range(w):
            if (x0, y0) in seen or not px[x0, y0][3]:
                continue
            comp, stack = [], [(x0, y0)]
            seen.add((x0, y0))
            while stack:
                x, y = stack.pop()
                comp.append((x, y))
                for dx in (-1, 0, 1):
                    for dy in (-1, 0, 1):
                        n = (x + dx, y + dy)
                        if 0 <= n[0] < w and 0 <= n[1] < h and n not in seen and px[n][3]:
                            seen.add(n)
                            stack.append(n)
            if len(comp) < size:
                for p in comp:
                    px[p] = (0, 0, 0, 0)


def main():
    bank = SpriteBank(open(sys.argv[1], "rb").read())
    recolor_palette(bank, 7, WEAPON_A2_COLORS)  # lame en acier bleute
    pal = bank.pals[17]
    anims_off, steps_off, _, frames_off = bank.character(0)
    index = {tuple(c[:3]): i for i, c in enumerate(pal) if i}
    offs = []
    for g in HERO_GROUPS:
        for a in struct.unpack_from("<4H", bank.b, anims_off + g * 14):
            for foff, _ in bank.anim_steps(steps_off, a):
                if foff not in offs:
                    offs.append(foff)
    bases = {d: grid(bank.render_frame(frames_off, offs[BASE[d]])[0], index) for d in range(4)}
    ids, frames, anims = {}, [], []
    for g in HERO_GROUPS:
        group = []
        for dirn, d in enumerate(struct.unpack_from("<4H", bank.b, anims_off + g * 14)):
            steps = []
            for foff, dur in bank.anim_steps(steps_off, d):
                if foff not in ids:
                    ids[foff] = len(frames)
                    img, ox, oy = bank.render_frame(frames_off, foff)
                    new, add = retouch(img, pal, len(frames), dirn, bases[dirn])
                    frames.append((new, ox, oy + add))
                steps.append((ids[foff], dur))
            group.append((steps, bank.anim_loops(steps_off, d)))
        anims.append(group)
    with open(sys.argv[2], "wb") as f:
        f.write(b"A1SP" + struct.pack("<H", len(frames)))
        for img, ox, oy in frames:
            f.write(struct.pack("<HHhh", img.width, img.height, ox, oy))
            f.write(img.tobytes())
        f.write(struct.pack("<H", len(anims)))
        for group in anims:
            for steps, loops in group:
                f.write(struct.pack("<H", len(steps) | (0 if loops else 0x8000)))
                for fid, dur in steps:
                    f.write(struct.pack("<HH", fid, dur))
    print(f"{len(frames)} images -> {sys.argv[2]}")
    if len(sys.argv) > 3:
        sh = Image.new("RGBA", (36 * 10, 52 * 6), (90, 120, 60, 255))
        for k, (img, ox, oy) in enumerate(frames):
            sh.alpha_composite(img, ((k % 10) * 36 + 18 - ox, (k // 10) * 52 + 48 - oy))
        sh.resize((sh.width * 4, sh.height * 4), Image.NEAREST).save(sys.argv[3])


if __name__ == "__main__":
    main()













