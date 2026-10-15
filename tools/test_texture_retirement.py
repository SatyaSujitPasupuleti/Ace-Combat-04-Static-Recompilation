"""Exercise actual renderer cache/allocator code with mocked GPU completion."""
from pathlib import Path
import os,subprocess,tempfile
r=Path(__file__).resolve().parents[1]
s=(r/'runtime/src/ps2_video.c').read_text()
def fn(start):
 a=s.index(start);return s[a:s.index('\n}\n',a)+3]
code=r'''
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
typedef uint8_t u8; typedef uint32_t u32; typedef int32_t s32; typedef uint64_t u64;
typedef uint64_t VkDeviceMemory,VkDeviceSize,VkImage,VkImageView,VkDescriptorSet;
typedef struct {VkDeviceSize size,alignment;u32 memoryTypeBits;} VkMemoryRequirements;
typedef struct {int sType;VkDeviceSize allocationSize;u32 memoryTypeIndex;} VkMemoryAllocateInfo;
#define VK_MAX_TEXTURES 8
#define VK_TEX_BYTES (64u<<10)
#define VK_NULL_HANDLE 0
#define VK_SUCCESS 0
#define VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT 1
#define VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO 1
#define ps2_log(...) ((void)0)
static int dev,locked,idle,wait_fail,bind_fail,freed,descriptors;
static int gpu_lock,vk_ready=1;
static VkImageView white_view=999;
static u32 tex_rec_seq=2;
static u64 clock_value,handle=1;
static u64 vk_mono_ns(void){return ++clock_value;}
static void pthread_mutex_lock(int *p){(void)p;assert(!locked);locked=1;}
static void pthread_mutex_unlock(int *p){(void)p;assert(locked);locked=0;}
static int vkDeviceWaitIdle(int d){(void)d;assert(locked);if(wait_fail)return -1;idle=1;return 0;}
static void vkDestroyImageView(int d,VkImageView v,void *a){(void)d;(void)v;(void)a;assert(locked&&idle);}
static void vkDestroyImage(int d,VkImage v,void *a){(void)d;(void)v;(void)a;assert(locked&&idle);}
static void vkFreeMemory(int d,VkDeviceMemory m,void *a){(void)d;(void)m;(void)a;freed++;}
static void rebind_set(VkDescriptorSet set,VkImageView view){assert(locked&&idle&&set&&view==white_view);descriptors++;}
static VkMemoryRequirements requirements={100,64,1};
static void vkGetImageMemoryRequirements(int d,VkImage i,VkMemoryRequirements *q){(void)d;(void)i;*q=requirements;}
static u32 find_mem(u32 bits,u32 flags){(void)bits;(void)flags;return 0;}
static int vkBindImageMemory(int d,VkImage i,VkDeviceMemory m,VkDeviceSize o){(void)d;(void)i;(void)m;assert(o%requirements.alignment==0);return bind_fail?-1:0;}
static int traced_allocate_memory(int d,VkMemoryAllocateInfo *a,void *b,VkDeviceMemory *m){(void)d;(void)a;(void)b;*m=++handle;return 0;}
'''
a=s.index('typedef struct {\n    u64 key;');b=s.index('} vk_texture;',a)+len('} vk_texture;')
code+=s[a:b]+'\nstatic vk_texture textures[VK_MAX_TEXTURES];\nstatic u32 ntextures;\n'
a=s.index('#define TEX_HASH');b=s.index('static pthread_mutex_t list_lock',a)
code+=s[a:b]
a=s.index('#define TEXMEM_BLOCK');b=s.index('static int make_tex_image(',a)
code+=s[a:b]
code+=fn('static void texture_collect(')+fn('u32 ps2_vk_texture(u64 key,')+fn('int ps2_vk_texture_is(')
code+=r'''
int main(void) {
    /* Returned GPU slices coalesce, respect new alignment and shrink the tail. */
    vk_texture a={.image=1},b={.image=2},c={.image=3},d={.image=4};
    assert(texmem_bind(&a)==0 && texmem_bind(&b)==0 && texmem_bind(&c)==0);
    assert(a.memory==b.memory && b.memory==c.memory && ntexmem==1);
    assert(texmem[0].used==356);
    texmem_release(&b);texmem_release(&a);
    assert(ntexmem_free==1 && texmem_free[0].off==0 && texmem_free[0].size==228);
    requirements.size=128;requirements.alignment=128;
    assert(texmem_bind(&d)==0 && d.memory_offset==0 && d.memory_size==128);
    texmem_release(&d);texmem_release(&c);
    assert(ntexmem_free==0 && texmem[0].used==0);
    /* Binding failure does not consume a reusable slice. */
    requirements.size=100;requirements.alignment=64;
    assert(texmem_bind(&a)==0 && texmem_bind(&b)==0);
    texmem_release(&a);bind_fail=1;
    assert(texmem_bind(&c)==-1 && texmem_free[0].size==100);
    bind_fail=0;texmem_release(&b);assert(texmem[0].used==0);
    vk_texture own={.memory=9999};texmem_release(&own);assert(freed==1);

    u8 pixels[64*64*4];memset(pixels,37,sizeof pixels);
    u32 first=ps2_vk_texture(1,1,pixels,64,64);
    u32 second=ps2_vk_texture(2,2,pixels,64,64);
    assert(first!=second && tex_bytes==VK_TEX_BYTES);
    textures[first].image=17;textures[first].view=18;textures[first].set=19;
    /* Both still referenced by the three display lists: no collection. */
    tex_rec_seq=4;
    assert(ps2_vk_texture(3,3,pixels,32,128)==0xFFFFFFFFu && !stat_tex_collects);
    tex_rec_seq=5;textures[second].rec_seq=tex_rec_seq;
    wait_fail=1;
    assert(ps2_vk_texture(3,3,pixels,32,128)==0xFFFFFFFFu);
    assert(ps2_vk_texture_is(first,1,1) && !stat_tex_retired && !locked);
    wait_fail=0;
    u32 replacement=ps2_vk_texture(3,3,pixels,32,128);
    assert(replacement==first && ntextures==2 && tex_bytes==VK_TEX_BYTES);
    assert(!ps2_vk_texture_is(first,1,1) && ps2_vk_texture_is(second,2,2));
    assert(ps2_vk_texture_is(first,3,3) && textures[first].set==19 && descriptors==1);
    assert(textures[first].w==32 && textures[first].h==128);
    assert(!memcmp(textures[first].rgba,pixels,sizeof pixels));
    assert(stat_tex_retired==1 && stat_tex_collects==1 && !locked);
    assert(ps2_vk_texture(4,4,pixels,0,1)==0xFFFFFFFFu);
    assert(ps2_vk_texture(4,4,pixels,0xffffffffu,0xffffffffu)==0xFFFFFFFFu);
    puts("PASS: GPU slice reuse/coalescing/alignment, bind failure, live-epoch protection, failed GPU wait, mixed-size retirement, stale identity rejection, descriptor fallback, size guards");
}
'''
with tempfile.TemporaryDirectory(prefix='ac4-retirement-') as td:
 p=Path(td);(p/'test.c').write_text(code)
 subprocess.run(['gcc','-std=gnu2x','-O2','-Wall','-Wextra',str(p/'test.c'),'-o',str(p/'test.exe')],check=True)
 subprocess.run([str(p/'test.exe')],check=True)
