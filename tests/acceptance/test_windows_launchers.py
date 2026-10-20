"""Real Windows launchers with a recording helper, not a game-playthrough test."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class WindowsLauncherAcceptance(unittest.TestCase):
    def setUp(self):
        work = ROOT / 'work'
        work.mkdir(exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(prefix='acceptance spaces ', dir=work)
        self.root = Path(self.temp.name)
        assert self.root.resolve().parent == work.resolve()
        for name in ('build.ps1', 'build.cmd', 'play.cmd'):
            shutil.copy2(ROOT / name, self.root / name)
        (self.root / 'tools').mkdir()
        self.iso = self.root / 'synthetic & spaces.iso'
        self.iso.touch()
        toolchain = self.root / 'fake ucrt64'
        (toolchain / 'bin').mkdir(parents=True)
        for name in ('gcc.exe', 'g++.exe', 'cmake.exe', 'ninja.exe', 'pkg-config.exe', 'glslc.exe'):
            (toolchain / 'bin' / name).touch()  # Discovery only; never executed.
        (self.root / 'local.json').write_text(json.dumps({'iso': str(self.iso), 'toolchain': str(toolchain)}))
        (self.root / 'tools/setup_ac4.py').write_text(
            "import json,os,sys\nfrom pathlib import Path\n"
            "p=Path(__file__).resolve().parents[1]\n"
            "with (p/'calls.jsonl').open('a') as f:f.write(json.dumps(sys.argv[1:])+'\\n')\n"
            "sys.exit(9 if os.environ.get('AC4_TEST_FAIL_ON')==sys.argv[1] else 0)\n", encoding='utf-8')
        self.env = os.environ.copy()
        self.env.update(AC4_NO_PAUSE='1', PATH=str(Path(sys.executable).parent)+os.pathsep+self.env.get('PATH', ''))
        self.env.pop('AC4_TEST_FAIL_ON', None)

    def tearDown(self):
        self.temp.cleanup()

    def invoke(self, *args, launcher='build.cmd', fail=None):
        env = self.env.copy()
        if fail:
            env['AC4_TEST_FAIL_ON'] = fail
        result = subprocess.run([str(self.root / launcher), *args], cwd=self.root,
                                env=env, text=True, capture_output=True, timeout=30)
        path = self.root / 'calls.jsonl'
        calls = [json.loads(line) for line in path.read_text().splitlines()] if path.exists() else []
        return result, calls

    def check(self, expected, *args, fail=None, code=0, launcher='build.cmd'):
        result, calls = self.invoke(*args, fail=fail, launcher=launcher)
        self.assertEqual(result.returncode, code, result.stdout+result.stderr)
        self.assertEqual([c[0] for c in calls], expected)
        return calls

    def test_dropped_iso_builds_without_terminal_setup(self):
        calls = self.check(['prepare', 'build'], str(self.iso), '-NonInteractive')
        self.assertEqual(calls[0][calls[0].index('--iso')+1], str(self.iso))

    def test_saved_paths_build_without_prompt(self):
        self.check(['prepare', 'build'], '-NonInteractive')

    def test_incomplete_gcc_install_is_skipped_for_complete_toolchain(self):
        config = json.loads((self.root / 'local.json').read_text())
        complete = config['toolchain']
        config['toolchain'] = ''
        (self.root / 'local.json').write_text(json.dumps(config))
        incomplete = self.root / 'gcc-only'
        (incomplete / 'bin').mkdir(parents=True)
        (incomplete / 'bin/gcc.exe').touch()
        self.env['AC4_TOOLCHAIN'] = str(incomplete)
        self.env['MSYSTEM_PREFIX'] = complete
        calls = self.check(['prepare', 'build'], '-NonInteractive')
        self.assertEqual(calls[0][calls[0].index('--toolchain')+1], complete)

    def test_explicit_incomplete_toolchain_reports_missing_cmake(self):
        config = json.loads((self.root / 'local.json').read_text())
        (Path(config['toolchain']) / 'bin/cmake.exe').unlink()
        result, calls = self.invoke('-Toolchain', config['toolchain'], '-NonInteractive')
        self.assertEqual(result.returncode, 1)
        self.assertIn('cmake.exe', result.stdout)
        self.assertEqual(calls, [])

    def test_existing_capture_selects_native_build(self):
        path = self.root / 'out/vu_programs'
        path.mkdir(parents=True)
        (path / 'manifest.json').write_text('{}')
        self.check(['prepare', 'build-native'], '-NonInteractive')

    def test_optimize_orders_capture_before_native_build(self):
        self.check(['prepare', 'build', 'capture-vu', 'build-native'], '-Action', 'Optimize', '-NonInteractive')

    def test_prepare_failure_stops_before_compilation(self):
        self.check(['prepare'], '-NonInteractive', fail='prepare', code=1)

    def test_capture_failure_stops_before_native_build(self):
        self.check(['prepare', 'build', 'capture-vu'], '-Action', 'Optimize', '-NonInteractive', fail='capture-vu', code=1)

    def test_missing_iso_fails_without_calling_helper(self):
        self.check([], '-Iso', str(self.root/'missing.iso'), '-NonInteractive', code=1)

    def test_check_does_not_generate_or_build(self):
        self.check(['check'], '-Action', 'Check', '-NonInteractive')

    def test_play_forwards_ram_cache_option(self):
        calls = self.check(['run'], '-DiscCache', 'memory', launcher='play.cmd')
        self.assertEqual(calls[0], ['run', '--disc-cache', 'memory', '--mesh-bridge', 'off'])

    def test_play_opts_into_mesh_bridge(self):
        calls = self.check(['run'], '-MeshBridge', 'triangles', launcher='play.cmd')
        self.assertEqual(calls[0][-2:], ['--mesh-bridge', 'triangles'])

    def test_invalid_mesh_mode_does_not_launch(self):
        self.check([], '-MeshBridge', 'all', launcher='play.cmd', code=1)

    def test_play_forwards_model_audit_without_enabling_meshes(self):
        calls = self.check(['run'], '-ModelAudit', launcher='play.cmd')
        self.assertEqual(calls[0], ['run', '--disc-cache', 'stream', '--mesh-bridge', 'off', '--model-audit'])

    def test_play_forwards_native_model_mesh(self):
        calls = self.check(['run'], '-ModelMesh', launcher='play.cmd')
        self.assertEqual(calls[0], ['run', '--disc-cache', 'stream', '--mesh-bridge', 'off', '--model-mesh'])

    def test_play_forwards_native_model_materials(self):
        calls = self.check(['run'], '-ModelMaterials', launcher='play.cmd')
        self.assertEqual(calls[0], ['run', '--disc-cache', 'stream', '--mesh-bridge', 'off', '--model-materials'])

    def test_play_forwards_native_model_textures(self):
        calls = self.check(['run'], '-ModelTextures', launcher='play.cmd')
        self.assertEqual(calls[0], ['run', '--disc-cache', 'stream', '--mesh-bridge', 'off', '--model-textures'])

    def test_play_forwards_native_unlit(self):
        calls = self.check(['run'], '-NativeUnlit', launcher='play.cmd')
        self.assertEqual(calls[0], ['run', '--disc-cache', 'stream', '--mesh-bridge', 'off', '--native-unlit'])

    def test_play_forwards_hybrid_renderer(self):
        calls = self.check(['run'], '-HybridRenderer', launcher='play.cmd')
        self.assertIn('--hybrid-renderer', calls[0])

    def test_play_forwards_aot_vu_policy(self):
        calls=self.check(['run'],'-HybridRenderer','-VuExecution','Aot',launcher='play.cmd')
        self.assertIn('--hybrid-renderer',calls[0])
        i=calls[0].index('--vu-execution')
        self.assertEqual(calls[0][i+1],'aot')

    def test_invalid_vu_policy_does_not_launch(self):
        self.check([], '-VuExecution', 'interpolated', launcher='play.cmd', code=1)

    def test_play_forwards_full_output_resolution(self):
        calls = self.check(['run'], '-HybridRenderer', '-FullOutputResolution', launcher='play.cmd')
        self.assertIn('--hybrid-renderer',calls[0])
        self.assertIn('--full-output-resolution',calls[0])

    def test_play_forwards_native_lit(self):
        calls = self.check(['run'], '-NativeLit', launcher='play.cmd')
        self.assertEqual(calls[0], ['run', '--disc-cache', 'stream', '--mesh-bridge', 'off', '--native-lit'])
