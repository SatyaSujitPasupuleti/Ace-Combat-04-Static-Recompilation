"""Require complete live cloud-plane projection evidence, not GPU acceptance."""
import argparse
from pathlib import Path
import re


def validate(text,require_packets=False,require_sprites=False,require_rows=False,require_mapping=False,require_continuation=False,require_native_rows=False):
    if require_continuation and require_native_rows:
        raise ValueError('Original continuation comparison and native replacement require separate runs')
    coverage=re.findall(r'ac4 cloud audit: (\d+) sprite projections, (\d+) plane vertices, (\d+) non-rejected plane vertices',text)
    plans=re.findall(r'ac4 cloud point plan: (\d+) matched, (\d+) mismatched, (\d+) declined',text)
    if len(coverage)!=1 or len(plans)!=1:
        raise ValueError('Require exactly one coverage report and one cloud point plan report')
    sprites,vertices,visible=map(int,coverage[0])
    if not vertices or not visible or visible>vertices or tuple(map(int,plans[0]))!=(vertices,0,0):
        raise ValueError('Require positive plane coverage with every call matched and no declines')
    result=dict(sprite_calls=sprites,plane_calls=vertices,non_rejected=visible)
    if require_rows or require_mapping or require_continuation or require_native_rows:
        reports=re.findall(r'ac4 cloud sprite rows: (\d+) calls, (\d+) matched, (\d+) mismatched, (\d+) declined, (\d+) pieces',text)
        if len(reports)!=1:raise ValueError('Require exactly one cloud sprite row report')
        calls,matched,bad,declined,pieces=map(int,reports[0])
        if not calls or calls!=matched or bad or declined or not pieces or pieces>64*calls:
            raise ValueError('Require positive complete cloud row coverage with no mismatches/declines')
        result.update(row_calls=calls,sprite_pieces=pieces)
        if require_continuation:
            reports=re.findall(r'ac4 cloud row continuation: (\d+) matched, (\d+) mismatched',text)
            if len(reports)!=1 or tuple(map(int,reports[0]))!=(calls,0):
                raise ValueError('Require full row continuation agreement for every call')
        if require_native_rows:
            native=re.findall(r'ac4 native rows: (\d+) replaced, (\d+) declined',text)
            shadow=re.findall(r'ac4 cloud row continuation: (\d+) matched, (\d+) mismatched',text)
            if len(native)!=1 or tuple(map(int,native[0]))!=(calls,0):
                raise ValueError('Require every observed row natively replaced without declines')
            if len(shadow)!=1 or tuple(map(int,shadow[0]))!=(0,0):
                raise ValueError('Native replacements must not count as original continuation comparisons')
        if require_mapping:
            mapping=re.findall(r'ac4 cloud sprite mapping: (\d+) mapped, (\d+) unmapped rows',text)
            if len(mapping)!=1 or tuple(map(int,mapping[0]))!=(calls,0):
                raise ValueError('Require every cloud sprite row mapped without ambiguity or missing inputs')
    if require_sprites:
        reports=re.findall(r'ac4 cloud sprite plan: (\d+) matched, (\d+) mismatched, (\d+) declined',text)
        if len(reports)!=1 or not sprites or tuple(map(int,reports[0]))!=(sprites,0,0):
            raise ValueError('Require positive sprite coverage with every call matched and no declines')
    if require_packets:
        reports=re.findall(r'ac4 cloud packets: (\d+) batches, (\d+) mapped, (\d+) declined, (\d+) quads',text)
        if len(reports)!=1:raise ValueError('Require exactly one cloud packet report')
        batches,mapped,declined,quads=map(int,reports[0])
        if not batches or not quads or batches!=mapped or declined:
            raise ValueError('Require positive complete cloud batch/quad mapping with no declines')
        result.update(batches=batches,quads=quads)
    return result


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('log',type=Path)
    parser.add_argument('--packets',action='store_true',help='Require complete cloud packet-to-input mapping')
    parser.add_argument('--sprites',action='store_true',help='Require exact sprite projection/output coverage')
    parser.add_argument('--rows',action='store_true',help='Require exact complete cloud sprite row packets')
    parser.add_argument('--mapping',action='store_true',help='Require all sprite rows mapped to original world inputs')
    parser.add_argument('--continuation',action='store_true',help='Require full scalar/vector context agreement for every row')
    parser.add_argument('--native-rows',action='store_true',help='Require native replacement of every observed row in a separate run')
    args=parser.parse_args()
    try:result=validate(args.log.read_text(errors='replace'),args.packets,args.sprites,args.rows,args.mapping,args.continuation,args.native_rows)
    except (OSError,ValueError) as error:parser.exit(1,str(error)+'\n')
    print(result)
    print('Requested cloud projection/packet evidence passed; GPU drawing and guest bypass remain separate gates.')
