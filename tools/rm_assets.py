"""Assets ORIGINAUX des ajouts du remake (tranche 3), style pixel Alundra 1.

Aucun sprite existant n'est copie ni recolore : chaque motif est construit ici
pixel par pixel (formes geometriques + rampes de couleurs discretes, lumiere en
haut a gauche, contour sombre teinte), a la resolution native (1 px = 1 px).

Les rampes de couleurs sont des valeurs OBSERVEES (pas inventees) :
  - STONE : decor zone 011 (bloc de pierre, mur, pilier), capture du remake ;
  - GOLD / WOOD : coffre du decor + icones A1 extraites (assets/ui/a1_menu) ;
  - IRON / VIOLET / RED : icones A1 extraites ;
  - FIRE / ICE / WATER : captures A1 locales.

Sorties (assets/ est ignore par Git, ne pas publier) :
  assets/sprites/rm_flammable.a1s, rm_ice_surface.a1s, rm_fragile.a1s,
  rm_mechanism.a1s, rm_lever.a1s  (2 images : 0 intact, 1 resolu ; ox,oy = milieu bas)
  assets/sprites/rm_icons.a1s     (8 icones 24x32, ox = oy = 0)
  assets/ui/rm_icons.txt          (index|id)
  extracted/preview/rm_assets/*.png (apercus x1 et x4)

Usage : py -3.12 tools/rm_assets.py
Format .a1s (src/sprite.cpp SpriteBank::load) : "A1SP", u16 n, puis par image
u16 w, u16 h, i16 ox, i16 oy, w*h*4 octets RGBA ; puis u16 groupes = 0 (LE).
"""
from __future__ import annotations

import math
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# --- palettes observees (sombre -> clair) -----------------------------------
STONE = [(32, 24, 16), (49, 41, 32), (65, 57, 49), (82, 74, 57), (98, 90, 74),
         (115, 106, 90), (131, 123, 98), (164, 156, 115)]
OUTLINE = (16, 16, 8)
FIRE = [(156, 65, 24), (189, 74, 8), (222, 107, 41), (222, 173, 74), (231, 198, 115), (255, 255, 231)]
ICE = [(57, 90, 164), (90, 123, 197), (115, 140, 189), (165, 198, 222), (214, 222, 222), (255, 255, 255)]
WATER = [(16, 33, 41), (16, 41, 82), (57, 74, 115), (74, 123, 173), (115, 140, 189)]
WOOD = [(41, 32, 8), (74, 57, 16), (115, 90, 24), (165, 132, 66)]
GOLD = [(74, 49, 16), (115, 82, 24), (165, 132, 66), (222, 181, 82), (231, 198, 115), (255, 255, 231)]
IRON = [(24, 24, 24), (49, 49, 49), (90, 90, 82), (115, 107, 115), (156, 156, 148), (198, 198, 198), (239, 239, 239)]
VIOLET = [(41, 41, 99), (57, 57, 115), (74, 66, 173), (115, 140, 189), (206, 198, 222)]
RED = [(41, 16, 24), (82, 33, 41), (140, 49, 57), (165, 90, 66)]

SQ2 = math.sqrt(2.0)


def pick(ramp, t):
    """t in [0,1] (0 sombre, 1 clair) -> couleur discrete de la rampe."""
    t = max(0.0, min(1.0, t))
    return ramp[min(len(ramp) - 1, int(t * len(ramp)))]


class Canvas:
    def __init__(self, w: int, h: int):
        self.w, self.h = w, h
        self.px: dict[tuple[int, int], tuple[int, int, int]] = {}
        self.no_outline: set[tuple[int, int]] = set()

    def put(self, x, y, c, glow=False):
        x, y = int(x), int(y)
        if 0 <= x < self.w and 0 <= y < self.h:
            self.px[(x, y)] = c
            if glow:
                self.no_outline.add((x, y))
            else:
                self.no_outline.discard((x, y))

    def fill(self, pred, color, glow=False):
        """pred(cx, cy) sur le centre du pixel ; color(x, y, cx, cy) -> rgb ou None."""
        for y in range(self.h):
            for x in range(self.w):
                cx, cy = x + 0.5, y + 0.5
                if pred(cx, cy):
                    c = color(x, y, cx, cy) if callable(color) else color
                    if c is not None:
                        self.put(x, y, c, glow)

    def outline(self, color=None):
        """Contour 4-voisins : teinte sombre du pixel voisin (ou couleur fixe)."""
        add = {}
        for (x, y), c in self.px.items():
            if (x, y) in self.no_outline:
                continue
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                q = (x + dx, y + dy)
                if q not in self.px and 0 <= q[0] < self.w and 0 <= q[1] < self.h:
                    add[q] = color or tuple(max(0, int(v * 0.28)) for v in c)
        for q, c in add.items():
            self.px[q] = c

    def rgba(self) -> bytes:
        out = bytearray()
        for y in range(self.h):
            for x in range(self.w):
                c = self.px.get((x, y))
                out += bytes((*c, 255)) if c else b"\0\0\0\0"
        return bytes(out)


