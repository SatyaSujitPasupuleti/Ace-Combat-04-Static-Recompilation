"""Exercise the production startup image pool with mocked Vulkan ownership."""
from pathlib import Path
import os,subprocess
r=Path(__file__).resolve().parents[1];w=r/'work/image-reserve-test';w.mkdir(exist_ok=True)
s=(r/'runtime/src/ps2_video.c').read_text();a=s.index('#define WARM_IMAGE_MAX');b=s.index('static void barrier_fill',a)
head=r"""
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
typedef uint32_t u32;typedef uint64_t u64;
typedef u64 VkImage,VkDeviceMemory,VkImageView;
typedef u32 VkFormat,VkImageUsageFlags,VkImageAspectFlags;
#define VK_NULL_HANDLE 0
#define VK_FORMAT_R8G8B8A8_UNORM 37
#define VK_FORMAT_D32_SFLOAT 126
#define VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT 16
#define VK_IMAGE_USAGE_SAMPLED_BIT 4
#define VK_IMAGE_USAGE_TRANSFER_SRC_BIT 1
#define VK_IMAGE_USAGE_TRANSFER_DST_BIT 2
#define VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT 32
#define VK_IMAGE_ASPECT_COLOR_BIT 1
#define VK_IMAGE_ASPECT_DEPTH_BIT 2
static int dev,allocated,destroyed;
static u32 rt_w=640,rt_h=896,rt_scale=5;
static u64 vk_mono_ns(void){return 0;}
#define ps2_log(...) ((void)0)
#define vkDestroyImageView(...) (++destroyed)
#define vkDestroyImage(...) (++destroyed)
#define vkFreeMemory(...) (++destroyed)
static int make_image_direct(u32 w,u32 h,VkFormat f,VkImageUsageFlags u,VkImageAspectFlags a,VkImage *i,VkDeviceMemory *m,VkImageView *v){*i=*m=*v=++allocated;return 0;}
"""
tail=r"""
int main(int argc,char **argv){
 assert(argc==2);u32 expected=atoi(argv[1]);warm_native_images();
 assert(warm_image_n==expected && allocated==(int)expected);
 for(u32 j=0;j<expected;j++){
  VkImage i;VkDeviceMemory m;VkImageView v;u64 handle=warm_images[j].image;
  assert(!make_image(warm_images[j].w,warm_images[j].h,warm_images[j].fmt,warm_images[j].usage,warm_images[j].aspect,&i,&m,&v));
  assert(i==handle && !warm_images[j].image && allocated==(int)expected);
 }
 warm_images_release();assert(!destroyed && !warm_image_n);
 VkImage i;VkDeviceMemory m;VkImageView v;
 assert(!make_image(4,4,37,6,1,&i,&m,&v));assert(allocated==(int)expected+1);
 warm_native_images();assert(warm_image_n==expected);warm_images_release();assert(destroyed==(int)expected*3);
 return 0;
}
"""
(w/'test.c').write_text(head+s[a:b]+tail)
bin=r.parent/'toolchain/msys64/ucrt64/bin';env=os.environ.copy();env['PATH']=str(bin)+os.pathsep+env['PATH']
subprocess.run([str(bin/'gcc.exe'),'-O2',str(w/'test.c'),'-o',str(w/'test.exe')],env=env,check=True)
for pairs,snaps,total in [('0','8',0),('14',None,32),('15','8',38),('28','100',64),('15','-1',30),('29','8',0)]:
 e=env.copy();e['PS2_IMAGE_PREWARM']=pairs;e.pop('PS2_SNAPSHOT_PREWARM',None)
 if snaps is not None:e['PS2_SNAPSHOT_PREWARM']=snaps
 subprocess.run([str(w/'test.exe'),str(total)],env=e,check=True)
print('PASS image reserve counts, bounds, exact-match ownership transfer, fallback and cleanup')
