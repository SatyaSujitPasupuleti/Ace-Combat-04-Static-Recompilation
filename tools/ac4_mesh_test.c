/* Exercise raw model submission and all fallback exits without a GPU/game. */
#include <assert.h>
#include <stdio.h>
#include "ps2_vk_mesh_blocks.h"
#include "ps2_vk_mip_chain.h"
#include "../runtime/src/rn/rn_ac4_mesh.c"
int rn_ac4_model_mesh_on=1,rn_ac4_model_material_on,rn_ac4_model_texture_on;
static int material_ok=1,texture_fail;static u32 tx,ty,tw,th;
static u32 texture_clamp;
static u32 texture_w=64,texture_h=32;
int ps2_vk_mesh_material_ok(const ps2_vk_state *s) {return material_ok && !s->tex_mip;}
void ps2_vk_texture_size(u32 i,u32 *w,u32 *h) {assert(i==77);*w=texture_w;*h=texture_h;}
const u8 *ps2_vk_texture_pixels(u32 i) {(void)i;return NULL;}
u32 ps2_vk_texture_native_wrap(u32 i,u32 x,u32 y,u32 w,u32 h,u32 clamp) {assert(i==77);tx=x;ty=y;tw=w;th=h;texture_clamp=clamp;return texture_fail?~(u32)0:88;}
u32 ps2_vk_texture_native_atlas(u32 src,u32 bw,u32 bh,u32 x,u32 y,u32 w,u32 h,u32 mxl,u32 clamp) {
    assert(bw==32 && bh==16 && mxl==3);return ps2_vk_texture_native_wrap(src,x,y,w,h,clamp);
}
static int room=1,store_fail,draws,flushes,stores;
static u8 saved[192];
static ps2_vk_mesh_block block;
void ps2_log(const char *fmt,...) {(void)fmt;}
void ps2_gs_native_frame(float *x,float *y,float *z) {*x=2;*y=3;*z=16777215;}
u64 ps2_gs_cur_zbuf(void) {return 1ull<<24;}
void rn_screen_flush(void) {flushes++;}
int ps2_vk_mesh_room(u32 v,u32 i,u32 d) {assert(v==3&&i==3&&d==1);return room;}
int ps2_vk_mesh_store(const u8 *r,u32 n,const u32 *idx,u32 ni,u32 *vb,u32 *ib) {
    assert(flushes>stores);stores++;
    if(store_fail)return -1;
    assert(n==3&&ni==3&&idx[0]==0&&idx[1]==1&&idx[2]==2);
    memcpy(saved,r,192);*vb=4;*ib=5;return 0;
}
void ps2_vk_draw_mesh(const ps2_vk_state *s,const ps2_vk_mesh_block *b,u32 vb,u32 ib,u32 n,int flags) {
    assert(s->tex_index==77&&vb==4&&ib==5&&n==3&&flags==(rn_ac4_model_material_on?0:PS2_VK_MESH_GSFRAG));
    block=*b;draws++;
}
int main(void) {
    ps2_vk_mip_layout ml={0},before;
    assert(ps2_vk_mip_layout_make(&ml,32,24,16,16,8,8,8,8,3));
    assert(ml.levels==4 && ml.bytes==(64+16+4+1)*4u);
    assert(ml.sx[0]==8 && ml.sy[0]==8 && ml.sx[1]==20 && ml.sy[1]==4);
    assert(ml.sx[2]==18 && ml.sy[2]==10 && ml.sx[3]==17 && ml.sy[3]==13);
    u8 source[32*24*4],dest[(64+16+4+1)*4+8];
    for(u32 y=0;y<24;y++)for(u32 x=0;x<32;x++)for(u32 c=0;c<4;c++)source[(y*32+x)*4+c]=(u8)(y*31+x+c);
    memset(dest,0xCD,sizeof dest);ps2_vk_mip_extract(dest,source,32,&ml);
    for(u32 lv=0;lv<4;lv++)for(u32 y=0;y<ml.h[lv];y++)for(u32 x=0;x<ml.w[lv];x++)for(u32 c=0;c<4;c++)
        assert(dest[ml.offset[lv]+(y*ml.w[lv]+x)*4+c]==(u8)((ml.sy[lv]+y)*31+ml.sx[lv]+x+c));
    for(size_t i=ml.bytes;i<sizeof dest;i++)assert(dest[i]==0xCD);
    before=ml;
    assert(!ps2_vk_mip_layout_make(&ml,32,24,16,16,7,8,8,8,3));
    assert(!ps2_vk_mip_layout_make(&ml,32,24,16,16,8,8,7,8,3));
    assert(!ps2_vk_mip_layout_make(&ml,31,24,16,16,8,8,8,8,3));
    assert(!ps2_vk_mip_layout_make(&ml,32,24,16,16,8,8,8,8,4));
    assert(!ps2_vk_mip_layout_make(&ml,32,24,16,16,16,8,8,8,3));
    assert(!memcmp(&before,&ml,sizeof ml));

    assert(ps2_vk_mesh_mip_supported(0,0));
    assert(ps2_vk_mesh_mip_supported(0,1));
    assert(ps2_vk_mesh_mip_supported(0x003F282Bu,1));
    assert(!ps2_vk_mesh_mip_supported(0x003F282Bu,0));
    for(u32 levels=0;levels<8;levels++)for(u32 mode=0;mode<8;mode++) {
        u32 mip=(1u<<20)|(mode<<3)|levels;
        int valid=levels>=1 && levels<=6 && mode>=2 && mode<=5;
        assert(ps2_vk_mesh_mip_supported(mip,1)==valid);
        assert(!ps2_vk_mesh_mip_supported(mip|(1u<<22),1));
        assert(!ps2_vk_mesh_mip_supported(mip&~(1u<<20),1) || !(mip&~(1u<<20)));
    }
    /* Actual arena helper: reuse only exact model constants in this arena. */
    ps2_vk_mesh_block arena[3],input={0};u32 used=0;int reused;
    input.prog=6;input.m[0]=1;
    assert(ps2_vk_mesh_block_store(arena,&used,3,&input,1,&reused)==0 && used==1 && !reused);
    assert(ps2_vk_mesh_block_store(arena,&used,3,&input,1,&reused)==0 && used==1 && reused);
    input.xoff=2;
    assert(ps2_vk_mesh_block_store(arena,&used,3,&input,1,&reused)==1 && used==2 && !reused);
    assert(arena[0].xoff==0); /* Previously recorded draws are immutable. */
    input.fog1=3;
    assert(ps2_vk_mesh_block_store(arena,&used,3,&input,1,&reused)==2 && used==3);
    assert(ps2_vk_mesh_block_store(arena,&used,3,&input,1,&reused)==2 && reused);
    input.zmax=65535;
    assert(ps2_vk_mesh_block_store(arena,&used,3,&input,1,&reused)==~(u32)0 && used==3 && !reused);
    used=0;
    assert(ps2_vk_mesh_block_store(arena,&used,3,&input,1,&reused)==0 && !reused);
    assert(ps2_vk_mesh_block_store(arena,&used,3,&input,0,&reused)==1 && !reused);
    used=0;input.prog=5;
    assert(ps2_vk_mesh_block_store(arena,&used,3,&input,1,&reused)==0);
    assert(ps2_vk_mesh_block_store(arena,&used,3,&input,1,&reused)==1 && !reused);
    ps2_vf c[32]={0},r[3][4]={0};rn_ac4_model_vertex p[3]={0};ps2_vk_vertex v[3]={0};
    ps2_vk_state st={0};st.tex_index=77;
    c[23].u[1]=2;c[0].f[0]=1;c[15].f[2]=.25f;c[30].f[0]=99;c[30].f[1]=2;
    for(int i=0;i<3;i++) {
        r[i][2].f[0]=10+i;r[i][2].f[1]=20;r[i][2].f[2]=30;r[i][2].f[3]=1;
        r[i][3].s[0]=4096;r[i][0].s[0]=4096;r[i][0].s[1]=8192;r[i][0].s[2]=4096;
        for(int k=0;k<4;k++) r[i][1].s[k]=p[i].rgba[k]=8<<k;
        p[i].xyzf[0]=160+16*i;p[i].xyzf[1]=320;p[i].xyzf[2]=480;p[i].xyzf[3]=4080;
        p[i].stq[0]=1;p[i].stq[1]=2;p[i].stq[2]=1;
        v[i].x=8.5f+i;v[i].y=17.5f;v[i].z=(float)(30.0/16777215.0);v[i].fog=1;
        v[i].s=1;v[i].t=2;v[i].q=1;v[i].r=8;v[i].g=16;v[i].b=32;v[i].a=64;
    }
    rn_ac4_model_mesh_on=0;rn_ac4_mesh_prepare(c,r,p,11);assert(!pending.active);
    rn_ac4_model_mesh_on=1;
    rn_ac4_mesh_prepare(c,r,p,3);assert(!pending.active); /* flat shading */
    rn_ac4_mesh_prepare(c,r,p,267);assert(!pending.active); /* fixed UV */
    rn_ac4_mesh_prepare(c,r,p,11);assert(rn_ac4_mesh_claim(&st,RN_PRIM_TRI,v,3));
    assert(draws==1&&block.prog==6&&block.mode==1&&block.p[2]==.25f&&block.fog0==99);
    float pos[4],uv[4];s32 normal[4];
    memcpy(pos,saved,16);memcpy(normal,saved+16,16);memcpy(uv,saved+48,16);
    assert(pos[0]==10&&pos[1]==20&&pos[2]==30&&pos[3]==1); /* model, NOT screen coordinates */
    assert(normal[0]==4096&&uv[0]==1&&uv[1]==2&&uv[2]==1);
    assert(!rn_ac4_mesh_claim(&st,RN_PRIM_TRI,v,3)); /* consumed once */
    rn_ac4_mesh_prepare(c,r,p,11);v[1].q=.5f;assert(!rn_ac4_mesh_claim(&st,RN_PRIM_TRI,v,3));
    v[1].q=1;assert(!rn_ac4_mesh_claim(&st,RN_PRIM_TRI,v,3)); /* later pass can't reclaim it */
    rn_ac4_mesh_prepare(c,r,p,11);st.fst=1;assert(!rn_ac4_mesh_claim(&st,RN_PRIM_TRI,v,3));st.fst=0;
    rn_ac4_mesh_prepare(c,r,p,11);room=0;assert(!rn_ac4_mesh_claim(&st,RN_PRIM_TRI,v,3));room=1;
    rn_ac4_mesh_prepare(c,r,p,11);store_fail=1;assert(!rn_ac4_mesh_claim(&st,RN_PRIM_TRI,v,3));store_fail=0;
    rn_ac4_mesh_prepare(c,r,p,11);rn_ac4_mesh_end();assert(!rn_ac4_mesh_claim(&st,RN_PRIM_TRI,v,3));
    assert(draws==1&&mesh_stats.attributes==1&&mesh_stats.state==1&&mesh_stats.capacity==2);
    rn_ac4_model_material_on=1;rn_ac4_mesh_prepare(c,r,p,11);assert(rn_ac4_mesh_claim(&st,RN_PRIM_TRI,v,3));assert(draws==2);
    ps2_vk_state tex={0},converted={0};tex.tme=1;tex.tex_index=77;tex.tex_w=64;tex.tex_h=32;
    ps2_vk_state original=tex;
    assert(model_texture(&tex,&converted)&&converted.tex_index==88&&!converted.native_region);
    assert(tx==0&&ty==0&&tw==64&&th==32&&!memcmp(&tex,&original,sizeof tex));
    assert(texture_clamp==0);
    tex.wms=2;tex.minu=4;tex.maxu=19;tex.wmt=2;tex.minv=8;tex.maxv=31;
    assert(model_texture(&tex,&converted)&&converted.native_region&&tx==4&&ty==8&&tw==16&&th==24);
    assert(texture_clamp==3);
    original=converted;tex.maxv=40;assert(!model_texture(&tex,&converted)&&!memcmp(&converted,&original,sizeof converted));tex.maxv=31;
    original=converted;texture_fail=1;assert(!model_texture(&tex,&converted)&&!memcmp(&converted,&original,sizeof converted));texture_fail=0;
    tex.minu=20;assert(!model_texture(&tex,&converted));tex.minu=64;tex.maxu=100;assert(!model_texture(&tex,&converted));tex.minu=4;tex.maxu=19;
    tex.tex_rt=1;assert(!model_texture(&tex,&converted));tex.tex_rt=0;
    tex.tex_lod=1;assert(!model_texture(&tex,&converted));tex.tex_lod=0;
    material_ok=0;assert(!model_texture(&tex,&converted));material_ok=1;
    /* Packed mip levels must never become part of the generated base image. */
    tex=(ps2_vk_state){0};tex.tme=1;tex.tex_index=77;tex.tex_w=32;tex.tex_h=16;
    tex.tex_mip=(1u<<20)|3u|(5u<<3);texture_w=64;texture_h=24;
    atlas_texture_on=0;original=converted;assert(!model_texture(&tex,&converted)&&!memcmp(&converted,&original,sizeof converted));
    atlas_texture_on=1;
    original=tex;assert(model_texture(&tex,&converted));
    assert(tx==0&&ty==0&&tw==32&&th==16&&!converted.tex_mip&&!memcmp(&tex,&original,sizeof tex));
    tex.wms=2;tex.minu=8;tex.maxu=15;
    assert(model_texture(&tex,&converted)&&tx==8&&tw==8&&th==16&&converted.native_region);
    assert(texture_clamp==1);
    original=converted;tex.maxu=40;assert(!model_texture(&tex,&converted)&&!memcmp(&converted,&original,sizeof converted));tex.maxu=15;
    texture_fail=1;assert(!model_texture(&tex,&converted)&&!memcmp(&converted,&original,sizeof converted));texture_fail=0;
    texture_h=25;assert(!model_texture(&tex,&converted));texture_h=24;
    tex.tex_w=32.5f;assert(!model_texture(&tex,&converted));tex.tex_w=NAN;assert(!model_texture(&tex,&converted));tex.tex_w=32;
    tex.tex_mip=1;assert(!model_texture(&tex,&converted));tex.tex_mip=(1u<<20)|7u|(5u<<3);assert(!model_texture(&tex,&converted));
    tex.tex_mip=(1u<<20)|3u|(5u<<3);texture_w=64;texture_h=24;
    supplied_mips_on=1;atlas_texture_on=0;original=tex;
    assert(model_texture(&tex,&converted));
    assert(converted.tex_mip==tex.tex_mip && converted.native_region==3 && texture_stats.supplied==1);
    assert(!memcmp(&tex,&original,sizeof tex));
    tex.tex_mip|=1u<<21;supplied_filter=1;
    assert(model_texture(&tex,&converted)&&converted.native_region==7);
    supplied_filter=2;assert(model_texture(&tex,&converted)&&converted.native_region==15);
    tex.tex_mip&=~(1u<<21);assert(model_texture(&tex,&converted)&&converted.native_region==3);
    tex.tex_mip|=1u<<21;tex.tex_mip=(tex.tex_mip&~(7u<<3))|(4u<<3);
    assert(model_texture(&tex,&converted)&&converted.native_region==3);
    supplied_filter=0;
    original=converted;texture_fail=1;
    assert(!model_texture(&tex,&converted)&&!memcmp(&converted,&original,sizeof converted));
    texture_fail=0;supplied_mips_on=0;
    texture_w=64;texture_h=32;
    rn_ac4_mesh_prepare(c,r,p,11);rn_ac4_mesh_prepare_secondary(c,r,p,11);
    assert(rn_ac4_mesh_claim(&st,RN_PRIM_TRI,v,3)&&block.mode==1&&rn_ac4_mesh_pending());
    assert(rn_ac4_mesh_claim(&st,RN_PRIM_TRI,v,3)&&block.mode==2&&!rn_ac4_mesh_pending());
    rn_ac4_mesh_prepare(c,r,p,11);assert(rn_ac4_mesh_claim(&st,RN_PRIM_TRI,v,3)&&block.mode==1);
    rn_ac4_model_vertex rejected[3];memcpy(rejected,p,sizeof rejected);rejected[0].rgba[0]=255;
    rn_ac4_mesh_prepare(c,r,rejected,11);rn_ac4_mesh_prepare_secondary(c,r,p,11);
    assert(rn_ac4_mesh_claim(&st,RN_PRIM_TRI,v,3)&&block.mode==2); /* primary ADC can suppress its draw */
    rn_ac4_mesh_prepare(c,r,p,11);rn_ac4_mesh_prepare_secondary(c,r,p,11);rn_ac4_mesh_end();
    assert(!rn_ac4_mesh_pending()&&!rn_ac4_mesh_claim(&st,RN_PRIM_TRI,v,3));
    /* Terrain keeps decoded floating positions/colours and independent UV streams. */
    rn_ac4_mesh_end();
    for(int i=0;i<3;i++)for(int k=0;k<4;k++) {
        r[i][0].f[k]=(float)(k+1);r[i][3].f[k]=(float)(k+5);
        r[i][1].f[k]=(float)p[i].rgba[k];
    }
    rn_ac4_mesh_terrain_append(c,r,p,0,0);rn_ac4_mesh_terrain_append(c,r,p,1,1);
    assert(rn_ac4_mesh_pending()&&rn_ac4_mesh_claim(&st,RN_PRIM_TRI,v,3));
    assert(block.prog==7&&block.mode==0&&terrain_draws[0]==1);
    memcpy(uv,saved+48,16);memcpy(normal,saved+16,16);
    assert(uv[0]==1&&uv[2]==3&&!memcmp(normal,r[0][1].s,16));
    assert(rn_ac4_mesh_claim(&st,RN_PRIM_TRI,v,3)&&block.prog==7&&block.mode==1);
    memcpy(uv,saved+48,16);assert(uv[0]==5&&uv[2]==7&&terrain_draws[1]==1&&terrain_clipped_draws[1]==1&&!terrain_clipped_draws[0]);
    assert(!rn_ac4_mesh_pending());
    rn_ac4_mesh_terrain_append(c,r,p,0,0);room=0;
    assert(!rn_ac4_mesh_claim(&st,RN_PRIM_TRI,v,3)&&!rn_ac4_mesh_pending());room=1;
    for(int i=0;i<17;i++)rn_ac4_mesh_terrain_append(c,r,p,0,0);
    assert(terrain_count==16);rn_ac4_mesh_end();assert(!rn_ac4_mesh_pending());
    c[23].u[1]=2;
    rn_ac4_mesh_prepare_cockpit(c,r,p,11);assert(rn_ac4_mesh_cockpit_pending());assert(rn_ac4_mesh_claim(&st,RN_PRIM_TRI,v,3));
    assert(block.prog==6 && block.mode==5 && cockpit_draws==1);
    rn_ac4_mesh_prepare(c,r,p,11);assert(!rn_ac4_mesh_cockpit_pending());assert(rn_ac4_mesh_claim(&st,RN_PRIM_TRI,v,3));
    assert(block.mode==1 && cockpit_draws==1); /* No cockpit mode leaks to ordinary models. */
    rn_ac4_mesh_prepare_cockpit(c,r,p,11);rn_ac4_mesh_prepare_secondary(c,r,p,11);
    assert(rn_ac4_mesh_claim(&st,RN_PRIM_TRI,v,3) && block.mode==5);
    assert(rn_ac4_mesh_claim(&st,RN_PRIM_TRI,v,3) && block.mode==6);
    assert(cockpit_draws==3 && cockpit_secondary_draws==1);
    rn_ac4_mesh_cockpit_append(c,r,p,0);rn_ac4_mesh_cockpit_append(c,r,p,1);
    assert(rn_ac4_mesh_cockpit_pending());
    assert(rn_ac4_mesh_claim(&st,RN_PRIM_TRI,v,3) && block.mode==5 && block.prog==6);
    assert(rn_ac4_mesh_claim(&st,RN_PRIM_TRI,v,3) && block.mode==6 && block.prog==6);
    assert(cockpit_clipped_draws==2 && cockpit_draws==5 && cockpit_secondary_draws==2);
    rn_ac4_mesh_terrain_append(c,r,p,0,0);
    assert(!rn_ac4_mesh_cockpit_pending());
    assert(rn_ac4_mesh_claim(&st,RN_PRIM_TRI,v,3) && block.mode==0 && block.prog==7);
    assert(cockpit_clipped_draws==2); /* Reused queue slots cannot retain cockpit identity. */
    puts("AC4 raw GPU submission: all synthetic checks passed");return 0;
}