# --- primitives --------------------------------------------------------------
def seg_uv(p0, p1, x, y):
    """Coordonnees (u le long du segment, v perpendiculaire, + = bas-droite)."""
    dx, dy = p1[0] - p0[0], p1[1] - p0[1]
    L = math.hypot(dx, dy)
    ax, ay = dx / L, dy / L
    nx, ny = -ay, ax
    if nx + ny < 0:
        nx, ny = -nx, -ny
    rx, ry = x - p0[0], y - p0[1]
    return rx * ax + ry * ay, rx * nx + ry * ny, L


def sphere(cv, cx0, cy0, r, ramp, glow=False, spec=None):
    def col(x, y, cx, cy):
        dx, dy = (cx - cx0) / r, (cy - cy0) / r
        dz = math.sqrt(max(0.0, 1 - dx * dx - dy * dy))
        lum = (-dx * 0.55 - dy * 0.6 + dz * 0.6) * 0.75 + 0.25
        return pick(ramp, lum)
    cv.fill(lambda cx, cy: (cx - cx0) ** 2 + (cy - cy0) ** 2 <= r * r, col, glow)
    if spec:
        cv.put(cx0 - r * 0.45, cy0 - r * 0.45, spec, glow)


def cylinder_v(cv, x0, x1, y0, y1, ramp, bias=0.0):
    """Cylindre vertical [x0,x1] x [y0,y1], lumiere a gauche."""
    cxm, r = (x0 + x1 + 1) / 2, (x1 - x0 + 1) / 2

    def col(x, y, cx, cy):
        n = (cx - cxm) / r
        return pick(ramp, 0.62 - 0.55 * n + bias)
    cv.fill(lambda cx, cy: x0 <= cx < x1 + 1 and y0 <= cy < y1 + 1, col)


def box(cv, x0, x1, ytop, ymid, ybot, ramp):
    """Bloc vu en 3/4 : face superieure [ytop,ymid), face avant [ymid,ybot]."""
    def col(x, y, cx, cy):
        if y < ymid:
            return pick(ramp, 0.95 - 0.25 * (x - x0) / max(1, x1 - x0) - 0.1 * (y - ytop) / max(1, ymid - ytop))
        if x == x0:
            return pick(ramp, 0.7)
        if x == x1:
            return pick(ramp, 0.2)
        return pick(ramp, 0.55 - 0.2 * (x - x0) / max(1, x1 - x0) - 0.15 * (y - ymid) / max(1, ybot - ymid))
    cv.fill(lambda cx, cy: x0 <= cx < x1 + 1 and ytop <= cy < ybot + 1, col)


def rock(cv, cx0, cy0, rx, ry, ramp=STONE):
    def col(x, y, cx, cy):
        dx, dy = (cx - cx0) / rx, (cy - cy0) / ry
        return pick(ramp, 0.75 - 0.45 * dx - 0.45 * dy)
    cv.fill(lambda cx, cy: ((cx - cx0) / rx) ** 2 + ((cy - cy0) / ry) ** 2 <= 1, col)


def polyline(cv, pts, c, glow=False):
    for (xa, ya), (xb, yb) in zip(pts, pts[1:]):
        n = max(abs(xb - xa), abs(yb - ya))
        for i in range(n + 1):
            cv.put(round(xa + (xb - xa) * i / n), round(ya + (yb - ya) * i / n), c, glow)


def in_poly(pts, x, y):
    inside = False
    for (xa, ya), (xb, yb) in zip(pts, pts[1:] + pts[:1]):
        if (ya > y) != (yb > y) and x < xa + (y - ya) * (xb - xa) / (yb - ya):
            inside = not inside
    return inside


