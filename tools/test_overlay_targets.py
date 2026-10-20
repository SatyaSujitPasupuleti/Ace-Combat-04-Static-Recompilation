"""Exercise actual overlay attachment replacement against allocation failures."""
from pathlib import Path
import subprocess
from test_2d_target_policy import function


def run(root, compiler, directory, env):
    video = (root / 'runtime/src/ps2_video.c').read_text()
    source = Path(directory) / 'overlay_targets.c'
    source.write_text(r'''#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
typedef unsigned u32;typedef unsigned long long u64;
typedef struct {int color,cmem,cview,depth,dmem,dview,snap,smem,sview,set,sset;
 int layout,dlayout,slayout,created;} vk_target;
#define VK_FORMAT_R8G8B8A8_UNORM 1
#define VK_FORMAT_D32_SFLOAT 2
#define VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT 1
#define VK_IMAGE_USAGE_SAMPLED_BIT 2
#define VK_IMAGE_USAGE_TRANSFER_SRC_BIT 4
#define VK_IMAGE_USAGE_TRANSFER_DST_BIT 8
#define VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT 16
#define VK_IMAGE_ASPECT_COLOR_BIT 1
#define VK_IMAGE_ASPECT_DEPTH_BIT 2
#define VK_IMAGE_LAYOUT_UNDEFINED 0
#define VK_NULL_HANDLE 0
#define VK_SUCCESS 0
#define PS2_GAME_AC4 1
#define RT_SCALE_MAX 16u
static vk_target ov_comp;
static u32 ov_k,ov_cw,ov_ch;
static u32 rt_w=640,rt_h=448,present_w=640,present_h=448,max_image_dim=4096,rt_scale=1;
static struct {u32 width,height;} swap_extent={1280,896};
static int ov_show,ov_seen=1,ov_on=1,dev,wait_fail;
static int ov_enabled(void){return ov_on;}
static int vk_env_flag(const char *name,int *cached){(void)name;(void)cached;return 1;}
static int vkDeviceWaitIdle(int d){(void)d;return wait_fail?-1:0;}
static void ps2_log(const char *s,...){(void)s;}
static int calls,fail_at,set_calls,set_fail,bindings,old_destroyed,live_images;
static int make_image(u32 w,u32 h,int format,int usage,int aspect,int *image,int *memory,int *view){
 assert(w==1280 && h==896);assert(format && usage && aspect);
 if(++calls==fail_at)return -1;
 *image=*memory=*view=100+calls;live_images++;return 0;
}
static int alloc_set(int view){assert(view>=100);if(++set_calls==set_fail)return 0;return 200+set_calls;}
static void rebind_set(int set,int view){assert(set && view>=100);bindings++;}
static void destroy_target_images(vk_target *t){
 if(t==&ov_comp){old_destroyed++;assert(bindings>=1);}
 else {live_images-=!!t->color+!!t->depth+!!t->snap;}
 t->color=t->depth=t->snap=0;t->created=0;
}
''' + function(video, 'static u32 window_cover_scale(') + '\n'
        + function(video, 'static int full_output_resolution(') + '\n'
        + function(video, 'static int ov_replace_target(') + '\n'
        + function(video, 'static void ov_prepare(') + r'''
int main(void){
 vk_target old={1,1,1,2,2,2,3,3,3,10,11,7,7,7,1};
 for(int failure=1;failure<=3;failure++){
  ov_comp=old;ov_k=1;ov_cw=640;ov_ch=448;
  calls=bindings=old_destroyed=live_images=0;fail_at=failure;
  assert(!ov_replace_target(2,640,448,1));
  assert(!memcmp(&ov_comp,&old,sizeof old));
  assert(ov_k==1 && ov_cw==640 && ov_ch==448);
  assert(!bindings && !old_destroyed && !live_images);
 }
 calls=bindings=old_destroyed=live_images=0;fail_at=0;
 assert(ov_replace_target(2,640,448,1));
 assert(old_destroyed==1 && bindings==2 && live_images==3);
 assert(ov_comp.color==101 && ov_comp.depth==102 && ov_comp.snap==103);
 assert(ov_comp.set==10 && ov_comp.sset==11 && ov_comp.created);
 assert(ov_k==2 && ov_cw==640 && ov_ch==448 && !ov_comp.layout);
 for(int failure=1;failure<=2;failure++){
  ov_comp=(vk_target){0};calls=bindings=old_destroyed=live_images=set_calls=0;set_fail=failure;
  assert(!ov_replace_target(2,640,448,1));
  assert(!old_destroyed && !live_images && !ov_comp.created);
 }
 ov_comp=old;calls=bindings=old_destroyed=live_images=0;set_fail=0;
 assert(ov_replace_target(2,640,448,0));assert(calls==2 && !ov_comp.snap);
 /* A failed window-size change retains pending mask resources at old scale. */
 ov_comp=old;ov_k=1;ov_cw=640;ov_ch=448;
 calls=bindings=old_destroyed=live_images=0;fail_at=3;ov_on=1;
 ov_prepare();assert(ov_show && ov_on && ov_k==1);
 assert(!memcmp(&ov_comp,&old,sizeof old) && !old_destroyed && !bindings);
 /* A failed fence must not allocate or retire anything. */
 calls=0;wait_fail=1;ov_prepare();assert(!calls && ov_show && ov_k==1);wait_fail=0;
 /* Different logical GS dimensions cannot reuse a mismatched target. */
 calls=0;ov_cw=320;ov_prepare();assert(!ov_show && !ov_on && !old_destroyed);
 puts("Overlay targets: image/descriptor failures, atomic replacement and retained dimensions passed");
}
''')
    exe = Path(directory) / 'overlay_targets.exe'
    subprocess.run([str(compiler), '-std=c2x', '-O2', '-Wall', '-Wextra',
                    str(source), '-o', str(exe)], env=env, check=True)
    subprocess.run([str(exe)], env=env, check=True)
