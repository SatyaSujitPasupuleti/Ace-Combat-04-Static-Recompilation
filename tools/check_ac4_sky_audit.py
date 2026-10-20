"""Validate live AC4 sky-band evidence; this does not certify native sky rendering."""
import argparse
from pathlib import Path
import re
import struct

REPORT = re.compile(r'ac4 sky audit: (\d+) bands, (\d+) matched, (\d+) mismatched, (\d+) declined; (\d+) domes, (\d+) hazes')


def validate(log, snapshot, require_dome=False, require_haze=False, require_continuation=False, require_native=False):
    if require_continuation and require_native:raise ValueError('Use separate original and native runs')
    reports = REPORT.findall(log)
    if len(reports) != 1:
        raise ValueError('Expected exactly one final sky audit report; zero coverage is not a pass.')
    bands, matched, mismatched, declined, domes, hazes = map(int, reports[0])
    if require_native:
        native=re.findall(r'ac4 native sky bands: (\d+) replaced, (\d+) declined',log)
        shadow=re.findall(r'ac4 sky band continuation: (\d+) matched, (\d+) mismatched',log)
        if len(native)!=1 or tuple(map(int,native[0]))!=(bands,0) or len(shadow)!=1 or tuple(map(int,shadow[0]))!=(0,0):
            raise ValueError('Require complete native band coverage, no declines/mismatches and no self-counted shadow evidence')
    if require_continuation:
        reports=re.findall(r'ac4 sky band continuation: (\d+) matched, (\d+) mismatched',log)
        if len(reports)!=1 or tuple(map(int,reports[0]))!=(bands,0):
            raise ValueError('Require complete sky-band context agreement for every call')
    if not bands or not domes or matched != bands or mismatched or declined:
        raise ValueError('Sky audit requires positive dome/band coverage and all bands matched without declines.')
    if require_dome:
        plans=re.findall(r'ac4 sky dome plan: (\d+) matched, (\d+) mismatched, (\d+) declined',log)
        if len(plans)!=1 or tuple(map(int,plans[0]))!=(domes,0,0):
            raise ValueError('Full dome plan requires exact coverage of every original dome with no declines.')
    if require_haze:
        plans=re.findall(r'ac4 sky haze plan: (\d+) matched, (\d+) mismatched, (\d+) declined',log)
        if not hazes or len(plans)!=1 or tuple(map(int,plans[0]))!=(hazes,0,0):
            raise ValueError('Full haze plan requires positive exact coverage of every original haze with no declines.')
    if len(snapshot) < 24:
        raise ValueError('Missing or truncated dome fixture header.')
    magic, size, packet_size, obj, start, end = struct.unpack_from('<6I', snapshot)
    def valid_range(address, length):
        return (address < 0x2000000 and length <= 0x2000000-address) or (0x70000000 <= address < 0x70004000 and length <= 0x70004000-address)
    if magic != 0x31594b53 or size != 0x440 or not 48 < packet_size <= 65536 or packet_size % 16:
        raise ValueError('Invalid dome fixture layout or empty geometry.')
    if end-start != packet_size or not valid_range(obj,size) or not valid_range(start,packet_size):
        raise ValueError('Invalid guest fixture ranges.')
    if len(snapshot) != 24+size+packet_size:
        raise ValueError('Dome fixture length does not match its header.')
    return dict(bands=bands, matched=matched, domes=domes, hazes=hazes, packet_bytes=packet_size)


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('log',type=Path)
    ap.add_argument('snapshot',type=Path)
    ap.add_argument('--dome-plan',action='store_true',help='Require complete native dome packet comparisons.')
    ap.add_argument('--haze-plan',action='store_true',help='Require complete native haze packet comparisons.')
    ap.add_argument('--continuation',action='store_true',help='Require complete sky-band context comparisons.')
    ap.add_argument('--native-bands',action='store_true',help='Require complete native sky-band replacement in a separate run.')
    args=ap.parse_args()
    try:
        result=validate(args.log.read_text(errors='replace'),args.snapshot.read_bytes(),args.dome_plan,args.haze_plan,args.continuation,args.native_bands)
    except (OSError,ValueError) as exc:
        ap.exit(1, str(exc)+'\n')
    print(result)
    print('Live sky packet evidence passed; GPU drawing and guest continuation remain separate acceptance gates.')

if __name__=='__main__':
    main()
