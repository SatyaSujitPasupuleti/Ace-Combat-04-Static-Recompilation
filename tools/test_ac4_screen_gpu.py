"""Opt-in AC4 screen-sprite GPU parity on one private DMA command stream."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import setup_ac4 as setup
from test_ac4_model_gpu import require_dma_capture, require_visible_ppm, require_complete_capture


def validate_screen_report(text, overlay=False, precompose=False, display=True):
    rows = re.findall(r'rn: 2D layer -- (\d+) elements \((\d+) accepted whole\), (\d+) primitives: (\d+) accepted, (\d+) kept emulated; (\d+) elements not closed by their end record', text)
    if len(rows) != 1:
        raise ValueError('Require exactly one native 2D coverage report')
    elements, whole, primitives, accepted, fallback, unclosed = map(int, rows[0])
    if not elements or not whole or whole > elements or not accepted or primitives != accepted + fallback or unclosed:
        raise ValueError('Require positive native elements/primitives and closed consistent ownership')
    if 'overlay list overflowed' in text:
        raise ValueError('Overlay list overflowed')
    if overlay:
        reports = re.findall(r'vk: 2D overlay composed on (\d+) frames at (\d+)x, (\d+) draws lifted onto it', text)
        if len(reports) != 1 or not all(map(int, reports[0])):
            raise ValueError('Require positive overlay frames, scale and lifted draws')
    if precompose:
        rows = re.findall(r'vk: AC4 pre-display overlay composed on (\d+) transfers', text)
        if len(rows) != 1 or int(rows[0]) < 1:
            raise ValueError('Require positive pre-display composition coverage')
        displays = re.findall(r'vk: AC4 native display: (\d+) copies, (\d+) presented, (\d+)x at (\d+)x(\d+)', text)
        if display and (len(displays) != 1 or not all(map(int, displays[0]))):
            raise ValueError('Require a copied and presented native display target')
        if not display and displays:
            raise ValueError('Forced display fallback must not copy/present the native display target')
    return dict(elements=elements, whole=whole, accepted=accepted, fallback=fallback)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('capture', type=Path)
    ap.add_argument('--field', type=int, default=2)
    ap.add_argument('--precompose', action='store_true', help='Test experimental pre-display composition at 1x')
    ap.add_argument('--display-fallback', action='store_true', help='Force native display allocation fallback and require exact regular-target output')
    args = ap.parse_args()
    if args.display_fallback:
        args.precompose = True
    if args.field < 1:
        ap.error('field must be positive')
    require_complete_capture(args.capture, args.field)
    root = Path(__file__).resolve().parents[1]
    output = Path(tempfile.mkdtemp(prefix='screen-gpu-parity-', dir=root / 'work'))
    build = root / 'build/native-poc'
    env = setup.environment(json.loads(setup.LOCAL.read_text()))
    env.update(PS2_SHADER_DIR=str(build / 'shaders'), PS2_VU_RECOMP='1',
               PS2_SETTINGS_FILE=str(output / 'settings.ini'), PS2_UPSCALE='1',
               PS2_LOCKSTEP='1', PS2_RN_2D_SCALE='1', PS2_AC4_SKY_MESH='1', PS2_AC4_CLOUD_MESH='1', PS2_AC4_SPRITE_MESH='1')
    print(f'Private evidence: {output}', flush=True)
    reference = None
    results = []
    for mode in ('gs', 'native-inplace', 'native-overlay'):
        env['PS2_AC4_SCREEN_2D'] = '0' if mode == 'gs' else '1'
        env['PS2_RN_2D_OVERLAY'] = '1' if mode == 'native-overlay' else '0'
        env['PS2_AC4_SCREEN_PRECOMPOSE'] = '1' if args.precompose else '0'
        env['PS2_AC4_SCREEN_DISPLAY_FALLBACK'] = '1' if args.display_fallback else '0'
        shot, log = output / (mode + '.ppm'), output / (mode + '.log')
        with log.open('w') as f:
            subprocess.run([str(build / 'gsreplay.exe'), str(args.capture.resolve()), '--loop', '1',
                            '--fields', str(args.field), '--hidden', '--shot', str(shot)], cwd=output,
                           env=env, stdout=f, stderr=f, check=True, timeout=120)
        image = shot.read_bytes()
        require_visible_ppm(image)
        coverage = {} if mode == 'gs' else validate_screen_report(log.read_text(errors='replace'), mode == 'native-overlay', args.precompose and mode == 'native-overlay', not args.display_fallback)
        if reference is None:
            reference = image
        exact = image == reference
        results.append(dict(mode=mode, precompose=args.precompose, display_fallback=args.display_fallback, exact=exact, sha256=hashlib.sha256(image).hexdigest(), **coverage))
        (output / 'comparison.json').write_text(json.dumps(results, indent=2))
        if not exact:
            raise RuntimeError(f'{mode} differs from GS on the same command stream; inspect {output}')
        print(f'{mode}: exact image match', flush=True)
    print('Captured screen sprites passed; this is not full HUD/radar/caption coverage.')


if __name__ == '__main__':
    main()
