"""Map private PS2_RN_2D_LOG observations to generated caller functions.

Produces evidence only: stable bounds do not prove a screen-fixed element.
"""
import argparse
import json
import math
from pathlib import Path
import re


def read_layout(text):
    rows = []
    for number, line in enumerate(text.splitlines(), 1):
        if not line.strip():
            continue
        fields = line.split()
        try:
            if len(fields) != 9:
                raise ValueError('expected nine fields')
            frame, caller, emitter = int(fields[0]), int(fields[1], 16), int(fields[2])
            box = list(map(float, fields[3:7]))
            primitives, native = map(int, fields[7:])
            if (frame < 0 or not 0 <= caller <= 0xffffffff or emitter < 0
                    or primitives < 1 or native not in (0, 1)
                    or not all(map(math.isfinite, box)) or box[2] < box[0] or box[3] < box[1]):
                raise ValueError('invalid bounds or counters')
            rows.append(dict(frame=frame, caller=f'{caller:08X}', emitter=emitter,
                             box=box, primitives=primitives, native=native))
        except ValueError as error:
            raise ValueError(f'Invalid layout line {number}: {error}') from error
    if not rows:
        raise ValueError('No layout observations')
    return rows


def caller_functions(text):
    result = {}
    parent = caller = None
    for line in text.splitlines():
        match = re.match(r'PS2_NOIPA void func_([0-9A-Fa-f]+)\(', line)
        if match:
            parent, caller = match[1].upper(), None
        match = re.search(r'ctx->r\[31\]\.ud\[0\] = \(u64\)\(0x([0-9A-Fa-f]+)ll\)', line)
        if match:
            caller = match[1].upper().zfill(8)
        if line.strip() == 'func_0015FEE0(ctx);' and parent and caller:
            result[caller] = parent
            caller = None
    return result


def summarize(rows, parents):
    groups = {}
    for row in rows:
        group = groups.setdefault(row['caller'], dict(parent=parents.get(row['caller']),
            observations=0, frames=set(), bounds=set(), emitters=set(), native=0))
        group['observations'] += 1
        group['frames'].add(row['frame']); group['bounds'].add(tuple(row['box']))
        group['emitters'].add(row['emitter']); group['native'] += row['native']
    for group in groups.values():
        for key in ('frames', 'bounds', 'emitters'):
            group[key] = sorted(group[key])
    return dict(sorted(groups.items()))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('layout', type=Path)
    parser.add_argument('--generated', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    parents = {}
    for source in args.generated.glob('ps2_code_*.c'):
        parents.update(caller_functions(source.read_text()))
    if not parents:
        raise ValueError('No generated sprite callsites found')
    result = summarize(read_layout(args.layout.read_text()), parents)
    args.output.write_text(json.dumps(result, indent=2))
    print(f'{len(result)} observed callsites; classifications require visual and source review.')


if __name__ == '__main__':
    main()
