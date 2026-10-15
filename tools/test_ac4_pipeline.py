"""Input rejection tests; do not invoke the compiler or game."""
import hashlib, json, pathlib, tempfile, unittest
from unittest.mock import patch
import ac4

class PipelineTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(dir=ac4.WORK)
        self.root=pathlib.Path(self.tmp.name)
        self.work=self.root/'work'; self.work.mkdir()
        self.patches=[patch.object(ac4,'ROOT',self.root),patch.object(ac4,'WORK',self.work)]
        for p in self.patches: p.start()
        (self.root/'CMakeLists.txt').write_text('test')
        (self.root/'config/ac4').mkdir(parents=True)
        (self.root/'config/ac4/seeds.json').write_text('{}')
    def tearDown(self):
        for p in self.patches: p.stop()
        self.tmp.cleanup()
    def test_wrong_elf_rejected_before_generation(self):
        elf=self.root/'bad.elf'; elf.write_bytes(b'not the game')
        with patch.object(ac4,'command') as command:
            with self.assertRaisesRegex(SystemExit,'ELF hash'):
                ac4.generate(elf)
            command.assert_not_called()
    def test_stale_profile_rejected_before_build(self):
        (self.work/'generation_manifest.json').write_text(json.dumps({'inputs':{},'outputs':{}}))
        with patch.object(ac4,'command') as command:
            with self.assertRaisesRegex(SystemExit,'configuration changed'):
                ac4.build(1)
            command.assert_not_called()
    def test_modified_binary_rejected_before_trace(self):
        disc=self.root/'disc.iso'; disc.write_bytes(b'disc')
        (self.root/'build/ac4').mkdir(parents=True)
        (self.root/'build/ac4/ac4.exe').write_bytes(b'changed')
        (self.work/'build_manifest.json').write_text(json.dumps({'executable_sha256':hashlib.sha256(b'original').hexdigest()}))
        with patch.object(ac4,'command') as command:
            with self.assertRaisesRegex(SystemExit,'Executable changed'):
                ac4.trace(disc,1,False)
            command.assert_not_called()

if __name__=='__main__': unittest.main()
