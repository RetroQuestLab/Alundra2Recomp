#!/usr/bin/env python3
r"""Verifications deterministes de la banque musicale CD streamee PS1.

Controle : fichiers VAGi generes, drapeaux de boucle par chunk, table de chemins,
header C de l'API streaming, mastering ISO et budget SPU. Aucune dependance
externe ; sortie "PS1_MUSIC_BANK: PASS/FAIL".
"""

import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MUSIC_DIR = ROOT / "ps1" / "assets" / "music"
HEADER = ROOT / "src" / "ps1" / "music_data_ps1.h"
EXPECTED_COUNT = 40
INTERLEAVE = 8192
SAMPLE_RATE = 22050
SECTOR = 2048
BANK_ADDR = 0x1010
STREAM_ADDR = 0x5D600
STREAM_BYTES = 2 * INTERLEAVE
SPU_LIMIT = 0x80000


def fail(msg):
    print(f"PS1_MUSIC_BANK: FAIL - {msg}")
    return 1


def check_vag_files():
    bank = ROOT / "ps1" / "assets" / "audio.vag"
    if not bank.exists():
        return "banque VAG residante absente (lancer build_ps1.ps1)"

    for theme in range(EXPECTED_COUNT):
        path = MUSIC_DIR / f"{theme:02d}.VAG"
        if not path.exists():
            return f"fichier CD manquant: {path.name}"
        data = path.read_bytes()
        if len(data) % SECTOR:
            return f"{path.name}: taille {len(data)} non multiple de {SECTOR}"
        if len(data) <= SECTOR:
            return f"{path.name}: aucun bloc audio"
        magic, _version, interleave = struct.unpack_from("<III", data, 0)
        if magic != 0x69474156:
            return f"{path.name}: magic VAGi invalide 0x{magic:08x}"
        if interleave != INTERLEAVE:
            return f"{path.name}: interleave {interleave} != {INTERLEAVE}"
        size, rate = struct.unpack_from(">II", data, 12)
        channels = struct.unpack_from("<H", data, 30)[0]
        if size != len(data) - SECTOR:
            return f"{path.name}: header size {size} != {len(data) - SECTOR}"
        if rate != SAMPLE_RATE:
            return f"{path.name}: sample_rate {rate} != {SAMPLE_RATE}"
        if channels != 1:
            return f"{path.name}: channels {channels} != 1 (mono)"

        payload = data[SECTOR:]
        chunks = (len(payload) + INTERLEAVE - 1) // INTERLEAVE
        for chunk in range(chunks):
            base = chunk * INTERLEAVE
            first = payload[base + 1]
            if not first & 0x04:
                return f"{path.name}: chunk {chunk} sans drapeau debut boucle"
            last_at = min(base + INTERLEAVE - 16, len(payload) - 16)
            last = payload[last_at + 1]
            if not last & 0x02:
                return f"{path.name}: chunk {chunk} sans drapeau fin boucle"
    return None


def check_path_header():
    if not HEADER.exists():
        return f"header C absent: {HEADER}"
    text = HEADER.read_text(encoding="utf-8")
    if "PS1_MUSIC_COUNT 40" not in text:
        return "PS1_MUSIC_COUNT 40 absent"
    if "g_ps1MusicPaths" not in text:
        return "table g_ps1MusicPaths absente"
    for theme in range(EXPECTED_COUNT):
        literal = f'"\\\\MUSIC\\\\{theme:02d}.VAG"'
        if literal not in text:
            return f"chemin CD absent: {literal}"
    return None


def check_spu_layout():
    bank = (ROOT / "ps1" / "assets" / "audio.vag").stat().st_size
    bank_end = BANK_ADDR + bank
    if bank_end > STREAM_ADDR:
        return f"banque residante 0x{bank_end:x} depasse la zone stream 0x{STREAM_ADDR:x}"
    stream_end = STREAM_ADDR + STREAM_BYTES
    if stream_end > SPU_LIMIT:
        return f"buffers stream 0x{stream_end:x} depassent la RAM SPU 0x{SPU_LIMIT:x}"
    return None


def check_stream_api():
    header = ROOT / "src" / "ps1" / "audio_ps1.h"
    if not header.exists():
        return "audio_ps1.h absent"
    text = header.read_text(encoding="utf-8")
    for field in ("active", "theme", "fileFound", "fileSectors", "feedBytes",
                  "chunks", "underruns", "lastError"):
        if not re.search(r"uint32_t\s+" + field + r"\s*;", text):
            return f"PS1StreamDiag.{field} absent"
    for fn in ("audioStreamMusicPS1", "audioStreamPumpPS1", "audioStreamStopPS1"):
        if fn not in text:
            return f"API {fn} absente"
    return None


def check_mastering():
    iso = (ROOT / "ps1" / "iso.xml").read_text(encoding="utf-8")
    if '<dir name="MUSIC">' not in iso:
        return "element <dir name=MUSIC> absent de iso.xml (mkpsxiso: <dir>, pas <directory>)"
    if len(re.findall(r'name="\d\d\.VAG"', iso)) != EXPECTED_COUNT:
        return "iso.xml ne reference pas les 40 fichiers VAG"
    cmake = (ROOT / "ps1" / "CMakeLists.txt").read_text(encoding="utf-8")
    if "cd_stream.cpp" not in cmake:
        return "cd_stream.cpp absent de CMakeLists.txt"
    if "assets/music" not in cmake:
        return "dependances VAG absentes de CMakeLists.txt"
    return None


def main():
    for check in (check_stream_api, check_vag_files, check_path_header,
                  check_spu_layout, check_mastering):
        error = check()
        if error:
            return fail(error)
    bank = (ROOT / "ps1" / "assets" / "audio.vag").stat().st_size
    total = sum(p.stat().st_size for p in MUSIC_DIR.glob("*.VAG"))
    print(f"PS1_MUSIC_BANK: PASS files={EXPECTED_COUNT} bytes={total} "
          f"bank={bank} stream=0x{STREAM_ADDR:x}-0x{STREAM_ADDR + STREAM_BYTES:x}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
