import unittest
import struct
import tempfile
from pathlib import Path
from test_ac4_screen_gpu import validate_screen_report, require_complete_capture


class ScreenGpuEvidenceTests(unittest.TestCase):
    good = 'rn: 2D layer -- 8 elements (7 accepted whole), 8 primitives: 7 accepted, 1 kept emulated; 0 elements not closed by their end record\n'
    overlay = 'vk: 2D overlay composed on 2 frames at 1x, 14 draws lifted onto it\n'

    def test_complete_coverage_and_explicit_fallback(self):
        self.assertEqual(validate_screen_report(self.good)['fallback'], 1)
        self.assertEqual(validate_screen_report(self.good + self.overlay, True)['accepted'], 7)

    def test_precompose_requires_actual_transfers(self):
        transfer = 'vk: AC4 pre-display overlay composed on 1 transfers\n'
        display = 'vk: AC4 native display: 1 copies, 1 presented, 2x at 1280x896\n'
        validate_screen_report(self.good + self.overlay + transfer + display, True, True)
        validate_screen_report(self.good + self.overlay + transfer, True, True, display=False)
        with self.assertRaises(ValueError):
            validate_screen_report(self.good + self.overlay + transfer + display, True, True, display=False)
        for extra in ('', transfer * 2, transfer.replace('1 transfers', '0 transfers')):
            with self.subTest(extra=extra), self.assertRaises(ValueError):
                validate_screen_report(self.good + self.overlay + extra + display, True, True)
        for extra in ('', display * 2, display.replace('1 presented', '0 presented'), display.replace('1280x896', '0x896')):
            with self.subTest(extra=extra), self.assertRaises(ValueError):
                validate_screen_report(self.good + self.overlay + transfer + extra, True, True)

    def test_missing_or_contradictory_evidence(self):
        for text in ('', self.good * 2, self.good.replace('7 accepted', '0 accepted'),
                     self.good.replace('8 primitives', '9 primitives'),
                     self.good.replace('0 elements not closed', '1 elements not closed'),
                     self.good + 'overlay list overflowed'):
            with self.subTest(text=text), self.assertRaises(ValueError):
                validate_screen_report(text)
        for text in ('', self.overlay * 2, self.overlay.replace('14 draws', '0 draws')):
            with self.subTest(text=text), self.assertRaises(ValueError):
                validate_screen_report(self.good + text, True)


class ScreenCaptureIntegrityTests(unittest.TestCase):
    def test_truncation_and_unavailable_fields_reject(self):
        header = bytearray(176)
        header[:8] = b'GSCAP2\0\0'
        struct.pack_into('<II', header, 8, 2, 1)
        struct.pack_into('<QQQ', header, 96, 176, 20, 196)
        struct.pack_into('<I', header, 128, 2)
        data = header + bytes(20 + 32)
        work = Path(__file__).resolve().parents[2] / 'work'
        work.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=work) as td:
            assert Path(td).resolve().parent == work.resolve()
            p = Path(td) / 'fixture.gscap'
            p.write_bytes(data)
            self.assertEqual(require_complete_capture(p, 2), 2)
            for field in (0, 3):
                with self.assertRaises(ValueError): require_complete_capture(p, field)
            for length in (8, 175, 176, 195, 196, len(data) - 1):
                p.write_bytes(data[:length])
                with self.subTest(length=length), self.assertRaises(ValueError): require_complete_capture(p, 1)
            struct.pack_into('<I', data, 136, 1)
            p.write_bytes(data)
            with self.assertRaises(ValueError): require_complete_capture(p, 1)