def tongue(cx0, top, base, w):
    """Langue de flamme (goutte inversee) : predicat."""
    def pred(cx, cy):
        if not top <= cy <= base:
            return False
        t = (cy - top) / (base - top)
        half = w * math.sin(min(1.0, t * 1.15) * math.pi * 0.62) * (1 - 0.25 * t)
        return abs(cx - cx0) <= half
    return pred


# --- objets d'enigme (24 px de large) ---------------------------------------
def brazier(lit: bool) -> Canvas:
    cv = Canvas(24, 32)
    box(cv, 5, 18, 28, 29, 31, STONE)                      # socle
    cylinder_v(cv, 8, 15, 21, 27, STONE)                    # fut
    for y in (21, 22):                                      # ombre sous la vasque
        for x in range(8, 16):
            cv.put(x, y, STONE[1])

    def bowl_pred(cx, cy):
        if not 12 <= cy < 21:
            return False
        hw = 10.5 - (cy - 12) * 0.62
        return abs(cx - 12) <= hw

    def bowl_col(x, y, cx, cy):
        hw = 10.5 - (cy - 12) * 0.62
        n = (cx - 12) / hw
        t = 0.6 - 0.5 * n - 0.15 * (cy - 12) / 9
        if y == 16:
            t -= 0.3                                        # rainure gravee
        if y == 17:
            t += 0.15
        return pick(STONE, t)
    cv.fill(bowl_pred, bowl_col)
    # levre superieure (face du dessus) et creux de la vasque
    cv.fill(lambda cx, cy: ((cx - 12) / 10.5) ** 2 + ((cy - 12) / 2.6) ** 2 <= 1,
            lambda x, y, cx, cy: pick(STONE, 0.98 - 0.35 * (cx - 1.5) / 21))
    inner = lambda cx, cy: ((cx - 12) / 8.2) ** 2 + ((cy - 12.3) / 1.7) ** 2 <= 1
    if not lit:
        cv.fill(inner, lambda x, y, cx, cy: STONE[0] if (x + y) % 3 else STONE[2])
        polyline(cv, [(7, 11), (10, 12)], WOOD[0])          # buches calcinees
        polyline(cv, [(16, 11), (13, 12)], WOOD[1])
        cv.put(7, 10, WOOD[1]); cv.put(16, 10, WOOD[2])
        cv.outline(OUTLINE)
        return cv
    cv.fill(inner, lambda x, y, cx, cy: FIRE[2] if (x + y) % 2 else FIRE[1])
    # reflets chauds sur la levre
    for x in (5, 6, 17, 18):
        cv.put(x, 10 if x in (6, 17) else 11, FIRE[3])
    cv.outline(OUTLINE)
    tongues = [(12, 0, 13, 5.6, 1.0), (7.0, 5, 13, 3.0, 0.6), (17.0, 4, 13, 3.0, 0.6), (10.0, 2, 13, 2.0, 0.5)]

    def heat(cx, cy):
        best = 0.0
        for (tx, top, base, w, k) in tongues:
            if not top <= cy <= base:
                continue
            t = (cy - top) / (base - top)
            half = w * math.sin(min(1.0, t * 1.1) * math.pi * 0.6) * (1 - 0.2 * t) + 1e-6
            if abs(cx - tx) <= half:
                best = max(best, k * (1 - abs(cx - tx) / half) * min(1.0, t * 1.5) * (1 - 0.35 * max(0.0, t - 0.8) / 0.2))
        return best

    def fcol(x, y, cx, cy):
        k = heat(cx, cy)
        return FIRE[5] if k > 0.7 else FIRE[4] if k > 0.55 else FIRE[3] if k > 0.38 else FIRE[2] if k > 0.18 else FIRE[1]
    cv.fill(lambda cx, cy: heat(cx, cy) > 0, fcol, glow=True)
    for (x, y) in ((4, 3), (19, 1), (10, -1 + 2), (15, 0)):  # etincelles
        cv.put(x, y, FIRE[3], glow=True)
    return cv


