"""Extraction deterministe des graphismes NATIFS de l'ecran d'inventaire d'Alundra 1.

Sources (ressources A1 locales, jamais publiées) :
  * extracted/a1/TAKI/SCREEN/WIND.TX  : 4bpp 256x256, pixel pair = nibble faible.
    WIND.CL : 16 banques de 16 couleurs BGR555 (banques 0..8 utilisees, 9..15 vides).
    Les regions du menu n'utilisent PAS toutes la meme banque ; banque par region
    mesuree par correspondance exacte des couleurs 5 bits sur gp/images.png
    (voir docs/visuel/a1_menu_assets.md) :
        parchemin / bandeaux ......... banque 0 (desc. parchemin : 99 % exact, banques 0 et 1 equivalentes)
        panneau pierre a briques ..... banque 3 (72 % exact, le reste = icones/carte par-dessus)
        crochets du curseur .......... banque 5 (100 % par gabarit exact ; corrige : pas 3)
        chiffres dores x9999 et "x" .. banque 5 (100 % par gabarit exact)
        icones des compteurs ......... banque 6 (piece/oiseau/cle : 74-82 % ; 2 index ecartes
                                       de +-1 niveau 5 bits = ecart de version FR/EN)
  * extracted/a1/DATA/DATAS.BIN bloc 2 : planche EZ 256x2048 4bpp (heros, objets).
    Bloc 0 : definitions de sprites, 39 palettes (tools/a1_sprites.py).
    - icones d'objets/armes : page 7 (y >= 1792), cellules 24x32. L'affectation
      cellule -> palette est lue dans les enregistrements de morceaux (14 octets)
      du bloc 0 : options&7 == 7, palette 30..36, rectangle non tourne.
    - portrait d'Alundra du menu : enregistrement bloc 0 @0xB7EA :
      page 2, u=200, v=56 (y=568), 48x56, palette 16.

Ecarts connus : gp/images.png est la version anglaise ; le disque local est la
version francaise (PAL). Pixels des icones identiques (alignement exact), mais
les palettes 30..35 de la reference sont legerement plus claires que celles du
disque local (ecart non uniforme : autre version). On exporte les palettes du
disque local, seule source native disponible.

Usage :
    py -3.12 tools/a1_menu_assets.py            # extraction complete
    py -3.12 tools/a1_menu_assets.py --check    # + score de correspondance avec gp/images.png
    py -3.12 tools/a1_menu_assets.py --reconstruct  # + recomposition des 2 ecrans et superpositions
    py -3.12 tools/a1_menu_assets.py --hud          # + HUD de jeu recompose sur des captures A1 (A1_REF)
Sorties : extracted/preview/menu_assets/ (planches, reconstructions), extracted/preview/hud_assets/
(HUD), assets/ui/a1_menu/ (PNG RGBA, banks.txt) et assets/sprites/a1_ui.a1s, a1_icons.a1s,
a1_hud.a1s (format SpriteBank::load).
Mise en page mesuree : docs/visuel/a1_menu_layout.md (LAYOUT) et docs/visuel/a1_hud_layout.md (HUD_LAYOUT).
"""
from __future__ import annotations

import os
import struct
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ez import decompress  # noqa: E402

from PIL import Image  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
A1 = ROOT / "extracted" / "a1"
PREVIEW = ROOT / "extracted" / "preview" / "menu_assets"
OUT = ROOT / "assets" / "ui" / "a1_menu"

# Regions de WIND.TX : nom -> (x, y, largeur, hauteur, banque CLUT)
WIND_REGIONS = {
    "parchment_tile": (16, 0, 80, 40, 0),      # texture de parchemin (panneau de description)
    "scroll_banner": (96, 0, 160, 40, 0),      # bandeaux a rouleaux (nom d'arme / d'objet)
    "digits_white": (0, 40, 104, 16, 0),       # 0123456789/x (petits, blancs)
    "stone_panel": (176, 72, 80, 56, 3),       # panneau pierre a briques diagonales + biseau
    "counter_icons": (56, 112, 56, 16, 6),     # piece, cle, oiseau (fleches), x
    "digits_gold": (0, 136, 104, 16, 5),       # chiffres dores des compteurs
    "cursor_brackets": (0, 152, 72, 40, 5),    # crochets du curseur de grille (banque 5 mesuree)
    "small_icons": (80, 160, 32, 16, 6),       # sac, cle, orbe
}

PORTRAIT = (200, 568, 48, 56, 16)  # x, y (planche), l, h, palette ; enregistrement bloc 0 @0xB7EA


def bgr555(v: int) -> tuple[int, int, int, int]:
    """Couleur PSX 5 bits -> RGBA (expansion <<3 | >>2) ; 0x0000 = transparent."""
    r, g, b = v & 31, (v >> 5) & 31, (v >> 10) & 31
    return ((r << 3) | (r >> 2), (g << 3) | (g >> 2), (b << 3) | (b >> 2), 0 if v & 0x7FFF == 0 else 255)


def nibbles(data: bytes) -> list[int]:
    out: list[int] = []
    for byte in data:
        out.append(byte & 15)
        out.append(byte >> 4)
    return out


def render(idx: list[int], width: int, pal: list[int], box: tuple[int, int, int, int]) -> Image.Image:
    x0, y0, w, h = box
    colors = [bgr555(v) for v in pal]
    img = Image.new("RGBA", (w, h))
    img.putdata([colors[idx[(y0 + y) * width + x0 + x]] for y in range(h) for x in range(w)])
    return img


def load_wind() -> tuple[list[int], list[list[int]]]:
    tx = (A1 / "TAKI" / "SCREEN" / "WIND.TX").read_bytes()
    cl = (A1 / "TAKI" / "SCREEN" / "WIND.CL").read_bytes()
    return nibbles(tx), [list(struct.unpack_from("<16H", cl, b * 32)) for b in range(16)]


