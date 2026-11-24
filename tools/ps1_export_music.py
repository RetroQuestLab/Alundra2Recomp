#!/usr/bin/env python3
r"""Exporte les 40 themes A2 en fichiers VAGi prets pour le streaming CD PS1.

Chaque fichier : secteur d'en-tete VAGi (2048 octets) puis des chunks mono de
8192 octets (512 blocs PS-ADPCM). Un drapeau de debut (0x04) et de fin (0x02)
de boucle est pose a chaque frontiere de chunk : le gestionnaire d'IRQ SPU
ecrase SPU_CH_LOOP_ADDR pour faire "sauter" la voix d'un buffer a l'autre.

Les WAV sources ne sont jamais modifies. Aucune dependance externe.
"""

import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

from ps1_wav_to_vag import encode_pcm, read_wav_mono  # noqa: E402

THEMES = 40
INTERLEAVE = 8192
SAMPLE_RATE = 22050
SECTOR = 2048
MAGIC = 0x69474156  # "VAGi" little-endian


def patch_chunk_flags(data: bytearray) -> None:
    """Marque chaque chunk comme une boucle autonome pour le materiel SPU."""
    last_block = len(data) - 16
    for base in range(0, len(data), INTERLEAVE):
        first = base + 1
        last = min(base + INTERLEAVE - 16, last_block) + 1
        data[first] = (data[first] & ~0x03) | 0x04
        if last > first:
            data[last] = (data[last] & ~0x03) | 0x02


def vag_header(theme: int, payload_size: int) -> bytes:
    header = bytearray(SECTOR)
    struct.pack_into("<III", header, 0, MAGIC, 0, INTERLEAVE)
    struct.pack_into(">II", header, 12, payload_size, SAMPLE_RATE)
    struct.pack_into("<H", header, 30, 1)  # mono
    name = f"MUSIC{theme:02d}".encode("ascii")
    header[32:32 + len(name)] = name
    return bytes(header)


def export_theme(theme: int, out_dir: Path, force: bool) -> int:
    out = out_dir / f"{theme:02d}.VAG"
    if out.exists() and not force:
        return out.stat().st_size
    src = ROOT / "assets" / "music" / f"{theme:02d}.wav"
    if not src.exists():
        raise FileNotFoundError(src)
    samples, rate = read_wav_mono(str(src))
    payload = bytearray(encode_pcm(samples, rate, False))
    padding = (-len(payload)) % SECTOR
    if padding:
        payload.extend(bytes(padding))
    # Les drapeaux doivent tomber sur les frontieres de chunks du flux REEL
    # (apres padding secteur), sinon l'IRQ SPU se desynchronise du ring buffer.
    patch_chunk_flags(payload)
    out.write_bytes(vag_header(theme, len(payload)) + bytes(payload))
    return out.stat().st_size


def write_path_header(path: Path) -> None:
    lines = [
        "#pragma once",
        "/* Table POD generee par tools/ps1_export_music.py. Ne pas editer. */",
        "#define PS1_MUSIC_COUNT 40",
        "",
        "static const char* const g_ps1MusicPaths[PS1_MUSIC_COUNT] = {",
    ]
    for theme in range(THEMES):
        lines.append(f'\t"\\\\MUSIC\\\\{theme:02d}.VAG",')
    lines.append("};")
    lines.append("")
    path.write_text("\n".join(lines), encoding="utf-8", newline="\n")


def main(argv: list[str]) -> int:
    force = "--force" in argv[1:]
    out_dir = ROOT / "ps1" / "assets" / "music"
    out_dir.mkdir(parents=True, exist_ok=True)
    total = 0
    for theme in range(THEMES):
        size = export_theme(theme, out_dir, force)
        total += size
        print(f"MUSIC_VAG {theme:02d} {size} bytes", flush=True)
    write_path_header(ROOT / "src" / "ps1" / "music_data_ps1.h")
    print(f"PS1_EXPORT_MUSIC: PASS files={THEMES} bytes={total}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
