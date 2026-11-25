"""Convertit les ressources Alundra 1 (A1MP/A1SP) en blobs PS1 4bpp.

Usage :
    py -3.12 tools/ps1_export_zone.py <zone.a1m> <hero.a1s> <dossier_sortie>

Les blocs P1ZM/P1SP contiennent deja les coordonnees VRAM choisies pour le decor
et les CLUT. Les pixels du heros restent en RAM : seule la frame courante est
televersee dans le scratch VRAM fixe (voir HERO_SCRATCH), ce qui economise
~130 Ko de VRAM. Aucune allocation dynamique cote console.

IMPORTANT : toutes les coordonnees VRAM sont en DEMI-MOTS 16 bits (x 0..1023,
y 0..511), comme l'attend LoadImage/getTPage. Une tuile 4bpp de 24 pixels
occupe donc 24/4 = 6 demi-mots de large. Les pages de texture font 64
demi-mots de large (256 pixels 4bpp) et 256 lignes de haut.
"""
import math
import os
import struct
import sys

# Doit rester synchronise avec src/platform/pf.h (test de non-regression).
VRAM_W, VRAM_H = 1024, 512
VIEW_W, VIEW_H = 320, 240
TILE_W, TILE_H = 24, 16
MARGIN_ROWS = 16
ATTR_BLOCK = 0x11
# Demi-mots VRAM. 512 est un multiple de 64 (debut de page valide) et y=256 est
# SOUS les framebuffers : y<240 dans x=320..639 serait ecrase par le DRAWENV.
HERO_SCRATCH = (512, 256, 128, 64)
# CLUT du heros, independantes de celles du decor : hero.dat doit rester le
# meme quelle que soit la zone active (64CLUT observees dans hero.a1s).
HERO_CLUTS = (512, 320, 16, 64)
# Atlas de police A1 : 256x256 pixels 4bpp = 64x256 demi-mots (4 pixels par
# demi-mot), sous les framebuffers. Ses 6 CLUT sont rangees a x=128 ; la 5e
# (y=260) porte l'orange du "- remake -" du titre depuis v0.8.15, la 6e
# (y=261) le bleu pale des lignes normales d'options depuis v0.8.16.
FONT_ATLAS = (0, 256, 64, 256)
FONT_CLUTS = (128, 256, 16, 9)
# Atlas WIND.TX de la fenetre de dialogue : meme format, 64x256 demi-mots.
# Ses 16 banques CLUT occupent 16 lignes a x=320, y=256.
WIND_ATLAS = (256, 256, 64, 256)
WIND_CLUTS = (320, 256, 16, 16)
# Deux sprites PNJ A1 (25x47 px -> 16 demi-mots x 48 lignes avec padding) et
# leurs CLUT. Synchronise avec PF_NPC*_X/Y de src/platform/pf.h.
NPC0_TEX = (640, 256, 16, 48)
NPC1_TEX = (656, 256, 16, 48)
NPC2_TEX = (672, 256, 16, 48)
# v0.8.37 : Paco compte 5 PNJ sur PC ; deux emplacements de plus, en VRAM
# libre (entre NPC2 et les acteurs ; apres l'atlas d'icones).
NPC3_TEX = (688, 256, 16, 48)
NPC4_TEX = (968, 256, 16, 48)
NPC_CLUTS = (640, 304, 16, 5)
# Acteurs du jalon boss (ogre 40x49, coffres 25x33) et du trone (Mephisto
# 40x49, Alexia 24x43, Roi 25x47) : plus larges qu'un slot PNJ.
# Six textures 32 demi-mots x 64 lignes, CLUT sur six lignes consecutives.
ACTOR0_TEX = (704, 256, 32, 64)
ACTOR1_TEX = (736, 256, 32, 64)
ACTOR2_TEX = (768, 256, 32, 64)
ACTOR3_TEX = (800, 256, 32, 64)
ACTOR4_TEX = (832, 256, 32, 64)
ACTOR5_TEX = (864, 256, 32, 64)
ACTOR_CLUTS = (704, 320, 3, 6)
# HUD A1 (a1_hud.a1s 1/4/5/8 gemmes, 9..18 chiffres, 19 slash) : 15 frames
# 4bpp dans une page. Les CLUT sont passees en fin de page pour laisser la
# place aux 11 glyphes 8x16 du suffixe "/MM" present sur la cible PC.
HUD_ATLAS = (64, 256, 16, 224)
HUD_CLUTS = (64, 480, 16, 16)
# Scratch de l'animation de l'ogre (v0.8.8) : six frames A1 en RAM dans
# enemy_anim.dat, une seule frame televersee ici a la fois. Synchronise avec
# PF_ENEMY_* de src/platform/pf.h et ENEMY_* de ps1_export_actor.py.
ENEMY_TEX = (80, 256, 32, 64)
# HUD complementaire (case d'objet, cristaux d'EP, bourse) : colonne libre entre
# l'ennemi et les CLUT de police. Synchronise avec PF_HUD2_* de pf.h.
HUD2_ATLAS = (112, 256, 16, 128)
HUD2_CLUTS = (112, 384, 16, 8)
# Buste du locuteur (48x56, un seul a la fois) et sa CLUT. Synchronise avec
# PF_PORTRAIT_* de pf.h.
PORTRAIT_TEX = (144, 256, 24, 64)
PORTRAIT_CLUT = (144, 320, 16, 1)
ENEMY_CLUT = (80, 320, 16, 1)
# Atlas d'icones d'inventaire A1 : grille 12x7 de cellules 6x32 demi-mots.
ICON_ATLAS = (896, 256, 72, 224)
ICON_CLUT = (896, 480, 16, 1)
RESERVED_RECTS = ((0, 0, 320, 240), (320, 0, 320, 240), HERO_SCRATCH,
                  HUD2_ATLAS, HUD2_CLUTS, PORTRAIT_TEX, PORTRAIT_CLUT,
                  HERO_CLUTS, FONT_ATLAS, FONT_CLUTS, WIND_ATLAS, WIND_CLUTS,
                  NPC0_TEX, NPC1_TEX, NPC2_TEX, NPC3_TEX, NPC4_TEX, NPC_CLUTS,
                  ACTOR0_TEX, ACTOR1_TEX, ACTOR2_TEX,
                  ACTOR3_TEX, ACTOR4_TEX, ACTOR5_TEX, ACTOR_CLUTS,
                  HUD_ATLAS, HUD_CLUTS, ENEMY_TEX, ENEMY_CLUT, ICON_ATLAS, ICON_CLUT)
