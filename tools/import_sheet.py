"""Importe la planche de sprites du heros locale (images/...png).

Detoure le fond vert, decoupe chaque sprite, le reduit a la taille du jeu et ecrit un .a1s.
    python tools/import_sheet.py <planche.png> assets/sprites/flint.a1s [apercu.png]

Groupes ecrits : 0 immobile, 1 marche, 2 attaque 1, 3 course, 4 attaque 2 (x 4 directions).
"""
import struct
import sys

from PIL import Image

# Bandes verticales des 4 directions sur la planche, dans l'ordre du jeu (bas, haut, gauche, droite)
ROWS = {0: (60, 160), 1: (445, 565), 2: (190, 290), 3: (325, 425)}
# Colonnes des sections et nombre d'images attendu
SECTIONS = [("idle", 113, 257, 3), ("walk", 288, 587, 8), ("attack", 972, 1302, 6),
            ("run", 612, 937, 7), ("attack2", 1315, 1685, 7)]
SCALE = 0.5   # la planche est dessinee environ en x2


def is_bg(p):
    r, g, b = p[:3]
    return g > 90 and g > r + 12 and g > b + 45 and r < 150


def mask_of(im):
    w, h = im.size
    px = im.load()
    return [[not is_bg(px[x, y]) for x in range(w)] for y in range(h)]


def split_columns(mask, n):
    """Decoupe en n sprites selon les colonnes vides ; fusionne/coupe si besoin."""
    w = len(mask[0])
    occ = [sum(mask[y][x] for y in range(len(mask))) for x in range(w)]
    runs, start = [], None
    for x in range(w + 1):
        on = x < w and occ[x] > 1
        if on and start is None:
            start = x
        elif not on and start is not None:
            runs.append([start, x])
            start = None
    runs = [r for r in runs if sum(occ[r[0]:r[1]]) > 30]
    while len(runs) > n:  # fusionne les deux plus proches (traînees detachees)
        i = min(range(len(runs) - 1), key=lambda k: runs[k + 1][0] - runs[k][1])
        runs[i] = [runs[i][0], runs[i + 1][1]]
        del runs[i + 1]
    while len(runs) < n:  # coupe le plus large au creux de densite
        i = max(range(len(runs)), key=lambda k: runs[k][1] - runs[k][0])
        a, b = runs[i]
        mid = min(range(a + 10, b - 10), key=lambda x: occ[x])
        runs[i:i + 1] = [[a, mid], [mid, b]]
    return runs


def components(m):
    """Composantes connexes (8-voisins) d'un masque."""
    h, w = len(m), len(m[0])
    seen = [[False] * w for _ in range(h)]
    comps = []
    for y in range(h):
        for x in range(w):
            if m[y][x] and not seen[y][x]:
                stack, pts = [(x, y)], []
                seen[y][x] = True
                while stack:
                    cx, cy = stack.pop()
                    pts.append((cx, cy))
                    for dy in (-1, 0, 1):
                        for dx in (-1, 0, 1):
                            nx, ny = cx + dx, cy + dy
                            if 0 <= nx < w and 0 <= ny < h and m[ny][nx] and not seen[ny][nx]:
                                seen[ny][nx] = True
                                stack.append((nx, ny))
                comps.append(pts)
    return comps


