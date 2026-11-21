"""Sprites des personnages d'Alundra 1 (bloc 0 = definitions, bloc 2 = planche EZ 256x2048 4bpp).

Bloc 0 : u32[3] a 0x0C -> table A (personnages, u32 offsets), table B (effets), palettes (32 octets chacune).
Personnage (table A) : u32 anims, u32 etapes, u32 ?, u32 frames.
  anims  : groupes de 7 u16 = 4 directions (offset dans "etapes") + 3 u16 inconnus
  etapes : [0x80|duree, u16 effet (FFFF = aucun), u16 frame] ; 0x01 = fin (boucle) ; 00 01 = boucle ;
           00 00 = fin, jouee une fois ; 00 xx = fin, l'image reste (saut : 00 2c ; KO : 00 4e ; touche : 00 80)
  anims  : les 3 u16 apres les 4 directions = vitesse horizontale, vitesse verticale, drapeaux, en 1/256 px par image :
           marche 208, course 336, saut 1280 vers le haut (groupes 2, 6, 43, 46), recul 256 / 1024 (groupe 62)
  Groupes du heros (personnage 0) : 0 immobile, 1 marche, 2 saut (1 image par direction), 3 course, 4 brule,
           5 ramasser, 6-7-12 porter (saut, marche, immobile), 8 fleau, 9 poser, 10-11 lancer, 14 grimper a une echelle (53 : immobile sur l'echelle),
           15/40 noyade, 16 epee, 17 baton, 18 fleau, 19 arc, 24 coup charge, 28 touche, 29 nage, 32-33 cape de sable,
           54 hochement de tete, 57 KO, 62 projete en arriere
  frame  : a frames + 2*offset : u8 ?, u8 nb_morceaux, puis morceaux de 14 octets :
           u8 options, u8 palette, u8 u, u8 v, u8 largeur, u8 hauteur, 4 coins (i8 x, i8 y)
  Le premier morceau est dessine au-dessus des suivants.
  options : bits 0-2 = page de la planche, bits 3-4 = semi-transparence (voir render_frame).
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ez import decompress  # noqa: E402

from PIL import Image  # noqa: E402


class SpriteBank:
    def __init__(self, datas=None, defs=None, sheet=None):
        """Banque globale (heros, objets : blocs 0 et 2 de DATAS.BIN) si `datas` est donne,
        sinon banque d'une zone : defs = sec3 (definitions), sheet = sec4 (planche EZ)."""
        if datas is not None:
            offs = [o for o in struct.unpack_from("<512I", datas) if o] + [len(datas)]
            defs, sheet = datas[offs[0]:offs[1]], datas[offs[2]:offs[3]]
        self.b = defs
        self.sheet = decompress(sheet)[0] if sheet[:2] == b"EZ" else sheet
        self.table_a, _table_b, self.pal_off, pal_end = struct.unpack_from("<4I", self.b, 0x0C)
        npal = (pal_end - self.pal_off) // 32
        self.pals = []
        self.stp = []
        for p in range(npal):
            c = struct.unpack_from("<16H", self.b, self.pal_off + p * 32)
            self.pals.append([((v & 31) * 255 // 31, ((v >> 5) & 31) * 255 // 31,
                               ((v >> 10) & 31) * 255 // 31, 0 if v & 0x7FFF == 0 else 255) for v in c])
            self.stp.append([bool(v & 0x8000) for v in c])  # bit 15 : texel melange si le morceau est semi-transparent

    def pix(self, x, y):
        o = y * 128 + x // 2
        if not (0 <= x < 256) or o >= len(self.sheet):
            return 0
        return self.sheet[o] >> 4 if x & 1 else self.sheet[o] & 15

    def character(self, idx):
        base = struct.unpack_from("<I", self.b, self.table_a + idx * 4)[0]
        return struct.unpack_from("<4I", self.b, base)

    def frame_parts(self, frames_off, off):
        p = frames_off + off * 2
        n = self.b[p + 1]
        parts = []
        for k in range(n):
            q = p + 2 + k * 14
            fl, pal, u, v, w, h = self.b[q:q + 6]
            parts.append((fl, pal, u, v, w, h, struct.unpack_from("<8b", self.b, q + 6)))
        return parts

    def render_frame(self, frames_off, off):
        """Renvoie (image RGBA, origine_x, origine_y) ; l'origine est le point (0,0) du sprite (les pieds)."""
        parts = self.frame_parts(frames_off, off)
        if not parts:
            return Image.new("RGBA", (1, 1)), 0, 0
        xs = [c for p in parts for c in p[6][0::2]]
        ys = [c for p in parts for c in p[6][1::2]]
        x0, y0 = min(xs), min(ys)
        img = Image.new("RGBA", (max(xs) - x0 + 1, max(ys) - y0 + 1))
        px = img.load()
        for fl, pal, u, v, w, h, c in reversed(parts):
            colors = self.pals[pal % len(self.pals)]
            page = fl & 0x07  # page de 256 lignes dans la planche
            # Bits 3-4 : morceau semi-transparent (lueur du coffre, etincelles, trainee de l'epee). La PS1 AJOUTE
            # alors a l'ecran les texels dont le bit 15 est mis. On l'approche avec un alpha egal a la luminosite :
            # un texel noir n'ajoute rien (invisible), un texel clair eclaire.
            glow = self.stp[pal % len(self.pals)] if fl & 0x18 else None
            (ax, ay, bx, by, cx, cy, _, _) = c
            # Le morceau est un parallelogramme A (haut-gauche), B (haut-droite), C (bas-gauche) :
            # miroirs et rotations (lames d'epee) sont geres par la transformation inverse.
            ux, uy, vx, vy = bx - ax, by - ay, cx - ax, cy - ay
            det = ux * vy - uy * vx
            if det == 0:
                continue
            qx = [ax, bx, cx, bx + cx - ax]
            qy = [ay, by, cy, by + cy - ay]
            for y in range(min(qy), max(qy) + 1):
                for x in range(min(qx), max(qx) + 1):
                    px_, py_ = x + 0.5 - ax, y + 0.5 - ay
                    s = (px_ * vy - py_ * vx) / det
                    t = (ux * py_ - uy * px_) / det
                    if not (0 <= s < 1 and 0 <= t < 1):
                        continue
                    # Texel : comme le GPU de la PS1, on interpole depuis le coin entier du pixel
                    # (pas son centre). Un morceau en miroir lit donc u+1..u+w : le jeu compense
                    # en stockant u-1. Avec le centre, on lisait la colonne voisine de la planche
                    # (traits parasites sur les images tournees vers la droite).
                    si = ((x - ax) * vy - (y - ay) * vx) / det
                    ti = (ux * (y - ay) - uy * (x - ax)) / det
                    ci = self.pix(u + int(si * w + 1e-6), page * 256 + v + int(ti * h + 1e-6))
                    if not ci:
                        continue
                    dx, dy = x - x0, y - y0
                    if 0 <= dx < img.width and 0 <= dy < img.height:
                        if glow and glow[ci]:
                            r, g, b, _ = colors[ci]
                            m = max(r, g, b)
                            if m < 8:
                                continue
                            br, bg, bb, ba = px[dx, dy]  # melange additif sur ce qui est deja dessine dans l'image
                            if ba == 255:
                                px[dx, dy] = (min(255, br + r), min(255, bg + g), min(255, bb + b), 255)
                            else:
                                px[dx, dy] = (r * 255 // m, g * 255 // m, b * 255 // m, max(ba, m))
                        else:
                            px[dx, dy] = colors[ci]
        return img, -x0, -y0

    def anim_loops(self, steps_off, off, limit=64):
        """Vrai si l'animation boucle (se termine par 00 01 ou 01), faux si elle se termine par 00 00."""
        p = steps_off + off
        for _ in range(limit):
            c = self.b[p]
            if c & 0x80:
                p += 5
            else:  # 01 ou 00 01 = boucle ; 00 xx = fin (00 00 jouee une fois, autres = image tenue / suite codee)
                return c == 0x01 or self.b[p + 1] == 0x01
        return True

    def anim_steps(self, steps_off, off, limit=64):
        """Liste de (offset_frame, duree)."""
        p = steps_off + off
        out = []
        while len(out) < limit and p < len(self.b):
            c = self.b[p]
            if c & 0x80:
                out.append((struct.unpack_from("<H", self.b, p + 3)[0], c & 0x7F or 1))
                p += 5
            elif c == 0x01:
                break
            else:  # 01, ou commande 00 xx : toujours la fin de l'animation de cette direction
                break
        return out


# Heros du remake (inspire d'Alundra 2) : on remplace des degrades entiers de la palette 17
# (indices 4-7 = cheveux, 11-14 = tunique ; peau 8-10 et pantalon 2-3 conserves).
HERO_A2_COLORS = {
    4: (38, 22, 14), 5: (86, 50, 30), 6: (140, 92, 54), 7: (198, 152, 100),         # cheveux bruns
    11: (14, 18, 44), 12: (34, 48, 100), 13: (66, 96, 156), 14: (150, 176, 214),    # tunique bleu nuit
}


# Lame de l'arme du remake (palette 18, partagee par toutes les armes du heros d'Alundra 1) :
# acier bleute a la place du beige dore d'Alundra 1 ; la poignee (indices 1-7) est conservee.
WEAPON_A2_COLORS = {
    8: (44, 52, 68), 9: (62, 74, 96), 10: (84, 100, 126), 11: (110, 130, 158),
    12: (142, 164, 190), 13: (176, 196, 216), 14: (204, 220, 236), 15: (236, 244, 252),
}

# Groupes d'animations exportes, dans l'ordre de SpriteBank::Group (src/sprite.h) :
# immobile, marche, epee, course, touche, KO, saut, bras leves (objet obtenu ; c'est la pose "porter"), coup charge
HERO_GROUPS = [0, 1, 16, 3, 28, 57, 2, 12, 24]


# --- Effets (table B) ---------------------------------------------------------------------------
# 29 effets dans la banque globale. Un effet : table de u16 (offsets des animations, le premier donne la taille
# de la table), puis des etapes de 3 octets [0x80|duree, u16 frame] terminees par un octet < 0x80 ; les images ont
# le meme format que celles des personnages (frame = base de l'effet + 2 * offset).
#   0 ombre, 1 et 21 ronds dans l'eau, 2 feuilles, 3 etoile bleue qui se forme, 4 explosion, 5-6 gerbes d'eau,
#   7 et 15 poussiere, 8 et 11 eclats de roche, 9 orbe puis eclat bleu, 10 debris, 12-13 et 27 explosions vertes,
#   14 et 28 etincelles, 16 etoile bleue qui s'eteint, 19 fumee, 20 tornade, 22 rochers, 23 portail bleu,
#   25 trait de lumiere, 26 orbe blanc, orbe jaune, puis eclats bleus (impact du coup charge).
def effect_steps(bank, k, anim=0):
    base = struct.unpack_from("<I", bank.b, 0x434 + k * 4)[0]
    first = struct.unpack_from("<H", bank.b, base)[0]
    offs = [o for o in struct.unpack_from("<%dH" % (first // 2), bank.b, base) if o]
    p, out = base + offs[anim], []
    while bank.b[p] & 0x80:
        out.append((struct.unpack_from("<H", bank.b, p + 1)[0], bank.b[p] & 0x7F))
        p += 3
    return base, out


def export_effect(bank, k, path, anim=0, first=0, last=None):
    """Ecrit un effet comme une banque .a1s d'un seul groupe (jouee une fois), les 4 directions identiques."""
    base, steps = effect_steps(bank, k, anim)
    steps = steps[first:last]
    frames = [bank.render_frame(base, foff) for foff, _ in steps]
    with open(path, "wb") as f:
        f.write(b"A1SP" + struct.pack("<H", len(frames)))
        for img, ox, oy in frames:
            f.write(struct.pack("<HHhh", img.width, img.height, ox, oy))
            f.write(img.tobytes())
        f.write(struct.pack("<H", 1))
        for _ in range(4):
            f.write(struct.pack("<H", len(steps) | 0x8000))
            for i, (_, dur) in enumerate(steps):
                f.write(struct.pack("<HH", i, dur))
    return len(frames)


def recolor_palette(bank, pal_index, colors):
    pal = list(bank.pals[pal_index])
    for i, rgb in colors.items():
        pal[i] = (*rgb, 255)
    bank.pals[pal_index] = pal

def export_character(bank, idx, path, groups):
    anims_off, steps_off, _, frames_off = bank.character(idx)
    frame_ids, frames, anims = {}, [], []
    for g in groups:
        dirs = struct.unpack_from("<4H", bank.b, anims_off + g * 14)
        group = []
        for d in dirs:
            steps = []
            for foff, dur in bank.anim_steps(steps_off, d):
                if foff not in frame_ids:
                    frame_ids[foff] = len(frames)
                    frames.append(bank.render_frame(frames_off, foff))
                steps.append((frame_ids[foff], dur))
            group.append((steps, bank.anim_loops(steps_off, d)))
        anims.append(group)
    with open(path, "wb") as f:
        f.write(b"A1SP" + struct.pack("<H", len(frames)))
        for img, ox, oy in frames:
            f.write(struct.pack("<HHhh", img.width, img.height, ox, oy))
            f.write(img.tobytes())
        f.write(struct.pack("<H", len(anims)))
        for group in anims:
            for steps, loops in group:
                f.write(struct.pack("<H", len(steps) | (0 if loops else 0x8000)))  # bit 15 = jouee une fois
                for fid, dur in steps:
                    f.write(struct.pack("<HH", fid, dur))
    return frames, anims


if __name__ == "__main__":
    data = open(sys.argv[1], "rb").read()
    bank = SpriteBank(data)
    os.makedirs(os.path.dirname(sys.argv[2]) or ".", exist_ok=True)
    frames, anims = export_character(bank, 0, sys.argv[2], HERO_GROUPS)
    print(f"{len(frames)} frames, {len(anims)} groupes d'animations")




