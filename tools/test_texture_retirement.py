"""Exercise actual renderer cache/allocator code with mocked GPU completion."""
from pathlib import Path
import os,subprocess,tempfile
r=Path(__file__).resolve().parents[1]
s=(r/'runtime/src/ps2_video.c').read_text()
def fn(start):
 # Skip texture_store's forward declaration when extracting its definition.
 offset=s.index('static void texture_collect(') if start.startswith('static u32 texture_store') else 0
 a=s.index(start,offset)
 return s[a:s.index('\n}\n',a)+3]
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
static int record_retire,retire_count,retire_order[8];
static int gpu_lock,vk_ready=1;
static VkImageView white_view=999;
static u32 tex_rec_seq=2;
static u64 clock_value,handle=1;
static u64 vk_mono_ns(void){return ++clock_value;}
static void pthread_mutex_lock(int *p){(void)p;assert(!locked);locked=1;}
static void pthread_mutex_unlock(int *p){(void)p;assert(locked);locked=0;}
static int vkDeviceWaitIdle(int d){(void)d;assert(locked);if(wait_fail)return -1;idle=1;return 0;}
static void vkDestroyImageView(int d,VkImageView v,void *a){(void)d;(void)v;(void)a;assert(locked&&idle);}
static void vkDestroyImage(int d,VkImage v,void *a){(void)d;(void)v;(void)a;assert(locked&&idle);if(record_retire)retire_order[retire_count++]=(int)v-100;}
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
code+=(r/'runtime/include/ps2_vk_mip_chain.h').read_text().replace('#include "ps2_runtime.h"','')
code+='\nstatic int fail_cpu_alloc;\nstatic void *fixture_texture_alloc(size_t n){if(fail_cpu_alloc && !--fail_cpu_alloc)return NULL;return malloc(n); }\n'
code+='\nstatic u32 fixture_lowest_vacant(void){u32 i=0;while(i<ntextures && textures[i].rgba)i++;return i;}\n'
code+=fn('static void texture_collect(')+fn('static u32 texture_store(u64 key,').replace('malloc(bytes)','fixture_texture_alloc(bytes)')+fn('u32 ps2_vk_texture(u64 key,')+fn('int ps2_vk_texture_is(')+fn('u32 ps2_vk_texture_find(')+fn('u32 ps2_vk_texture_native_wrap(')
code+='\nstatic unsigned atlas_alloc_calls;\nstatic void *atlas_alloc(size_t bytes) {atlas_alloc_calls++;return bytes>VK_TEX_BYTES?NULL:malloc(bytes); }\n'
code+=fn('u32 ps2_vk_texture_native_atlas(').replace('malloc(layout.bytes)','atlas_alloc(layout.bytes)')
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
    for(int failure=1;failure<=2;failure++) {
        fail_cpu_alloc=failure;
        assert(texture_store(99,99,pixels,2,2,1,0)==0xffffffffu);
        assert(!ntextures && !tex_bytes && fixture_lowest_vacant()==0);
    }
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
    /* Native allocation must retire, not recycle, an uploaded GS image.
     * The old path returned that image with levels=1, permanently rejecting
     * the native conversion for this identity. Keep the other slot live. */
    textures[first].image=21;textures[first].view=22;
    tex_rec_seq+=3;textures[second].rec_seq=tex_rec_seq;
    u64 native_key=1ull<<63;
    u32 native=texture_store(native_key,7,pixels,32,128,1,0);
    assert(native==first && !textures[native].image && !textures[native].levels);
    assert(stat_tex_retired==2 && ps2_vk_texture_is(second,2,2));
    textures[native].levels=3;textures[native].image=23;
    assert(texture_store(native_key,7,pixels,32,128,1,0)==native);
    /* Extra owned mip bytes must be reserved before base allocation. All
     * entries are live: the rejected reservation must leave identities intact. */
    assert(texture_store(native_key+1,8,pixels,16,16,1,4096)==0xFFFFFFFFu);
    assert(ps2_vk_texture_is(native,native_key,7) && tex_bytes==VK_TEX_BYTES);
    assert(texture_store(native_key+1,8,pixels,16,16,1,VK_TEX_BYTES)==0xFFFFFFFFu);
    tex_rec_seq+=3;textures[second].rec_seq=tex_rec_seq;
    u32 supplied=texture_store(native_key+1,8,pixels,16,16,1,4096);
    assert(supplied==native && !textures[supplied].image);
    assert(tex_bytes+4096<=VK_TEX_BYTES && stat_tex_retired==3);
    /* Exercise the real native entry point after saturating the store. */
    assert(ps2_vk_texture(10,10,pixels,32,112)!=0xFFFFFFFFu);
    assert(ps2_vk_texture(11,11,pixels,16,16)!=0xFFFFFFFFu);
    assert(tex_bytes==VK_TEX_BYTES);
    tex_rec_seq+=3;textures[supplied].rec_seq=tex_rec_seq;
    u32 cropped=ps2_vk_texture_native_wrap(supplied,0,0,16,16,3);
    assert(cropped!=0xFFFFFFFFu && cropped!=supplied);
    assert(!textures[cropped].image && textures[cropped].levels==5);
    assert(textures[cropped].native_clamp==3);
    assert(!memcmp(textures[cropped].rgba,textures[supplied].rgba,16*16*4));
    assert(ps2_vk_texture_native_wrap(supplied,0,0,16,16,3)==cropped);
    /* Supplied levels use the same safe collection decision, including their
     * owned bytes. Keep the source live while retiring an unrelated image. */
    tex_rec_seq+=3;texture_collect(VK_TEX_BYTES);assert(!tex_bytes);
    u32 source=ps2_vk_texture(20,20,pixels,64,40);
    assert(source!=0xFFFFFFFFu);
    u32 pressure=ps2_vk_texture(21,21,pixels,64,64);
    assert(pressure!=0xFFFFFFFFu);
    textures[pressure].image=31;textures[pressure].view=32;
    tex_rec_seq+=3;textures[source].rec_seq=tex_rec_seq;
    u32 chain=ps2_vk_texture_native_atlas(source,32,32,0,0,32,32,3,1);
    assert(chain!=0xFFFFFFFFu && chain!=source && atlas_alloc_calls==1);
    assert(textures[chain].native_mip_bytes==5440 && textures[chain].levels==4);
    assert(textures[chain].native_clamp==1 && !textures[chain].image);
    assert(tex_bytes==64*40*8+32*32*8+5440);
    for(size_t j=0;j<5440;j++)assert(textures[chain].native_mips[j]==37);
    assert(ps2_vk_texture_native_atlas(source,32,32,0,0,32,32,3,1)==chain);
    assert(atlas_alloc_calls==1);
    /* A hypothetical oversized source must be rejected before attempting a
     * temporary chain allocation or reading its pixel memory. */
    textures[source].w=256;textures[source].h=136;
    assert(ps2_vk_texture_native_atlas(source,128,128,0,0,128,128,3,1)==0xFFFFFFFFu);
    assert(atlas_alloc_calls==1);
    textures[source].w=64;textures[source].h=40;
    /* Reuse only cold single-level images, including epoch wrap; queued and
     * native-mip images keep their identities under a saturated slot count. */
    tex_rec_seq+=3;texture_collect(VK_TEX_BYTES);assert(!tex_bytes);
    u32 slots[VK_MAX_TEXTURES];
    for(u32 k=0;k<VK_MAX_TEXTURES;k++)slots[k]=ps2_vk_texture(1000+k,1000+k,pixels,2,2);
    tex_rec_seq=1;
    for(u32 k=0;k<VK_MAX_TEXTURES;k++)textures[slots[k]].rec_seq=1;
    textures[slots[0]].rec_seq=0; /* age one */
    textures[slots[1]].rec_seq=0xffffffffu; /* age two */
    textures[slots[2]].rec_seq=0xfffffffdu;textures[slots[2]].levels=2;
    textures[slots[6]].rec_seq=0xfffffffdu;
    textures[slots[7]].rec_seq=0xfffffffcu;
    u32 recycled=ps2_vk_texture(2000,2000,pixels,2,2);
    assert(recycled==slots[6] || recycled==slots[7]);
    u32 other=ps2_vk_texture(2001,2001,pixels,2,2);
    assert(other!=recycled && (other==slots[6] || other==slots[7]));
    for(u32 k=0;k<6;k++)assert(ps2_vk_texture_is(slots[k],1000+k,1000+k));
    assert(ps2_vk_texture_is(recycled,2000,2000) && ps2_vk_texture_is(other,2001,2001));
    assert(!memcmp(textures[recycled].rgba,pixels,16));
    /* General lookup must use the complete identity and renew live ownership. */
    tex_rec_seq+=1;
    assert(ps2_vk_texture_find(2000,2000,2,2)==recycled);
    assert(textures[recycled].rec_seq==tex_rec_seq);
    assert(ps2_vk_texture_find(2000,2001,2,2)==0xffffffffu);
    assert(ps2_vk_texture_find(2000,2000,1,4)==0xffffffffu);
    /* Compare actual retirement order with the previous repeated oldest scan.
     * Include equal ages, live epochs and unsigned counter wrap. */
    for(u32 trial=0;trial<100;trial++) {
        tex_rec_seq+=10;texture_collect(VK_TEX_BYTES);
        for(u32 k=0;k<VK_MAX_TEXTURES;k++)slots[k]=texture_store(3000+k,3000+k,pixels,2,2,1,0);
        tex_rec_seq=(trial&1)?1:100;
        u32 ages[VK_MAX_TEXTURES],expected[VK_MAX_TEXTURES],live[VK_MAX_TEXTURES],count=0;
        for(u32 k=0;k<VK_MAX_TEXTURES;k++) {
            ages[k]=(trial*13+k*7+k/2)%9;live[k]=ages[k]<=2;
            textures[k].rec_seq=tex_rec_seq-ages[k];textures[k].image=100+k;
        }
        for(;;) {
            u32 age=2,victim=0xffffffffu;
            for(u32 k=0;k<VK_MAX_TEXTURES;k++)if(ages[k]>age){age=ages[k];victim=k;}
            if(victim==0xffffffffu)break;
            expected[count++]=victim;ages[victim]=0;
        }
        record_retire=1;retire_count=0;texture_collect(VK_TEX_BYTES);record_retire=0;
        assert((u32)retire_count==count && !memcmp(retire_order,expected,count*sizeof(u32)));
        for(u32 k=0;k<VK_MAX_TEXTURES;k++)
            assert((textures[k].rgba!=NULL)==live[k]);
    }
    /* Fill/retire/refill subsets and compare with the original lowest-hole
     * scan. Partial retirement must not change identity or slot ordering. */
    for(u32 trial=0;trial<500;trial++) {
        tex_rec_seq+=10;texture_collect(VK_TEX_BYTES);
        for(u32 k=0;k<VK_MAX_TEXTURES;k++) {
            u32 expected=0;while(expected<ntextures && textures[expected].rgba)expected++;
            u32 got=texture_store(10000+trial*16+k,trial*16+k,pixels,2,2,1,0);
            assert(got==expected);
        }
        assert(fixture_lowest_vacant()==ntextures);
        for(u32 k=0;k<ntextures;k++)textures[k].rec_seq=tex_rec_seq-((trial+k*3)%6);
        texture_collect(VK_TEX_BYTES);
        for(u32 k=0;k<VK_MAX_TEXTURES;k++) {
            u32 expected=0;while(expected<ntextures && textures[expected].rgba)expected++;
            assert(fixture_lowest_vacant()==expected);
            if(expected==ntextures)break;
            u32 got=texture_store(20000+trial*16+k,20000+trial*16+k,pixels,2,2,1,0);
            assert(got==expected);
        }
    }
    tex_rec_seq+=10;texture_collect(VK_TEX_BYTES);
    u32 hole=0;while(hole<ntextures && textures[hole].rgba)hole++;
    for(int failure=1;failure<=2;failure++) {
        fail_cpu_alloc=failure;
        assert(texture_store(40000,40000,pixels,2,2,1,0)==0xffffffffu);
        assert(fixture_lowest_vacant()==hole && !tex_bytes);
    }
    assert(texture_store(40000,40000,pixels,2,2,1,0)==hole);
    puts("PASS: 500 lowest-vacant oracle sequences, append/hole allocation failures, GPU slice reuse/coalescing/alignment, bind failure, live-epoch protection, failed GPU wait, mixed-size retirement, stale identity rejection, descriptor fallback, size guards, immutable native mip allocation under pressure, owned mip budget");
}
'''
with tempfile.TemporaryDirectory(prefix='ac4-retirement-') as td:
 p=Path(td);(p/'test.c').write_text(code)
 subprocess.run(['gcc','-std=gnu2x','-O2','-Wall','-Wextra',str(p/'test.c'),'-o',str(p/'test.exe')],check=True)
 subprocess.run([str(p/'test.exe')],check=True)
