"""Real async queue: diagnostic field records, timestamps, wrapping and overflow."""
from pathlib import Path
import csv,subprocess

def run(root,compiler,directory,env):
 source=Path(directory)/'gfx_timeline.c'
 prefix='#include <pthread.h>\nstatic int tracked_broadcast(pthread_cond_t *);\n#define pthread_cond_broadcast tracked_broadcast\n#include "'+str(root/'runtime/src/ps2_gfxq.c').replace('\\','/')+'"\n#undef pthread_cond_broadcast\n'
 source.write_text(prefix+r''' #include "ps2_gfxq.h"
#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
u64 ps2_phase_cycles[PS2_PH_N],ps2_phase_self[PS2_PH_N],ps2_phase_calls[PS2_PH_N];
int ps2_phase_on;_Thread_local u64 ps2_phase_child;
static u32 transfers,presents,word_writes,field_signals,ring_signals;static int first_queued,ring_mode;
static int tracked_broadcast(pthread_cond_t *c){
 if(c==&q_field_ready)field_signals++;
 if(c==&q_not_full)ring_signals++;
 return pthread_cond_broadcast(c);
}
void ps2_log(const char *fmt,...){va_list a;va_start(a,fmt);vprintf(fmt,a);va_end(a);putchar('\n');}
void ps2_host_prof_attach(const char *name){(void)name;}
void ps2_vu1_speed_counters(u64 *a,u64 *b,u64 *c){*a=*b=*c=0;}
void ps2_scene_speed_report(double dt){(void)dt;}
void ps2_request_exit(void){abort();}
void ps2_gif_transfer(const ps2_reg128 *q,u32 n){(void)q;(void)n;abort();}
void ps2_gs_priv_write_now(u32 a,u64 v){(void)a;(void)v;abort();}
void ps2_gif_ctrl_write(u32 a,u32 v){(void)a;(void)v;abort();}
void rn_gs_tag(u32 a,u32 b,u32 c){(void)a;(void)b;(void)c;abort();}
void rn_gs_intent(const u8 *p,u32 n){(void)p;(void)n;abort();}
void ps2_vif_write(int ch,u32 v){assert(ch==1&&v==0xabcdef01u);word_writes++;}
void ps2_vif_fbrst(int ch,u32 v){(void)ch;(void)v;abort();}
void ps2_vif_clear_stall(int ch){(void)ch;abort();}
void ps2_vif_transfer(int ch,const ps2_reg128 *q,u32 n){
 assert(ch==1&&n==(transfers<70?(ring_mode?32768:1024):1)&&q->uw[0]==transfers);
 if(!transfers){while(!__atomic_load_n(&first_queued,__ATOMIC_ACQUIRE))Sleep(0);Sleep(30);}
 if(ring_mode)Sleep(2);transfers++;
}
int ps2_gs_present_now(void){assert(transfers==presents+1);presents++;return 1;}
int main(int argc,char **argv){
 assert(argc==3);ring_mode=getenv("PS2_TEST_RING")!=NULL;u32 fields=(u32)atoi(argv[2]);ps2_gfxq_init();assert(ps2_gfxq_on);
 static ps2_reg128 q[32768];
 for(u32 i=0;i<fields;i++){if(i<70)for(int w=0;w<600;w++)ps2_gfxq_vif_word(1,0xabcdef01u);q[0].uw[0]=i;ps2_gfxq_vif_qw(1,q,i<70?(ring_mode?32768:1024):1);q[0].uw[0]=0xffffffffu;assert(ps2_gfxq_field());if(!i)__atomic_store_n(&first_queued,1,__ATOMIC_RELEASE);}
 ps2_gfxq_drain();assert(transfers==fields&&presents==fields);assert(field_signals==(q_split_field_wait?fields:0)&&ring_signals>fields&&word_writes==(fields<70?fields:70)*600);if(ring_mode)assert(st_producer_waits>0);ps2_gfxq_report();
 FILE *before=fopen(argv[1],"rb");assert(!before); /* reporting must not write during gameplay */
 ps2_gfxq_shutdown();ps2_gfxq_report();return 0;
}
''')
 exe=Path(directory)/'gfx_timeline.exe'
 subprocess.run([str(compiler),'-std=gnu2x','-O2','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-I'+str(root/'runtime/include'),str(source),'-lpthread','-o',str(exe)],env=env,check=True)
 for name,fields,enabled in [('off',70,False),('on',70,True),('ring',70,True),('legacy',70,True),('overflow',33000,True)]:
  path=Path(directory)/(name+'.csv');e=dict(env,PS2_ASYNC_GFX='1',PS2_ASYNC_MB='1');e.pop('PS2_SPEED_LOG',None);e.pop('PS2_GFX_TIMELINE',None)
  if name=='legacy':e['PS2_GFX_FIELD_WAKE_LEGACY']='1'
  if name=='ring':e.update(PS2_TEST_RING='1',PS2_ASYNC_FIELDS='8')
  if enabled:e.update(PS2_GFX_TIMELINE=str(path),PS2_GFX_JOB_TIMING='1')
  result=subprocess.run([str(exe),str(path),str(fields)],env=e,check=True,capture_output=True,text=True,timeout=30)
  if not enabled:assert not path.exists();continue
  rows=[{k:int(v) for k,v in r.items()} for r in csv.DictReader(path.open())]
  assert len(rows)==min(fields,32768)
  jobs=list(csv.DictReader(Path(str(path)+'.jobs.csv').open()))
  vif=[r for r in jobs if int(r['opcode'])==1]
  present=[r for r in jobs if int(r['opcode'])==11]
  assert len(vif)==len(rows) and len(present)==len(rows)
  assert all(int(r['count'])==(600 if int(r['opcode'])==3 else 1) and int(r['total_ns'])>=int(r['max_ns']) for r in jobs)
  assert int(vif[0]['max_ns'])>=20000000 and int(vif[0]['max_bytes'])==(524294 if name=='ring' else 16390)
  for i,r in enumerate(rows):
   assert r['field']==i+1 and r['producer_ready_ns']<=r['queued_ns']<=r['worker_ready_ns'] and r['queue_bytes']>=33 and r['producer_tid']>0
   if i:
    assert r['producer_tid']==rows[i-1]['producer_tid']
    for key in ['producer_ready_ns','producer_cpu_ns','queued_ns','worker_ready_ns','worker_cpu_ns','worker_idle_ns','worker_cycles','worker_lock_ns','worker_apply_ns','worker_notify_ns']:assert r[key]>=rows[i-1][key]
  assert rows[0]['worker_ready_ns']-rows[0]['queued_ns']>=20000000
  assert rows[0]['worker_apply_ns']>=20000000 and rows[-1]['worker_cycles']>0
  if fields>32768:assert '232 overflow rows' in result.stdout
 print('GFX timeline: normal/traced field order, copied payloads, backpressure, ring wrapping, clock/CPU ordering, delayed worker, deferred IO and bounded overflow passed')