PAGE_W = 64  # demi-mots par page de texture (256 pixels en 4bpp)
# Ordre impose par hero_ps1.cpp : 0=repos, 1=marche, 2=course, 3=saut, 4=epee.
# Les indices A1SP viennent de src/sprite.h::SpriteBank :
#   IDLE=0, WALK=1, ATTACK=2, RUN=3, HURT=4, KO=5, JUMP=6, ITEM=7, CHARGE=8.
# L'ancien choix (0,1,3,2,16) utilisait ATTACK comme saut et le groupe 16
# (vide dans hero.a1s) comme attaque : le heros disparaissait sur Carre.
HERO_SUBSET = (0, 1, 3, 6, 2)  # repos, marche, course, saut, epee
# Zones dont le blob P1ZM est embarque dans l'EXE (salle du trone incluse).
PS1_ZONE_IDS = ('011', '013', '019', '119', '120')


def to_bgr555(r, g, b):
    return ((r * 31 // 255) & 0x1F) | (((g * 31 // 255) & 0x1F) << 5) | (((b * 31 // 255) & 0x1F) << 10)


def make_clut(pixels):
    """pixels : liste [(r,g,b,a)]. Entree 0 = transparent, entrees 1..15 = couleurs."""
    clut = [0] * 16
    for i, (r, g, b, _a) in enumerate(pixels[:15], 1):
        clut[i] = to_bgr555(r, g, b) & 0x7FFF
    return clut


def dedupe_cluts(cluts):
    """Renvoie (cluts_uniques, indices) ; l'ordre de premiere apparition est conserve."""
    unique, indices = [], []
    for clut in cluts:
        key = tuple(clut)
        if key not in unique:
            unique.append(key)
        indices.append(unique.index(key))
    return [list(u) for u in unique], indices


def packed_width(w):
    """Largeur 4bpp alignee sur la VRAM : multiple de 4 pixels (2 octets par paire)."""
    return (w + 3) // 4 * 4


def quantize(rgba, w, h):
    """Quantifie une image RGBA en 4bpp + CLUT 16 couleurs.

    alpha < 128 -> index 0 (transparent GPU). Les couleurs opaques distinctes
    sont placees aux index 1..15 ; au-dela, on replie sur la plus proche.
    """
    pw = packed_width(w)
    colors, freq = [], {}
    for i in range(w * h):
        if rgba[i * 4 + 3] < 128:
            continue
        c = (int(rgba[i * 4]), int(rgba[i * 4 + 1]), int(rgba[i * 4 + 2]))
        if c not in colors:
            colors.append(c)
        freq[c] = freq.get(c, 0) + 1
    if len(colors) > 15:
        colors = sorted(colors, key=lambda c: (-freq[c], c))[:15]
    clut = make_clut([(c[0], c[1], c[2], 255) for c in colors])
    out = bytearray(pw * h // 2)
    for y in range(h):
        for x in range(pw):
            idx = 0
            if x < w and rgba[(y * w + x) * 4 + 3] >= 128:
                c = tuple(rgba[(y * w + x) * 4:(y * w + x) * 4 + 3])
                if c in colors:
                    idx = colors.index(c) + 1
                else:
                    idx = 1 + min(range(len(colors)),
                                  key=lambda k: sum((c[j] - colors[k][j]) ** 2 for j in range(3)))
            o = y * (pw // 2) + x // 2
            if (x & 1) == 0:
                out[o] = (out[o] & 0xF0) | idx
            else:
                out[o] = (out[o] & 0x0F) | (idx << 4)
    return bytes(out), clut


def pad_rows(packed, w, h):
    """Aligne le transfert LoadImage sur 16 mots de 32 bits.

    PCSX-Redux recale les transferts dont la longueur (en mots 32 bits) n'est
    pas un multiple de 16 ; les mots restants etaient alors interpretes comme
    des commandes GPU et desynchronisaient la liste d'affichage. On aligne la
    largeur sur 8 demi-mots et on ajoute des lignes transparentes (index 0) si
    necessaire. Le header P1SP stocke stride (demi-mots/ligne) et upload_h
    (lignes reellement televersees) ; le sprite garde sa hauteur d'origine.
    """
    stride = (packed_width(w) // 2 + 7) // 8 * 8
    src = packed_width(w) // 2
    step = 32 // math.gcd(stride, 32)
    upload_h = ((h + step - 1) // step) * step
    out = bytearray(stride * 2 * upload_h)
    for y in range(h):
        o = y * stride * 2
        out[o:o + src] = packed[y * src:(y + 1) * src]
    return bytes(out), stride, upload_h


def _free_rects(reserved):
    """Complement des rectangles reserves, decoupe en bandes horizontales."""
    ys = sorted({0, VRAM_H} | {r[1] for r in reserved} | {r[1] + r[3] for r in reserved})
    rects = []
    for y0, y1 in zip(ys, ys[1:]):
        if y0 >= y1:
            continue
        cuts = sorted((max(r[0], 0), min(r[0] + r[2], VRAM_W))
                      for r in reserved if r[1] < y1 and r[1] + r[3] > y0)
        x = 0
        for a, b in cuts:
            if a > x:
                rects.append((x, y0, a - x, y1 - y0))
            x = max(x, b)
        if x < VRAM_W:
            rects.append((x, y0, VRAM_W - x, y1 - y0))
    return rects


def hw4(pixels):
    """Largeur en demi-mots VRAM d'une image 4bpp (4 pixels par demi-mot)."""
    return (pixels + 3) // 4


def place_vram(blobs, reserved):
    """Place des blobs (largeur_en_demi_mots, hauteur) dans la VRAM libre.

    Une CLUT (16x1) est alignee sur 16 demi-mots. Une texture 4bpp ne doit
    franchir ni une frontiere de page X (64 demi-mots) ni une frontiere de
    page Y (256 lignes) : getTPage arrondit l'origine au debut de page.
    """
    rects = _free_rects(reserved)
    state = [[r[0], r[1], 0] for r in rects]
    placed = []
    for w, h in blobs:
        is_clut = (w == 16 and h == 1)
        done = False
        for ri, (rx, ry, rw, rh) in enumerate(rects):
            cx, cy, sh = state[ri]
            for _attempt in range(12):
                if is_clut:
                    cx = (cx + 15) // 16 * 16
                    if cx + w > rx + rw:
                        cx, cy, sh = rx, cy + sh, 0
                        continue
                else:
                    page_x = (cx // PAGE_W) * PAGE_W
                    if cx + w > min(rx + rw, page_x + PAGE_W):
                        nxt = page_x + PAGE_W
                        if nxt + w <= rx + rw:
                            cx = nxt  # page suivante, meme rangee
                            continue
                        cx, cy, sh = rx, cy + sh, 0  # rangee suivante
                        continue
                    if (cy & 0xff) + h > 256:
                        cy = (cy // 256 + 1) * 256
                        sh = 0
                        continue
                if cy + h > ry + rh:
                    break
                placed.append((cx, cy, w, h))
                state[ri] = [cx + w, cy, max(sh, h)]
                done = True
                break
            if done:
                break
        if not done:
            raise ValueError(f"VRAM pleine pour un blob {w}x{h}")
    return placed


def parse_a1m(path):
    b = open(path, "rb").read()
    assert b[:4] == b"A1MP"
    w, h, ntiles = struct.unpack_from("<HHH", b, 4)
    tiles_off = 10
    cells_off = tiles_off + ntiles * TILE_W * TILE_H * 4
    cells = b[cells_off:cells_off + w * h * 8]
    nwalls = struct.unpack_from("<I", b, cells_off + len(cells))[0]
    walls_off = cells_off + len(cells) + 4
    walls = list(struct.unpack_from(f"<{nwalls}H", b, walls_off))
    tiles = [b[tiles_off + i * TILE_W * TILE_H * 4:
               tiles_off + (i + 1) * TILE_W * TILE_H * 4] for i in range(ntiles)]
    return {"w": w, "h": h, "nTiles": ntiles, "tiles": tiles,
            "cells": cells, "nWalls": nwalls, "walls": walls}


def parse_a1s(path):
    b = open(path, "rb").read()
    assert b[:4] == b"A1SP"
    nframes, ngroups = struct.unpack_from("<HH", b, 4)
    o = 6
    frames = []
    for _ in range(nframes):
        w, h, ox, oy = struct.unpack_from("<HHhh", b, o)
        o += 8
        frames.append({"w": w, "h": h, "ox": ox, "oy": oy, "rgba": b[o:o + w * h * 4]})
        o += w * h * 4
    anims = []
    for _ in range(ngroups):
        dirs = []
        for _d in range(4):
            steps = []
            if o + 2 <= len(b):
                count = struct.unpack_from("<H", b, o)[0]
                o += 2
                count &= 0x7FFF
                for _k in range(count):
                    if o + 4 > len(b):
                        break
                    f, ticks = struct.unpack_from("<HH", b, o)
                    o += 4
                    steps.append((f, ticks))
            dirs.append(steps)
        anims.append(dirs)
    return {"nFrames": nframes, "nGroups": ngroups, "frames": frames, "anims": anims}


def subset_hero(bank):
    """Garde les groupes HERO_SUBSET et reindexe les frames referencees."""
    needed = set()
    for g in HERO_SUBSET:
        if g < bank["nGroups"]:
            for dir_steps in bank["anims"][g]:
                for f, _t in dir_steps:
                    needed.add(f)
    order = sorted(needed)
    old_to_new = {f: i for i, f in enumerate(order)}
    frames = [bank["frames"][f] for f in order]
    anims = []
    for g in HERO_SUBSET:
        dirs = []
        for dir_steps in bank["anims"][g] if g < bank["nGroups"] else [[], [], [], []]:
            dirs.append([(old_to_new[f], t) for f, t in dir_steps if f in old_to_new])
        anims.append(dirs)
    return {"nFrames": len(frames), "nGroups": len(HERO_SUBSET),
            "frames": frames, "anims": anims}


def can_stand_ps1(cells, w, h, x, y, z, hw, hd):
    """Port exact de walkable()/canStand() de src/world.cpp."""
    def walkable(px, py):
        cx, cy = int(math.floor(px / TILE_W)), int(math.floor(py / TILE_H))
        if cx < 0 or cy < 0 or cx >= w or cy >= h:
            return False
        o = (cy * w + cx) * 8
        top = struct.unpack_from("<H", cells, o + 4)[0]
        attr, height = cells[o + 3], cells[o]
        return top != 0xFFFF and not (attr & ATTR_BLOCK) and abs(height - z) <= 1
    return (walkable(x - hw, y - hd) and walkable(x + hw, y - hd)
            and walkable(x - hw, y) and walkable(x + hw, y))


def camera_for(hx, hy):
    cx = max(0, min(hx - VIEW_W // 2, 52 * TILE_W - VIEW_W))
    cy = max(0, min(hy - VIEW_H // 2, (60 + MARGIN_ROWS) * TILE_H - VIEW_H))
    return cx, cy


def export(zone, hero, out_dir, zone_name='zone011'):
    os.makedirs(out_dir, exist_ok=True)
    tile_data = [quantize(t, TILE_W, TILE_H) for t in zone["tiles"]]
    hero_raw = [quantize(f["rgba"], f["w"], f["h"]) for f in hero["frames"]]
    hero_data = [pad_rows(p, f["w"], f["h"])
                 for (p, _c), f in zip(hero_raw, hero["frames"])]
    tile_unique, tile_indices = dedupe_cluts([c for _p, c in tile_data])
    hero_unique, hero_indices = dedupe_cluts([c for _p, c in hero_raw])
    if len(hero_unique) > HERO_CLUTS[3]:
        raise ValueError(f'trop de CLUT heros : {len(hero_unique)} > {HERO_CLUTS[3]}')
    blobs = [(hw4(TILE_W), TILE_H)] * len(tile_data) + [(16, 1)] * len(tile_unique)
    placed = place_vram(blobs, RESERVED_RECTS)
    tile_rects, clut_rects = placed[:len(tile_data)], placed[len(tile_data):]
    hero_clut_rects = [(HERO_CLUTS[0], HERO_CLUTS[1] + i)
                       for i in range(len(hero_unique))]

    # --- P1ZM ---
    head = b"P1ZM" + struct.pack("<HHHHH", zone["w"], zone["h"], zone["nTiles"], len(tile_unique), 0)
    head += struct.pack(f"<{len(tile_data)}H", *tile_indices)
    for r in tile_rects:
        head += struct.pack("<HHHH", *r)
    for r in clut_rects:
        head += struct.pack("<HH", r[0], r[1])
    for clut in tile_unique:
        head += struct.pack("<16H", *clut)
    zdata = head + b"".join(p for p, _c in tile_data) + zone["cells"]
    zdata += struct.pack("<I", zone["nWalls"]) + struct.pack(f"<{zone['nWalls']}H", *zone["walls"])
    zone_file = f"{zone_name}.dat"
    open(os.path.join(out_dir, zone_file), "wb").write(zdata)
    print(f"{zone_file} : {zone['nTiles']} tuiles, {len(tile_unique)} CLUT, {len(zdata)} octets")

    # --- P1SP : pixels en RAM, CLUT en VRAM dedicate (independante de la zone) ---
    hdata = b"P1SP" + struct.pack("<HHHH", hero["nFrames"], hero["nGroups"], len(hero_unique), 0)
    hdata += struct.pack(f"<{len(hero_data)}H", *hero_indices)
    for r in hero_clut_rects:
        hdata += struct.pack("<HH", r[0], r[1])
    for clut in hero_unique:
        hdata += struct.pack("<16H", *clut)
    for (packed, stride, upload_h), f in zip(hero_data, hero["frames"]):
        hdata += struct.pack("<HHhhHH", f["w"], f["h"], f["ox"], f["oy"], stride, upload_h) + packed
    for dirs in hero["anims"]:
        for steps in dirs:
            hdata += struct.pack("<H", len(steps) & 0x7FFF)
            for fr, ticks in steps:
                hdata += struct.pack("<HH", fr, ticks & 0x7FFF)
    open(os.path.join(out_dir, "hero.dat"), "wb").write(hdata)
    print(f"hero.dat : {hero['nFrames']} frames, {len(hero_unique)} CLUT independantes, {len(hdata)} octets")


def main():
    if len(sys.argv) not in (4, 5):
        print(__doc__)
        return 2
    stem = os.path.splitext(os.path.basename(sys.argv[1]))[0]
    zone_name = f'zone{stem}' if stem.isdigit() else stem
    # 5e argument : zone du remake quand la carte A1 differe
    # (data/world.txt : `map 042 028` -> zone042 depuis 028.a1m).
    if len(sys.argv) == 5:
        zone_name = f'zone{sys.argv[4]}'
    export(parse_a1m(sys.argv[1]), subset_hero(parse_a1s(sys.argv[2])),
           sys.argv[3], zone_name)
    return 0


if __name__ == "__main__":
    sys.exit(main())
