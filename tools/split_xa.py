"""Decoupe les flux XA decodes d'Alundra 2 (assets/music_a2/st_CC_00.wav, un par canal) en morceaux,
aux silences d'au moins 1 s. Ecrit assets/music_a2/tracks/NN.wav et la liste tracks.txt.

    python tools/split_xa.py assets/music_a2
"""
import glob
import os
import sys
import wave

import numpy as np

QUIET = 60        # niveau RMS considere comme silence
MIN_GAP = 1.0     # secondes
MIN_LEN = 3.0     # en dessous : ignore (bruits)


def main():
    root = sys.argv[1]
    out = os.path.join(root, "tracks")
    os.makedirs(out, exist_ok=True)
    lines = []
    n = 0
    for path in sorted(glob.glob(os.path.join(root, "st_*.wav"))):
        w = wave.open(path)
        rate = w.getframerate()
        a = np.frombuffer(w.readframes(w.getnframes()), "<i2").reshape(-1, 2)
        win = rate // 20
        k = len(a) // win
        rms = np.sqrt((a[:k * win].astype(np.float32) ** 2).mean(1).reshape(k, win).mean(1))
        loud = rms >= QUIET
        segs, start, gap = [], None, 0
        for i, on in enumerate(loud):
            if on:
                if start is None:
                    start = i
                elif gap >= MIN_GAP * 20:
                    segs.append((start, i - gap))
                    start = i
                gap = 0
            else:
                gap += 1
        if start is not None:
            segs.append((start, k - gap))
        for s, e in segs:
            if (e - s) / 20 < MIN_LEN:
                continue
            name = os.path.join(out, f"{n:02d}.wav")
            with wave.open(name, "wb") as o:
                o.setnchannels(2)
                o.setsampwidth(2)
                o.setframerate(rate)
                o.writeframes(a[s * win:e * win].tobytes())
            lines.append(f"{n:02d} {os.path.basename(path)} {s / 20:.1f}s-{e / 20:.1f}s ({(e - s) / 20:.0f} s)")
            n += 1
    open(os.path.join(root, "tracks.txt"), "w").write("\n".join(lines) + "\n")
    print("\n".join(lines))


if __name__ == "__main__":
    main()