def ice_surface(frozen: bool) -> Canvas:
    cv = Canvas(24, 18)

    def rim_pred(cx, cy):
        return 1 <= cx < 23 and 4 <= cy < 18 and not ((cx < 2 or cx > 22) and (cy < 5 or cy > 17))

    def rim_col(x, y, cx, cy):
        if y >= 15:                                          # epaisseur avant de la dalle
            return pick(STONE, 0.45 - 0.25 * x / 23 - 0.1 * (y - 15))
        return pick(STONE, 0.95 - 0.3 * x / 23 - 0.25 * (y - 4) / 11)
    cv.fill(rim_pred, rim_col)
    for x in (6, 12, 18):                                    # joints de dalles
        cv.put(x, 15, STONE[1]); cv.put(x, 16, STONE[1])
    basin = lambda cx, cy: 4 <= cx < 20 and 6 <= cy < 14 and not ((cx < 5 or cx > 19) and (cy < 7 or cy > 13))
    if not frozen:
        def wcol(x, y, cx, cy):
            if y == 6:
                return WATER[0]                              # ombre de la margelle
            t = 0.3 + 0.08 * (y - 7) + (0.35 if (y in (9, 12) and 7 <= x <= 15 and (x + y) % 5 < 3) else 0)
            return pick(WATER, t)
        cv.fill(basin, wcol)
        cv.put(9, 8, WATER[4]); cv.put(10, 8, WATER[4]); cv.put(15, 11, WATER[4])
        cv.outline(OUTLINE)
        return cv

    def icol(x, y, cx, cy):
        if y == 6:
            return ICE[1]
        d = (x - 4) + (y - 6)                                # degrade haut-gauche -> bas-droite
        if (x - y) in (2, 3) and 7 <= y <= 11:
            return ICE[5]                                    # reflet diagonal
        return pick(ICE, 0.85 - d * 0.03)
    cv.fill(basin, icol)
    polyline(cv, [(12, 8), (14, 10), (13, 12)], ICE[1])       # fissures
    polyline(cv, [(14, 10), (17, 11)], ICE[1])
    # cristaux de givre qui depassent
    for (x, y) in ((4, 5), (5, 5), (6, 5), (9, 5), (10, 5), (14, 5), (17, 5), (18, 5), (19, 5),
                   (3, 6), (3, 7), (3, 10), (20, 8), (20, 9), (6, 14), (7, 14), (13, 14), (16, 14)):
        cv.put(x, y, ICE[4])                                 # givre sur la margelle
    for (x, y) in ((5, 5), (18, 5), (3, 6)):
        cv.put(x, y, ICE[5])
    cv.outline(OUTLINE)
    return cv


def pillar(broken: bool) -> Canvas:
    cv = Canvas(24, 32)
    box(cv, 5, 18, 27, 28, 31, STONE)                        # base
    if not broken:
        cylinder_v(cv, 7, 16, 5, 26, STONE)
        for x in (9, 12, 15):                                # cannelures
            for y in range(6, 27):
                c = cv.px.get((x, y))
                if c:
                    cv.put(x, y, STONE[max(0, STONE.index(c) - 2)])
        box(cv, 5, 18, 0, 2, 4, STONE)                       # chapiteau
        for x in range(7, 17):
            cv.put(x, 5, STONE[1])
        cv.px.pop((18, 0), None); cv.px.pop((17, 0), None)   # coin ebreche
        crack = [(13, 6), (11, 10), (13, 14), (10, 19), (12, 23)]
        polyline(cv, crack, STONE[0])
        polyline(cv, [(13, 14), (15, 16)], STONE[0])
        for (x, y) in ((12, 8), (12, 12), (11, 16), (11, 21), (14, 16)):
            cv.put(x - 1, y, STONE[7])                       # arete eclairee de la fissure
        cv.outline(OUTLINE)
        return cv
    tops = [19, 17, 20, 16, 18, 21, 17, 19, 22, 20]
    for i, x in enumerate(range(7, 17)):
        for y in range(tops[i], 27):
            n = (x + 0.5 - 12) / 5
            cv.put(x, y, pick(STONE, 0.62 - 0.55 * n) if y > tops[i] else STONE[7 if i < 5 else 6])
    for x in (9, 12, 15):
        for y in range(tops[x - 7] + 1, 27):
            c = cv.px[(x, y)]
            cv.put(x, y, STONE[max(0, STONE.index(c) - 2)])
    rock(cv, 3, 28.5, 2.6, 2.2)                              # eclats au sol
    rock(cv, 20.5, 29, 3.0, 2.0)
    rock(cv, 2.5, 23.5, 1.6, 1.3)
    rock(cv, 21, 24, 1.5, 1.3)
    rock(cv, 9, 30.5, 1.4, 1.0)
    cv.outline(OUTLINE)
    return cv


