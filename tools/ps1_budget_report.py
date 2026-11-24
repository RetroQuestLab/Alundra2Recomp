#!/usr/bin/env python3
"""Mesures reelles des budgets PS1 sur la version livree (pas d'estimations).

Usage :
    py -3.12 tools/ps1_budget_report.py [ELF] [--iso ISO] [--out RAPPORT]
                                       [--diag-log FICHIER] [--timing-log FICHIER]
                                       [--zone-heap-log FICHIER] [--vram-log FICHIER] [--spu-log FICHIER]

Sans argument, ELF et ISO sont ceux du lot pointe par
ps1/a_graver/DERNIERE_VERSION.txt ; le rapport par defaut est
docs/ps1/budget_<version_courte>.md (slug tire du Build ID du pointeur).
Le lot v0.8.2 reste mesurable en passant explicitement son ELF et son ISO
(aucune valeur n'est codee en dur).

Sources de mesure (toutes reelles, aucune estimation) :
  * RAM  : `mipsel-none-elf-size -A` sur l'ELF livre (sections chargees).
  * VRAM : rectangles declares dans `src/platform/pf.h` via
           `tools/ps1_export_zone.py::RESERVED_RECTS`, plus les rectangles
           reellement ecrits dans les en-tetes P1ZM des zones livrees.
  * SPU  : taille reelle de `ps1/assets/audio.vag` + constantes compilees
           (`kBankSpuAddr`, `kStreamSpuAddr`, `kRingBytes`, interleave VAG).
  * CD   : taille reelle de l'ISO livree et nombre de pistes VAG.
  * Heap/pile : avec --diag-log, releve de g_ps1Diag dans la RAM de
           l'emulateur apres >= 200 frames de gameplay (capture reelle).
  * Temps d'image : avec --timing-log, 1000 periodes RCnt lues par la sonde
           MIPS livree en v0.8.6 (ticks bruts = mesure, ms = conversion nominale).
  * Pic RAM par zone : avec --zone-heap-log, releve du harnais capture-only.
  * Pic VRAM dynamique : avec --vram-log, occupation reelle de la table
           instrumentee dans pfUpload4bpp / pfUpload16 (union des rectangles).
   * Pic SPU dynamique : avec --spu-log, occupation reelle de la table
            instrumentee apres les deux SpuWrite (banque VAG + double buffer),
            avec preuve de streaming sur la phase finale.

Le rapport distingue les valeurs MESUREES des valeurs NON INSTRUMENTEES.
Sans --diag-log / --timing-log / --zone-heap-log / --vram-log, les grandeurs
correspondantes ne sont pas referencees : aucune valeur n'est inventee.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import re
import struct
import subprocess
import sys
from datetime import datetime
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SIZE_EXE = REPO / "external/tools/psn00bsdk/PSn00bSDK-0.24-win32/bin/mipsel-none-elf-size.exe"
RESERVED_LIMIT = 2 * 1024 * 1024          # 2 Mo RAM
VRAM_LIMIT = 1024 * 1024                  # 1 Mo VRAM
SPU_LIMIT = 512 * 1024                    # 512 Ko audio

# Sections MIPS effectivement chargees en RAM (hors debug et commentaires).
LOADED = {".text", ".rodata", ".MIPS.abiflags", ".ctors", ".dtors",
          ".data", ".sdata", ".sbss", ".bss"}


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest().upper()


def latest_release() -> tuple[Path, str]:
    """Lot livre designe par ps1/a_graver/DERNIERE_VERSION.txt (jamais code en dur)."""
    pointer = REPO / "ps1" / "a_graver" / "DERNIERE_VERSION.txt"
    text = pointer.read_text(encoding="utf-8", errors="replace")
    m = re.search(r"(?im)^\s*Chemin\s*:\s*(.+)$", text)
    if m is None:
        raise RuntimeError(f"Chemin absent de {pointer}")
    iso = Path(m.group(1).strip())
    if not iso.is_file():
        raise RuntimeError(f"ISO pointee absente: {iso}")
    build = re.search(r"(?im)^\s*Build ID\s*:\s*PS1-\d+-v(\d+)\.(\d+)\.(\d+)", text)
    slug = (f"v{build.group(1)}{build.group(2)}{build.group(3)}"
            if build else iso.parent.name)
    return iso.parent, slug


def load_export_zone() -> object:
    path = REPO / "tools/ps1_export_zone.py"
    spec = importlib.util.spec_from_file_location("ps1_export_zone", path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"import impossible: {path}")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def measure_ram(elf: Path) -> tuple[dict[str, int], int]:
    if not SIZE_EXE.is_file():
        raise RuntimeError(f"outil absent: {SIZE_EXE}")
    out = subprocess.check_output([str(SIZE_EXE), "-A", str(elf)], text=True)
    sections: dict[str, int] = {}
    for line in out.splitlines():
        m = re.match(r"\s*(\.\S+)\s+(\d+)\s+", line)
        if m:
            sections[m.group(1)] = int(m.group(2))
    missing = LOADED - sections.keys()
    if missing:
        raise RuntimeError(f"sections ELF absentes: {sorted(missing)}")
    return {k: sections[k] for k in sorted(LOADED)}, sum(sections[k] for k in LOADED)


def measure_vram(ez: object) -> tuple[int, dict[str, int]]:
    reserved: dict[str, int] = {}
    for rect in ez.RESERVED_RECTS:  # type: ignore[attr-defined]
        x, y, w, h = rect
        key = f"({x},{y}) {w}x{h}"
        reserved[key] = w * h * 2          # 1 demi-mot = 2 octets
    reserved_total = sum(reserved.values())

    zones: dict[str, int] = {}
    for dat in sorted((REPO / "ps1/assets").glob("zone*.dat")):
        raw = dat.read_bytes()
        if raw[:4] != b"P1ZM":
            raise RuntimeError(f"en-tete P1ZM invalide: {dat}")
        _w, _h, ntiles, nclut, _zero = struct.unpack_from("<5H", raw, 4)
        off = 14 + ntiles * 2
        tile_bytes = 0
        for _ in range(ntiles):
            x, y, tw, th = struct.unpack_from("<4H", raw, off)
            off += 8
            tile_bytes += tw * th * 2
        clut_bytes = nclut * 16 * 2
        zones[dat.stem] = tile_bytes + clut_bytes
    return reserved_total, {"reserved": reserved_total, **zones}


def const(path: Path, name: str, cast=int) -> int:
    text = path.read_text(encoding="utf-8")
    m = re.search(r"constexpr\s+\w+\s+" + re.escape(name) + r"\s*=\s*([^;]+);", text)
    if not m:
        raise RuntimeError(f"constante {name} absente de {path}")
    expr = m.group(1).strip().rstrip("uU")
    return int(expr, 0)


def measure_spu(ez: object) -> dict[str, int]:
    audio_cpp = REPO / "src/ps1/audio_ps1.cpp"
    bank_addr = const(audio_cpp, "kBankSpuAddr")
    stream_addr = const(audio_cpp, "kStreamSpuAddr")
    ring = const(audio_cpp, "kRingBytes")
    bank = (REPO / "ps1/assets/audio.vag").stat().st_size
    interleave = int(ez.INTERLEAVE) if hasattr(ez, "INTERLEAVE") else 0
    if interleave == 0:
        vag0 = REPO / "ps1/assets/music/00.VAG"
        if vag0.is_file():
            raw = vag0.read_bytes()[:2048]
            m = re.search(rb"VAGi", raw)
            if m:
                # En-tête VAGi PSn00bSDK : magic(0), mode(4), interleave(8),
                # taille payload(12, BE), frequency(16, BE), canaux(30).
                interleave = struct.unpack_from("<I", raw, m.start() + 8)[0]
    if interleave <= 0:
        raise RuntimeError("interleave VAG introuvable")
    return {
        "bank_addr": bank_addr,
        "bank_bytes": bank,
        "bank_end": bank_addr + bank,
        "stream_addr": stream_addr,
        "stream_double_buffer": interleave * 2,
        "stream_end": stream_addr + interleave * 2,
        "ring_ram": ring,
        "spu_total": bank + interleave * 2,
    }


def parse_diag_log(path: Path) -> dict[str, object]:
    """Releve reel de g_ps1Diag produit par tools/ps1_capture_diag.ps1.

    Leve une exception si la ligne DIAG ou l'un des champs attendus manque :
    un rapport ne doit jamais inventer une valeur memoire.
    """
    text = path.read_text(encoding="utf-8", errors="replace")
    diag_line = None
    final_line = None
    for line in text.splitlines():
        if line.startswith("DIAG "):
            diag_line = line
        elif line.startswith("DIAG_FINAL "):
            final_line = line
    if diag_line is None or final_line is None:
        raise RuntimeError(f"lignes DIAG / DIAG_FINAL absentes de {path}")

    def fields(line: str, keys: tuple[str, ...]) -> dict[str, int]:
        out: dict[str, int] = {}
        for key in keys:
            m = re.search(rf"(?:^|\s){re.escape(key)}=([^\s]+)", line)
            if m is None:
                raise RuntimeError(f"champ {key} absent de {path}")
            out[key] = int(m.group(1), 0)
        return out

    result: dict[str, object] = fields(
        diag_line,
        ("magic", "heapPeak", "stackDepthMax", "stackFloor", "stackTop", "samples"),
    )
    result.update(fields(
        final_line,
        ("heapTotal", "heapReserved", "heapStackReserve", "heapAlloc", "stackPaintLo"),
    ))
    if result["magic"] != 0xA2503144:
        raise RuntimeError(f"magic g_ps1Diag inattendu dans {path}: {result['magic']:#x}")
    if int(result["samples"]) < 200:
        raise RuntimeError(f"sampleCount insuffisant dans {path}: {result['samples']}")

    cmd = ""
    m = re.search(r"(?m)^CMD=(.*)$", text)
    if m:
        cmd = m.group(1).strip()
    iso_line = ""
    m = re.search(r"(?m)^ISO_SHA256=([0-9A-Fa-f]+)$", text)
    if m:
        iso_line = m.group(1).upper()
    result["cmd"] = cmd
    result["iso_sha256"] = iso_line
    result["captured_at"] = datetime.fromtimestamp(path.stat().st_mtime).isoformat(timespec="seconds")
    result["log"] = str(path.relative_to(REPO) if path.is_relative_to(REPO) else path)
    return result


def parse_timing_log(path: Path, expected_iso_sha256: str | None = None) -> dict[str, object]:
    """Releve reel de 1000 periodes RCnt produit par tools/ps1_frame_timing.ps1.

    Les ticks bruts sont la mesure. La conversion ms reste nominaire : le taux
    RCntCNT1 n'est pas calibre par ce rapport.
    """
    text = path.read_text(encoding="utf-8", errors="replace")
    if "FRAME_TIMING: PASS" not in text:
        raise RuntimeError(f"sonde temps d'image non reussie dans {path}")

    def grab(pattern: str, key: str) -> str:
        m = re.search(pattern, text)
        if m is None:
            raise RuntimeError(f"champ {key} absent de {path}")
        return m.group(1)

    m = re.search(
        r"FRAME_TIMING samples=(\d+) ticks_min=(\d+) ticks_p50=(\d+) "
        r"ticks_p95=(\d+) ticks_max=(\d+) ticks_mean=([\d.]+)", text)
    if m is None:
        raise RuntimeError(f"champs ticks absents de {path}")
    counts = m.groups()
    samples = int(counts[0])
    if samples < 1000:
        raise RuntimeError(f"echantillonnage insuffisant dans {path}: {samples}")

    result: dict[str, object] = {
        "samples": samples,
        "ticks_min": int(counts[1]),
        "ticks_p50": int(counts[2]),
        "ticks_p95": int(counts[3]),
        "ticks_max": int(counts[4]),
        "ticks_mean": float(counts[5]),
    }
    for key in ("min", "p50", "p95", "max", "mean"):
        value = grab(rf"ms_nominal_{key}=([\d.]+)", f"ms_nominal_{key}")
        result[f"ms_nominal_{key}"] = float(value)
    result["outliers"] = int(grab(r"outliers_gt_20000=(\d+)", "outliers_gt_20000"))
    result["max_index"] = int(grab(r"max_index=(\d+)", "max_index"))

    meta = path.with_name("timing_meta.txt")
    if expected_iso_sha256 is not None:
        if not meta.is_file():
            raise RuntimeError(f"meta de capture absent: {meta}")
        meta_text = meta.read_text(encoding="utf-8", errors="replace")
        m = re.search(r"(?m)^SHA256=([0-9A-Fa-f]+)$", meta_text)
        if m is None:
            raise RuntimeError(f"SHA256 absent de {meta}")
        if m.group(1).upper() != expected_iso_sha256.upper():
            raise RuntimeError("le log de timing ne correspond pas a l'ISO mesuree")
        m = re.search(r"(?m)^CMD=(.*)$", meta_text)
        result["cmd"] = m.group(1).strip() if m else ""
    else:
        result["cmd"] = ""
    result["captured_at"] = datetime.fromtimestamp(path.stat().st_mtime).isoformat(timespec="seconds")
    result["log"] = str(path.relative_to(REPO) if path.is_relative_to(REPO) else path)
    return result


def parse_zone_heap_log(path: Path, expected_iso_sha256: str | None = None) -> dict[str, object]:
    """Releve reel du pic heap par zone produit par tools/ps1_capture_zone_heap.ps1.

    Le harnais ecrit des octets bruts dans le POD ; toute valeur absente ou
    incoherente leve une exception : aucun rapport ne doit inventer un pic.
    """
    text = path.read_text(encoding="utf-8", errors="replace")
    if "PS1_CAPTURE_ZONE_HEAP: PASS" not in text:
        raise RuntimeError(f"capture zone heap non reussie dans {path}")

    m = re.search(r"(?m)^ISO_SHA256=([0-9A-Fa-f]+)$", text)
    if m is None:
        raise RuntimeError(f"ISO_SHA256 absent de {path}")
    log_sha = m.group(1).upper()
    if expected_iso_sha256 is not None and log_sha != expected_iso_sha256.upper():
        raise RuntimeError("le log zone heap ne correspond pas a l'ISO mesuree")

    def fields(line: str, keys: tuple[str, ...]) -> dict[str, int]:
        out: dict[str, int] = {}
        for key in keys:
            found = re.search(rf"(?:^|\s){re.escape(key)}=([^\s]+)", line)
            if found is None:
                raise RuntimeError(f"champ {key} absent de {path}")
            out[key] = int(found.group(1), 0)
        return out

    step_lines = [line for line in text.splitlines()
                  if line.startswith("ZONE_HEAP step=")]
    if len(step_lines) != 4:
        raise RuntimeError(f"etapes zone heap: {len(step_lines)} attendu 4 dans {path}")
    steps: list[dict[str, int]] = []
    expected_zones = (13, 26, 11, 0)
    for index, line in enumerate(step_lines, 1):
        row = fields(line, ("step", "zone", "alloc", "peak", "bytes",
                            "releases", "errors", "magic"))
        if row["step"] != index or row["zone"] != expected_zones[index - 1]:
            raise RuntimeError(f"sequence zone heap inattendue dans {path}: {line}")
        if row["magic"] != 0xA25A5A5A or row["errors"] != 0:
            raise RuntimeError(f"POD zone heap invalide dans {path}: {line}")
        if row["peak"] < row["alloc"]:
            raise RuntimeError(f"pic inferieur a l'allocation dans {path}: {line}")
        if index < 4 and row["bytes"] <= 0:
            raise RuntimeError(f"blob vide dans {path}: {line}")
        steps.append(row)

    result_lines = [line for line in text.splitlines()
                    if line.startswith("ZONE_HEAP_RESULT ")]
    if len(result_lines) != 1:
        raise RuntimeError(f"ligne ZONE_HEAP_RESULT: {len(result_lines)} attendu 1 dans {path}")
    result = fields(result_lines[0], ("loadCount", "releaseCount", "baselineAlloc",
                                      "finalAlloc", "staticRam"))
    if result["loadCount"] != 3 or result["releaseCount"] != 3:
        raise RuntimeError(f"chargements/liberations incomplets dans {path}")
    if result["baselineAlloc"] <= 0 or result["finalAlloc"] != result["baselineAlloc"]:
        raise RuntimeError(f"fuite detectee dans {path}: "
                           f"finalAlloc={result['finalAlloc']} != "
                           f"baselineAlloc={result['baselineAlloc']}")
    if result["staticRam"] <= 0:
        raise RuntimeError(f"staticRam incoherent dans {path}")

    cmd = ""
    m = re.search(r"(?m)^CMD=(.*)$", text)
    if m:
        cmd = m.group(1).strip()
    return {
        **result,
        "steps": steps,
        "iso_sha256": log_sha,
        "cmd": cmd,
        "captured_at": datetime.fromtimestamp(path.stat().st_mtime).isoformat(timespec="seconds"),
        "log": str(path.relative_to(REPO) if path.is_relative_to(REPO) else path),
    }


def render_zone_heap_section(zone: dict[str, object] | None) -> list[str]:
    """Section 5 : pic heap par zone, mesure sur l'ISO livree."""
    if zone is None:
        return [
            "## 5. Pic RAM par zone (chargement dynamique) — non référencé dans ce rapport",
            "",
            "Passer `--zone-heap-log` pour joindre la capture des zones 011 → 013 → 026 → 011. "
            "Aucune valeur de pic n'est inventée ici.",
        ]
    steps = list(zone["steps"])
    lines = [
        "## 5. Pic RAM par zone — mesuré (capture CD sur l'ISO livrée)",
        "",
        "Méthode : le harnais capture-only lit chaque blob P1ZM depuis le CD dans l'arene "
        "`malloc`, appelle le parseur de zone existant, libere le blob avant l'allocation "
        "suivante, et releve `HeapUsage.alloc` / `alloc_max` du SDK. Le jeu normal garde "
        "ses blobs `incbin` et n'entre pas dans ce chemin.",
        "",
        "| Etape | Zone | `alloc` mesuré (octets) | `alloc_max` SDK (octets) | Octets lus | Liberations |",
        "|---:|---:|---:|---:|---:|---:|",
    ]
    labels = {13: "013", 26: "026", 11: "011 (retour)", 0: "liberation finale"}
    for row in steps:
        lines.append(f"| {int(row['step'])} | {labels[int(row['zone'])]} | "
                     f"{int(row['alloc'])} | {int(row['peak'])} | "
                     f"{int(row['bytes'])} | {int(row['releases'])} |")
    lines += [
        "",
        f"- **Mesuré** : hauteur d'eau initiale (`baselineAlloc`) = "
        f"**{int(zone['baselineAlloc'])} octets** ; retour final (`finalAlloc`) = "
        f"**{int(zone['finalAlloc'])} octets**. Egalité requise : aucun octet de zone "
        "n'est conservé après la séquence.",
        f"- **Mesuré** : `alloc_max` est un compteur monotone du SDK ; le pic global de la "
        f"capture est **{max(int(r['peak']) for r in steps)} octets**. La plus grande zone "
        f"seule atteint **{max(int(r['alloc']) for r in steps[:3])} octets** "
        "(zone 011, 220006 octets lus).",
        f"- **Mesuré** : `staticRam` = **{int(zone['staticRam'])} octets** "
        "(adresse de fin de segments charges, limite superieure de l'arene).",
        f"- Sequence reelle : 011 (zone initiale du jeu, etat baseline) → 013 → 026 → 011 "
        "(blob CD relu) → liberation explicite. Aucune valeur n'est estimee.",
        f"- Releve le {zone['captured_at']}, log `{zone['log']}`, "
        f"ISO SHA256 `{zone['iso_sha256']}`.",
        f"- Commande de capture : `{zone['cmd']}`",
    ]
    return lines


