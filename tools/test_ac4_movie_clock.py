"""Verify movie cadence, reset, bypass and bounded clock quantization."""
from pathlib import Path
import os,subprocess
r=Path(__file__).resolve().parents[1];w=r/'work/movie-clock-tests';w.mkdir(exist_ok=True)
prefix=r.parent/'toolchain/msys64/ucrt64';env=os.environ.copy();env['PATH']=str(prefix/'bin')+os.pathsep+env['PATH']
src=w/'clock.c';src.write_text(r'''
#include "ps2_runtime.h"
#include <SDL3/SDL.h>
#include <assert.h>
#include <stdio.h>
u8 *ps2_pt[PS2_PT_ENTRIES];
static u8 page[65536];
u8 *ps2_ram=page;
static u64 field;static unsigned waits;
u64 ps2_kernel_vblank_count(void) { return field; }
void ps2_preempt(void) { ++field; ++waits; }
void SDL_Delay(Uint32 ms) { (void)ms; }
u32 ps2_mmio_r32(u32 a) { abort(); }
u8 ps2_mmio_r8(u32 a) { abort(); }
void ps2_log(const char *f,...) { (void)f; }
void ac4_movie_clock_reset(ps2_ctx *);
void ac4_movie_clock_wait(ps2_ctx *);
void ac4_movie_audio_capture(u32,u32);
void ac4_movie_audio_start(u32,u32);
int ac4_movie_audio_sample(s16 [2]);
static void word(u32 a,u32 v) { memcpy(page+a,&v,4); }
static void state(u32 s) { memcpy(page+0x190,&s,4); }
int main(void) {
 ps2_ctx ctx={0};ctx.r[4].uw[0]=0x100;for(u32 i=0;i<16;++i)ps2_pt[i]=page+i*PS2_PAGE_SIZE;field=100;state(0);
 u32 chunk=0x300,header=0xB3010000u;memcpy(page+0x108,&chunk,4);memcpy(page+0x310,&header,4);page[0x317]=5;
 ac4_movie_clock_reset(&ctx);ac4_movie_clock_wait(&ctx);assert(field==100 && !waits);
 state(3);
 for(u64 n=0;n<4170;++n) {
  ac4_movie_clock_wait(&ctx);
  u64 target=100+(n*2000+1000)/1001;
  assert(field==target);
  assert((field-100)*1001>=n*2000 && (field-100)*1001-n*2000<1001);
 }
 state(5);u64 last=field;ac4_movie_clock_wait(&ctx);assert(field==last);
 ac4_movie_clock_reset(&ctx);state(4);ac4_movie_clock_wait(&ctx);assert(field==last);
 field+=10;ac4_movie_clock_wait(&ctx);assert(field==last+10);
 page[0x317]=4;ac4_movie_clock_reset(&ctx);last=field;
 for(u64 n=0;n<3000;++n) { ac4_movie_clock_wait(&ctx);assert(field==last+2*n); }
 _putenv_s("PS2_AC4_MOVIE_UNPACED","1");last=field;ac4_movie_clock_wait(&ctx);assert(field==last);
 _putenv_s("PS2_AC4_MOVIE_UNPACED","");ac4_movie_clock_reset(&ctx);
 word(0x100,0x400);word(0x104,0x8400);
 for(u32 j=0;j<2;++j) {
  u32 base=j?0x8400:0x400;word(base+16,0xB3010000u);word(base+8,0x80);word(base+12,0x4000);
  for(u32 i=0;i<4096;++i) {
   u32 off=base+0x80+(i/256)*1024+(i%256)*2;s16 l=100+j,r=-l;
   memcpy(page+off,&l,2);memcpy(page+off+512,&r,2);
  }
  ac4_movie_audio_capture(base,0x4800);memset(page+base+0x80,0,0x4000);
 }
 ac4_movie_audio_start(0x40180,32768);
 for(u32 i=0;i<8192;++i) { s16 v[2];assert(ac4_movie_audio_sample(v));assert(v[0]==100+i/4096 && v[1]==-v[0]); }
 s16 v[2]={9,9};assert(ac4_movie_audio_sample(v) && !v[0] && !v[1]);
 ac4_movie_audio_start(0,0);assert(!ac4_movie_audio_sample(v));
 ac4_movie_clock_reset(&ctx);ac4_movie_audio_start(0x40180,32768);assert(!ac4_movie_audio_sample(v));
 puts("PASS: owned PCM survives source overwrite; chunk order, drain, stop and reset");
 puts("PASS: 4170 pictures at30Hz and3000 at29.97Hz; drift < one field; inactive/reset/late/bypass paths");
}
''')
exe=w/'clock.exe'
subprocess.run([str(prefix/'bin/gcc.exe'),'-std=c23','-O2','-I'+str(r/'runtime/include'),str(src),str(r/'runtime/src/ps2_ac4_movie.c'),'-o',str(exe)],env=env,check=True)
subprocess.run([str(exe)],env=env,check=True)