def extract(sheet, x0, x1, y0, y1, n):
    """Grille reguliere de n cases ; chaque morceau detoure va a la case de son centre.
    Un morceau tres large (sprites qui se touchent) est coupe aux limites des cases."""
    pad = 40
    band = sheet.crop((x0 - pad, y0, x1 + pad, y1))
    m = mask_of(band)
    src0 = band.load()
    cw = (x1 - x0) / n
    cell_of = lambda x: min(n - 1, max(0, int((x - pad) // cw)))
    cells = [[] for _ in range(n)]
    cyan = lambda p: p[2] > p[0] + 40 and p[2] > 170
    body = [[m[y][x] and not cyan(src0[x, y]) for x in range(len(m[0]))] for y in range(len(m))]
    arcs = [[m[y][x] and cyan(src0[x, y]) for x in range(len(m[0]))] for y in range(len(m))]
    for pts in components(body) + components(arcs):
        if len(pts) < 6:
            continue
        ys_ = [p[1] for p in pts]
        xs_ = [p[0] for p in pts]
        if max(ys_) - min(ys_) <= 3 and max(xs_) - min(xs_) > 8:  # trait de cadre
            continue
        xs = [p[0] for p in pts]
        slash = cyan(src0[pts[0][0], pts[0][1]])
        if max(xs) - min(xs) > cw * 1.5 and not slash:
            for p in pts:
                cells[cell_of(p[0])].append(p)
        else:
            mx = sum(xs) / len(xs)
            if pad <= mx < pad + (x1 - x0):
                cells[cell_of(mx)].extend(pts)
    src = band.load()
    out = []
    for pts in cells:
        if not pts:
            pts = [(pad, 0)]
        xs = [p[0] for p in pts]
        ys = [p[1] for p in pts]
        bx, by = min(xs), min(ys)
        spr = Image.new("RGBA", (max(xs) - bx + 1, max(ys) - by + 1), (0, 0, 0, 0))
        dst = spr.load()
        for x, y in pts:
            dst[x - bx, y - by] = (*src[x, y][:3], 255)
        w, h = spr.size
        al = spr.getchannel("A").load()
        # ancrage : centre des pixels du bas (les pieds)
        fx = [x for y in range(int(h * 0.85), h) for x in range(w) if al[x, y]]
        out.append((spr, sum(fx) / len(fx) if fx else w / 2))
    return out

def shrink(spr, foot_x):
    w, h = spr.size
    nw, nh = max(1, round(w * SCALE)), max(1, round(h * SCALE))
    # moyenne des couleurs ponderee par l'opacite, puis alpha tranche net
    big = spr.resize((nw, nh), Image.BOX)
    px = big.load()
    for y in range(nh):
        for x in range(nw):
            r, g, b, a = px[x, y]
            if a < 110:
                px[x, y] = (0, 0, 0, 0)
            else:
                k = 255 / a
                px[x, y] = (min(255, int(r * k)), min(255, int(g * k)), min(255, int(b * k)), 255)
    big = big.quantize(colors=40, method=Image.Quantize.FASTOCTREE).convert("RGBA")
    # le quantize remet du noir opaque sur les pixels transparents : on restaure l'alpha
    a2 = spr.resize((nw, nh), Image.BOX).getchannel("A").point(lambda v: 255 if v >= 110 else 0)
    big.putalpha(a2)
    return big, round(foot_x * SCALE), nh - 1


def main():
    sheet = Image.open(sys.argv[1]).convert("RGB")
    out = sys.argv[2]
    frames, anims = [], []
    for mode, x0, x1, n in SECTIONS:
        for d in range(4):
            y0, y1 = ROWS[d]
            sprites = extract(sheet, x0, x1, y0, y1, n)
            first = len(frames)
            for spr, fx in sprites:
                frames.append(shrink(spr, fx))
            idx = list(range(first, first + n))
            if mode == "idle":
                anims.append(([(idx[0], 40), (idx[1], 12), (idx[2], 40), (idx[1], 12)], 0))
            elif mode == "walk":
                anims.append(([(i, 5) for i in idx], 0))
            elif mode == "run":
                anims.append(([(i, 4) for i in idx], 0))
            else:
                anims.append((list(zip(idx, (3, 2, 2, 3, 4, 5, 3))), 0x8000))
    with open(out, "wb") as f:
        f.write(b"A1SP" + struct.pack("<H", len(frames)))
        for img, ox, oy in frames:
            f.write(struct.pack("<HHhh", img.width, img.height, ox, oy))
            f.write(img.tobytes())
        f.write(struct.pack("<H", len(SECTIONS)))
        for steps, flags in anims:
            f.write(struct.pack("<H", len(steps) | flags))
            for fr, du in steps:
                f.write(struct.pack("<HH", fr, du))
    print(f"{len(frames)} images -> {out}")
    if len(sys.argv) > 3:
        cw, ch = 48, 48
        sheet2 = Image.new("RGBA", (cw * 8, ch * 20), (90, 120, 60, 255))
        k = 0
        for s, (mode, _, _, n) in enumerate(SECTIONS):
            for d in range(4):
                for t in range(n):
                    img, ox, oy = frames[k]
                    sheet2.alpha_composite(img, (t * cw + 24 - ox, (s * 4 + d) * ch + 44 - oy))
                    k += 1
        sheet2.resize((sheet2.width * 3, sheet2.height * 3), Image.NEAREST).save(sys.argv[3])


if __name__ == "__main__":
    main()
