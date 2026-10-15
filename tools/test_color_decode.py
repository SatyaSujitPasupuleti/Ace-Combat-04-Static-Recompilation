"""Compare the production color decoder against its existing scalar conversion."""
from pathlib import Path
import os,subprocess
r=Path(__file__).resolve().parents[1];w=r/'work/color-decode-test';w.mkdir(exist_ok=True)
s=(r/'runtime/src/ps2_gs.c').read_text()
def function(start):
 a=s.index(start);b=s.index('\n}\n',a)+3;return s[a:b]
source=r"""
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
typedef uint8_t u8;typedef uint16_t u16;typedef uint32_t u32;typedef uint64_t u64;
enum {GS_TEXA,PSM_T8=19,PSM_T4=20,PSM_T8H=27,PSM_T4HL=36,PSM_T4HH=44};
static u64 gs_reg[1];static u32 clut_entry(u32 x){return x;}
"""
source+=function('static u32 texa_alpha(')+function('static u32 texel_rgba(')+function('static inline u32 decode_color32(')
source+=r"""
int main(void){
 u32 seed=0x1628394u;
 for(u32 trial=0;trial<100000;trial++){
  u8 block[256];u16 offsets[64];u32 want[64],got[64];
  for(u32 i=0;i<64;i++){
   seed=seed*1664525u+1013904223u;u32 raw=seed;
   if(i%7==0)raw=(trial&255u)<<24; /* zero RGB and every alpha */
   memcpy(block+i*4,&raw,4);offsets[i]=((i*13+trial)%64)*32;
  }
  u32 psm=(trial>>8)&1,run=trial%65,hash=seed,expected=hash;
  gs_reg[0]=(trial&255u)|((u64)((trial>>9)&1u)<<15)|((u64)seed<<32);
  for(u32 k=0;k<run;k++){
   u32 raw;memcpy(&raw,block+(offsets[k]>>3),4);
   want[k]=texel_rgba(raw,psm);expected=(expected^raw)*16777619u;
  }
  assert(decode_color32(block,offsets,got,run,hash,psm,gs_reg[0])==expected);
  assert(!memcmp(want,got,run*4));
 }
 puts("PASS 100000 color decoder cases: pixels, alpha/AEM, reordered offsets, tails, raw hashes");
}
"""
(w/'test.c').write_text(source);bin=r.parent/'toolchain/msys64/ucrt64/bin';env=os.environ.copy();env['PATH']=str(bin)+os.pathsep+env['PATH']
subprocess.run([str(bin/'gcc.exe'),'-O2',str(w/'test.c'),'-o',str(w/'test.exe')],env=env,check=True)
subprocess.run([str(w/'test.exe')],env=env,check=True)