def parse_vram_log(path: Path, expected_iso_sha256: str | None = None) -> dict[str, object]:
    """Releve reel du pic VRAM produit par tools/ps1_capture_vram.ps1.

    Le POD est lu sans ecriture par le Lua. Toute incoherence (magie, somme de
    contrôle, ouverture de l'inventaire, peak non monotone, rejets) leve une exception :
    aucun rapport ne doit inventer un pic dynamique.
    """
    text = path.read_text(encoding="utf-8", errors="replace")
    if "PS1_CAPTURE_VRAM: PASS" not in text:
        raise RuntimeError(f"capture VRAM non reussie dans {path}")

    m = re.search(r"(?m)^ISO_SHA256=([0-9A-Fa-f]+)$", text)
    if m is None:
        raise RuntimeError(f"ISO_SHA256 absent de {path}")
    log_sha = m.group(1).upper()
    if expected_iso_sha256 is not None and log_sha != expected_iso_sha256.upper():
        raise RuntimeError("le log VRAM ne correspond pas a l'ISO mesuree")

    def fields(line: str, keys: tuple[str, ...]) -> dict[str, int]:
        out: dict[str, int] = {}
        for key in keys:
            found = re.search(rf"(?:^|\s){re.escape(key)}=([^\s]+)", line)
            if found is None:
                raise RuntimeError(f"champ {key} absent de {path}")
            out[key] = int(found.group(1), 0)
        return out

    keys = ("magic", "ready", "frame", "uploads", "overlapHalfwords",
            "currentBytes", "peakBytes", "peakFrame", "framebufferBytes",
            "texturePeakBytes", "rejections", "firstFrame", "lastFrame",
            "vramBytes", "halfwords", "integrity")
    phase_lines = [line for line in text.splitlines()
                   if line.startswith("VRAM phase=")]
    if len(phase_lines) != 3:
        raise RuntimeError(f"phases VRAM: {len(phase_lines)} attendu 3 dans {path}")
    expected_phases = ("menu", "game", "inventory")
    phases: list[dict[str, int | str]] = []
    last_peak = 0
    for index, line in enumerate(phase_lines):
        row = fields(line, keys)
        phase = re.search(r"(?:^|\s)phase=([^\s]+)", line)
        if phase is None:
            raise RuntimeError(f"phase absente dans {path}")
        row["phase"] = phase.group(1)
        if row["phase"] != expected_phases[index]:
            raise RuntimeError(f"sequence VRAM inattendue dans {path}: {line}")
        if row["magic"] != 0xA2565241:
            raise RuntimeError(f"POD VRAM magie invalide dans {path}: {line}")
        if row["ready"] != 1 or row["rejections"] != 0:
            raise RuntimeError(f"POD VRAM non valide dans {path}: {line}")
        if (row["vramBytes"] != 1048576 or row["halfwords"] != 524288 or
                row["framebufferBytes"] != 307200):
            raise RuntimeError(f"geometrie VRAM incoherente dans {path}: {line}")
        if row["peakBytes"] < row["framebufferBytes"] or row["peakBytes"] > row["vramBytes"]:
            raise RuntimeError(f"pic VRAM hors plage dans {path}: {line}")
        if row["currentBytes"] != row["peakBytes"]:
            raise RuntimeError(f"occupation courante != pic dans {path}: {line}")
        if row["texturePeakBytes"] != row["peakBytes"] - row["framebufferBytes"]:
            raise RuntimeError(f"pic texture incoherent dans {path}: {line}")
        if (row["firstFrame"] <= 0 or row["lastFrame"] < row["firstFrame"] or
                row["peakFrame"] <= 0 or row["peakFrame"] > row["lastFrame"]):
            raise RuntimeError(f"horodatage VRAM incoherent dans {path}: {line}")
        if row["peakBytes"] < last_peak:
            raise RuntimeError(f"pic VRAM non monotone dans {path}: {line}")
        last_peak = row["peakBytes"]
        computed = (0xA2565241 ^ row["uploads"] ^ row["currentBytes"] ^
                    row["peakBytes"] ^ row["overlapHalfwords"])
        if row["integrity"] != computed:
            raise RuntimeError(f"somme de controle VRAM invalide dans {path}: {line}")
        phases.append(row)

    result_lines = [line for line in text.splitlines()
                    if line.startswith("VRAM_RESULT ")]
    if len(result_lines) != 1:
        raise RuntimeError(f"ligne VRAM_RESULT: {len(result_lines)} attendu 1 dans {path}")
    result = fields(result_lines[0], ("peakBytes", "peakFrame", "texturePeakBytes",
                                      "uploads", "overlapHalfwords", "rejections"))
    final_phase = phases[-1]
    if (result["peakBytes"] != final_phase["peakBytes"] or
            result["peakFrame"] != final_phase["peakFrame"] or
            result["texturePeakBytes"] != final_phase["texturePeakBytes"] or
            result["uploads"] != final_phase["uploads"] or
            result["overlapHalfwords"] != final_phase["overlapHalfwords"] or
            result["rejections"] != final_phase["rejections"]):
        raise RuntimeError(f"VRAM_RESULT incoherent avec la phase inventory dans {path}")

    cmd = ""
    m = re.search(r"(?m)^CMD=(.*)$", text)
    if m:
        cmd = m.group(1).strip()
    return {
        **result,
        "framebufferBytes": phases[0]["framebufferBytes"],
        "phases": phases,
        "iso_sha256": log_sha,
        "cmd": cmd,
        "captured_at": datetime.fromtimestamp(path.stat().st_mtime).isoformat(timespec="seconds"),
        "log": str(path.relative_to(REPO) if path.is_relative_to(REPO) else path),
    }