def mechanism(opened: bool) -> Canvas:
    cv = Canvas(24, 30)
    box(cv, 1, 22, 2, 8, 29, STONE)
    cx0, cy0, R = 12.0, 19.0, 8.2
    glyph_c = GOLD[4] if opened else STONE[0]
    ang0 = math.pi / 4 if opened else 0.0

    def wheel(x, y, cx, cy):
        d = math.hypot(cx - cx0, cy - cy0)
        if d > R:
            return None
        if d > 6.6:                                          # anneau sculpte
            return pick(STONE, 0.8 - 0.4 * (cx - cx0 + cy - cy0) / (2 * R))
        if d > 5.6:
            return STONE[0]                                  # rainure
        a = math.atan2(cy - cy0, cx - cx0) - ang0
        spoke = min(abs(math.sin(a)), abs(math.cos(a))) * d < 0.8
        if spoke:
            return pick(STONE, 0.85 - 0.3 * (cx - cx0 + cy - cy0) / (2 * R))
        return pick(STONE, 0.4 - 0.2 * (cx - cx0 + cy - cy0) / (2 * R))
    cv.fill(lambda cx, cy: True, wheel)
    for k in range(8):                                       # glyphes de l'anneau
        a = ang0 + k * math.pi / 4 + math.pi / 8
        gx, gy = cx0 + 7.4 * math.cos(a), cy0 + 7.4 * math.sin(a)
        cv.put(gx - 0.5, gy - 0.5, glyph_c, glow=opened)
    sphere(cv, cx0, cy0, 1.8, GOLD if opened else IRON)
    if opened:
        for (x, y) in ((10, 4), (11, 4), (12, 4), (13, 4), (11, 5), (12, 5)):
            cv.put(x, y, STONE[0])                           # logement du verrou vide
        cv.outline(OUTLINE)
        for k in range(8):                                   # halo des glyphes (sans contour)
            a = ang0 + k * math.pi / 4 + math.pi / 8
            gx, gy = cx0 + 7.4 * math.cos(a), cy0 + 7.4 * math.sin(a)
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                q = (int(gx - 0.5) + dx, int(gy - 0.5) + dy)
                if cv.px.get(q) not in (GOLD[4],):
                    if q in cv.px and math.hypot(q[0] + 0.5 - cx0, q[1] + 0.5 - cy0) > 6.4:
                        cv.put(*q, GOLD[2])
        return cv
    for y in range(2, 7):                                    # verrou de fer enfonce
        for x in (11, 12):
            cv.put(x, y, IRON[5] if x == 11 else IRON[3])
    for x in range(10, 14):                                  # tete du verrou
        cv.put(x, 0, IRON[6] if x < 12 else IRON[4]); cv.put(x, 1, IRON[4] if x < 12 else IRON[2])
    cv.outline(OUTLINE)
    return cv


def lever(down: bool) -> Canvas:
    cv = Canvas(24, 30)
    piv = (12.0, 24.5)
    end = (19.5, 19.5) if down else (6.5, 6.5)

    def handle(cx, cy):
        u, v, L = seg_uv(piv, end, cx, cy)
        return -0.5 <= u <= L and abs(v) <= 1.05

    def hcol(x, y, cx, cy):
        u, v, L = seg_uv(piv, end, cx, cy)
        return pick(WOOD, 0.65 - 0.5 * v)
    cv.fill(handle, hcol)
    box(cv, 5, 18, 23, 26, 29, STONE)                        # socle devant le manche
    for x in range(9, 15):
        cv.put(x, 24, STONE[0])                              # fente
    cv.put(11, 24, WOOD[1]); cv.put(12, 24, WOOD[2])
    sphere(cv, end[0], end[1], 2.6, IRON, spec=IRON[6])     # pommeau de fer
    cv.outline(OUTLINE)
    return cv


# --- icones 24x32 ------------------------------------------------------------
def diag_weapon(cv, p0, p1, parts):
    """parts : liste (u0, u1, halfw(u) ou nombre, ramp) le long de l'axe p0->p1."""
    for (u0, u1, hw, ramp) in parts:
        def pred(cx, cy, u0=u0, u1=u1, hw=hw):
            u, v, L = seg_uv(p0, p1, cx, cy)
            w = hw(u) if callable(hw) else hw
            return u0 <= u < u1 and abs(v) <= w

        def col(x, y, cx, cy, hw=hw, ramp=ramp):
            u, v, L = seg_uv(p0, p1, cx, cy)
            w = hw(u) if callable(hw) else hw
            return pick(ramp, 0.62 - 0.55 * v / max(0.8, w))
        cv.fill(pred, col)


