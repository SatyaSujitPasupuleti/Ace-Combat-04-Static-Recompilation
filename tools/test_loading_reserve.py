"""Production loading reserve: real Vulkan layouts with mocked budgets/allocations."""
from pathlib import Path
import subprocess
from test_2d_target_policy import function

def run(root,compiler,directory,env):
 video=(root/'runtime/src/ps2_video.c').read_text()
 head=r'''#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>
typedef uint32_t u32;typedef uint64_t u64;
static VkDevice dev;static VkPhysicalDevice phys;
static VkPhysicalDeviceMemoryProperties memprops;
static int memory_budget_enabled=1,PS2_GAME_AC4=1;
static u32 rt_w=640,rt_h=896,rt_scale=8,max_image_dim=16384;
static struct {u32 width,height;} swap_extent={3840,2160};
static struct {int internal_res;} ps2_cfg;
static u64 available=20ull<<30;static int allocated,destroyed,pressure_at,split_heap;
#define ps2_log(...) ((void)0)
static u64 vk_mono_ns(void){return 0;}
static int full_output_resolution(void){return 1;}
static u32 window_cover_scale(u32 w,u32 h,u32 pw,u32 ph){u32 x=(w+pw-1)/pw,y=(h+ph-1)/ph;return x>y?x:y;}
static u32 find_mem(u32 bits,VkMemoryPropertyFlags flags){(void)flags;return bits==2?1:0;}
static u64 image_cost(VkFormat f,VkImageUsageFlags u){return f==VK_FORMAT_D32_SFLOAT?149553168:f==VK_FORMAT_R32G32B32A32_SFLOAT?587202560:u==6?146800640:147374080;}
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceMemoryProperties2(VkPhysicalDevice p,VkPhysicalDeviceMemoryProperties2 *props){
 (void)p;VkPhysicalDeviceMemoryBudgetPropertiesEXT *b=props->pNext;
 b->heapBudget[0]=24ull<<30;b->heapUsage[0]=b->heapBudget[0]-available;
}
VKAPI_ATTR void VKAPI_CALL vkGetDeviceImageMemoryRequirements(VkDevice d,const VkDeviceImageMemoryRequirements *di,VkMemoryRequirements2 *req){
 (void)d;assert(di->sType==VK_STRUCTURE_TYPE_DEVICE_IMAGE_MEMORY_REQUIREMENTS);
 const VkImageCreateInfo *i=di->pCreateInfo;
 assert(i->extent.width==5120&&i->extent.height==7168&&i->mipLevels==1&&i->samples==VK_SAMPLE_COUNT_1_BIT);
 req->memoryRequirements.size=image_cost(i->format,i->usage);
 req->memoryRequirements.memoryTypeBits=split_heap&&i->format==VK_FORMAT_D32_SFLOAT?2:1;
}
VKAPI_ATTR void VKAPI_CALL vkDestroyImageView(VkDevice d,VkImageView v,const VkAllocationCallbacks *a){(void)d;(void)v;(void)a;destroyed++;}
VKAPI_ATTR void VKAPI_CALL vkDestroyImage(VkDevice d,VkImage i,const VkAllocationCallbacks *a){(void)d;(void)i;(void)a;destroyed++;}
VKAPI_ATTR void VKAPI_CALL vkFreeMemory(VkDevice d,VkDeviceMemory m,const VkAllocationCallbacks *a){(void)d;(void)m;(void)a;destroyed++;}
static int make_image_direct(u32 w,u32 h,VkFormat fmt,VkImageUsageFlags usage,VkImageAspectFlags aspect,VkImage *image,VkDeviceMemory *memory,VkImageView *view){
 (void)w;(void)h;(void)aspect;allocated++;
 *image=(VkImage)(uintptr_t)allocated;*memory=(VkDeviceMemory)(uintptr_t)allocated;*view=(VkImageView)(uintptr_t)allocated;
 u64 bytes=image_cost(fmt,usage);available=available>bytes?available-bytes:0;
 if(pressure_at&&allocated>=pressure_at)available=1ull<<30;
 return 0;
}
'''
 code=head+function(video,'static void image_create_info(')
 a=video.index('#define WARM_IMAGE_MAX');b=video.index('static void barrier_fill',a)
 code+="\n"+video[a:b]
 code+=r'''
static void reset(void){warm_images_release();allocated=destroyed=pressure_at=split_heap=0;available=20ull<<30;memory_budget_enabled=1;}
int main(void){
 _putenv("PS2_IMAGE_PREWARM=15");_putenv("PS2_SNAPSHOT_PREWARM=8");_putenv("PS2_LOADING_PREWARM=1");_putenv("PS2_UPSCALE=0");
 memprops.memoryHeapCount=2;memprops.memoryTypeCount=2;memprops.memoryTypes[0].propertyFlags=memprops.memoryTypes[1].propertyFlags=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;memprops.memoryTypes[0].heapIndex=0;memprops.memoryTypes[1].heapIndex=1;
 assert(warm_heap_available(0)==20ull<<30);assert(warm_heap_available(2)==0);
 u64 cost[5];u32 heap=0;
 assert(warm_loading_capacity(5120,7168,cost,&heap)&&heap==0&&cost[3]==149553168&&cost[4]==147374080);
 split_heap=1;assert(!warm_loading_capacity(5120,7168,cost,&heap));split_heap=0;
 memprops.memoryTypes[0].propertyFlags=0;assert(!warm_loading_capacity(5120,7168,cost,&heap));memprops.memoryTypes[0].propertyFlags=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
 warm_native_images();assert(warm_image_n==43&&allocated==43);
 assert(warm_layout_matches(640,896,8));
 swap_extent.width=1920;swap_extent.height=1080;assert(!warm_layout_matches(640,896,4));
 swap_extent.width=3840;swap_extent.height=2160;
 assert(warm_images[42].usage==21&&warm_images[40].usage==32&&warm_images[41].usage==32);
 warm_images_release();assert(destroyed==129);reset();
 available=8ull<<30;warm_native_images();assert(warm_image_n==13);reset();
 memory_budget_enabled=0;warm_native_images();assert(warm_image_n==13);reset();
 _putenv("PS2_LOADING_PREWARM=0");warm_native_images();assert(warm_image_n==13);reset();_putenv("PS2_LOADING_PREWARM=1");
 pressure_at=6;warm_native_images();assert(warm_image_n==6&&allocated==6);reset();
 _putenv("PS2_IMAGE_PREWARM=0");warm_native_images();assert(!allocated&&!warm_image_n);
 puts("Loading reserve: exact requirements, heap selection, startup preparation, low/unsupported budget fallback, opt-out, pressure stop and cleanup passed");
}
'''
 source=Path(directory)/'loading_reserve.c';source.write_text(code)
 exe=Path(directory)/'loading_reserve.exe'
 subprocess.run([str(compiler),'-std=c2x','-O2','-Wall','-Wextra',str(source),'-o',str(exe)],env=env,check=True)
 subprocess.run([str(exe)],env=env,check=True)