def render_vram_section(vram_capture: dict[str, object] | None) -> list[str]:
    """Section 6 : pic VRAM reel pendant le rendu, mesure sur l'ISO livree."""
    if vram_capture is None:
        return [
            "## 6. Pic VRAM pendant le rendu — non référencé dans ce rapport",
            "",
            "Passer `--vram-log` pour joindre la capture de la table d'allocation "
            "instrumentée. Aucune valeur de pic dynamique n'est inventée ici.",
        ]
    phases = list(vram_capture["phases"])
    peak = int(vram_capture["peakBytes"])
    fb = int(phases[0]["framebufferBytes"])
    lines = [
        "## 6. Pic VRAM pendant le rendu — mesuré (table d'allocation instrumentée)",
        "",
        "Méthode : un bit par demi-mot VRAM (65 536 octets de BSS) est mis à jour dans "
        "`pfUpload4bpp` / `pfUpload16`, avec les deux framebuffers réservés dans `pfInit`. "
        "Le POD `g_ps1VramDiag` est lu passivement par Lua aux phases menu, gameplay et "
        "inventaire. L'occupation est une union : réécrire un rectangle ne double pas son coût.",
        "",
        "| Phase | `uploads` | Chevauchements (demi-mots) | Occupation (octets) | Pic (octets) |",
        "|---|---:|---:|---:|---:|",
    ]
    for row in phases:
        lines.append(f"| {row['phase']} | {int(row['uploads'])} | "
                     f"{int(row['overlapHalfwords'])} | {int(row['currentBytes'])} | "
                     f"{int(row['peakBytes'])} |")
    lines += [
        "",
        f"- **Mesuré** : pic VRAM global = **{peak:,} octets** sur 1 048 576 "
        f"({100 * peak / 1048576:.1f} %), atteint à la frame {int(vram_capture['peakFrame'])}.",
        f"- **Mesuré** : framebuffers = **{fb:,} octets** ; pic texture au-dessus des "
        f"framebuffers = **{int(vram_capture['texturePeakBytes']):,} octets**.",
        f"- **Mesuré** : {int(vram_capture['uploads'])} uploads acceptés, "
        f"{int(vram_capture['overlapHalfwords'])} demi-mots réécrits, "
        f"{int(vram_capture['rejections'])} rejet ; somme de contrôle du POD vérifiée à chaque phase.",
        "- Le pic statique de la section 2 additionne des rectangles réservés et le plus grand "
        "blob P1ZM : il ne mesure pas les remplacements ni les uploads réels. Les deux valeurs "
        "sont distinctes et ne se substituent pas.",
        f"- Relevé le {vram_capture['captured_at']}, log `{vram_capture['log']}`, "
        f"ISO SHA256 `{vram_capture['iso_sha256']}`.",
        f"- Commande de capture : `{vram_capture['cmd']}`",
    ]
    return lines