def icon_ember_blade() -> Canvas:
    cv = Canvas(24, 32)
    p0, p1 = (3.5, 28.5), (21.5, 10.5)
    L = 18 * SQ2
    diag_weapon(cv, p0, p1, [
        (0, 3, 1.1, RED),
        (3, 7.5, 1.0, RED),
        (7.5, 9.5, 4.6, GOLD),
        (9.5, L, lambda u: min(1.75, (L - u) * 0.75), [RED[1], FIRE[0], FIRE[1], FIRE[2], FIRE[3]]),
    ])
    sphere(cv, 4, 28, 1.7, GOLD)
    # arete centrale incandescente
    for i in range(8, 17):
        cv.put(3 + i, 28 - i, FIRE[4] if i % 3 else FIRE[5])
    cv.outline()
    for (x, y) in ((18, 7), (22, 13), (14, 10), (20, 4)):
        cv.put(x, y, FIRE[3], glow=True)
    return cv


def icon_frost_lance() -> Canvas:
    cv = Canvas(24, 32)
    p0, p1 = (2.5, 29.5), (21.5, 10.5)
    L = 19 * SQ2
    diag_weapon(cv, p0, p1, [
        (0, 1.6, 1.2, IRON),
        (1.6, 16.5, 0.95, WOOD),
        (16.5, 18.5, 1.6, IRON),
        (18.5, L + 0.2, lambda u: 3.9 * math.sin(math.pi * max(0.0, min(1.0, (u - 18.0) / (L - 18.0))) ** 0.8), ICE),
    ])
    for i in range(15, 18):                                  # nervure du fer
        cv.put(3 + i, 29 - i, ICE[5])
    cv.outline()
    for (x, y) in ((15, 6), (22, 16), (14, 7), (16, 7), (15, 8)):
        cv.put(x, y, ICE[4] if (x, y) != (15, 7) else ICE[5], glow=True)
    return cv


def icon_rift_hammer() -> Canvas:
    cv = Canvas(24, 32)
    p0, p1 = (4.5, 28.5), (17.5, 15.5)
    diag_weapon(cv, p0, p1, [
        (0, 2.2, 1.3, IRON),
        (2.2, 15.0, 1.0, WOOD),
    ])
    hc = (15.0, 15.5)

    def hp(cx, cy):
        u, v, L = seg_uv(p0, p1, cx, cy)
        return 12.0 <= u <= 19.5 and abs(v) <= 6.6

    def hcol(x, y, cx, cy):
        u, v, L = seg_uv(p0, p1, cx, cy)
        t = 0.75 - 0.35 * v / 6.6 - 0.25 * (u - 12) / 7.5
        if abs(abs(v) - 6.0) < 0.6:
            t -= 0.2                                         # chanfrein des faces de frappe
        return pick(IRON, t)
    cv.fill(hp, hcol)
    rift = [(10, 12), (13, 15), (14, 13), (17, 17), (18, 15), (20, 18)]
    polyline(cv, [(x + 1, y + 1) for x, y in rift], VIOLET[1])  # ombre de la faille
    polyline(cv, rift, VIOLET[3])                                # faille
    for (x, y) in ((13, 15), (17, 17), (14, 13), (11, 13)):
        cv.put(x, y, VIOLET[4])
    cv.outline()
    return cv


def icon_ember_shard() -> Canvas:
    cv = Canvas(24, 32)
    big = [(12, 4), (17, 13), (15, 24), (11, 28), (7, 20), (8, 11)]
    small = [(17, 16), (21, 21), (18, 27), (15, 24)]

    def facet(poly, ridge_x):
        def col(x, y, cx, cy):
            left = cx < ridge_x + (cy - 16) * -0.1
            t = 0.75 if left else 0.38
            t += 0.15 * (16 - cy) / 12
            return pick([RED[1], RED[2], FIRE[0], FIRE[1], FIRE[2], FIRE[3], FIRE[4]], t)
        cv.fill(lambda cx, cy: in_poly(poly, cx, cy), col)
    facet(small, 18.5)
    facet(big, 11.5)
    polyline(cv, [(12, 6), (11, 16), (11, 25)], FIRE[4])     # arete lumineuse
    cv.put(11, 14, FIRE[5]); cv.put(11, 15, FIRE[5]); cv.put(10, 10, FIRE[5])
    cv.outline()
    for (x, y) in ((6, 7), (19, 9), (4, 16)):
        cv.put(x, y, FIRE[3], glow=True)
    return cv


