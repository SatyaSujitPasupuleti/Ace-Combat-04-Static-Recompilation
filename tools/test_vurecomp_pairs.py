"""Execute generated C for asset-free VU pair and E-bit regressions."""
import argparse
from pathlib import Path
import struct
import subprocess
import tempfile

from vurecomp.emit import emit_program


def main():
    ap = argparse.ArgumentParser(); ap.add_argument('--cc', default='gcc')
    args = ap.parse_args()
    nop = 0x2ff
    lower_nop = 0x8000033c
    load = 0x80000000 | 8 << 21 | 1 << 16 | 1 << 11 | 0x37c
    add = 8 << 21 | 1 << 11 | 0x28
    programs = []
    for name, pairs in [
        ('load', [(load, add | 2 << 6 | 1 << 30), (lower_nop, nop)]),
        ('shared', [(load, add | 1 << 6 | 1 << 30), (lower_nop, nop)]),
        ('end', [(0x10020007, nop | 1 << 30), (0x10030009, nop)]),
        ('multi', [(lower_nop, nop | 1 << 30), (lower_nop, nop),
                   (0x1004000b, nop | 1 << 30), (0x1005000d, nop)])
    ]:
        image = b''.join(struct.pack('<II', lo, up) for lo, up in pairs)
        programs.append(emit_program(name, image.ljust(16384, b'\0'), [0, 16] if name == 'multi' else [0], 0, len(image))[0])
    # Only arithmetic/storage helpers used by these tiny programs are needed.
    # Expectations below check externally visible results, not emitted strings.
    support = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint32_t u32; typedef uint16_t u16; typedef uint8_t u8;
typedef union {float f[4]; u32 u[4];} ps2_vf;
typedef ps2_vf vu_f4;
typedef struct {
    ps2_vf vf[32]; u16 vi[16]; u8 *mem; u32 mem_size;
    struct {u32 mac,status; int valid;} fpipe[4];
    u32 mac,status,q_ready,p_ready; float i;
} ps2_vu;
#define PS2_UNLIKELY(x) (x)
static void vu_q_commit(ps2_vu *v) {(void)v;}
static void vu_p_commit(ps2_vu *v) {(void)v;}
static vu_f4 vu_ld(ps2_vf *v) {return *v;}
static vu_f4 vu_add4(vu_f4 a,vu_f4 b) {
    for(int i=0;i<4;i++) a.f[i]+=b.f[i]; return a;
}
static void vu_store(ps2_vu *v,int r,int mask,const float *f) {
    if(r) for(int i=0;i<4;i++) if(mask & (8>>i)) v->vf[r].f[i]=f[i];
}
static void vu_wr(ps2_vu *v,int r,int mask,vu_f4 f) {vu_store(v,r,mask,f.f);}
static void vu_set_flags(ps2_vu *v,int m,vu_f4 f,u32 s) {(void)v;(void)m;(void)f;(void)s;}
'''
    check = r'''
int main(void) {
    u8 mem[256]={0}; ps2_vu v={0}; u32 pc=0,g=0,s=0;
    v.mem=mem;v.mem_size=sizeof mem;v.vi[1]=2;v.vf[1].f[0]=10;
    float loaded=20;memcpy(mem+32,&loaded,4);
    assert(vu1_prog_load(&v,0,10,&pc,&g,&s)==1);
    assert(v.vf[1].f[0]==20 && v.vf[2].f[0]==10 && v.vi[1]==3);
    assert(pc==16 && g==2);
    v.vi[1]=2;v.vf[1].f[0]=10;
    assert(vu1_prog_shared(&v,0,10,&pc,&g,&s)==1);
    assert(v.vf[1].f[0]==10 && v.vi[1]==2);
    assert(vu1_prog_end(&v,0,10,&pc,&g,&s)==1);
    assert(v.vi[2]==7 && v.vi[3]==9 && pc==16 && g==2);
    assert(vu1_prog_multi(&v,0,10,&pc,&g,&s)==1);
    assert(v.vi[4]==0 && v.vi[5]==0);
    assert(vu1_prog_multi(&v,16,10,&pc,&g,&s)==1);
    assert(v.vi[4]==11 && v.vi[5]==13 && pc==32 && g==2);
    ps2_vu before=v;u32 saved_pc=pc,saved_g=g,saved_s=s;
    assert(vu1_prog_multi(&v,8,10,&pc,&g,&s)==0);
    assert(!memcmp(&before,&v,sizeof v) && pc==saved_pc && g==saved_g && s==saved_s);
    puts("PASS: additional entries, non-mutating unknown entry, pre-pair VF reads, shared-destination load suppression, E-bit lower op and delay slot");
    return 0;
}
'''
    with tempfile.TemporaryDirectory(prefix='vu-pairs-') as td:
        source = Path(td)/'pairs.c'; exe=Path(td)/'pairs.exe'
        source.write_text(support+'\n'.join(programs)+check)
        subprocess.run([args.cc,'-std=c11','-O2',str(source),'-o',str(exe)],check=True)
        subprocess.run([str(exe)],check=True)


if __name__ == '__main__': main()
