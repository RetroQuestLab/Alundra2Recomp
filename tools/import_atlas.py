"""Version finale du heros : assemble les planches locales.

  - "Atlas pixel-art JRPG du heros roux.png" (fond quasi noir) : immobile, marche, course
    dans les 4 directions ; attaque 1, degats et KO de profil (miroir pour la gauche).
  - "Planche de sprites du heros pixel art.png" (fond vert) : attaque 1 de face et de dos.

    python tools/import_atlas.py <dossier images> assets/sprites/flint.a1s [apercu.png]

Groupes : 0 immobile, 1 marche, 2 attaque, 3 course, 4 degats, 5 KO (x 4 directions :
bas, haut, gauche, droite).
"""
import os
import struct
import sys

from PIL import Image

import import_sheet as sheet_tools

ATLAS = "Atlas pixel-art JRPG du héros roux.png"
GREEN = "Planche de sprites du héros pixel art.png"

ATLAS_ROWS = {0: (40, 106), 2: (110, 173), 3: (177, 240), 1: (244, 308)}  # bas, gauche, droite, dos
ATLAS_SCALE = 0.68


def atlas_bg(p):
    r, g, b = p[:3]
    return r < 34 and g < 32 and b < 44 and abs(r - g) < 12


def green_bg(p):
    r, g, b = p[:3]
    return g > 90 and g > r + 12 and g > b + 45 and r < 150


def grab(img, bg, scale, x0, x1, y0, y1, n, mirror=False):
    sheet_tools.is_bg = bg
    sheet_tools.SCALE = scale
    out = []
    for spr, fx in sheet_tools.extract(img, x0, x1, y0, y1, n):
        if mirror:
            spr = spr.transpose(Image.FLIP_LEFT_RIGHT)
            fx = spr.width - 1 - fx
        out.append(sheet_tools.shrink(spr, fx))
    return out


def main():
    folder, out = sys.argv[1], sys.argv[2]
    atlas = Image.open(os.path.join(folder, ATLAS)).convert("RGB")
    green = Image.open(os.path.join(folder, GREEN)).convert("RGB")
    A = lambda *a, **k: grab(atlas, atlas_bg, ATLAS_SCALE, *a, **k)
    G = lambda *a, **k: grab(green, green_bg, 0.5, *a, **k)

    groups = []  # liste de (images par direction, durees, drapeaux)
    groups.append(([A(133, 316, *ATLAS_ROWS[d], 3) for d in range(4)], [40, 12, 40, 12], 0, [0, 1, 2, 1]))
    groups.append(([A(350, 709, *ATLAS_ROWS[d], 7) for d in range(4)], [5] * 7, 0, None))
    side_atk = (22, 412, 352, 414, 6)
    groups.append(([G(972, 1302, 60, 160, 6), G(972, 1302, 445, 565, 6),
                    A(*side_atk, mirror=True), A(*side_atk)], [3, 2, 2, 3, 4, 5], 0x8000, None))
    groups.append(([A(749, 1159, *ATLAS_ROWS[d], 8) for d in range(4)], [4] * 8, 0, None))
    hurt = (22, 272, 545, 605, 4)
    h = A(*hurt)
    groups.append(([h, h, A(*hurt, mirror=True), h], [4, 4, 6, 8], 0x8000, None))
    ko = (880, 1258, 545, 605, 6)
    k = A(*ko)
    groups.append(([k, k, A(*ko, mirror=True), k], [6, 6, 6, 8, 10, 60], 0x8000, None))

    frames, anims = [], []
    for per_dir, durs, flags, order in groups:
        for d in range(4):
            first = len(frames)
            frames.extend(per_dir[d])
            idx = order if order else list(range(len(per_dir[d])))
            steps = [(first + i, durs[j % len(durs)]) for j, i in enumerate(idx)]
            anims.append((steps, flags))

    with open(out, "wb") as f:
        f.write(b"A1SP" + struct.pack("<H", len(frames)))
        for img, ox, oy in frames:
            f.write(struct.pack("<HHhh", img.width, img.height, ox, oy))
            f.write(img.tobytes())
        f.write(struct.pack("<H", len(groups)))
        for steps, flags in anims:
            f.write(struct.pack("<H", len(steps) | flags))
            for fr, du in steps:
                f.write(struct.pack("<HH", fr, du))
    print(f"{len(frames)} images -> {out}")

    if len(sys.argv) > 3:
        cw, ch = 56, 50
        prev = Image.new("RGBA", (cw * 8, ch * 4 * len(groups)), (90, 120, 60, 255))
        k = 0
        for gi, (per_dir, _, _, _) in enumerate(groups):
            for d in range(4):
                for t, (img, ox, oy) in enumerate(per_dir[d]):
                    prev.alpha_composite(img, (t * cw + 28 - ox, (gi * 4 + d) * ch + 46 - oy))
        prev.resize((prev.width * 3, prev.height * 3), Image.NEAREST).save(sys.argv[3])


if __name__ == "__main__":
    main()
