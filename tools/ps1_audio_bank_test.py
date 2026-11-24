#!/usr/bin/env python3
"""Verifie la banque audio PS1 generee : budget, table et drapeaux de boucle."""

import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
BANK = REPO / "ps1" / "assets" / "audio.vag"
HEADER = REPO / "src" / "ps1" / "audio_data_ps1.h"
LIMIT = 512 * 1024


def sfx_ids() -> list[int]:
    ids = set()
    for line in (REPO / "data" / "sfx.txt").read_text(encoding="utf-8").splitlines():
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        parts = line.split()
        if len(parts) >= 2 and parts[1].isdigit():
            ids.add(int(parts[1]))
    return sorted(ids)


def test_bank() -> None:
    subprocess.run([sys.executable, str(REPO / "tools" / "ps1_export_audio.py")],
                   check=True, cwd=REPO)
    assert BANK.exists(), "banque VAG absente"
    data = BANK.read_bytes()
    assert len(data) % 64 == 0, "banque non alignee 64 octets"
    assert len(data) <= LIMIT, f"banque hors budget: {len(data)} > {LIMIT}"

    header = HEADER.read_text(encoding="utf-8")
    size = int(re.search(r"#define PS1_AUDIO_BANK_SIZE\s+(\d+)", header).group(1))
    assert size == len(data), "taille header/banque incoherente"

    entries = [(int(i), int(a, 16), int(s)) for i, a, s in
               re.findall(r"\{\s*(\d+)\s*,\s*0x([0-9a-fA-F]+)\s*,\s*(\d+)\s*\}", header)]
    assert sorted(i for i, _, _ in entries) == sfx_ids(), "IDs SFX incoherents avec data/sfx.txt"
    for _, addr, clip_size in entries:
        assert addr % 64 == 0 and addr + clip_size <= len(data), "clip hors banque"

    music_addr = int(re.search(r"#define PS1_AUDIO_MUSIC_ADDR\s+0x([0-9a-fA-F]+)",
                               header).group(1), 16)
    music_size = int(re.search(r"#define PS1_AUDIO_MUSIC_SIZE\s+(\d+)", header).group(1))
    music = data[music_addr:music_addr + music_size]
    flags = [music[i + 1] for i in range(0, len(music) - 15, 16)]
    assert any(f & 0x04 for f in flags), "drapeau de debut de boucle absent"
    assert any(f & 0x02 for f in flags), "drapeau de fin de boucle absent"
    print(f"PS1_AUDIO_BANK: PASS clips={len(entries)} bytes={len(data)}")


if __name__ == "__main__":
    test_bank()
