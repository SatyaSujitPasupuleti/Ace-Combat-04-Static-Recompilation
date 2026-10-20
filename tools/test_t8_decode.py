"""Check actual GS byte-offset decoder against the original bit-offset loop."""
from pathlib import Path
import subprocess
from test_2d_target_policy import function

def run(root, compiler, directory, env):
    gs=(root/'runtime/src/ps2_gs.c').read_text()
    prefix=r'''#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint8_t u8;typedef uint16_t u16;typedef uint32_t u32;
#define GS_SW32 0u
#define GS_SW16 1u
#define GS_SW8 2u
#define GS_SW4 3u
#define ps2_log(...) ((void)0)
static u16 gs_swz[4][512];static int gs_swz_ready;
static u8 gs_swz4_bytes[512],gs_swz4_shift[16],gs_swz8_bytes[256];
static int gs_swz4_uniform;
'''
    code=prefix+function(gs,'static u32 gs_swz_bit(')+function(gs,'static void gs_swz_init(')+function(gs,'static inline u32 decode_t8_bytes(')
    code+=r'''
static u32 seed=7;
static u32 rnd(void){seed=seed*1664525u+1013904223u;return seed;}
int main(int argc,char **argv){
    (void)argv;gs_swz_init();assert(gs_swz_ready);
    for(u32 i=0;i<256;i++)assert(gs_swz8_bytes[i]==(gs_swz[GS_SW8][i]>>3));
    u8 block[257];u32 pal[256],out[18],want[18];u32 cases=0;
    for(u32 trial=0;trial<128;trial++){
        for(u32 i=0;i<257;i++)block[i]=(u8)(rnd()>>16);
        for(u32 i=0;i<256;i++)pal[i]=rnd();
        for(u32 y=0;y<16;y++)for(u32 x=0;x<16;x++)for(u32 n=0;n<=16-x;n++){
            memset(out,0xA5,sizeof out);memcpy(want,out,sizeof out);
            u32 hash=rnd(),expected=hash,k=0;const u8 *src=block+(trial&1);
            const u16 *offsets=gs_swz[GS_SW8]+y*16+x;
            for(;k+4<=n;k+=4){u32 packed=0;
                for(u32 j=0;j<4;j++){u32 raw=src[offsets[k+j]>>3];want[k+j+1]=pal[raw];packed|=raw<<(j*8);}
                expected=(((expected<<13)|(expected>>19))^packed)*0x9E3779B1u;
            }
            for(;k<n;k++){u32 raw=src[offsets[k]>>3];want[k+1]=pal[raw];expected=(expected^raw)*16777619u;}
            assert(decode_t8_bytes(src,gs_swz8_bytes+y*16+x,pal,out+1,n,hash)==expected);
            assert(!memcmp(out,want,sizeof out));cases++;
        }
    }
    printf("T8 decode: %u pixel/hash/guard cases passed (layout %s)\n",cases,argc>1?"linear":"swizzled");
}
'''
    source=Path(directory)/'t8_decode.c';source.write_text(code)
    exe=Path(directory)/'t8_decode.exe'
    subprocess.run([str(compiler),'-std=c2x','-O3','-Wall','-Wextra',str(source),'-o',str(exe)],env=env,check=True)
    clean=dict(env,PS2_GS_LINEAR_BLOCK='0');subprocess.run([str(exe)],env=clean,check=True)
    subprocess.run([str(exe),'linear'],env=dict(env,PS2_GS_LINEAR_BLOCK='1'),check=True)
