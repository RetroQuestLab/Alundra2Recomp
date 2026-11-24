"""Tests du convertisseur PS1 A1MP/A1SP -> P1ZM/P1SP.

Usage : py -3.12 tools/ps1_export_zone_test.py
Aucune dependance autre que PIL deja presente dans le projet.
"""
import os
import struct
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import ps1_export_zone as px  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def _rgba(w, h, pixels):
    """pixels : dict (x,y) -> (r,g,b,a) ; le reste est transparent."""
    out = bytearray(w * h * 4)
    for (x, y), c in pixels.items():
        o = (y * w + x) * 4
        out[o:o + 4] = bytes(c)
    return bytes(out)


def test_transparent_is_index_zero():
    img = _rgba(24, 16, {(0, 0): (0, 0, 0, 0), (1, 0): (248, 8, 8, 255)})
    packed, clut = px.quantize(img, 24, 16)
    assert len(packed) == 24 * 16 // 2
    assert (packed[0] & 0x0F) == 0, "le texel transparent doit etre l'index 0"
    assert clut[0] == 0x0000, "l'entree CLUT 0 doit etre noire/transparente"
    assert (packed[0] >> 4) == 1, "la premiere couleur opaque doit prendre l'index 1"
    print("test_transparent_is_index_zero: PASS")


