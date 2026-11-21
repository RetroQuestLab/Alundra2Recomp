"""Genere le sprite original de Flint (heros d'Alundra 2) en pixel art.

Aucun pixel n'est repris d'un jeu : le personnage est assemble a partir de formes simples
(tete, cheveux en epis, echarpe, epauliere, pantalon bouffant...), puis ombre en 3 tons
avec des contours colores (style 32 bits PlayStation).

    python tools/make_flint.py assets/sprites/flint.a1s [apercu.png]
"""
import math
import struct
import sys

from PIL import Image

W, H = 36, 50          # taille d'une image
OX, B = 18, 44         # position des pieds (B = premiere ligne sous les semelles)

# Palettes : (clair, base, sombre)
PAL = {
    "skin":  ((255, 218, 176), (240, 178, 130), (196, 122, 88)),
    "hair":  ((255, 124, 76), (222, 62, 42), (150, 30, 34)),
    "scarf": ((96, 140, 236), (52, 84, 196), (30, 44, 124)),
    "top":   ((255, 178, 70), (242, 124, 34), (184, 72, 24)),
    "pad":   ((206, 178, 255), (152, 122, 232), (96, 72, 172)),
    "brown": ((156, 100, 62), (114, 68, 42), (72, 42, 28)),
    "belt":  ((140, 88, 50), (100, 60, 36), (64, 38, 24)),
    "pants": ((118, 128, 236), (74, 84, 204), (46, 52, 146)),
    "cuff":  ((150, 160, 246), (104, 114, 222), (60, 66, 160)),
    "pantsb": ((86, 94, 200), (60, 66, 170), (38, 42, 120)),
    "sole":  ((176, 146, 255), (132, 104, 232), (82, 62, 162)),
    "gold":  ((255, 234, 128), (222, 182, 62), (150, 110, 30)),
    "blade": ((248, 252, 255), (196, 210, 230), (112, 128, 156)),
    "eye":   ((70, 24, 34), (70, 24, 34), (40, 12, 20)),
    "white": ((255, 255, 255), (255, 255, 255), (255, 255, 255)),
}
FLAT = {"eye", "white", "blade"}          # pas d'ombrage automatique
NO_OUTLINE_INNER = {"eye", "white", "blade", "gold"}


