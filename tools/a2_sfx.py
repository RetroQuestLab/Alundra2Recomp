"""Bruitages d'Alundra 2 : banque d'effets PACK0 #595 (SShd) + #596 (ADPCM).
Rend chacun des 89 effets du jeu, tel que le pilote sonore les joue, dans assets/sfx/NN.wav
(NN = numero a plat) et liste les effets dans assets/sfx/sfx.txt (groupe, index, notes).

    python tools/a2_sfx.py extracted/PACK0.PAK assets/sfx

Format (deduit) :
  0x102 : u16 dernier groupe (7), puis u16 offsets des groupes (relatifs a 0x102).
          Groupe : u16 nombre-1, puis u16 offsets des effets (relatifs a 0x102).
  Effet : evenements suivis d'un delta (longueur variable) :
          Ax note velocite programme  (velocite 0 = arret de la note)
          Bx cc valeur(1 octet si cc < 0x40, sinon 2) 00 note   (controleurs, ignores ici)
          FF 2F 00 = fin.
  Programmes (a 0x9e8) : en-tete 8 octets (ff 7f 40 00 0c 7f note_min note_max) puis un ton de
          16 octets par note (kit de batterie : note -> ton note - note_min). Ton :
          u8 couche, u8 ?, u8 hauteur (60 = hauteur d'origine), i8 accord fin (1/16 demi-ton),
          u16 adresse/8, u16 ADSR1, u16 ADSR2, u8 0, u8 volume, u8 pan, ...
"""
import os
import struct
import sys
import wave

import numpy as np

sys.path.insert(0, os.path.dirname(__file__))
import a1_music as M  # noqa: E402

RATE = 44100
B = 0x102
TICK = 1 / 240  # duree d'un tick (estimee)


def load(pak):
    n = struct.unpack_from("<I", pak, 0)[0] // 4
    offs = list(struct.unpack_from("<%dI" % n, pak, 0)) + [len(pak)]
    return pak[offs[595]:offs[596]], pak[offs[596]:offs[597]]


def programs(hd):
    progs, o = [], 0x9e8
    while o + 8 <= len(hd):
        h = hd[o:o + 8]
        if h[4] != 0x0C or h[5] != 0x7F:
            break
        lo, hi = h[6], h[7]
        tones = [hd[o + 8 + 16 * k:o + 24 + 16 * k] for k in range(hi - lo + 1)]
        progs.append((lo, tones))
        o += 8 + 16 * (hi - lo + 1)
    return progs


def parse(hd, p):
    ev, t = [], 0
    while p + 1 < len(hd):
        st = hd[p]
        if st == 0xFF:
            if hd[p + 1] == 0x2F:
                break
            p += 3
            continue
        if st & 0xF0 == 0xA0:
            ev.append((t, hd[p + 1], hd[p + 2], hd[p + 3]))
            p += 4
        elif st & 0xF0 == 0xB0:
            p += 2 + (2 if hd[p + 1] >= 0x40 else 1) + 2
        else:
            break
        d = 0
        while p < len(hd):
            c = hd[p]
            p += 1
            d = (d << 7) | (c & 0x7F)
            if not c & 0x80:
                break
        t += d
    return ev


def effects(hd):
    out = []
    last = struct.unpack_from("<H", hd, B)[0]
    for g in range(last + 1):
        go = B + struct.unpack_from("<H", hd, B + 2 + 2 * g)[0]
        for i in range(struct.unpack_from("<H", hd, go)[0] + 1):
            out.append((g, i, parse(hd, B + struct.unpack_from("<H", hd, go + 2 + 2 * i)[0])))
    return out


class Samples:
    def __init__(self, bd):
        self.bd, self.cache = bd, {}

    def get(self, a):
        if a not in self.cache:
            end = a
            while end + 16 <= len(self.bd):
                end += 16
                if self.bd[end - 15] & 1:
                    break
            s, loop, _ = M.decode_vag(self.bd[a:end])
            self.cache[a] = (np.asarray(s, np.float32), loop)
        return self.cache[a]


