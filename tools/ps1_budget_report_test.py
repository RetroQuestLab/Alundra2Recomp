#!/usr/bin/env python3
"""Harnais TDD du rapport de budget PS1 — parse du log de temps d'image.

Usage : py -3.12 tools/ps1_budget_report_test.py
Aucune valeur n'est codee en dur pour l'implementation : les attentes viennent
du log reel qa/ps1_logs/frame_timing_v0819/timing_stdout.txt et de son meta.
"""
from __future__ import annotations

import importlib.util
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
MODULE_PATH = REPO / "tools" / "ps1_budget_report.py"
REAL_LOG = REPO / "qa" / "ps1_logs" / "frame_timing_v0819" / "timing_stdout.txt"
REAL_META = REAL_LOG.with_name("timing_meta.txt")

spec = importlib.util.spec_from_file_location("ps1_budget_report", MODULE_PATH)
budget = importlib.util.module_from_spec(spec)
assert spec.loader is not None
spec.loader.exec_module(budget)


def read_expected_sha() -> str:
    import re
    text = REAL_META.read_text(encoding="utf-8", errors="replace")
    m = re.search(r"(?m)^SHA256=([0-9A-Fa-f]+)$", text)
    assert m is not None
    return m.group(1).upper()


def sample_log() -> str:
    return REAL_LOG.read_text(encoding="utf-8", errors="replace")