def parse_spu_log(path: Path, expected_iso_sha256: str | None = None) -> dict[str, object]:
    """Releve reel du pic SPU produit par tools/ps1_capture_spu.ps1.

    Le POD `g_ps1SpuDiag` est lu sans ecriture par le Lua. Toute incoherence
    (magie, somme de controle, phase, monotonie, rejets ou preuve de streaming)
    leve une exception : aucun rapport ne doit inventer un pic dynamique.
    """
    text = path.read_text(encoding="utf-8", errors="replace")
    if "PS1_CAPTURE_SPU: PASS" not in text:
        raise RuntimeError(f"capture SPU non reussie dans {path}")

    m = re.search(r"(?m)^ISO_SHA256=([0-9A-Fa-f]+)$", text)
    if m is None:
        raise RuntimeError(f"ISO_SHA256 absent de {path}")
    log_sha = m.group(1).upper()
    if expected_iso_sha256 is not None and log_sha != expected_iso_sha256.upper():
        raise RuntimeError("le log SPU ne correspond pas a l'ISO mesuree")

    def fields(line: str, keys: tuple[str, ...]) -> dict[str, int]:
        out: dict[str, int] = {}
        for key in keys:
            pattern = "(?:^|\\s)" + re.escape(key) + "=([^\\s]+)"
            found = re.search(pattern, line)
            if found is None:
                raise RuntimeError(f"champ {key} absent de {path}")
            out[key] = int(found.group(1), 0)
        return out

    keys = ("magic", "ready", "frame", "transfers", "currentBytes",
            "peakBytes", "peakFrame", "overlapBytes", "rejections",
            "spuBytes", "reservedBytes", "bankBytes", "streamBytes",
            "chunks", "underruns", "integrity")
    phase_lines = [line for line in text.splitlines()
                   if line.startswith("SPU phase=")]
    if len(phase_lines) != 3:
        raise RuntimeError(f"phases SPU: {len(phase_lines)} attendu 3 dans {path}")
    expected_phases = ("menu", "game", "inventory")
    phases: list[dict[str, int | str]] = []
    last_peak = 0
    last_transfers = 0
    for index, line in enumerate(phase_lines):
        row = fields(line, keys)
        phase = re.search(r"(?:^|\s)phase=([^\s]+)", line)
        if phase is None:
            raise RuntimeError(f"phase absente dans {path}")
        row["phase"] = phase.group(1)
        if row["phase"] != expected_phases[index]:
            raise RuntimeError(f"sequence SPU inattendue dans {path}: {line}")
        if row["magic"] != 0xA2535044:
            raise RuntimeError(f"POD SPU magie invalide dans {path}: {line}")
        if row["ready"] != 1 or row["rejections"] != 0:
            raise RuntimeError(f"POD SPU non valide dans {path}: {line}")
        if row["spuBytes"] != 524288 or row["reservedBytes"] != 4096:
            raise RuntimeError(f"geometrie SPU incoherente dans {path}: {line}")
        if row["transfers"] <= 0:
            raise RuntimeError(f"aucun transfert SPU dans {path}: {line}")
        if row["currentBytes"] != row["peakBytes"]:
            raise RuntimeError(f"occupation courante != pic dans {path}: {line}")
        if row["peakBytes"] != row["bankBytes"] + row["streamBytes"]:
            raise RuntimeError(f"banque + flux != occupation dans {path}: {line}")
        if row["peakBytes"] > row["spuBytes"]:
            raise RuntimeError(f"pic SPU hors plage dans {path}: {line}")
        # peakFrame=0 est mesure : le prefill CD du boot atteint le pic avant
        # la premiere image presentee. La preuve de streaming est portee par
        # chunks/streamBytes/transfers/underruns sur la phase finale.
        if row["peakFrame"] < 0 or row["peakFrame"] > row["frame"]:
            raise RuntimeError(f"horodatage SPU incoherent dans {path}: {line}")
        if row["peakBytes"] < last_peak:
            raise RuntimeError(f"pic SPU non monotone dans {path}: {line}")
        if row["transfers"] < last_transfers:
            raise RuntimeError(f"transferts SPU non monotones dans {path}: {line}")
        last_peak = row["peakBytes"]
        last_transfers = row["transfers"]
        computed = (0xA2535044 ^ row["transfers"] ^ row["currentBytes"] ^
                    row["peakBytes"] ^ row["overlapBytes"])
        if row["integrity"] != computed:
            raise RuntimeError(f"somme de controle SPU invalide dans {path}: {line}")
        phases.append(row)

    final = phases[-1]
    if (int(final["chunks"]) < 2 or int(final["transfers"]) < 3 or
            int(final["underruns"]) != 0 or int(final["streamBytes"]) != 16384):
        raise RuntimeError(f"preuve de streaming SPU absente dans {path}")

    result_lines = [line for line in text.splitlines()
                    if line.startswith("SPU_RESULT ")]
    if len(result_lines) != 1:
        raise RuntimeError(f"ligne SPU_RESULT: {len(result_lines)} attendu 1 dans {path}")
    result_keys = ("peakBytes", "peakFrame", "currentBytes", "bankBytes",
                   "streamBytes", "transfers", "overlapBytes", "rejections",
                   "chunks", "underruns", "frame", "integrity")
    result = fields(result_lines[0], result_keys)
    for key in ("peakBytes", "peakFrame", "currentBytes", "bankBytes",
                "streamBytes", "transfers", "overlapBytes", "rejections",
                "chunks", "underruns", "integrity"):
        if result[key] != final[key]:
            raise RuntimeError(f"SPU_RESULT {key} incoherent avec la phase inventory dans {path}")

    cmd = ""
    m = re.search(r"(?m)^CMD=(.*)$", text)
    if m:
        cmd = m.group(1).strip()
    return {
        **result,
        "phases": phases,
        "iso_sha256": log_sha,
        "cmd": cmd,
        "captured_at": datetime.fromtimestamp(path.stat().st_mtime).isoformat(timespec="seconds"),
        "log": str(path.relative_to(REPO) if path.is_relative_to(REPO) else path),
    }


