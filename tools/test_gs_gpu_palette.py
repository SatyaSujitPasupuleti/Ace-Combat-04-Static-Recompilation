"""Asset-free GPU palette regression; run with --replay path/to/gsreplay.exe."""
import argparse
import os
from pathlib import Path
import struct
import subprocess
import tempfile


def fixture(path):
    stream = bytearray()
    count = 0
    def reg(r, v):
        nonlocal count
        stream.extend(struct.pack('<BBQ', 1, r, v)); count += 1
    def priv(r, v):
        nonlocal count
        stream.extend(struct.pack('<BIQ', 3, r, v)); count += 1
    def sprite(x0, y0, x1, y1, color):
        reg(1, (0x3f800000 << 32) | color)
        reg(5, x0*16 | (y0*16 << 16))
        reg(5, x1*16 | (y1*16 << 16))
    # A GPU-written 16x16 CSM1 palette; CPU VRAM remains all zero.
    colors = [0x800000ff, 0x8000ff00, 0x80ff0000, 0x80ffffff]
    indices = [0, 8, 16, 255]  # Exercise the CSM1 bit 3/4 permutation.
    pal = 0x200000
    tex = 0x300000
    for field in range(4):
        priv(0x12000000, 1)
        priv(0x12000070, 1 << 9)
        priv(0x12000080, (63 << 32) | (63 << 44))
        reg(0x1a, 1); reg(0x18, 0)
        reg(0x40, (63 << 16) | (63 << 48))
        reg(0x4e, 1 << 32); reg(0x47, 0); reg(0x46, 1)
        reg(0x4c, pal//8192 | 1 << 16)
        reg(0, 6)
        sprite(0, 0, 64, 32, 0)
        for idx, color in zip(indices, colors[field:] + colors[:field]):
            entry = (idx & ~24) | ((idx & 8) << 1) | ((idx & 16) >> 1)
            x, y = entry % 16, entry // 16
            sprite(x, y, x+1, y+1, color)
        # Upload four index bands through T8H.
        reg(0x50, (tex//256 << 32) | (1 << 48) | (27 << 56))
        reg(0x51, 0); reg(0x52, 64 | (64 << 32)); reg(0x53, 0)
        pixels = bytes(indices[x//16] for y in range(64) for x in range(64))
        for k in range(0, len(pixels), 8):
            reg(0x54, int.from_bytes(pixels[k:k+8], 'little'))
        reg(0x4c, 1 << 16)
        reg(6, tex//256 | (1 << 14) | (27 << 20) | (6 << 26) |
            (6 << 30) | (1 << 34) | (1 << 35) | (pal//256 << 37) | (1 << 61))
        reg(0x14, 0); reg(8, 5)
        reg(0, 6 | (1 << 4) | (1 << 8))
        reg(1, (0x3f800000 << 32) | 0x80808080)
        reg(3, 0); reg(5, 0)
        reg(3, 1024 | (1024 << 16)); reg(5, 1024 | (1024 << 16))
        stream.extend(struct.pack('<BII', 11, field, field & 1)); count += 1
    stream.append(13); count += 1
    sizes = [11208, 2448, 64, 4194304, 16384, 16384, 16384, 4096, 4096]
    hdr = bytearray(176)
    hdr[:8] = b'GSCAP2\0\0'
    struct.pack_into('<11I', hdr, 8, 2, 0, *sizes)
    struct.pack_into('<QQQQ', hdr, 96, 176+sum(sizes), len(stream), 0, count)
    struct.pack_into('<I', hdr, 128, 4)
    path.write_bytes(hdr + bytes(sum(sizes)) + stream)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--replay', required=True, type=Path)
    args = ap.parse_args()
    replay = args.replay.resolve()
    with tempfile.TemporaryDirectory(prefix='gs-palette-') as tmp:
        root = Path(tmp)
        cap = root/'palette.gscap'
        fixture(cap)
        for scale in (1, 5):
            env = os.environ.copy()
            env.update(PS2_UPSCALE=str(scale), PS2_SHADER_DIR=str(replay.parent/'shaders'))
            shot = root/f'palette{scale}.ppm'
            result = subprocess.run([str(replay), str(cap), '--fields', '4', '--shot', str(shot)],
                                    cwd=root, env=env, capture_output=True, text=True)
            assert result.returncode == 0, result.stdout + result.stderr
            data = shot.read_bytes()
            header, width_height, depth, pixels = data.split(b'\n', 3)
            width, height = map(int, width_height.split())
            assert header == b'P6' and depth == b'255'
            actual = [tuple(pixels[((height//2)*width + width*(2*i+1)//8)*3:]
                            [:3]) for i in range(4)]
            expected = [(255,255,255), (255,0,0), (0,255,0), (0,0,255)]
            assert actual == expected, (scale, actual)
            print(f'GPU CSM1 palette at {scale}x: {actual} PASS')


if __name__ == '__main__':
    main()
