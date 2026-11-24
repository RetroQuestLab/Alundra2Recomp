#!/usr/bin/env python3
"""Audit local des chemins de preuves cites dans notes.md (lots v0.8.13 a v0.8.22).

Lecture seule : aucun fichier n'est modifie. Un chemin cite dans un bloc
autoritaire doit exister sur disque (fichier ou repertoire), sinon le verdict
PS1_EVIDENCE est FAIL. Les jokers et les noms de fichiers nus ne sont pas
consideres : ils ne designent pas une preuve unique.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
NOTES = REPO / "notes.md"
BLOCKS = re.compile(
    r"^### BLOC AUTORITAIRE[^\n]*\((v0\.8\.(?:1[3-9]|2[0-5]))-[^)\n]+\)\s*$(.*?)(?=^### |\Z)",
    re.MULTILINE | re.DOTALL,
)
PATH_TOKEN = re.compile(r"`((?:qa/ps1_logs|docs/ps1)/[^`]+)`")


def read_log(path: Path) -> str:
    """Tee-Object ecrit en UTF-16LE avec BOM ; d'autres logs sont en UTF-8."""
    raw = path.read_bytes()
    if raw.startswith(b"\xff\xfe") or raw.startswith(b"\xfe\xff"):
        return raw.decode("utf-16", errors="replace")
    return raw.decode("utf-8", errors="replace")


def main() -> int:
    text = NOTES.read_text(encoding="utf-8", errors="replace")
    seen: set[str] = set()
    failures: list[str] = []
    counts: dict[str, int] = {}

    for match in BLOCKS.finditer(text):
        tag = match.group(1).replace(".", "")  # v0.8.13 -> v0813
        body = match.group(2)
        paths = sorted(set(PATH_TOKEN.findall(body)))
        counts[tag] = 0
        for raw in paths:
            candidate = raw.rstrip("/")
            if "*" in candidate or "…" in candidate or "..." in candidate:
                continue
            target = REPO / candidate
            if not target.exists():
                failures.append(f"{tag}: chemin cite absent: {candidate}")
                continue
            counts[tag] += 1
            seen.add(candidate)

    logs_root = REPO / "qa" / "ps1_logs"
    verdict_counts: dict[str, int] = {}
    for tag in sorted(counts):
        wanted = tag  # v0813
        found = {"build": False, "smoke": False}
        for path in logs_root.rglob("*.txt"):
            relative = str(path.relative_to(REPO)).replace("\\", "/")
            # Les rapports de cet audit contiennent les verdicts en texte et
            # sont ecrits par Tee-Object pendant l'execution : les exclure.
            if relative.endswith(("evidence_paths.txt", "lot_hashes.txt",
                                  "lot_hashes_negative_control.txt")):
                continue
            if wanted not in relative:
                continue
            content = read_log(path)
            if "PS1_BUILD: PASS" in content:
                found["build"] = True
            if "PS1_SMOKE: PASS" in content:
                found["smoke"] = True
            if all(found.values()):
                break
        verdict_counts[tag] = int(found["build"]) + int(found["smoke"])
        for key, verdict in (("build", "PS1_BUILD: PASS"), ("smoke", "PS1_SMOKE: PASS")):
            if not found[key]:
                failures.append(f"{tag}: verdict absent dans les logs: {verdict}")

    print(f"BLOCS_AUDITES={len(counts)}")
    print(f"CHEMINS_UNIQUES_VERIFIES={len(seen)}")
    for tag in sorted(counts):
        print(f"BLOC {tag}: chemins_verifies={counts[tag]} verdicts={verdict_counts[tag]}/2")
    print(f"ECHECS={len(failures)}")
    for failure in failures:
        print(f"FAIL {failure}")
    if failures or len(counts) != 13:
        print("PS1_EVIDENCE: FAIL")
        return 1
    print("PS1_EVIDENCE: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())

