"""Launcher routing and environment isolation; no game or compiler required."""
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import setup_ac4 as setup


class LaunchTests(unittest.TestCase):
    def setUp(self):
        work = setup.ROOT / 'work'
        work.mkdir(exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(prefix='ac4 launcher ', dir=work)
        self.root = Path(self.temp.name)
        assert self.root.resolve().parent == work.resolve()
        self.root_patch = patch.object(setup, 'ROOT', self.root)
        self.root_patch.start()
        self.cfg = {'iso': str(self.root / 'game with spaces.iso'), 'toolchain': str(self.root / 'ucrt64')}

    def tearDown(self):
        self.root_patch.stop()
        self.temp.cleanup()

    def exe(self, name):
        p = self.root / 'build' / name / 'ac4.exe'
        p.parent.mkdir(parents=True, exist_ok=True)
        p.touch()
        return p

    def test_first_build_is_playable_without_native_capture(self):
        exe = self.exe('bootstrap')
        with patch.object(setup, 'call') as call:
            setup.run(self.cfg, False, False)
        args, env = call.call_args.args
        self.assertEqual(args[0], exe)
        self.assertEqual(env['PS2_VU_RECOMP'], '0')
        self.assertEqual(args[args.index('--disc') + 1], self.cfg['iso'])

    def test_native_preferred_but_bootstrap_can_be_selected(self):
        native = self.exe('native-poc')
        bootstrap = self.exe('bootstrap')
        with patch.object(setup, 'call') as call:
            setup.run(self.cfg, False, True)
            args, env = call.call_args.args
            self.assertEqual(args[0], native)
            self.assertEqual(env['PS2_VU_RECOMP'], '1')
            self.assertEqual(env['PS2_DISC_PRELOAD'], '1')
            setup.run(self.cfg, False, False, 'bootstrap')
            self.assertEqual(call.call_args.args[0][0], bootstrap)

    def test_explicit_missing_native_does_not_silently_fallback(self):
        self.exe('bootstrap')
        with patch.object(setup, 'call') as call:
            with self.assertRaisesRegex(ValueError, 'Build'):
                setup.run(self.cfg, False, False, 'native')
            call.assert_not_called()

    def test_capture_uses_bootstrap_and_creates_nested_output(self):
        bootstrap = self.exe('bootstrap')
        self.exe('native-poc')
        with patch.object(setup, 'call') as call:
            setup.run(self.cfg, True, False)
        args, env = call.call_args.args
        self.assertEqual(args[0], bootstrap)
        self.assertEqual(env['PS2_VU_CENSUS'], '1')
        self.assertTrue((self.root / 'out/vu_programs').is_dir())

    def test_developer_fault_injection_does_not_leak_into_play(self):
        with patch.dict(os.environ, {'PS2_AC4_MOVIE_TEST_STALL_FIELDS': '60', 'PS2_FPS_CAP': '120'}):
            env = setup.environment(self.cfg)
        self.assertNotIn('PS2_AC4_MOVIE_TEST_STALL_FIELDS', env)
        self.assertNotIn('PS2_FPS_CAP', env)


if __name__ == '__main__':
    unittest.main()
