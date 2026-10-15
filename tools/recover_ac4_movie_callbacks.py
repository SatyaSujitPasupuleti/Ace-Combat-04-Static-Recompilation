"""Recover AC4 movie callbacks from the verified NTSC-U executable.

Two callbacks are installed by 0x270910; two eight-entry decoder tables
at 0x3FD848/0x3FD868 feed the indirect calls in 0x272E38.
Run without --apply to validate the source evidence only.
"""
from pathlib import Path
import json, sys
from ps2recomp.elf import ElfFile
r=Path(__file__).resolve().parents[1]
e=ElfFile(str(r.parent/'work/viability/SLUS_201.52'))
expected=[0x272FC8,0x2730D8,0x273260,0x2733C8,0x2735C0,0x273718,0x2738E8,0x273A98,
          0x273040,0x273190,0x273310,0x2734C0,0x273668,0x273800,0x2739C8,0x273BC0]
assert [e.word(0x3FD848+i*4) for i in range(16)]==expected
for pc,word in {0x270970:0x3C070027,0x270978:0x3C030027,0x270980:0x24E72010,0x270988:0x24632020,
                0x272010:0x8C840040,0x272014:0x0809DD62,0x272018:0x2484004C,
                0x272020:0x8C840040,0x272024:0x0809DD9C,0x272028:0x2484004C}.items():
 assert e.word(pc)==word,hex(pc)
found={a:f'ELF movie decoder table entry {0x3FD848+i*4:08X}; indirect dispatch in 00272E38' for i,a in enumerate(expected)}
found.update({0x272010:'Initializer 00270910 materializes callback at 00270970/00270980; observed dispatch from 00270D98',
              0x272020:'Initializer 00270910 materializes callback at 00270978/00270988; decoded tail wrapper to 00277670'})
for a in found: assert any(lo<=a<hi for lo,hi in e.text_ranges()),hex(a)
if '--apply' in sys.argv:
 p=r/'config/ac4/seeds.json';s=json.loads(p.read_text());s['ptr']=sorted(set(s['ptr'])|found.keys());p.write_text(json.dumps(s,indent=2))
 p=r/'config/ac4/symbols.json';s=json.loads(p.read_text())
 for a,evidence in found.items():s[f'0x{a:08X}']={'name':f'ac4_movie_callback_{a:08X}','confidence':'ELF callback registration or explicit function-pointer table','evidence':evidence}
 p.write_text(json.dumps(s,indent=2))
print(f'Validated {len(found)} movie callbacks'+ ('; updated source maps' if '--apply' in sys.argv else ''))