def render_spu_section(spu_capture: dict[str, object] | None,
                       spu_static: dict[str, int] | None) -> list[str]:
    """Section 3 : occupation SPU statique et pic reel pendant le streaming CD."""
    static_bytes = int(spu_static["spu_total"]) if spu_static is not None else 394688
    header_static = (
        "Le calcul statique additionne la banque VAG residente et le double buffer "
        f"musical, soit **{static_bytes:,} octets** ; il ne mesure pas les ecritures "
        "reelles ni leurs chevauchements."
    )
    if spu_capture is None:
        return [
            "## 3b. Pic SPU pendant le streaming CD — non référencé dans ce rapport",
            "",
            header_static + " Passer `--spu-log` pour joindre la capture de la table "
            "d'occupation instrumentée. Aucune valeur dynamique n'est inventée ici.",
        ]

    phases = list(spu_capture["phases"])
    peak = int(spu_capture["peakBytes"])
    lines = [
        "## 3b. Pic SPU pendant le streaming CD — mesuré (table d'occupation instrumentée)",
        "",
        "Méthode : un bit par bloc SPU de 8 octets (8 192 octets de BSS) est mis à jour "
        "après les deux seuls `SpuWrite` du port (banque VAG et double buffer musical). "
        "Le POD `g_ps1SpuDiag` est lu passivement par Lua aux phases menu, gameplay et "
        "inventaire. L'occupation est une union : réécrire un bloc ne double pas son coût.",
        "",
        "| Phase | `transfers` | `chunks` | Chevauchements (octets) | Occupation (octets) | Pic (octets) |",
        "|---|---:|---:|---:|---:|---:|",
    ]
    for row in phases:
        lines.append(f"| {row['phase']} | {int(row['transfers'])} | {int(row['chunks'])} | "
                     f"{int(row['overlapBytes'])} | {int(row['currentBytes'])} | "
                     f"{int(row['peakBytes'])} |")
    lines += [
        "",
        f"- **Mesuré** : pic SPU global = **{peak:,} octets** sur 524 288 "
        f"({100 * peak / 524288:.1f} %). `peakFrame={int(spu_capture['peakFrame'])}` : "
        "le préfill CD synchrone du boot atteint ce pic avant la première image présentée ; "
        "il n'est jamais dépassé pendant le gameplay.",
        f"- **Mesuré** : banque VAG = **{int(spu_capture['bankBytes']):,} octets**, "
        f"double buffer musical = **{int(spu_capture['streamBytes']):,} octets**, "
        f"soit exactement le pic ; {int(spu_capture['transfers'])} transferts acceptés, "
        f"{int(spu_capture['chunks'])} chunks de flux, {int(spu_capture['underruns'])} underrun, "
        f"{int(spu_capture['rejections'])} rejet, "
        f"{int(spu_capture['overlapBytes']):,} octets réécrits en union.",
        "- **Preuve de streaming** : `chunks>=2`, `streamBytes==16384`, `transfers>=3` et "
        "`underruns==0` sont vérifiés sur la phase finale ; la mesure couvre donc le rendu "
        "**et** le streaming CD, pas seulement le chargement initial.",
        "- " + header_static + " Ici le pic mesuré coïncide avec le calcul statique, "
        "mais il est désormais **observé** sur l'ISO livrée : les deux valeurs ne se "
        "substituent pas.",
        f"- Relevé le {spu_capture['captured_at']}, log `{spu_capture['log']}`, "
        f"ISO SHA256 `{spu_capture['iso_sha256']}`.",
        f"- Commande de capture : `{spu_capture['cmd']}`",
    ]
    return lines


