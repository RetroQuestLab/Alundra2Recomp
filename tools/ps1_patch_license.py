"""Ajoute la licence (secteurs 0-15) a une livraison PS1 existante.

Usage :
    py -3.12 tools/ps1_patch_license.py <lot_source> <lot_cible> <disque_original.bin>

Copie ps1/a_graver/<lot_source> vers ps1/a_graver/<lot_cible>, remplace les
16 premiers secteurs bruts (2352 octets : synchro, en-tete MSF, sous-en-tete,
donnees, EDC/ECC) du BIN par ceux du disque ORIGINAL de l'utilisateur (memes
LBA 0-15, donc en-tetes et codes correcteurs valides tels quels), regenere
l'ISO 2048 et les empreintes. Tous les autres secteurs restent identiques,
octet pour octet (verifie). Une vraie PS1 reste bloquee sur le logo sans ces
secteurs ; ils ne sont jamais publies (ps1/a_graver est hors depot).
"""
import hashlib
import os
import shutil
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RAW, N = 2352, 16


def sha(p):
    h = hashlib.sha256()
    with open(p, 'rb') as f:
        for c in iter(lambda: f.read(1 << 20), b''):
            h.update(c)
    return h.hexdigest().upper()


def main():
    if len(sys.argv) != 4:
        print(__doc__)
        return 2
    src = os.path.join(REPO, 'ps1', 'a_graver', sys.argv[1])
    dst = os.path.join(REPO, 'ps1', 'a_graver', sys.argv[2])
    disc = sys.argv[3]
    if os.path.exists(dst):
        raise SystemExit(f'lot deja present : {dst}')
    with open(disc, 'rb') as f:
        lic = f.read(N * RAW)
    if b'Licensed' not in lic[4 * RAW:5 * RAW]:
        raise SystemExit('secteur 4 du disque original sans texte de licence')
    os.makedirs(dst)
    for name in ('alundra2_ps1.cue', 'alundra2.exe', 'alundra2.elf', 'alundra2.map'):
        shutil.copy2(os.path.join(src, name), os.path.join(dst, name))
    data = bytearray(open(os.path.join(src, 'alundra2_ps1.bin'), 'rb').read())
    before = bytes(data)
    data[:N * RAW] = lic
    assert data[N * RAW:] == before[N * RAW:], 'secteurs >= 16 modifies'
    open(os.path.join(dst, 'alundra2_ps1.bin'), 'wb').write(data)
    with open(os.path.join(dst, 'alundra2_ntsc.iso'), 'wb') as iso:
        for n in range(len(data) // RAW):
            s = data[n * RAW:(n + 1) * RAW]
            assert s[:12] == b'\x00' + b'\xff' * 10 + b'\x00'
            iso.write(s[24:24 + 2048])
    files = ('alundra2_ps1.bin', 'alundra2_ps1.cue', 'alundra2.exe', 'alundra2.elf',
             'alundra2.map', 'alundra2_ntsc.iso')
    with open(os.path.join(dst, 'SHA256.txt'), 'w', encoding='utf-8', newline='\n') as f:
        for n in files:
            f.write(f'{sha(os.path.join(dst, n))}  {n}\n')
    with open(os.path.join(dst, 'BUILD_INFO.txt'), 'w', encoding='utf-8', newline='\n') as f:
        f.write(f'PS1-{sys.argv[2]}\n')
        f.write(f'Image {sys.argv[1]} + secteurs de licence 0-15 du disque original '
                f'({os.path.basename(disc)}). Secteurs >= 16 identiques octet pour octet.\n')
        f.write('A GRAVER : alundra2_ps1.cue (avec alundra2_ps1.bin), mode DAO / Disc-At-Once, '
                'vitesse lente. Ne pas graver l ISO 2048.\n')
    print('LICENCE :', lic[4 * RAW + 24:4 * RAW + 24 + 80].decode('latin-1').split())
    print('BIN_SHA256=' + sha(os.path.join(dst, 'alundra2_ps1.bin')))
    print('CUE_SHA256=' + sha(os.path.join(dst, 'alundra2_ps1.cue')))
    print('EXE identique a la source :', sha(os.path.join(dst, 'alundra2.exe')) == sha(os.path.join(src, 'alundra2.exe')))
    return 0


if __name__ == '__main__':
    sys.exit(main())
