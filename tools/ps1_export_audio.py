#!/usr/bin/env python3
"""Genere la banque audio PS1 (VAG brut) et le header POD associe.

Sources : assets/sfx/NN.wav (IDs de data/sfx.txt) et le theme de la ligne
`victoire` de data/music.txt. Aucune donnee A2 n'est modifiee, aucun WAV n'est
copie : seule une banque ADPCM reside dans l'EXE via psn00bsdk_target_incbin.
"""

import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))

from ps1_wav_to_vag import encode_pcm, read_wav_mono  # noqa: E402

BANK = REPO / "ps1" / "assets" / "audio.vag"
HEADER = REPO / "src" / "ps1" / "audio_data_ps1.h"
LIMIT = 512 * 1024
ALIGN = 64


def read_sfx_ids() -> list[int]:
    ids = set()
    for line in (REPO / "data" / "sfx.txt").read_text(encoding="utf-8").splitlines():
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        parts = line.split()
        if len(parts) >= 2 and parts[1].isdigit():
            ids.add(int(parts[1]))
    return sorted(ids)


def read_victory_track() -> int:
    for line in (REPO / "data" / "music.txt").read_text(encoding="utf-8").splitlines():
        parts = line.split()
        if len(parts) >= 2 and parts[0] == "victoire" and parts[1].isdigit():
            return int(parts[1])
    raise ValueError("categorie victoire absente de data/music.txt")


def align_64(data: bytes) -> bytes:
    return data + bytes((-len(data)) % ALIGN)


def main() -> int:
    clips = []
    for sfx_id in read_sfx_ids():
        path = REPO / "assets" / "sfx" / f"{sfx_id:02d}.wav"
        if not path.exists():
            raise FileNotFoundError(path)
        samples, rate = read_wav_mono(str(path))
        clips.append((sfx_id, encode_pcm(samples, rate, False), False))

    track = read_victory_track()
    path = REPO / "assets" / "music" / f"{track:02d}.wav"
    samples, rate = read_wav_mono(str(path))
    music = encode_pcm(samples, rate, True)
    clips.append((track, music, True))

    bank = bytearray()
    table = []
    music_addr = music_size = 0
    for clip_id, data, is_music in clips:
        addr = len(bank)
        payload = align_64(data)
        bank += payload
        if is_music:
            music_addr, music_size = addr, len(data)
        else:
            table.append((clip_id, addr, len(data)))

    if len(bank) > LIMIT:
        raise ValueError(f"banque audio {len(bank)} octets > budget {LIMIT}")

    BANK.parent.mkdir(parents=True, exist_ok=True)
    BANK.write_bytes(bytes(bank))

    lines = [
        "#pragma once",
        "/* Genere par tools/ps1_export_audio.py - ne pas editer a la main. */",
        "#include <stdint.h>",
        "",
        f"#define PS1_AUDIO_BANK_SIZE {len(bank)}",
        f"#define PS1_AUDIO_SFX_COUNT {len(table)}",
        f"#define PS1_AUDIO_MUSIC_ADDR 0x{music_addr:08x}",
        f"#define PS1_AUDIO_MUSIC_SIZE {music_size}",
        "",
        "struct PS1AudioClip {",
        "\tuint16_t id;",
        "\tuint32_t addr;",
        "\tuint32_t size;",
        "};",
        "",
        "static const struct PS1AudioClip g_ps1AudioSfx[PS1_AUDIO_SFX_COUNT] = {",
    ]
    for clip_id, addr, size in table:
        lines.append(f"\t{{ {clip_id}, 0x{addr:08x}, {size} }},")
    lines += ["};", ""]
    HEADER.write_text("\n".join(lines), encoding="utf-8", newline="\n")

    print(f"PS1_EXPORT_AUDIO: PASS clips={len(table)} music={track} "
          f"bytes={len(bank)}/{LIMIT}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