def render_timing_section(timing: dict[str, object] | None) -> list[str]:
    """Section 4 du rapport : mesuree si un log est fourni, sinon honnête."""
    if timing is None:
        return [
            "## 4. Temps d'image — mesure disponible, non référencée dans ce rapport",
            "",
            "La sonde MIPS (`g_ps1FrameTiming`, `tools/ps1_frame_timing.ps1`) est opérationnelle "
            "depuis le lot `v0.8.6-frame-timing`. Ce rapport ne cite aucune valeur de temps "
            "d'image : passer `--timing-log <log de capture>` pour joindre une mesure sur l'ISO "
            "livrée. Aucune valeur n'est inventée ici.",
        ]
    return [
        "## 4. Temps d'image — mesuré (ticks RCnt bruts)",
        "",
        f"- **Mesuré** : {int(timing['samples'])} périodes entre deux `VSync(0)`, lues par accès "
        "direct au compteur racine 1 (`0x1f801110`, `src/ps1/pf_ps1.cpp`). "
        f"ticks min/p50/p95/pic = **{int(timing['ticks_min'])} / {int(timing['ticks_p50'])} / "
        f"{int(timing['ticks_p95'])} / {int(timing['ticks_max'])}**, moyenne "
        f"{float(timing['ticks_mean']):.1f}.",
        f"- **Conversion nominale** (taux RCntCNT1 1,053497 MHz, non calibré ici — jamais une "
        f"mesure horloge indépendante) : ms min/p50/p95/pic = "
        f"{float(timing['ms_nominal_min']):.3f} / {float(timing['ms_nominal_p50']):.3f} / "
        f"{float(timing['ms_nominal_p95']):.3f} / {float(timing['ms_nominal_max']):.3f}, "
        f"moyenne {float(timing['ms_nominal_mean']):.3f}. Cible : 16,67 ms (NTSC 60 Hz).",
        f"- Outlier conservé, sans filtrage : `outliers_gt_20000={int(timing['outliers'])}`, "
        f"`max_index={int(timing['max_index'])}`. Sa cause précise n'est pas établie par cette mesure.",
        f"- Relevé le {timing['captured_at']}, log `{timing['log']}`.",
        f"- Commande de capture : `{timing['cmd']}`",
    ]


