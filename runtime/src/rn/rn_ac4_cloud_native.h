/* Included after rn_ac4_sky_native.h for shared exact GS vertex matching. */
#include "rn_ac4_cloud_packet.h"
int rn_ac4_cloud_mesh_on;
static struct {
    int active;u32 n,tri;
    ac4_cloud_mesh_vertex vertices[1600];
    u64 intents,draws,rejected,unused;
} ac4_native_cloud;
void rn_ac4_cloud_native_init(void){const char *e=getenv("PS2_AC4_CLOUD_MESH");rn_ac4_cloud_mesh_on=e && !strcmp(e,"1");}
void rn_ac4_cloud_native_end(void){
    if(ac4_native_cloud.active)ac4_native_cloud.unused+=ac4_native_cloud.n/2-ac4_native_cloud.tri;
    ac4_native_cloud.active=0;
}
void rn_ac4_cloud_native_intent(const u8 *rec,u32 len){
    rn_ac4_cloud_native_end();if(!rn_ac4_cloud_mesh_on || len<sizeof(rn_intent_hdr)+4)return;
    u32 n;memcpy(&n,rec+sizeof(rn_intent_hdr),4);
    if(!n || n>1600 || n%4 || len!=sizeof(rn_intent_hdr)+4+n*sizeof(ac4_cloud_mesh_vertex))return;
    memcpy(ac4_native_cloud.vertices,rec+sizeof(rn_intent_hdr)+4,n*sizeof(ac4_cloud_mesh_vertex));
    ac4_native_cloud.n=n;ac4_native_cloud.tri=0;ac4_native_cloud.active=1;ac4_native_cloud.intents++;
}
int rn_ac4_cloud_native_claim(const ps2_vk_state *st,int kind,const ps2_vk_vertex *v,int n){
    if(!ac4_native_cloud.active)return 0;
    const char *reason="state";
    if(!st || !v || n!=3 || kind!=RN_PRIM_TRI || !st->tme || st->fst || st->fge)goto reject;
    ps2_vk_mesh_block b;rn_mesh_block_init(&b);
    u32 psm=(u32)((ps2_gs_cur_zbuf()>>24)&15);double zm=psm==1?16777215.0:psm>=2?65535.0:4294967295.0;
    u32 base=(ac4_native_cloud.tri/2)*4,ids[3]={0,1,2},order[3],used=0;
    if(ac4_native_cloud.tri&1){ids[0]=1;ids[1]=2;ids[2]=3;}
    reason="vertex";
    for(unsigned i=0;i<3;++i){int found=0;
        for(unsigned j=0;j<3;++j)if(!(used&(1u<<j))){
            const ac4_cloud_mesh_vertex *p=ac4_native_cloud.vertices+base+ids[j];
            ac4_sky_mesh_vertex ref={0,0,(u32)p->point.output.xyz,(u32)(p->point.output.xyz>>32),p->rgba,0};
            if(ac4_sky_vertex_matches(&ref,v+i,&b,zm) && v[i].s==p->point.output.st[0]
               && v[i].t==p->point.output.st[1] && v[i].q==p->point.output.q){order[i]=ids[j];used|=1u<<j;found=1;break;}
        }
        if(!found)goto reject;
    }
    const float *matrix=ac4_native_cloud.vertices[base+order[0]].point.input.matrix;
    for(unsigned i=1;i<3;++i)if(memcmp(matrix,ac4_native_cloud.vertices[base+order[i]].point.input.matrix,64))goto reject;
    rn_screen_flush();reason="capacity";if(!ps2_vk_mesh_room(3,3,1))goto reject;
    u8 records[192];u32 indices[3]={0,1,2},vb,ib;
    for(unsigned i=0;i<3;++i){
        const ac4_cloud_mesh_vertex *p=ac4_native_cloud.vertices+base+order[i];
        const ac4_cloud_point_input *d=&p->point.input;
        s32 normal[4]={0,(s32)d->depth,0,0},color[4];rn_rgba_lanes(p->rgba,color);
        float uv[4]={d->uv[0],d->uv[1],d->scale,0};rn_mesh_rec(records+i*64,d->position,normal,color,uv);
    }
    reason="store";if(ps2_vk_mesh_store(records,3,indices,3,&vb,&ib)!=0)goto reject;
    memcpy(b.m,matrix,64);b.prog=9;ps2_vk_draw_mesh(st,&b,vb,ib,3,PS2_VK_MESH_GSFRAG);
    ac4_native_cloud.tri++;ac4_native_cloud.draws++;
    if(ac4_native_cloud.tri==ac4_native_cloud.n/2)ac4_native_cloud.active=0;
    return 1;
reject:
    if(ac4_native_cloud.rejected<3)ps2_log("ac4 cloud fallback: %s triangle %u/%u",reason,ac4_native_cloud.tri,ac4_native_cloud.n/2);
    ac4_native_cloud.rejected++;rn_ac4_cloud_native_end();return 0;
}
void rn_ac4_cloud_native_report(void){
    if(ac4_native_cloud.intents)ps2_log("ac4 native clouds: %llu intents, %llu raw GPU triangles, %llu rejected, %llu unused triangles",
        (unsigned long long)ac4_native_cloud.intents,(unsigned long long)ac4_native_cloud.draws,
        (unsigned long long)ac4_native_cloud.rejected,(unsigned long long)ac4_native_cloud.unused);
}
