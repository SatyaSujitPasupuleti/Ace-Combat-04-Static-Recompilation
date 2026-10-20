"""Run the actual hybrid launcher preset with an isolated private flight route."""
import argparse
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from unittest.mock import patch

import setup_ac4 as setup
from test_ac4_screen_gpu import validate_screen_report
from analyze_ac4_hud_groups import validate as validate_hud_groups
from test_ac4_model_gpu import require_visible_ppm


def validate_route_report(text, cockpit=False, hud=False):
    result = {'screen': validate_screen_report(text, overlay=True, precompose=True)}
    meshes = re.findall(r'vk: (\d+) native mesh draws \(mesh.vert\), (\d+) recorded with the native material \(mesh.frag\); (\d+) batches refused for arena space', text)
    if len(meshes) != 1 or not (0 < int(meshes[0][1]) <= int(meshes[0][0])) or int(meshes[0][2]):
        raise ValueError('Require native GPU meshes/materials without arena overflow')
    result['gpu'] = dict(zip(('meshes', 'materials', 'arena_refusals'), map(int, meshes[0])))
    patterns = {
        'lit': r'ac4 lit native: (\d+) activations',
        'unlit': r'ac4 unlit native: (\d+) activations',
        'terrain': r'ac4 terrain native: (\d+) activations',
        'sky': r'ac4 native sky: (\d+) intents',
        'clouds': r'ac4 native clouds: (\d+) intents',
        'supplied_mips': r'ac4 supplied mip chains: (\d+) draws',
    }
    for name, pattern in patterns.items():
        rows = re.findall(pattern, text)
        if len(rows) != 1 or int(rows[0]) <= 0:
            raise ValueError(f'Missing positive, unique {name} coverage; route may not have reached flight')
        result[name] = int(rows[0])
    textures = re.findall(r'ac4 model textures: (\d+) native mip draws, (\d+) allocation fallbacks, (\d+) unsupported states', text)
    if len(textures) != 1 or int(textures[0][0]) <= 0:
        raise ValueError('Missing native texture coverage')
    result['textures'] = dict(zip(('converted', 'fallbacks', 'unsupported'), map(int, textures[0])))
    if result['textures']['converted'] < result['supplied_mips']:
        raise ValueError('Supplied mip coverage exceeds total converted textures')
    filtering = re.findall(r'ac4 supplied filtering: (\d+) hardware, (\d+) anisotropic draws', text)
    if len(filtering) != 1 or not (0 < int(filtering[0][1]) <= int(filtering[0][0]) <= result['supplied_mips']):
        raise ValueError('Missing or inconsistent hardware anisotropic path coverage')
    samplers = re.findall(r'vk: native mip sampler anisotropy ([\d.]+)x', text)
    if len(samplers) != 1 or float(samplers[0]) < 2:
        raise ValueError('Full filtering coverage requires native anisotropy enabled in settings')
    shadow = re.findall(r'ac4 screen sprite shadow: (\d+) calls, (\d+) matched, (\d+) mismatched, (\d+) declined, (\d+) unpaired, (\d+) open', text)
    if len(shadow) != 1:
        raise ValueError('Missing screen ownership shadow report')
    calls, matched, mismatched, declined, unpaired, opened = map(int, shadow[0])
    if not matched or mismatched or unpaired or opened or calls != matched + declined:
        raise ValueError('Screen ownership shadow is inconsistent or mismatched')
    result['interval_fields_per_second'] = [float(x) for x in re.findall(r'-- ([\d.]+) fields/s over the last 600', text)]
    if cockpit:
        rows = re.findall(r'ac4 cockpit native: (\d+) activations, (\d+) packets, (\d+) original instructions replaced', text)
        gif = re.findall(r'ac4 cockpit GIF packets: (\d+) packets, (\d+) qwords bypassed generic parsing', text)
        gpu = re.findall(r'ac4 cockpit GPU: (\d+) draws', text)
        if len(rows)!=1 or len(gif)!=1 or len(gpu)!=1:
            raise ValueError('Require unique cockpit execution, GIF and GPU coverage')
        activations, packets, instructions = map(int,rows[0]); bypass,words=map(int,gif[0]);draws=int(gpu[0])
        if not (0<activations<=packets and instructions>0 and 0<bypass<=packets and words>bypass and draws>0):
            raise ValueError('Require positive consistent cockpit coverage')
        result['cockpit'] = dict(activations=activations,packets=packets,instructions=instructions,gif_packets=bypass,gif_qwords=words,draws=draws)
    if hud:
        result['hud'] = validate_hud_groups(text, native=True, views=cockpit)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pad', type=Path, required=True)
    parser.add_argument('--saves', type=Path, required=True)
    parser.add_argument('--output-root', type=Path, required=True)
    parser.add_argument('--fields', type=int, default=10400)
    parser.add_argument('--timeout', type=int, default=600)
    parser.add_argument('--opening', choices=('Show', 'Skip', 'GameDefault'), default='Show')
    parser.add_argument('--cockpit-views', action='store_true', help='Mission 1 developer route: toggle view at fields 12020/12800 and require cockpit coverage')
    args = parser.parse_args()
    if args.cockpit_views and args.fields < 13400:
        parser.error('--cockpit-views requires --fields 13400 or more')
    if args.fields < 1 or args.timeout < 1 or not args.saves.is_dir():
        parser.error('Positive fields/timeout and a source save directory are required')
    with args.pad.open('rb') as source:
        if source.read(8) != b'PS2PAD1\0':
            parser.error('Pad input is not a PS2PAD1 recording')
    args.output_root.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='hybrid-route-', dir=args.output_root.resolve()))
    print(f'Private evidence: {output}', flush=True)

    def launch(command, env):
        env = dict(env)
        shutil.copytree(args.saves, output / 'saves')
        settings = Path(env['PS2_SETTINGS_FILE'])
        if settings.is_file():
            shutil.copyfile(settings, output / 'settings.ini')
        env.update(PS2_SAVE_DIR=str(output / 'saves'), PS2_SETTINGS_FILE=str(output / 'settings.ini'),
                   PS2_PAD_PLAY=str(args.pad.resolve()), PS2_PAD_PLAY_STOP='0',
                   PS2_AC4_OPENING=args.opening, PS2_SHOT_EVERY='600', PS2_SHOT_SLOTS='8',
                   PS2_SHOT_DIR=str(output))
        if args.cockpit_views:
            env['PS2_PAD_SCRIPT'] = '12020:DOWN,12800:DOWN'
        command = list(map(str, command))
        command[command.index('--vblanks') + 1] = str(args.fields)
        command.append('--hidden')
        (output / 'command.json').write_text(json.dumps(command, indent=2))
        (output / 'config.json').write_text(json.dumps({k:v for k,v in env.items() if k.startswith('PS2_')}, indent=2))
        with (output / 'run.log').open('w') as log:
            subprocess.run(command, env=env, cwd=output, stdout=log, stderr=log,
                           check=True, timeout=args.timeout)

    with patch.object(setup, 'call', launch):
        setup.run(json.loads(setup.LOCAL.read_text()), False, False, hybrid_renderer=True)
    report = validate_route_report((output / 'run.log').read_text(errors='replace'), args.cockpit_views, hud=True)
    require_visible_ppm((output / 'ps2_frame.ppm').read_bytes())
    (output / 'coverage.json').write_text(json.dumps(report, indent=2))
    print('PASS: bounded combined-route coverage. Images, audio and frame-time behavior still require review.')


if __name__ == '__main__':
    main()
