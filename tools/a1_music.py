"""Musiques d'Alundra 1 : SOUND.BIN contient des paires SEQ (partition, facon MIDI de Sony)
+ VAB (instruments : programmes, tons, echantillons VAG en ADPCM PS1).

On fait le rendu hors-ligne de chaque morceau en WAV (synthese par echantillons, enveloppes ADSR
de la puce son de la PS1), avec un fichier .loop qui donne l'echantillon de debut de boucle.

    python tools/a1_music.py <SOUND.BIN> assets/music [numero ...]
"""
import os
import re
import struct
import sys
import wave

import numpy as np

RATE = 44100
MUSIC_START = 5916672  # avant : effets sonores ; apres : paires SEQ + VAB des musiques
MAX_SECONDS = 300

# --- ADPCM PS1 -------------------------------------------------------------------------------
FILTERS = [(0, 0), (60, 0), (115, -52), (98, -55), (122, -60)]


def decode_vag(data):
    """Renvoie (echantillons float32, debut de boucle ou None, fin)."""
    out = []
    s1 = s2 = 0
    loop_start = None
    end = None
    for blk in range(len(data) // 16):
        b = data[blk * 16:blk * 16 + 16]
        shift, filt = b[0] & 15, min(4, b[0] >> 4)
        flags = b[1]
        if flags & 4 and loop_start is None:
            loop_start = blk * 28
        f0, f1 = FILTERS[filt]
        for byte in b[2:]:
            for nib in (byte & 15, byte >> 4):
                v = nib - 16 if nib >= 8 else nib
                v = (v << 12) >> shift
                v += (s1 * f0 + s2 * f1 + 32) >> 6
                v = max(-32768, min(32767, v))
                out.append(v)
                s2, s1 = s1, v
        if flags & 1:
            end = (blk + 1) * 28
            if not flags & 2:
                loop_start = None
            break
    pcm = np.array(out, dtype=np.float32) / 32768.0
    return pcm, loop_start, end or len(pcm)


# --- VAB -------------------------------------------------------------------------------------
class Vab:
    def __init__(self, d, o):
        _, _, _, fsize, _, ps, ts, vs = struct.unpack_from("<4sIIIHHHH", d, o)
        self.mvol = d[o + 28] if d[o + 28] else 127
        prog = o + 32
        self.progs = {}
        used = []
        for p in range(128):
            tones, mvol, _, _, mpan = struct.unpack_from("<5B", d, prog + p * 16)
            if tones:
                self.progs[p] = {"vol": mvol, "pan": mpan, "tones": []}
                used.append(p)
        tone_base = prog + 128 * 16
        for k, p in enumerate(used):
            for t in range(16):
                q = tone_base + (k * 16 + t) * 32
                (prior, mode, vol, pan, center, shift, mn, mx, _, _, _, _, pbmin, pbmax, _, _,
                 adsr1, adsr2, tprog, vag) = struct.unpack_from("<16BHHhh", d, q)
                if t < self.progs[p]["tones"].__len__() or vag <= 0:
                    continue
                self.progs[p]["tones"].append(dict(vol=vol, pan=pan, center=center, shift=shift,
                                                   min=mn, max=mx, adsr1=adsr1, adsr2=adsr2,
                                                   vag=vag, pbmax=pbmax))
        sizes_off = tone_base + ps * 16 * 32
        sizes = [v << 3 for v in struct.unpack_from("<256H", d, sizes_off)]
        pos = sizes_off + 512
        self.vags = {}
        for i in range(1, vs + 1):
            self.vags[i] = (pos, sizes[i])
            pos += sizes[i]
        self.d = d
        self.cache = {}

    def sample(self, i):
        if i not in self.cache:
            p, n = self.vags.get(i, (0, 0))
            self.cache[i] = decode_vag(self.d[p:p + n]) if n else (np.zeros(28, np.float32), None, 28)
        return self.cache[i]


# --- Enveloppe ADSR de la SPU (simulee par paquets de 32 echantillons) ----------------------
BLOCK = 32


def env_step(level, rate, increase, exponential):
    shift = rate >> 2
    step = (7 - (rate & 3)) if increase else (-8 + (rate & 3))
    adsr_step = step << max(0, 11 - shift)
    cycles = 1 << max(0, shift - 11)
    if increase and exponential and level > 0x6000:
        cycles *= 4
    if not increase and exponential:
        adsr_step = (adsr_step * level) >> 15
    return level + adsr_step * BLOCK / cycles


def envelope(adsr1, adsr2, held, total):
    """Enveloppe (float, 1 valeur par paquet) ; held/total en paquets."""
    ar, dr, sl = (adsr1 >> 8) & 0x7F, (adsr1 >> 4) & 0xF, adsr1 & 0xF
    a_exp = bool(adsr1 & 0x8000)
    s_exp, s_dec, sr = bool(adsr2 & 0x8000), bool(adsr2 & 0x4000), (adsr2 >> 6) & 0x7F
    r_exp, rr = bool(adsr2 & 0x20), adsr2 & 0x1F
    sus_level = (sl + 1) * 0x800
    out = np.zeros(total, np.float32)
    level, phase = 0.0, 0
    for k in range(total):
        if k >= held and phase < 3:
            phase = 3
        if phase == 0:
            level = env_step(int(level), ar, True, a_exp)
            if level >= 0x7FFF:
                level, phase = 0x7FFF, 1
        elif phase == 1:
            level = env_step(int(level), dr * 4, False, True)
            if level <= sus_level:
                level, phase = sus_level, 2
        elif phase == 2:
            level = env_step(int(level), sr, not s_dec, s_exp)
        else:
            level = env_step(int(level), rr * 4, False, r_exp)
        level = max(0.0, min(0x7FFF, level))
        out[k] = level / 0x7FFF
        if phase == 3 and level <= 0:
            return out[:k + 1]
    return out


# --- SEQ -------------------------------------------------------------------------------------
def parse_seq(d, o):
    res = struct.unpack_from(">H", d, o + 8)[0]
    tempo = int.from_bytes(d[o + 10:o + 13], "big")
    p = o + 15
    t = 0
    status = 0
    events = []  # (tick, type, ch, a, b)
    while p < len(d):
        delta = 0
        while True:
            c = d[p]
            p += 1
            delta = (delta << 7) | (c & 0x7F)
            if not c & 0x80:
                break
        t += delta
        c = d[p]
        if c & 0x80:
            status = c
            p += 1
        typ, ch = status & 0xF0, status & 0x0F
        if status == 0xFF:
            meta = d[p]
            p += 1
            if meta == 0x51:
                events.append((t, "tempo", 0, int.from_bytes(d[p:p + 3], "big"), 0))
                p += 3
            elif meta == 0x2F:
                events.append((t, "end", 0, 0, 0))
                break
            continue
        if typ in (0x80, 0x90, 0xA0, 0xB0, 0xE0):
            a, b = d[p], d[p + 1]
            p += 2
            events.append((t, typ, ch, a, b))
        elif typ in (0xC0, 0xD0):
            events.append((t, typ, ch, d[p], 0))
            p += 1
        else:
            break
    return res, tempo, events


def render(seq_d, seq_o, vab):
    res, tempo, events = parse_seq(seq_d, seq_o)
    return render_events(res, tempo, events, vab)


def render_events(res, tempo, events, vab):
    # temps reel de chaque tick
    sec, last_tick, cur_tempo = 0.0, 0, tempo
    times = []
    loop_start = loop_end = None
    nrpn = {}
    for t, typ, ch, a, b in events:
        sec += (t - last_tick) * cur_tempo / 1e6 / res
        last_tick = t
        times.append(sec)
        if typ == "tempo":
            cur_tempo = a
        if typ == 0xB0:
            if a == 99:
                nrpn[ch] = b
            elif a == 6 and nrpn.get(ch) == 20 and loop_start is None:
                loop_start = sec
            elif a == 6 and nrpn.get(ch) == 30:
                loop_end = sec
    total = min(MAX_SECONDS, (loop_end or sec) + 2.0)
    n = int(total * RATE)
    mix = np.zeros((n + RATE * 4, 2), np.float32)
    chan = {c: dict(prog=0, vol=100, pan=64, expr=127, bend=0) for c in range(16)}
    active = {}
    notes = []
    for (t, typ, ch, a, b), s in zip(events, times):
        if loop_end is not None and s > loop_end + 0.001:
            break
        st = chan[ch]
        if typ == 0xC0:
            st["prog"] = a
        elif typ == 0xB0:
            if a == 7:
                st["vol"] = b
            elif a == 10:
                st["pan"] = b
            elif a == 11:
                st["expr"] = b
        elif typ == 0xE0:
            st["bend"] = ((b << 7) | a) - 8192
        elif typ == 0x90 and b > 0:
            key = (ch, a)
            if key in active:
                notes.append(active.pop(key) + (s,))
            active[key] = (s, ch, a, b, dict(st))
        elif typ == 0x80 or (typ == 0x90 and b == 0):
            if (ch, a) in active:
                notes.append(active.pop((ch, a)) + (s,))
    for k, v in active.items():
        notes.append(v + (total,))
    for start, ch, key, vel, st, stop in notes:
        prog = vab.progs.get(st["prog"])
        if not prog:
            continue
        for tone in prog["tones"]:
            if not tone["min"] <= key <= tone["max"]:
                continue
            pcm, lstart, lend = vab.sample(tone["vag"])
            pitch = 2 ** ((key - tone["center"] + tone["shift"] / 128.0) / 12.0)
            pitch *= 2 ** (st["bend"] / 8192.0 * tone["pbmax"] / 12.0) if tone["pbmax"] else 1
            held = max(1, int((stop - start) * RATE / BLOCK))
            env = envelope(tone["adsr1"], tone["adsr2"], held, held + int(RATE * 3 / BLOCK))
            length = len(env) * BLOCK
            pos = np.arange(length, dtype=np.float64) * pitch
            if lstart is not None and lend > lstart:
                over = pos >= lend
                pos[over] = lstart + np.mod(pos[over] - lstart, lend - lstart)
            else:
                length = min(length, int((lend - 1) / pitch))
                pos = pos[:length]
            if length <= 0:
                continue
            i0 = pos.astype(np.int64)
            frac = (pos - i0).astype(np.float32)
            i1 = np.minimum(i0 + 1, len(pcm) - 1)
            wav = pcm[np.minimum(i0, len(pcm) - 1)] * (1 - frac) + pcm[i1] * frac
            wav *= np.repeat(env, BLOCK)[:length]
            g = (vel / 127) * (tone["vol"] / 127) * (prog["vol"] / 127) * (st["vol"] / 127) * \
                (st["expr"] / 127) * (vab.mvol / 127)
            pan = max(0.0, min(1.0, (st["pan"] - 64 + tone["pan"] - 64 + prog["pan"] - 64 + 64) / 127))
            s0 = int(start * RATE)
            if s0 >= n:
                continue
            seg = wav[:max(0, min(length, len(mix) - s0))]
            mix[s0:s0 + len(seg), 0] += seg * g * np.cos(pan * np.pi / 2)
            mix[s0:s0 + len(seg), 1] += seg * g * np.sin(pan * np.pi / 2)
    mix = mix[:n]
    if loop_end is not None and loop_start is not None:
        # la queue des notes jouees apres la fin de boucle revient au debut de la boucle
        mix = mix[:int(loop_end * RATE)]
    peak = np.max(np.abs(mix)) or 1.0
    mix *= 0.9 / peak
    return mix, (int(loop_start * RATE) if loop_start is not None else None)


def main():
    d = open(sys.argv[1], "rb").read()
    out = sys.argv[2]
    os.makedirs(out, exist_ok=True)
    seqs = [m.start() for m in re.finditer(b"pQES", d) if m.start() >= MUSIC_START]
    wanted = {int(a) for a in sys.argv[3:]}
    for n, so in enumerate(seqs):
        if wanted and n not in wanted:
            continue
        vo = d.find(b"pBAV", so)
        mix, loop = render(d, so, Vab(d, vo))
        path = os.path.join(out, f"{n:02d}.wav")
        with wave.open(path, "wb") as w:
            w.setnchannels(2)
            w.setsampwidth(2)
            w.setframerate(RATE)
            w.writeframes((mix * 32767).astype("<i2").tobytes())
        with open(path[:-4] + ".loop", "w") as f:
            f.write(str(loop if loop is not None else 0))
        print(f"{n:02d} : {len(mix) / RATE:.1f} s, boucle {loop}")


if __name__ == "__main__":
    main()


