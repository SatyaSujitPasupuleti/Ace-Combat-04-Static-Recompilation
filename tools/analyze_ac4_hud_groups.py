"""Validate fixed HUD producer census from a private AC4 runtime log."""
import argparse
import json
from pathlib import Path
import re

SITES = {0x162958: "PTS", 0x1633c8: "TIME", 0x163818: "weapons", 0x163c08: "DMG", 0x162d98: "TGT", 0x1609d8: "speed", 0x161250: "altitude", 0x160ff8: "speed bracket", 0x161890: "altitude bracket", 0x160e78: "alternate speed", 0x161720: "alternate altitude", 0x169010: "radar"}
ALTERNATE = {0x160e78, 0x161720}

def validate(text, native=False, views=False, map_candidate=False):
    rows = re.findall(r"ac4 HUD group: site ([0-9a-fA-F]{8}) calls (\d+) matched (\d+) empty (\d+) declined (\d+) packets (\d+)", text)
    result = {}
    for address, *counts in rows:
        site = int(address, 16)
        if site not in SITES or site in result:
            raise ValueError("unknown or duplicate HUD producer")
        calls, matched, empty, declined, packets = map(int, counts)
        if (not matched and (views or site not in ALTERNATE)) or declined or calls != matched + empty or packets < matched:
            raise ValueError(f"incomplete or inconsistent HUD coverage at {address}")
        result[site] = dict(name=SITES[site], calls=calls, matched=matched, empty=empty, packets=packets)
    if result.keys() != SITES.keys():
        raise ValueError("missing HUD producer coverage")
    balance = re.findall(r"ac4 HUD group balance: (\d+) open, (\d+) unpaired", text)
    if balance != [("0", "0")]:
        raise ValueError("missing, duplicate or unbalanced HUD hook report")
    if map_candidate:
        candidates = re.findall(r"ac4 HUD candidate: site 0016[bB][cC]68 calls (\d+) matched (\d+) empty (\d+) declined (\d+) packets (\d+)", text)
        if len(candidates) != 1:
            raise ValueError("missing or duplicate alternate-map candidate coverage")
        calls, matched, empty, declined, packets = map(int, candidates[0])
        if not matched or declined or calls != matched + empty or packets < matched:
            raise ValueError("incomplete or inconsistent alternate-map candidate coverage")
    if native:
        ownership = re.findall(r"ac4 HUD group ownership: (\d+) queued, (\d+) declined", text)
        expected = sum(row["matched"] for row in result.values())
        if ownership != [(str(expected), "0")]:
            raise ValueError("missing, inconsistent or declined native group ownership")
    return {f"{site:08x}": row for site, row in result.items()}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--views", action="store_true", help="Also require alternate-view speed/altitude coverage")
    parser.add_argument("--native", action="store_true", help="Require every matched producer to have native ownership")
    parser.add_argument("--map-candidate", action="store_true", help="Require validated observer-only alternate-map packets, not native ownership")
    args = parser.parse_args()
    report = json.dumps(validate(args.log.read_text(errors="replace"), args.native, args.views, args.map_candidate), indent=2)
    if args.output:
        args.output.write_text(report + "\n")
    print(report)

if __name__ == "__main__":
    main()
