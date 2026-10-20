/* Synthetic tests of the production bridge, without game data or a GPU. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../runtime/src/rn/rn_screen.c"

int rn_vu_kicking, rn_census_native_prog;
void (*ps2_vk_before_record)(void);
static int available=1, room=1, store_fail, draws, fallbacks, stores;
static u8 saved[64*12];
static u32 saved_idx[12], saved_nv, saved_ni;
static ps2_vk_state last_state;
void ps2_log(const char *fmt, ...) { (void)fmt; }
const rn_emitter *rn_emitter_get(u32 id) { (void)id; return NULL; }
void ps2_gs_native_frame(float *x,float *y,float *z) { *x=0; *y=0; *z=16777215; }
int ps2_vk_mesh_available(void) { return available; }
int ps2_vk_mesh_room(u32 v,u32 i,u32 d) { assert(v>0 && i>0 && d==1); return available && room; }
int ps2_vk_mesh_store(const u8 *r,u32 n,const u32 *idx,u32 ni,u32 *vb,u32 *ib) {
    stores++;
    if(store_fail) return -1;
    assert(n<=12 && ni<=12);
    memcpy(saved,r,n*64); memcpy(saved_idx,idx,ni*4); saved_nv=n; saved_ni=ni;
    *vb=10; *ib=20; return 0;
}
void ps2_vk_draw_mesh_topo(const ps2_vk_state *st,const ps2_vk_mesh_block *b,
                         u32 vb,u32 ib,u32 ni,int flags,int topo) {
    if(ps2_vk_before_record) ps2_vk_before_record();
    assert(b->prog==5 && vb==10 && ib==20 && ni==saved_ni);
    assert(flags==PS2_VK_MESH_GSFRAG && topo==PS2_VK_TRIANGLES);
    last_state=*st; draws++;
}
void ps2_vk_draw(int topo,const ps2_vk_state *st,const ps2_vk_vertex *v,int n) {
    if(ps2_vk_before_record) ps2_vk_before_record();
    assert(topo==PS2_VK_TRIANGLES && n==3 && v && st);
    fallbacks++;
}
static void reset(void) {
    assert(!run.n); memset(&run,0,sizeof run);
    available=room=1; store_fail=draws=fallbacks=stores=0;
}
int main(void) {
    ps2_vk_state st={0}; ps2_vk_vertex v[3]={0};
    for(int i=0;i<3;i++) {
        v[i].x=10.25f+i; v[i].y=-3.5f+i; v[i].z=.375f;
        v[i].r=128+i; v[i].g=32; v[i].b=255; v[i].a=128;
        v[i].s=.125f; v[i].t=.25f; v[i].q=.5f; v[i].fog=.75f; v[i].round_uv=1;
    }
    _putenv_s("PS2_AC4_MESH_BRIDGE", "off"); rn_screen_init();
    assert(!rn_screen_claim_prim(&st,RN_PRIM_TRI,v,3));
    _putenv_s("PS2_AC4_MESH_BRIDGE", "triangles"); rn_screen_init();
    assert(!rn_screen_claim_prim(&st,RN_PRIM_SPRITE,v,3));
    assert(!rn_screen_claim_prim(&st,RN_PRIM_TRI,v,2));
    available=0; assert(!rn_screen_claim_prim(&st,RN_PRIM_TRI,v,3)); available=1;
    v[0].r=128.5f; assert(!rn_screen_claim_prim(&st,RN_PRIM_TRI,v,3));
    v[0].r=NAN; assert(!rn_screen_claim_prim(&st,RN_PRIM_TRI,v,3)); v[0].r=128;
    rn_vu_kicking=1; /* AC4 path must not rely on AC5 producer classification. */
    assert(rn_screen_claim_prim(&st,RN_PRIM_TRI,v,3));
    assert(rn_screen_claim_prim(&st,RN_PRIM_TRI,v,3));
    rn_screen_flush(); assert(draws==1 && saved_nv==6 && saved_ni==6);
    for(int i=0;i<6;i++) {
        float pos[4],uv[4]; s32 col[4],nrm[4];
        memcpy(pos,saved+i*64,16); memcpy(nrm,saved+i*64+16,16);
        memcpy(col,saved+i*64+32,16); memcpy(uv,saved+i*64+48,16);
        assert(pos[0]==v[i%3].x && pos[1]==v[i%3].y && pos[2]==v[i%3].z);
        assert(col[0]==(s32)v[i%3].r && col[2]==255 && nrm[0]==1);
        assert(uv[0]==.125f && uv[1]==.25f && uv[2]==.5f && uv[3]==.75f);
        assert(saved_idx[i]==(u32)i);
    }
    reset();
    assert(rn_screen_claim_prim(&st,RN_PRIM_TRI,v,3));
    ps2_vk_state next=st; next.emitter=99;
    assert(rn_screen_claim_prim(&next,RN_PRIM_TRI,v,3));
    assert(draws==1 && last_state.emitter==st.emitter);
    rn_screen_flush(); assert(draws==2 && last_state.emitter==99);
    reset();
    assert(rn_screen_claim_prim(&st,RN_PRIM_TRI,v,3));
    v[0].r=.5f; assert(!rn_screen_claim_prim(&st,RN_PRIM_TRI,v,3));
    ps2_vk_draw(PS2_VK_TRIANGLES,&st,v,3); assert(draws==1 && fallbacks==1 && !run.n);
    v[0].r=128;
    reset(); room=0;
    assert(rn_screen_claim_prim(&st,RN_PRIM_TRI,v,3)); rn_screen_flush();
    assert(!draws && !stores && fallbacks==1); /* Includes block exhaustion. */
    reset(); store_fail=1;
    assert(rn_screen_claim_prim(&st,RN_PRIM_TRI,v,3)); rn_screen_flush();
    assert(!draws && stores==1 && fallbacks==1);
    reset(); assert(rn_screen_claim_prim(&st,RN_PRIM_TRI,v,3)); available=0;
    rn_screen_flush(); assert(!draws && fallbacks==1);
    reset(); room=0;
    for(unsigned i=0;i<RUN_MAX+1;i++) assert(rn_screen_claim_prim(&st,RN_PRIM_TRI,v,3));
    rn_screen_flush(); assert(!draws && fallbacks==RUN_MAX+1 && !run.n);
    puts("AC4 mesh bridge: all synthetic checks passed");
    return 0;
}
