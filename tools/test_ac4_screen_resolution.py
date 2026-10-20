"""Private-capture native display resolution smoke test (not visual parity)."""
import argparse
import json
import math
from pathlib import Path
import re
import subprocess
import tempfile

import setup_ac4 as setup
from test_ac4_model_gpu import require_visible_ppm
from test_ac4_screen_gpu import require_complete_capture, validate_screen_report


def dimensions(data):
    require_visible_ppm(data)
    return tuple(map(int, re.match(rb'P6\s+(\d+)\s+(\d+)', data).groups()))


def validate_resolution(base, native, report, height, scale=0):
    bw, bh = dimensions(base)
    nw, nh = dimensions(native)
    validate_screen_report(report, overlay=True, precompose=True)
    expected = scale or math.ceil(height / bh)
    if (nw, nh) != (bw * expected, bh * expected):
        raise ValueError('Native output does not match the requested/automatic scale')
    shown = re.findall(r'vk: AC4 native display: \d+ copies, \d+ presented, (\d+)x at (\d+)x(\d+)', report)
    k, tw, th = map(int, shown[0])
    if k != expected or tw < nw or th < nh:
        raise ValueError('Native display target does not cover its output rectangle')
    return dict(scale=k, output=[nw, nh], target=[tw, th])


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('capture', type=Path)
    ap.add_argument('--output-root', type=Path, required=True)
    ap.add_argument('--width', type=int, default=3840)
    ap.add_argument('--height', type=int, default=2160)
    ap.add_argument('--scale', type=int, default=0, choices=range(9))
    ap.add_argument('--field', type=int, default=2)
    args = ap.parse_args()
    if not (320 <= args.width <= 7680 and 224 <= args.height <= 4320):
        ap.error('Unsupported test window dimensions')
    require_complete_capture(args.capture, args.field)
    root = Path(__file__).resolve().parents[1]
    args.output_root.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='screen-resolution-', dir=args.output_root.resolve()))
    build = root / 'build/native-poc'
    env = setup.environment(json.loads(setup.LOCAL.read_text()))
    env.update(PS2_SHADER_DIR=str(build / 'shaders'), PS2_VU_RECOMP='1', PS2_LOCKSTEP='1',
               PS2_UPSCALE='1', PS2_RN_2D_SCALE=str(args.scale), PS2_AC4_SKY_MESH='1',
               PS2_AC4_CLOUD_MESH='1', PS2_AC4_SPRITE_MESH='1')
    print(f'Private evidence: {output}', flush=True)
    for mode in ('baseline', 'native'):
        enabled = str(int(mode == 'native'))
        env.update(PS2_AC4_SCREEN_2D=enabled, PS2_AC4_SCREEN_PRECOMPOSE=enabled,
                   PS2_RN_2D_OVERLAY=enabled)
        settings = output / (mode + '.ini')
        settings.write_text(f'[display]\nwindow_mode=0\nwindow_width={args.width}\nwindow_height={args.height}\n')
        env['PS2_SETTINGS_FILE'] = str(settings)
        (output / (mode + '-config.json')).write_text(json.dumps({k:v for k,v in env.items() if k.startswith('PS2_')}, indent=2))
        with (output / (mode + '.log')).open('w') as log:
            subprocess.run([str(build / 'gsreplay.exe'), str(args.capture.resolve()), '--hidden',
                            '--loop', '1', '--fields', str(args.field), '--shot', str(output / (mode + '.ppm'))],
                           cwd=output, env=env, stdout=log, stderr=log, check=True, timeout=120)
    result = validate_resolution((output / 'baseline.ppm').read_bytes(),
                                 (output / 'native.ppm').read_bytes(),
                                 (output / 'native.log').read_text(), args.height, args.scale)
    (output / 'resolution.json').write_text(json.dumps(result, indent=2))
    print(f'Resolution smoke passed: {result}. Visual, timing and full HUD coverage remain separate gates.')


if __name__ == '__main__':
    main()