class Canvas:
    def __init__(self):
        self.part = [[None] * W for _ in range(H)]
        self.smear = []   # traînee de l'epee (pixels additifs)

    def px(self, x, y, p):
        x, y = int(round(x)), int(round(y))
        if 0 <= x < W and 0 <= y < H:
            self.part[y][x] = p

    def rect(self, x0, y0, x1, y1, p):
        for y in range(int(y0), int(y1) + 1):
            for x in range(int(x0), int(x1) + 1):
                self.px(x, y, p)

    def ellipse(self, cx, cy, rx, ry, p, clip=None):
        for y in range(int(cy - ry) - 1, int(cy + ry) + 2):
            for x in range(int(cx - rx) - 1, int(cx + rx) + 2):
                if ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 <= 1.0 and (clip is None or clip(x, y)):
                    self.px(x, y, p)

    def line(self, x0, y0, x1, y1, p, w=1):
        n = int(max(abs(x1 - x0), abs(y1 - y0)) * 2) + 1
        for i in range(n + 1):
            t = i / n
            x, y = x0 + (x1 - x0) * t, y0 + (y1 - y0) * t
            if w == 1:
                self.px(x, y, p)
            else:
                self.rect(round(x) - (w - 1) // 2, round(y) - (w - 1) // 2,
                          round(x) + w // 2, round(y) + w // 2, p)

    def render(self):
        img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        P = self.part
        get = lambda x, y: P[y][x] if 0 <= x < W and 0 <= y < H else None
        for y in range(H):
            for x in range(W):
                p = P[y][x]
                if p is None:
                    # Contour colore : teinte sombre de la piece voisine
                    nb = [get(x + dx, y + dy) for dx, dy in ((0, 1), (0, -1), (1, 0), (-1, 0))]
                    nb = [n for n in nb if n is not None]
                    if nb:
                        c = PAL[nb[0]][2]
                        img.putpixel((x, y), (c[0] // 3, c[1] // 4, c[2] // 3 + 10, 255))
                    continue
                light, base, dark = PAL[p]
                c = base
                if p not in FLAT:
                    up, left = get(x, y - 1) != p, get(x - 1, y) != p
                    down, right = get(x, y + 1) != p, get(x + 1, y) != p
                    if (down or right) and not (up or left):
                        c = dark
                    elif (up or left) and not (down or right):
                        c = light
                    # ligne interieure : bas d'une piece posee sur une autre
                    below = get(x, y + 1)
                    if below is not None and below != p and p not in NO_OUTLINE_INNER \
                            and below not in NO_OUTLINE_INNER and below != "eye":
                        c = tuple(v * 3 // 5 for v in dark)
                img.putpixel((x, y), (*c, 255))
        for x, y, a in self.smear:  # arc lumineux de l'epee
            if 0 <= x < W and 0 <= y < H:
                r, g, b, al = img.getpixel((x, y))
                k = a / 255
                img.putpixel((x, y), (min(255, int(r * (1 - k) + 230 * k)), min(255, int(g * (1 - k) + 245 * k)),
                                      255, max(al, a)))
        return img


# ---------------------------------------------------------------------------------------
# Pieces du corps. dir : 0 bas, 1 haut, 2 gauche, 3 droite. Les vues de gauche sont
# dessinees en miroir de la droite, mais l'epauliere (epaule gauche) change de cote.

def head_front(c, by, back=False):
    hy = by - 31
    if not back:
        c.ellipse(OX, hy + 1, 5.6, 5.4, "skin")
        # cheveux : calotte + epis
        c.ellipse(OX, hy - 2, 6.6, 5.0, "hair", clip=lambda x, y: y <= hy - 1)
        for x, y in ((OX - 6, hy - 1), (OX - 6, hy), (OX - 6, hy + 1), (OX - 5, hy + 2), (OX + 6, hy - 1),
                     (OX + 6, hy), (OX + 6, hy + 1), (OX + 5, hy + 2),
                     (OX - 3, hy), (OX - 2, hy), (OX - 2, hy + 1), (OX + 1, hy), (OX + 2, hy), (OX + 3, hy),
                     (OX + 3, hy + 1), (OX - 4, hy)):
            c.px(x, y, "hair")
        for x, y in ((OX - 4, hy - 7), (OX - 1, hy - 8), (OX, hy - 8), (OX + 3, hy - 7), (OX + 6, hy - 5),
                     (OX - 7, hy - 4), (OX + 1, hy - 9)):
            c.px(x, y, "hair")
        # yeux (grands, style anime) et reflets
        for ex in (OX - 3, OX + 2):
            c.rect(ex, hy + 2, ex + 1, hy + 3, "eye")
            c.px(ex + 1, hy + 2, "white")
        c.px(OX, hy + 5, "brown")  # bouche discrete
    else:
        c.ellipse(OX, hy, 6.4, 6.0, "hair")
        c.rect(OX - 4, hy + 4, OX + 4, hy + 6, "hair")
        for x, y in ((OX - 4, hy - 7), (OX - 1, hy - 8), (OX + 2, hy - 8), (OX + 5, hy - 6), (OX - 7, hy - 3),
                     (OX + 7, hy - 2), (OX - 2, hy + 7), (OX + 2, hy + 7)):
            c.px(x, y, "hair")


def head_side(c, by):
    hy = by - 31
    c.ellipse(OX + 1, hy + 1, 5.0, 5.4, "skin")
    c.px(OX + 6, hy + 2, "skin")  # nez
    c.ellipse(OX - 1, hy - 1, 6.0, 5.4, "hair", clip=lambda x, y: x <= OX + 1 or y <= hy - 2)
    for x, y in ((OX + 3, hy - 1), (OX + 4, hy - 1), (OX + 5, hy - 2), (OX + 2, hy), (OX - 1, hy + 3),
                 (OX - 1, hy + 4), (OX, hy + 5)):
        c.px(x, y, "hair")
    for x, y in ((OX - 8, hy - 2), (OX - 8, hy + 1), (OX - 7, hy - 5), (OX - 4, hy - 7), (OX, hy - 8),
                 (OX + 3, hy - 7), (OX - 7, hy + 3)):
        c.px(x, y, "hair")
    c.rect(OX + 3, hy + 2, OX + 4, hy + 3, "eye")
    c.px(OX + 4, hy + 2, "white")


def legs_front(c, by, lift_l, lift_r):
    for side, lift in ((-1, lift_l), (1, lift_r)):
        x0 = OX - 5 if side < 0 else OX + 1
        top = by - 14
        bot = B - 7 - lift
        c.rect(x0, top, x0 + 4, bot, "pants")
        c.rect(x0 - (1 if side < 0 else 0), bot - 3, x0 + 4 + (1 if side > 0 else 0), bot, "pants")
        c.rect(x0 - (1 if side < 0 else 0), bot - 1, x0 + 4 + (1 if side > 0 else 0), bot, "cuff")
        c.rect(x0, bot + 1, x0 + 4, B - 2 - lift, "brown")
        c.rect(x0, B - 2 - lift, x0 + 4, B - 1 - lift, "sole")
    c.rect(OX - 5, by - 14, OX + 5, by - 11, "pants")


def legs_side(c, by, stride):
    for k, (off, part) in enumerate(((-stride, "pantsb"), (stride, "pants"))):
        x0 = OX - 2 + off
        lift = 1 if (k == 0 and abs(stride) > 1) else 0
        c.rect(OX - 3, by - 14, OX + 2, by - 11, part)
        c.line(OX - 1, by - 12, x0 + 1, B - 8 - lift, part, w=4)
        c.rect(x0 - 1, B - 10 - lift, x0 + 3, B - 7 - lift, part if part == "pantsb" else "pants")
        c.rect(x0 - 1, B - 8 - lift, x0 + 3, B - 7 - lift, "cuff" if part == "pants" else "pantsb")
        c.rect(x0 - 1, B - 6 - lift, x0 + 3, B - 2 - lift, "brown")
        c.rect(x0 + 4, B - 4 - lift, x0 + 4, B - 2 - lift, "brown")  # bout de la botte
        c.rect(x0 - 1, B - 2 - lift, x0 + 4, B - 1 - lift, "sole")


def torso_front(c, by, back=False):
    c.rect(OX - 4, by - 22, OX + 4, by - 15, "top")
    c.rect(OX - 5, by - 21, OX + 5, by - 17, "top")
    # lanière en diagonale
    if back:
        c.line(OX + 4, by - 22, OX - 3, by - 15, "brown")
    else:
        c.line(OX - 4, by - 22, OX + 3, by - 15, "brown")
    c.rect(OX - 5, by - 15, OX + 5, by - 14, "belt")
    if not back:
        c.rect(OX - 1, by - 15, OX, by - 14, "gold")
    px = OX + 4 if not back else OX - 6
    c.rect(px, by - 14, px + 2, by - 12, "brown")  # sacoche
    # echarpe
    c.rect(OX - 4, by - 24, OX + 4, by - 22, "scarf")
    c.rect(OX - 3, by - 25, OX + 3, by - 24, "scarf")


def scarf_tail(c, by, dx, flutter):
    c.line(OX + dx, by - 22, OX + dx - 1 + flutter, by - 18, "scarf", w=2)


def arm(c, sx, sy, hx, hy):
    c.line(sx, sy, hx, hy, "skin", w=2)
    c.rect(round(hx) - 1, round(hy) - 1, round(hx) + 1, round(hy) + 1, "brown")  # mitaine


def pad(c, x, y, r=3.4):
    c.ellipse(x, y, r, r * 0.9, "pad")


def torso_side(c, by):
    c.rect(OX - 3, by - 22, OX + 2, by - 15, "top")
    c.line(OX + 2, by - 22, OX - 2, by - 15, "brown")
    c.rect(OX - 3, by - 15, OX + 2, by - 14, "belt")
    c.rect(OX - 5, by - 14, OX - 3, by - 12, "brown")
    c.rect(OX - 3, by - 25, OX + 3, by - 22, "scarf")


# ---------------------------------------------------------------------------------------
def sword(c, hx, hy, ang, length=14):
    vx, vy = math.cos(ang), math.sin(ang)
    c.line(hx - vx * 3, hy - vy * 3, hx, hy, "brown")                 # poignee
    c.line(hx + vx * 2, hy + vy * 2, hx + vx * length, hy + vy * length, "blade", w=2 if abs(vx) > 0.4 and abs(vy) > 0.4 else 1)
    px, py = -vy, vx
    c.line(hx + vx - px * 2, hy + vy - py * 2, hx + vx + px * 2, hy + vy + py * 2, "gold")  # garde
    return hx + vx * length, hy + vy * length


FACING = {0: (0, 1), 1: (0, -1), 3: (1, 0)}


def frame(d, mode, t):
    """mode : idle / walk / attack ; t : indice de l'image."""
    mirror = d == 2
    dd = 3 if mirror else d
    c = Canvas()
    bob = 0
    if mode == "walk":
        p = t / 6 * 2 * math.pi
        bob = -1 if t % 3 == 1 else 0
    if mode == "idle":
        bob = 1 if t == 1 else 0
    by = B + bob

    attacking = mode == "attack"
    # angles de l'epee (radians) par image, relatifs a la direction du regard
    swing = [-2.0, -1.3, -0.4, 0.5, 1.15, 1.25][t] if attacking else 0
    fx, fy = FACING[dd]
    base = math.atan2(fy, fx)

    if dd == 0:  # face
        if attacking:
            lean = [0, 0, 1, 1, 1, 0][t]
            by += lean
        flutter = (t % 2) if mode != "attack" else 1
        if mode == "walk":
            ll = 2 if t in (1, 2) else 0
            lr = 2 if t in (4, 5) else 0
        else:
            ll = lr = 0
        legs_front(c, by, ll, lr)
        torso_front(c, by)
        scarf_tail(c, by, -3, flutter - 1)
        swingy = (1 if t in (1, 2) else -1 if t in (4, 5) else 0) if mode == "walk" else 0
        if attacking:
            ang = base - swing   # main droite = cote gauche de l'ecran
            hx, hy = OX - 6 + 5 * math.cos(ang) * 0.6, by - 18 + 4 * math.sin(ang)
            arm(c, OX + 6, by - 21, OX + 7, by - 15)
            pad(c, OX + 6, by - 21)
            head_front(c, by)
            arm(c, OX - 6, by - 21, hx, hy)
            tip = sword(c, hx, hy, ang)
        else:
            arm(c, OX - 6, by - 21, OX - 7, by - 15 + swingy)
            arm(c, OX + 6, by - 21, OX + 7, by - 15 - swingy)
            pad(c, OX + 6, by - 21)
            head_front(c, by)
    elif dd == 1:  # dos
        if mode == "walk":
            ll = 2 if t in (1, 2) else 0
            lr = 2 if t in (4, 5) else 0
        else:
            ll = lr = 0
        swingy = (1 if t in (1, 2) else -1 if t in (4, 5) else 0) if mode == "walk" else 0
        if attacking:  # l'epee passe derriere la tete
            ang = base + swing
            hx, hy = OX + 6 + 4 * math.cos(ang) * 0.6, by - 21 + 3 * math.sin(ang)
            tip = sword(c, hx, hy, ang)
        legs_front(c, by, ll, lr)
        torso_front(c, by, back=True)
        head_front(c, by, back=True)
        c.rect(OX - 1, by - 23, OX + 1, by - 21, "scarf")  # noeud
        scarf_tail(c, by, 1, (t % 2))
        arm(c, OX - 6, by - 21, OX - 7, by - 15 - swingy)
        pad(c, OX - 6, by - 21)
        if attacking:
            arm(c, OX + 6, by - 21, hx, hy)
        else:
            arm(c, OX + 6, by - 21, OX + 7, by - 15 + swingy)
    else:  # profil (droite)
        stride = [0, 2, 3, 0, -2, -3][t] if mode == "walk" else (2 if attacking and t >= 2 else 0)
        swingx = [0, -2, -3, 0, 2, 3][t] if mode == "walk" else 0
        if not mirror:
            pass
        else:
            pad(c, OX - 1, by - 22, 2.6)  # epauliere du cote oppose, a moitie cachee
        legs_side(c, by, stride)
        torso_side(c, by)
        c.line(OX - 3, by - 23, OX - 6 - (t % 2), by - 19, "scarf", w=2)  # pan d'echarpe
        head_side(c, by)
        if attacking:
            ang = base + swing
            hx, hy = OX + 1 + 6 * math.cos(ang), by - 19 + 5 * math.sin(ang)
            arm(c, OX, by - 21, hx, hy)
            tip = sword(c, hx, hy, ang)
        else:
            arm(c, OX, by - 21, OX + swingx, by - 15)
        if not mirror:
            pad(c, OX, by - 21)

    if attacking and t in (2, 3, 4) and dd != 1:  # arc lumineux suivant la pointe
        a0 = [-2.0, -1.3, -0.4, 0.5, 1.15][t - 1]
        a1 = swing
        cx0, cy0 = (OX, B - 19)
        sgn = -1 if dd == 0 else 1
        for i in range(24):
            a = base + sgn * (a0 + (a1 - a0) * i / 23)
            for rr, alpha in ((15, 140), (16, 200), (17, 120)):
                c.smear.append((int(cx0 + math.cos(a) * rr), int(cy0 + math.sin(a) * rr * 0.8),
                                int(alpha * (0.4 + 0.6 * i / 23)) if t != 4 else alpha // 3))

    img = c.render()
    if mirror:
        img = img.transpose(Image.FLIP_LEFT_RIGHT)
    return img


def main():
    out = sys.argv[1]
    frames, anims = [], []
    groups = [("idle", 3), ("walk", 6), ("attack", 6)]
    for mode, n in groups:
        for d in range(4):
            first = len(frames)
            for t in range(n):
                frames.append(frame(d, mode, t))
            idx = list(range(first, first + n))
            if mode == "idle":
                steps, flags = [(idx[0], 50), (idx[1], 14), (idx[2], 40), (idx[1], 14)], 0
            elif mode == "walk":
                steps, flags = [(i, 5) for i in idx], 0
            else:
                steps, flags = list(zip(idx, (4, 2, 2, 3, 4, 6))), 0x8000
            anims.append((steps, flags))
    ox = OX
    with open(out, "wb") as f:
        f.write(b"A1SP" + struct.pack("<H", len(frames)))
        for img in frames:
            f.write(struct.pack("<HHhh", W, H, ox, B - 1))
            f.write(img.tobytes())
        f.write(struct.pack("<H", len(groups)))
        for steps, flags in anims:
            f.write(struct.pack("<H", len(steps) | flags))
            for fr, du in steps:
                f.write(struct.pack("<HH", fr, du))
    print(f"{len(frames)} images -> {out}")
    if len(sys.argv) > 2:  # planche d'apercu agrandie x4 sur fond vert
        sheet = Image.new("RGBA", (W * 6, H * 12), (90, 120, 60, 255))
        row = 0
        k = 0
        for mode, n in groups:
            for d in range(4):
                for t in range(n):
                    sheet.alpha_composite(frames[k], (t * W, row * H))
                    k += 1
                row += 1
        sheet.resize((sheet.width * 4, sheet.height * 4), Image.NEAREST).save(sys.argv[2])


if __name__ == "__main__":
    main()
