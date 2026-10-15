"""Compile the actual interrupt-batch function with a timer-arrival model."""
from pathlib import Path
import os, subprocess, tempfile
r=Path(__file__).resolve().parents[1]
s=(r/'runtime/src/ps2_kernel.c').read_text()
start=s.index('static int run_pending_vblanks(int owner) {')
end=s.index('\nstatic void yield_to_scheduler',start)
prefix='''#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#define TID_INTR -2
static int in_intr,current_tid,kernel_exit_requested;
static unsigned vblank_pending,delivered,arrivals;
typedef unsigned long long u64;
#define CLOCK_TRACE_CAP 65536u
typedef struct { u64 field,start,end,pending; } clock_delivery;
static clock_delivery *clock_deliveries;
static unsigned clock_delivery_n;
static u64 vblank_delivered;
static u64 mono_ns(void) { return 0; }
static pthread_mutex_t ee_lock=PTHREAD_MUTEX_INITIALIZER;
static void deliver_vblank(void) {
    assert(in_intr && current_tid==TID_INTR);
    ++delivered;
    __atomic_fetch_add(&vblank_pending,arrivals,__ATOMIC_RELAXED);
    assert(delivered<20); /* fails promptly for an unbounded busy batch */
}
'''
suffix='''
int main(void) {
    pthread_mutex_lock(&ee_lock);
    current_tid=7; vblank_pending=3; arrivals=1;
    assert(run_pending_vblanks(7)==1);
    assert(delivered==3 && vblank_pending==3);
    assert(current_tid==7 && !in_intr);
    arrivals=0;
    assert(run_pending_vblanks(7)==1);
    assert(delivered==6 && vblank_pending==0);
    assert(!run_pending_vblanks(7));
    vblank_pending=1; in_intr=1;
    assert(!run_pending_vblanks(7)); in_intr=0;
    assert(!run_pending_vblanks(8));
    kernel_exit_requested=1;
    assert(!run_pending_vblanks(7));
    assert(vblank_pending==1);
    pthread_mutex_unlock(&ee_lock);
    puts("PASS: new ticks cannot extend batch; pending ticks preserved; ownership and exit respected");
}
'''
env=os.environ.copy();env['PATH']=str(r.parent/'toolchain/msys64/ucrt64/bin')+';'+env['PATH']
with tempfile.TemporaryDirectory(prefix='ac4-batch-') as tmp:
 p=Path(tmp);(p/'test.c').write_text(prefix+s[start:end]+suffix)
 subprocess.run([str(r.parent/'toolchain/msys64/ucrt64/bin/gcc.exe'),'-O2',str(p/'test.c'),'-o',str(p/'test.exe'),'-lpthread'],env=env,check=True)
 subprocess.run([str(p/'test.exe')],env=env,check=True,timeout=10)
