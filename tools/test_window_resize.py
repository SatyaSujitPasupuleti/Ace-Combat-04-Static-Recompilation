"""Exercise the actual hidden-window resize guard and failure handling against mocks."""
from pathlib import Path
import subprocess
from test_2d_target_policy import function

def run(root, compiler, directory, env):
    source=Path(directory)/'window_resize.c'
    body=function((root/'runtime/src/ps2_video.c').read_text(),'int ps2_vk_test_resize_window(')
    body += '\n' + function((root/'runtime/src/ps2_video.c').read_text(),'int ps2_vk_test_skip_next_present(')
    body += '\n' + function((root/'runtime/src/ps2_video.c').read_text(),'static u32 window_cover_scale(')
    source.write_text(r'''#include <assert.h>
#include <stdio.h>
#include <stdint.h>
typedef uint32_t u32;typedef uint64_t u64;
#define RT_SCALE_MAX 8
#define VK_SUCCESS 0
static int vk_ready=1,window=1,window_hidden=1,gpu_lock,locked,dev,swap_dirty=1;
static int test_skip_present;
static int fail,sets,syncs,creates,wanted_w,wanted_h;
static struct {int window_w,window_h;} ps2_cfg={800,600};
static struct {unsigned width,height;} swap_extent;
static void pthread_mutex_lock(int *p){assert(p==&gpu_lock&&!locked);locked=1;}
static void pthread_mutex_unlock(int *p){assert(p==&gpu_lock&&locked);locked=0;}
static int vkDeviceWaitIdle(int d){(void)d;assert(locked);return fail==1?-1:0;}
static int SDL_SetWindowSize(int w,int x,int y){assert(w==window&&locked);sets++;wanted_w=x;wanted_h=y;return fail!=2;}
static int SDL_SyncWindow(int w){assert(w==window&&locked);syncs++;return fail!=3;}
static int create_swapchain(void){assert(locked);creates++;swap_extent.width=wanted_w;swap_extent.height=wanted_h;return fail==4?-1:0;}
static void ps2_log(const char *f,...){(void)f;}
''' + body + r'''
int main(void){
 assert(window_cover_scale(3840,2160,512,448)==8);
 assert(window_cover_scale(3840,2160,640,448)==6);
 assert(window_cover_scale(1920,1080,512,448)==4);
 assert(window_cover_scale(1920,1440,640,480)==3);
 assert(window_cover_scale(720,1280,640,448)==3);
 assert(window_cover_scale(0,0,640,448)==1);
 assert(window_cover_scale(3840,2160,1,1)==1);
 assert(window_cover_scale(UINT32_MAX,UINT32_MAX,512,448)==8);
 window_hidden=0;assert(!ps2_vk_test_skip_next_present()&&!test_skip_present);window_hidden=1;
 vk_ready=0;assert(!ps2_vk_test_skip_next_present()&&!test_skip_present);vk_ready=1;
 assert(ps2_vk_test_skip_next_present()&&test_skip_present);test_skip_present=0;
 assert(!ps2_vk_test_resize_window(0,720));assert(!ps2_vk_test_resize_window(7681,720));assert(!ps2_vk_test_resize_window(1280,4321));
 window_hidden=0;assert(!ps2_vk_test_resize_window(1280,720));window_hidden=1;
 vk_ready=0;assert(!ps2_vk_test_resize_window(1280,720));vk_ready=1;
 assert(!sets&&!syncs&&!creates&&!locked);
 for(fail=1;fail<=4;fail++){
  sets=syncs=creates=0;assert(!ps2_vk_test_resize_window(1280,720));
  assert(!locked&&ps2_cfg.window_w==800&&ps2_cfg.window_h==600&&swap_dirty==1);
  assert(sets==(fail>1)&&syncs==(fail>2)&&creates==(fail>3));
 }
 fail=0;assert(ps2_vk_test_resize_window(3840,2160));
 assert(!locked&&!swap_dirty&&ps2_cfg.window_w==3840&&ps2_cfg.window_h==2160);
 puts("Window resize: hidden-only guards, SDL/GPU failures, serialized resize and unchanged failed settings passed");
}
''')
    exe=Path(directory)/'window_resize.exe'
    subprocess.run([str(compiler),'-std=c2x','-O2','-Wall','-Wextra',str(source),'-o',str(exe)],env=env,check=True)
    subprocess.run([str(exe)],env=env,check=True)
