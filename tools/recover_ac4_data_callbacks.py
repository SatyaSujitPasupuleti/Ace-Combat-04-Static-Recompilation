from pathlib import Path
import sys,json
r=Path(__file__).resolve().parents[1];sys.path.insert(0,str(r/'tools'))
from ps2recomp.elf import ElfFile
from ps2recomp.r5900 import decode
e=ElfFile(str(r.parent/'work/viability/SLUS_201.52'));found={}
for sec in e.sections:
 if not sec.is_alloc or not sec.data or sec.is_exec or sec.name=='.vutext':continue
 for off in range(0,len(sec.data)-3,4):
  v=int.from_bytes(sec.data[off:off+4],'little')
  if v%4 or not any(lo<=v<hi-16 for lo,hi in e.text_ranges()):continue
  ins=[decode(e.word(v+k*4),v+k*4) for k in range(4)]
  # Only recover a short scalar setup followed by an explicit stack frame.
  if ins[0].name not in ('LW','LUI','ADDIU','DADDU','ADDU','OR'):continue
  for k,i in enumerate(ins):
   if i.name=='ADDIU' and i.rs==29 and i.rt==29 and i.simm<0 and i.simm%16==0:
    if k:found.setdefault(v,[]).append(sec.addr+off)
    break
   if i.name not in ('LW','LUI','DADDU','ADDU','OR','ADDIU'):break
print('Recovered delayed-prologue data callbacks:',len(found))
assert 0x2261b8 in found
if '--apply' in sys.argv:
 p=r/'config/ac4/seeds.json';s=json.loads(p.read_text());s['ptr']=sorted(set(s['ptr'])|found.keys());p.write_text(json.dumps(s,indent=2))
 p=r/'config/ac4/symbols.json';s=json.loads(p.read_text())
 for addr,refs in found.items():s.setdefault(f'0x{addr:08X}',dict(name=f'ac4_data_callback_{addr:08X}',confidence='allocated-data pointer and delayed stack-frame prologue',evidence='ELF data references '+','.join(f'{a:08X}' for a in refs)))
 p.write_text(json.dumps(s,indent=2))
