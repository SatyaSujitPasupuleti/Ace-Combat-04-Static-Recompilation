from pathlib import Path
import os,subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'runtime/src/ps2_gs.c').read_text()
start=s.index('static inline u32 decode_t4_bytes(');end=s.index('\nstatic u32 current_texture_inner',start)
prefix='''#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
typedef uint8_t u8; typedef uint32_t u32;
'''
suffix='''
static u32 seed=1;
static u32 rnd(void){seed=seed*1664525u+1013904223u;return seed;}
int main(void) {
    u8 block[256],offsets[32];u32 pal[16],expanded[256],out[34],want[34];
    for(unsigned trial=0;trial<1000;trial++) {
        for(unsigned i=0;i<256;i++)block[i]=(u8)(rnd()>>16);
        for(unsigned i=0;i<32;i++)offsets[i]=(u8)(rnd()>>16);
        for(unsigned i=0;i<16;i++)pal[i]=rnd();
        for(unsigned shift=0;shift<=4;shift+=4) {
            for(unsigned i=0;i<256;i++)expanded[i]=pal[(i>>shift)&15];
            for(unsigned run=1;run<=32;run++) {
                memset(out,0xA5,sizeof out);memset(want,0xA5,sizeof want);
                u32 hash=rnd(),expected=hash,k=0;
                for(;k+4<=run;k+=4) {
                    u32 indices=0;
                    for(unsigned j=0;j<4;j++) {
                        u32 raw=(block[offsets[k+j]]>>shift)&15;
                        want[k+j]=pal[raw];indices|=raw<<(j*8);
                    }
                    expected=(((expected<<13)|(expected>>19))^indices)*0x9E3779B1u;
                }
                for(;k<run;k++) {
                    u32 raw=(block[offsets[k]]>>shift)&15;
                    want[k]=pal[raw];expected=(expected^raw)*16777619u;
                }
                assert(decode_t4_bytes(block,offsets,shift,expanded,out,run,hash)==expected);
                assert(!memcmp(out,want,sizeof out));
            }
        }
    }
    puts("PASS: 64000 T4 runs preserve pixels, hash and destination bounds");
}
'''
env=os.environ.copy();tc=r.parent/'toolchain/msys64/ucrt64/bin';env['PATH']=str(tc)+';'+env['PATH']
with tempfile.TemporaryDirectory(prefix='ac4-t4-') as tmp:
 p=Path(tmp);(p/'test.c').write_text(prefix+s[start:end]+suffix)
 subprocess.run([str(tc/'gcc.exe'),'-O3',str(p/'test.c'),'-o',str(p/'test.exe')],env=env,check=True)
 subprocess.run([str(p/'test.exe')],env=env,check=True,timeout=15)
