"""Compile the renderer's actual field-state handoff with synthetic field sequences."""
from pathlib import Path
import subprocess
from test_2d_target_policy import function

def run(root,compiler,directory,env):
    video=(root/'runtime/src/ps2_video.c').read_text()
    code=r'''#include <assert.h>
#include <stdint.h>
#include <stdio.h>
typedef uint32_t u32;typedef uint64_t u64;
#define RT_SLOTS 32u
#define PS2_VK_PRESENT_BLANK 0xffffffffu
typedef struct {u32 x,y,w,h,rt;int blank;} vk_present_state;
static vk_present_state pub_present={0,0,640,448,0,0};
static u32 present_x,present_y,present_w=640,present_h=448,present_rt;
static int present_blank;
static int *rep_verts,*rep_draws,*rep_mesh,*pub_verts,*pub_draws,*pub_mesh;
static u32 rep_nverts,rep_ndraws,pub_nverts,pub_ndraws;
static int frame_requested,frame_rendering,frame_trace_on=1;
static u64 rep_source,rep_ready_ns,rep_publish_ns,rep_take_ns,pub_source,pub_ready_ns,pub_publish_ns;
static int list_lock,list_cv,locked,broadcasts;
static void pthread_mutex_lock(int *p){assert(p==&list_lock&&!locked);locked=1;}
static void pthread_mutex_unlock(int *p){assert(p==&list_lock&&locked);locked=0;}
static void pthread_cond_broadcast(int *p){assert(p==&list_cv&&locked);broadcasts++;}
static u64 vk_mono_ns(void){return 99;}
'''
    for signature in ('static void publish_present_state(', 'static void latch_pending_frame(', 'static int take_pending_frame('):
        code+=function(video,signature)+'\n'
    code+=r'''
int main(void){
 int a,b,c;
 pub_verts=pub_draws=pub_mesh=&a;pub_nverts=3;pub_ndraws=1;
 pub_source=10;pub_ready_ns=11;pub_publish_ns=12;
 publish_present_state(0,0,512,448,1,-2);frame_requested=1;
 assert(take_pending_frame()&&!locked&&broadcasts==1);
 assert(rep_verts==&a&&rep_draws==&a&&rep_mesh==&a&&rep_nverts==3&&rep_ndraws==1);
 assert(present_w==512&&present_h==448&&present_rt==1&&frame_rendering&&!frame_requested);
 assert(rep_source==10&&rep_ready_ns==11&&rep_publish_ns==12&&rep_take_ns==99);
 /* Publish pause field B while A is being rendered: A's crop/target must stay immutable. */
 pub_verts=pub_draws=pub_mesh=&b;pub_nverts=6;pub_ndraws=2;
 publish_present_state(12,8,640,896,2,-2);frame_requested=1;
 assert(rep_verts==&a&&present_x==0&&present_w==512&&present_h==448&&present_rt==1);
 /* Replace queued B with C; omitted dimensions inherit B, not the current renderer A. */
 pub_verts=pub_draws=pub_mesh=&c;pub_nverts=9;pub_ndraws=3;
 publish_present_state(4,6,0,0,PS2_VK_PRESENT_BLANK,-2);
 assert(present_w==512&&!present_blank);
 assert(take_pending_frame()&&rep_verts==&c&&rep_nverts==9&&rep_ndraws==3);
 assert(present_x==4&&present_y==6&&present_w==640&&present_h==896&&present_rt==0&&present_blank);
 assert(!take_pending_frame()&&rep_verts==&c&&broadcasts==2);
 publish_present_state(0,0,512,448,PS2_VK_PRESENT_BLANK,35);frame_requested=1;
 assert(take_pending_frame()&&present_rt==3&&!present_blank);
 publish_present_state(0,0,512,448,90,-2);frame_requested=1;
 assert(take_pending_frame()&&present_rt==0&&!present_blank);
 puts("Field handoff: immutable active display, queued replacement, modal adoption, blank and debug targets passed");
}
'''
    source=Path(directory)/'frame_handoff.c';source.write_text(code)
    exe=Path(directory)/'frame_handoff.exe'
    subprocess.run([str(compiler),'-std=c2x','-O2','-Wall','-Wextra',str(source),'-o',str(exe)],env=env,check=True)
    subprocess.run([str(exe)],env=env,check=True)

    # Validate row batching against a per-pixel reference, including crop/stride.
    code = "#include <stdint.h>\n#include <stdio.h>\n#include <stdlib.h>\n#include <assert.h>\ntypedef uint32_t u32;typedef uint8_t u8;\n"
    code += function(video, 'static int write_rgba_ppm(')
    code += r'''
int main(int argc,char **argv) {
 assert(argc==2);u8 pixels[7*5*4];
 for (unsigned i=0;i<sizeof pixels;i++) pixels[i]=(u8)(i*17);
 assert(write_rgba_ppm(argv[1],pixels,7,2,1,3,2)==0);
 FILE *fp=fopen(argv[1],"rb");assert(fp);char header[32];
 assert(fgets(header,sizeof header,fp));assert(fgets(header,sizeof header,fp));assert(fgets(header,sizeof header,fp));
 for(unsigned y=1;y<3;y++)for(unsigned x=2;x<5;x++)for(unsigned c=0;c<3;c++)assert(fgetc(fp)==pixels[(y*7+x)*4+c]);
 assert(fgetc(fp)==EOF);fclose(fp);
 assert(write_rgba_ppm(argv[1],pixels,7,6,0,3,2)==-1);
 assert(write_rgba_ppm(argv[1],pixels,7,0,0,0,2)==-1);
 puts("Batched screenshot: exact RGB, padded stride, crop and invalid dimensions passed");
}
'''
    source=Path(directory)/'screenshot_rows.c';source.write_text(code)
    exe=Path(directory)/'screenshot_rows.exe'
    subprocess.run([str(compiler),'-std=c2x','-O2','-Wall','-Wextra',str(source),'-o',str(exe)],env=env,check=True)
    subprocess.run([str(exe),str(Path(directory)/'rows.ppm')],env=env,check=True)
