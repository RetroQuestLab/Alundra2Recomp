"""Reconstruction d'une zone d'Alundra 1 en PNG.

Bloc de zone (DATAS.BIN, entrees >= 10) : 7 sections.
  sec0 : en-tete + palettes (16 couleurs BGR555, a partir de 0x10)
  sec1 : u8 largeur, u8 hauteur, 2 octets ?, 0x600 octets (table inconnue),
         puis largeur*hauteur cases de 8 octets :
            u16 attributs, u16 (hauteur<<8 | ?), u16 tuile du dessus, u16 pointeur murs
         puis la zone des murs : listes [i8 decalage, u8 nombre] + nombre tuiles ; le mur i est dessine a (i + 1 - decalage) rangees sous la face du dessus.
  sec2 : tuiles du decor (EZ), feuille 4bpp de 256 px de large, tuiles 24x16 (10 par ligne).
Tuile u16 : bits 0-11 numero, bits 12-15 palette ; 0xFFFF = rien.

Usage : python tools/a1_map.py <DATAS.BIN> <bloc> <sortie.png>
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ez import decompress  # noqa: E402

from PIL import Image  # noqa: E402

GRID = 0x604
TW, TH = 24, 16  # tuiles de 24x16, rangees par 10 dans la feuille de 256 px


def load_block(data, idx):
    offs = [o for o in struct.unpack_from("<512I", data) if o] + [len(data)]
    b = data[offs[idx]:offs[idx + 1]]
    h = list(struct.unpack_from("<7I", b)) + [len(b)]
    return [b[h[k]:h[k + 1]] for k in range(7)]


def palettes(s0):
    pals = []
    for p in range(16):
        c = struct.unpack_from("<16H", s0, 0x10 + p * 32)
        pals.append([((v & 31) * 255 // 31, ((v >> 5) & 31) * 255 // 31,
                      ((v >> 10) & 31) * 255 // 31, 0 if v == 0 else 255) for v in c])
    return pals


class Renderer:
    def __init__(self, gfx, pals):
        self.gfx, self.pals = gfx, pals
        self.cache = {}

    def tile(self, v):
        if v in self.cache:
            return self.cache[v]
        t, pal = v & 0xFFF, self.pals[v >> 12]
        tx, ty = (t % 10) * TW, (t // 10) * TH
        im = Image.new("RGBA", (TW, TH))
        px = im.load()
        for j in range(TH):
            for i in range(TW):
                o = (ty + j) * 128 + (tx + i) // 2
                if o >= len(self.gfx):
                    continue
                c = self.gfx[o] >> 4 if (tx + i) & 1 else self.gfx[o] & 15
                if c:
                    px[i, j] = pal[c]
        self.cache[v] = im
        return im


def render(data, idx, scale_h=16):
    s = load_block(data, idx)
    s1 = s[1]
    W, H = s1[0], s1[1]
    walls = GRID + W * H * 8
    gfx, _ = decompress(s[2])
    r = Renderer(gfx, palettes(s[0]))
    margin = TH * 16
    img = Image.new("RGBA", (W * TW, H * TH + margin), (0, 0, 0, 255))
    for y in range(H):
        for x in range(W):
            attr, hv, top, ptr = struct.unpack_from("<4H", s1, GRID + (y * W + x) * 8)
            hgt = hv >> 8
            sy = margin + (y - hgt) * TH if scale_h else margin + y * TH
            if ptr != 0xFFFF and walls + ptr * 2 + 1 < len(s1):
                q = walls + ptr * 2
                off = struct.unpack_from("<b", s1, q)[0]
                n = s1[q + 1]
                for i in range(n):
                    v = struct.unpack_from("<H", s1, q + 2 + i * 2)[0]
                    if v != 0xFFFF:
                        img.alpha_composite(r.tile(v), (x * TW, sy + (i + 1 - off) * TH))
            if top != 0xFFFF:
                img.alpha_composite(r.tile(top), (x * TW, sy))
    return img


def main():
    data = open(sys.argv[1], "rb").read()
    render(data, int(sys.argv[2])).save(sys.argv[3])


if __name__ == "__main__":
    main()


