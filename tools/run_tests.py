"""Run redistributable tests. No ISO, compiler, GPU, or game assets required."""
import argparse
import os
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--suite', choices=['all', 'unit', 'acceptance'], default='all')
    args = ap.parse_args()
    if args.suite != 'unit' and os.name != 'nt':
        ap.error('Acceptance tests require Windows PowerShell and cmd. Use --suite unit on other platforms.')
    suite = unittest.TestSuite()
    if args.suite in ('all', 'unit'):
        suite.addTests(unittest.defaultTestLoader.loadTestsFromName('test_setup_ac4'))
        suite.addTests(unittest.TestLoader().discover(str(ROOT / 'tests/unit')))
    if args.suite in ('all', 'acceptance'):
        suite.addTests(unittest.TestLoader().discover(str(ROOT / 'tests/acceptance')))
    if not suite.countTestCases():
        raise SystemExit('No tests discovered; refusing an empty pass.')
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    return 0 if result.wasSuccessful() else 1


if __name__ == '__main__':
    sys.exit(main())