def test_cluts_roundtrip():
    palettes = [
        [(255, 0, 0, 255), (0, 255, 0, 255)],
        [(255, 0, 0, 255), (0, 255, 0, 255)],
        [(0, 0, 255, 255), (255, 255, 0, 255)],
    ]
    cluts = [px.make_clut(p) for p in palettes]
    unique, indices = px.dedupe_cluts(cluts)
    assert len(unique) == 2, "deux palettes identiques doivent etre fusionnees"
    assert indices == [0, 0, 1]
    # Verification aller-retour : chaque couleur non transparente est retrouvee.
    for pal, clut in zip(palettes, cluts):
        for r, g, b, a in pal:
            assert a == 255
            found = any(
                abs(((c & 0x1F) * 255 // 31) - r) <= 8
                and abs((((c >> 5) & 0x1F) * 255 // 31) - g) <= 8
                and abs((((c >> 10) & 0x1F) * 255 // 31) - b) <= 8
                for c in clut
            )
            assert found, "couleur absente de la CLUT"
    print("test_cluts_roundtrip: PASS")


def test_reserved_rects_disjoint():
    """Le scratch heros ne doit pas chevaucher un framebuffer : sinon le clear
    du DRAWENV efface la texture a chaque frame et le sprite affiche des
    couleurs de framebuffer (arc-en-ciel observe en capture)."""
    rects = px.RESERVED_RECTS
    def overlaps(a, b):
        return (a[0] < b[0] + b[2] and b[0] < a[0] + a[2]
                and a[1] < b[1] + b[3] and b[1] < a[1] + a[3])
    for i, a in enumerate(rects):
        for b in rects[i + 1:]:
            assert not overlaps(a, b), f"zones reservees chevauchantes : {a} / {b}"
    hero = px.HERO_SCRATCH
    assert hero[1] >= 240, f"scratch heros dans les framebuffers : {hero}"
    print("test_reserved_rects_disjoint: PASS")


def test_vram_layout_no_overlap():
    reserved = px.RESERVED_RECTS
    blobs = [(px.hw4(24), 16)] * 821 + [(16, 1)] * 120
    placed = px.place_vram(blobs, reserved)
    assert len(placed) == len(blobs)
    def overlaps(a, b):
        return (a[0] < b[0] + b[2] and b[0] < a[0] + a[2]
                and a[1] < b[1] + b[3] and b[1] < a[1] + a[3])
    for x, y, w, h in placed:
        assert 0 <= x and x + w <= px.VRAM_W
        assert 0 <= y and y + h <= px.VRAM_H
        if not (w == 16 and h == 1):  # texture 4bpp : pas de chevauchement de page
            assert x // px.PAGE_W == (x + w - 1) // px.PAGE_W, f"page X franchie : {(x,y,w,h)}"
            assert y // 256 == (y + h - 1) // 256, f"page Y franchie : {(x,y,w,h)}"
    for i, a in enumerate(placed):
        for b in reserved:
            assert not overlaps(a, b), f"chevauchement reserve {a} / {b}"
        for b in placed[i + 1:]:
            assert not overlaps(a, b), f"chevauchement VRAM {a} / {b}"
    print("test_vram_layout_no_overlap: PASS")


def test_hero_rows_aligned():
    for w, h in ((25, 41), (33, 49), (60, 46), (24, 41)):
        packed, _clut = px.quantize(_rgba(w, h, {(0, 0): (255, 0, 0, 255)}), w, h)
        padded, stride, upload_h = px.pad_rows(packed, w, h)
        assert stride % 8 == 0, f"stride={stride} n'est pas un multiple de 8 demi-mots"
        assert upload_h >= h
        assert (stride * upload_h) % 32 == 0, "LoadImage doit recevoir un multiple de 16 mots 32 bits"
        assert len(padded) == stride * 2 * upload_h
        assert padded[:stride * 2] == packed[:len(packed) // h] + b"\x00" * (stride * 2 - len(packed) // h)
    print("test_hero_rows_aligned: PASS")


def test_zone011_counts():
    z = px.parse_a1m(os.path.join(REPO, "assets", "maps", "011.a1m"))
    assert (z["w"], z["h"], z["nTiles"], z["nWalls"]) == (52, 60, 821, 4099)
    assert len(z["cells"]) == 52 * 60 * 8
    assert len(z["walls"]) == 4099
    print("test_zone011_counts: PASS")


def test_zone120_blob_and_vram():
    """La salle du trone doit etre exportee comme les autres zones, avec un blob
    P1ZM exploitable par zone120Init() et des tuiles hors des zones reservees."""
    assert px.PS1_ZONE_IDS == ('011', '013', '019', '119', '120'), \
        f"zones PS1 incompletes : {px.PS1_ZONE_IDS}"
    zone = px.parse_a1m(os.path.join(REPO, "assets", "maps", "120.a1m"))
    hero = px.subset_hero(px.parse_a1s(os.path.join(REPO, "assets", "sprites", "hero.a1s")))
    with tempfile.TemporaryDirectory() as out:
        px.export(zone, hero, out, 'zone120')
        path = os.path.join(out, 'zone120.dat')
        assert os.path.exists(path), "zone120.dat non genere"
        b = open(path, 'rb').read()
        assert b[:4] == b'P1ZM'
        w, h, ntiles, ncluts, _ = struct.unpack_from('<HHHHH', b, 4)
        assert (w, h, ntiles) == (zone['w'], zone['h'], zone['nTiles'])
        p = 14 + ntiles * 2  # entete + indices de CLUT ; viennent les rects de tuiles
        rects = [struct.unpack_from('<HHHH', b, p + i * 8) for i in range(ntiles)]
        def overlaps(a, c):
            return (a[0] < c[0] + c[2] and c[0] < a[0] + a[2]
                    and a[1] < c[1] + c[3] and c[1] < a[1] + a[3])
        for r in rects:
            assert 0 <= r[0] and r[0] + r[2] <= px.VRAM_W
            assert 0 <= r[1] and r[1] + r[3] <= px.VRAM_H
            for reserved in px.RESERVED_RECTS:
                assert not overlaps(r, reserved), f"tuile zone120 sur VRAM reserve : {r} / {reserved}"
    print("test_zone120_blob_and_vram: PASS")


def test_hero_subset_groups():
    s = px.parse_a1s(os.path.join(REPO, "assets", "sprites", "hero.a1s"))
    sub = px.subset_hero(s)
    assert len(sub["anims"]) == len(px.HERO_SUBSET)
    # Ordre impose par hero_ps1.cpp::heroUpdate : repos, marche, course, saut, epee.
    # SpriteBank (src/sprite.h) : IDLE=0 WALK=1 ATTACK=2 RUN=3 JUMP=6.
    assert px.HERO_SUBSET == (0, 1, 3, 6, 2), f"mauvais mapping d'animations : {px.HERO_SUBSET}"
    for gi, group in enumerate(sub["anims"]):
        assert len(group) == 4
        total = 0
        for di, steps in enumerate(group):
            # Un groupe vide fait retourner heroDraw() des qu'on appuie sur le
            # bouton correspondant : le heros disparait (observe sur Carré).
            assert len(steps) > 0, f"groupe {gi} direction {di} sans frame"
            total += len(steps)
            for frame, _ticks in steps:
                assert 0 <= frame < sub["nFrames"]
        assert total > 0, f"groupe {gi} vide"
    # Le groupe 16 de hero.a1s est vide : il ne doit plus etre selectionne.
    assert 16 not in px.HERO_SUBSET
    print("test_hero_subset_groups: PASS")


def test_pf_constants_match_exporter():
    header = open(os.path.join(REPO, "src", "platform", "pf.h"), encoding="utf-8").read()
    assert f"#define PF_VRAM_W {px.VRAM_W}" in header
    assert f"#define PF_VRAM_H {px.VRAM_H}" in header
    assert "#define PF_FB0_X 0" in header and "#define PF_FB0_Y 0" in header
    assert "#define PF_FB1_X 320" in header and "#define PF_FB1_Y 0" in header
    for name, value in (("PF_FONT_X", 0), ("PF_FONT_Y", 256), ("PF_FONT_W", 64),
                        ("PF_FONT_H", 256), ("PF_FONT_CLUT_X", 128),
                        ("PF_FONT_CLUT_Y", 256), ("PF_WIND_X", 256),
                        ("PF_WIND_Y", 256), ("PF_WIND_W", 64),
                        ("PF_WIND_H", 256), ("PF_WIND_CLUT_X", 320),
                        ("PF_WIND_CLUT_Y", 256),
                        ("PF_ACTOR3_TEX_X", 800), ("PF_ACTOR4_TEX_X", 832),
                        ("PF_ACTOR5_TEX_X", 864), ("PF_ACTOR_TEX_Y", 256),
                         ("PF_HERO_CLUT_X", px.HERO_CLUTS[0]),
                         ("PF_HERO_CLUT_Y", px.HERO_CLUTS[1]),
                         ("PF_ICON_TEX_X", px.ICON_ATLAS[0]),
                         ("PF_ICON_TEX_Y", px.ICON_ATLAS[1]),
                         ("PF_ICON_TEX_W", px.ICON_ATLAS[2]),
                         ("PF_ICON_TEX_H", px.ICON_ATLAS[3]),
                         ("PF_ICON_CLUT_X", px.ICON_CLUT[0]),
                         ("PF_ICON_CLUT_Y", px.ICON_CLUT[1]),
                         ("PF_HUD_TEX_X", px.HUD_ATLAS[0]),
                         ("PF_HUD_TEX_Y", px.HUD_ATLAS[1]),
                         ("PF_HUD_TEX_W", px.HUD_ATLAS[2]),
                         ("PF_HUD_TEX_H", px.HUD_ATLAS[3]),
                         ("PF_HUD_CLUT_X", px.HUD_CLUTS[0]),
                         ("PF_HUD_CLUT_Y", px.HUD_CLUTS[1])):
        assert f"#define {name} {value}" in header, f"{name} desynchronise de pf.h"
    print("test_pf_constants_match_exporter: PASS")


def test_directions_match_a1sp_order():
    """Ordre reel des directions dans hero.a1s : UP=0, LEFT=1, RIGHT=2, DOWN=3.

    Reconnu sur materiel reel le 2026-11-21 : avec l'ancien mapping
    UP=0/RIGHT=1/LEFT=2/DOWN=3, le heros avancait a reculons sur les deux
    profils ; les dirs 1 et 2 sont donc inverses. Les directions 0 et 3
    (dos/face) etaient correctes. Les profils sont aussi verifies visuellement
    sur les frames agrandies de hero.a1s.
    """
    import re
    header = open(os.path.join(REPO, "src", "platform", "pf.h"), encoding="utf-8").read()
    hero = open(os.path.join(REPO, "src", "ps1", "hero_ps1.cpp"), encoding="utf-8").read()
    for name, value in (("UP", 0), ("LEFT", 1), ("RIGHT", 2), ("DOWN", 3)):
        assert re.search(rf"#define\s+PF_DIR_{name}\s+{value}\b", header), \
            f"PF_DIR_{name} absent ou faux"
    for btn, direction in (("UP", "UP"), ("LEFT", "LEFT"),
                           ("RIGHT", "RIGHT"), ("DOWN", "DOWN")):
        assert f"if (pad & PF_BTN_{btn}) return PF_DIR_{direction};" in hero, \
            f"direction {direction} non reliee a PF_BTN_{btn}"
    print("test_directions_match_a1sp_order: PASS")


def test_idle_is_static():
    """Le groupe repos doit rester sur sa premiere frame : les frames A1SP
    du groupe 0 levaient les jambes et simulaient une marche sur place."""
    hero = open(os.path.join(REPO, "src", "ps1", "hero_ps1.cpp"), encoding="utf-8").read()
    assert "if (g_group == 0) {" in hero
    assert "g_step = 0;" in hero and "g_timer = 0;" in hero
    print("test_idle_is_static: PASS")


def test_canStand_port():
    # Carte 4x4 fabriquee : tout est solide sauf (1,1),(2,1),(1,2),(2,2).
    w = h = 4
    cells = bytearray(w * h * 8)
    for y in range(h):
        for x in range(w):
            struct.pack_into("<BbBBHH", cells, (y * w + x) * 8,
                             0, 0, 0, 0x00 if 1 <= x <= 2 and 1 <= y <= 2 else 0x11, 0, 0)
    for x, y, expected in ((36, 24, True), (12, 24, False), (84, 24, False),
                           (36, 12, False), (36, 52, False), (60, 60, False)):
        got = px.can_stand_ps1(bytes(cells), w, h, x, y, 0, 6, 3)
        assert got == expected, f"canStand({x},{y}) = {got}, attendu {expected}"
    print("test_canStand_port: PASS")


def test_camera_clamp():
    for hx, hy in ((0, 0), (52 * 24 - 1, (60 + 16) * 16 - 1), (624, 480), (-50, -50)):
        cx, cy = px.camera_for(hx, hy)
        assert 0 <= cx <= 52 * 24 - px.VIEW_W
        assert 0 <= cy <= (60 + 16) * 16 - px.VIEW_H
    print("test_camera_clamp: PASS")


def test_font_vram_reserved():
    """L'atlas de police et ses CLUT doivent etre reserves dans la VRAM :
    sinon le convertisseur de zone ecrase les glyphes du titre."""
    from ps1_export_zone import RESERVED_RECTS
    font = (0, 256, 64, 256)       # 256 px 4bpp = 64 demi-mots
    clut = px.FONT_CLUTS           # 7 banques depuis v0.8.17 (bleu pale + bleu aide)
    assert clut == (128, 256, 16, 7)
    wind = (256, 256, 64, 256)
    wind_clut = (320, 256, 16, 16)
    for rect in (font, clut, wind, wind_clut):
        assert rect in RESERVED_RECTS, f"rectangle VRAM non reserve: {rect}"
    for rect in (font, wind):
        for r in RESERVED_RECTS:
            if r == rect:
                continue
            assert r[0] + r[2] <= rect[0] or rect[0] + rect[2] <= r[0] or \
                   r[1] + r[3] <= rect[1] or rect[1] + rect[3] <= r[1], \
                   f"chevauchement VRAM: {r} et {rect}"
    print("test_font_vram_reserved: PASS")


def test_npc_vram_reserved():
    """Les sprites PNJ A1 et leurs CLUT ne doivent pas ecraser les tuiles de
    zone, le scratch heros, la police ou la fenetre."""
    import ps1_export_npc as pn
    for rect in (pn.NPC0_TEX, pn.NPC1_TEX, pn.NPC2_TEX, pn.NPC_CLUTS):
        assert rect in px.RESERVED_RECTS, f"rectangle PNJ non reserve: {rect}"
    assert pn.NPC0_TEX[0] + pn.NPC0_TEX[2] <= pn.NPC1_TEX[0]
    assert pn.NPC1_TEX[0] + pn.NPC1_TEX[2] <= pn.NPC2_TEX[0]
    assert pn.NPC0_TEX[1] + pn.NPC0_TEX[3] <= pn.NPC_CLUTS[1]
    for name, (path, frame, zone, slot) in pn.SOURCE.items():
        assert frame >= 0 and zone in (11, 13, 19, 26, 42, 50, 119), f"source PNJ invalide: {name}"
        assert 0 <= slot < len(pn.SLOT_RECTS), f"slot PNJ invalide: {name}"
    print("test_npc_vram_reserved: PASS")


def test_npc_blob_header_uploadable():
    """Le blob P1NP doit decrire tous les PNJ A1, avec leur zone et leur slot
    VRAM, sans depasser les trois emplacements reserves."""
    import struct
    import ps1_export_npc as pn
    out = os.path.join(REPO, 'ps1', 'assets')
    os.makedirs(out, exist_ok=True)
    size = pn.build(out)
    b = open(os.path.join(out, 'npc.dat'), 'rb').read()
    assert len(b) == size and b[:4] == b'P1NP'
    count, _ = struct.unpack_from('<HH', b, 4)
    assert count == len(pn.SOURCE) == 15
    assert list(pn.SOURCE)[9] == 'guide_vallee' and pn.SOURCE['guide_vallee'][2] == 26
    by_zone = {}
    p = 8
    for i in range(count):
        w, h, ox, oy, stride, upload_h = struct.unpack_from('<HHhhHH', b, p)
        p += 12
        tex_x, tex_y, clut_x, clut_y, zone, slot = struct.unpack_from('<HHHHHH', b, p)
        p += 12
        assert w > 0 and h > 0 and stride >= 8 and upload_h >= h
        rect = (pn.NPC0_TEX, pn.NPC1_TEX, pn.NPC2_TEX)[slot]
        assert (tex_x, tex_y) == (rect[0], rect[1])
        assert stride <= rect[2] and upload_h <= rect[3]
        assert (clut_x, clut_y) == (pn.NPC_CLUTS[0], pn.NPC_CLUTS[1] + slot)
        by_zone.setdefault(zone, []).append(slot)
        p += 32 + stride * 2 * upload_h
    assert p == len(b)
    assert set(by_zone) == {11, 13, 19, 26, 42, 50, 119}, f'zones PNJ inattendues : {sorted(by_zone)}'
    for zone, slots in by_zone.items():
        assert len(slots) <= 3 and len(set(slots)) == len(slots), \
            f'slots PNJ en conflit dans la zone {zone} : {slots}'
    print("test_npc_blob_header_uploadable: PASS")


def test_actor_vram_and_blob_uploadable():
    """Ogre, coffres, Mephisto, Alexia et Roi : VRAM dediee et blob P1AC."""
    import struct
    import ps1_export_actor as pa
    rects = (pa.ACTOR0_TEX, pa.ACTOR1_TEX, pa.ACTOR2_TEX,
             pa.ACTOR3_TEX, pa.ACTOR4_TEX, pa.ACTOR5_TEX)
    for rect in (*rects, pa.ACTOR_CLUTS):
        assert rect in px.RESERVED_RECTS, f"rectangle acteur non reserve: {rect}"
    for a, b in zip(rects, rects[1:]):
        assert a[0] + a[2] <= b[0], f"slots acteurs en conflit : {a} / {b}"
    assert rects[0][1] + rects[0][3] <= pa.ACTOR_CLUTS[1]
    expected = {'ogre', 'coffre_strange_key', 'coffre_lettre',
                'mephisto', 'alexia_trone', 'roi_trone', 'coffre_ordre',
                'golem', 'coffre_lampe', 'golem_eden', 'coffre_prisme',
                'capitaine', 'coffre_cristal'}
    assert set(pa.SOURCE) == expected, f"acteurs PS1 incomplets : {sorted(pa.SOURCE)}"
    for name in ('mephisto', 'alexia_trone', 'roi_trone'):
        assert pa.SOURCE[name][2] == 120, f"{name} hors de la zone 120"
    # L'ordre de SOURCE fixe l'index QST_ACTOR_* : coffre_ordre est le 7e (6).
    assert list(pa.SOURCE)[6] == 'coffre_ordre'
    assert list(pa.SOURCE)[7:9] == ['golem', 'coffre_lampe']
    assert pa.SOURCE['golem'][2] == pa.SOURCE['coffre_lampe'][2] == 26
    assert pa.SOURCE['coffre_ordre'][2] == 119
    # Deux acteurs d'une meme zone ne partagent jamais un slot VRAM.
    used = {}
    for name, (_, _, zone, slot) in pa.SOURCE.items():
        assert (zone, slot) not in used, f"{name} et {used.get((zone, slot))} : slot partage"
        used[(zone, slot)] = name
    out = os.path.join(REPO, 'ps1', 'assets')
    os.makedirs(out, exist_ok=True)
    size = pa.build(out)
    b = open(os.path.join(out, 'actor.dat'), 'rb').read()
    assert len(b) == size and b[:4] == b'P1AC'
    count, _ = struct.unpack_from('<HH', b, 4)
    assert count == len(pa.SOURCE) == 13
    p = 8
    zones = set()
    for i in range(count):
        w, h, ox, oy, stride, upload_h = struct.unpack_from('<HHhhHH', b, p)
        p += 12
        tex_x, tex_y, clut_x, clut_y, zone, slot = struct.unpack_from('<HHHHHH', b, p)
        p += 12
        assert w > 0 and h > 0 and stride >= 8 and upload_h >= h
        rect = rects[slot]
        assert (tex_x, tex_y, clut_y - pa.ACTOR_CLUTS[1]) == (rect[0], rect[1], slot)
        assert stride <= rect[2] and upload_h <= rect[3]
        zones.add(zone)
        p += 32 + stride * 2 * upload_h
    assert p == len(b)
    assert zones == {13, 19, 26, 42, 50, 119, 120}, f"zones acteur inattendues : {sorted(zones)}"
    print("test_actor_vram_and_blob_uploadable: PASS")


def test_hud_vram_reserved():
    """Les 15 frames du HUD (gemmes + /MM) ne doivent ecraser ni la police,
    ni la fenetre, ni le scratch heros, ni les PNJ."""
    import ps1_export_hud as hud
    import ps1_export_npc as pn
    expected_frames = (1, 4, 5, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19)
    assert hud.FRAMES == expected_frames, "frames HUD differentes du HUD PC"
    for rect in (hud.HUD_ATLAS, hud.HUD_CLUTS):
        assert rect in px.RESERVED_RECTS, f"rectangle HUD non reserve: {rect}"
    assert hud.HUD_ATLAS == (64, 256, 16, 224), "atlas HUD trop petit pour /MM"
    assert hud.HUD_CLUTS == (64, 480, 16, 16), "CLUT HUD trop courtes pour /MM"
    for a, b in ((hud.HUD_ATLAS, px.HERO_SCRATCH), (hud.HUD_ATLAS, px.WIND_ATLAS),
                 (hud.HUD_ATLAS, px.FONT_ATLAS), (hud.HUD_ATLAS, px.ENEMY_TEX),
                 (hud.HUD_ATLAS, hud.HUD_CLUTS),
                 (hud.HUD_CLUTS, px.FONT_CLUTS), (hud.HUD_CLUTS, pn.NPC_CLUTS)):
        assert a[0] + a[2] <= b[0] or b[0] + b[2] <= a[0] or \
               a[1] + a[3] <= b[1] or b[1] + b[3] <= a[1], \
               f"chevauchement HUD: {a} et {b}"
    print("test_hud_vram_reserved: PASS")


def _assert_no_atlas_overlap(rows):
    """Garde-fou : deux frames ne doivent jamais partager une ligne d'atlas."""
    previous_end = -1
    for atlas_y, upload_h in rows:
        assert atlas_y >= previous_end, (
            f"frames HUD chevauchees : {atlas_y} < fin precedente {previous_end}")
        previous_end = atlas_y + upload_h


def test_hud_blob_header_uploadable():
    """Le blob P1HU doit decrire 15 frames A1 non vides (4 gemmes, 10 chiffres,
    1 slash), transferables dans l'atlas reserve, sans texture inventee."""
    import struct
    import ps1_export_hud as hud
    expected_frames = (1, 4, 5, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19)
    expected_atlas_y = (0, 16, 32, 40) + tuple(48 + 16 * i for i in range(11))
    out = os.path.join(REPO, 'ps1', 'assets')
    os.makedirs(out, exist_ok=True)
    size = hud.build(out)
    b = open(os.path.join(out, 'hud.dat'), 'rb').read()
    assert len(b) == size and b[:4] == b'P1HU'
    count, _ = struct.unpack_from('<HH', b, 4)
    assert count == 15
    p = 8
    atlas_rows = []
    previous_clut = -1
    for i in range(count):
        w, h, ox, oy, stride, upload_h, atlas_y, clut_y = struct.unpack_from('<HHhhHHHH', b, p)
        p += 16
        assert w > 0 and h > 0 and stride >= 2 and upload_h >= h
        assert atlas_y == hud.HUD_ATLAS[1] + expected_atlas_y[i]
        assert clut_y == hud.HUD_CLUTS[1] + i
        assert clut_y > previous_clut, "les banques CLUT doivent etre distinctes"
        previous_clut = clut_y
        assert stride <= hud.HUD_ATLAS[2] and upload_h <= hud.HUD_ATLAS[3]
        assert atlas_y + upload_h <= hud.HUD_ATLAS[1] + hud.HUD_ATLAS[3]
        atlas_rows.append((atlas_y, upload_h))
        clut = struct.unpack_from('<16H', b, p)
        assert clut[0] == 0, "l'index 0 doit rester transparent"
        p += 32 + stride * 2 * upload_h
    _assert_no_atlas_overlap(atlas_rows)
    # Controle negatif du garde-fou : un chevauchement synthetique est refuse.
    overlap_rejected = False
    try:
        _assert_no_atlas_overlap([(0, 16), (8, 16)])
    except AssertionError:
        overlap_rejected = True
    assert overlap_rejected, "le garde-fou doit rejeter deux frames qui se chevauchent"
    assert p == len(b)
    print("test_hud_blob_header_uploadable: PASS")


def test_hero_blob_is_zone_independent():
    """P1SP doit rester identique quelle que soit la zone exportee.

    Les CLUT du heros ne doivent pas etre dedupliees avec celles du decor :
    sinon le dernier export (zone 119) ecrase hero.dat et le heros recoit les
    couleurs de la zone 011 apres un passage de zone.
    """
    hero = px.subset_hero(px.parse_a1s(os.path.join(REPO, 'assets', 'sprites', 'hero.a1s')))
    zone11 = px.parse_a1m(os.path.join(REPO, 'assets', 'maps', '011.a1m'))
    zone119 = px.parse_a1m(os.path.join(REPO, 'assets', 'maps', '119.a1m'))
    with tempfile.TemporaryDirectory() as a, tempfile.TemporaryDirectory() as b:
        px.export(zone11, hero, a, 'zone011')
        px.export(zone119, hero, b, 'zone119')
        one = open(os.path.join(a, 'hero.dat'), 'rb').read()
        two = open(os.path.join(b, 'hero.dat'), 'rb').read()
    assert one == two, 'hero.dat depend de la zone exportee'
    print('test_hero_blob_is_zone_independent: PASS')


def test_font_generator_matches_a1_header():
    bits = open(os.path.join(REPO, 'src', 'a1_font.h'), encoding='utf-8').read()
    assert 'kGlyphBits' in bits
    generator = os.path.join(REPO, 'tools', 'ps1_export_font.py')
    assert os.path.exists(generator), "generateur de police PS1 absent"
    out = subprocess.check_output([sys.executable, generator, '--stdout'])
    assert len(out) >= 8192 + 4 * 32
    print("test_font_generator_matches_a1_header: PASS")


def test_font_header_rows_are_comma_separated():
    """Chaque ligne de donnees doit finir par une virgule, sinon le C++ MIPS ne compile pas."""
    import ps1_export_font as pf
    atlas, cluts, adv, wind, wind_cluts = pf.build()
    header = pf.format_header(atlas, cluts, adv, wind, wind_cluts)
    start = header.index('kA1FontAtlas')
    end = header.index('};', start)
    rows = [line.strip() for line in header[start:end].splitlines() if line.strip()]
    body = rows[1:]
    assert body and all(line.endswith(',') for line in body), \
        "ligne de tableau sans virgule : le compilateur MIPS rejette le header"
    print("test_font_header_rows_are_comma_separated: PASS")


def test_font_atlas_matches_pc_glyph_mask():
    """Les 256 masques 4bpp PS1 doivent etre identiques aux glyphes FONT3 du PC.

    Verrouille notamment u (0x75) vs v (0x76) : toute ambiguite visuelle observee
    sur capte PS1 doit venir de FONT3.TIM lui-meme, pas de l'exporteur.
    """
    import ps1_export_font as pf
    atlas, _cluts, _adv, _wind, _wind_cluts = pf.build()
    bits = pf.read_glyph_bits(open(os.path.join(REPO, 'src', 'a1_font.h'), encoding='utf-8').read())
    for g in range(256):
        gx, gy = (g % 16) * 16, (g // 16) * 16
        for y in range(16):
            row = bits[g * 32 + y * 2:g * 32 + y * 2 + 2]
            for x in range(16):
                expected = bool(row[x // 8] & (0x80 >> (x % 8)))
                off = ((gy + y) * 256 + gx + x) // 2
                byte = atlas[off]
                nibble = byte & 0x0F if (gx + x) % 2 == 0 else byte >> 4
                assert bool(nibble) == expected, f"glyphe 0x{g:02x} px ({x},{y}) : PS1 {nibble}, PC {int(expected)}"
    # u et v sont deux masques distincts dans FONT3 (controle cible du rapport graphismes).
    def mask(g):
        return tuple(bits[g * 32 + y * 2] for y in range(16))
    assert mask(0x75) != mask(0x76), "u et v ont le meme masque dans FONT3"
    print("test_font_atlas_matches_pc_glyph_mask: PASS")


def test_dialogue_exporter_matches_pc_pagination():
    """Les panneaux PS1 doivent venir du meme texte A2 et de la meme largeur que le PC."""
    import ps1_export_dialogue as dlg
    panels = dlg.build_panels()
    assert panels, "aucun panneau de dialogue genere"
    assert b"Enfin debout" in panels[0][0], "le texte A2 de Prunewell est absent"
    adv = dlg.advances()
    for pi, panel in enumerate(panels):
        assert len(panel) == 3, f"panneau {pi}: {len(panel)} lignes au lieu de 3"
        for li, line in enumerate(panel):
            width = sum(adv[c] for c in line)
            assert width <= 254, f"panneau {pi} ligne {li}: {width}px > 254"
    print("test_dialogue_exporter_matches_pc_pagination: PASS")


def test_dialogue_header_is_pod_and_uploadable():
    """Le header genere doit etre un tableau POD ASCII, sans STL ni octets bruts."""
    import ps1_export_dialogue as dlg
    panels = {i: dlg.build_panels(i) for i in dlg.DIALOGUE_IDS}
    text = dlg.format_header(panels)
    assert 'static const char kDlgLines[' in text
    assert 'kDlgPanelCount' in text
    assert 'kDlgTable' in text and 'DlgTableEntry' in text
    expected = ('prunewell', 'garde', 'finrod', 'marchand_kindra',
                'alexia_epave', 'belgar_epave', 'toroledo_natasha', 'jeehan',
                'objet_strange_key', 'alexia_epave_cle', 'victoire',
                'objet_lettre', 'mephisto_arrivee', 'mephisto_trone',
                'mephisto_vaincu', 'roi_trone', 'roi_trone_mephisto_vaincu',
                'alexia_finale', 'alexia_finale_mephisto_vaincu',
                'finrod_prunewell',
                'alexia_kindra', 'objet_ordre', 'jeehan_objet_ordre',
                'vallee_arrivee', 'guide_vallee', 'boss_026', 'objet_lampe',
                'eden_arrivee', 'roswell', 'roswell_objet_lampe',
                'roswell_objet_prisme', 'ome', 'boss_042', 'objet_prisme',
                'medusa_arrivee', 'ruby_puerto', 'albert_puerto', 'zach_fort',
                'boss_medusa', 'objet_cristal')
    assert dlg.DIALOGUE_IDS == expected, f'identifiants de dialogue incomplets : {dlg.DIALOGUE_IDS}'
    assert 'kDlgLines_garde' in text, "le dialogue du garde doit etre exporte"
    # Aucune touche clavier PC ne doit apparaitre dans un texte PS1.
    joined = b' '.join(l for p in panels['jeehan_objet_ordre'] for l in p)
    assert b'Touche Q' not in joined and b'START' in joined
    for name in expected:
        # kDlgLines est l'alias historique de prunewell (captures v0.3.0) ;
        # les autres identifiants ont leur propre tableau suffixe.
        array = 'kDlgLines' if name == 'prunewell' else 'kDlgLines_' + name
        assert array in text, f"dialogue {name} absent du header"
        assert f'"{name}"' in text, f"nom {name} absent de kDlgNames"
    for name in expected[12:]:
        assert dlg.source_lines(name), f"dialogue {name} absent de data/dialogues.txt"
    assert text.isascii(), "octets non ASCII : les caracteres Latin-1 doivent etre echappes"
    assert 'std::' not in text and '#include <' not in text
    print("test_dialogue_header_is_pod_and_uploadable: PASS")


def test_throne_runtime_hooks():
    """Le jalon trone/fin doit etre cable dans la boucle et le harnais de capture."""
    def src(path):
        return open(os.path.join(REPO, path), encoding='utf-8').read()
    boot = src(os.path.join('src', 'ps1', 'boot.cpp'))
    ih = src(os.path.join('src', 'ps1', 'interaction_ps1.h'))
    ic = src(os.path.join('src', 'ps1', 'interaction_ps1.cpp'))
    dc = src(os.path.join('src', 'ps1', 'dialogue_ps1.cpp'))
    lua = src(os.path.join('docs', 'ps1', 'lua', 'capture_ending.lua'))
    for needle in ('interactionEndingPS1', 'interactionCaptureZonePS1', 'endingDrawPS1'):
        assert needle in boot, f'{needle} absent de boot.cpp'
    assert 'interactionEndingPS1' in ih and 'interactionEndingPS1' in ic
    assert 'interactionCaptureZonePS1' in ih and 'interactionCaptureZonePS1' in ic
    assert 'g_ps1CaptureZone' in ic, 'variable de capture absente'
    assert 'questThroneEnter(&g_quest)' in ic, 'arrivee dans la salle non declenchee'
    assert 'kDlgLines_alexia_finale_mephisto_vaincu' in dc, \
        'l ecran de fin doit reutiliser les six lignes A2 exportees'
    assert 'ENDING_CAPTURE: PASS' in lua
    print('test_throne_runtime_hooks: PASS')


def test_intro_exporter_preserves_all_prologue_text():
    """Les panneaux d'intro doivent contenir tout le prologue A2, sans perte."""
    import ps1_export_intro as intro
    panels = intro.build_panels()
    assert len(panels) == 10, f"{len(panels)} panneaux au lieu de 10"
    folded = [intro.fold(line) for line in intro.source_lines()]
    expected = b''.join(folded).replace(b' ', b'')
    actual = b''.join(line for panel in panels for line in panel).replace(b' ', b'')
    assert expected == actual, "le texte du prologue est modifie par la pagination"
    assert b'Voyons' in actual, "le mot 'Voyons' est absent des panneaux generes"
    adv = intro.advances()
    for pi, panel in enumerate(panels):
        assert len(panel) == 3, f"panneau {pi}: {len(panel)} lignes au lieu de 3"
        for li, line in enumerate(panel):
            width = sum(adv[c] for c in line)
            assert width <= 254, f"intro panneau {pi} ligne {li}: {width}px > 254"
    print("test_intro_exporter_preserves_all_prologue_text: PASS")


if __name__ == "__main__":
    all_tests = [
        test_transparent_is_index_zero,
        test_cluts_roundtrip,
        test_reserved_rects_disjoint,
        test_vram_layout_no_overlap,
        test_zone011_counts,
        test_zone120_blob_and_vram,
        test_hero_subset_groups,
        test_hero_rows_aligned,
        test_pf_constants_match_exporter,
        test_directions_match_a1sp_order,
        test_idle_is_static,
        test_canStand_port,
        test_camera_clamp,
        test_font_vram_reserved,
        test_npc_vram_reserved,
        test_npc_blob_header_uploadable,
        test_actor_vram_and_blob_uploadable,
        test_hud_vram_reserved,
        test_hud_blob_header_uploadable,
        test_hero_blob_is_zone_independent,
        test_font_generator_matches_a1_header,
        test_font_header_rows_are_comma_separated,
        test_font_atlas_matches_pc_glyph_mask,
        test_dialogue_exporter_matches_pc_pagination,
        test_dialogue_header_is_pod_and_uploadable,
        test_throne_runtime_hooks,
        test_intro_exporter_preserves_all_prologue_text,
    ]
    # Sans argument : tous les tests. Avec des noms : seulement ceux la (TDD par tache).
    tests = [t for t in all_tests if not sys.argv[1:] or t.__name__ in sys.argv[1:]]
    failed = 0
    for t in tests:
        try:
            t()
        except Exception as exc:  # noqa: BLE001 - rapport brut pour le RED
            failed += 1
            print(f"{t.__name__}: FAIL ({type(exc).__name__}: {exc})")
    print(f"{len(tests) - failed}/{len(tests)} tests PASS")
    sys.exit(1 if failed else 0)
