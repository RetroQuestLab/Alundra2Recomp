#!/usr/bin/env python3
r"""Encodeur/decodeur PS-ADPCM (VAG) sans dependance, pour le portage PS1 natif.

Format Sony : blocs de 16 octets / 28 echantillons. Octet 0 = `filter<<4|shift`,
octet 1 = drapeaux (0x04 debut de boucle, 0x02 fin/boucle, 0x01 fin), puis 28
nibbles signes, petit nibble d'abord. Les coefficients de prediction sont ceux
du decodeur materiel : (0,0), (60,0), (115,-52), (98,-55), (122,-60) en 1/64.

Usage :
    py -3.12 tools\ps1_wav_to_vag.py encode <in.wav> <out.vag> [--loop]
    py -3.12 tools\ps1_wav_to_vag.py decode <in.vag> <out.wav>
    py -3.12 tools\ps1_wav_to_vag.py selftest
"""

import math
import struct
import sys
import wave
from pathlib import Path

FILTERS = ((0, 0), (60, 0), (115, -52), (98, -55), (122, -60))
BLOCK_BYTES = 16
BLOCK_SAMPLES = 28
TARGET_RATE = 22050


def clamp16(v: int) -> int:
    return -32768 if v < -32768 else (32767 if v > 32767 else v)


def downmix(samples: list[int], rate: int) -> list[int]:
    """44,1 kHz -> 22,05 kHz par moyenne de deux echantillons ; 22,05 kHz direct."""
    if rate == TARGET_RATE:
        return list(samples)
    if rate == 44100:
        return [(samples[i] + samples[i + 1]) // 2
                for i in range(0, len(samples) - 1, 2)]
    raise ValueError(f"taux d'echantillonnage non pris en charge: {rate}")


def _predict(filter_index: int, s1: int, s2: int) -> int:
    k1, k2 = FILTERS[filter_index]
    return (s1 * k1 + s2 * k2 + 32) >> 6


def encode_pcm(samples: list[int], rate: int, loop: bool) -> bytes:
    """Encode une liste PCM 16 bits en blocs VAG bruts (sans header VAGp)."""
    mono = downmix(samples, rate)
    if not mono:
        return bytes(BLOCK_BYTES)
    padding = (-len(mono)) % BLOCK_SAMPLES
    if padding:
        mono = mono + [mono[-1]] * padding

    frame_count = len(mono) // BLOCK_SAMPLES
    out = bytearray(BLOCK_BYTES)  # bloc d'intro silence : initialise l'historique
    s1 = s2 = 0

    for frame in range(frame_count):
        chunk = mono[frame * BLOCK_SAMPLES:(frame + 1) * BLOCK_SAMPLES]
        best_score = None
        best = (0, 0, [0] * BLOCK_SAMPLES, 0, 0)

        for filt in range(len(FILTERS)):
            preds = []
            largest = 0
            h1, h2 = s1, s2
            for sample in chunk:
                pred = _predict(filt, h1, h2)
                preds.append(pred)
                largest = max(largest, abs(sample - pred))
                h2, h1 = h1, clamp16(pred)
            if largest == 0:
                shifts = (0,)
            else:
                base = max(0, min(12, 12 - math.ceil(math.log2((largest + 1) / 7.0))))
                shifts = (max(0, base - 1), base, min(12, base + 1))
            for shift in dict.fromkeys(shifts):
                step = 1 << (12 - shift)
                nibbles = []
                decoded_frame = []
                h1, h2 = s1, s2
                score = 0
                for sample in chunk:
                    pred = _predict(filt, h1, h2)
                    residual = sample - pred
                    q = int(math.floor(residual / float(step) + 0.5))
                    if q > 7:
                        q = 7
                    elif q < -8:
                        q = -8
                    decoded = clamp16(pred + q * step)
                    err = sample - decoded
                    score += err * err
                    nibbles.append(q & 0x0F)
                    decoded_frame.append(decoded)
                    h2, h1 = h1, decoded
                if best_score is None or score < best_score:
                    best_score = score
                    best = (filt, shift, nibbles, h1, h2)

        filt, shift, nibbles, s1, s2 = best
        flags = 0
        if loop:
            if frame == 0:
                flags = 0x04
            elif frame == frame_count - 1:
                flags = 0x02
        elif frame == frame_count - 1:
            flags = 0x01
        out.append((filt << 4) | shift)
        out.append(flags)
        for i in range(0, BLOCK_SAMPLES, 2):
            out.append(nibbles[i] | (nibbles[i + 1] << 4))

    return bytes(out)


