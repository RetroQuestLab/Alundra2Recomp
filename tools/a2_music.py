"""Musiques d'Alundra 2 (BGM sequencees) : DATA/PACK0.PAK contient des trios
  SSsq (partition, quasi-MIDI : evenement PUIS delta), SShd (programmes / tons), banque ADPCM brute.
Rendu hors-ligne en WAV avec le synthetiseur de tools/a1_music.py.

    python tools/a2_music.py extracted/PACK0.PAK assets/music [numero ...]

Formats deduits :
  SSsq : u16 volume, u16 resolution, u16 tempo (BPM), 6 octets, 'SSsq', 16 x 16 octets (canaux),
         puis evenements : statut + donnees, puis delta (longueur variable). FF 51 = tempo (u16 BPM),
         FF 2F = fin. Pitch bend sur 1 octet. Boucle : NRPN 99=20 (debut) / 99=30 (fin).
  SShd : u32 taille, u32 taille banque, 4 octets, 'SShd', u32 offset programmes (0x80) ...
         a 0x80 : u16 dernier programme, puis u16 offsets (relatifs a 0x80) des programmes.
         Programme : u8 nb_tons-1, u8 volume, u8 pan, 5 octets ; puis tons de 16 octets :
         u8 note min, u8 note max, u8 note de base, i8 accord fin (1/16 de demi-ton), u16 adresse/8 dans la banque,
         u16 ADSR1, u16 ADSR2, u8 0, u8 volume, u8 pan, u8 ?, u8 ?, u8 ?.
"""
import os
import struct
import sys
import wave

import numpy as np

sys.path.insert(0, os.path.dirname(__file__))
import a1_music as M  # noqa: E402


class Bank:
    def __init__(self, hd, bd):
        self.bd = bd
        self.mvol = 127
        self.cache = {}
        last = struct.unpack_from("<H", hd, 0x80)[0]
        self.progs = {}
        starts = [0x80 + struct.unpack_from("<H", hd, 0x82 + p * 2)[0] for p in range(last + 1)]
        tables = struct.unpack_from("<I", hd, 0x14)[0]  # debut de la section suivante
        for p in range(last + 1):
            po = starts[p]
            if po + 8 > len(hd):
                continue
            nxt = min([s for s in starts if s > po] + [tables])
            n = max(0, (nxt - po - 8) // 16)  # l'octet 0 porte des drapeaux (0x80...)
            prog = {"vol": hd[po + 1], "pan": hd[po + 2], "tones": []}
            for k in range(n):
                t = hd[po + 8 + k * 16:po + 24 + k * 16]
                if len(t) < 16:
                    break
                mn, mx, root, _fine, addr, adsr1, adsr2 = struct.unpack_from("<4BHHH", t, 0)
                prog["tones"].append(dict(min=mn, max=mx, center=root, shift=((_fine ^ 0x80) - 0x80) * 8, vag=addr * 8,
                                          adsr1=adsr1, adsr2=adsr2, vol=t[11], pan=t[12], pbmax=2, b3=_fine))
            self.progs[p] = prog

    def sample(self, off):
        if off not in self.cache:
            end = off
            while end + 16 <= len(self.bd):
                end += 16
                if self.bd[end - 15] & 1:
                    break
            self.cache[off] = M.decode_vag(self.bd[off:end])
        return self.cache[off]


def parse_ssq(x):
    res = struct.unpack_from("<H", x, 2)[0]
    bpm = struct.unpack_from("<H", x, 4)[0] or 120
    p, t, events = 272, 0, []
    status = 0
    while p < len(x):
        if x[p] & 0x80:
            st = x[p]
            if st != 0xFF:
                status = st
        else:  # running status : on reutilise le dernier statut
            st = status
            p -= 1
        if st == 0xFF:
            meta = x[p + 1]
            if meta == 0x2F:
                events.append((t, "end", 0, 0, 0))
                break
            if meta == 0x51:
                events.append((t, "tempo", 0, int(60e6 / max(1, struct.unpack_from("<H", x, p + 2)[0])), 0))
                p += 4
            else:
                p += 2
        else:
            typ, ch = st & 0xF0, st & 0x0F
            if typ in (0x80, 0x90, 0xA0, 0xB0):
                events.append((t, typ, ch, x[p + 1], x[p + 2]))
                p += 3
            elif typ in (0xC0, 0xD0):
                events.append((t, typ, ch, x[p + 1], 0))
                p += 2
            elif typ == 0xE0:  # pitch bend sur un octet (0x40 = centre)
                v = (x[p + 1] << 7) - 8192
                events.append((t, typ, ch, (v + 8192) & 0x7F, (v + 8192) >> 7))
                p += 2
            else:
                break
        delta = 0
        while p < len(x):
            c = x[p]
            p += 1
            delta = (delta << 7) | (c & 0x7F)
            if not c & 0x80:
                break
        t += delta
    return res, int(60e6 / bpm), events


def songs(pak):
    n = struct.unpack_from("<I", pak, 0)[0] // 4
    offs = list(struct.unpack_from("<%dI" % n, pak, 0)) + [len(pak)]
    ent = [pak[offs[i]:offs[i + 1]] for i in range(n)]
    kind = ["sq" if e[12:16] == b"SSsq" else "hd" if e[12:16] == b"SShd" else "" for e in ent]
    out = []
    for i, k in enumerate(kind):
        if k != "sq":
            continue
        j = next((j for j in range(i + 1, min(n, i + 4)) if kind[j] == "hd"), None)
        if j is None:
            j = next((j for j in range(i - 1, max(0, i - 6), -1) if kind[j] == "hd"), None)
        if j is not None:
            out.append((i, ent[i], ent[j], ent[j + 1]))
    return out


def main():
    pak = open(sys.argv[1], "rb").read()
    out = sys.argv[2]
    os.makedirs(out, exist_ok=True)
    wanted = {int(a) for a in sys.argv[3:]}
    lines = []
    for n, (idx, sq, hd, bd) in enumerate(songs(pak)):
        if wanted and n not in wanted:
            continue
        res, tempo, events = parse_ssq(sq)
        mix, loop = M.render_events(res, tempo, events, Bank(hd, bd))
        path = os.path.join(out, f"{n:02d}.wav")
        with wave.open(path, "wb") as w:
            w.setnchannels(2)
            w.setsampwidth(2)
            w.setframerate(M.RATE)
            w.writeframes((mix * 32767).astype("<i2").tobytes())
        with open(path[:-4] + ".loop", "w") as f:
            f.write(str(loop or 0))
        line = f"{n:02d} (PACK0 #{idx}) {len(mix) / M.RATE:.1f} s, boucle {loop}"
        lines.append(line)
        print(line, flush=True)
    if not wanted:
        open(os.path.join(out, "tracks.txt"), "w").write("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()








