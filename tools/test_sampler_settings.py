"""Execute the renderer's actual sampler creation/settings code against mocks."""
from pathlib import Path
import subprocess
from test_2d_target_policy import function

def run(root,compiler,directory,env):
    video=(root/'runtime/src/ps2_video.c').read_text()
    source=Path(directory)/'sampler_settings.c'
    source.write_text(r'''#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
typedef unsigned u32;
typedef int VkResult,VkSampler,VkDevice;
typedef struct {int sType,magFilter,minFilter,mipmapMode,addressModeU,addressModeV,addressModeW,anisotropyEnable;float maxLod,maxAnisotropy;} VkSamplerCreateInfo;
#define VK_NULL_HANDLE 0
#define VK_SUCCESS 0
#define VK_TRUE 1
#define VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO 1
#define VK_FILTER_LINEAR 1
#define VK_SAMPLER_MIPMAP_MODE_NEAREST 0
#define VK_SAMPLER_MIPMAP_MODE_LINEAR 1
#define VK_SAMPLER_ADDRESS_MODE_REPEAT 0
#define VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE 1
#define VK_IMAGE_LAYOUT_UNDEFINED 0
#define VK_LOD_CLAMP_NONE 1000.0f
#define VK_MAX_TEXTURES 5
#define RT_SLOTS 1
static VkDevice dev=1;
static int vk_ready=1,gpu_lock,locked;
static struct {u32 nd,nv;int overflow;} ov_lists[2]={{3,9,1},{2,6,1}};
static unsigned long long ov_hold[2][2]={{7,8},{9,10}};
static unsigned char ov_grid[2][2]={{1,2},{3,4}};
static int ov_accum=1,ov_show=1,ov_seen=1,ov_presented=1;
static int ov_display_show=1,ov_display_presented=1,ov_display_ready=1,drains;
static void ps2_vk_drain(void){assert(!locked);drains++;}
static void pthread_mutex_lock(int *m){assert(m==&gpu_lock&&!locked);locked=1;}
static void pthread_mutex_unlock(int *m){assert(m==&gpu_lock&&locked);locked=0;}
static float max_aniso=8;
static int sampler_aniso=16,sampler=10,sampler_mip=20,sampler_mip_clamp[4]={20,21,22,23};
static int white_set=1,white_view=1;
static struct {int anisotropy;} ps2_cfg={4};
typedef struct {int set,view;u32 levels,native_clamp;} vk_texture;
static vk_texture textures[5]={{2,1,1,0},{3,1,4,0},{4,1,4,1},{5,1,4,2},{6,1,4,3}};
static u32 ntextures=5;
typedef struct {int created,set,cview,sset,sview;} vk_target;
static vk_target rts[1]={{1,7,1,8,1}},ov_comp={1,9,1,11,1},ov_display={1,10,1,0,0};
static int creates,fail_at,destroys,waits,wait_fail,bindings[16];
static VkSamplerCreateInfo captured[8];
static VkResult vkCreateSampler(VkDevice d,const VkSamplerCreateInfo *s,void *a,VkSampler *out) {
    (void)d;(void)a;captured[creates]=*s;creates++;
    if(creates==fail_at)return -1;
    *out=100+creates;return 0;
}
static void vkDestroySampler(VkDevice d,VkSampler s,void *a) {(void)d;(void)a;assert(s);destroys++;}
static VkResult vkDeviceWaitIdle(VkDevice d) {(void)d;waits++;return wait_fail?-1:0;}
static void rebind_set_s(int set,int view,VkSampler s) {(void)view;bindings[set]=s;}
static void rebind_set(int set,int view) {rebind_set_s(set,view,sampler);}
static void ps2_log(const char *fmt,...) {(void)fmt;}
''' + function(video,'static VkResult make_linear_sampler(')+'\n'
        + function(video,'static VkResult make_mip_samplers(')+'\n'
        + function(video,'static void apply_sampler_settings(')+'\n'
        + function(video,'int ps2_vk_set_anisotropy(')+'\n'
        + function(video,'void ps2_vk_replay_overlay_reset(')+r'''
int main(void) {
    for(int failure=1;failure<=5;failure++) {
        creates=destroys=waits=0;fail_at=failure;
        apply_sampler_settings();
        assert(sampler==10 && sampler_aniso==16 && sampler_mip==20);
        assert(!waits && destroys==failure-1 && !bindings[1]);
        for(int j=0;j<4;j++)assert(sampler_mip_clamp[j]==20+j);
    }
    fail_at=0;creates=destroys=waits=0;wait_fail=1;
    apply_sampler_settings();assert(sampler==10 && sampler_aniso==16 && destroys==5 && waits==1);
    wait_fail=0;creates=destroys=waits=0;
    apply_sampler_settings();assert(sampler==101 && sampler_aniso==4 && sampler_mip==102);
    assert(waits==1 && destroys==5 && bindings[1]==101 && bindings[2]==101);
    for(int i=0;i<4;i++) {
        assert(bindings[3+i]==102+i && sampler_mip_clamp[i]==102+i);
        assert(captured[i+1].mipmapMode==VK_SAMPLER_MIPMAP_MODE_LINEAR);
        assert(captured[i+1].addressModeU==(i&1?1:0));
        assert(captured[i+1].addressModeV==(i&2?1:0));
        assert(captured[i+1].maxAnisotropy==4);
    }
    assert(bindings[7]==101 && bindings[8]==101 && bindings[9]==101 && bindings[10]==101);
    assert(bindings[11]==101); /* The DATE snapshot must not retain a destroyed sampler. */
    creates=destroys=waits=0;apply_sampler_settings();assert(!creates&&!destroys&&!waits);
    ps2_cfg.anisotropy=0;apply_sampler_settings();
    for(int i=0;i<5;i++)assert(!captured[i].anisotropyEnable);
    ps2_cfg.anisotropy=16;creates=0;apply_sampler_settings();
    for(int i=0;i<5;i++)assert(captured[i].maxAnisotropy==8);
    assert(!ps2_vk_set_anisotropy(3) && !locked && sampler_aniso==16);
    creates=0;fail_at=1;assert(!ps2_vk_set_anisotropy(4));
    assert(!locked && ps2_cfg.anisotropy==16 && sampler_aniso==16);
    creates=0;fail_at=0;assert(ps2_vk_set_anisotropy(0));
    assert(!locked && ps2_cfg.anisotropy==0 && sampler_aniso==0);
    ps2_vk_replay_overlay_reset();
    for(int y=0;y<2;y++)for(int x=0;x<2;x++)assert(!ov_hold[y][x]&&!ov_grid[y][x]);
    assert(drains==1 && !locked && !ov_accum && !ov_show && !ov_seen && !ov_presented);
    assert(!ov_display_show && !ov_display_presented && !ov_display_ready);
    for(int i=0;i<2;i++)assert(!ov_lists[i].nd && !ov_lists[i].nv && !ov_lists[i].overflow);
    vk_ready=0;ps2_vk_replay_overlay_reset();assert(drains==1);
    assert(!ps2_vk_set_anisotropy(16) && !locked && ps2_cfg.anisotropy==0);
    puts("Sampler settings: native mip/wrap preservation, disable, limits and atomic failure rollback passed");
}
''')
    exe=Path(directory)/'sampler_settings.exe'
    subprocess.run([str(compiler),'-std=c2x','-O2','-Wall','-Wextra',str(source),'-o',str(exe),'-lm'],env=env,check=True)
    subprocess.run([str(exe)],env=env,check=True)