def icon_frost_core() -> Canvas:
    cv = Canvas(24, 32)
    sphere(cv, 12, 17, 7.6, [WATER[2], ICE[0], ICE[1], ICE[2], ICE[3], ICE[4]])
    gem = [(12, 11), (16, 16), (12, 22), (8, 16)]           # coeur facette interne
    cv.fill(lambda cx, cy: in_poly(gem, cx, cy),
            lambda x, y, cx, cy: ICE[4] if cx < 12 and cy < 16.5 else (ICE[3] if cx < 12 or cy < 16.5 else ICE[2]))
    cv.put(8, 12, ICE[5]); cv.put(9, 12, ICE[5]); cv.put(8, 13, ICE[5])
    cv.outline()
    for (x, y, c) in ((12, 6, ICE[5]), (12, 7, ICE[4]), (11, 7, ICE[4]), (13, 7, ICE[4]), (20, 10, ICE[4]), (4, 22, ICE[4])):
        cv.put(x, y, c, glow=True)
    return cv


def icon_rift_fragment() -> Canvas:
    cv = Canvas(24, 32)
    poly = [(7, 9), (14, 5), (19, 11), (18, 23), (10, 27), (5, 18)]

    def col(x, y, cx, cy):
        if cx + cy < 22:
            t = 0.85
        elif cx > 13 + (cy - 16) * 0.3:
            t = 0.25
        else:
            t = 0.55
        return pick([VIOLET[0], VIOLET[1], VIOLET[2], VIOLET[3]], t)
    cv.fill(lambda cx, cy: in_poly(poly, cx, cy), col)
    polyline(cv, [(9, 11), (12, 15), (10, 19), (14, 22), (13, 25)], VIOLET[4])
    polyline(cv, [(12, 15), (16, 13)], VIOLET[4])
    cv.outline()
    for (x, y) in ((4, 8), (20, 7), (21, 20)):
        cv.put(x, y, VIOLET[3], glow=True)
    return cv


def icon_ironwood() -> Canvas:
    cv = Canvas(24, 32)
    p0, p1 = (5.5, 25.5), (15.5, 15.5)
    R = 5.4

    def pred(cx, cy):
        u, v, L = seg_uv(p0, p1, cx, cy)
        return 0 <= u <= L and abs(v) <= R

    def col(x, y, cx, cy):
        u, v, L = seg_uv(p0, p1, cx, cy)
        t = 0.62 - 0.5 * v / R
        if int(u * 1.3 + v * 0.4) % 4 == 0:
            t -= 0.25                                        # stries d'ecorce
        if abs(v + 2.2) < 0.7 and int(u) % 6 < 4:
            return IRON[5] if t > 0.6 else IRON[4]           # reflet metallique du bois de fer
        return pick([WOOD[0], WOOD[1], IRON[2], WOOD[2]], t)
    cv.fill(pred, col)
    # extremite coupee (cernes) cote haut-droit
    ex, ey = 15.5, 15.5

    def end(x, y, cx, cy):
        d = math.hypot(cx - ex, cy - ey)
        if d > R - 1.0:
            return WOOD[1]                                   # ecorce
        return [GOLD[3], GOLD[2], GOLD[3], GOLD[1], GOLD[2]][int(d * 1.1) % 5]
    cv.fill(lambda cx, cy: math.hypot(cx - ex, cy - ey) <= R, end)
    cv.put(15, 15, WOOD[1])
    cv.outline()
    return cv


def icon_rune_dust() -> Canvas:
    cv = Canvas(24, 32)
    tan = [WOOD[1], WOOD[2], GOLD[2], (189, 132, 74), GOLD[4]]

    def bag(cx, cy):
        return ((cx - 12) / 7.4) ** 2 + ((cy - 21) / 7.0) ** 2 <= 1

    def bcol(x, y, cx, cy):
        dx, dy = (cx - 12) / 7.4, (cy - 21) / 7.0
        return pick(tan, 0.72 - 0.4 * dx - 0.45 * dy)
    cv.fill(bag, bcol)
    cv.fill(lambda cx, cy: 9 <= cx < 15 and 11 <= cy < 15 and abs(cx - 12) <= 1.5 + (14.5 - cy) * 0.6,
            lambda x, y, cx, cy: pick(tan, 0.75 - 0.15 * (cx - 9)))  # col fronce
    for x in range(9, 15):
        cv.put(x, 15, RED[2] if x % 2 else RED[3])            # cordon
    cv.put(15, 16, RED[2]); cv.put(16, 17, RED[1])
    polyline(cv, [(10, 19), (12, 23), (14, 19)], VIOLET[2])   # rune brodee
    polyline(cv, [(12, 18), (12, 25)], VIOLET[2])
    cv.put(12, 21, VIOLET[4])
    cv.outline()
    for (x, y, c) in ((12, 6, GOLD[5]), (12, 5, GOLD[4]), (12, 7, GOLD[4]), (11, 6, GOLD[4]), (13, 6, GOLD[4]),
                      (8, 9, ICE[4]), (16, 8, GOLD[4]), (18, 11, ICE[3]), (6, 12, GOLD[3])):
        cv.put(x, y, c, glow=True)
    return cv


