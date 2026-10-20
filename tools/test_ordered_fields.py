"""Exercise the actual field wait with delayed consumers, spurious wakeups and exit."""
from pathlib import Path
import subprocess
from test_2d_target_policy import function

def run(root,compiler,directory,env):
    video=(root/'runtime/src/ps2_video.c').read_text()
    code=r'''#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>
#include <errno.h>
typedef uint64_t u64;
static pthread_mutex_t list_lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t list_cv=PTHREAD_COND_INITIALIZER;
static int frame_requested,renderer_running=1,window_closed,payload,consumed;
static atomic_int exiting;
static u64 handoff_wait_count,handoff_timeout_count,handoff_wait_ns,handoff_wait_max;
static int ps2_exit_pending(void){return atomic_load(&exiting);}
static void ps2_request_exit(void){atomic_store(&exiting,1);}
static void ps2_log(const char *fmt,...){(void)fmt;}
static u64 vk_mono_ns(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (u64)t.tv_sec*1000000000ull+t.tv_nsec;}
static void sleep_ms(int ms){struct timespec t={ms/1000,(ms%1000)*1000000L};nanosleep(&t,0);}
'''
    code+=function(video,'static int wait_pending_field(')
    code+=r'''
static void *consumer(void *arg){
 (void)arg;
 for(int expected=1;expected<=12;expected++){
  sleep_ms(expected==1?150:expected==2?65:2);
  pthread_mutex_lock(&list_lock);
  while(!frame_requested)pthread_cond_wait(&list_cv,&list_lock);
  assert(payload==expected);consumed++;frame_requested=0;
  pthread_cond_broadcast(&list_cv);pthread_mutex_unlock(&list_lock);
 }
 return 0;
}
static void *spurious(void *arg){(void)arg;for(int i=0;i<20;i++){sleep_ms(3);pthread_mutex_lock(&list_lock);pthread_cond_broadcast(&list_cv);pthread_mutex_unlock(&list_lock);}return 0;}
static void *cancel(void *arg){sleep_ms(20);if(arg){pthread_mutex_lock(&list_lock);renderer_running=-1;pthread_cond_broadcast(&list_cv);pthread_mutex_unlock(&list_lock);}else ps2_request_exit();return 0;}
int main(void){
 pthread_t reader,waker;pthread_create(&reader,0,consumer,0);pthread_create(&waker,0,spurious,0);
 pthread_mutex_lock(&list_lock);
 for(int n=1;n<=12;n++){
  assert(wait_pending_field(1,0));assert(!frame_requested);
  payload=n;frame_requested=1;pthread_cond_broadcast(&list_cv);
 }
 assert(wait_pending_field(1,0));assert(!frame_requested&&consumed==12);
 pthread_mutex_unlock(&list_lock);pthread_join(reader,0);pthread_join(waker,0);
 assert(handoff_timeout_count>=1&&handoff_wait_max>=100000000ull);
 pthread_mutex_lock(&list_lock);frame_requested=1;payload=77;
 assert(wait_pending_field(0,0)&&frame_requested&&payload==77);
 u64 before=handoff_timeout_count;assert(wait_pending_field(0,1));assert(frame_requested&&payload==77&&handoff_timeout_count==before+1);
 pthread_create(&reader,0,cancel,0);assert(!wait_pending_field(1,0));assert(frame_requested&&payload==77);
 pthread_mutex_unlock(&list_lock);pthread_join(reader,0);atomic_store(&exiting,0);
 pthread_mutex_lock(&list_lock);pthread_create(&reader,0,cancel,(void*)1);assert(!wait_pending_field(1,0));assert(frame_requested&&payload==77);
 pthread_mutex_unlock(&list_lock);pthread_join(reader,0);
 puts("Ordered fields: exact delayed-consumer sequence, >100ms retention, spurious wakeups, opt-in dropping and cancellation passed");
}
'''
    source=Path(directory)/'ordered_fields.c';source.write_text(code)
    exe=Path(directory)/'ordered_fields.exe'
    subprocess.run([str(compiler),'-std=c2x','-O2','-Wall','-Wextra',str(source),'-o',str(exe),'-lpthread'],env=env,check=True)
    subprocess.run([str(exe)],env=env,check=True,timeout=10)
