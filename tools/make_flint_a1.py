"""Heros du remake : les sprites d'Alundra 1 (resolution et animations d'origine)
retouches pixel par pixel pour devenir Flint (Alundra 2).

  - cheveux (degrade jaune du haut de la tete)  -> roux
  - meme degrade ailleurs (bottes, sac)          -> cuir marron
  - tunique bleue                                -> haut orange ; au ras du cou -> echarpe bleue
  - pantalon mauve                               -> bleu

    python tools/make_flint_a1.py <DATAS.BIN> assets/sprites/flint.a1s [apercu.png]
"""
import os
import struct
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(__file__))
from a1_sprites import HERO_GROUPS, WEAPON_A2_COLORS, SpriteBank, recolor_palette  # noqa: E402

HAIR = [(58, 10, 16), (138, 28, 26), (206, 60, 40), (255, 126, 78)]
LEATHER = [(40, 20, 10), (90, 52, 28), (140, 90, 52), (192, 140, 92)]
TOP = [(74, 26, 10), (172, 72, 18), (238, 130, 34), (255, 196, 112)]
SCARF = [(16, 20, 72), (36, 58, 152), (72, 112, 222), (150, 190, 250)]
SOLE = [(70, 52, 150), (126, 98, 222), (180, 156, 255)]
PANTS = {2: (52, 66, 172), 3: (104, 124, 228)}


def recolor_frame(img, pal):
    w, h = img.size
    px = img.load()
    index = {tuple(c[:3]): i for i, c in enumerate(pal) if i}
    idx = [[index.get(px[x, y][:3]) if px[x, y][3] else None for x in range(w)] for y in range(h)]
    opaque_rows = [y for y in range(h) if any(px[x, y][3] for x in range(w))]
    if not opaque_rows:
        return img
    yellow = lambda i: i in (4, 5, 6, 7)
    ys = [y for y in range(h) for x in range(w) if yellow(idx[y][x])]
    top = ys[0] if ys else opaque_rows[0]   # sommet des cheveux (l'epee peut depasser au-dessus)
    bottom = opaque_rows[-1]
    # Cheveux : degrade jaune relie au sommet du sprite, limite a la hauteur de la tete
    hair = set()
    stack = [(x, y) for y in range(top, min(h, top + 5)) for x in range(w) if yellow(idx[y][x])]
    hair.update(stack)
    while stack:
        x, y = stack.pop()
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                nx, ny = x + dx, y + dy
                if 0 <= nx < w and top <= ny < top + 16 and (nx, ny) not in hair and yellow(idx[ny][nx]):
                    hair.add((nx, ny))
                    stack.append((nx, ny))
    out = img.copy()
    op = out.load()
    for y in range(h):
        for x in range(w):
            i = idx[y][x]
            if i is None:
                continue
            a = px[x, y][3]
            if yellow(i):
                c = (HAIR if (x, y) in hair else LEATHER)[i - 4]
            elif i in (11, 12, 13, 14):
                c = (SCARF if top + 10 <= y <= top + 13 else TOP)[i - 11]
            elif i in (8, 9, 10) and y >= bottom - 4:   # bout des bottes -> semelles violettes
                c = SOLE[i - 8]
            elif i in PANTS:
                c = PANTS[i]
            else:
                continue
            op[x, y] = (*c, a)
    return out


def main():
    bank = SpriteBank(open(sys.argv[1], "rb").read())
    recolor_palette(bank, 7, WEAPON_A2_COLORS)  # lame en acier bleute
    pal = bank.pals[17]
    anims_off, steps_off, _, frames_off = bank.character(0)
    frame_ids, frames, anims = {}, [], []
    for g in HERO_GROUPS:
        group = []
        for d in struct.unpack_from("<4H", bank.b, anims_off + g * 14):
            steps = []
            for foff, dur in bank.anim_steps(steps_off, d):
                if foff not in frame_ids:
                    frame_ids[foff] = len(frames)
                    img, ox, oy = bank.render_frame(frames_off, foff)
                    frames.append((recolor_frame(img, pal), ox, oy))
                steps.append((frame_ids[foff], dur))
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
        sh = Image.new("RGBA", (30 * 12, 48 * 5), (90, 120, 60, 255))
        for k, (img, ox, oy) in enumerate(frames):
            sh.alpha_composite(img, ((k % 12) * 30, (k // 12) * 48))
        sh.resize((sh.width * 3, sh.height * 3), Image.NEAREST).save(sys.argv[3])


if __name__ == "__main__":
    main()
