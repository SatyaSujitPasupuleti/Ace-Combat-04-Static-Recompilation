"""VIF0 DMA completion must be visible to EE while the VIF1 worker is blocked."""
from pathlib import Path
import os, subprocess
root=Path(__file__).resolve().parents[1]
out=root/'work/gfxq_vif0_test';out.mkdir(parents=True,exist_ok=True)
bin=root.parent/'toolchain/msys64/ucrt64/bin'
env=os.environ.copy();env['PATH']=str(bin)+os.pathsep+env['PATH']
env['PS2_ASYNC_GFX']='1';env.pop('PS2_SPEED_LOG',None)
source=out/'test.c'
source.write_text(r'''#include "ps2_gfxq.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdarg.h>
u64 ps2_phase_cycles[PS2_PH_N],ps2_phase_self[PS2_PH_N],ps2_phase_calls[PS2_PH_N];
int ps2_phase_on;
_Thread_local u64 ps2_phase_child;
static pthread_t ee;
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed=PTHREAD_COND_INITIALIZER;
static int entered,released;
static u32 data,word,reset,clear;
void ps2_log(const char *fmt,...) {(void)fmt;}
void ps2_host_prof_attach(const char *name) {(void)name;}
void ps2_vu1_speed_counters(u64 *a,u64 *b,u64 *c){*a=*b=*c=0;}
void ps2_scene_speed_report(double dt){(void)dt;}
void ps2_request_exit(void){abort();}
void ps2_gif_transfer(const ps2_reg128 *q,u32 n){(void)q;(void)n;abort();}
void ps2_gs_priv_write_now(u32 a,u64 v){(void)a;(void)v;abort();}
void ps2_gif_ctrl_write(u32 a,u32 v){(void)a;(void)v;abort();}
int ps2_gs_present_now(void){abort();}
void rn_gs_tag(u32 a,u32 b,u32 c){(void)a;(void)b;(void)c;abort();}
void rn_gs_intent(const u8 *p,u32 n){(void)p;(void)n;abort();}
void ps2_vif_transfer(int ch,const ps2_reg128 *q,u32 n){
    if(!ch){assert(pthread_equal(ee,pthread_self()));assert(n==1);data=q->uw[0];return;}
    assert(ch==1 && !pthread_equal(ee,pthread_self()));
    pthread_mutex_lock(&lock);entered=1;pthread_cond_signal(&changed);
    while(!released)pthread_cond_wait(&changed,&lock);
    pthread_mutex_unlock(&lock);
    assert(q->uw[0]==0x12345678); /* queued payload was copied */
}
void ps2_vif_write(int ch,u32 v){assert(!(ch&1)&&pthread_equal(ee,pthread_self()));word=v;assert(ch==PS2_VIF_WORD_AT(0,3));}
void ps2_vif_fbrst(int ch,u32 v){assert(!ch&&pthread_equal(ee,pthread_self()));reset=v;}
void ps2_vif_clear_stall(int ch){assert(!ch&&pthread_equal(ee,pthread_self()));clear++;}
int main(void){
    ee=pthread_self();ps2_gfxq_init();assert(ps2_gfxq_on);
    ps2_reg128 q={0};q.uw[0]=0x12345678;ps2_gfxq_vif_qw(1,&q,1);
    pthread_mutex_lock(&lock);while(!entered)pthread_cond_wait(&changed,&lock);pthread_mutex_unlock(&lock);
    q.uw[0]=0x87654321;ps2_gfxq_vif_qw(0,&q,1);
    ps2_gfxq_vif_word(PS2_VIF_WORD_AT(0,3),0x690f0000);
    ps2_gfxq_fbrst(0,1);ps2_gfxq_vif_clear(0);
    assert(data==0x87654321 && word==0x690f0000 && reset==1 && clear==1);
    pthread_mutex_lock(&lock);released=1;pthread_cond_signal(&changed);pthread_mutex_unlock(&lock);
    ps2_gfxq_drain();ps2_gfxq_shutdown();puts("VIF0 EE ownership / asynchronous VIF1: PASS");return 0;
}
''')
exe=out/'test.exe'
subprocess.run([str(bin/'gcc.exe'),'-std=gnu2x','-O2','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-I'+str(root/'runtime/include'),str(source),str(root/'runtime/src/ps2_gfxq.c'),'-lpthread','-o',str(exe)],env=env,check=True)
subprocess.run([str(exe)],env=env,check=True,timeout=10)
