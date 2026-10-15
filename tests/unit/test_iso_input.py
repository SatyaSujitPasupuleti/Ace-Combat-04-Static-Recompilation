"""Synthetic ISO9660 fixtures; no bytes from the commercial game."""
import hashlib
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import setup_ac4 as setup


class IsoInputTests(unittest.TestCase):
    def setUp(self):
        work = setup.ROOT / 'work'
        work.mkdir(exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(prefix='synthetic-iso-', dir=work)
        self.root = Path(self.temp.name)
        assert self.root.resolve().parent == work.resolve()
        self.iso = self.root / 'test.iso'
        self.payload = b'AC4 TEST FIXTURE: NOT GAME CODE'
        self.profile = dict(setup.PROFILE, elf_sha256=hashlib.sha256(self.payload).hexdigest())

    def tearDown(self):
        self.temp.cleanup()

    def image(self, name=b'SLUS_201.52;1', payload=None):
        data = bytearray(26 * 2048)
        data[16*2048:16*2048+7] = b'\x01CD001\x01'
        root = 16*2048+156
        data[root+2:root+6] = (20).to_bytes(4, 'little')
        data[root+10:root+14] = (2048).to_bytes(4, 'little')
        entry = 20*2048
        data[entry] = 34+len(name)
        data[entry+2:entry+6] = (24).to_bytes(4, 'little')
        data[entry+10:entry+14] = len(self.payload).to_bytes(4, 'little')
        data[entry+32] = len(name)
        data[entry+33:entry+33+len(name)] = name
        body = self.payload if payload is None else payload
        data[24*2048:24*2048+len(body)] = body
        return data

    def extract(self, data):
        self.iso.write_bytes(data)
        with patch.object(setup, 'PROFILE', self.profile):
            return setup.extract(self.iso)

    def test_valid_executable_is_extracted_and_verified(self):
        self.assertEqual(self.extract(self.image()), self.payload)

    def test_modified_executable_is_rejected(self):
        with self.assertRaisesRegex(ValueError, 'hash'):
            self.extract(self.image(payload=b'X'*len(self.payload)))

    def test_wrong_region_is_rejected(self):
        with self.assertRaisesRegex(ValueError, 'not found'):
            self.extract(self.image(name=b'SLES_503.15;1'))

    def test_truncated_executable_is_rejected(self):
        with self.assertRaisesRegex(ValueError, 'hash'):
            self.extract(self.image()[:24*2048+3])

    def test_non_iso_is_rejected(self):
        with self.assertRaisesRegex(ValueError, 'ISO9660'):
            self.extract(b'not an ISO')

    def test_oversized_directory_is_rejected(self):
        data = self.image()
        start = 16*2048+156+10
        data[start:start+4] = (0xffffffff).to_bytes(4, 'little')
        with self.assertRaisesRegex(ValueError, 'directory size'):
            self.extract(data)

    def test_malformed_directory_record_is_rejected(self):
        data = self.image()
        data[20*2048] = 12
        with self.assertRaisesRegex(ValueError, 'directory record'):
            self.extract(data)
