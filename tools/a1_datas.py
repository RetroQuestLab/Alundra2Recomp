"""Analyse de DATA/DATAS.BIN d'Alundra 1.

Format observe : table de 512 offsets u32 (2048 octets) en tete ; le bloc i
va de offset[i] a offset[i+1]. Les entrees vides ont deux offsets egaux.

Usage :
    python tools/a1_datas.py info    <DATAS.BIN>
    python tools/a1_datas.py split   <DATAS.BIN> <dossier>
"""
import os
import struct
import sys


def entries(data):
    first = struct.unpack_from("<I", data, 0)[0]
    n = first // 4
    offs = list(struct.unpack_from(f"<{n}I", data, 0))
    offs = [o for o in offs if o]  # fin de table remplie de zeros
    offs.append(len(data))
    for i in range(len(offs) - 1):
        yield i, offs[i], offs[i + 1] - offs[i]


def main():
    data = open(sys.argv[2], "rb").read()
    if sys.argv[1] == "info":
        for i, off, size in entries(data):
            if size:
                head = data[off:off + 32].hex(" ")
                print(f"{i:4d} {off:#010x} {size:9d}  {head}")
    elif sys.argv[1] == "split":
        os.makedirs(sys.argv[3], exist_ok=True)
        for i, off, size in entries(data):
            if size:
                with open(os.path.join(sys.argv[3], f"{i:03d}.bin"), "wb") as f:
                    f.write(data[off:off + size])


if __name__ == "__main__":
    main()
