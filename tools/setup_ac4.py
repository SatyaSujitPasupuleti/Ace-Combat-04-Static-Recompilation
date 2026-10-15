"""Build AC4 from a user-supplied ISO; no downloaded game data or Python packages."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
LOCAL = ROOT / 'local.json'
PROFILE = json.loads((ROOT / 'config/ac4/profile.json').read_text())


def extract(iso):
    """Read the supported root executable from an ISO9660 primary volume."""
    with iso.open('rb') as f:
        f.seek(16 * 2048)
        pvd = f.read(2048)
        if pvd[:7] != b'\x01CD001\x01':
            raise ValueError('Expected an uncompressed ISO9660 ISO image.')
        root = pvd[156:190]
        lba = int.from_bytes(root[2:6], 'little')
        size = int.from_bytes(root[10:14], 'little')
        if not 0 < size <= 16 * 1024 * 1024:
            raise ValueError('Invalid ISO root directory size.')
        f.seek(lba * 2048)
        directory = f.read(size)
        pos = 0
        while pos < len(directory):
            n = directory[pos]
            if n == 0:
                pos = (pos // 2048 + 1) * 2048
                continue
            entry = directory[pos:pos+n]
            if len(entry) < 34:
                raise ValueError('Truncated ISO directory record.')
            pos += n
            name = entry[33:33+entry[32]].split(b';')[0]
            if name != PROFILE['serial'].encode('ascii'):
                continue
            length = int.from_bytes(entry[10:14], 'little')
            if not 0 < length <= 64 * 1024 * 1024:
                raise ValueError('Invalid executable size.')
            f.seek(int.from_bytes(entry[2:6], 'little') * 2048)
            data = f.read(length)
            if len(data) != length or hashlib.sha256(data).hexdigest() != PROFILE['elf_sha256']:
                raise ValueError('Unsupported executable hash. Use the original NTSC-U SLUS_201.52 disc.')
            return data
    raise ValueError('SLUS_201.52 was not found in the ISO root.')


def environment(cfg):
    env = os.environ.copy()
    # Isolate game tests from developer-only fault injection/environment switches.
    for key in list(env):
        if key.startswith('PS2_'):
            del env[key]
    prefix = Path(cfg['toolchain'])
    env['PATH'] = str(prefix / 'bin') + os.pathsep + env.get('PATH', '')
    env['CMAKE_PREFIX_PATH'] = str(prefix)
    env['PKG_CONFIG_PATH'] = str(prefix / 'lib/pkgconfig')
    env['PYTHONPATH'] = str(ROOT / 'tools')
    return env


def call(args, env):
    subprocess.run([str(x) for x in args], cwd=ROOT, env=env, check=True)


def build(cfg, native, jobs):
    env = environment(cfg)
    prefix = Path(cfg['toolchain']) / 'bin'
    build_dir = ROOT / ('build/native-poc' if native else 'build/bootstrap')
    generated = ROOT / 'work/public-vu-generated'
    if native:
        if not (ROOT / 'out/vu_programs/manifest.json').is_file():
            raise ValueError('Run capture-vu and exit normally before build-native.')
        generated.mkdir(parents=True, exist_ok=True)
        call([sys.executable, '-m', 'vurecomp', '--programs', ROOT / 'out/vu_programs',
              '-o', generated / 'ps2_vu1_progs.inc'], env)
    call([prefix / 'cmake.exe', '-S', ROOT, '-B', build_dir, '-G', 'Ninja',
          '-DCMAKE_BUILD_TYPE=Release', '-DPS2_DIAG=OFF', '-DPS2_GENERATED_OPT=-O1',
          f'-DCMAKE_C_COMPILER={(prefix / "gcc.exe").as_posix()}',
          f'-DCMAKE_CXX_COMPILER={(prefix / "g++.exe").as_posix()}',
          f'-DCMAKE_MAKE_PROGRAM={(prefix / "ninja.exe").as_posix()}',
          '-DPS2_VU_PROGRAM_DIR=' + (generated.as_posix() if native else '')], env)
    call([prefix / 'cmake.exe', '--build', build_dir, '--target', 'ac4', '-j', jobs], env)


def run(cfg, capture, memory):
    env = environment(cfg)
    (ROOT / 'saves').mkdir(exist_ok=True)
    folder = ROOT / ('build/bootstrap' if capture else 'build/native-poc')
    if not (folder / 'ac4.exe').is_file():
        raise ValueError('Build the selected executable first.')
    env.update(PS2_SHADER_DIR=str(folder / 'shaders'),
               PS2_SETTINGS_FILE=str(folder / 'ac4_settings.ini'),
               PS2_PIPELINE_CACHE=str(folder / 'ac4_pipelines'),
               PS2_VU_RECOMP='0' if capture else '1', PS2_FPS_CAP='60',
               PS2_AC4_OPENING='Show', PS2_PRESENT_QUEUE='1',
               PS2_T4_PARTIAL='1', PS2_T4_SOURCE_HASH='1',
               PS2_PIPELINE_PREWARM='1', PS2_IMAGE_PREWARM='15',
               PS2_SNAPSHOT_PREWARM='8', PS2_RT_RESERVE_HEIGHT='896', PS2_SHOT_EVERY='0')
    if capture:
        (ROOT / 'out/vu_programs').mkdir(parents=True, exist_ok=True)
        env['PS2_VU_CENSUS'] = '1'
        print('Capture: play representative scenes, then exit with Escape. Each session replaces the manifest.', flush=True)
    if memory:
        env['PS2_DISC_PRELOAD'] = '1'
    call([folder / 'ac4.exe', '--data', ROOT / 'generated', '--disc', cfg['iso'],
          '--seconds', '0', '--vblanks', '0', '--watchdog', '30'], env)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('action', choices=['prepare', 'build', 'capture-vu', 'build-native', 'run'])
    ap.add_argument('--iso', type=Path)
    ap.add_argument('--toolchain', type=Path, help='MSYS2 ucrt64 prefix, e.g. C:/msys64/ucrt64')
    ap.add_argument('--jobs', type=int, default=4)
    ap.add_argument('--disc-cache', choices=['stream', 'memory'], default='stream')
    args = ap.parse_args()
    if args.jobs < 1:
        ap.error('--jobs must be positive')
    if args.action == 'prepare':
        if not args.iso or not args.toolchain:
            ap.error('prepare needs --iso and --toolchain')
        cfg = {'iso': str(args.iso.resolve()), 'toolchain': str(args.toolchain.resolve())}
        for binary in ('gcc.exe', 'g++.exe', 'cmake.exe', 'ninja.exe', 'pkg-config.exe', 'glslc.exe'):
            if not (args.toolchain / 'bin' / binary).is_file():
                raise ValueError('Missing toolchain binary: ' + binary)
        data = extract(args.iso)
        work = ROOT / 'work/public-input'
        work.mkdir(parents=True, exist_ok=True)
        elf = work / PROFILE['serial']
        elf.write_bytes(data)
        call([sys.executable, ROOT / 'tools/ac4.py', 'generate', '--elf', elf], environment(cfg))
        LOCAL.write_text(json.dumps(cfg, indent=2))
        print('Verified ISO executable and generated local game code. Next: build.')
        return
    if not LOCAL.is_file():
        ap.error('Run prepare first.')
    cfg = json.loads(LOCAL.read_text())
    if args.action in ('build', 'build-native'):
        build(cfg, args.action == 'build-native', args.jobs)
    else:
        run(cfg, args.action == 'capture-vu', args.disc_cache == 'memory')


if __name__ == '__main__':
    try:
        main()
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        raise SystemExit(str(error))
