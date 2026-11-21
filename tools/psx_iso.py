"""Lecture d'une image disque PS1 brute (.bin, secteurs de 2352 octets, mode 2).

    python tools/psx_iso.py <image.bin>                 liste les fichiers
    python tools/psx_iso.py <image.bin> <dossier>       extrait les fichiers (donnees mode 2 form 1 ;
                                                        les fichiers XA sont gardes en secteurs bruts 2336)
"""
import os
import struct
import sys

SECTOR = 2352


class Disc:
    def __init__(self, path):
        self.f = open(path, "rb")

    def raw(self, lba):
        self.f.seek(lba * SECTOR)
        return self.f.read(SECTOR)

    def data(self, lba):
        return self.raw(lba)[24:24 + 2048]

    def files(self):
        pvd = self.data(16)
        root = pvd[156:156 + 34]
        out = []
        self._walk(struct.unpack_from("<I", root, 2)[0], struct.unpack_from("<I", root, 10)[0], "", out)
        return out

    def _walk(self, lba, size, prefix, out):
        buf = b"".join(self.data(lba + i) for i in range((size + 2047) // 2048))
        p = 0
        while p < len(buf):
            n = buf[p]
            if n == 0:
                p = (p // 2048 + 1) * 2048
                continue
            rec = buf[p:p + n]
            elba, esize = struct.unpack_from("<I", rec, 2)[0], struct.unpack_from("<I", rec, 10)[0]
            flags, nl = rec[25], rec[32]
            name = rec[33:33 + nl]
            p += n
            if name in (b"\x00", b"\x01"):
                continue
            name = name.decode("ascii", "replace").split(";")[0]
            # attributs XA (form 2 / entrelace) apres le nom
            xa = rec[33 + nl + (1 - nl % 2):]
            is_xa = len(xa) >= 8 and xa[6:8] == b"XA" and bool(struct.unpack_from(">H", xa, 4)[0] & 0x3000)
            if flags & 2:
                self._walk(elba, esize, prefix + name + "/", out)
            else:
                out.append((prefix + name, elba, esize, is_xa))


def main():
    disc = Disc(sys.argv[1])
    files = disc.files()
    if len(sys.argv) < 3:
        for name, lba, size, xa in files:
            print(f"{lba:7d} {size:10d} {'XA' if xa else '  '} {name}")
        return
    for name, lba, size, xa in files:
        path = os.path.join(sys.argv[2], name)
        os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
        with open(path, "wb") as f:
            n = (size + 2047) // 2048
            for i in range(n):
                f.write(disc.raw(lba + i)[16:16 + 2336] if xa else disc.data(lba + i))
        print(name)


if __name__ == "__main__":
    main()
