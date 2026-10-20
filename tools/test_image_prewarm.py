"""Test full-output image reservation budgets and safe reuse/failure fallback."""
from pathlib import Path
import subprocess
from test_2d_target_policy import function

def run(root,compiler,directory,env):
    video=(root/'runtime/src/ps2_video.c').read_text()
    code=r'''#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
typedef uint32_t u32;typedef uint64_t u64;
typedef int VkFormat;typedef int VkImageUsageFlags;typedef int VkImageAspectFlags;
typedef int VkImage;typedef int VkDeviceMemory;typedef int VkImageView;
#define VK_NULL_HANDLE 0
static struct {u32 w,h;int fmt,usage,aspect,image,memory,view;} warm_images[4];
static int PS2_GAME_AC4=1,full=1;
static struct {u32 width,height;} swap_extent={3840,2160};
typedef uint64_t Uint64;
static struct {int internal_res;} ps2_cfg;
static struct {int created;} rts[1];
static u32 auto_scale_target=6,rt_scale=3,rt_w=640,rt_h=896,max_image_dim=8192,scale_failed;
static u64 stat_frames;
#define RT_SCALE_MAX 8u
#define ps2_log(...) ((void)0)
static Uint64 SDL_GetTicks(void){return 1000;}
static int full_output_resolution(void){return full;}
static u32 window_cover_scale(u32 w,u32 h,u32 pw,u32 ph){u32 x=(w+pw-1)/pw,y=(h+ph-1)/ph;return x>y?x:y;}
static u32 warm_image_n,warm_w=640,warm_h=896,warm_scale=8;
static u32 warm_window_w=3840,warm_window_h=2160;
static u64 warm_image_hits;static int warm_multiscale=1,direct_calls,failures,releases;
static void warm_images_release(void){releases++;warm_image_n=0;}
static int make_image_direct(u32 w,u32 h,int fmt,int usage,int aspect,int *image,int *memory,int *view){
 (void)w;(void)h;(void)fmt;(void)usage;(void)aspect;direct_calls++;
 if(failures){failures--;return -1;}*image=91;*memory=92;*view=93;return 0;
}
'''
    for name in ('static void warm_image_budget(', 'static int warm_layout_matches(', 'static int make_image(', 'static u32 ac4_stable_auto_scale(', 'static u32 choose_scale(', 'static int warm_loading_plan('):code+=function(video,name)
    code+=r'''
int main(void){
 u64 costs[5]={147374080,149553168,146800640,149553168,147374080};
 assert(warm_loading_plan(20ull<<30,costs,15,8));
 assert(!warm_loading_plan(8ull<<30,costs,15,8));
 assert(!warm_loading_plan(0,costs,15,8));
 assert(!warm_loading_plan(1ull<<30,costs,15,8));
 assert(!warm_loading_plan(20ull<<30,costs,28,8));
 costs[0]=0;assert(!warm_loading_plan(20ull<<30,costs,15,8));
 costs[0]=~(u64)0;assert(!warm_loading_plan(~(u64)0,costs,15,8));

 assert(choose_scale(640,896)==8);
 ps2_cfg.internal_res=2;assert(choose_scale(640,896)==2);ps2_cfg.internal_res=0;
 max_image_dim=4096;assert(choose_scale(640,896)==4);max_image_dim=8192;
 assert(ac4_stable_auto_scale(5)==8&&ac4_stable_auto_scale(6)==8&&ac4_stable_auto_scale(8)==8);
 assert(ac4_stable_auto_scale(9)==9);
 swap_extent.width=1920;swap_extent.height=1080;assert(ac4_stable_auto_scale(3)==4);
 swap_extent.width=1280;swap_extent.height=720;assert(ac4_stable_auto_scale(2)==3);
 full=0;assert(ac4_stable_auto_scale(2)==2);full=1;
 PS2_GAME_AC4=0;assert(ac4_stable_auto_scale(2)==2);PS2_GAME_AC4=1;
 _putenv("PS2_AC4_STABLE_AUTO=0");assert(ac4_stable_auto_scale(2)==2);_putenv("PS2_AC4_STABLE_AUTO=");

 for(u32 base=1;base<=16;base++)for(u32 target=base;target<=16;target++)for(int n=1;n<=28;n++)for(int s=0;s<=8;s++){
  int p=n,q=s;warm_image_budget(base,target,&p,&q);
  assert(p>=0&&q>=0&&p<=n&&q<=s);
  assert((u64)(2*p+q)*target*target<=(u64)(2*n+s)*base*base);
 }
 int pairs=15,snaps=8;warm_image_budget(5,8,&pairs,&snaps);assert(pairs==5&&snaps==3);
 assert(!warm_layout_matches(640,896,4));
 swap_extent.width=3840;swap_extent.height=2160;
 assert(warm_layout_matches(640,896,6)&&warm_layout_matches(640,896,8));
 assert(!warm_layout_matches(640,896,9)&&!warm_layout_matches(640,448,8));
 warm_multiscale=0;assert(!warm_layout_matches(640,896,6));
 warm_image_n=1;warm_images[0].w=5120;warm_images[0].h=7168;
 warm_images[0].fmt=1;warm_images[0].usage=2;warm_images[0].aspect=3;
 warm_images[0].image=11;warm_images[0].memory=12;warm_images[0].view=13;
 int image,mem,view;assert(make_image(5120,7168,1,2,3,&image,&mem,&view)==0);
 assert(image==11&&mem==12&&view==13&&!warm_images[0].image&&warm_image_hits==1&&!direct_calls);
 assert(make_image(5120,7168,1,2,3,&image,&mem,&view)==0&&image==91&&direct_calls==1);
 failures=1;assert(make_image(3840,5376,1,2,3,&image,&mem,&view)==0);
 assert(releases==1&&direct_calls==3&&!warm_image_n);
 failures=1;assert(make_image(3840,5376,1,2,3,&image,&mem,&view)==-1&&direct_calls==4&&releases==1);
 puts("Image prewarm: bounded full-4K budget, scale transitions, exact ownership and allocation-pressure release passed");
}
'''
    source=Path(directory)/'image_prewarm.c';source.write_text(code)
    exe=Path(directory)/'image_prewarm.exe'
    subprocess.run([str(compiler),'-std=c2x','-O2','-Wall','-Wextra',str(source),'-o',str(exe)],env=env,check=True)
    subprocess.run([str(exe)],env=env,check=True)
