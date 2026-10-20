/* AC4-only opt-in sky mesh consumer, included by rn_sky.c. */
#include "rn_ac4_sky_haze.h"
int rn_ac4_sky_mesh_on;
static struct {
    int active;
    size_t nv,tri;
    ac4_sky_dome_input input;
    ac4_sky_mesh_vertex vertices[AC4_SKY_HAZE_VERTICES];
    int haze;
    u64 haze_intents,haze_draws;
    u64 intents,draws,rejected,unused;
} ac4_native_sky;
void rn_ac4_sky_native_init(void) {
    const char *e=getenv("PS2_AC4_SKY_MESH");rn_ac4_sky_mesh_on=e && !strcmp(e,"1");
}
void rn_ac4_sky_native_end(void) {
    if(ac4_native_sky.active) ac4_native_sky.unused+=ac4_native_sky.nv/2-ac4_native_sky.tri;
    ac4_native_sky.active=0;
}
void rn_ac4_sky_native_intent(const u8 *rec,u32 len) {
    rn_ac4_sky_native_end();
    if(!rn_ac4_sky_mesh_on || len<sizeof(rn_intent_hdr)) return;
    rn_intent_hdr header;memcpy(&header,rec,sizeof header);
    ac4_native_sky.haze=header.kind==RN_INT_AC4_HAZE;
    static ac4_sky_qword packet[AC4_SKY_HAZE_QWORDS];size_t count;int ok;
    if(ac4_native_sky.haze){
        if(len!=sizeof header+sizeof(ac4_sky_haze_input))return;
        ac4_sky_haze_input h;memcpy(&h,rec+sizeof header,sizeof h);ac4_native_sky.input=h.sky;
        ok=ac4_sky_haze_plan(&h,packet,AC4_SKY_HAZE_QWORDS,&count,ac4_native_sky.vertices,
                            AC4_SKY_HAZE_VERTICES,&ac4_native_sky.nv);
    }else{
        if(len!=sizeof header+sizeof(ac4_sky_dome_input))return;
        memcpy(&ac4_native_sky.input,rec+sizeof header,sizeof ac4_native_sky.input);
        ok=ac4_sky_dome_plan(&ac4_native_sky.input,packet,AC4_SKY_HAZE_QWORDS,&count,
                            ac4_native_sky.vertices,AC4_SKY_HAZE_VERTICES,&ac4_native_sky.nv);
    }
    if(!ok){ac4_native_sky.rejected++;return;}
    if(ac4_native_sky.haze)ac4_native_sky.haze_intents++;
    ac4_native_sky.tri=0;ac4_native_sky.active=ac4_native_sky.nv!=0;ac4_native_sky.intents++;
}
static int ac4_sky_vertex_matches(const ac4_sky_mesh_vertex *p,const ps2_vk_vertex *v,
                                   const ps2_vk_mesh_block *b,double zm) {
    return v->x==(float)(p->xy&65535u)/16.0f-b->xoff+0.5f
        && v->y==(float)(p->xy>>16)/16.0f-b->yoff+0.5f
        && v->z==(float)fmin((double)p->depth/zm,1.0)
        && v->r==(float)(p->color&255u) && v->g==(float)((p->color>>8)&255u)
        && v->b==(float)((p->color>>16)&255u) && v->a==(float)(p->color>>24);
}
int rn_ac4_sky_native_claim(const ps2_vk_state *st,int kind,const ps2_vk_vertex *v,int n) {
    if(!ac4_native_sky.active) return 0;
    const char *reason="state";
    if(!st || !v || n!=3 || kind!=RN_PRIM_TRI || st->tme || st->fge) goto reject;
    ps2_vk_mesh_block b;rn_mesh_block_init(&b);
    u32 psm=(u32)((ps2_gs_cur_zbuf()>>24)&15u);
    double zm=psm==1?16777215.0:psm>=2?65535.0:4294967295.0;
    size_t base=(ac4_native_sky.tri/2)*4;
    unsigned ids[3];
    if(ac4_native_sky.tri&1){ids[0]=1;ids[1]=2;ids[2]=3;}else{ids[0]=0;ids[1]=1;ids[2]=2;}
    unsigned order[3],used=0;
    for(unsigned i=0;i<3;++i){
        int found=0;
        for(unsigned j=0;j<3;++j)if(!(used&(1u<<j)) && ac4_sky_vertex_matches(ac4_native_sky.vertices+base+ids[j],v+i,&b,zm)){
            order[i]=ids[j];used|=1u<<j;found=1;break;
        }
        if(!found){
            if(ac4_native_sky.rejected<4) {
                const ac4_sky_mesh_vertex *p=ac4_native_sky.vertices+base+ids[i];
                ps2_log("ac4 sky mismatch: intent=%llu triangle=%u/%u vertex=%u expectedXY=%08x depth=%u color=%08x actual=(%.9g,%.9g,%.9g;%.9g,%.9g,%.9g,%.9g) offset=(%.9g,%.9g) zmax=%.9g",
                    (unsigned long long)ac4_native_sky.intents,(unsigned)ac4_native_sky.tri,(unsigned)(ac4_native_sky.nv/2),i,
                    p->xy,p->depth,p->color,v[i].x,v[i].y,v[i].z,v[i].r,v[i].g,v[i].b,v[i].a,b.xoff,b.yoff,zm);
            }
            reason="vertex";goto reject;
        }
    }
    rn_screen_flush();
    reason="capacity";if(!ps2_vk_mesh_room(3,3,1)) goto reject;
    u8 records[192];u32 indices[3]={0,1,2},vb,ib;
    for(unsigned i=0;i<3;++i){
        const ac4_sky_mesh_vertex *p=ac4_native_sky.vertices+base+order[i];
        const ac4_sky_dome_input *d=&ac4_native_sky.input;
        s32 normal[4]={(s32)p->xy,(s32)p->depth,(s32)p->fallback,0},color[4];
        float uv[4]={d->rings[p->ring][0],d->rings[p->ring][1],0,0};rn_rgba_lanes(p->color,color);
        rn_mesh_rec(records+i*64,d->segments[p->segment],normal,color,uv);
    }
    reason="store";if(ps2_vk_mesh_store(records,3,indices,3,&vb,&ib)!=0)goto reject;
    memcpy(b.m,ac4_native_sky.input.matrix,sizeof b.m);b.prog=8;
    ps2_vk_draw_mesh(st,&b,vb,ib,3,PS2_VK_MESH_GSFRAG);
    ac4_native_sky.draws++;ac4_native_sky.tri++;
    if(ac4_native_sky.haze)ac4_native_sky.haze_draws++;
    if(ac4_native_sky.tri==ac4_native_sky.nv/2)ac4_native_sky.active=0;
    return 1;
reject:
    if(ac4_native_sky.rejected<4)ps2_log("ac4 sky fallback: %s, intent=%llu triangle=%u/%u kind=%d n=%d",reason,
        (unsigned long long)ac4_native_sky.intents,(unsigned)ac4_native_sky.tri,(unsigned)(ac4_native_sky.nv/2),kind,n);
    ac4_native_sky.rejected++;rn_ac4_sky_native_end();return 0;
}
void rn_ac4_sky_native_report(void) {
    if(!ac4_native_sky.intents)return;
    ps2_log("ac4 native haze: %llu intents, %llu raw GPU triangles",(unsigned long long)ac4_native_sky.haze_intents,
            (unsigned long long)ac4_native_sky.haze_draws);
    ps2_log("ac4 native sky: %llu intents, %llu raw GPU triangles, %llu rejected, %llu unused triangles",
        (unsigned long long)ac4_native_sky.intents,(unsigned long long)ac4_native_sky.draws,
        (unsigned long long)ac4_native_sky.rejected,(unsigned long long)ac4_native_sky.unused);
}
