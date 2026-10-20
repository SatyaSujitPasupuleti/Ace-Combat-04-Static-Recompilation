"""Opt-in GPU acceptance using a private local VU/GIF capture, never bundled data."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import struct
import tempfile

import setup_ac4 as setup


def require_dma_capture(header):
    if len(header) < 16 or header[:8] != b'GSCAP2\x00\x00':
        raise ValueError('A valid GSCAP2 header is required for renderer acceptance.')
    version, level = struct.unpack_from('<II', header, 8)
    if version != 2 or level != 1:
        raise ValueError('Native geometry acceptance requires a version-2 DMA capture; GS-only captures omit VU execution.')


def require_visible_ppm(data):
    """Reject malformed or uniform final frames; this is not a visual correctness test."""
    match = re.match(rb'P6\s+(\d+)\s+(\d+)\s+255(?:\r\n|\n)', data)
    if not match:
        raise ValueError('Unsupported PPM header')
    width, height = map(int, match.groups())
    pixels = data[match.end():]
    if width < 1 or height < 1 or len(pixels) != width * height * 3:
        raise ValueError('Invalid PPM dimensions or truncated payload')
    if pixels == pixels[:3] * (width * height):
        raise ValueError('Uniform final frame cannot establish visible rendering parity')


def validate_unlit_report(report):
    match = re.search(r'ac4 unlit plan: (\d+) built, (\d+) declined, (\d+) packets, (\d+) packet mismatches, (\d+) ended, (\d+) memory mismatches, (\d+) missing sequences, (\d+) control mismatches', report)
    if not match:
        raise ValueError('FAIL: no unlit packet-plan evidence.')
    built, declined, packets, bad, ended, memory, missing, control = map(int, match.groups())
    if not built or not packets or ended != built or bad or memory or missing or control:
        raise ValueError('FAIL: unlit packet plan lacks coverage or differs; inspect plan.log.')
    registers = re.search(r'ac4 unlit state: (\d+) register mismatches', report)
    if not registers or int(registers[1]):
        raise ValueError('FAIL: unlit continuation-register evidence missing or mismatched.')
    flags = re.search(r'ac4 unlit flags: (\d+) pipeline mismatches', report)
    if not flags or int(flags[1]):
        raise ValueError('FAIL: unlit arithmetic pipeline evidence missing or mismatched.')
    cycles = re.search(r'ac4 unlit cycles: (\d+) instruction-count mismatches', report)
    if not cycles or int(cycles[1]):
        raise ValueError('FAIL: unlit cycle accounting missing or mismatched.')
    return built, declined, packets

def validate_culling_report(report, native=False):
    match = re.search(r'ac4 unlit culling: (\d+) planned activations, (\d+) executed natively', report)
    if not match:
        raise ValueError('FAIL: no culling coverage report.')
    planned, executed = map(int, match.groups())
    if not planned or executed != (planned if native else 0):
        raise ValueError('FAIL: missing or inconsistent culling coverage.')
    return planned


def validate_clipping_report(report):
    match = re.search(r'ac4 unlit clip: (\d+) packets compared, (\d+) exact, (\d+) mismatched, (\d+) declined', report)
    scratch = re.search(r'ac4 unlit clip scratch: (\d+) mismatched packets', report)
    if not match or not scratch:
        raise ValueError('FAIL: missing partial-clipping evidence.')
    compared, exact, bad, declined = map(int, match.groups())
    if not compared or exact != compared or bad or declined or int(scratch[1]):
        raise ValueError('FAIL: partial-clipping coverage or packet/scratch parity failed.')
    controls = re.search(r'ac4 unlit clip control: (\d+) integer-state mismatches', report)
    if not controls or int(controls[1]):
        raise ValueError('FAIL: partial-clipping integer-state evidence missing or mismatched.')
    vectors = re.search(r'ac4 unlit clip vector: (\d+) state mismatches', report)
    if not vectors or int(vectors[1]):
        raise ValueError('FAIL: partial-clipping vector-state evidence missing or mismatched.')
    scalar = re.search(r'ac4 unlit clip scalar: (\d+) state mismatches; (\d+) drain mismatches', report)
    if not scalar or any(map(int, scalar.groups())):
        raise ValueError('FAIL: partial-clipping scalar/drain evidence missing or mismatched.')
    cycles = re.search(r'ac4 unlit clip cycles: (\d+) activations, (\d+) mismatches', report)
    if not cycles or not int(cycles[1]) or int(cycles[2]):
        raise ValueError('FAIL: partial-clipping cycle evidence missing or mismatched.')
    flags = re.search(r'ac4 unlit clip flags: (\d+) state mismatches', report)
    if not flags or int(flags[1]):
        raise ValueError('FAIL: partial-clipping arithmetic-state evidence missing or mismatched.')
    memory = re.search(r'ac4 unlit mixed memory: (\d+) mismatched activations, (\d+) packet mismatches, (\d+) sequence mismatches', report)
    if not memory or any(map(int, memory.groups())):
        raise ValueError('FAIL: mixed-clipping memory/sequence evidence missing or mismatched.')
    registers = re.search(r'ac4 unlit mixed registers: (\d+) state mismatches', report)
    if not registers or int(registers[1]):
        raise ValueError('FAIL: mixed-clipping register evidence missing or mismatched.')
    return compared


def validate_mixed_execution(shadow, native):
    expected = re.search(r'ac4 mixed eligible: (\d+) activations, (\d+) packets, (\d+) instructions', shadow)
    actual = re.search(r'ac4 mixed native: (\d+) activations, (\d+) packets, (\d+) original instructions replaced', native)
    if not expected or not actual or any(int(x) == 0 for x in expected.groups()) or expected.groups() != actual.groups():
        raise ValueError('FAIL: mixed native execution lacks matching nonzero shadow coverage.')
    return tuple(map(int, actual.groups()))


def validate_lit_secondary(report):
    match = re.search(r'ac4 lit second: (\d+) vertices, (\d+) header / (\d+) stq / (\d+) colour / (\d+) geometry mismatches, (\d+) rejected;', report)
    if not match or not int(match[1]) or any(int(x) for x in match.groups()[1:]):
        raise ValueError('FAIL: lit secondary stream lacks nonzero matching evidence.')
    adc = re.search(r'ac4 lit ADC: (\d+) marked vertices, (\d+) mismatches', report)
    if not adc or int(adc[2]):
        raise ValueError('FAIL: lit ADC evidence missing or mismatched.')
    packets = re.search(r'ac4 lit packets: (\d+) compared, (\d+) mismatched', report)
    if not packets or not int(packets[1]) or int(packets[2]):
        raise ValueError('FAIL: complete lit packet evidence missing or mismatched.')
    return int(match[1])


def validate_lit_memory(report):
    match = re.search(r'ac4 lit memory: (\d+) activations, (\d+) packets, (\d+) memory / (\d+) sequence mismatches, (\d+) declined, (\d+) packet mismatches', report)
    if not match or not int(match[1]) or not int(match[2]) or any(int(match[i]) for i in range(3, 7)):
        raise ValueError('Lit memory requires positive activation/packet coverage and zero memory, sequence, packet mismatches or declines.')
    return int(match[1]), int(match[2])


def validate_lit_integers(report):
    activations, packets = validate_lit_memory(report)
    match = re.search(r'ac4 lit integers: (\d+) packets, (\d+) activations, (\d+) kick / (\d+) continuation mismatches, (\d+) declined', report)
    if not match or int(match[1]) != packets or int(match[2]) != activations or any(int(match[i]) for i in range(3, 6)):
        raise ValueError('Lit integer evidence must cover every memory-verified packet/activation with zero mismatches or declines.')
    return activations, packets


def validate_lit_scalar(report):
    activations, packets = validate_lit_integers(report)
    match = re.search(r'ac4 lit scalar: (\d+) packets, (\d+) activations, (\d+) kick / (\d+) continuation mismatches, (\d+) declined', report)
    if not match or int(match[1]) != packets or int(match[2]) != activations or any(int(match[i]) for i in range(3, 6)):
        raise ValueError('Lit scalar evidence must cover every memory-verified packet/activation with zero mismatches or declines.')
    return activations, packets


def validate_lit_vectors(report):
    activations, packets = validate_lit_scalar(report)
    match = re.search(r'ac4 lit vectors: (\d+) packets, (\d+) activations, (\d+) kick / (\d+) continuation mismatches, (\d+) skipped packets', report)
    if not match or int(match[1]) != packets or int(match[2]) != activations or any(int(match[i]) for i in range(3, 6)):
        raise ValueError('Lit vector evidence must cover every packet/activation with zero mismatches or skips.')
    return activations, packets


def validate_lit_cycles(report):
    activations, packets = validate_lit_vectors(report)
    match = re.search(r'ac4 lit cycles: (\d+) activations, (\d+) mismatches, (\d+) declined', report)
    if not match or int(match[1]) != activations or int(match[2]) or int(match[3]):
        raise ValueError('Lit instruction-count evidence must cover every activation with zero mismatches or declines.')
    return activations, packets


def validate_lit_flags(report):
    activations, packets = validate_lit_cycles(report)
    match = re.search(r'ac4 lit flags: (\d+) activations, (\d+) mismatches, (\d+) declined', report)
    if not match or int(match[1]) != activations or int(match[2]) or int(match[3]):
        raise ValueError('Lit arithmetic flag evidence must cover every activation with zero mismatches.')
    return activations, packets


def validate_lit_execution(shadow, native):
    expected = validate_lit_flags(shadow)
    predicted = re.search(r'ac4 lit instructions: (\d+) predicted', shadow)
    actual = re.search(r'ac4 lit native: (\d+) activations, (\d+) packets, (\d+) original instructions replaced', native)
    if not actual or not predicted or (int(actual[1]),int(actual[2])) != expected or not int(actual[3]) or actual[3] != predicted[1]:
        raise ValueError('Native lit activation, packet and instruction counts must equal full shadow evidence.')
    return (*expected, int(actual[3]))


def validate_second_pass(report):
    match = re.search(r'ac4 model second GPU: (\d+) draws', report)
    if not match or not int(match[1]):
        raise ValueError('Capture must exercise native secondary model draws.')
    return int(match[1])


def validate_terrain_preparation(report):
    match = re.search(r'ac4 terrain preparation: (\d+) seen, (\d+) compared, (\d+) vertices, (\d+) mismatches, (\d+) declined', report)
    if not match or not int(match[1]) or match[1] != match[2] or not int(match[3]) or int(match[4]) or int(match[5]):
        raise ValueError('Terrain preparation requires complete positive coverage and zero mismatches/declines.')
    for mode in (2, 4):
        coverage = re.search(rf'ac4 terrain mode {mode}: (\d+)', report)
        if not coverage or not int(coverage[1]):
            raise ValueError('Terrain preparation requires both morphed and non-morphed patch coverage.')
    return int(match[2]), int(match[3])


def validate_terrain_gpu(report):
    match = re.search(r'ac4 terrain GPU: (\d+) primary, (\d+) secondary draws', report)
    if not match or not all(int(x) > 0 for x in match.groups()):
        raise ValueError('Native terrain requires positive GPU coverage in both material passes.')
    return tuple(map(int, match.groups()))


def validate_clipped_terrain_gpu(report):
    match = re.search(r'ac4 clipped terrain GPU: (\d+) primary, (\d+) secondary draws', report)
    if not match or not all(int(x)>0 for x in match.groups()):
        raise ValueError('Clipped terrain must exercise both native GPU material streams.')
    return tuple(map(int, match.groups()))


def validate_terrain_culling(report):
    match = re.search(r'ac4 terrain culling: (\d+) seen, (\d+) ended, (\d+) predicted empty, (\d+) observed empty, (\d+) mismatches, (\d+) partial, (\d+) invalid', report)
    if not match:
        raise ValueError('Terrain culling evidence is missing.')
    seen, ended, predicted, observed, bad, partial, invalid = map(int, match.groups())
    if not predicted or seen != ended or predicted != observed or seen != predicted + partial or bad or invalid:
        raise ValueError('Terrain culling requires complete activations, positive exact empty predictions and zero mismatches/invalid inputs.')
    return seen, predicted, partial


def validate_terrain_polygons(report):
    seen, empty, partial = validate_terrain_culling(report)
    match = re.search(r'ac4 terrain polygons: (\d+) built, (\d+) declined, (\d+) packets, (\d+) mismatches, (\d+) sequence mismatches', report)
    if not match or int(match[1]) != seen or int(match[2]) or not int(match[3]) or int(match[4]) or int(match[5]) or not partial:
        raise ValueError('Clipped terrain requires complete plans, positive partial packet coverage and exact packets/order.')
    return seen, empty, int(match[3])


def validate_terrain_memory(report):
    patches, _ = validate_terrain_preparation(report)
    match = re.search(r'ac4 terrain memory: (\d+) built, (\d+) ended, (\d+) mismatches, (\d+) declined', report)
    if not match:
        raise ValueError('Terrain continuation-memory evidence is missing.')
    built, ended, bad, declined = map(int, match.groups())
    if not built or built != ended or built != patches or declined or bad:
        raise ValueError('Every terrain activation must have exact full-memory continuation with no exclusions.')
    return built, declined


def validate_terrain_integers(report):
    patches, _ = validate_terrain_preparation(report)
    expected_kicks = patches
    if re.search(r'ac4 terrain mode 1: [1-9]', report):
        seen, _, packets = validate_terrain_polygons(report)
        expected_kicks = patches-seen+packets
    match = re.search(r'ac4 terrain integers: (\d+) built, (\d+) ended, (\d+) kicks, (\d+) end / (\d+) kick / (\d+) sequence mismatches, (\d+) declined', report)
    if not match:
        raise ValueError('Terrain integer-state evidence is missing.')
    built, ended, kicks, end_bad, kick_bad, sequence, declined = map(int, match.groups())
    if not built or built != patches or ended != built or kicks != expected_kicks or end_bad or kick_bad or sequence or declined:
        raise ValueError('All terrain integer state must match at every exit and packet submission without exclusions.')
    return built, kicks, declined


def validate_mesh_gif(report):
    match=re.search(r'ac4 mesh GIF packets: (\d+) packets, (\d+) qwords bypassed generic parsing',report)
    if not match or int(match[1])<=0 or int(match[2])<=int(match[1]):
        raise ValueError('Whole mesh GIF bypass requires positive packet and payload coverage.')
    return tuple(map(int,match.groups()))


def validate_terrain_execution(shadow,native):
    expected=validate_terrain_continuation(shadow)
    match=re.search(r'ac4 terrain native: (\d+) activations, (\d+) packets, (\d+) original instructions replaced',native)
    if not match or tuple(map(int,match.groups())) != expected:
        raise ValueError('Native terrain execution must match the complete original-run activation, packet and instruction totals.')
    return expected


def validate_terrain_continuation(report):
    patches, packets, _ = validate_terrain_integers(report)
    _, instructions, _ = validate_terrain_instructions(report)
    validate_terrain_memory(report)
    validate_terrain_scalars(report)
    validate_terrain_vectors(report)
    validate_terrain_flags(report)
    match = re.search(r'ac4 terrain continuation: (\d+) built, (\d+) ended, (\d+) packets, (\d+) instructions, (\d+) mismatches, (\d+) declined',report)
    if not match or tuple(map(int,match.groups())) != (patches,patches,packets,instructions,0,0):
        raise ValueError('Full terrain continuation must match all component coverage, packets and instruction totals with no mismatches or exclusions.')
    submissions = re.search(r'ac4 terrain submissions: (\d+) packets, (\d+) memory mismatches, (\d+) sequence mismatches',report)
    if not submissions or tuple(map(int,submissions.groups())) != (packets,0,0):
        raise ValueError('Terrain submissions must cover every packet with exact intermediate memory and ordering.')
    return patches, packets, instructions


def validate_terrain_flags(report):
    patches, _ = validate_terrain_preparation(report)
    match = re.search(r'ac4 terrain flags: (\d+) built, (\d+) ended, (\d+) mismatches, (\d+) excluded',report)
    if not match or tuple(map(int,match.groups())) != (patches,patches,0,0):
        raise ValueError('Terrain flags must cover every prepared activation without mismatches or exclusions.')
    return patches, 0


def validate_terrain_vectors(report):
    patches, kicks, _ = validate_terrain_integers(report)
    match = re.search(r'ac4 terrain vectors: (\d+) built, (\d+) ended, (\d+) kicks, (\d+) end / (\d+) kick mismatches, (\d+) excluded',report)
    if not match or tuple(map(int,match.groups())) != (patches,patches,kicks,0,0,0):
        raise ValueError('Terrain vectors must cover every validated integer exit and packet submission with no mismatches or exclusions.')
    return patches, kicks, 0


def validate_terrain_instructions(report):
    patches, _ = validate_terrain_preparation(report)
    match = re.search(r'ac4 terrain instructions: (\d+) built, (\d+) ended, (\d+) mismatches, (\d+) excluded, (\d+) predicted / (\d+) observed', report)
    if not match:
        raise ValueError('Missing terrain instruction evidence.')
    built, ended, bad, skipped, predicted, observed = map(int,match.groups())
    if not built or (built,ended,bad,skipped) != (patches,patches,0,0) or not predicted or predicted != observed:
        raise ValueError('Terrain instruction evidence must cover every terrain activation with no mismatches or exclusions.')
    return built, predicted, skipped


def validate_terrain_scalars(report):
    built, kicks, declined = validate_terrain_integers(report)
    match = re.search(r'ac4 terrain scalars: (\d+) built, (\d+) ended, (\d+) kicks, (\d+) end / (\d+) kick / (\d+) sequence mismatches, (\d+) declined', report)
    if not match or tuple(map(int, match.groups())) != (built,built,kicks,0,0,0,0):
        raise ValueError('Terrain scalar evidence must cover every validated integer exit/kick with no mismatches or exclusions.')
    return built, kicks


def validate_terrain_packets(report):
    patches, vertices = validate_terrain_preparation(report)
    ordinary = patches
    clipped = re.search(r'ac4 terrain mode 1: (\d+)', report)
    if clipped and int(clipped[1]):
        seen, empty, packets = validate_terrain_polygons(report)
        if seen != int(clipped[1]) or seen > patches:
            raise ValueError('Clipped terrain activation coverage differs from preparation.')
        topology = re.search(r'ac4 terrain clipped topology: (\d+) packets, (\d+) triangles, (\d+) declined', report)
        if not topology or int(topology[1]) != packets or not int(topology[2]) or int(topology[3]):
            raise ValueError('Every clipped terrain packet requires supported topology.')
        ordinary -= seen
    match = re.search(r'ac4 terrain packets: (\d+) compared, (\d+) packet / (\d+) bank mismatches, (\d+) declined', report)
    if not match or int(match[1]) != ordinary or any(int(match[i]) for i in range(2,5)):
        raise ValueError('Terrain packet evidence must cover every prepared patch with zero packet/bank mismatches or declines.')
    topology = re.search(r'ac4 terrain topology: (\d+) patches, (\d+) triangles, (\d+) declined', report)
    if not topology or int(topology[1]) != ordinary or not int(topology[2]) or int(topology[3]):
        raise ValueError('Terrain topology must cover every prepared patch with positive triangles and zero declines.')
    return patches, vertices


def validate_lit_clipping(report):
    match = re.search(r'ac4 lit clipping: (\d+) packets, (\d+) empty, (\d+) packet / (\d+) scratch mismatches, (\d+) declined', report)
    if not match or not int(match[1]) or any(int(x) for x in match.groups()[2:]):
        raise ValueError('FAIL: lit clipping lacks nonzero packet/scratch parity.')
    return int(match[1])


def require_complete_capture(path, field):
    with path.open('rb') as f:
        header = f.read(176)
    require_dma_capture(header)
    if len(header) != 176:
        raise ValueError('Capture header is truncated')
    start, size, index = struct.unpack_from('<QQQ', header, 96)
    fields = struct.unpack_from('<I', header, 128)[0]
    truncated = struct.unpack_from('<I', header, 136)[0]
    length = path.stat().st_size
    if truncated or not size or start < 176 or start + size > length:
        raise ValueError('Capture stream is incomplete; do not use it as renderer evidence')
    if not index or index < start + size or index + fields * 16 > length:
        raise ValueError('Capture field index is missing or truncated')
    if field < 1 or field > fields:
        raise ValueError('Requested field is outside the completed capture')
    return fields


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture', type=Path)
    parser.add_argument('--fields', type=int, default=8)
    parser.add_argument('--output-root', type=Path, help='Directory for private replay artifacts')
    parser.add_argument('--scale', type=int, choices=range(1, 9), help='Explicit internal scale for reproducible comparisons')
    parser.add_argument('--materials', action='store_true', help='Also require native model-material image parity')
    parser.add_argument('--unlit-plan', action='store_true', help='Verify independent unlit packet sequences and final memory without bypassing VU')
    parser.add_argument('--native-unlit', action='store_true', help='Also compare actual native unlit execution; implies unlit-plan verification')
    parser.add_argument('--culling', action='store_true', help='Require nonzero culled-batch shadow and native coverage; implies native-unlit')
    parser.add_argument('--lit-clipping', action='store_true', help='Require clipped lit packet/scratch parity; implies lit-plan')
    parser.add_argument('--lit-plan', action='store_true', help='Verify lit second-stream attributes and headers; keeps original execution')
    parser.add_argument('--clipping', action='store_true', help='Require partial-clipping packet and scratch-buffer parity; implies unlit-plan')
    parser.add_argument('--native-lit', action='store_true', help='Require actual native lit execution matching the full shadow subset')
    parser.add_argument('--second-pass', action='store_true', help='Require positive native GPU secondary material coverage in every mesh mode')
    parser.add_argument('--native-terrain', action='store_true', help='Compare experimental terrain VU bypass with full continuation evidence')
    parser.add_argument('--terrain-only', action='store_true', help='Require terrain GPU evidence without requiring model geometry in the scene')
    parser.add_argument('--terrain-mesh', action='store_true', help='Exercise native terrain GPU projection (experimental)')
    parser.add_argument('--terrain-prep', action='store_true', help='Verify terrain decoding and distance morphing without replacing execution')
    args = parser.parse_args()
    args.terrain_mesh |= args.terrain_only or args.native_terrain
    args.terrain_prep |= args.terrain_mesh
    args.lit_plan |= args.lit_clipping or args.native_lit
    args.native_unlit |= args.culling
    verify_unlit = args.unlit_plan or args.native_unlit or args.clipping
    args.unlit_plan = verify_unlit or args.lit_plan or args.terrain_prep
    if not args.capture.is_file() or args.fields < 1:
        parser.error('Provide an existing capture and a positive field count.')
    require_complete_capture(args.capture, args.fields)
    root = setup.ROOT
    cfg = json.loads(setup.LOCAL.read_text())
    build = root / 'build/native-poc'
    work = args.output_root.resolve() if args.output_root else root / 'work'
    work.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='model-gpu-parity-', dir=work))
    print(f'Private test artifacts: {output}', flush=True)
    env = setup.environment(cfg)
    env.update(PS2_SHADER_DIR=str(build / 'shaders'),
               PS2_SETTINGS_FILE=str(output / 'settings.ini'), PS2_VU_RECOMP='1', PS2_LOCKSTEP='1')
    if args.scale is not None:
        env['PS2_UPSCALE'] = str(args.scale)
    images = []
    modes = ['off', 'audit', 'fast'] + (['materials'] if args.materials else []) + (['plan'] if args.unlit_plan else []) + (['native'] if args.native_unlit else []) + (['native-materials'] if args.native_unlit and args.materials else [])
    if args.native_lit:
        modes += ['native-lit'] + (['native-lit-materials'] if args.materials else [])
    if args.native_terrain:
        modes += ['native-terrain'] + (['native-terrain-materials'] if args.materials else [])
        if args.native_lit:
            modes += ['native-combined'] + (['native-combined-materials'] if args.materials else [])
    for mode in modes:
        env['PS2_AC4_TERRAIN_NATIVE'] = '1' if mode.startswith(('native-terrain','native-combined')) else '0'
        env['PS2_AC4_MODEL_TEXTURE'] = '0'  # Compare geometry/materials with unchanged sampling.
        env['PS2_AC4_TERRAIN_MESH'] = '1' if args.terrain_mesh and mode not in ('off','plan') else '0'
        env['PS2_AC4_LIT_NATIVE'] = '1' if (mode.startswith('native-lit') or mode.startswith('native-combined')) else '0'
        env['PS2_AC4_MODEL_MESH'] = '0' if mode == 'off' else '1'
        env['PS2_AC4_MODEL_NATIVE'] = '1' if mode in ('native','native-materials') else '0'
        env['PS2_AC4_MODEL_PLAN'] = '1' if mode == 'plan' else '0'
        env['PS2_AC4_MODEL_AUDIT'] = '1' if mode == 'audit' else '0'
        env['PS2_AC4_MODEL_MATERIAL'] = '1' if mode == 'materials' or mode.endswith('-materials') else '0'
        shot = output / f'{mode}.ppm'
        log = output / f'{mode}.log'
        with log.open('w') as stream:
            subprocess.run([str(build / 'gsreplay.exe'), str(args.capture.resolve()),
                            '--loop', '1', '--fields', str(args.fields), '--hidden', '--shot', str(shot)],
                           cwd=output, env=env, stdout=stream, stderr=stream,
                           check=True, timeout=120)
        data = shot.read_bytes()
        require_visible_ppm(data)
        images.append(data)
    draws = None
    if not args.terrain_only:
        report = (output / 'audit.log').read_text(errors='replace')
        draws = re.search(r'(\d+) model-space draws', report)
        audit = re.search(r'ac4 model audit: (\d+) vertices, (\d+) exact,', report)
        if not draws or int(draws[1]) == 0 or not audit or int(audit[1]) == 0 or audit[1] != audit[2]:
            raise SystemExit('FAIL: capture did not exercise verified model draws; inspect logs.')
        fast = (output / 'fast.log').read_text(errors='replace')
        forwarded = re.search(r'ac4 model forwarding: (\d+) vertices', fast)
        validate_mesh_gif(fast)
        fast_draws = re.search(r'(\d+) model-space draws', fast)
        if not forwarded or int(forwarded[1]) == 0 or not fast_draws or fast_draws[1] != draws[1]:
            raise SystemExit('FAIL: fast path was not exercised or model draw counts differ.')
        if 'ac4 model audit: 0 vertices, 0 exact,' not in fast:
            raise SystemExit('FAIL: fast mode unexpectedly repeated the CPU reference audit.')
    if args.materials:
        native = (output / 'materials.log').read_text(errors='replace')
        covered = re.search(r'(\d+) recorded with the native material', native)
        if not covered or int(covered[1]) == 0:
            raise SystemExit('FAIL: native material path was not exercised.')
    if args.unlit_plan:
        report = (output / 'plan.log').read_text(errors='replace')
        if args.terrain_prep:
            if re.search(r'ac4 terrain mode 1: [1-9]', report):
                print(f'Terrain polygons: {validate_terrain_polygons(report)} activations/empty/packets match; full terrain coverage still checked separately.')
            print(f'Terrain preparation/packets: {validate_terrain_packets(report)} patches/vertices match.')
            print(f'Terrain continuation memory: {validate_terrain_memory(report)} exact activations/exclusions.')
            print(f'Terrain integers: {validate_terrain_integers(report)} exact exits/kicks/exclusions.')
            print(f'Terrain scalars: {validate_terrain_scalars(report)} exact exits/kicks.')
            print(f'Terrain instructions: {validate_terrain_instructions(report)} activations/instructions/exclusions.')
            print(f'Terrain vectors: {validate_terrain_vectors(report)} exits/kicks/exclusions.')
            print(f'Terrain flags: {validate_terrain_flags(report)} activations/exclusions.')
            print(f'Terrain full continuation: {validate_terrain_continuation(report)} activations/packets/instructions.')
        if verify_unlit:
            built, declined, packets = validate_unlit_report(report)
        if args.lit_clipping:
            print(f'Lit clipping: {validate_lit_clipping(report)} packets and scratch writes match.')
        if args.lit_plan:
            print(f'Lit memory/order/registers/counts/flags: {validate_lit_flags(report)} activations/packets match.')
            print(f'Lit secondary: {validate_lit_secondary(report)} vertices and stream headers match including ADC; clipping excluded.')
        if args.clipping:
            print(f'Partial clipping: {validate_clipping_report(report)} packets, scratch writes, integer controls and traced vector state match.')
        if args.culling:
            culled = validate_culling_report(report)
        if verify_unlit:
            print(f'Unlit packet plan: {built} activations, {packets} ordered packets, full memory and mapped controls match; {declined} declined.')
    if args.native_unlit:
        shadow_report = (output / 'plan.log').read_text(errors='replace')
        for mode in (['native', 'native-materials'] if args.materials else ['native']):
            report = (output / f'{mode}.log').read_text(errors='replace')
            native = re.search(r'ac4 unlit native: (\d+) activations, (\d+) packets, (\d+) original instructions replaced', report)
            if not native or int(native[1]) != built or int(native[2]) != packets or not int(native[3]):
                raise SystemExit('FAIL: native execution coverage differs from the shadow-verified subset.')
            if args.clipping:
                runs, mixed_packets, instructions = validate_mixed_execution(shadow_report, report)
                print(f'{mode}: mixed execution replaces {instructions} instructions across {runs} activations/{mixed_packets} packets.')
            if args.culling and validate_culling_report(report, native=True) != culled:
                raise SystemExit('FAIL: native culled activation count differs from shadow verification.')
            if mode == 'native-materials':
                material = re.search(r'(\d+) recorded with the native material', report)
                if not material or not int(material[1]):
                    raise SystemExit('FAIL: combined native execution/materials path was not exercised.')
            print(f'{mode}: {native[1]} activations, {native[3]} original instructions replaced.')
    if args.native_lit:
        shadow = (output / 'plan.log').read_text(errors='replace')
        for mode in (['native-lit','native-lit-materials'] if args.materials else ['native-lit']):
            report = (output / f'{mode}.log').read_text(errors='replace')
            runs, packets, instructions = validate_lit_execution(shadow, report)
            if mode.endswith('materials'):
                material = re.search(r'(\d+) recorded with the native material', report)
                if not material or not int(material[1]):
                    raise ValueError('Native lit/materials combination did not exercise native materials.')
            print(f'{mode}: {runs} activations, {instructions} original instructions replaced.')
    if args.second_pass:
        counts = [validate_second_pass((output / f'{mode}.log').read_text(errors='replace')) for mode in modes if mode != 'off']
        if len(set(counts)) != 1:
            raise ValueError('Secondary GPU draw coverage differs across mesh modes.')
        print(f'Native secondary material: {counts[0]} draws in each mesh mode.')
    if args.native_terrain:
        shadow=(output/'plan.log').read_text(errors='replace')
        native=(output/'native-terrain.log').read_text(errors='replace')
        print(f'Native terrain execution: {validate_terrain_execution(shadow,native)} activations/packets/instructions.')
        if args.native_lit:
            combined=(output/'native-combined.log').read_text(errors='replace')
            print(f'Combined native terrain: {validate_terrain_execution(shadow,combined)}.')
            print(f'Combined native lit models: {validate_lit_execution(shadow,combined)}.')
    if args.native_terrain and args.materials:
        shadow=(output/'plan.log').read_text(errors='replace')
        for mode in ['native-terrain-materials'] + (['native-combined-materials'] if args.native_lit else []):
            report=(output/f'{mode}.log').read_text(errors='replace')
            validate_terrain_execution(shadow,report)
            if mode.startswith('native-combined'):
                validate_lit_execution(shadow,report)
            material=re.search(r'(\d+) recorded with the native material',report)
            if not material or not int(material[1]):
                raise ValueError(f'{mode}: native material coverage is missing')
            print(f'{mode}: verified native execution and {material[1]} native material draws.')
    if args.terrain_mesh:
        coverage = [validate_terrain_gpu((output / f'{mode}.log').read_text(errors='replace'))
                    for mode in modes if mode not in ('off','plan')]
        if len(set(coverage)) != 1:
            raise ValueError('Terrain GPU coverage differs across renderer modes.')
        print(f'Native terrain GPU: {coverage[0]} primary/secondary draws per mesh mode.')
        if re.search(r'ac4 terrain mode 1: [1-9]', (output / 'plan.log').read_text(errors='replace')):
            clipped = [validate_clipped_terrain_gpu((output / f'{mode}.log').read_text(errors='replace'))
                       for mode in modes if mode not in ('off','plan')]
            if len(set(clipped)) != 1:
                raise ValueError('Clipped terrain GPU coverage differs across modes.')
            print(f'Native clipped terrain GPU: {clipped[0]} primary/secondary draws.')
    gif_counts=[validate_mesh_gif((output/f'{mode}.log').read_text(errors='replace'))
                for mode in modes if mode not in ('off','audit','plan')]
    if len(set(gif_counts))!=1:
        raise ValueError('Whole mesh GIF packet coverage differs across enabled fast/native modes.')
    print(f'Whole mesh GIF bypass: {gif_counts[0]} packets/qwords per fast/native mode.')
    if any(img != images[0] for img in images[1:]):
        raise SystemExit('FAIL: final PPM differs; inspect the retained images and logs.')
    print(f'PASS: {draws[1] + ' GPU model draws' if draws else 'terrain-only coverage'}; all {len(images)} mode PPMs identical. '
          f'SHA256 {hashlib.sha256(images[0]).hexdigest()}')
    print('This compares the final captured image only, not every frame or full gameplay.')


if __name__ == '__main__':
    main()
