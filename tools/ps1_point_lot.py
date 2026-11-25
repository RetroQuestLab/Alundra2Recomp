"""Fait pointer ps1/a_graver/DERNIERE_VERSION.txt et SHA256.txt vers un lot.

Usage :
    py -3.12 tools/ps1_point_lot.py <version> "<nouveautes>" "<preuves>" "<a tester>"

Les empreintes sont relues dans ps1/a_graver/<version>/SHA256.txt (jamais
saisies a la main). L'historique de SHA256.txt est conserve sous l'en-tete.
"""
import datetime
import os
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ROOT = os.path.join(REPO, 'ps1', 'a_graver')


def main():
    if len(sys.argv) != 5:
        print(__doc__)
        return 2
    version, news, proofs, totest = sys.argv[1:]
    lot = os.path.join(ROOT, version)
    hashes = {}
    with open(os.path.join(lot, 'SHA256.txt'), encoding='utf-8') as f:
        for line in f:
            h, name = line.split()
            hashes[name] = h
    iso = os.path.join(lot, 'alundra2_ntsc.iso')
    size = os.path.getsize(iso)
    today = datetime.date.today().isoformat()
    with open(os.path.join(ROOT, 'DERNIERE_VERSION.txt'), 'w', encoding='utf-8', newline='\r\n') as f:
        f.write('DERNIERE VERSION VERIFIEE\n')
        f.write(f'A GRAVER : {os.path.join(lot, "alundra2_ps1.cue")} (avec alundra2_ps1.bin dans le meme dossier)\n')
        f.write('Gravure : ouvrir le .cue (pas l ISO), mode DAO / Disc-At-Once, vitesse la plus lente proposee, CD-R.\n')
        f.write(f'BIN SHA256 : {hashes["alundra2_ps1.bin"]}\n')
        f.write(f'ISO 2048 (emulateurs uniquement, NE PAS GRAVER) : {iso}\n')
        f.write(f'Build ID : PS1-{version}\nDate : {today}\n')
        f.write(f'Nouveautes : {news}\nVerifications : {proofs} ISO SHA256 '
                f'{hashes["alundra2_ntsc.iso"]}. Console physique non testee.\n')
        f.write(f'A tester : {totest}\n')
    path = os.path.join(ROOT, 'SHA256.txt')
    old = open(path, encoding='utf-8').read() if os.path.exists(path) else ''
    with open(path, 'w', encoding='utf-8', newline='\r\n') as f:
        f.write(f'DERNIERE ISO ({version})\nalundra2_ntsc.iso {size} octets, '
                f'{size // 2048} secteurs (ISO 2048 convertie depuis le BIN)\n'
                f'SHA256 {hashes["alundra2_ntsc.iso"]}\nChemin : {version}\\alundra2_ntsc.iso\n'
                f'Detail : {version}\\SHA256.txt\n\n')
        f.write(old.replace('\r\n', '\n'))
    print(f'POINTEUR -> {version} {hashes["alundra2_ntsc.iso"]}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
