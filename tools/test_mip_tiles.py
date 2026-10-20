"""Compare actual mip-atlas row copies with the original per-pixel mapping."""
from pathlib import Path
import subprocess


def run(root, compiler, directory, env):
    helper=(root/'runtime/include/ps2_gs_tile_copy.h').read_text().replace('#include "ps2_runtime.h"','')
    code=r'''#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t u8;typedef uint32_t u32;
'''+helper+r'''
static u32 seed=9127;
static u32 random_u32(void){seed=seed*1664525u+1013904223u;return seed;}
int main(void){
 u8 src[96*96*4],actual[128*128*4+128],expected[sizeof actual];
 for(u32 test=0;test<20000;test++){
  u32 sw=1+random_u32()%96,sh=1+random_u32()%96;
  u32 w=random_u32()%120,h=random_u32()%120;
  size_t stride=w*4+random_u32()%17,offset=random_u32()%16;
  for(size_t i=0;i<sizeof src;i++)src[i]=(u8)(random_u32()>>24);
  memset(actual,0xa5,sizeof actual);memset(expected,0xa5,sizeof expected);
  for(u32 y=0;y<h;y++)for(u32 x=0;x<w;x++)
   memcpy(expected+offset+y*stride+x*4,src+((y%sh)*sw+x%sw)*4,4);
  ps2_copy_tiled_rgba(actual+offset,stride,src,sw,sh,w,h);
  assert(!memcmp(actual,expected,sizeof actual));
 }
 puts("Mip atlas copies: 20000 wrapped/cropped/strided/unaligned cases and padding guards passed");
}
'''
    source=Path(directory)/'mip_tiles.c';exe=Path(directory)/'mip_tiles.exe';source.write_text(code)
    subprocess.run([str(compiler),'-std=c11','-O2','-Wall','-Wextra',str(source),'-o',str(exe)],env=env,check=True)
    subprocess.run([str(exe)],env=env,check=True)
