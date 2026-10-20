"""Byte-exact GS stream row optimization, including clipping, wrap and aliasing."""
from pathlib import Path
import subprocess
from test_2d_target_policy import function

def run(root,compiler,directory,env,benchmark=False):
 gs=(root/'runtime/src/ps2_gs.c').read_text()
 code=r'''#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
typedef uint8_t u8;typedef uint16_t u16;typedef uint32_t u32;typedef uint64_t u64;
#define GS_VRAM_SIZE 1024u
#define GS_SW32 0u
#define GS_SW16 1u
#define GS_SW8 2u
#define GS_SW4 3u
'''
 code+=function(gs,'static u32 gs_swz_bit(')+"\n"
 code+=function(gs,'static void trx_copy_row(').replace('static void trx_copy_row(', 'static __attribute__((noinline,noclone)) void trx_copy_row(')+"\n"
 code+=r'''
static __attribute__((noinline,noclone)) void oracle(u8 *dst,const u8 *src,u64 blockbit,const u16 *off,u32 xb,u32 run,u32 bpp,int disjoint){
 (void)xb;(void)disjoint;
 for(u32 k=0;k<run;k++){
  u32 address=(u32)((blockbit+off[k])>>3);
  if(bpp==4){if(address<=GS_VRAM_SIZE-4)memcpy(dst+address,src+(size_t)k*4,4);}
  else if(address<GS_VRAM_SIZE)dst[address]=src[k];
 }
}
static void offsets(u16 *row,u32 bpp,u32 y,int linear){
 u32 width=bpp==4?8:16;
 for(u32 x=0;x<width;x++)row[x]=(u16)(linear?(y*width+x)*bpp*8:gs_swz_bit(bpp==4?GS_SW32:GS_SW8,x,y));
}
int main(int argc,char **argv){
 (void)argv;u8 a[GS_VRAM_SIZE+32],b[sizeof a],src[128];u16 row[16];u64 cases=0;
 for(u32 i=0;i<sizeof src;i++)src[i]=(u8)(i*71+19);
 const u64 bases[]={0,8,255*8,(GS_VRAM_SIZE-256)*8,(GS_VRAM_SIZE-255)*8,(GS_VRAM_SIZE-4)*8,(GS_VRAM_SIZE+256)*8,1ull<<35};
 for(u32 mode=0;mode<2;mode++)for(int linear=0;linear<2;linear++){
  u32 bpp=mode?1:4,width=mode?16:8,height=mode?16:8;
  for(u32 y=0;y<height;y++){
   offsets(row,bpp,y,linear);
   for(u32 x=0;x<width;x++)for(u32 n=0;n<=width-x;n++)for(u32 base=0;base<8;base++)for(u32 align=0;align<8;align++)for(int fast=0;fast<2;fast++){
    memset(a,0x91,sizeof a);memcpy(b,a,sizeof a);
    oracle(a+16,src+align,bases[base],row+x,x,n,bpp,fast);
    trx_copy_row(b+16,src+align,bases[base],row+x,x,n,bpp,fast);
    assert(!memcmp(a,b,sizeof a));cases++;
   }
   for(u32 x=0;x<width;x++)for(int shift=-8;shift<=8;shift+=4){
    for(u32 i=0;i<sizeof a;i++)a[i]=(u8)(i*13+7);
    memcpy(b,a,sizeof a);
    oracle(a+16,a+16+80+shift,80*8,row+x,x,width-x,bpp,0);
    trx_copy_row(b+16,b+16+80+shift,80*8,row+x,x,width-x,bpp,0);
    assert(!memcmp(a,b,sizeof a));cases++;
   }
  }
 }
 printf("GS row copies: %llu scalar differential cases, all rows/start/counts, unaligned input, edge/wrapped addresses, aliasing and guards passed\n",(unsigned long long)cases);
 if(argc>1){
  const u32 iterations=2000000;volatile u64 digest=0;
  for(u32 mode=0;mode<2;mode++){
   u32 bpp=mode?1:4,width=mode?16:8;offsets(row,bpp,0,0);
   for(int repeat=0;repeat<5;repeat++){
    clock_t t=clock();
    for(u32 i=0;i<iterations;i++)oracle(a+16,src,(i&1u)*256u*8u,row,0,width,bpp,1);
    double old=(double)(clock()-t)/CLOCKS_PER_SEC;
    t=clock();for(u32 i=0;i<iterations;i++)trx_copy_row(b+16,src,(i&1u)*256u*8u,row,0,width,bpp,1);
    double current=(double)(clock()-t)/CLOCKS_PER_SEC;
    digest+=a[16]+b[16];printf("benchmark bpp=%u old=%.6f new=%.6f speedup=%.3f\n",bpp,old,current,old/current);
   }
  }
  printf("digest=%llu\n",(unsigned long long)digest);
 }
}
'''
 source=Path(directory)/'trx_rows.c';source.write_text(code)
 exe=Path(directory)/'trx_rows.exe'
 subprocess.run([str(compiler),'-std=c2x','-O3','-Wall','-Wextra',str(source),'-o',str(exe)],env=env,check=True)
 subprocess.run([str(exe)]+(['--benchmark'] if benchmark else []),env=env,check=True)