class TimingLogTests(unittest.TestCase):
    def test_timing_parses_real_log(self):
        timing = budget.parse_timing_log(REAL_LOG, read_expected_sha())
        self.assertEqual(timing["samples"], 1000)
        self.assertEqual(timing["ticks_p50"], 18145)
        self.assertEqual(timing["ticks_p95"], 18206)
        self.assertEqual(timing["ticks_max"], 60004)
        self.assertEqual(timing["ms_nominal_p50"], 17.224)
        self.assertEqual(timing["outliers"], 1)
        self.assertEqual(timing["max_index"], 1)

    def test_timing_rejects_short_sample(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "short.txt"
            p.write_text(sample_log().replace("samples=1000", "samples=999"),
                         encoding="utf-8")
            with self.assertRaises(RuntimeError):
                budget.parse_timing_log(p)

    def test_timing_requires_nominal_fields(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "no_ms.txt"
            body = "\n".join(line for line in sample_log().splitlines()
                             if "ms_nominal_p50" not in line)
            p.write_text(body + "\n", encoding="utf-8")
            with self.assertRaises(RuntimeError):
                budget.parse_timing_log(p)

    def test_timing_rejects_wrong_iso(self):
        with self.assertRaises(RuntimeError):
            budget.parse_timing_log(REAL_LOG, "00" * 32)

    def test_no_timing_log_uses_honest_wording(self):
        text = "\n".join(budget.render_timing_section(None))
        self.assertIn("non référencé", text)
        self.assertNotIn("reste à instrumenter", text)
        self.assertNotIn("n'est pas encore instrumenté", text)

    def test_timing_keeps_outlier(self):
        timing = budget.parse_timing_log(REAL_LOG, read_expected_sha())
        text = "\n".join(budget.render_timing_section(timing))
        self.assertIn("60004", text)
        self.assertIn("outliers_gt_20000=1", text)
        self.assertIn("max_index=1", text)

    def test_report_labels_nominal_ms(self):
        timing = budget.parse_timing_log(REAL_LOG, read_expected_sha())
        text = "\n".join(budget.render_timing_section(timing))
        self.assertIn("nominal", text)
        self.assertIn("ticks", text)


class ZoneHeapLogTests(unittest.TestCase):
    REAL = REPO / "qa" / "ps1_logs" / "zone_heap_v0820" / "zone_heap_capture.txt"

    def sample_zone(self) -> str:
        return self.REAL.read_text(encoding="utf-8", errors="replace")

    def expected_sha(self) -> str:
        import re
        m = re.search(r"(?m)^ISO_SHA256=([0-9A-Fa-f]+)$", self.sample_zone())
        assert m is not None
        return m.group(1).upper()

    def test_zone_parses_real_log(self):
        data = budget.parse_zone_heap_log(self.REAL, self.expected_sha())
        self.assertEqual(data["loadCount"], 3)
        self.assertEqual(data["releaseCount"], 3)
        self.assertEqual(data["baselineAlloc"], 4096)
        self.assertEqual(data["finalAlloc"], 4096)
        self.assertEqual(data["staticRam"], 1785044)
        self.assertEqual(len(data["steps"]), 4)
        self.assertEqual([s["zone"] for s in data["steps"]], [13, 26, 11, 0])
        self.assertEqual([s["alloc"] for s in data["steps"]],
                         [159744, 106496, 225280, 225280])
        self.assertEqual([s["bytes"] for s in data["steps"]],
                         [154636, 102282, 220006, 220006])

    def test_zone_rejects_log_without_pass(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "no_pass.txt"
            p.write_text(self.sample_zone().replace(
                "PS1_CAPTURE_ZONE_HEAP: PASS", "PS1_CAPTURE_ZONE_HEAP: FAIL"),
                encoding="utf-8")
            with self.assertRaises(RuntimeError):
                budget.parse_zone_heap_log(p)

    def test_zone_rejects_wrong_load_count(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "load.txt"
            p.write_text(self.sample_zone().replace("loadCount=3", "loadCount=2"),
                         encoding="utf-8")
            with self.assertRaises(RuntimeError):
                budget.parse_zone_heap_log(p)

    def test_zone_rejects_leak(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "leak.txt"
            p.write_text(self.sample_zone().replace("finalAlloc=4096", "finalAlloc=7000"),
                         encoding="utf-8")
            with self.assertRaises(RuntimeError):
                budget.parse_zone_heap_log(p)

    def test_zone_rejects_wrong_iso(self):
        with self.assertRaises(RuntimeError):
            budget.parse_zone_heap_log(self.REAL, "00" * 32)

    def test_zone_render_reports_measured_values(self):
        data = budget.parse_zone_heap_log(self.REAL, self.expected_sha())
        text = "\n".join(budget.render_zone_heap_section(data))
        for value in ("159744", "161792", "225280", "4096", "1785044"):
            self.assertIn(value, text)
        self.assertIn("mesuré", text)
        self.assertIn("154636", text)
        self.assertIn("102282", text)
        self.assertIn("220006", text)


class VramLogTests(unittest.TestCase):
    REAL = REPO / "qa" / "ps1_logs" / "vram_v0821" / "vram_capture.txt"

    def sample_vram(self) -> str:
        return self.REAL.read_text(encoding="utf-8", errors="replace")

    def expected_sha(self) -> str:
        import re
        m = re.search(r"(?m)^ISO_SHA256=([0-9A-Fa-f]+)$", self.sample_vram())
        assert m is not None
        return m.group(1).upper()

    def test_vram_parses_real_log(self):
        data = budget.parse_vram_log(self.REAL, self.expected_sha())
        self.assertEqual(data["peakBytes"], 594272)
        self.assertEqual(data["texturePeakBytes"], 287072)
        self.assertEqual(data["framebufferBytes"], 307200)
        self.assertEqual(data["rejections"], 0)
        self.assertEqual([p["phase"] for p in data["phases"]],
                         ["menu", "game", "inventory"])
        self.assertEqual([p["peakBytes"] for p in data["phases"]],
                         [572576, 592928, 594272])

    def test_vram_rejects_log_without_pass(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "no_pass.txt"
            p.write_text(self.sample_vram().replace(
                "PS1_CAPTURE_VRAM: PASS", "PS1_CAPTURE_VRAM: FAIL"),
                encoding="utf-8")
            with self.assertRaises(RuntimeError):
                budget.parse_vram_log(p)

    def test_vram_rejects_bad_integrity(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "bad_integrity.txt"
            p.write_text(self.sample_vram().replace(
                "integrity=0xa256545b", "integrity=0x00000000", 1),
                encoding="utf-8")
            with self.assertRaises(RuntimeError):
                budget.parse_vram_log(p)

    def test_vram_rejects_wrong_iso(self):
        with self.assertRaises(RuntimeError):
            budget.parse_vram_log(self.REAL, "00" * 32)

    def test_vram_rejects_rejections(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "rejections.txt"
            p.write_text(self.sample_vram().replace(
                "rejections=0", "rejections=1"), encoding="utf-8")
            with self.assertRaises(RuntimeError):
                budget.parse_vram_log(p)

    def test_vram_render_reports_measured_values(self):
        data = budget.parse_vram_log(self.REAL, self.expected_sha())
        text = "\n".join(budget.render_vram_section(data))
        self.assertIn("594,272", text)
        self.assertIn("287,072", text)
        self.assertIn("mesuré", text)
        self.assertNotIn("non instrumenté", text)


class SpuLogTests(unittest.TestCase):
    REAL = REPO / "qa" / "ps1_logs" / "spu_v0822" / "spu_capture.txt"

    def sample_spu(self) -> str:
        return self.REAL.read_text(encoding="utf-8", errors="replace")

    def expected_sha(self) -> str:
        import re
        m = re.search(r"(?m)^ISO_SHA256=([0-9A-Fa-f]+)$", self.sample_spu())
        assert m is not None
        return m.group(1).upper()

    def test_spu_parses_real_log(self):
        data = budget.parse_spu_log(self.REAL, self.expected_sha())
        self.assertEqual(data["peakBytes"], 394688)
        self.assertEqual(data["bankBytes"], 378304)
        self.assertEqual(data["streamBytes"], 16384)
        self.assertEqual(data["transfers"], 7)
        self.assertEqual(data["chunks"], 6)
        self.assertEqual(data["underruns"], 0)
        self.assertEqual(data["overlapBytes"], 32768)
        self.assertEqual(data["peakFrame"], 0)
        self.assertEqual(data["integrity"], 0xA253D043)
        self.assertEqual([p["phase"] for p in data["phases"]],
                         ["menu", "game", "inventory"])
        self.assertEqual([p["chunks"] for p in data["phases"]], [0, 6, 6])

    def test_spu_rejects_log_without_pass(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "no_pass.txt"
            p.write_text(self.sample_spu().replace(
                "PS1_CAPTURE_SPU: PASS", "PS1_CAPTURE_SPU: FAIL"), encoding="utf-8")
            with self.assertRaises(RuntimeError):
                budget.parse_spu_log(p)

    def test_spu_rejects_bad_integrity(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "bad_integrity.txt"
            p.write_text(self.sample_spu().replace(
                "integrity=0xa2535047", "integrity=0x00000000", 1), encoding="utf-8")
            with self.assertRaises(RuntimeError):
                budget.parse_spu_log(p)

    def test_spu_rejects_wrong_iso(self):
        with self.assertRaises(RuntimeError):
            budget.parse_spu_log(self.REAL, "00" * 32)

    def test_spu_rejects_wrong_phase_order(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "phase_order.txt"
            lines = self.sample_spu().splitlines()
            lines[12], lines[16] = lines[16], lines[12]
            p.write_text("\n".join(lines) + "\n", encoding="utf-8")
            with self.assertRaises(RuntimeError):
                budget.parse_spu_log(p)

    def test_spu_rejects_rejections(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "rejections.txt"
            p.write_text(self.sample_spu().replace(
                "rejections=0", "rejections=1"), encoding="utf-8")
            with self.assertRaises(RuntimeError):
                budget.parse_spu_log(p)

    def test_spu_rejects_non_monotonic_peak(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "non_monotonic.txt"
            lines = self.sample_spu().splitlines()
            lines[14] = lines[14].replace(
                "currentBytes=394688 peakBytes=394688",
                "currentBytes=394000 peakBytes=394000")
            p.write_text("\n".join(lines) + "\n", encoding="utf-8")
            with self.assertRaises(RuntimeError):
                budget.parse_spu_log(p)

    def test_spu_rejects_divergent_result(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "divergent_result.txt"
            p.write_text(self.sample_spu().replace(
                "SPU_RESULT peakBytes=394688", "SPU_RESULT peakBytes=123456"),
                encoding="utf-8")
            with self.assertRaises(RuntimeError):
                budget.parse_spu_log(p)

    def test_spu_rejects_bank_stream_mismatch(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "bank_mismatch.txt"
            p.write_text(self.sample_spu().replace(
                "bankBytes=378304", "bankBytes=378296", 1), encoding="utf-8")
            with self.assertRaises(RuntimeError):
                budget.parse_spu_log(p)

    def test_spu_rejects_missing_streaming_proof(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "no_streaming.txt"
            lines = self.sample_spu().splitlines()
            lines[16] = lines[16].replace("chunks=6", "chunks=0")
            p.write_text("\n".join(lines) + "\n", encoding="utf-8")
            with self.assertRaises(RuntimeError):
                budget.parse_spu_log(p)

    def test_spu_render_reports_measured_values(self):
        data = budget.parse_spu_log(self.REAL, self.expected_sha())
        text = "\n".join(budget.render_spu_section(data, None))
        for value in ("394,688", "378,304", "16,384", "mesuré", "6"):
            self.assertIn(value, text)
        self.assertNotIn("non instrumenté", text)

    def test_no_spu_log_uses_honest_wording(self):
        text = "\n".join(budget.render_spu_section(None, {"spu_total": 394688}))
        self.assertIn("non référencé", text)
        self.assertIn("394,688", text)
        self.assertIn("calcul statique", text)


if __name__ == "__main__":
    unittest.main(verbosity=2)
