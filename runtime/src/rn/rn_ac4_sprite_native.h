/* AC4 cloud sprites: raw world geometry with verified row/piece ordering. */
#include "rn_ac4_cloud_sprite_packet.h"
#include "rn_ac4_cloud_row.h"
int rn_ac4_sprite_mesh_on;
static struct {
    int active;u32 piece,pieces;ac4_cloud_sprite_mesh mesh;
    u64 row[AC4_CLOUD_ROW_BYTES/8],intents,draws,rejected,unused;
} ac4_native_sprite;
void rn_ac4_sprite_native_init(void){const char *e=getenv("PS2_AC4_SPRITE_MESH");rn_ac4_sprite_mesh_on=e && !strcmp(e,"1");}
void rn_ac4_sprite_native_end(void){
    if(ac4_native_sprite.active)ac4_native_sprite.unused+=ac4_native_sprite.pieces-ac4_native_sprite.piece;
    ac4_native_sprite.active=0;
}
void rn_ac4_sprite_native_intent(const u8 *rec,u32 len){
    rn_ac4_sprite_native_end();
    if(!rn_ac4_sprite_mesh_on || len!=sizeof(rn_intent_hdr)+sizeof(ac4_cloud_sprite_mesh))return;
    memcpy(&ac4_native_sprite.mesh,rec+sizeof(rn_intent_hdr),sizeof ac4_native_sprite.mesh);
    ac4_cloud_sprite_output predicted=ac4_native_sprite.mesh.sprite.output;
    if(!ac4_cloud_sprite_project(&ac4_native_sprite.mesh.sprite.input,&predicted) || !(predicted.result>0)
       || memcmp(predicted.bytes+16,ac4_native_sprite.mesh.sprite.output.bytes+16,4)
       || memcmp(predicted.bytes+24,ac4_native_sprite.mesh.sprite.output.bytes+24,8))return;
    const u8 *o=ac4_native_sprite.mesh.sprite.output.bytes;u32 xy,uv0,uv1;u64 xyz;size_t bytes;
    memcpy(&xy,o+16,4);memcpy(&xyz,o+24,8);memcpy(&uv0,o+8,4);memcpy(&uv1,o+12,4);
    if(!ac4_cloud_row_build(xy,xyz,uv0,uv1,ac4_native_sprite.row,sizeof ac4_native_sprite.row,&bytes) || !bytes)return;
    ac4_native_sprite.pieces=(u32)(bytes-16)/32;ac4_native_sprite.piece=0;ac4_native_sprite.active=1;ac4_native_sprite.intents++;
}
static void ac4_sprite_corners(ps2_vk_vertex v[4],const ps2_vk_mesh_block *b,double zm){
    const u64 *r=ac4_native_sprite.row+2+4*ac4_native_sprite.piece;
    float x0=(float)(r[1]&65535u)/16-b->xoff+.5f,x1=(float)(r[3]&65535u)/16-b->xoff+.5f;
    float y0=(float)((r[1]>>16)&65535u)/16-b->yoff+.5f,y1=(float)((r[3]>>16)&65535u)/16-b->yoff+.5f;
    float s0=(float)(r[0]&16383u)/16,s1=(float)(r[2]&16383u)/16;
    float t0=(float)((r[0]>>16)&16383u)/16,t1=(float)((r[2]>>16)&16383u)/16;
    if(x1>x0){float p0=ceilf(x0-.5f),p1=ceilf(x1-.5f);if(p1>p0){
        float ds=(s1-s0)/(x1-x0);s0=s0+(p0-x0)*ds;s1=s1+(p1-x1)*ds;x0=p0;x1=p1;
    }}
    u32 color=ac4_native_sprite.mesh.rgba;
    for(unsigned k=0;k<4;++k){int right=k==1 || k==2,bottom=k>=2;memset(v+k,0,sizeof *v);
        v[k].x=right?x1:x0;v[k].y=bottom?y1:y0;v[k].z=(float)fmin((double)(u32)(r[3]>>32)/zm,1);
        v[k].r=color&255;v[k].g=(color>>8)&255;v[k].b=(color>>16)&255;v[k].a=color>>24;
        v[k].s=right?s1:s0;v[k].t=bottom?t1:t0;v[k].q=1;
    }
}
int rn_ac4_sprite_native_claim(const ps2_vk_state *st,int kind,const ps2_vk_vertex *v,int n){
    if(!ac4_native_sprite.active)return 0;
    const char *reason="state";
    if(!st || !v || kind!=RN_PRIM_SPRITE || n!=6 || !st->tme || !st->fst || st->fge || st->tex_lod || st->shuffle_rg)goto reject;
    ps2_vk_mesh_block b;rn_mesh_block_init(&b);u32 psm=(ps2_gs_cur_zbuf()>>24)&15;
    double zm=psm==1?16777215.0:psm>=2?65535.0:4294967295.0;
    ps2_vk_vertex expected[4];ac4_sprite_corners(expected,&b,zm);
    const u32 indices[6]={0,1,2,0,2,3};reason="vertex";
    for(unsigned i=0;i<6;++i)if(memcmp(v+i,expected+indices[i],10*sizeof(float)) || v[i].round_uv!=v[0].round_uv)goto reject;
    rn_screen_flush();reason="capacity";if(!ps2_vk_mesh_room(4,6,1))goto reject;
    const ac4_cloud_sprite_input *d=&ac4_native_sprite.mesh.sprite.input;
    u8 records[256];u32 vb,ib;u32 depth;memcpy(&depth,ac4_native_sprite.mesh.sprite.output.bytes+28,4);
    /* Match ps2_video's upload-time row snapping at scale > 1. Keep both
       UV variants so the shader can select using the actual render scale. */
    float t0=expected[0].t,t1=expected[2].t;
    float y0=expected[0].y,y1=expected[2].y,p0=ceilf(y0-.5f),p1=ceilf(y1-.5f);
    if(y1>y0 && p1>p0){float dt=(t1-t0)/(y1-y0);
        t0=t0+(p0-y0)*dt;t1=t1+(p1-y1)*dt;
    }
    for(unsigned k=0;k<4;++k){
        s32 normal[4]={k==1||k==2,k>=2,(s32)ac4_native_sprite.piece,(s32)v[0].round_uv},color[4];
        float uv[4]={expected[k].s,expected[k].t,k>=2?t1:t0,0};rn_rgba_lanes(ac4_native_sprite.mesh.rgba,color);
        rn_mesh_rec(records+k*64,d->position,normal,color,uv);
    }
    reason="store";if(ps2_vk_mesh_store(records,4,indices,6,&vb,&ib)!=0)goto reject;
    memcpy(b.m,d->matrix,64);memcpy(b.c,d->scale,16);memcpy(b.c+4,d->lower,16);
    memcpy(b.c+8,d->upper,16);memcpy(b.c+12,d->clip,16);b.p[0]=d->size;b.p[1]=(float)depth;b.prog=10;
    ps2_vk_draw_mesh(st,&b,vb,ib,6,PS2_VK_MESH_GSFRAG);
    ac4_native_sprite.draws++;if(++ac4_native_sprite.piece==ac4_native_sprite.pieces)ac4_native_sprite.active=0;return 1;
reject:
    if(ac4_native_sprite.rejected<3)ps2_log("ac4 sprite fallback: %s piece %u/%u kind=%d n=%d tme=%d fst=%d fog=%d lod=%u rt=%u shuffle=%d",reason,ac4_native_sprite.piece,ac4_native_sprite.pieces,
        kind,n,st?st->tme:0,st?st->fst:0,st?st->fge:0,st?st->tex_lod:0,st?st->tex_rt:0,st?st->shuffle_rg:0);
    ac4_native_sprite.rejected++;rn_ac4_sprite_native_end();return 0;
}
void rn_ac4_sprite_native_report(void){
    if(ac4_native_sprite.intents)ps2_log("ac4 native sprites: %llu intents, %llu raw GPU pieces, %llu rejected, %llu unused pieces",
        (unsigned long long)ac4_native_sprite.intents,(unsigned long long)ac4_native_sprite.draws,
        (unsigned long long)ac4_native_sprite.rejected,(unsigned long long)ac4_native_sprite.unused);
}
