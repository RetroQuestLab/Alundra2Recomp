"""Planche visuelle des armes du remake, composee a partir de sprites ALUNDRA 1 extraits.

Source : extracted/a1/DATA/DATAS.BIN (jeu original, jamais publie).
 - Lames/baton/fleau/arc : frames des groupes 16 a 19 du heros (palette 18 des armes).
 - Auras, etincelles, boucliers et anneaux : effets de la banque globale (0, 14, 16, 25, 28).
Les couleurs sont remplacees pour creer les designs "façon Alundra 2" (epees mithril, runes,
gantelet d'eclair, boucliers, bagues elementaires). Aucune image A2 n'est recopiee : les NOMS
viennent de la traduction A2 (extracted/a2_text), les FORMES viennent d'Alundra 1.

Usage : py -3.12 tools/make_arsenal_sheet.py
Sortie : docs/arsenal_alundra2.png
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from a1_sprites import SpriteBank, effect_steps  # noqa: E402
from PIL import Image, ImageDraw, ImageFont, ImageChops  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'extracted', 'a1', 'DATA', 'DATAS.BIN')
OUT = os.path.join(ROOT, 'docs', 'arsenal_alundra2.png')

# ---------------------------------------------------------------------------
# Palettes A1 (16 couleurs) : 0 transparent, 1-3 contour, 4-7 poignee,
# 8-10 corps de la lame, 11-15 reflets / lueur.
# ---------------------------------------------------------------------------
def mix(a, b, t):
    return tuple(round(a[i] + (b[i] - a[i]) * t) for i in range(3))


def ramp(outline, grip, blade, shine):
    dark, mid, light = outline, blade, shine
    colors = [(0, 0, 0, 0)]
    colors += [mix(dark, grip, i / 3) + (255,) for i in range(3)]          # 1-3
    colors += [mix(grip, blade, i / 3) + (255,) for i in range(4)]          # 4-7
    colors += [mix(blade, light, i / 5) + (255,) for i in range(3)]         # 8-10
    colors += [mix(mid, light, (i + 1) / 5) + (255,) for i in range(5)]     # 11-15
    return [tuple(c) for c in colors]


STEEL = ramp((25, 28, 36), (92, 68, 42), (145, 162, 180), (238, 246, 255))
MITHRIL = ramp((12, 38, 55), (90, 130, 145), (110, 205, 225), (240, 255, 255))
BRONZE = ramp((42, 26, 16), (120, 72, 32), (185, 145, 78), (255, 228, 165))
RUNES = ramp((28, 12, 48), (85, 45, 115), (150, 95, 215), (245, 205, 255))
BRIGHT = ramp((55, 42, 10), (175, 145, 45), (235, 215, 110), (255, 255, 220))
SUPREME = ramp((14, 34, 70), (75, 120, 195), (175, 220, 250), (255, 255, 255))
ECLAIR = ramp((8, 24, 62), (26, 72, 150), (55, 150, 245), (225, 245, 255))
STONE = ramp((34, 25, 20), (105, 65, 35), (125, 128, 134), (205, 205, 212))
DART = ramp((28, 34, 45), (75, 88, 108), (165, 178, 195), (242, 248, 255))
SHIELD_BRONZE = ramp((40, 25, 15), (125, 80, 38), (180, 135, 70), (245, 215, 150))
SHIELD_IRON = ramp((22, 26, 34), (70, 78, 92), (125, 140, 155), (225, 235, 245))
SHIELD_TITAN = ramp((16, 18, 24), (48, 52, 60), (80, 88, 100), (170, 180, 195))
SHIELD_MITHRIL = ramp((10, 35, 50), (80, 125, 140), (115, 205, 225), (245, 255, 255))
SHIELD_GOLD = ramp((48, 34, 8), (165, 130, 35), (230, 195, 80), (255, 250, 205))
SHIELD_VALAR = ramp((35, 15, 55), (95, 55, 135), (175, 130, 230), (250, 225, 255))
RING_PIXIE = ramp((12, 40, 22), (50, 130, 65), (110, 220, 120), (235, 255, 220))
RING_TRITON = ramp((10, 30, 65), (35, 90, 175), (80, 165, 245), (220, 240, 255))
RING_DRYAD = ramp((45, 25, 10), (135, 75, 25), (210, 135, 55), (255, 225, 160))
RING_SIRENE = ramp((8, 45, 48), (35, 135, 140), (95, 220, 220), (225, 255, 250))

# ---------------------------------------------------------------------------
bank = SpriteBank(open(SRC, 'rb').read())
anims_off, steps_off, _, frames_off = bank.character(0)


def weapon_frame(group, index=0, direction=0):
    dirs = __import__('struct').unpack_from('<4H', bank.b, anims_off + group * 14)
    return bank.anim_steps(steps_off, dirs[direction])[index][0]


def render_mask(render, palette_index, colors):
    """Rend une image A1 et son masque en neutralisant sa palette (1x1 au lieu de 0 : evite le melange additif)."""
    saved = [tuple(c) for c in bank.pals[palette_index]]
    bank.pals[palette_index] = colors
    img = render()[0]
    bank.pals[palette_index] = [(1, 1, 1, 1) for _ in saved]
    base = render()[0]
    bank.pals[palette_index] = saved
    mask = ImageChops.difference(img.convert('RGBA'), base.convert('RGBA')).getchannel('A')
    if not mask.getbbox():
        return None
    out = img.copy()
    out.putalpha(mask)
    return out.crop(mask.getbbox())


def weapon(group, colors, index=0, direction=0):
    foff = weapon_frame(group, index, direction)
    return render_mask(lambda: bank.render_frame(frames_off, foff), 18, colors)


def effect(k, colors, step=0, pal=None):
    base, steps = effect_steps(bank, k)
    foff = steps[step][0]
    if pal is None:
        parts = bank.frame_parts(base, foff)
        pal = parts[0][1]
    return render_mask(lambda: bank.render_frame(base, foff), pal, colors)


def fit(img, size):
    if img is None:
        return None
    w, h = img.size
    s = min(size / w, size / h)
    return img.resize((max(1, round(w * s)), max(1, round(h * s))), Image.Resampling.NEAREST)


def center(dst, src, cx, cy):
    if src is None:
        return
    dst.alpha_composite(src, (cx - src.width // 2, cy - src.height // 2))


def icon(cell, kind, colors):
    cx, cy = cell.width // 2, 74
    if kind == 'sword':
        center(cell, fit(weapon(16, colors, 1), 138), cx, cy)
    elif kind == 'supreme':
        center(cell, fit(effect(28, SUPREME, 1), 150), cx, cy - 8)
        center(cell, fit(weapon(16, colors, 1), 126), cx, cy)
    elif kind == 'gantlet':
        center(cell, fit(effect(25, ECLAIR, 2), 150), cx, cy)
        center(cell, fit(weapon(16, colors, 1), 128), cx, cy)
    elif kind == 'hammer':
        blade = weapon(16, colors, 0)
        if blade:
            w, h = blade.size
            # Tete de marteau : on elargit le haut de la lame A1, le manche reste d'origine.
            px = blade.load()
            head = blade.copy()
            hp = head.load()
            for y in range(h):
                for x in range(w):
                    if y > h * 0.55 or px[x, y][3] == 0:
                        continue
                    for dx in (-2, -1, 1, 2):
                        nx = x + dx
                        if 0 <= nx < w:
                            hp[nx, y] = px[x, y]
            center(cell, fit(head, 138), cx, cy)
    elif kind == 'darts':
        center(cell, fit(effect(14, DART, 3), 125), cx - 12, cy - 18)
        center(cell, fit(weapon(19, colors, 4), 115), cx + 8, cy + 12)
    elif kind == 'shield':
        base = fit(effect(0, colors, 0), 138)
        if base:
            # Le disque d'ombre A1 devient un bouclier : on le renforce d'un boss et d'une bande.
            d = ImageDraw.Draw(base)
            w, h = base.size
            d.rectangle((w // 2 - 2, 4, w // 2 + 2, h - 4), fill=colors[12])
            d.rectangle((6, h // 2 - 2, w - 6, h // 2 + 2), fill=colors[12])
            center(cell, base, cx, cy)
        center(cell, fit(effect(16, colors, 2), 42), cx, cy)
    elif kind == 'ring':
        center(cell, fit(effect(0, colors, 0), 112), cx, cy + 12)
        center(cell, fit(effect(16, colors, 3), 58), cx, cy - 10)


# ---------------------------------------------------------------------------
FONT_PATH = r'C:\Windows\Fonts\segoeui.ttf'


def font(size, bold=False):
    name = 'seguisb.ttf' if bold else FONT_PATH.rsplit('\\', 1)[-1]
    try:
        return ImageFont.truetype(os.path.join(r'C:\Windows\Fonts', name), size)
    except OSError:
        return ImageFont.load_default()


W, H = 1280, 1120
img = Image.new('RGBA', (W, H), (14, 18, 30, 255))
d = ImageDraw.Draw(img)
d.text((40, 26), "Arsenal d'Alundra 2", font=font(40, True), fill=(240, 232, 205))
d.text((42, 76), "Noms A2 · formes recomposees depuis les sprites extraits d'Alundra 1", font=font(19), fill=(150, 175, 205))

ITEMS = [
    ('Épée longue', 'sword', STEEL), ('Épée Mithril', 'sword', MITHRIL),
    ('Épée large', 'sword', BRONZE), ('Épée des runes', 'sword', RUNES),
    ('Épée brillante', 'sword', BRIGHT), ('Épée suprême', 'supreme', SUPREME),
    ('Gantelet d’éclair', 'gantlet', ECLAIR), ('Marteau normal', 'hammer', STONE),
    ('Fléchettes', 'darts', DART),
    ('Bouclier Mini', 'shield', SHIELD_BRONZE), ('Bouclier Moyen', 'shield', SHIELD_IRON),
    ('Bouclier Titan', 'shield', SHIELD_TITAN), ('Bouclier Mithril', 'shield', SHIELD_MITHRIL),
    ('Bouclier Suprême', 'shield', SHIELD_GOLD), ('Bouclier Valar', 'shield', SHIELD_VALAR),
    ('Bague Pixie', 'ring', RING_PIXIE), ('Bague de Triton', 'ring', RING_TRITON),
    ('Bague de Dryade', 'ring', RING_DRYAD), ('Bague de la Sirène', 'ring', RING_SIRENE),
]
ROWS = [
    ('ÉPÉES', 6, 112), ('AUTRES ARMES', 3, 352),
    ('BOUCLIERS', 6, 592), ('ANNEAUX ÉLÉMENTAIRES', 4, 832),
]

cell_w, cell_h, gap = 196, 216, 12
start = 0
for title, count, y in ROWS:
    d.text((42, y - 28), title, font=font(20, True), fill=(220, 185, 110))
    row = Image.new('RGBA', (count * (cell_w + gap) - gap, cell_h), (0, 0, 0, 0))
    rd = ImageDraw.Draw(row)
    for i in range(count):
        name, kind, colors = ITEMS[start + i]
        x = i * (cell_w + gap)
        rd.rounded_rectangle((x, 0, x + cell_w, cell_h - 6), radius=10,
                             fill=(24, 31, 48, 255), outline=(62, 78, 108, 255), width=2)
        cell = Image.new('RGBA', (cell_w, cell_h), (0, 0, 0, 0))
        icon(cell, kind, colors)
        row.alpha_composite(cell, (x, 0))
        label = name
        f = font(17, True)
        while rd.textlength(label, font=f) > cell_w - 16 and len(label) > 8:
            label = label[:-2] + '…'
        rd.text((x + cell_w // 2, cell_h - 42), label, font=f, fill=(238, 238, 225), anchor='mm')
        rd.text((x + cell_w // 2, cell_h - 20), 'A1 → A2', font=font(12), fill=(120, 145, 175), anchor='mm')
    img.alpha_composite(row, (42, y))
    start += count

d.text((42, H - 48),
       "Planche de travail : creations originales a partir des ressources A1 extraites. Aucune image A2 n'est incluse.",
       font=font(16), fill=(125, 140, 165))

os.makedirs(os.path.dirname(OUT), exist_ok=True)
img.convert('RGB').save(OUT, optimize=True)
print(f'{OUT} : {len(ITEMS)} armes/objets, source {SRC}')
