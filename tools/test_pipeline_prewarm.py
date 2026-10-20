"""Check shipped pipeline coverage and actual budgeted startup preparation."""
from pathlib import Path
import subprocess
from test_2d_target_policy import function

def run(root,compiler,directory,env):
    video=(root/'runtime/src/ps2_video.c').read_text()
    code=r'''#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
typedef uint32_t u32;typedef uint64_t u64;
#define ps2_log(...) ((void)0)
'''+(root/'runtime/include/ac4_pipeline_states.h').read_text()+r'''
static u64 *pipe_warm,stat_pipe_warm,clock_value;
static u32 pipe_warm_n,pipe_warm_i,seen_n;
static u64 seen[512];
static u64 vk_mono_ns(void){return ++clock_value;}
static u64 get_pipeline_ex(u32 topology,u32 blend,u32 depth,u32 mesh,u32 cull){
    assert(seen_n<512);u64 k=((u64)topology<<40)|((u64)blend<<16)|depth
        |((u64)(mesh&3u)<<48)|((u64)(cull&1u)<<50);
    for(u32 i=0;i<seen_n;i++)assert(seen[i]!=k);
    seen[seen_n++]=k;return k;
}
'''+function(video,'static void pipe_warm_step(')+r'''
int main(void){
    const u32 n=sizeof(ac4_pipeline_states)/sizeof(ac4_pipeline_states[0]);
    assert(n>0 && n<=512);pipe_warm=malloc(sizeof(ac4_pipeline_states));assert(pipe_warm);
    memcpy(pipe_warm,ac4_pipeline_states,sizeof(ac4_pipeline_states));pipe_warm_n=n;
    pipe_warm_step(1);assert(pipe_warm && pipe_warm_i==1 && seen_n==1);
    while(pipe_warm)pipe_warm_step(3);
    assert(!pipe_warm_n && !pipe_warm_i && stat_pipe_warm==n && seen_n==n);
    int mission2=0;
    for(u32 i=0;i<n;i++){assert(seen[i]==ac4_pipeline_states[i]);if(seen[i]==0x000302000064011Full)mission2=1;}
    assert(mission2);pipe_warm_step(1);assert(seen_n==n);
    puts("Pipeline preparation: unique shipped coverage, Mission2 transition, key roundtrip, budget continuation, cleanup and empty re-entry passed");
}
'''
    source=Path(directory)/'pipeline_prewarm.c';source.write_text(code)
    exe=Path(directory)/'pipeline_prewarm.exe'
    subprocess.run([str(compiler),'-std=c2x','-O3','-Wall','-Wextra',str(source),'-o',str(exe)],env=env,check=True)
    subprocess.run([str(exe)],env=env,check=True)
