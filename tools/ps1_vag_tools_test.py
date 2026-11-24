#!/usr/bin/env python3
"""Test hote de l'encodeur/decodeur PS-ADPCM (VAG) pour la cible PS1."""

import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import ps1_wav_to_vag as vag  # noqa: E402


def _sine(count: int, rate: int) -> list[int]:
    return [int(18000.0 * math.sin(2.0 * math.pi * 440.0 * i / rate)) for i in range(count)]


def test_roundtrip_and_layout() -> None:
    samples = _sine(28 * 40, 22050)
    blob = vag.encode_pcm(samples, 22050, loop=True)

    assert len(blob) % 16 == 0 and len(blob) >= 16, "blocs VAG de 16 octets"
    blocks = [blob[i:i + 16] for i in range(0, len(blob), 16)]
    assert blocks[0] == bytes(16), "bloc d'intro silence requis"
    assert blocks[1][1] & 0x04, "drapeau de debut de boucle 0x04"
    assert blocks[-1][1] & 0x02, "drapeau de fin de boucle 0x02"
    for block in blocks:
        assert (block[0] >> 4) <= 4, "filtre VAG hors 0..4"
        assert (block[0] & 0x0F) <= 12, "shift VAG hors 0..12"
        assert block[1] <= 0x07, "drapeau VAG inattendu"

    decoded = vag.decode_vag(blob)
    assert len(decoded) == len(blocks) * 28, "28 echantillons par bloc"
    snr = vag.snr_db(samples, decoded[28:28 + len(samples)])
    assert snr > 20.0, f"SNR insuffisant: {snr:.1f} dB"


def test_oneshot_flags() -> None:
    blob = vag.encode_pcm(_sine(28 * 4, 22050), 22050, loop=False)
    blocks = [blob[i:i + 16] for i in range(0, len(blob), 16)]
    assert not (blocks[1][1] & 0x04), "pas de debut de boucle sur un son unique"
    assert blocks[-1][1] & 0x01, "drapeau de fin 0x01 sur un son unique"


def main() -> int:
    test_roundtrip_and_layout()
    test_oneshot_flags()
    print("PS1_VAG_TOOLS: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