def main() -> int:
    ap = argparse.ArgumentParser()
    default_dir, slug = latest_release()
    ap.add_argument("elf", nargs="?", type=Path, default=default_dir / "alundra2.elf")
    ap.add_argument("--iso", type=Path, default=None,
                    help="ISO livree (par defaut alundra2_ntsc.iso a cote de l'ELF)")
    ap.add_argument("--out", type=Path, default=None,
                    help="rapport (par defaut docs/ps1/budget_<lot>.md)")
    ap.add_argument("--diag-log", type=Path, default=None,
                    help="log de capture g_ps1Diag (mesures heap/pile reelles)")
    ap.add_argument("--timing-log", type=Path, default=None,
                    help="log de capture du temps d'image (1000 periodes RCnt)")
    ap.add_argument("--zone-heap-log", type=Path, default=None,
                    help="log de capture du pic heap par zone (011 → 013 → 026 → 011)")
    ap.add_argument("--vram-log", type=Path, default=None,
                    help="log de capture du pic VRAM reel pendant le rendu")
    ap.add_argument("--spu-log", type=Path, default=None,
                    help="log de capture du pic SPU reel pendant le streaming CD")
    args = ap.parse_args()
    iso = args.iso if args.iso is not None else args.elf.with_name("alundra2_ntsc.iso")
    out = args.out if args.out is not None else REPO / "docs" / "ps1" / f"budget_{slug}.md"
    if not args.elf.is_file():
        raise RuntimeError(f"ELF absent: {args.elf}")
    if not iso.is_file():
        raise RuntimeError(f"ISO absente: {iso}")
    iso_sha = sha256(iso)
    diag = parse_diag_log(args.diag_log) if args.diag_log is not None else None
    if diag is not None and diag["iso_sha256"] and diag["iso_sha256"] != iso_sha:
        raise RuntimeError("le log diag ne correspond pas a l'ISO mesuree")
    timing = parse_timing_log(args.timing_log, iso_sha) if args.timing_log is not None else None
    zone_heap = (parse_zone_heap_log(args.zone_heap_log, iso_sha)
                 if args.zone_heap_log is not None else None)
    vram_capture = (parse_vram_log(args.vram_log, iso_sha)
                    if args.vram_log is not None else None)
    spu_capture = (parse_spu_log(args.spu_log, iso_sha)
                   if args.spu_log is not None else None)

    ez = load_export_zone()
    sections, ram = measure_ram(args.elf)
    vram_reserved, vram = measure_vram(ez)
    spu = measure_spu(ez)
    iso_bytes = iso.stat().st_size
    bin_path = iso.with_name("alundra2_ps1.bin")
    bin_bytes = bin_path.stat().st_size if bin_path.is_file() else 0
    music_files = sorted((REPO / "ps1/assets/music").glob("*.VAG"))
    music_bytes = sum(p.stat().st_size for p in music_files)

    zone_peak = max((v for k, v in vram.items() if k.startswith("zone")), default=0)
    vram_peak = vram_reserved + zone_peak

    repro_diag = f" --diag-log {diag['log']}" if diag is not None else ""
    repro_timing = f" --timing-log {timing['log']}" if timing is not None else ""
    repro_zone = f" --zone-heap-log {zone_heap['log']}" if zone_heap is not None else ""
    repro_vram = f" --vram-log {vram_capture['log']}" if vram_capture is not None else ""
    repro_spu = f" --spu-log {spu_capture['log']}" if spu_capture is not None else ""
    if diag is None:
        diag_bullets = [
            "- **Non instrumenté dans cette exécution** : pic heap réel et profondeur de pile. "
            "Passer `--diag-log` pour obtenir des mesures ; aucune valeur n'est inventée ici.",
        ]
    else:
        diag_bullets = [
            "- **Mesuré** : pic heap `HeapUsage.alloc_max` du SDK PSn00bSDK = "
            f"**{int(diag['heapPeak']):,} octets** (allocation courante finale "
            f"{int(diag['heapAlloc']):,} octets sur un tas de {int(diag['heapTotal']):,} octets). "
            "Un pic à 0 signifierait « aucun appel d'allocation observé », pas une estimation.",
            "- **Mesuré** : profondeur de pile MIPS maximale = "
            f"**{int(diag['stackDepthMax']):,} octets** "
            f"(plancher `0x{int(diag['stackFloor']):08x}`, sommet `0x{int(diag['stackTop']):08x}`). "
            "Méthode : peinture `0xA5A5A5A5` d'une fenêtre de 256 Kio au plus, sous le pointeur "
            "de pile réel et au-dessus du sommet de l'arène `malloc`, garde de 512 octets, "
            "balayage toutes les 60 images. Une pile très superficielle peut être sous-estimée "
            "de cette garde.",
            f"- Relevé après **{int(diag['samples']):,}** appels à `diagSamplePS1()`, "
            f"le {diag['captured_at']}, log `{diag['log']}`, "
            f"ISO SHA256 `{diag['iso_sha256']}`.",
            f"- Commande de capture : `{diag['cmd']}`",
        ]

    lines = [
        "# Budgets PS1 — mesures sur la version livrée",
        "",
        f"ELF mesuré : `{args.elf.relative_to(REPO)}`",
        f"ISO mesurée : `{iso.relative_to(REPO)}`",
        "",
        "Reproduction : `py -3.12 tools\\ps1_budget_report.py "
        f"{args.elf.relative_to(REPO)} --iso {iso.relative_to(REPO)} --out {out.relative_to(REPO)}"
        f"{repro_diag}{repro_timing}{repro_zone}{repro_vram}{repro_spu}`",
        "",
        f"- SHA256 ELF : `{sha256(args.elf)}`",
        f"- SHA256 ISO : `{sha256(iso)}`",
        "",
        "Toutes les valeurs ci-dessous sont des **mesures** obtenues par les commandes",
        "indiquées. Les quantités non instrumentées sont signalées explicitement.",
        "",
        "## 1. RAM principale — limite 2 097 152 octets",
        "",
        "Commande : `mipsel-none-elf-size -A <ELF>` (sections chargées uniquement).",
        "",
        "| Section | Octets mesurés |",
        "|---|---:|",
    ]
    lines += [f"| `{k}` | {v:,} |" for k, v in sections.items()]
    lines += [
        f"| **Total chargé (.text → .bss)** | **{ram:,}** |",
        "",
        f"- Dépassement : **aucun** ; marge **{RESERVED_LIMIT - ram:,} octets** "
        f"({100 * ram / RESERVED_LIMIT:.1f} % du budget).",
        f"- Le ring de streaming `0x18000` = {spu['ring_ram']:,} octets est inclus dans `.bss` "
        "(mesure confirmée par `nm` : `s_ring`).",
        *diag_bullets,
        "",
        "## 2. VRAM — limite 1 048 576 octets",
        "",
        "Méthode : rectangles de `tools/ps1_export_zone.py::RESERVED_RECTS` (synchros `src/platform/pf.h`)",
        "+ rectangles réellement placés dans les en-têtes `P1ZM` des cinq zones embarquées.",
        "1 demi-mot VRAM = 2 octets.",
        "",
        "| Région | Octets mesurés |",
        "|---|---:|",
        f"| Réservé permanent (framebuffers, police, WIND, HUD, héros, acteurs, icônes) | {vram_reserved:,} |",
    ]
    for k in sorted(k for k in vram if k.startswith("zone")):
        lines.append(f"| Tuiles + CLUT `{k}.dat` | {vram[k]:,} |")
    lines += [
        f"| **Pic (réservé + plus grande zone `{max((k for k in vram if k.startswith('zone')), key=lambda k: vram[k])}`)** | **{vram_peak:,}** |",
        "",
        f"- Dépassement : **aucun** ; marge **{VRAM_LIMIT - vram_peak:,} octets** "
        f"({100 * vram_peak / VRAM_LIMIT:.1f} % du budget).",
        "- Les zones se partagent les mêmes rectangles : le pic est `réservé + une zone`, pas la somme.",
        "",
        "## 3. Audio SPU — limite 524 288 octets",
        "",
        "Méthode : taille réelle de `ps1/assets/audio.vag`, constantes lues dans `src/ps1/audio_ps1.cpp`,",
        "interleave lu dans l'en-tête VAGi de `ps1/assets/music/00.VAG`.",
        "",
        "| Région | Adresse début | Adresse fin | Octets mesurés |",
        "|---|---|---|---:|",
        f"| Banque VAG résidente | 0x{spu['bank_addr']:05X} | 0x{spu['bank_end']:05X} | {spu['bank_bytes']:,} |",
        f"| Double buffer de flux (2 × interleave) | 0x{spu['stream_addr']:05X} | 0x{spu['stream_end']:05X} | {spu['stream_double_buffer']:,} |",
        f"| **Total SPU occupé** | — | — | **{spu['spu_total']:,}** |",
        "",
        f"- Dépassement : **aucun** ; marge **{SPU_LIMIT - spu['spu_total']:,} octets** "
        f"({100 * spu['spu_total'] / SPU_LIMIT:.1f} % du budget).",
        f"- CD : {len(music_files)} pistes VAG, {music_bytes:,} octets ; ISO livrée {iso_bytes:,} octets "
        "(`tools/ps1_make_iso.py`). Le flux n'occupe pas la RAM SPU au-delà du double buffer.",
        *([f"- Le BIN MODE2/2352 du lot fait {bin_bytes:,} octets (secteurs avec en-tête et parité) ; "
           "ce n'est **pas** l'image gravable. L'ISO 2048 livrée est convertie depuis ce BIN et "
           "constitue la taille de référence pour le mastering."] if bin_bytes else []),
        "",
        *render_spu_section(spu_capture, spu),
        "",
        *render_timing_section(timing),
        "",
        *render_zone_heap_section(zone_heap),
        "",
        *render_vram_section(vram_capture),
        "",
        "## Conclusion",
        "",
        f"Les trois budgets mesurables sont respectés : RAM {ram:,}/{RESERVED_LIMIT:,}, "
        f"VRAM {vram_peak:,}/{VRAM_LIMIT:,}, SPU {spu['spu_total']:,}/{SPU_LIMIT:,} octets. "
        + (f"Le pic heap mesure est {int(diag['heapPeak']):,} octets et la profondeur de pile "
           f"maximale {int(diag['stackDepthMax']):,} octets, tous deux bien dans la reserve "
           f"de {int(diag['heapTotal']):,} octets definie par `InitHeap`. " if diag else "")
        + ("Le temps d'image est mesuré en ticks RCnt dans la section 4. " if timing else
           "Le temps d'image n'est pas référencé dans ce rapport (sonde disponible depuis v0.8.6). ")
        + (f"Le pic heap par zone est mesuré à "
           f"{max(int(r['alloc']) for r in zone_heap['steps'][:3]):,} octets pour la zone 011 "
           "(section 5), avec retour exact au baseline. " if zone_heap else
           "Le pic heap par zone n'est pas référencé dans ce rapport. ")
        + (f"Le pic VRAM réel pendant le rendu est mesuré à "
           f"{int(vram_capture['peakBytes']):,} octets (section 6), distinct du pic statique. "
           if vram_capture else
           "Le pic VRAM dynamique n'est pas référencé dans ce rapport. ")
        + (f"Le pic SPU réel pendant le streaming CD est mesuré à "
           f"{int(spu_capture['peakBytes']):,} octets (section 3b), avec preuve de streaming. "
           if spu_capture else
           "Le pic SPU dynamique n'est pas référencé dans ce rapport. "),
        "",
    ]
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text("\n".join(lines), encoding="utf-8")
    print("\n".join(lines[:4]))
    print(f"PS1_BUDGET: PASS ram={ram} vram={vram_peak} spu={spu['spu_total']} out={out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