def voice(samples, tone, key, vel, hold):
    """Une note : echantillon a sa hauteur, boucle tant que tenue, enveloppe ADSR."""
    root, fine = tone[2], (tone[3] ^ 0x80) - 0x80
    addr = struct.unpack_from("<H", tone, 4)[0] * 8
    adsr1, adsr2 = struct.unpack_from("<HH", tone, 6)
    s, loop = samples.get(addr)
    if len(s) < 2:
        return np.zeros((0, 2), np.float32)
    ratio = 2 ** ((key - root + fine / 16) / 12)  # comme un ton de VAB : note jouee - note racine
    maxlen = int(RATE * 3)
    # longueur lue dans l'echantillon source
    if loop is None or loop >= len(s) - 1:
        n_out = min(maxlen, int((len(s) - 1) / ratio))
        held = n_out
    else:
        held = int(hold * RATE) if hold else int(RATE * 0.6)
        n_out = min(maxlen, held + RATE)
    pos = np.arange(n_out) * ratio
    if loop is not None and loop < len(s) - 1:
        L = len(s) - 1 - loop
        pos = np.where(pos >= len(s) - 1, loop + (pos - loop) % L, pos)
    pos = np.minimum(pos, len(s) - 1.001)
    k = pos.astype(np.int64)
    f = pos - k
    x = s[k] * (1 - f) + s[np.minimum(k + 1, len(s) - 1)] * f
    blocks = n_out // 32 + 1
    env = M.envelope(adsr1, adsr2, min(blocks, held // 32 + 1), blocks)
    e = np.repeat(env, 32)
    e = np.pad(e, (0, max(0, n_out - len(e))))[:n_out]
    x = x * e * (tone[11] / 127) * (vel / 127)
    pan = tone[12] / 127
    return np.stack([x * min(1, 2 * (1 - pan)), x * min(1, 2 * pan)], 1)


def render(hd, progs, samples, ev):
    notes = []
    for idx, (t, key, vel, prog) in enumerate(ev):
        if vel == 0 or prog >= len(progs):
            continue
        lo, tones = progs[prog]
        if not 0 <= key - lo < len(tones):
            continue
        off = next((t2 for t2, k2, v2, p2 in ev[idx + 1:] if k2 == key and v2 == 0), None)
        notes.append((t * TICK, tones[key - lo], key, vel, (off - t) * TICK if off is not None else None))
    if not notes:
        return np.zeros((1, 2), np.float32)
    parts = [(int(t * RATE), voice(samples, tone, key, vel, hold)) for t, tone, key, vel, hold in notes]
    mix = np.zeros((max(o + len(v) for o, v in parts) + 1, 2), np.float32)
    for o, v in parts:
        mix[o:o + len(v)] += v
    # coupe le silence final
    loud = np.nonzero(np.abs(mix).max(1) > 1e-3)[0]
    return mix[:loud[-1] + 1] if len(loud) else mix


def main():
    hd, bd = load(open(sys.argv[1], "rb").read())
    out = sys.argv[2]
    os.makedirs(out, exist_ok=True)
    progs, samples = programs(hd), Samples(bd)
    # table systeme du jeu (PACK0 #597) : identifiant -> (groupe, effet) ; c'est ce numero que le code
    # passe a PlaySe (0x8002e024 dans SLES_026.01 ; < 200 = banque systeme, sinon banque de la zone)
    pak = open(sys.argv[1], "rb").read()
    n0 = struct.unpack_from("<I", pak, 0)[0] // 4
    offs = list(struct.unpack_from("<%dI" % n0, pak, 0)) + [len(pak)]
    tab = pak[offs[597]:offs[598]]
    ids = {}
    for o in range(0, len(tab) - 7, 8):
        sid, g, i = struct.unpack_from("<hBB", tab, o)
        ids.setdefault((g, i), sid)
    lines = []
    for n, (g, i, ev) in enumerate(effects(hd)):
        if (g, i) not in ids:
            continue
        n = ids[(g, i)]
        mix = render(hd, progs, samples, ev)
        peak = float(np.abs(mix).max()) or 1.0
        mix = mix / peak * 0.9
        with wave.open(os.path.join(out, f"{n:02d}.wav"), "wb") as w:
            w.setnchannels(2)
            w.setsampwidth(2)
            w.setframerate(RATE)
            w.writeframes((mix * 32767).astype("<i2").tobytes())
        lines.append(f"id {n:02d} groupe {g} effet {i:2d}  {len(mix) / RATE:5.2f} s  notes "
                     + " ".join(f"{k}/p{p}" for t, k, v, p in ev if v))
        print(lines[-1])
    open(os.path.join(out, "sfx.txt"), "w").write("\n".join(sorted(lines)) + "\n")


if __name__ == "__main__":
    main()
