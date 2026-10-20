"""Opt-in sky GPU acceptance; requires a private DMA capture with sky intents."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile

import setup_ac4 as setup
from test_ac4_model_gpu import require_dma_capture, require_visible_ppm


def validate_sky_report(text, require_haze=False, require_clouds=False, require_sprites=False, require_sprite_gif=False):
    reports = re.findall(r'ac4 native sky: (\d+) intents, (\d+) raw GPU triangles, (\d+) rejected, (\d+) unused triangles', text)
    if len(reports) != 1:
        raise ValueError('Require exactly one native sky coverage report')
    intents, triangles, rejected, unused = map(int, reports[0])
    if not intents or not triangles or rejected or unused:
        raise ValueError('Require positive native sky coverage with no rejected or unused triangles')
    if require_haze:
        haze = re.findall(r'ac4 native haze: (\d+) intents, (\d+) raw GPU triangles', text)
        if len(haze) != 1:
            raise ValueError('Require exactly one native haze coverage report')
        hi, ht = map(int, haze[0])
        if not hi or not ht or hi > intents or ht > triangles:
            raise ValueError('Require positive native haze coverage consistent with total sky coverage')
    if require_clouds:
        clouds=re.findall(r'ac4 native clouds: (\d+) intents, (\d+) raw GPU triangles, (\d+) rejected, (\d+) unused triangles',text)
        if len(clouds)!=1:raise ValueError('Require exactly one native cloud report')
        ci,ct,cr,cu=map(int,clouds[0])
        if not ci or not ct or cr or cu:raise ValueError('Require positive native cloud coverage without rejection or unused triangles')
    if require_sprites:
        reports=re.findall(r'ac4 native sprites: (\d+) intents, (\d+) raw GPU pieces, (\d+) rejected, (\d+) unused pieces',text)
        if len(reports)!=1:raise ValueError('Require exactly one native sprite report')
        si,sp,sr,su=map(int,reports[0])
        if not si or not sp or sr or su:raise ValueError('Require positive native sprite coverage without rejection or unused pieces')
    if require_sprite_gif:
        reports=re.findall(r'ac4 sprite GIF packets: (\d+) packets, (\d+) qwords bypassed generic parsing',text)
        if len(reports)!=1:raise ValueError('Require exactly one sprite GIF bypass report')
        packets,qwords=map(int,reports[0])
        if not packets or not 3*packets<=qwords<=129*packets or (qwords-packets)%2:
            raise ValueError('Require positive bounded complete sprite GIF rows')
    return intents, triangles


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture', type=Path)
    parser.add_argument('--fields', type=int, default=8)
    parser.add_argument('--scale', type=int, choices=range(1, 9),
                        help='Internal resolution multiplier for both paths')
    parser.add_argument('--haze', action='store_true', help='Require positive native haze coverage too')
    parser.add_argument('--clouds', action='store_true', help='Enable and require positive native cloud-plane coverage')
    parser.add_argument('--sprites', action='store_true', help='Enable and require positive native cloud-sprite coverage')
    parser.add_argument('--sprite-gif', action='store_true', help='Also require measured sprite GIF parser bypass')
    parser.add_argument('--first-field', type=int, default=1,
                        help='First field to compare; use 2 for captures whose first field only primes display buffers')
    args = parser.parse_args()
    if args.sprite_gif and not args.sprites:
        parser.error('--sprite-gif requires --sprites')
    if args.first_field < 1 or args.fields < args.first_field:
        parser.error('Require 1 <= first-field <= fields')
    with args.capture.open('rb') as stream:
        require_dma_capture(stream.read(16))
    root = Path(__file__).resolve().parents[1]
    build = root / 'build/native-poc'
    output = Path(tempfile.mkdtemp(prefix='sky-gpu-parity-', dir=root / 'work'))
    env = setup.environment(json.loads(setup.LOCAL.read_text()))
    env.update(PS2_SHADER_DIR=str(build / 'shaders'), PS2_VU_RECOMP='1',
               PS2_SETTINGS_FILE=str(output / 'settings.ini'))
    if args.scale is not None:
        env['PS2_UPSCALE'] = str(args.scale)
    print(f'Private artifacts: {output}', flush=True)
    evidence = []
    for field in range(args.first_field, args.fields + 1):
        images = []
        for mode in ('off', 'on'):
            env['PS2_AC4_SKY_MESH'] = '1' if mode == 'on' else '0'
            env['PS2_AC4_CLOUD_MESH'] = '1' if args.clouds and mode == 'on' else '0'
            env['PS2_AC4_SPRITE_MESH'] = '1' if args.sprites and mode == 'on' else '0'
            shot = output / f'{field}-{mode}.ppm'
            log = output / f'{field}-{mode}.log'
            with log.open('w') as stream:
                subprocess.run([str(build / 'gsreplay.exe'), str(args.capture.resolve()),
                                '--loop', '1', '--fields', str(field), '--shot', str(shot)],
                               cwd=output, env=env, stdout=stream, stderr=stream,
                               check=True, timeout=120)
            data = shot.read_bytes()
            require_visible_ppm(data)
            images.append(data)
            if mode == 'on':
                try:
                    validate_sky_report(log.read_text(errors='replace'), args.haze, args.clouds, args.sprites, args.sprite_gif)
                except ValueError as error:
                    raise RuntimeError(f'Field {field}: {error}; inspect {log}') from error
        if images[0] != images[1]:
            raise RuntimeError(f'Field {field}: native sky image differs; inspect {output}')
        evidence.append({'field': field, 'sha256': hashlib.sha256(images[0]).hexdigest(),
                         'exact': True})
        (output / 'frames.json').write_text(json.dumps(evidence, indent=2))
        # Keep differing images for diagnosis and the final successful pair;
        # identical intermediate images need not consume hundreds of megabytes.
        if field != args.fields:
            for mode in ('off', 'on'):
                (output / f'{field}-{mode}.ppm').unlink()
        print(f'Field {field}: positive native sky coverage and exact image parity', flush=True)
    print('Captured fields passed; this does not establish full-game coverage or guest execution bypass.')


if __name__ == '__main__':
    main()
