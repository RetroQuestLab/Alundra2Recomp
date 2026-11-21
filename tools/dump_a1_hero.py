"""Exporte les images du heros d'Alundra 1 en grilles texte (une lettre par index de palette)
pour les retoucher a la main.
    python tools/dump_a1_hero.py extracted/a1/DATA/DATAS.BIN sortie.txt
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(__file__))
from a1_sprites import HERO_GROUPS, SpriteBank  # noqa: E402

LETTERS = ".0123456789abcdefghijklmnopqrstuvwxyz"


def main():
    bank = SpriteBank(open(sys.argv[1], "rb").read())
    pal = bank.pals[17]
    index = {tuple(c[:3]): i for i, c in enumerate(pal) if i}
    anims_off, steps_off, _, frames_off = bank.character(0)
    seen = []
    out = []
    for g in HERO_GROUPS:
        for d, a in enumerate(struct.unpack_from("<4H", bank.b, anims_off + g * 14)):
            for foff, dur in bank.anim_steps(steps_off, a):
                if foff in seen:
                    continue
                seen.append(foff)
                img, ox, oy = bank.render_frame(frames_off, foff)
                px = img.load()
                out.append(f"# frame {len(seen) - 1} group {g} dir {d} size {img.width}x{img.height} origin {ox},{oy}")
                for y in range(img.height):
                    out.append("".join(
                        LETTERS[index.get(px[x, y][:3], 0) + 1] if px[x, y][3] else "." for x in range(img.width)))
    out.append("# palette " + " ".join(f"{LETTERS[i + 1]}={c[:3]}" for i, c in enumerate(pal) if i))
    open(sys.argv[2], "w").write("\n".join(out))
    print(len(seen), "images")


if __name__ == "__main__":
    main()
