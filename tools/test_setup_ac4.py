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
        with patch.dict(os.environ, {'PS2_AC4_MOVIE_TEST_STALL_FIELDS': '60', 'PS2_FPS_CAP': '120', 'PS2_AC4_TERRAIN_NATIVE': '1', 'PS2_AC4_SKY_MESH': '1', 'PS2_AC4_CLOUD_MESH': '1', 'PS2_AC4_SPRITE_MESH': '1', 'PS2_AC4_ROW_NATIVE': '1'}):
            env = setup.environment(self.cfg)
        self.assertNotIn('PS2_AC4_MOVIE_TEST_STALL_FIELDS', env)
        self.assertNotIn('PS2_FPS_CAP', env)
        self.assertNotIn('PS2_AC4_TERRAIN_NATIVE', env)
        self.assertNotIn('PS2_AC4_SKY_MESH', env)
        self.assertNotIn('PS2_AC4_CLOUD_MESH', env)
        self.assertNotIn('PS2_AC4_SPRITE_MESH', env)
        self.assertNotIn('PS2_AC4_ROW_NATIVE', env)

    def test_hybrid_preset_is_explicit_and_capture_stays_baseline(self):
        self.exe('native-poc');self.exe('bootstrap')
        flags=('MODEL_MESH','MODEL_MATERIAL','MODEL_TEXTURE','MODEL_NATIVE','LIT_NATIVE',
               'TERRAIN_NATIVE','TERRAIN_MESH','COCKPIT_NATIVE','COCKPIT_MESH','SKY_MESH','CLOUD_MESH','SPRITE_MESH',
               'SKY_BAND_NATIVE','ROW_NATIVE','SCREEN_2D','SCREEN_PRECOMPOSE','SCREEN_MASK','HUD_GROUP_NATIVE','SUPPLIED_MIPS')
        with patch.dict(os.environ,{'PS2_AC4_SUPPLIED_FILTER':'2','PS2_AC4_SCREEN_2D':'1','PS2_AC4_COCKPIT_NATIVE':'1','PS2_AC4_COCKPIT_MESH':'1','PS2_AC4_SCREEN_MASK':'1','PS2_AC4_HUD_GROUP_NATIVE':'1'}), patch.object(setup,'call') as call:
            setup.run(self.cfg,False,False,hybrid_renderer=True)
            args,env=call.call_args.args
            self.assertEqual(args[0],self.root/'build/native-poc/ac4.exe')
            for flag in flags:self.assertEqual(env['PS2_AC4_'+flag],'1')
            self.assertEqual(env['PS2_AC4_SUPPLIED_FILTER'],'2')
            self.assertEqual(env['PS2_RN_2D_SCALE'],'0')
            self.assertEqual(env['PS2_RN_2D_OVERLAY'],'1')
            self.assertNotIn('PS2_AC4_ATLAS_TEXTURE',env)
            for capture in (False,True):
                setup.run(self.cfg,capture,False,hybrid_renderer=capture)
                args,env=call.call_args.args
                for flag in flags:self.assertNotEqual(env.get('PS2_AC4_'+flag),'1')
                self.assertNotIn('PS2_AC4_SUPPLIED_FILTER',env)
                if capture:self.assertEqual(args[0],self.root/'build/bootstrap/ac4.exe')

    def test_launch_preserves_fields_even_with_inherited_dropping(self):
        self.exe('native-poc')
        with patch.dict(os.environ,{'PS2_DROP_FIELDS':'1'}), patch.object(setup,'call') as call:
            setup.run(self.cfg,False,False,hybrid_renderer=True)
            self.assertEqual(call.call_args.args[1]['PS2_DROP_FIELDS'],'0')
            self.assertEqual(call.call_args.args[1]['PS2_AC4_FULL_OUTPUT_RES'],'1')

    def test_full_output_resolution_defaults_to_hybrid_only(self):
        self.exe('native-poc');self.exe('bootstrap')
        with patch.dict(os.environ,{'PS2_AC4_FULL_OUTPUT_RES':'1'}), patch.object(setup,'call') as call:
            with self.assertRaisesRegex(ValueError,'requires'):
                setup.run(self.cfg,False,False,full_output_resolution=True)
            setup.run(self.cfg,False,False,hybrid_renderer=True,full_output_resolution=True)
            self.assertEqual(call.call_args.args[1]['PS2_AC4_FULL_OUTPUT_RES'],'1')
            setup.run(self.cfg,False,False,hybrid_renderer=True)
            self.assertEqual(call.call_args.args[1]['PS2_AC4_FULL_OUTPUT_RES'],'1')
            setup.run(self.cfg,False,False)
            self.assertNotIn('PS2_AC4_FULL_OUTPUT_RES',call.call_args.args[1])
            setup.run(self.cfg,True,False,hybrid_renderer=True,full_output_resolution=True)
            self.assertNotIn('PS2_AC4_FULL_OUTPUT_RES',call.call_args.args[1])

    def test_aot_vu_keeps_hybrid_gpu_and_full_resolution(self):
        self.exe('native-poc')
        with patch.dict(os.environ,{'PS2_AC4_MODEL_NATIVE':'1'}), patch.object(setup,'call') as call:
            setup.run(self.cfg,False,False,hybrid_renderer=True,vu_execution='aot')
            args,env=call.call_args.args
            self.assertEqual(args[0],self.root/'build/native-poc/ac4.exe')
            self.assertEqual(env['PS2_VU_RECOMP'],'1')
            self.assertEqual(env['PS2_AC4_FULL_OUTPUT_RES'],'1')
            for feature in ('MODEL_NATIVE','LIT_NATIVE','TERRAIN_NATIVE','COCKPIT_NATIVE'):
                self.assertEqual(env['PS2_AC4_'+feature],'0')
            for feature in ('MODEL_MESH','MODEL_MATERIAL','MODEL_TEXTURE','TERRAIN_MESH',
                            'COCKPIT_MESH','SKY_MESH','CLOUD_MESH','SCREEN_2D','HUD_GROUP_NATIVE'):
                self.assertEqual(env['PS2_AC4_'+feature],'1')
            setup.run(self.cfg,False,False,hybrid_renderer=True)
            for feature in ('MODEL_NATIVE','LIT_NATIVE','TERRAIN_NATIVE','COCKPIT_NATIVE'):
                self.assertEqual(call.call_args.args[1]['PS2_AC4_'+feature],'1')

    def test_aot_vu_requires_hybrid_and_does_not_change_captures(self):
        self.exe('native-poc');self.exe('bootstrap')
        with patch.object(setup,'call') as call:
            with self.assertRaisesRegex(ValueError,'requires --hybrid-renderer'):
                setup.run(self.cfg,False,False,vu_execution='aot')
            call.assert_not_called()
            setup.run(self.cfg,True,False,hybrid_renderer=True,vu_execution='aot')
            self.assertEqual(call.call_args.args[1]['PS2_VU_RECOMP'],'0')
            self.assertEqual(call.call_args.args[1]['PS2_VU_CENSUS'],'1')
            self.assertNotEqual(call.call_args.args[1].get('PS2_AC4_FULL_OUTPUT_RES'),'1')

    def test_hybrid_requires_native_and_rejects_conflicting_bridge(self):
        self.exe('bootstrap')
        with patch.object(setup,'call') as call:
            with self.assertRaisesRegex(ValueError,'Build'):
                setup.run(self.cfg,False,False,hybrid_renderer=True)
            with self.assertRaisesRegex(ValueError,'native build'):
                setup.run(self.cfg,False,False,renderer='bootstrap',hybrid_renderer=True)
            with self.assertRaisesRegex(ValueError,'mesh-bridge'):
                setup.run(self.cfg,False,False,mesh_bridge='triangles',hybrid_renderer=True)
            call.assert_not_called()

    def test_mesh_bridge_requires_explicit_selection(self):
        self.exe('native-poc')
        with patch.dict(os.environ, {'PS2_AC4_MESH_BRIDGE': 'triangles'}), patch.object(setup, 'call') as call:
            setup.run(self.cfg, False, False)
            self.assertEqual(call.call_args.args[1]['PS2_AC4_MESH_BRIDGE'], 'off')
            setup.run(self.cfg, False, False, mesh_bridge='triangles')
            self.assertEqual(call.call_args.args[1]['PS2_AC4_MESH_BRIDGE'], 'triangles')

    def test_capture_keeps_baseline_renderer(self):
        self.exe('bootstrap')
        with patch.object(setup, 'call') as call:
            setup.run(self.cfg, True, False, mesh_bridge='triangles', model_audit=True)
            self.assertEqual(call.call_args.args[1]['PS2_AC4_MODEL_AUDIT'], '0')
            self.assertEqual(call.call_args.args[1]['PS2_AC4_MESH_BRIDGE'], 'off')

    def test_model_audit_is_explicit_and_observer_only(self):
        self.exe('native-poc')
        with patch.dict(os.environ, {'PS2_AC4_MODEL_AUDIT': '1'}), patch.object(setup, 'call') as call:
            setup.run(self.cfg, False, False)
            self.assertEqual(call.call_args.args[1]['PS2_AC4_MODEL_AUDIT'], '0')
            setup.run(self.cfg, False, False, model_audit=True)
            self.assertEqual(call.call_args.args[1]['PS2_AC4_MODEL_AUDIT'], '1')
            self.assertEqual(call.call_args.args[1]['PS2_AC4_MESH_BRIDGE'], 'off')

    def test_model_mesh_is_explicit_and_capture_disables_it(self):
        self.exe('native-poc'); self.exe('bootstrap')
        with patch.dict(os.environ, {'PS2_AC4_MODEL_MESH': '1'}), patch.object(setup, 'call') as call:
            setup.run(self.cfg, False, False)
            self.assertEqual(call.call_args.args[1]['PS2_AC4_MODEL_MESH'], '0')
            setup.run(self.cfg, False, False, model_mesh=True)
            self.assertEqual(call.call_args.args[1]['PS2_AC4_MODEL_MESH'], '1')
            self.assertEqual(call.call_args.args[1]['PS2_AC4_MESH_BRIDGE'], 'off')
            self.assertEqual(call.call_args.args[1]['PS2_AC4_MODEL_AUDIT'], '0')
            setup.run(self.cfg, False, False, model_mesh=True, model_audit=True)
            self.assertEqual(call.call_args.args[1]['PS2_AC4_MODEL_AUDIT'], '1')
            self.assertEqual(call.call_args.args[1]['PS2_AC4_MODEL_MESH'], '1')
            setup.run(self.cfg, True, False, model_mesh=True)
            self.assertEqual(call.call_args.args[1]['PS2_AC4_MODEL_MESH'], '0')


    def test_model_materials_imply_mesh_and_capture_disables_both(self):
        self.exe('native-poc'); self.exe('bootstrap')
        with patch.dict(os.environ, {'PS2_AC4_MODEL_MATERIAL': '1'}), patch.object(setup, 'call') as call:
            setup.run(self.cfg, False, False)
            self.assertEqual(call.call_args.args[1]['PS2_AC4_MODEL_MATERIAL'], '0')
            setup.run(self.cfg, False, False, model_materials=True)
            self.assertEqual(call.call_args.args[1]['PS2_AC4_MODEL_MATERIAL'], '1')
            self.assertEqual(call.call_args.args[1]['PS2_AC4_MODEL_MESH'], '1')
            setup.run(self.cfg, True, False, model_materials=True)
            self.assertEqual(call.call_args.args[1]['PS2_AC4_MODEL_MATERIAL'], '0')
            self.assertEqual(call.call_args.args[1]['PS2_AC4_MODEL_MESH'], '0')

    def test_texture_opt_in_and_capture_isolation(self):
        self.exe('native-poc'); self.exe('bootstrap')
        with patch.dict(os.environ, {'PS2_AC4_MODEL_TEXTURE': '1'}), patch.object(setup, 'call') as call:
            for capture, enabled in ((False, False), (False, True), (True, True)):
                setup.run(self.cfg, capture, False, model_textures=enabled)
                for key in ('TEXTURE', 'MATERIAL', 'MESH'):
                    self.assertEqual(call.call_args.args[1]['PS2_AC4_MODEL_' + key],
                                     '1' if enabled and not capture else '0')

    def test_native_unlit_requires_opt_in_and_capture_disables_it(self):
        self.exe('native-poc'); self.exe('bootstrap')
        with patch.dict(os.environ, {'PS2_AC4_MODEL_NATIVE': '1'}), patch.object(setup, 'call') as call:
            for capture, enabled in ((False,False), (False,True), (True,True)):
                setup.run(self.cfg, capture, False, native_unlit=enabled)
                for key in ('NATIVE','MESH'):
                    self.assertEqual(call.call_args.args[1]['PS2_AC4_MODEL_' + key], '1' if enabled and not capture else '0')

    def test_native_lit_requires_opt_in_and_capture_disables_it(self):
        self.exe('native-poc'); self.exe('bootstrap')
        with patch.dict(os.environ, {'PS2_AC4_LIT_NATIVE': '1', 'PS2_AC4_TERRAIN_MESH': '1'}), patch.object(setup, 'call') as call:
            for capture, enabled in ((False,False), (False,True), (True,True)):
                setup.run(self.cfg, capture, False, native_lit=enabled)
                self.assertEqual(call.call_args.args[1]['PS2_AC4_TERRAIN_MESH'], '0')
                self.assertEqual(call.call_args.args[1]['PS2_AC4_LIT_NATIVE'], '1' if enabled and not capture else '0')
                self.assertEqual(call.call_args.args[1]['PS2_AC4_MODEL_MESH'], '1' if enabled and not capture else '0')

if __name__ == '__main__':
    unittest.main()
