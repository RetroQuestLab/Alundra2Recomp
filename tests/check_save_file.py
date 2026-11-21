"""Sauvegarde reelle et atomique (stdlib) : ouvre le coffre d'herbe de Paco avec
--savefile, puis verifie le fichier ecrit (version 4, objet, coffre), l'absence de
fichier temporaire residuel, la copie de secours .bak a la deuxieme ecriture, et la
reprise depuis .bak quand la sauvegarde principale est corrompue."""
from __future__ import annotations

import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def run(exe: Path, save: Path) -> int:
    args = [str(exe), "--auto", "D1 W2 T2 W200 X600 W5", "--zone", "011", "--at", "29", "49",
            "--savefile", str(save), "--expect", "qty:herbe=1"]
    return subprocess.run(args, cwd=ROOT, timeout=120, check=False).returncode


def main() -> int:
    if len(sys.argv) != 2:
        print("ERR: usage: check_save_file.py <exe>")
        return 2
    exe = Path(sys.argv[1])
    errors: list[str] = []
    with tempfile.TemporaryDirectory() as tmp:
        save = Path(tmp) / "sauvegarde.txt"
        code = run(exe, save)
        if code != 0:
            errors.append(f"premiere partie : code {code}")
        if not save.is_file():
            errors.append("aucune sauvegarde ecrite avec --savefile")
        else:
            text = save.read_text(encoding="utf-8")
            for needle in ("version 4", "zone 011", "objet herbe 1", "coffre 011:29:50"):
                if needle not in text:
                    errors.append(f"sauvegarde sans « {needle} »")
        if save.with_name(save.name + ".tmp").exists():
            errors.append("fichier temporaire residuel (.tmp)")
        code = run(exe, save)  # deuxieme ecriture : l'ancienne sauvegarde passe en .bak
        if code != 0:
            errors.append(f"deuxieme partie : code {code}")
        if not save.with_name(save.name + ".bak").is_file():
            errors.append("copie de secours .bak absente apres une seconde ecriture")
        # Reprise depuis l'ecran titre (« Continuer ») avec une sauvegarde principale corrompue :
        # le jeu doit reprendre depuis la copie de secours, et echouer sans elle.
        fixture = (ROOT / "tests" / "fixtures" / "v4_puzzle.txt").read_text(encoding="utf-8")
        resume = [str(exe), "--auto", "W200 T2 W300", "--zone", "titre", "--savefile", str(save),
                  "--expect", "zone=011"]
        save.write_text("donnees corrompues" + chr(10), encoding="utf-8")
        save.with_name(save.name + ".bak").write_text(fixture, encoding="utf-8")
        code = subprocess.run(resume, cwd=ROOT, timeout=120, check=False).returncode
        if code != 0:
            errors.append(f"reprise depuis .bak (principale corrompue) : code {code}")
        # La reprise a declenche une sauvegarde automatique (fichier principal redevenu valide) :
        # on recorrompt le principal et on retire tout secours avant le controle negatif.
        save.write_text("donnees corrompues" + chr(10), encoding="utf-8")
        save.with_name(save.name + ".bak").unlink(missing_ok=True)
        code = subprocess.run(resume, cwd=ROOT, timeout=120, check=False).returncode
        if code == 0:
            errors.append("reprise reussie alors que la sauvegarde est corrompue et sans secours")
    if errors:
        for e in errors:
            print(f"ERR: {e}")
        print(f"ERR: {len(errors)} | sauvegarde atomique non conforme")
        return 1
    print("ERR: 0 | sauvegarde atomique : fichier complet, pas de .tmp, secours .bak")
    return 0


if __name__ == "__main__":
    sys.exit(main())
