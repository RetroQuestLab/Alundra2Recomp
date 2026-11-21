"""Exporte les zones d'Alundra 1 vers assets/maps/NNN.a1m (format lu par le moteur C++).

Format .a1m (petit-boutiste) :
  char[4] "A1MP"
  u16 largeur, u16 hauteur (en cases)
  u16 nb_tuiles                 tuiles 24x16 deja colorisees (palette appliquee)
  nb_tuiles * 24*16*4 octets    RGBA
  largeur*hauteur cases :
      u8 hauteur, i8 decalage_murs, u8 nb_murs, u8 attributs (voir export_map),
      u16 tuile_dessus (0xFFFF = rien), u16 premier_mur (index dans la table des murs)
  u32 nb_murs_total
  u16 * nb_murs_total           indices de tuiles (0xFFFF = rien)

Usage : python tools/export_assets.py <DATAS.BIN> <dossier_assets>
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from a1_map import GRID, TH, TW, Renderer, load_block, palettes  # noqa: E402
from ez import decompress  # noqa: E402


def export_map(data, idx, path):
    s = load_block(data, idx)
    s1 = s[1]
    W, H = s1[0], s1[1]
    walls_base = GRID + W * H * 8
    gfx, _ = decompress(s[2])
    r = Renderer(gfx, palettes(s[0]))

    remap = {}
    tiles = []

    def tid(v):
        if v == 0xFFFF:
            return 0xFFFF
        if v not in remap:
            remap[v] = len(tiles)
            tiles.append(r.tile(v).tobytes())
        return remap[v]

    cells = bytearray()
    walls = []
    for y in range(H):
        for x in range(W):
            attr, hv, top, ptr = struct.unpack_from("<4H", s1, GRID + (y * W + x) * 8)
            off, n, first = 0, 0, len(walls)
            if ptr != 0xFFFF and walls_base + ptr * 2 + 1 < len(s1):
                q = walls_base + ptr * 2
                off = struct.unpack_from("<b", s1, q)[0]
                n = s1[q + 1]
                for i in range(n):
                    walls.append(tid(struct.unpack_from("<H", s1, q + 2 + i * 2)[0]))
            # attributs compactes : bits 0-3 = attr & 0xF (1 = solide, 2 = vegetation),
            # bits 4-7 = attr >> 12 (1 = bordure, 8 = case speciale)
            flags = (attr & 0x0F) | ((attr >> 8) & 0xF0)
            cells += struct.pack("<BbBBHH", hv >> 8, off, n, flags, tid(top), first)

    with open(path, "wb") as f:
        f.write(b"A1MP" + struct.pack("<HHH", W, H, len(tiles)))
        for t in tiles:
            f.write(t)
        f.write(cells)
        f.write(struct.pack("<I", len(walls)))
        f.write(struct.pack(f"<{len(walls)}H", *walls))


def main():
    data = open(sys.argv[1], "rb").read()
    out = os.path.join(sys.argv[2], "maps")
    os.makedirs(out, exist_ok=True)
    offs = [o for o in struct.unpack_from("<512I", data) if o] + [len(data)]
    count = 0
    for idx in range(10, len(offs) - 1):
        if data[offs[idx]:offs[idx] + 4] != b"\x1c\0\0\0":
            continue
        try:
            export_map(data, idx, os.path.join(out, f"{idx:03d}.a1m"))
            count += 1
        except Exception as e:  # zone au format inattendu : on la saute
            print(f"zone {idx} ignoree : {e}")
    print(f"{count} zones exportees dans {out}")

    # Heros : personnage 0, groupes 0 (immobile), 1 (marche), 16 (epee) ; 4 directions (bas, haut, gauche, droite)
    from a1_sprites import HERO_GROUPS, SpriteBank, export_character
    sprites = os.path.join(sys.argv[2], "sprites")
    os.makedirs(sprites, exist_ok=True)
    bank = SpriteBank(data)
    # Heros : sprites d'origine d'Alundra 1, sans aucune recoloration,
    # y compris la palette 18 des armes : la lame doree A1 est conservee telle quelle.
    frames, anims = export_character(bank, 0, os.path.join(sprites, "hero.a1s"), HERO_GROUPS)
    print(f"Heros exporte : {len(frames)} frames, {len(anims)} animations")

    # Monstres et PNJ : definis par zone (sec3 = definitions, sec4 = planche)
    from a1_map import load_block
    for name, blk, idx, groups in CHARACTERS:
        if blk is None:  # objet de la banque globale (coffre, herbe, fiole...)
            zbank = bank
        else:
            s = load_block(data, blk)
            zbank = SpriteBank(defs=s[3], sheet=s[4])
        try:
            frames, anims = export_character(zbank, idx, os.path.join(sprites, name + ".a1s"), groups)
            print(f"  {name} : {len(frames)} frames")
        except Exception as e:
            print(f"  {name} ignore : {e}")

    # Personnages de l'histoire d'Alundra 2 : on part des vrais sprites A1 exportes ci-dessus.
    # Aucun sprite n'est dessine a la main : seule la palette change (format et resolution A1 conserves).
    from recolor_a1s import PALETTES, recolor
    for src, dst, palette in (("villageoise", "alexia", "alexia"), ("gobelin", "droide", "droide"),
                              ("ancien", "jeehan", "jeehan"), ("ogre", "golem", "golem"),
                              ("gobelin", "capitaine", "capitaine"), ("ogre", "mephisto", "mephisto"),
                              ("villageoise", "milena", "milena")):
        src_path = os.path.join(sprites, src + ".a1s")
        if os.path.exists(src_path):
            with open(src_path, "rb") as f:
                data_out = recolor(f.read(), PALETTES[palette])
            with open(os.path.join(sprites, dst + ".a1s"), "wb") as f:
                f.write(data_out)
            print(f"  {dst} : recoloration de {src} ({len(PALETTES[palette])} couleurs)")

    # Effets de la banque globale (table B) : (nom, effet, animation, premiere etape, derniere etape)
    from a1_sprites import export_effect
    for name, k, anim, first, last in EFFECTS:
        n = export_effect(bank, k, os.path.join(sprites, name + ".a1s"), anim, first, last)
        print(f"  effet {name} : {n} frames")


# Coup charge d'Alundra 1 (repere sur une video du jeu) : orbe clair a l'impact, puis eclats bleus le long de l'arc
EFFECTS = [
    ("fx_orbe", 26, 0, 0, 2),     # orbe blanc puis jaune
    ("fx_eclats", 26, 0, 3, None),  # eclats bleus qui retombent
    ("fx_eclat", 9, 0, 1, None),  # eclat bleu en etoile
    ("fx_etoile", 16, 0, 0, None),  # etoile bleue qui s'eteint
    ("fx_poussiere", 7, 0, 0, None),
]

# (nom, bloc de zone, personnage, groupes d'animations) ; groupe 0 = immobile, 1 = deplacement,
# le 3e groupe exporte (s'il existe) = attaque
CHARACTERS = [
    ("gelee", 54, 204, [0, 1]),
    ("tortue", 13, 181, [0, 1]),
    ("gobelin", 48, 130, [0, 1, 3]),
    ("ogre", 37, 169, [0, 1]),
    ("villageois", 20, 0, [0, 1]),
    ("villageoise", 20, 15, [0, 1]),
    ("ancien", 20, 5, [0, 1]),
    # Banque globale (bloc None) : objets communs a tout le jeu. Coffre : 0 ferme, 1 ouvert, 2 ouverture.
    ("coffre", None, 30, [0, 1, 2]),
    ("coeur", None, 67, [0]),   # fiole de vie
    ("herbe", None, 66, [0]),
    ("lettre", None, 73, [0]),  # parchemin
    ("cle", None, 90, [0]),
]


if __name__ == "__main__":
    main()





