"""Empaquette une livraison PS1 dans ps1/a_graver/<version>/.

Usage :
    py -3.12 tools/ps1_package_lot.py <version> "<resume une ligne>"

Copie BIN/CUE/EXE/ELF/MAP depuis ps1/build, convertit le BIN MODE2/2352 en
ISO 2048 (24 octets d'en-tete Mode 2 Form 1 retires par secteur), ecrit
BUILD_INFO.txt et SHA256.txt. Refuse d'ecraser une livraison existante :
les lots precedents restent figes.
"""
import hashlib
import os
import shutil
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD = os.path.join(REPO, 'ps1', 'build')
ARTS = ('alundra2_ps1.bin', 'alundra2_ps1.cue', 'alundra2.exe', 'alundra2.elf', 'alundra2.map')
RAW, DATA, HEADER = 2352, 2048, 24


def sha256(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest().upper()


def bin_to_iso(src, dst):
    size = os.path.getsize(src)
    if size % RAW:
        raise SystemExit(f'BIN non aligne sur 2352 : {size}')
    with open(src, 'rb') as fi, open(dst, 'wb') as fo:
        for _ in range(size // RAW):
            sector = fi.read(RAW)
            if sector[:12] != b'\x00' + b'\xff' * 10 + b'\x00':
                raise SystemExit('synchro de secteur invalide')
            fo.write(sector[HEADER:HEADER + DATA])
    return size // RAW


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    version, summary = sys.argv[1], sys.argv[2]
    out = os.path.join(REPO, 'ps1', 'a_graver', version)
    if os.path.exists(out):
        raise SystemExit(f'livraison deja presente, figee : {out}')
    os.makedirs(out)
    for name in ARTS:
        shutil.copy2(os.path.join(BUILD, name), os.path.join(out, name))
    iso = os.path.join(out, 'alundra2_ntsc.iso')
    sectors = bin_to_iso(os.path.join(out, 'alundra2_ps1.bin'), iso)
    files = ARTS + ('alundra2_ntsc.iso',)
    hashes = {n: sha256(os.path.join(out, n)) for n in files}
    with open(os.path.join(out, 'SHA256.txt'), 'w', encoding='utf-8', newline='\n') as f:
        for n in files:
            f.write(f'{hashes[n]}  {n}\n')
    with open(os.path.join(out, 'BUILD_INFO.txt'), 'w', encoding='utf-8', newline='\n') as f:
        f.write(f'PS1-{version}\n{"=" * (4 + len(version))}\n{summary}\n')
        f.write(f'ISO 2048 : {os.path.getsize(iso)} octets, {sectors} secteurs.\n')
    print(f'LOT {version} : ISO {os.path.getsize(iso)} octets / {sectors} secteurs')
    print(f'ISO_SHA256={hashes["alundra2_ntsc.iso"]}')
    print(f'ELF_SHA256={hashes["alundra2.elf"]}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
