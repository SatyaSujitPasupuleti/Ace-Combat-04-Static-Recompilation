from pathlib import Path
import sys,json,struct
r=Path(__file__).resolve().parents[1];sys.path.insert(0,str(r/'tools'))
from ps2recomp.elf import ElfFile
from ps2recomp.r5900 import decode
e=ElfFile(str(r.parent/'work/viability/SLUS_201.52'))
def add(a,b):
 if isinstance(a,int) and isinstance(b,int): return (a+b)&0xffffffff
 return ('add',a,b)
def parts(x):
 if isinstance(x,int): return x,[]
 if x[0]=='add':
  a,aa=parts(x[1]);b,bb=parts(x[2]);return (a+b)&0xffffffff,aa+bb
 return 0,[x]
switches=[]
for lo,hi in e.text_ranges():
 for pc in range(lo,hi,4):
  ins=decode(e.word(pc),pc)
  if ins.name!='JR' or ins.rs==31:continue
  regs=[('reg',i) for i in range(32)];regs[0]=0;bounds=[]
  for a in range(max(lo,pc-160),pc,4):
   i=decode(e.word(a),a);n=i.name;rs=regs[i.rs];rt=regs[i.rt]
   if n=='LUI': regs[i.rt]=(i.word&65535)<<16
   elif n in ('ADDIU','DADDIU'):regs[i.rt]=add(rs,i.simm)
   elif n in ('ADDU','DADDU'):regs[i.rd]=add(rs,rt)
   elif n=='ORI' and isinstance(rs,int):regs[i.rt]=rs|(i.word&65535)
   elif n=='SLL':regs[i.rd]=('shift',rt,(i.word>>6)&31)
   elif n=='SLTIU':bounds.append((rs,i.word&65535));regs[i.rt]=('bound',a)
   elif n=='LW':regs[i.rt]=('mem',add(rs,i.simm))
   elif n in ('JAL','JALR'):
    for k in list(range(1,16))+[24,25,31]:regs[k]=('call',a,k)
   elif n in ('NOP','BEQ','BNE','BEQL','BNEL','SB','SH','SW','SD','SQ','SWC1','SDC1'):pass
   else:
    # Refuse inference through an unknown writer to either possible destination.
    if i.rt:regs[i.rt]=('unknown',a)
    if i.rd:regs[i.rd]=('unknown',a)
   regs[0]=0
  target=regs[ins.rs]
  if not isinstance(target,tuple) or target[0]!='mem':continue
  base,terms=parts(target[1])
  if len(terms)!=1 or terms[0][0]!='shift' or terms[0][2]!=2:continue
  index=terms[0][1];counts=[n for v,n in bounds if v==index and 1<n<=256]
  if len(set(counts))!=1:continue
  count=counts[0]
  try: targets=[e.word(base+k*4) for k in range(count)]
  except Exception:continue
  if not all(t%4==0 and any(l<=t<h for l,h in e.text_ranges()) for t in targets):continue
  switches.append(dict(ea=pc,targets=targets,table=base,count=count,evidence='Bounded SLTIU + index*4 + constant table + LW/JR; targets validated against ELF text.'))
# This state is reloaded after calls; earlier bounds on the same memory
# expression are stale, so the conservative window scan refuses inference.
# Preserve the observed, locally guarded six-way post-movie switch explicitly.
if not any(x['ea']==0x2370e4 for x in switches):
 assert e.word(0x2370c8)==0x2c620006 and e.word(0x2370e4)==0x00800008
 targets=[e.word(0x4089f0+k*4) for k in range(6)]
 assert targets==[0x2370ec,0x237100,0x23713c,0x237158,0x237174,0x237268]
 switches.append(dict(ea=0x2370e4,targets=targets,table=0x4089f0,count=6,
  evidence='Observed after movie Start. SLTIU v0,v1,6 at 002370C8 and BEQ default 00237284 precede index*4 and table 004089F0; prior calls invalidate earlier bounds on the same state field.'))
# Flight-effect dispatch stores the 16-bit kind between its bounds check
# and table load. SH is not a GPR writer and must preserve that bound.
flight=next(x for x in switches if x['ea']==0x1FC754)
assert flight['table']==0x4058F0 and flight['count']==5
assert flight['targets']==[e.word(0x4058F0+k*4) for k in range(5)]
# This flight dispatch hoists s0*4 into t3 well before the local guard.
# The 160-byte scanner window cannot see the shift; validate the observed
# chain and the complete table explicitly rather than adding a case as a function.
assert e.word(0x194248)==0x00105880  # SLL t3,s0,2
assert e.word(0x194678)==0x2E02000A  # SLTIU v0,s0,10
assert e.word(0x19467C)==0x10400179  # BEQ v0,zero,default
assert [e.word(a) for a in range(0x194680,0x194698,4)]==[
 0x3C020040,0x24425540,0x01621021,0x8C430000,0x00600008,0]
flight_targets=[0x194698]*2+[0x1947EC]*3+[0x194980]*3+[0x194AD4]*2
assert [e.word(0x405540+k*4) for k in range(10)]==flight_targets
if not any(x['ea']==0x194690 for x in switches):
 switches.append(dict(ea=0x194690,targets=flight_targets,table=0x405540,count=10,
  evidence='Observed in extended Mission 01 flight. SLL t3,s0,2 at 00194248; local SLTIU s0,10 and default branch guard the table load from 00405540 at 00194680..00194690.'))
# Attract-mode effect switch: s1 holds the table's high half outside the
# conservative scan window. Validate the bounded 13-way local dispatch.
assert e.word(0x14F5F8)==0x3C110040
assert [e.word(a) for a in range(0x14F6AC,0x14F6CC,4)]==[
 0x2C43000D,0x10600038,0x00021080,0x26234760,
 0x00431021,0x8C440000,0x00800008,0]
effect_targets=[0x14F6CC,0x14F6E4,0x14F6D4,0x14F704,0x14F6F4,
 0x14F714,0x14F724,0x14F734,0x14F744,0x14F754,0x14F764,0x14F774,0x14F784]
assert [e.word(0x404760+k*4) for k in range(13)]==effect_targets
if not any(x['ea']==0x14F6C4 for x in switches):
 switches.append(dict(ea=0x14F6C4,targets=effect_targets,table=0x404760,count=13,
  evidence='Observed attract-mode target 0014F6CC. LUI s1,0x40 at 0014F5F8; SLTIU index,13 and default branch precede table 00404760.'))
switches.sort(key=lambda x:x['ea'])
db=dict(functions=[],names={},switches=switches)
(r/'config/ac4/control_flow.json').write_text(json.dumps(db,indent=2))
print('Recovered',len(switches),'bounded jump tables')
assert any(x['ea']==0x214a8c for x in switches),'Observed switch not recovered'
