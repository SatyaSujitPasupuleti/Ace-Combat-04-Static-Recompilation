"""Asset-free GS replay regression: upload bursts and cache turnover."""
import argparse
import os
from pathlib import Path
import struct
import subprocess
import tempfile


def fixture(path, mixed=False):
    fields = 48 if mixed else 17
    stream = bytearray()
    count = 0
    def reg(r, v):
        nonlocal count
        stream.extend(struct.pack('<BBQ', 1, r, v)); count += 1
    def priv(r, v):
        nonlocal count
        stream.extend(struct.pack('<BIQ', 3, r, v)); count += 1
    colors = [(20 + i * 11, 50, 200 - i * 7) for i in range(18)]
    # 288 unique 1 MB decoded textures exceed the old 512 MB double-buffered
    # cache. Each field needs 18 MB of uploads, above the old 16 MB staging cap.
    for field in range(fields):
        priv(0x12000000, 1); priv(0x12000070, 2 << 9)
        priv(0x12000080, 127 << 32 | 63 << 44)
        reg(0x1a, 1); reg(0x18, 0); reg(0x40, 127 << 16 | 63 << 48)
        reg(0x4e, 1 << 32); reg(0x47, 0); reg(0x46, 1)
        reg(0x4c, 2 << 16); reg(0x14, 0); reg(8, 5)
        for i, (r, g, b) in enumerate(colors):
            color = r | (g + field * 3) << 8 | b << 16 | 0x80000000
            reg(0x50, (0x300000 // 256 << 32) | 1 << 48)
            reg(0x51, 0); reg(0x52, 16 | 1 << 32); reg(0x53, 0)
            for _ in range(8): reg(0x54, color | color << 32)
            # Final field revisits evicted keys to exercise stale GS cache IDs.
            index = (field if field < 16 else 0) * 18 + i
            base = 0x100000 + index * 256
            reg(6, base // 256 | 8 << 14 | 19 << 20 | (8 if mixed and field >= 16 and field % 2 == 0 else 9) << 26 | (10 if mixed and field >= 16 and field % 2 == 0 else 9) << 30
                | 1 << 34 | 1 << 35 | (0x300000 // 256) << 37 | 1 << 61)
            reg(0, 6 | 1 << 4 | 1 << 8); reg(1, 0x3f80000080808080)
            reg(3, 8 | 8 << 16); reg(5, (i * 7) * 16)
            reg(3, 24 | 24 << 16); reg(5, (i * 7 + 7) * 16 | (64 * 16) << 16)
        stream.extend(struct.pack('<BII', 11, field, field & 1)); count += 1
    stream.append(13); count += 1
    sizes = [11208, 2448, 64, 4194304, 16384, 16384, 16384, 4096, 4096]
    header = bytearray(176); header[:8] = b'GSCAP2\0\0'
    struct.pack_into('<11I', header, 8, 2, 0, *sizes)
    struct.pack_into('<QQQQ', header, 96, 176 + sum(sizes), len(stream), 0, count)
    struct.pack_into('<I', header, 128, fields)
    path.write_bytes(header + bytes(sum(sizes)) + stream)
    return [(r, g + (fields - 1) * 3, b) for r, g, b in colors]


def main():
    ap = argparse.ArgumentParser(); ap.add_argument('--replay', type=Path, required=True)
    ap.add_argument('--shaders', type=Path)
    ap.add_argument('--mixed', action='store_true')
    ap.add_argument('--log', type=Path)
    args = ap.parse_args(); replay = args.replay.resolve()
    with tempfile.TemporaryDirectory(prefix='gs-pressure-') as td:
        root = Path(td); cap = root / 'pressure.gscap'; shot = root / 'shot.ppm'
        colors = fixture(cap, args.mixed)
        env = {k: v for k, v in os.environ.items() if not k.startswith('PS2_')}
        env.update(PS2_UPSCALE='1', PS2_PRESENT_MODE='immediate', PS2_PIPELINE_CACHE=str(root / 'pipelines'), PS2_TEX_STAGING_MB='16',
                   PS2_SHADER_DIR=str((args.shaders or replay.parent / 'shaders').resolve()))
        run = subprocess.run([str(replay), str(cap), '--shot', str(shot)], env=env, capture_output=True)
        log = (run.stdout + run.stderr).decode(errors='replace')
        if args.log: args.log.write_text(log)
        assert run.returncode == 0, log
        with shot.open('rb') as f:
            assert f.readline().strip() == b'P6'
            w, h = map(int, f.readline().split()); assert f.readline().strip() == b'255'
            data = f.read()
        samples = [tuple(data[(32 * w + i * 7 + 3) * 3: (32 * w + i * 7 + 3) * 3 + 3]) for i in range(18)]
        assert samples == colors, (samples, colors, log[-2400:])
        assert 'staging exhausted' not in log and 'store exhausted' not in log, log
        assert 'cold texture allocations recycled' in log, log
        assert 'texture staging grew' in log, log
        if args.mixed:
            assert '864 draw calls' in log and '864 texture uploads' in log, log
            assert 'texture pressure collection:' in log, log
            assert 'texture images in 4 blocks of 64 MB' in log, log
        print('PASS: mixed-size turnover' if args.mixed else 'PASS: same-size turnover', 'and 18 MB upload bursts; all 18 color samples exact')


if __name__ == '__main__': main()
