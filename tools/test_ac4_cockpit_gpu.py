"""Compare the verified direct cockpit mesh subset with its original VU/GS output."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile
import setup_ac4 as setup
from test_ac4_model_gpu import require_complete_capture, require_visible_ppm


def validate_cockpit(text, gpu=False, secondary=False, clipping=False, state_plan=False):
    rows = re.findall(r'ac4 cockpit audit: (\d+) activations, (\d+) packets, (\d+) vertices, (\d+) exact, (\d+) xyz / (\d+) fog / (\d+) uv / (\d+) color mismatches, (\d+) skipped, (\d+) rejected', text)
    if len(rows) != 1:
        raise ValueError('Require exactly one cockpit audit report')
    activation, packets, vertices, exact, xyz, fog, uv, color, skipped, rejected = map(int, rows[0])
    if not activation or not packets or not vertices or vertices != exact or any((xyz, fog, uv, color, rejected)):
        raise ValueError('Require positive cockpit coverage with exact direct vertices and no rejected vertices')
    draws = re.findall(r'ac4 cockpit GPU: (\d+) draws', text)
    second_draws = re.findall(r'ac4 cockpit secondary GPU: (\d+) draws', text)
    second_count = int(second_draws[0]) if len(second_draws) == 1 else 0
    clipped_draws = re.findall(r'ac4 cockpit clipped GPU: (\d+) draws', text)
    clipped_count = int(clipped_draws[0]) if len(clipped_draws) == 1 else 0
    if len(clipped_draws) > 1:
        raise ValueError('Duplicated cockpit clipped GPU report')
    if len(second_draws) > 1:
        raise ValueError('Duplicated cockpit secondary GPU report')
    if gpu and (len(draws) != 1 or not 0 < int(draws[0])-second_count <= packets+clipped_count
                or second_count > int(draws[0])-second_count):
        raise ValueError('Require positive cockpit GPU coverage bounded by verified packets')
    if not gpu and draws:
        raise ValueError('Observer-only mode unexpectedly replaced draws')
    if secondary:
        reports = re.findall(r'ac4 cockpit secondary: (\d+) vertices, (\d+) exact, (\d+) mismatches, (\d+) header failures, (\d+) rejected', text)
        if len(reports) != 1 or not int(reports[0][0]) or reports[0][0] != reports[0][1] or any(map(int,reports[0][2:])):
            raise ValueError('Require exact positive cockpit secondary evidence')
        if gpu and not second_count:
            raise ValueError('Require actual secondary cockpit GPU draws')
    if clipping or clipped_count:
        reports = re.findall(r'ac4 cockpit clipping: (\d+) packets, (\d+) exact, (\d+) packet / (\d+) scratch mismatches, (\d+) declined, (\d+) empty', text)
        if len(reports) != 1:
            raise ValueError('Require exactly one cockpit clipping report')
        cp, ce, cb, cs, cd, empty = map(int, reports[0])
        if not cp or ce != cp or any((cb, cs, cd)) or not 0 <= empty < cp:
            raise ValueError('Require exact clipped packets/scratch including nonempty polygons')
        if gpu and (not 0 < clipped_count <= 12*(cp-empty) or clipped_count > int(draws[0])):
            raise ValueError('Require clipped GPU coverage bounded by verified polygons')
    if state_plan:
        memory = re.findall(r'ac4 cockpit memory: (\d+) begun, (\d+) ended, (\d+) packets, (\d+) kick / (\d+) end mismatches, (\d+) sequence failures, (\d+) declined', text)
        integers = re.findall(r'ac4 cockpit integers: (\d+) packets, (\d+) ended, (\d+) kick / (\d+) end mismatches, (\d+) declined', text)
        if len(memory) != 1 or len(integers) != 1:
            raise ValueError('Require exactly one memory and integer state report')
        begun, ended, observed, *errors = map(int, memory[0])
        ipackets, iended, *ierrors = map(int, integers[0])
        if (begun != activation or ended != activation or observed != packets+skipped
                or ipackets != observed or iended != ended or any(errors+ierrors)):
            raise ValueError('Require complete exact memory/integer activation and packet coverage')
        scalars = re.findall(r'ac4 cockpit scalars: (\d+) packets, (\d+) ended, (\d+) kick / (\d+) end mismatches, (\d+) declined', text)
        if len(scalars) != 1:
            raise ValueError('Require exactly one scalar state report')
        spackets, send, *serrors = map(int, scalars[0])
        if spackets != observed or send != ended or any(serrors):
            raise ValueError('Require complete exact scalar state coverage')
        vectors = re.findall(r'ac4 cockpit vectors: (\d+) packets, (\d+) ended, (\d+) kick / (\d+) end mismatches, (\d+) declined', text)
        if len(vectors) != 1:
            raise ValueError('Require exactly one vector state report')
        vpackets, vend, *verrors = map(int, vectors[0])
        if vpackets != observed or vend != ended or any(verrors):
            raise ValueError('Require complete exact vector/accumulator state coverage')
        cycles = re.findall(r'ac4 cockpit cycles: (\d+) ended, (\d+) mismatches, (\d+) declined', text)
        if len(cycles) != 1 or int(cycles[0][0]) != ended or any(map(int,cycles[0][1:])):
            raise ValueError('Require complete exact instruction-count coverage')
        flags = re.findall(r'ac4 cockpit flags: (\d+) ended, (\d+) mismatches', text)
        if len(flags) != 1 or int(flags[0][0]) != ended or int(flags[0][1]):
            raise ValueError('Require complete exact arithmetic flag coverage')
    return dict(activations=activation, packets=packets, vertices=vertices,
                skipped=skipped, gpu_draws=int(draws[0]) if draws else 0, secondary_draws=second_count, clipped_draws=clipped_count)


def validate_native_cockpit(report, coverage, baseline, gif=False):
    rows = re.findall(r'ac4 cockpit native: (\d+) activations, (\d+) packets, (\d+) original instructions replaced', report)
    if len(rows) != 1 or int(rows[0][0]) != coverage['activations'] or int(rows[0][1]) != coverage['packets']+coverage['skipped'] or not int(rows[0][2]):
        raise ValueError('Require complete positive cockpit native execution coverage')
    for key in ('activations', 'packets', 'vertices', 'skipped'):
        if coverage[key] != baseline[key]:
            raise ValueError('Native cockpit coverage differs from original execution')
    if gif:
        rows_gif = re.findall(r'ac4 cockpit GIF packets: (\d+) packets, (\d+) qwords bypassed generic parsing', report)
        if len(rows_gif) != 1:
            raise ValueError('Require exactly one cockpit GIF bypass report')
        packets, words = map(int, rows_gif[0])
        if not 0 < packets <= coverage['packets']+coverage['skipped'] or not packets < words <= packets*1024:
            raise ValueError('Require positive bounded cockpit GIF bypass coverage')
    return int(rows[0][2])


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('capture', type=Path)
    ap.add_argument('--fields', type=int, default=4)
    ap.add_argument('--scale', type=int, choices=range(1, 9), default=1)
    ap.add_argument('--output-root', type=Path, required=True)
    ap.add_argument('--secondary', action='store_true', help='Require exact secondary vertices and actual secondary GPU coverage')
    ap.add_argument('--clipping', action='store_true', help='Require exact clipped packets/scratch and actual clipped GPU coverage')
    ap.add_argument('--state-plan', action='store_true', help='Require memory, register, flags and instruction-count coverage; does not enable execution bypass')
    ap.add_argument('--native', action='store_true', help='Also require native cockpit execution and exact output')
    ap.add_argument('--gif', action='store_true', help='Also compare guarded cockpit GIF bypass (requires --native)')
    args = ap.parse_args()
    if args.gif and not args.native:
        ap.error('--gif requires --native')
    require_complete_capture(args.capture, args.fields)
    root = Path(__file__).resolve().parents[1]
    out = Path(tempfile.mkdtemp(prefix='cockpit-gpu-', dir=args.output_root))
    env = setup.environment(json.loads(setup.LOCAL.read_text()))
    env.update(PS2_SHADER_DIR=str(root/'build/native-poc/shaders'),
               PS2_SETTINGS_FILE=str(out/'settings.ini'), PS2_UPSCALE=str(args.scale),
               PS2_LOCKSTEP='1', PS2_AC4_MODEL_AUDIT='1')
    results, reference = [], None
    print(out, flush=True)
    modes = ('gs', 'audit', 'mesh', 'material', 'native', 'native-material') if args.native else ('gs', 'audit', 'mesh', 'material')
    if args.gif:
        modes += ('native-gif', 'native-gif-material')
    for mode in modes:
        fast = 'gif' in mode
        env['PS2_AC4_MODEL_AUDIT'] = '0' if fast else '1'
        native = mode.startswith('native')
        gpu = mode in ('mesh', 'material') or native
        env['PS2_AC4_COCKPIT_AUDIT'] = '0' if mode == 'gs' else '1'
        env['PS2_AC4_MODEL_PLAN'] = str(int(args.state_plan and mode != 'gs' and not native))
        env['PS2_AC4_COCKPIT_NATIVE'] = str(int(native))
        env['PS2_AC4_COCKPIT_MESH'] = env['PS2_AC4_MODEL_MESH'] = str(int(gpu))
        env['PS2_AC4_MODEL_MATERIAL'] = str(int(mode.endswith('material')))
        log, shot = out/f'{mode}.log', out/f'{mode}.ppm'
        with log.open('w') as f:
            subprocess.run([str(root/'build/native-poc/gsreplay.exe'), str(args.capture.resolve()),
                            '--hidden', '--loop', '1', '--fields', str(args.fields), '--shot', str(shot)],
                           cwd=out, env=env, stdout=f, stderr=f, timeout=120, check=True)
        image = shot.read_bytes(); require_visible_ppm(image)
        report = log.read_text()
        coverage = {} if mode == 'gs' else validate_cockpit(report, gpu, args.secondary, args.clipping, args.state_plan and not native)
        if native:
            baseline = next(r for r in results if r['mode'] == 'audit')
            coverage['instructions_replaced'] = validate_native_cockpit(report, coverage, baseline, fast)
        if mode.endswith('material'):
            material = re.search(r'vk: (\d+) native mesh draws \(mesh.vert\), (\d+) recorded with the native material \(mesh.frag\); (\d+) batches refused', report)
            if not material or int(material[2]) < coverage['gpu_draws'] or int(material[3]):
                raise ValueError('Require native material coverage for the cockpit subset with no arena overflow')
            coverage['material_draws'] = int(material[2])
        if reference is None:
            reference = image
        results.append(dict(mode=mode, exact=image == reference,
                            sha256=hashlib.sha256(image).hexdigest(), **coverage))
        (out/'comparison.json').write_text(json.dumps(results, indent=2))
        if image != reference:
            raise RuntimeError(f'{mode} differs from original GS output')
        print(results[-1], flush=True)
    print('Captured cockpit paths passed; whole-game coverage and live validation remain required.')


if __name__ == '__main__':
    main()
