"""Flint construit a partir d'un vrai personnage d'Alundra 1 qui lui ressemble deja :
le jeune homme du bloc 318 (personnage 103) : haut sans manches, bras nus, cheveux en epis,
pantalon bouffant, bottes. On garde son dessin et ses animations (immobile, marche, 4 directions)
et on le recolore en Flint ; l'attaque a l'epee vient du heros retouche (flint_retouch.py).

    python tools/flint_npc.py <DATAS.BIN> assets/sprites/flint.a1s [apercu.png]
"""
import os
import struct
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(__file__))
from a1_map import load_block  # noqa: E402
from a1_sprites import WEAPON_A2_COLORS, SpriteBank, recolor_palette  # noqa: E402
import flint_retouch as R  # noqa: E402

NPC_BLOCK, NPC_INDEX, NPC_PAL = 318, 103, 2
F = R.FLINT

# Palette 2 du PNJ : 1 contour ; 2-3 haut jaune ; 4 brun fonce ; 5,6,7,15 cheveux gris-bleu
# (5 sert aussi de contour des bras) ; 8-10 peau ; 11-15 pantalon/bottes gris.
def material(i, y, h, top):
    y -= top
    h -= top
    if i == 1:
        return "1"
    if y <= 10:  # tete
        return {4: "q", 5: "r", 6: "R", 7: "H", 15: "h"}.get(i)
    if y >= h - 6 and i == 4:  # ombre au sol dessinee dans le sprite
        return "_"
    if y >= h - 9:  # bottes
        if y >= h - 2:
            return {11: "1", 12: "v", 13: "V", 14: "V", 15: "V"}.get(i)
        return {11: "t", 12: "T", 13: "u", 14: "U", 15: "U"}.get(i)
    if y <= 21:  # torse
        return {2: "O", 3: "Y", 12: "o", 11: "o", 13: "Y", 14: "y", 4: "T", 5: "T"}.get(i)
    return {11: "n", 12: "n", 13: "N", 14: "M", 15: "Z", 2: "N", 3: "M", 4: "t"}.get(i)  # pantalon


def recolor(img, pal):
    index = {tuple(c[:3]): i for i, c in enumerate(pal) if i}
    w, h = img.size
    px = img.load()
    rows = [y for y in range(h) if any(px[x, y][3] for x in range(w))]
    top = rows[0] if rows else 0
    bottom = rows[-1] + 1 if rows else h
    out = img.copy()
    op = out.load()
    for y in range(h):
        for x in range(w):
            p = px[x, y]
            if not p[3]:
                continue
            m = material(index.get(p[:3], 0), y, bottom, top)
            if m:
                op[x, y] = (*F[m], 255)
    return out


def npc_groups(data):
    s = load_block(data, NPC_BLOCK)
    zb = SpriteBank(defs=s[3], sheet=s[4])
    a, st, _, fo = zb.character(NPC_INDEX)
    pal = zb.pals[NPC_PAL]
    groups = []
    for g in (0, 1):
        per_dir = []
        for d in struct.unpack_from("<4H", zb.b, a + g * 14):
            per_dir.append(([(recolor(*zb.render_frame(fo, f)[:1], pal), zb.render_frame(fo, f)[1],
                              zb.render_frame(fo, f)[2], du) for f, du in zb.anim_steps(st, d)],
                            zb.anim_loops(st, d)))
        groups.append(per_dir)
    return groups


def hero_attack(data):
    bank = SpriteBank(data)
    recolor_palette(bank, 7, WEAPON_A2_COLORS)
    pal = bank.pals[17]
    index = {tuple(c[:3]): i for i, c in enumerate(pal) if i}
    a, st, _, fo = bank.character(0)
    offs = []
    for g in R.HERO_GROUPS:
        for d in struct.unpack_from("<4H", bank.b, a + g * 14):
            for f, _ in bank.anim_steps(st, d):
                if f not in offs:
                    offs.append(f)
    bases = {d: R.grid(bank.render_frame(fo, offs[R.BASE[d]])[0], index) for d in range(4)}
    per_dir = []
    for d, ad in enumerate(struct.unpack_from("<4H", bank.b, a + 16 * 14)):
        steps = []
        for f, du in bank.anim_steps(st, ad):
            img, ox, oy = bank.render_frame(fo, f)
            new, add = R.retouch(img, pal, offs.index(f), d, bases[d])
            steps.append((new, ox, oy + add, du))
        per_dir.append((steps, bank.anim_loops(st, ad)))
    return per_dir


def main():
    data = open(sys.argv[1], "rb").read()
    groups = npc_groups(data) + [hero_attack(data)]
    frames, anims = [], []
    for per_dir in groups:
        for steps, loops in per_dir:
            seq = []
            for img, ox, oy, du in steps:
                frames.append((img, ox, oy))
                seq.append((len(frames) - 1, du))
            anims.append((seq, loops))
    with open(sys.argv[2], "wb") as f:
        f.write(b"A1SP" + struct.pack("<H", len(frames)))
        for img, ox, oy in frames:
            f.write(struct.pack("<HHhh", img.width, img.height, ox, oy))
            f.write(img.tobytes())
        f.write(struct.pack("<H", len(groups)))
        for seq, loops in anims:
            f.write(struct.pack("<H", len(seq) | (0 if loops else 0x8000)))
            for fid, du in seq:
                f.write(struct.pack("<HH", fid, du))
    print(f"{len(frames)} images -> {sys.argv[2]}")
    if len(sys.argv) > 3:
        cw, ch = 40, 56
        n = max(len(s) for g in groups for s, _ in g)
        sh = Image.new("RGBA", (cw * n, ch * 4 * len(groups)), (90, 120, 60, 255))
        for gi, per_dir in enumerate(groups):
            for d, (steps, _) in enumerate(per_dir):
                for t, (img, ox, oy, _) in enumerate(steps):
                    sh.alpha_composite(img, (t * cw + 20 - ox, (gi * 4 + d) * ch + 50 - oy))
        sh.resize((sh.width * 3, sh.height * 3), Image.NEAREST).save(sys.argv[3])


if __name__ == "__main__":
    main()