ICONS = [
    ("rm_ember_blade", icon_ember_blade), ("rm_frost_lance", icon_frost_lance),
    ("rm_rift_hammer", icon_rift_hammer), ("rm_ember_shard", icon_ember_shard),
    ("rm_frost_core", icon_frost_core), ("rm_rift_fragment", icon_rift_fragment),
    ("rm_ironwood", icon_ironwood), ("rm_rune_dust", icon_rune_dust),
]

OBJECTS = [
    ("rm_flammable", brazier), ("rm_ice_surface", ice_surface), ("rm_fragile", pillar),
    ("rm_mechanism", mechanism), ("rm_lever", lever),
]


def grain(cv: Canvas) -> Canvas:
    """Grain de pierre deterministe (taches sombres/claires isolees), comme la pierre texturee du decor A1.
    Seuls les pixels STONE interieurs (4 voisins opaques) sont touches ; meme motif pour les deux images."""
    out = {}
    for (x, y), c in cv.px.items():
        if c not in STONE or (x, y) in cv.no_outline:
            continue
        if not all((x + dx, y + dy) in cv.px for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
            continue
        hsh = ((x * 73856093) ^ (y * 19349663)) % 97
        i = STONE.index(c)
        if hsh < 9 and i > 1:
            out[(x, y)] = STONE[i - 1]
        elif hsh > 90 and i < len(STONE) - 1:
            out[(x, y)] = STONE[i + 1]
    cv.px.update(out)
    return cv


def write_a1s(path: Path, frames) -> None:
    """frames : liste (Canvas, ox, oy)."""
    path.parent.mkdir(parents=True, exist_ok=True)
    out = bytearray(b"A1SP") + struct.pack("<H", len(frames))
    for cv, ox, oy in frames:
        out += struct.pack("<HHhh", cv.w, cv.h, ox, oy) + cv.rgba()
    out += struct.pack("<H", 0)
    path.write_bytes(bytes(out))


def preview(path: Path, canvases, scale=4) -> None:
    try:
        from PIL import Image
    except ImportError:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    pad = 2
    W = sum(c.w + pad for c in canvases) + pad
    H = max(c.h for c in canvases) + 2 * pad
    im = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    x = pad
    for c in canvases:
        im.paste(Image.frombytes("RGBA", (c.w, c.h), c.rgba()), (x, pad))
        x += c.w + pad
    im.save(path.with_name(path.stem + "_x1.png"))
    im.resize((W * scale, H * scale), Image.NEAREST).save(path)


def main() -> None:
    sprites = ROOT / "assets" / "sprites"
    prev = ROOT / "extracted" / "preview" / "rm_assets"
    allobj = []
    for name, fn in OBJECTS:
        frames = [grain(fn(False)), grain(fn(True))]
        write_a1s(sprites / f"{name}.a1s", [(c, c.w // 2, c.h - 1) for c in frames])
        allobj += frames
        print(f"{name}.a1s : 2 images {frames[0].w}x{frames[0].h}")
    preview(prev / "objets.png", allobj)
    icons = [fn() for _, fn in ICONS]
    write_a1s(sprites / "rm_icons.a1s", [(c, 0, 0) for c in icons])
    preview(prev / "icones.png", icons)
    txt = ROOT / "assets" / "ui" / "rm_icons.txt"
    txt.parent.mkdir(parents=True, exist_ok=True)
    txt.write_text("# index|id  (genere par tools/rm_assets.py ; assets originaux du remake)\n" +
                   "".join(f"{i}|{n}\n" for i, (n, _) in enumerate(ICONS)), encoding="utf-8")
    print(f"rm_icons.a1s : {len(icons)} icones 24x32 ; rm_icons.txt ecrit")


if __name__ == "__main__":
    main()
