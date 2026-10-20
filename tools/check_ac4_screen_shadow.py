"""Require complete original-vs-plan screen-sprite packet evidence, not HUD coverage."""
import argparse
import json
from pathlib import Path
import re


def validate(text):
    reports = re.findall(r'^ac4 screen sprite shadow: (\d+) calls, (\d+) matched, (\d+) mismatched, (\d+) declined, (\d+) unpaired, (\d+) open$', text, re.MULTILINE)
    if len(reports) != 1:
        raise ValueError('Require exactly one complete screen-sprite shadow report')
    calls, matched, mismatched, declined, unpaired, opened = map(int, reports[0])
    if not calls or calls != matched or any((mismatched, declined, unpaired, opened)):
        raise ValueError('Require positive coverage, all packets matched, no declines or unclosed calls')
    return dict(calls=calls, matched=matched)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    args = parser.parse_args()
    try:
        result = validate(args.log.read_text(encoding='utf-8', errors='replace'))
    except ValueError as e:
        parser.error(str(e))
    print(json.dumps(result))