def load_datas() -> tuple[bytes, list[int], list[list[int]]]:
    d = (A1 / "DATA" / "DATAS.BIN").read_bytes()
    offs = [o for o in struct.unpack_from("<512I", d) if o] + [len(d)]
    b0, b2 = d[offs[0]:offs[1]], d[offs[2]:offs[3]]
    sheet = decompress(b2)[0]
    _ta, _tb, pal_off, pal_end = struct.unpack_from("<4I", b0, 0x0C)
    pals = [list(struct.unpack_from("<16H", b0, pal_off + p * 32)) for p in range((pal_end - pal_off) // 32)]
    return b0, nibbles(sheet), pals


def icon_records(b0: bytes) -> list[tuple[int, int, int, int, int]]:
    """Morceaux de sprite page 7 (icones), non tournes, palettes 30..36 : (u, y, w, h, pal) uniques."""
    recs: Counter = Counter()
    for q in range(len(b0) - 14):
        fl, pal, u, v, w, h = b0[q:q + 6]
        if fl & 7 != 7 or fl >= 0x20 or not 30 <= pal <= 36 or not (8 <= w <= 32 and 8 <= h <= 32) or u + w > 256:
            continue
        ax, ay, bx, by, cx, cy, dx, dy = struct.unpack_from("<8b", b0, q + 6)
        if by == ay and cx == ax and abs(bx - ax) == w and cy - ay == h and dx == bx and dy == cy:
            recs[(u, 1792 + v, w, h, pal)] += 1
    return sorted(recs)


def contact(images: list[tuple[str, Image.Image]], cols: int, scale: int, bg=(255, 0, 255, 255)) -> Image.Image:
    cw = max(i.width for _, i in images) + 2
    ch = max(i.height for _, i in images) + 2
    rows = (len(images) + cols - 1) // cols
    sheet = Image.new("RGBA", (cols * cw, rows * ch), bg)
    for k, (_, im) in enumerate(images):
        sheet.alpha_composite(im, ((k % cols) * cw + 1, (k // cols) * ch + 1))
    return sheet.resize((sheet.width * scale, sheet.height * scale), Image.NEAREST)


def check_banks(wind_pals: list[list[int]]) -> None:
    """Pourcentage de pixels de gp/images.png egaux (5 bits) a une couleur de chaque banque."""
    ref = Image.open(ROOT / "gp" / "images.png").convert("RGB")
    zones = {"stone": (20, 20, 160, 45), "desc_parchment": (30, 180, 290, 220),
             "digits_gold": (205, 100, 232, 110), "coin": (180, 97, 192, 110), "cursor": (15, 15, 24, 22)}
    for name, box in zones.items():
        px = [((r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10)) for r, g, b in ref.crop(box).getdata()]
        score = sorted(((sum(p in set(pal) for p in px) * 100 // len(px)), k) for k, pal in enumerate(wind_pals))
        print(f"  {name:15s} meilleures banques (%, banque) : {score[::-1][:3]}")


# ---------------------------------------------------------------------------
# Banques runtime (.a1s) : elements d'interface et icones
# ---------------------------------------------------------------------------
# Elements d'interface : nom -> (x, y, l, h, banque CLUT de WIND.TX). Rectangles et banques
# mesures par gabarit exact sur gp/images.png (voir docs/visuel/a1_menu_layout.md).
UI_PIECES: list[tuple[str, tuple[int, int, int, int, int]]] = [
    ("ui.stone", (176, 72, 80, 64, 3)),          # tuile pierre complete (biseau 8 px + interieur 64x48)
    ("ui.stone_tl", (176, 72, 8, 8, 3)),         # decoupe 9 parties de ui.stone
    ("ui.stone_t", (184, 72, 64, 8, 3)),
    ("ui.stone_tr", (248, 72, 8, 8, 3)),
    ("ui.stone_l", (176, 80, 8, 48, 3)),
    ("ui.stone_c", (184, 80, 64, 48, 3)),
    ("ui.stone_r", (248, 80, 8, 48, 3)),
    ("ui.stone_bl", (176, 128, 8, 8, 3)),
    ("ui.stone_b", (184, 128, 64, 8, 3)),
    ("ui.stone_br", (248, 128, 8, 8, 3)),
    ("ui.stone_ext", (160, 72, 96, 64, 3)),      # atlas des bords : bande 160..176 + tuile (morceaux 8x8)
    ("ui.parchment", (16, 0, 80, 40, 0)),        # corps de parchemin (tuile 80x40)
    ("ui.scroll_top", (120, 0, 80, 8, 0)),       # bord haut du parchemin (tuile 80x8)
    ("ui.scroll_bottom", (120, 32, 80, 8, 0)),   # bord bas du parchemin (tuile 80x8)
    ("ui.roll_l", (96, 0, 24, 40, 0)),           # rouleau gauche (40 px ; bandeau : 16 px du bas)
    ("ui.roll_l_top", (224, 0, 24, 16, 0)),      # chapeau du rouleau gauche
    ("ui.roll_r", (200, 0, 24, 40, 0)),          # rouleau droit (40 px ; bandeau : 16 px du bas)
    ("ui.roll_r_top", (224, 24, 24, 16, 0)),     # chapeau du rouleau droit
    *[(f"ui.digit{d}", (8 * d, 136, 8, 16, 5)) for d in range(10)],   # chiffres dores
    ("ui.times", (104, 119, 8, 11, 5)),          # signe "x" des compteurs
    ("ui.coin", (56, 116, 16, 17, 6)),           # piece (compteur d'or)
    ("ui.key", (72, 116, 16, 17, 6)),            # cle (compteur de cles)
    ("ui.bird", (88, 116, 16, 17, 6)),           # oiseau / plume (2e compteur, "x 50")
    ("ui.cursor_tl", (49, 153, 6, 6, 5)),        # coins du curseur (identiques dans les 3 images
    ("ui.cursor_tr", (66, 153, 6, 6, 5)),        #  de l'animation de WIND.TX ; seul l'ecart change)
    ("ui.cursor_bl", (49, 178, 6, 6, 5)),
    ("ui.cursor_br", (66, 178, 6, 6, 5)),
]

# Noms des 82 icones, dans l'ordre de icon_records() (tri u, y, l, h, palette).
# Noms donnes d'apres l'inspection visuelle de extracted/preview/menu_layout/icons_indexed_x4.png.
ICON_NAMES = [
    "bomb", "book_silver", "jar_dark", "card_blue", "chest_ornate", "book_silver_b", "book_blue",
    "sword_short", "sword_long", "bow_green", "urn_gold", "urn_silver", "flail1", "ring_small_gold",
    "ring_small_silver", "key_silver", "flask_green", "lamp_white", "jar_gray", "sword_gold",
    "empty_72_1824", "boots_brown", "bracelet_gold", "armor_silver", "key_gold_ornate", "sword_gold_b",
    "sword_ice", "boots_blue", "boots_blue_b", "glove_green", "ring_silver", "bracer_silver",
    "ring_gold", "bag_green", "map_green", "crystal_ball", "sword_legend", "orb_gold", "flail2",
    "boots_charm", "flail3", "key_gold", "key_green", "key_blue", "wand_blue", "wand_red",
    "flask_blue", "flask_red", "seed_gold", "seed_red", "tome_gold", "tome_green", "tome_blue",
    "tome_pink", "amulet_blue", "amulet_gold", "cape_green", "cape_gold", "gem_green", "bow_gold",
    "vial_green", "vial_blue", "wand_pink", "scroll_gold", "scroll_green", "scroll_blue",
    "scroll_pink", "flower_pot", "herb", "gem_purple", "gem_square_dark", "gem_square_red",
    "gem_small_red", "crystal_cluster", "gem_white", "gem_white_small", "gem_red_oval", "gem_silver",
    "gem_blue", "gem_tiny_red", "chest_white", "chest_blue",
]
# Icones visibles dans gp/images.png (ecran 2, gemmes jaunes) mais hors filtre palette 30..36 :
# enregistrements bloc 0 @0x321d2 et @0x32276 (options 7, palette 26). Ajoutees APRES les 82
# pour garder des index stables. Les autres morceaux palette 26..29/37 de la page 7 (lingot,
# coffres, statue, etincelles, ombres) sont des objets de terrain : non exportes.
EXTRA_ICONS = [((192, 1984, 24, 24, 26), "gem_yellow_square"), ((216, 1984, 24, 24, 26), "gem_yellow_drop")]

# Mise en page mesuree au pixel natif sur gp/images.png (ecran 1 : y 0..239 ; ecran 2 : y 242..481).
# Rectangles (x, y, l, h) en coordonnees ecran 320x240. Detail : docs/visuel/a1_menu_layout.md.
LAYOUT = {
    # Panneaux pierre (banque 3). Interieur : ui.stone_c (64x48) repete depuis (x+8, y+8), decale de
    # phase_y lignes dans la tuile. Bords : morceaux 8x8 MESURES un par un (sequences non periodiques
    # dans la reference), coordonnees (cx, cy) relatives au coin haut-gauche de la tuile pierre
    # (WIND.TX 176,72) ; cx = -16 / -8 = bande verticale WIND.TX 160 / 168. Dans ui.stone_ext
    # (WIND.TX 160,72 96x64), le morceau (cx, cy) est en (cx + 16, cy).
    "screen1_stone": [
        {"rect": (8, 16, 168, 48), "phase_y": 0,
         "top": [0, 8, 16, 24, 32, 40, 48, 56, 64, 8, 16, 24, 32, 40, 48, 56, 64, 8, 16, -16, -8],
         "bottom": [0, 40, 24, 32, 40, 48, 56, 16, 24, 32, 40, 48, 56, 40, 16, 24, 32, 40, 48, 56, 72],
         "left": [(0, 0), (0, 8), (0, 32), (0, 24), (0, 48), (0, 56)],
         "right": [(-8, 0), (-8, 8), (72, 16), (72, 32), (72, 48), (72, 56)]},
        {"rect": (8, 64, 168, 104), "phase_y": 8,
         "top": [0, 8, 16, 24, 32, 40, 48, 56, 64, 8, 16, 24, 32, 40, 48, 56, 64, 8, 16, -16, -8],
         "bottom": [0, 8, 16, 24, 32, 40, 48, 56, 64, 8, 16, 24, 32, 40, 48, 56, 16, 24, 32, 40, 72],
         "left": [(0, 0)] + [(0, 8 * k) for k in (2, 3, 4, 5, 6, 1, 2, 3, 4, 5, 6)] + [(0, 56)],
         "right": [(-8, 0)] + [(-8, 8 * k) for k in (2, 3, 4, 5, 6, 1, 2, 3, 4, 5, 6)] + [(72, 56)]},
    ],
    "screen2_stone": [
        {"rect": (8, 16, 120, 112), "phase_y": 0,
         "top": [0, 8, 16, 24, 32, 40, 48, 56, 64, 8, 16, 24, 32, 40, -8],
         "bottom": [0, 8, 16, 24, 32, 40, 48, 56, 64, 8, 16, 24, 32, 40, 72],
         "left": [(0, 8 * k) for k in (0, 1, 2, 3, 4, 5, 6, 1, 2, 3, 4, 5, 6, 7)],
         "right": [(-8, 0), (-8, 8), (-8, 16)] + [(72, 8 * k) for k in (2, 3, 4, 5, 2, 1, 2, 3, 4, 5, 7)]},
        {"rect": (136, 16, 40, 112), "phase_y": 0,
         "top": [0, 8, 16, 24, 72],
         "bottom": [0, 8, 16, 64, 72],
         "left": [(0, 8 * k) for k in (0, 1, 2, 3, 4, 5, 6, 1, 2, 3, 4, 5, 6, 7)],
         "right": [(72, 8 * k) for k in (0, 1, 1, 5, 3, 4, 5, 6, 1, 2, 3, 4, 5, 7)]},
        {"rect": (8, 128, 168, 40), "phase_y": 0,
         "top": [0, 8, 16, 24, 32, 40, 48, 56, 64, 8, 16, 24, 32, 40, 48, 56, 64, 8, 16, -16, -8],
         "bottom": [0, 8, 16, 24, 32, 40, 48, 56, 64, 8, 16, 24, 32, 40, 48, 56, 64, 8, 16, 64, 72],
         "left": [(0, 0), (0, 8), (0, 16), (0, 24), (0, 56)],
         "right": [(-8, 0), (-8, 8), (-8, 16), (-8, 24), (72, 56)]},
    ],
    "banners": [(176, 16, 144, 32), (176, 64, 144, 32)],   # identiques sur les deux ecrans
    "description": (16, 168, 288, 56),
    "portrait": (248, 104, 48, 56),
    "counter_rows_y": [100, 124, 148],                     # piece, oiseau, cle
    "counter_icon_x": 176, "counter_times_dx": 16, "counter_times_dy": 3,
    "counter_digit_x": 200, "counter_digit_pitch": 8,
    # Ecran 1 : rangee d'armes (6) + grille d'objets 6x3 ; ecran 2 : 7 gemmes + colonne + rangee
    "screen1_weapons": [(16 + 24 * i, 24) for i in range(6)],
    "screen1_items": [(16 + 24 * i, 72 + 28 * j) for j in range(3) for i in range(6)],
    "screen2_gems": [(16, 36), (56, 20), (96, 36), (56, 60), (16, 84), (96, 84), (56, 100)],
    "screen2_equip": [(144, 40), (144, 80)],
    "screen2_keyrow": [(16, 132), (48, 132)],
    "cursor_offsets": ((1, 1), (18, 1), (1, 26), (18, 26)),   # TL, TR, BL, BR / origine de l'icone
    # Quantite d'un objet (herbe "9") : chiffres dores 8x16 alignes a droite, dernier chiffre en
    # (cellule + 16, cellule + 16). Un seul exemple dans la reference (cellule sous le curseur).
    "item_qty_last_digit": (16, 16),
}


# ---------------------------------------------------------------------------
# HUD de jeu (banque separee a1_hud) : mesure sur les captures A1 640x512 (= 320x256 x2, PAL ;
# zone active 320x240 en haut, 16 lignes noires en bas) du dossier de référence local, par gabarit exact
# 5 bits sur WIND.TX (voir docs/visuel/a1_hud_layout.md). Les pieces marquees "non observee"
# sont des variantes voisines dans WIND.TX dont l'usage n'est pas atteste par une capture fixe.
# ---------------------------------------------------------------------------
HUD_PIECES: list[tuple[str, tuple[int, int, int, int, int] | None]] = [
    ("hud.item_box", None),                       # genere : polygone degrade mesure (voir hud_item_box)
    ("hud.gem_big_full", (96, 40, 16, 16, 7)),    # grosse gemme pleine (dizaine de PV remplie)
    ("hud.gem_big_112", (112, 40, 16, 16, 7)),    # non observee
    ("hud.gem_big_128", (128, 40, 16, 16, 7)),    # non observee
    ("hud.gem_big_empty", (144, 40, 16, 16, 7)),  # grosse gemme vide (grise)
    ("hud.gem_small_full", (112, 56, 8, 8, 7)),   # petite gemme pleine (1 PV)
    ("hud.gem_small_120", (120, 56, 8, 8, 7)),    # non observee
    ("hud.gem_small_128", (128, 56, 8, 8, 7)),    # non observee
    ("hud.gem_small_empty", (136, 56, 8, 8, 7)),  # petite gemme vide (grise)
    *[(f"hud.digit{d}", (8 * d, 40, 8, 16, 5)) for d in range(10)],  # = ui.digit* (meme dessin, rangee y 40)
    ("hud.slash", (80, 40, 8, 16, 5)),            # "/" du maximum de PV
    ("hud.mp_full_a", (8, 56, 8, 16, 5)),         # cristal de magie plein : 4 images d'animation
    ("hud.mp_full_b", (16, 56, 8, 16, 5)),        #  observees a -> b -> c -> d ; le 2e cristal a
    ("hud.mp_full_c", (24, 56, 8, 16, 5)),        #  une image d'avance sur le 1er
    ("hud.mp_full_d", (40, 56, 8, 16, 5)),
    ("hud.mp_empty", (104, 56, 8, 16, 5)),        # cristal vide (sombre)
    ("hud.mp_0", (0, 56, 8, 16, 5)),              # non observee
    ("hud.mp_48", (48, 56, 8, 16, 5)),            # non observee
    ("hud.mp_56", (56, 56, 8, 16, 5)),            # non observee
    ("hud.mp_64", (64, 56, 8, 16, 5)),            # non observee
    ("hud.mp_80", (80, 56, 8, 16, 5)),            # non observee
    ("hud.mp_88", (88, 56, 8, 16, 5)),            # non observee
    ("hud.mp_96", (96, 56, 8, 16, 5)),            # non observee
    ("hud.gold_bag", (144, 56, 8, 16, 5)),        # bourse a droite du compteur d'or
    ("hud.gold_bag_152", (152, 56, 8, 16, 5)),    # non observee
    ("hud.gold_bag_160", (160, 56, 8, 16, 5)),    # non observee
    ("hud.gold_bag_168", (168, 56, 8, 16, 5)),    # non observee
]

# Positions natives (x, y) mesurees ; ecran 320x240, origine en haut a gauche.
HUD_LAYOUT = {
    "item_boxes": [(16, 16), (48, 16)],   # cases 24x32 : arme equipee, objet choisi ; icone a l'origine
    "gem_big_x": 104, "gem_big_y": 16, "gem_big_pitch": -8,   # grosses gemmes vers la gauche, la plus a droite dessinee en dernier
    "gem_small_x": 120, "gem_small_rows_y": (16, 24), "gem_small_pitch": 8, "gem_small_cols": 5,
    "slash": (160, 16), "max_digits_x": 168, "digit_pitch": 8, "digit_y": 16,
    "mp_x": 216, "mp_y": 16, "mp_pitch": 8,
    "gold_digits_x": 256, "gold_digits": 4, "gold_bag": (288, 16),
}


def hud_item_box() -> Image.Image:
    """Case d'objet du HUD : quad degrade semi-transparent (mode PSX 0,5 B + 0,5 F).

    Mesure sur lp07_21 (fond noir) : sur noir, la case vaut 128 * poids bilineaires des
    couleurs de sommet HG noir, HD jaune, BG cyan, BD bleu (u = x / 24, v = y / 32) ; ajustement
    sur 993 pixels de bord : I = 0,48..0,50 * fond + constante, donc melange 50 %. On exporte la
    couleur pleine (2 x valeur observee sur noir) avec alpha 128 : SDL_BLENDMODE_BLEND reproduit
    0,5 * case + 0,5 * fond. Ce n'est PAS une texture de WIND.TX.
    """
    corners = {(0, 0): (0, 0, 0), (1, 0): (255, 255, 0), (0, 1): (0, 255, 255), (1, 1): (0, 0, 255)}
    im = Image.new("RGBA", (24, 32))
    for y in range(32):
        for x in range(24):
            u, v = x / 24, y / 32
            c = [sum(corners[(i, j)][k] * (u if i else 1 - u) * (v if j else 1 - v) for i in (0, 1) for j in (0, 1))
                 for k in range(3)]
            im.putpixel((x, y), (*(min(255, round(ch)) for ch in c), 128))
    return im


def hud_images(widx: list[int], wpals: list[list[int]]) -> list[tuple[str, Image.Image, str]]:
    out = []
    for name, spec in HUD_PIECES:
        if spec is None:
            out.append((name, hud_item_box(), "genere: quad degrade HG(0,0,0) HD(255,255,0) BG(0,255,255) BD(0,0,255) alpha 128"))
            continue
        x, y, w, h, bank = spec
        out.append((name, render(widx, 256, wpals[bank], (x, y, w, h)), f"WIND.TX {x},{y} {w}x{h} clut_bank={bank}"))
    return out


def write_a1s(path: Path, images: list[Image.Image]) -> None:
    """Banque lue par SpriteBank::load : "A1SP", u16 n, (u16 w, u16 h, i16 ox, i16 oy, RGBA)*n, u16 0 groupe."""
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "wb") as f:
        f.write(b"A1SP" + struct.pack("<H", len(images)))
        for im in images:
            im = im.convert("RGBA")
            f.write(struct.pack("<HHhh", im.width, im.height, 0, 0))
            f.write(im.tobytes())
        f.write(struct.pack("<H", 0))


def build_banks(widx: list[int], wpals: list[list[int]], sidx: list[int], spals: list[list[int]],
                recs: list[tuple[int, int, int, int, int]]) -> dict[str, Image.Image]:
    assert len(ICON_NAMES) == len(recs) == len(set(ICON_NAMES))
    lines = ["# banque|index|nom|w|h|source  (genere par tools/a1_menu_assets.py ; ne pas publier)"]
    named: dict[str, Image.Image] = {}
    ui = []
    for k, (name, (x, y, w, h, bank)) in enumerate(UI_PIECES):
        im = render(widx, 256, wpals[bank], (x, y, w, h))
        ui.append(im)
        named[name] = im
        lines.append(f"a1_ui|{k}|{name}|{w}|{h}|WIND.TX {x},{y} {w}x{h} clut_bank={bank}")
    icons = []
    for k, ((u, y, w, h, pal), name) in enumerate(list(zip(recs, ICON_NAMES)) + EXTRA_ICONS):
        im = render(sidx, 256, spals[pal], (u, y, w, h))
        icons.append(im)
        named[f"icon.{name}"] = im
        lines.append(f"a1_icons|{k}|icon.{name}|{w}|{h}|DATAS.BIN/blk2 {u},{y} {w}x{h} blk0_pal={pal}")
    hud = []
    for k, (name, im, source) in enumerate(hud_images(widx, wpals)):
        hud.append(im)
        named[name] = im
        lines.append(f"a1_hud|{k}|{name}|{im.width}|{im.height}|{source}")
    sprites = ROOT / "assets" / "sprites"
    write_a1s(sprites / "a1_ui.a1s", ui)
    write_a1s(sprites / "a1_icons.a1s", icons)
    write_a1s(sprites / "a1_hud.a1s", hud)
    (OUT / "banks.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"banques : assets/sprites/a1_ui.a1s ({len(ui)} images), assets/sprites/a1_icons.a1s ({len(icons)} images),"
          f" assets/sprites/a1_hud.a1s ({len(hud)} images)")
    return named


# ---------------------------------------------------------------------------
# Reconstruction de controle (hors jeu) a partir des seules ressources + LAYOUT
# ---------------------------------------------------------------------------
def _tile(dst: Image.Image, src: Image.Image, x: int, y: int, w: int, h: int, phx: int = 0, phy: int = 0) -> None:
    """Remplit (x, y, w, h) en repetant src, la tuile commencant (phx, phy) pixels plus loin."""
    for yy in range(-phy, h, src.height):
        for xx in range(-phx, w, src.width):
            sx0, sy0 = max(0, -xx), max(0, -yy)
            sx1, sy1 = min(src.width, w - xx), min(src.height, h - yy)
            if sx1 > sx0 and sy1 > sy0:
                dst.alpha_composite(src.crop((sx0, sy0, sx1, sy1)), (x + xx + sx0, y + yy + sy0))


def draw_stone(dst: Image.Image, p: dict[str, Image.Image], panel: dict) -> None:
    x, y, w, h = panel["rect"]
    _tile(dst, p["ui.stone_c"], x + 8, y + 8, w - 16, h - 16, 0, panel["phase_y"])
    ext = p["ui.stone_ext"]

    def chunk(cx: int, cy: int, dx: int, dy: int) -> None:
        dst.alpha_composite(ext.crop((cx + 16, cy, cx + 24, cy + 8)), (dx, dy))

    for k, cx in enumerate(panel["top"]):
        chunk(cx, 0, x + 8 * k, y)
    for k, cx in enumerate(panel["bottom"]):
        chunk(cx, 56, x + 8 * k, y + h - 8)
    for k, (cx, cy) in enumerate(panel["left"]):
        chunk(cx, cy, x, y + 8 * k)
    for k, (cx, cy) in enumerate(panel["right"]):
        chunk(cx, cy, x + w - 8, y + 8 * k)


def draw_scroll(dst: Image.Image, p: dict[str, Image.Image], x: int, y: int, w: int, h: int) -> None:
    """Parchemin a rouleaux : bandeau de nom (h = 32) ou panneau de description (h = 56)."""
    body = w - 48
    _tile(dst, p["ui.scroll_top"], x + 24, y, body, 8)
    _tile(dst, p["ui.parchment"], x + 24, y + 8, body, h - 16)
    _tile(dst, p["ui.scroll_bottom"], x + 24, y + h - 8, body, 8)
    dst.alpha_composite(p["ui.roll_l_top"], (x, y))
    dst.alpha_composite(p["ui.roll_r_top"], (x + w - 24, y))
    roll_h = h - 16                      # 16 (bandeau) ou 40 (description) : bas du rouleau
    dst.alpha_composite(p["ui.roll_l"].crop((0, 40 - roll_h, 24, 40)), (x, y + 16))
    dst.alpha_composite(p["ui.roll_r"].crop((0, 40 - roll_h, 24, 40)), (x + w - 24, y + 16))


def draw_counters(dst: Image.Image, p: dict[str, Image.Image], values: list[str]) -> None:
    L = LAYOUT
    for y, icon, val in zip(L["counter_rows_y"], ("ui.coin", "ui.bird", "ui.key"), values):
        dst.alpha_composite(p[icon], (L["counter_icon_x"], y))
        dst.alpha_composite(p["ui.times"], (L["counter_icon_x"] + L["counter_times_dx"], y + L["counter_times_dy"]))
        for k, ch in enumerate(val):
            if ch != " ":
                dst.alpha_composite(p[f"ui.digit{ch}"], (L["counter_digit_x"] + L["counter_digit_pitch"] * k, y))


def draw_cursor(dst: Image.Image, p: dict[str, Image.Image], x: int, y: int) -> None:
    for (dx, dy), c in zip(LAYOUT["cursor_offsets"], ("tl", "tr", "bl", "br")):
        dst.alpha_composite(p[f"ui.cursor_{c}"], (x + dx, y + dy))


def _score_zones(n: int) -> dict[str, tuple[int, int, int, int]]:
    z = {f"pierre{k}": s["rect"] for k, s in enumerate(LAYOUT[f"screen{n}_stone"])}
    z.update({f"bandeau{k}": b for k, b in enumerate(LAYOUT["banners"])})
    z["description"] = LAYOUT["description"]
    for k, y in enumerate(LAYOUT["counter_rows_y"]):
        z[f"compteur{k}"] = (176, y, 56, 17)
    return z


# Icones reellement visibles dans gp/images.png (identifiees par gabarit), pour la variante "verif".
REF_ICONS = {
    1: ["sword_legend", "flail1", "bow_green", "sword_ice", "wand_pink", "wand_blue",
        "herb", "flask_green", "flask_blue", "vial_blue", "urn_gold", "bomb",
        "tome_gold", "tome_blue", "tome_pink", "tome_green", "cape_gold", "cape_green",
        "amulet_blue", "glove_green", "seed_gold", "ring_silver", "bracer_silver", "ring_gold"],
    2: ["gem_blue", "gem_purple", "gem_yellow_square", "gem_silver", "gem_yellow_drop", "gem_red_oval",
        "gem_green", "armor_silver", "boots_charm", "card_blue", "map_green"],
}


def compose(p: dict[str, Image.Image], n: int, icon_names: list[str]) -> Image.Image:
    L = LAYOUT
    cells = L["screen1_weapons"] + L["screen1_items"] if n == 1 else         L["screen2_gems"] + L["screen2_equip"] + L["screen2_keyrow"]
    im = Image.new("RGBA", (320, 240), (0, 0, 0, 255))
    for panel in L[f"screen{n}_stone"]:
        draw_stone(im, p, panel)
    for x, y, w, h in L["banners"]:
        draw_scroll(im, p, x, y, w, h)
    draw_scroll(im, p, *L["description"])
    draw_counters(im, p, ["9999", "  50", "  00"])
    for (x, y), name in zip(cells, icon_names):
        im.alpha_composite(p[f"icon.{name}"], (x, y))
    if n == 1:                                   # quantite de l'herbe (cellule 0 de la grille)
        cx, cy = L["screen1_items"][0]
        qx, qy = L["item_qty_last_digit"]
        im.alpha_composite(p["ui.digit9"], (cx + qx, cy + qy))
    for x, y in ([L["screen1_weapons"][0], L["screen1_items"][0]] if n == 1 else L["screen2_equip"]):
        draw_cursor(im, p, x, y)
    return im


def reconstruct(p: dict[str, Image.Image]) -> None:
    """Recompose les 2 ecrans (textes omis) et les superpose a gp/images.png.

    Variante "hasard" (icones tirees au hasard) : reconstruction_ecranN.png, overlay_ecranN_*.png.
    Variante "verif" (icones identifiees dans la reference) : *_verif*.png + scores par zone.
    """
    import random
    from PIL import ImageDraw
    rnd = random.Random(2)
    pool = [k[5:] for k in p if k.startswith("icon.") and k != "icon.empty_72_1824"]
    ref = Image.open(ROOT / "gp" / "images.png").convert("RGBA")
    for n in (1, 2):
        r = ref.crop((0, 0 if n == 1 else 242, 320, 240 if n == 1 else 482))
        r.save(PREVIEW / f"reference_ecran{n}.png")
        for variant, names in (("", [rnd.choice(pool) for _ in range(30)]), ("_verif", REF_ICONS[n])):
            im = compose(p, n, names)
            px, py, pw, ph = LAYOUT["portrait"]
            ImageDraw.Draw(im).rectangle([px, py, px + pw - 1, py + ph - 1], outline=(255, 0, 255, 255))
            im.save(PREVIEW / f"reconstruction_ecran{n}{variant}.png")
            Image.blend(r, im, 0.5).resize((960, 720), Image.NEAREST).save(PREVIEW / f"overlay_ecran{n}{variant}_50.png")
            side = Image.new("RGBA", (644, 240), (255, 255, 255, 255))
            side.paste(r, (0, 0))
            side.paste(im, (324, 0))
            side.resize((1288, 480), Image.NEAREST).save(PREVIEW / f"overlay_ecran{n}{variant}_cote.png")
            # Carte des ecarts : vert = identique (5 bits), jaune = ecart <= 3 niveaux 5 bits
            # (palette de version), rouge = different, noir = non dessine.
            rp, ip = r.load(), im.load()
            diff = Image.new("RGB", (320, 240))
            for y in range(240):
                for x in range(320):
                    a, b = rp[x, y], ip[x, y]
                    d5 = max(abs((a[i] >> 3) - (b[i] >> 3)) for i in range(3))
                    if b[:3] == (0, 0, 0):
                        col = (0, 0, 0)
                    else:
                        col = (0, 200, 0) if d5 == 0 else ((230, 200, 0) if d5 <= 3 else (220, 0, 0))
                    diff.putpixel((x, y), col)
            diff.resize((960, 720), Image.NEAREST).save(PREVIEW / f"overlay_ecran{n}{variant}_diff.png")
            if not variant:
                continue
            for name, (x0, y0, w, h) in _score_zones(n).items():
                drawn = ok = 0
                for y in range(y0, y0 + h):
                    for x in range(x0, x0 + w):
                        if ip[x, y][:3] == (0, 0, 0):
                            continue
                        drawn += 1
                        ok += all((rp[x, y][i] >> 3) == (ip[x, y][i] >> 3) for i in range(3))
                print(f"  ecran {n} {name:12s} {ok * 100 / max(1, drawn):5.1f} % des {drawn:5d} pixels dessines"
                      f" identiques ({x0},{y0} {w}x{h})")


# ---------------------------------------------------------------------------
# Reconstruction de controle du HUD (hors jeu), superposee aux captures A1 de reference
# ---------------------------------------------------------------------------
HUD_PREVIEW = ROOT / "extracted" / "preview" / "hud_assets"
A1_REF = Path(os.environ.get("A1_REF", str(ROOT / "extracted" / "a1_ref")))
# Etat lu sur chaque capture (PV, max, images des cristaux, or, icones des cases), releve par
# classement exact des pieces (docs/visuel/a1_hud_layout.md, section 2).
HUD_REF_STATES = {
    "lp11_21": dict(hp=22, max_hp=22, mp=["hud.mp_full_a", "hud.mp_full_b"], gold=618,
                    weapon="sword_short", item="bomb", box_bg_black=False),
    "lp07_21": dict(hp=13, max_hp=16, mp=["hud.mp_full_a"], gold=386,
                    weapon="sword_gold_b", item="bomb", box_bg_black=True),
    "lp11_1": dict(hp=18, max_hp=21, mp=["hud.mp_empty", "hud.mp_empty"], gold=557,
                   weapon=None, item=None, box_bg_black=False),
}


def hud_gems(hp: int, max_hp: int) -> tuple[list[bool], list[bool]]:
    """Regle A1 deduite de 196 captures : 10 petites gemmes (1 PV chacune) + ceil(max/10) - 1
    grosses gemmes (une par dizaine). Dizaines completes L = min(grosses, (hp - 1) // 10) ;
    petites pleines = hp - 10 L. Renvoie (grosses de droite a gauche, petites dans l'ordre de
    remplissage : rangee haute de gauche a droite, puis rangee basse)."""
    n_big = max(0, -(-max_hp // 10) - 1)
    full_tens = min(n_big, (hp - 1) // 10) if hp > 0 else 0
    small = hp - 10 * full_tens
    return [k < full_tens for k in range(n_big)], [i < small for i in range(10)]


def draw_hud(dst: Image.Image, p: dict[str, Image.Image], hp: int, max_hp: int, mp: list[str], gold: int,
             weapon: str | None, item: str | None, boxes: bool = True) -> None:
    L = HUD_LAYOUT
    if boxes:
        for (bx, by), icon in zip(L["item_boxes"], (weapon, item)):
            dst.alpha_composite(p["hud.item_box"], (bx, by))
            if icon:
                dst.alpha_composite(p[f"icon.{icon}"], (bx, by))
    big, small = hud_gems(hp, max_hp)
    for k in reversed(range(len(big))):          # la plus a gauche d'abord : la plus a droite la recouvre
        dst.alpha_composite(p["hud.gem_big_full" if big[k] else "hud.gem_big_empty"],
                            (L["gem_big_x"] + L["gem_big_pitch"] * k, L["gem_big_y"]))
    for i, full in enumerate(small):
        dst.alpha_composite(p["hud.gem_small_full" if full else "hud.gem_small_empty"],
                            (L["gem_small_x"] + L["gem_small_pitch"] * (i % 5), L["gem_small_rows_y"][i // 5]))
    dst.alpha_composite(p["hud.slash"], L["slash"])
    for k, ch in enumerate(f"{max_hp:d}"):
        dst.alpha_composite(p[f"hud.digit{ch}"], (L["max_digits_x"] + L["digit_pitch"] * k, L["digit_y"]))
    for k, name in enumerate(mp):
        dst.alpha_composite(p[name], (L["mp_x"] + L["mp_pitch"] * k, L["mp_y"]))
    for k, ch in enumerate(f"{gold:0{L['gold_digits']}d}"):
        dst.alpha_composite(p[f"hud.digit{ch}"], (L["gold_digits_x"] + L["digit_pitch"] * k, L["digit_y"]))
    dst.alpha_composite(p["hud.gold_bag"], L["gold_bag"])


def reconstruct_hud(p: dict[str, Image.Image]) -> None:
    """Recompose le HUD de captures A1 connues et le superpose : *_50 (fondu, x4, bandeau haut),
    *_cote (reference | reconstruction), *_diff (vert = identique 5 bits, jaune <= 3 niveaux,
    rouge = different, bleu = case semi-transparente non verifiable, noir = non dessine)."""
    HUD_PREVIEW.mkdir(parents=True, exist_ok=True)
    import glob
    for cap, st in HUD_REF_STATES.items():
        files = glob.glob(str(A1_REF / f"{cap}-*.png"))
        if not files:
            print(f"  {cap} : capture absente de {A1_REF}")
            continue
        ref = Image.open(files[0]).convert("RGBA").resize((320, 256), Image.NEAREST)
        ref.save(HUD_PREVIEW / f"ref_{cap}_native.png")
        layer = Image.new("RGBA", (320, 256), (0, 0, 0, 0))
        draw_hud(layer, p, st["hp"], st["max_hp"], st["mp"], st["gold"], None, None, boxes=False)
        boxes = Image.new("RGBA", (320, 256), (0, 0, 0, 0))
        for (bx, by), icon in zip(HUD_LAYOUT["item_boxes"], (st["weapon"], st["item"])):
            if st["box_bg_black"]:
                boxes.alpha_composite(Image.new("RGBA", (24, 32), (0, 0, 0, 255)), (bx, by))
            boxes.alpha_composite(p["hud.item_box"], (bx, by))
            if icon:
                boxes.alpha_composite(p[f"icon.{icon}"], (bx, by))
        reco = Image.new("RGBA", (320, 256), (0, 0, 0, 255))
        reco.alpha_composite(boxes)
        reco.alpha_composite(layer)
        reco.save(HUD_PREVIEW / f"hud_reco_{cap}.png")
        top = (0, 0, 320, 48)
        Image.blend(ref, reco, 0.5).crop(top).resize((1280, 192), Image.NEAREST).save(HUD_PREVIEW / f"hud_overlay_{cap}_50.png")
        on_ref = ref.copy()
        on_ref.alpha_composite(layer)                 # pieces opaques posees sur la capture : doivent disparaitre
        side = Image.new("RGBA", (320, 98), (255, 0, 255, 255))
        side.paste(ref.crop(top), (0, 0))
        side.paste(reco.crop(top), (0, 50))
        side.resize((1280, 392), Image.NEAREST).save(HUD_PREVIEW / f"hud_overlay_{cap}_cote.png")
        rp, lp, bp = ref.load(), layer.load(), boxes.load()
        diff = Image.new("RGB", (320, 48))
        score: dict[str, list[int]] = {}
        zones = {"cases": (16, 72), "gemmes": (88, 160), "maximum": (160, 184), "magie": (216, 248), "or": (256, 296)}
        for y in range(48):
            for x in range(320):
                src = lp[x, y] if lp[x, y][3] else (bp[x, y] if bp[x, y][3] else None)
                if src is None:
                    diff.putpixel((x, y), (0, 0, 0))
                    continue
                if not lp[x, y][3] and bp[x, y][3] < 255 and not st["box_bg_black"]:
                    diff.putpixel((x, y), (0, 0, 200))
                    continue
                val = reco.getpixel((x, y))
                d5 = max(abs((rp[x, y][i] >> 3) - (val[i] >> 3)) for i in range(3))
                diff.putpixel((x, y), (0, 200, 0) if d5 == 0 else ((230, 200, 0) if d5 <= 3 else (220, 0, 0)))
                for zn, (x0, x1) in zones.items():
                    if x0 <= x < x1:
                        s = score.setdefault(zn, [0, 0, 0])
                        s[0] += 1
                        s[1] += d5 == 0
                        s[2] += d5 <= 3
        diff.resize((1280, 192), Image.NEAREST).save(HUD_PREVIEW / f"hud_overlay_{cap}_diff.png")
        print(f"  {cap} : " + " ; ".join(f"{zn} {s[1] * 100 / s[0]:.1f} % exacts, {s[2] * 100 / s[0]:.1f} % <= 3 niveaux"
                                         f" ({s[0]} px)" for zn, s in score.items()))
    reconstruct_title()


def reconstruct_title() -> None:
    """Nom de lieu A1 (lp05_12, « Olen's Nightmare ») : parchemin de dialogue plein ecran et texte
    FONT3 centre. Recherche de l'origine du texte par correspondance de l'encre (couleur 5 bits
    (5, 6, 2)) puis superposition."""
    import glob
    from capture_compare import render_text
    from a1_font import SCREEN, parse_tim
    files = glob.glob(str(A1_REF / "lp05_12-*.png"))
    if not files:
        return
    ref = Image.open(files[0]).convert("RGBA").resize((320, 256), Image.NEAREST)
    font, _, _ = parse_tim(SCREEN / "FONT3.TIM")
    text = render_text("Olen's Nightmare", font)
    tp, rp = text.load(), ref.load()
    ink_ref = {(x, y) for y in range(168, 224) for x in range(40, 280)
               if max(abs((rp[x, y][0] >> 3) - 5), abs((rp[x, y][1] >> 3) - 6), abs((rp[x, y][2] >> 3) - 2)) <= 1}
    ink_txt = [(x, y) for y in range(text.height) for x in range(text.width) if tp[x, y][3]]
    best = max(((sum((x + ox, y + oy) in ink_ref for x, y in ink_txt), ox, oy)
                for oy in range(166, 200) for ox in range(80, 140)))
    hit, ox, oy = best
    width = text.width - 1                                   # render_text : encre + 1 px
    print(f"  titre lp05_12 : texte FONT3 {text.width}x{text.height} meilleur placement ({ox}, {oy}),"
          f" {hit}/{len(ink_txt)} pixels d'encre communs (encre ref : {len(ink_ref)}) ;"
          f" centre de l'encre x = {ox + width / 2:.1f}")
    over = ref.copy()
    red = Image.new("RGBA", text.size, (255, 0, 0, 255))
    over.paste(red, (ox, oy), text)
    over.crop((0, 160, 320, 232)).resize((1280, 288), Image.NEAREST).save(HUD_PREVIEW / "title_lp05_12_overlay.png")
    ref.crop((0, 160, 320, 232)).resize((1280, 288), Image.NEAREST).save(HUD_PREVIEW / "title_lp05_12_ref.png")


def main() -> None:
    PREVIEW.mkdir(parents=True, exist_ok=True)
    OUT.mkdir(parents=True, exist_ok=True)
    widx, wpals = load_wind()

    # 1) WIND.TX : planche complete par banque + regions nommees.
    for bank in range(9):
        render(widx, 256, wpals[bank], (0, 0, 256, 256)).save(PREVIEW / f"wind_bank{bank}.png")
    named = []
    for name, (x, y, w, h, bank) in WIND_REGIONS.items():
        im = render(widx, 256, wpals[bank], (x, y, w, h))
        im.save(OUT / f"wind_{name}.png")
        named.append((name, im))
        print(f"WIND.TX {name:16s} x={x:3d} y={y:3d} {w:3d}x{h:3d} banque {bank} -> assets/ui/a1_menu/wind_{name}.png")
    contact(named, 2, 3).save(PREVIEW / "wind_regions_x3.png")
    # Panneau pierre dans les 9 banques (theme de couleur).
    contact([(f"b{b}", render(widx, 256, wpals[b], WIND_REGIONS["stone_panel"][:4])) for b in range(9)], 9, 2) \
        .save(PREVIEW / "stone_panel_banks_x2.png")

    # 2) DATAS.BIN : icones (page 7) et portrait (page 2).
    b0, sidx, spals = load_datas()
    recs = icon_records(b0)
    icons = []
    for u, y, w, h, pal in recs:
        im = render(sidx, 256, spals[pal], (u, y, w, h))
        name = f"icon_u{u:03d}_y{y:04d}_p{pal}"
        im.save(OUT / f"{name}.png")
        icons.append((name, im))
    print(f"DATAS.BIN bloc 2 : {len(icons)} icones (u, y, l, h, palette) -> assets/ui/a1_menu/icon_*.png")
    contact(icons, 12, 3).save(PREVIEW / "icons_x3.png")
    # Page d'icones entiere avec chaque palette d'objets 30..36 (inspection).
    contact([(f"p{p}", render(sidx, 256, spals[p], (0, 1792, 256, 256))) for p in range(30, 37)], 4, 1) \
        .save(PREVIEW / "icon_page_pals30_36.png")
    x, y, w, h, pal = PORTRAIT
    por = render(sidx, 256, spals[pal], (x, y, w, h))
    por.save(OUT / "portrait_alundra.png")
    por.resize((w * 4, h * 4), Image.NEAREST).save(PREVIEW / "portrait_alundra_x4.png")
    print(f"DATAS.BIN bloc 2 portrait x={x} y={y} {w}x{h} palette {pal} -> assets/ui/a1_menu/portrait_alundra.png")

    with open(OUT / "manifest.txt", "w", encoding="utf-8") as f:
        f.write("# genere par tools/a1_menu_assets.py ; source = ressources A1 locales, ne pas publier\n")
        for name, (x, y, w, h, bank) in WIND_REGIONS.items():
            f.write(f"wind_{name}.png WIND.TX {x} {y} {w} {h} clut_bank={bank}\n")
        for u, y, w, h, pal in recs:
            f.write(f"icon_u{u:03d}_y{y:04d}_p{pal}.png DATAS.BIN/blk2 {u} {y} {w} {h} blk0_pal={pal}\n")
        f.write(f"portrait_alundra.png DATAS.BIN/blk2 {x} {y} {w} {h} blk0_pal={pal}\n")

    named = build_banks(widx, wpals, sidx, spals, recs)
    if "--check" in sys.argv:
        check_banks(wpals)
    if "--reconstruct" in sys.argv:
        reconstruct(named)
    if "--hud" in sys.argv:
        reconstruct_hud(named)


if __name__ == "__main__":
    main()
