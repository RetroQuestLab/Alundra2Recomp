"""Lecture d'images CD PS1 brutes (secteurs de 2352 octets) et de leur systeme ISO9660.

Usage :
    python tools/discfs.py list    <image>             liste les fichiers
    python tools/discfs.py extract <image> <dossier>   extrait tous les fichiers
"""
import os
import struct
import sys

RAW = 2352
DATA = 2048


class Disc:
    def __init__(self, path):
        self.f = open(path, "rb")
        size = os.path.getsize(path)
        self.raw = size % RAW == 0 and self._has_sync()
        self.sector_size = RAW if self.raw else DATA

    def _has_sync(self):
        self.f.seek(0)
        return self.f.read(12) == b"\x00" + b"\xff" * 10 + b"\x00"

    def read_sector(self, lba):
        self.f.seek(lba * self.sector_size)
        s = self.f.read(self.sector_size)
        if not self.raw:
            return s
        mode = s[15]
        off = 16 if mode == 1 else 24  # mode 2 XA : 8 octets de sous-en-tete
        return s[off:off + DATA]

    def read(self, lba, length):
        out = bytearray()
        while len(out) < length:
            out += self.read_sector(lba)
            lba += 1
        return bytes(out[:length])

    def walk(self):
        """Renvoie (chemin, lba, taille) pour chaque fichier."""
        pvd = self.read_sector(16)
        if pvd[1:6] != b"CD001":
            raise ValueError("Systeme de fichiers ISO9660 introuvable")
        root = pvd[156:156 + 34]
        lba, size = struct.unpack_from("<I", root, 2)[0], struct.unpack_from("<I", root, 10)[0]
        yield from self._walk_dir(lba, size, "")

    def _walk_dir(self, lba, size, prefix):
        data = self.read(lba, size)
        pos = 0
        while pos < len(data):
            n = data[pos]
            if n == 0:  # fin de secteur : saut au suivant
                pos = (pos // DATA + 1) * DATA
                continue
            rec = data[pos:pos + n]
            pos += n
            elba = struct.unpack_from("<I", rec, 2)[0]
            esize = struct.unpack_from("<I", rec, 10)[0]
            flags = rec[25]
            name = rec[33:33 + rec[32]]
            if name in (b"\x00", b"\x01"):
                continue
            name = name.decode("ascii", "replace").split(";")[0]
            path = f"{prefix}/{name}"
            if flags & 2:
                yield from self._walk_dir(elba, esize, path)
            else:
                yield path, elba, esize


def main():
    if len(sys.argv) < 3 or sys.argv[1] not in ("list", "extract"):
        print(__doc__)
        sys.exit(1)
    disc = Disc(sys.argv[2])
    if sys.argv[1] == "list":
        total = 0
        for path, lba, size in disc.walk():
            print(f"{lba:8d} {size:12d}  {path}")
            total += size
        print(f"Total : {total / 1048576:.1f} Mo")
    else:
        dest = sys.argv[3]
        for path, lba, size in disc.walk():
            out = os.path.join(dest, path.lstrip("/"))
            os.makedirs(os.path.dirname(out), exist_ok=True)
            with open(out, "wb") as f:
                f.write(disc.read(lba, size))
            print("extrait", path)


if __name__ == "__main__":
    main()
