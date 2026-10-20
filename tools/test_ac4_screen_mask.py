"""Experimental ordered high-resolution destination-alpha overlay acceptance."""
import argparse
import hashlib
import json
import re
from pathlib import Path
import subprocess
import tempfile
import setup_ac4 as setup
from test_ac4_model_gpu import require_complete_capture, require_visible_ppm


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('capture', type=Path)
    ap.add_argument('--fields', type=int, default=2)
    ap.add_argument('--scale', type=int, default=1)
    ap.add_argument('--native-geometry', action='store_true', help='Also compare native models/terrain/materials with the masked overlay')
    ap.add_argument('--output-root', type=Path, required=True)
    args = ap.parse_args()
    if args.fields < 1 or args.scale < 1:
        ap.error('fields and scale must be positive')
    require_complete_capture(args.capture, args.fields)
    root = Path(__file__).resolve().parents[1]
    out = Path(tempfile.mkdtemp(prefix='screen-mask-', dir=args.output_root))
    env = setup.environment(json.loads(setup.LOCAL.read_text()))
    # Two loops exercise the warmed overlay allocation; replay resets ownership
    # and hazard cooldowns between loops, while retaining allocated GPU targets.
    env.update(PS2_SHADER_DIR=str(root / 'build/native-poc/shaders'),
               PS2_SETTINGS_FILE=str(out / 'settings.ini'), PS2_UPSCALE='1',
               PS2_LOCKSTEP='1', PS2_RN_2D_SCALE=str(args.scale),
               PS2_AC4_SCREEN_PRECOMPOSE='1', PS2_RN_2D_OVERLAY='1',
               PS2_RN_OV_REGION='10,290,140,440', PS2_RN_OV_DETAIL='1')
    results = []
    reference = None
    print(out, flush=True)
    modes = ('gs', 'overlay', 'mask', 'native-mask') if args.native_geometry else ('gs', 'overlay', 'mask')
    for mode in modes:
        env['PS2_AC4_SCREEN_2D'] = '0' if mode == 'gs' else '1'
        env['PS2_AC4_SCREEN_MASK'] = '1' if mode.endswith('mask') else '0'
        for feature in ('MODEL_MESH', 'MODEL_MATERIAL', 'MODEL_NATIVE', 'LIT_NATIVE',
                        'TERRAIN_NATIVE', 'TERRAIN_MESH', 'SKY_MESH', 'CLOUD_MESH',
                        'SPRITE_MESH', 'SKY_BAND_NATIVE', 'ROW_NATIVE', 'SUPPLIED_MIPS'):
            env['PS2_AC4_' + feature] = '1' if mode == 'native-mask' else '0'
        # Preserve original texture filtering for the byte-exact oracle.
        env['PS2_AC4_SUPPLIED_FILTER'] = '0'
        log, shot = out / f'{mode}.log', out / f'{mode}.ppm'
        with log.open('w') as stream:
            subprocess.run([str(root / 'build/native-poc/gsreplay.exe'),
                            str(args.capture.resolve()), '--hidden', '--loop', '2',
                            '--fields', str(args.fields), '--shot', str(shot)],
                           cwd=out, env=env, stdout=stream, stderr=stream,
                           check=True, timeout=120)
        pixels = shot.read_bytes()
        require_visible_ppm(pixels)
        if reference is None:
            reference = pixels
        text = log.read_text(errors='replace')
        masks = text.count('vk: overlay DATE snapshot at ')
        results.append(dict(mode=mode, sha256=hashlib.sha256(pixels).hexdigest(),
                            exact=pixels == reference, snapshot_reports=masks))
        (out / 'comparison.json').write_text(json.dumps(results, indent=2))
        if mode.endswith('mask') and not masks:
            raise RuntimeError('No DATE snapshot executed; this is not mask acceptance')
        if mode == 'native-mask':
            terrain = re.search(r'ac4 terrain GPU: (\d+) primary, (\d+) secondary draws', text)
            if not terrain or not int(terrain[1]):
                raise RuntimeError('No native terrain draws; this is not combined geometry acceptance')
        if args.scale == 1 and pixels != reference:
            raise RuntimeError(f'{mode}: ordered overlay differs from the GS reference')
        print(results[-1], flush=True)
    print('Captured mask path passed; higher-scale output still needs visual review.')


if __name__ == '__main__':
    main()
