"""Tests du convertisseur PS1 A1MP/A1SP -> P1ZM/P1SP.

Usage : py -3.12 tools/ps1_export_zone_test.py
Aucune dependance autre que PIL deja presente dans le projet.
"""
import os
import struct
import subprocess
import sys

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
    font = (0, 256, 128, 256)
    clut = (128, 256, 16, 4)
    assert font in RESERVED_RECTS and clut in RESERVED_RECTS
    for r in RESERVED_RECTS:
        if r == font:
            continue
        assert r[0] + r[2] <= font[0] or font[0] + font[2] <= r[0] or \
               r[1] + r[3] <= font[1] or font[1] + font[3] <= r[1]
    print("test_font_vram_reserved: PASS")


def test_font_generator_matches_a1_header():
    bits = open(os.path.join(REPO, 'src', 'a1_font.h'), encoding='utf-8').read()
    assert 'kGlyphBits' in bits
    generator = os.path.join(REPO, 'tools', 'ps1_export_font.py')
    assert os.path.exists(generator), "generateur de police PS1 absent"
    out = subprocess.check_output([sys.executable, generator, '--stdout'])
    assert len(out) >= 8192 + 4 * 32
    print("test_font_generator_matches_a1_header: PASS")


if __name__ == "__main__":
    all_tests = [
        test_transparent_is_index_zero,
        test_cluts_roundtrip,
        test_reserved_rects_disjoint,
        test_vram_layout_no_overlap,
        test_zone011_counts,
        test_hero_subset_groups,
        test_hero_rows_aligned,
        test_pf_constants_match_exporter,
        test_directions_match_a1sp_order,
        test_idle_is_static,
        test_canStand_port,
        test_camera_clamp,
        test_font_vram_reserved,
        test_font_generator_matches_a1_header,
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
