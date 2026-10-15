"""Recover the SLUS-20152 pause-menu callbacks, including frameless leaves."""
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from ps2recomp.elf import ElfFile

elf = ElfFile(str(ROOT.parent / "work/viability/SLUS_201.52"))
# Eleven 0x38-byte descriptors. +0x2c is an optional value reader;
# +0x30 is the selected action, called by JALR at 0017E67C.
expected = [
    (0, 0x17D938), (0, 0x17D958), (0, 0x17D9A8), (0, 0x17DA28),
    (0x17DAD8, 0x17DAA8), (0x17DB28, 0x17DAF0),
    (0, 0x17D9A8), (0, 0x17DA28), (0, 0x17DB40),
    (0, 0x17DBC0), (0x17DC48, 0x17DBD8),
]
assert elf.word(0x17E678) == 0x8E020030
assert elf.word(0x17E67C) == 0x0040F809
assert [elf.word(0x17D938 + i*4) for i in range(7)] == [
    0x8F83AAB8, 0x0000202D, 0x0000282D, 0x8C620000,
    0x34420400, 0x080879E6, 0xAC620000]
refs = {}
for i, pair in enumerate(expected):
    base = 0x2B8108 + i * 0x38
    assert (elf.word(base + 0x2C), elf.word(base + 0x30)) == pair
    for offset, target in zip((0x2C, 0x30), pair):
        if not target:
            continue
        assert target % 4 == 0 and any(lo <= target < hi for lo, hi in elf.text_ranges())
        refs.setdefault(target, []).append(base + offset)
if "--apply" in sys.argv:
    p = ROOT / "config/ac4/seeds.json"
    seeds = json.loads(p.read_text())
    seeds["ptr"] = sorted(set(seeds["ptr"]) | refs.keys())
    p.write_text(json.dumps(seeds, indent=2))
    p = ROOT / "config/ac4/symbols.json"
    symbols = json.loads(p.read_text())
    for target, addresses in refs.items():
        symbols.setdefault(f"0x{target:08X}", {
            "name": f"ac4_pause_callback_{target:08X}",
            "confidence": "validated pause-menu descriptor callback",
            "evidence": "ELF descriptor references " + ", ".join(f"{a:08X}" for a in addresses)
        })
    p.write_text(json.dumps(symbols, indent=2))
print(f"Validated {len(refs)} callbacks from {len(expected)} pause-menu descriptors")
