"""Compile actual renderer target predicates for both AC4 and inherited AC5."""
from pathlib import Path
import subprocess


def function(text, signature):
    start = text.index(signature)
    pos = text.index('{', start)
    # A forward declaration can precede the real definition.
    while text.find(';', start, pos) >= 0:
        start = text.index(signature, text.index(';', start) + 1)
        pos = text.index('{', start)
    depth = 1
    end = pos + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


def run(root, compiler, directory, env):
    video = (root / 'runtime/src/ps2_video.c').read_text()
    layer = (root / 'runtime/src/rn/rn_2d.c').read_text()
    source = Path(directory) / 'target_policy.c'
    source.write_text(r'''#include "ps2_vk.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static u32 present_rt;
#define RT_SLOTS 32u
typedef struct {int value,snap,sset;} vk_target;
static vk_target rts[RT_SLOTS],ov_comp,ov_display;
static int mask_enabled;
static int ov_precompose;
static int vk_env_flag(const char *name,int *cached){(void)cached;return strstr(name,"PRECOMPOSE")?ov_precompose:mask_enabled;}
static int vk_ready,frame_requested,frame_rendering,renderer_running;
static int list_lock,list_cv,lock_held,waits;
static void pthread_mutex_lock(int *p){assert(p==&list_lock);assert(!lock_held);lock_held=1;}
static void pthread_mutex_unlock(int *p){assert(p==&list_lock);assert(lock_held);lock_held=0;}
static void pthread_cond_wait(int *c,int *m){
 assert(c==&list_cv && m==&list_lock && lock_held);waits++;
 if(frame_requested){frame_requested=0;frame_rendering=1;}
 else frame_rendering=0;
}
typedef struct {u32 tex_rt,tex_depth,rt,depth,native,clut_rt,tex_self,date,frag_flags,blend;} vk_draw;
static vk_draw rep_draws[3];
static u32 rep_ndraws;
static const void *screen_mesh_rec(const vk_draw *d,int i){(void)d;(void)i;return 0;}
enum {REJ_NONE,REJ_TARGET,REJ_TEXRT,REJ_ZWRITE,REJ_DATE,REJ_DSTALPHA,REJ_COLCLIP,REJ_MASK};
''' + function(video, 'static int ov_is_composite(') + '\n'
        + function(video, 'static int ov_single_composite(') + '\n'
        + function(video, 'static int ov_safe(') + '\n'
        + function(video, 'static int ov_mask_tail(') + '\n'
        + function(video, 'static u32 present_log_slot(') + '\n'
        + function(video, 'void ps2_vk_drain(') + '\n'
        + function(layer, 'static int reject_reason(') + r'''
int main(void){
 vk_ready=1;renderer_running=1;
 ps2_vk_drain();assert(!waits && !lock_held);
 frame_requested=1;ps2_vk_drain();assert(waits==2 && !lock_held);
 /* An older frame's completion cannot hide the active final render. */
 frame_rendering=1;ps2_vk_drain();assert(waits==3 && !lock_held);
 renderer_running=0;frame_requested=1;ps2_vk_drain();assert(waits==3 && !lock_held);
 vk_ready=0;renderer_running=1;ps2_vk_drain();assert(waits==3 && !lock_held);
 for(unsigned i=0;i<RT_SLOTS;i++)assert(present_log_slot(rts+i)==i);
 assert(present_log_slot(&ov_comp)==RT_SLOTS);
 assert(present_log_slot(&ov_display)==RT_SLOTS+1u);
 const u32 scene=PS2_GAME_AC4?1u:0u,display=PS2_GAME_AC4?0u:1u;
 assert(PS2_VK_2D_SCENE_RT==scene);present_rt=display;
 vk_draw d={0};d.rt=display;d.tex_rt=scene+1;
 rep_ndraws=0;assert(ov_single_composite());
 rep_draws[0]=d;rep_ndraws=1;assert(ov_single_composite());
 rep_draws[1]=d;rep_ndraws=2;assert(!ov_single_composite());
 rep_draws[1].rt=scene;assert(ov_single_composite());
 assert(ov_is_composite(&d));d.tex_depth=1;assert(!ov_is_composite(&d));d.tex_depth=0;
 d.rt=scene;assert(!ov_is_composite(&d));d.rt=display;d.tex_rt=display+1;assert(!ov_is_composite(&d));
 d=(vk_draw){0};d.native=PS2_VK_NATIVE_2D;d.depth=1;
 assert(ov_safe(&d));d.tex_rt=scene+1;assert(!ov_safe(&d));d.tex_rt=0;
 d.date=1;assert(!ov_safe(&d));d.date=0;d.blend=0x110;assert(!ov_safe(&d));
 d=(vk_draw){0};d.date=1;d.depth=1;
 assert(!ov_mask_tail(&d));mask_enabled=1;assert(!ov_mask_tail(&d));
 ov_comp.snap=1;assert(!ov_mask_tail(&d));ov_comp.sset=1;
 assert(!ov_mask_tail(&d));ov_precompose=1;
 assert(ov_mask_tail(&d)==PS2_GAME_AC4);assert(!ov_safe(&d));
 d.tex_self=1;assert(!ov_mask_tail(&d));d.tex_self=0;
 d.tex_rt=scene+1;assert(!ov_mask_tail(&d));d.tex_rt=0;
 d.frag_flags=1u<<30;assert(!ov_mask_tail(&d));d.frag_flags=0;
 d.blend=0x110;assert(!ov_mask_tail(&d));d.blend=0;
 d.depth=2;assert(!ov_mask_tail(&d));d.depth=1;
 d.native=PS2_VK_NATIVE_MESH;assert(!ov_mask_tail(&d));
 ps2_vk_state st={0};st.rt=scene;assert(reject_reason(&st)==REJ_NONE);
 st.rt=display;assert(reject_reason(&st)==REJ_TARGET);st.rt=scene;
 st.tex_rt=1;assert(reject_reason(&st)==REJ_TEXRT);st.tex_rt=0;
 st.zte=st.zwrite=1;assert(reject_reason(&st)==REJ_ZWRITE);st.zwrite=0;assert(reject_reason(&st)==REJ_NONE);
 st.date=1;assert(reject_reason(&st)==REJ_DATE);st.date=0;
 st.abe=1;st.alpha_c=1;assert(reject_reason(&st)==REJ_DSTALPHA);st.abe=0;
 st.fb_mask=1;assert(reject_reason(&st)==REJ_MASK);st.fb_mask=0xff000000;assert(reject_reason(&st)==REJ_NONE);
 st.colclip=1;assert(reject_reason(&st)==REJ_COLCLIP);
 printf("2D target policy: AC%d composition, scene-read hazards and fallback checks passed\n",PS2_GAME_AC4?4:5);
}
''')
    for game in (0, 1):
        exe = Path(directory) / f'target_policy_{game}.exe'
        subprocess.run([str(compiler), '-std=c2x', '-O2', '-Wall', '-Wextra',
                        f'-DPS2_GAME_AC4={game}', '-I' + str(root / 'runtime/include'),
                        str(source), '-o', str(exe)], env=env, check=True)
        subprocess.run([str(exe)], env=env, check=True)
