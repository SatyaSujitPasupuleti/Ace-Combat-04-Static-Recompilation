import sys,json
from pathlib import Path
r=Path(__file__).resolve().parents[1];sys.path.insert(0,str(r/'tools'))
from ps2recomp.elf import ElfFile
from ps2recomp.r5900 import decode
e=ElfFile(str(r.parent/'work/viability/SLUS_201.52'))
found={}
for lo,hi in e.text_ranges():
 for pc in range(lo,hi,4):
  i=decode(e.word(pc),pc)
  if i.name!='JAL' or i.target!=0x266280:continue
  value=None
  for a in range(max(lo,pc-100),pc+8,4):
   j=decode(e.word(a),a)
   if j.name=='LUI' and j.rt==11:value=(j.word&65535)<<16
   elif j.name=='ADDIU' and j.rt==11:value=(value+j.simm)&0xffffffff if j.rs==11 and value is not None else None
   elif j.name in ('JAL','JALR') and a!=pc:value=None
   elif j.name not in ('SD','SW','SQ','SB','SH','BEQ','BNE','NOP','JAL') and (j.rt==11 or j.rd==11):value=None
  if value is not None and any(l<=value<h for l,h in e.text_ranges()) and value%4==0:found.setdefault(value,[]).append(pc)
print({hex(k):[hex(x) for x in v] for k,v in found.items()})
if '--apply' in sys.argv:
 p=r/'config/ac4/seeds.json';s=json.loads(p.read_text());s['ptr']=sorted(set(s['ptr'])|found.keys());p.write_text(json.dumps(s,indent=2))
 p=r/'config/ac4/symbols.json';s=json.loads(p.read_text())
 for a,calls in found.items():s.setdefault(f'0x{a:08X}',dict(name=f'ac4_rpc_completion_{a:08X}',confidence='literal SifCallRpc end_func argument',evidence='a7 address construction before calls '+','.join(f'{x:08X}' for x in calls)))
 p.write_text(json.dumps(s,indent=2))
