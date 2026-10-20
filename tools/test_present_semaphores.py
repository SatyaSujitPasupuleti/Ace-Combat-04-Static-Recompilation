"""Check the actual presentation-semaphore pool lifetime and failed recreation."""
from pathlib import Path
import subprocess
from test_2d_target_policy import function

def run(root,compiler,directory,env):
    video=(root/'runtime/src/ps2_video.c').read_text()
    # Both submit and present must use the acquired image, including after resize.
    draw=function(video,'static void draw_and_present(')
    assert 'show ? swap_rendered[image_index] : VK_NULL_HANDLE' in draw
    assert 'si.pSignalSemaphores = &present_done' in draw
    assert 'pi.pWaitSemaphores = &present_done' in draw
    assert 'fr->rendered' not in video
    code=r'''#include <assert.h>
#include <string.h>
#include <stdio.h>
typedef unsigned u32;typedef unsigned VkSemaphore;
typedef struct {int sType;} VkSemaphoreCreateInfo;
#define VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO 1
#define VK_SUCCESS 0
static unsigned dev=1,next=1,calls,fail_call,live[128];
static VkSemaphore swap_rendered[8];
static int vkCreateSemaphore(unsigned d,const VkSemaphoreCreateInfo *ci,void *p,VkSemaphore *out){
 assert(d==dev&&ci->sType==1&&!p);calls++;if(calls==fail_call)return -1;
 *out=next++;assert(*out<128);live[*out]=1;return 0;
}
static void vkDestroySemaphore(unsigned d,VkSemaphore sem,void *p){assert(d==dev&&!p&&sem&&live[sem]);live[sem]=0;}
'''
    code+=function(video,'static int create_present_semaphores(')
    code+=r'''
typedef int VkResult;
typedef struct {int acquired;} vk_frame;
#define VK_ERROR_OUT_OF_DATE_KHR -2
#define VK_SUBOPTIMAL_KHR 1
#define VK_WAIT_NS 10
#define VK_NULL_HANDLE 0
static int swapchain,swap_dirty,results[2],acquires,idles,recreate_failed;
static int vkAcquireNextImageKHR(unsigned d,int sc,int timeout,int sem,int fence,u32 *image){assert(d==dev&&sc==swapchain&&timeout==10&&sem==7&&!fence);*image=2;return results[acquires++];}
static void vkDeviceWaitIdle(unsigned d){assert(d==dev);idles++;}
static int create_swapchain(void){return recreate_failed;}
'''
    code+=function(video,'static VkResult acquire_swap_image(')
    code+=r'''
typedef int VkCommandBuffer;
#define VK_IMAGE_ASPECT_COLOR_BIT 1
#define VK_IMAGE_LAYOUT_UNDEFINED 2
#define VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL 3
#define VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT 4
#define VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT 5
static int swap_images[8]={10,11,12},barriers;
static void barrier(int cmd,int image,int aspect,int old,int next,int src,int src_access,int dst,int dst_access){assert(cmd==8&&image==12&&aspect==1&&old==2&&next==3&&src==4&&!src_access&&dst==4&&dst_access==5);barriers++;}
'''
    code+=function(video,'static void acquire_image_barrier(')
    code+=r'''
int main(void){
 acquire_image_barrier(8,2);assert(barriers==1);
 vk_frame frame={7};u32 image;
 results[0]=VK_SUBOPTIMAL_KHR;assert(acquire_swap_image(&frame,&image)==VK_SUBOPTIMAL_KHR);
 assert(acquires==1&&!idles&&swap_dirty&&image==2);
 acquires=swap_dirty=0;results[0]=VK_ERROR_OUT_OF_DATE_KHR;results[1]=VK_SUCCESS;
 assert(acquire_swap_image(&frame,&image)==VK_SUCCESS&&acquires==2&&idles==1&&!swap_dirty);
 acquires=idles=0;results[1]=VK_SUBOPTIMAL_KHR;
 assert(acquire_swap_image(&frame,&image)==VK_SUBOPTIMAL_KHR&&acquires==2&&idles==1&&swap_dirty);
 acquires=idles=swap_dirty=0;recreate_failed=1;
 assert(acquire_swap_image(&frame,&image)==VK_ERROR_OUT_OF_DATE_KHR&&acquires==1&&idles==1);
 acquires=idles=0;results[0]=-99;
 assert(acquire_swap_image(&frame,&image)==-99&&acquires==1&&!idles);

 assert(create_present_semaphores(0)==1&&calls==0);
 assert(create_present_semaphores(9)==1&&calls==0);
 assert(create_present_semaphores(3)==0);
 unsigned old[8];memcpy(old,swap_rendered,sizeof old);
 // Image acquisition is deliberately not CPU frame order (0,1,0,1...).
 unsigned images[]={2,0,1,2,1,0};
 for(unsigned i=0;i<6;i++){assert(swap_rendered[images[i]]==old[images[i]]);assert(live[swap_rendered[images[i]]]);}
 for(unsigned failure=1;failure<=4;failure++){
  fail_call=calls+failure;assert(create_present_semaphores(4)==1);
  assert(!memcmp(old,swap_rendered,sizeof old));
  for(unsigned i=1;i<next;i++){int kept=0;for(unsigned j=0;j<3;j++)kept|=i==old[j];assert(live[i]==(unsigned)kept);}
 }
 fail_call=0;assert(create_present_semaphores(4)==0);
 for(unsigned i=0;i<3;i++)assert(!live[old[i]]);
 assert(swap_rendered[3]&&live[swap_rendered[3]]);
 memcpy(old,swap_rendered,sizeof old);assert(create_present_semaphores(2)==0);
 for(unsigned i=0;i<4;i++)assert(!live[old[i]]);
 for(unsigned i=2;i<8;i++)assert(!swap_rendered[i]);
 puts("Presentation synchronization: image ownership, suboptimal acquisition, resize and failure rollback passed");
}
'''
    source=Path(directory)/'present_semaphores.c';source.write_text(code)
    exe=Path(directory)/'present_semaphores.exe'
    subprocess.run([str(compiler),'-std=c2x','-O2','-Wall','-Wextra',str(source),'-o',str(exe)],env=env,check=True)
    subprocess.run([str(exe)],env=env,check=True)