def decode_vag(data: bytes) -> list[int]:
    """Decode VAG brut avec la meme arithmetique que le hardware (verification)."""
    if len(data) % BLOCK_BYTES:
        raise ValueError("taille VAG non multiple de 16")
    result = []
    s1 = s2 = 0
    for offset in range(0, len(data), BLOCK_BYTES):
        block = data[offset:offset + BLOCK_BYTES]
        shift = block[0] & 0x0F
        if shift > 12:
            shift = 9
        filt = (block[0] >> 4) & 0x07
        if filt >= len(FILTERS):
            filt = 0
        step = 1 << (12 - shift)
        for i in range(BLOCK_SAMPLES):
            byte = block[2 + i // 2]
            nibble = byte & 0x0F if (i & 1) == 0 else byte >> 4
            signed = (nibble ^ 8) - 8
            pred = _predict(filt, s1, s2)
            sample = clamp16((signed * step) + pred)
            result.append(sample)
            s2, s1 = s1, sample
    return result


def snr_db(reference: list[int], got: list[int]) -> float:
    n = min(len(reference), len(got))
    if n == 0:
        return 0.0
    signal = sum(reference[i] * reference[i] for i in range(n))
    error = sum((reference[i] - got[i]) ** 2 for i in range(n))
    if error == 0:
        return 120.0
    if signal == 0:
        return 0.0
    return 10.0 * math.log10(signal / error)


def read_wav_mono(path: str) -> tuple[list[int], int]:
    with wave.open(path, "rb") as wav:
        channels = wav.getnchannels()
        width = wav.getsampwidth()
        rate = wav.getframerate()
        raw = wav.readframes(wav.getnframes())
    if width != 2:
        raise ValueError(f"WAV 16 bits attendu: {path}")
    values = struct.unpack("<%dh" % (len(raw) // 2), raw)
    if channels == 1:
        return list(values), rate
    if channels == 2:
        return [(values[i] + values[i + 1]) // 2
                for i in range(0, len(values), 2)], rate
    raise ValueError(f"WAV mono/stereo attendu: {path}")


def write_wav_mono(path: str, samples: list[int], rate: int = TARGET_RATE) -> None:
    with wave.open(path, "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(rate)
        wav.writeframes(struct.pack("<%dh" % len(samples), *samples))


def main(argv: list[str]) -> int:
    if len(argv) < 2:
        print(__doc__)
        return 2
    command = argv[1]
    if command == "selftest":
        samples = [int(18000 * math.sin(2 * math.pi * 440 * i / TARGET_RATE))
                   for i in range(28 * 40)]
        blob = encode_pcm(samples, TARGET_RATE, True)
        value = snr_db(samples, decode_vag(blob)[28:28 + len(samples)])
        print(f"VAG_SELFTEST SNR={value:.1f}dB bytes={len(blob)}")
        return 0 if value > 20.0 else 1
    if command == "encode" and len(argv) >= 4:
        samples, rate = read_wav_mono(argv[2])
        data = encode_pcm(samples, rate, "--loop" in argv[4:])
        Path(argv[3]).write_bytes(data)
        print(f"VAG_ENCODE {argv[2]} -> {argv[3]} bytes={len(data)}")
        return 0
    if command == "decode" and len(argv) >= 4:
        data = Path(argv[2]).read_bytes()
        write_wav_mono(argv[3], decode_vag(data))
        print(f"VAG_DECODE {argv[2]} -> {argv[3]}")
        return 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
