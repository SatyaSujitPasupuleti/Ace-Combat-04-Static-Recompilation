#include "../runtime/src/rn/rn_int.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#define ps2_log sprite_test_log
void sprite_test_log(const char *,...);
#include "../runtime/src/rn/rn_ac4_sprite_native.h"
static int room=1,fail,draws;static u8 saved[256];static ps2_vk_mesh_block block;
void sprite_test_log(const char *s,...){(void)s;}
void ps2_gs_native_frame(float*x,float*y,float*z){*x=1792;*y=1824;*z=16777215;}
u64 ps2_gs_cur_zbuf(void){return 1ull<<24;}
void rn_screen_flush(void){}
int ps2_vk_mesh_room(u32 a,u32 b,u32 c){assert(a==4&&b==6&&c==1);return room;}
int ps2_vk_mesh_store(const u8*r,u32 n,const u32*i,u32 ni,u32*vb,u32*ib){
    assert(n==4&&ni==6&&i[0]==0&&i[1]==1&&i[2]==2&&i[5]==3);if(fail)return -1;
    memcpy(saved,r,256);*vb=1;*ib=2;return 0;
}
void ps2_vk_draw_mesh(const ps2_vk_state*s,const ps2_vk_mesh_block*b,u32 vb,u32 ib,u32 n,int flags){
    assert(s&&vb==1&&ib==2&&n==6&&flags==PS2_VK_MESH_GSFRAG);block=*b;draws++;
}
static u8 record[sizeof(rn_intent_hdr)+sizeof(ac4_cloud_sprite_mesh)];
static void prepare(ps2_vk_vertex v[6]){
    ac4_cloud_sprite_mesh mesh={0};ac4_cloud_sprite_input *d=&mesh.sprite.input;
    d->matrix[0]=d->matrix[5]=d->matrix[10]=d->matrix[15]=1;d->matrix[12]=d->matrix[13]=2048;
    d->position[2]=d->position[3]=1;d->scale[0]=d->scale[1]=1;d->size=100;
    d->clip[0]=d->lower[0]=1792;d->clip[1]=d->lower[1]=1824;
    d->clip[2]=d->upper[0]=2304;d->clip[3]=d->upper[1]=2272;
    u16 uv[4]={0,0,1024,1024};memcpy(mesh.sprite.output.bytes+8,uv,8);
    assert(ac4_cloud_sprite_project(d,&mesh.sprite.output));mesh.rgba=0x80402010;
    rn_intent_hdr h={0};h.kind=RN_INT_AC4_SPRITE;h.len=sizeof record;
    memcpy(record,&h,sizeof h);memcpy(record+sizeof h,&mesh,sizeof mesh);
    rn_ac4_sprite_mesh_on=1;rn_ac4_sprite_native_intent(record,sizeof record);assert(ac4_native_sprite.active);
    /* First row piece: X 1948..1984? The next 64px boundary is 1984. */
    ps2_vk_mesh_block b;rn_mesh_block_init(&b);ps2_vk_vertex corners[4];ac4_sprite_corners(corners,&b,16777215);
    const unsigned order[6]={0,1,2,0,2,3};for(unsigned i=0;i<6;++i){v[i]=corners[order[i]];v[i].round_uv=PS2_VK_RUV_SPRITE;}
    assert(v[0].x==156 && v[1].x==192 && v[0].y==124.5f);
}
int main(void){
    ps2_vk_state st={0};st.tme=st.fst=1;st.tex_rt=15;ps2_vk_vertex v[6];prepare(v);
    assert(rn_ac4_sprite_native_claim(&st,RN_PRIM_SPRITE,v,6) && draws==1 && block.prog==10);
    float pos[4];memcpy(pos,saved,16);assert(pos[2]==1 && pos[3]==1 && block.p[0]==100);
    /* Native records retain the unscaled T and upload-time snapped T.
       These differ even for integer-sized sprites because GS adds half a pixel. */
    float uv_top[4],uv_bottom[4];memcpy(uv_top,saved+48,16);memcpy(uv_bottom,saved+2*64+48,16);
    assert(uv_top[1]==0 && uv_bottom[1]==64);
    assert(uv_top[2]==-.16f && uv_bottom[2]==64-.16f);
    u32 pieces=ac4_native_sprite.pieces;
    while(ac4_native_sprite.active){
        ps2_vk_mesh_block b;rn_mesh_block_init(&b);ps2_vk_vertex corners[4];ac4_sprite_corners(corners,&b,16777215);
        const unsigned order[6]={0,1,2,0,2,3};for(unsigned i=0;i<6;++i){v[i]=corners[order[i]];v[i].round_uv=PS2_VK_RUV_SPRITE;}
        assert(rn_ac4_sprite_native_claim(&st,RN_PRIM_SPRITE,v,6));
    }
    assert(draws==(int)pieces && !ac4_native_sprite.unused);
    prepare(v);v[2].s+=1;assert(!rn_ac4_sprite_native_claim(&st,RN_PRIM_SPRITE,v,6) && !ac4_native_sprite.active);
    prepare(v);room=0;assert(!rn_ac4_sprite_native_claim(&st,RN_PRIM_SPRITE,v,6));room=1;
    prepare(v);fail=1;assert(!rn_ac4_sprite_native_claim(&st,RN_PRIM_SPRITE,v,6));fail=0;
    prepare(v);st.shuffle_rg=1;assert(!rn_ac4_sprite_native_claim(&st,RN_PRIM_SPRITE,v,6));st.shuffle_rg=0;
    rn_ac4_sprite_native_intent(record,sizeof record-1);assert(!ac4_native_sprite.active);
    ac4_cloud_sprite_mesh damaged;memcpy(&damaged,record+sizeof(rn_intent_hdr),sizeof damaged);damaged.sprite.input.size=NAN;
    memcpy(record+sizeof(rn_intent_hdr),&damaged,sizeof damaged);rn_ac4_sprite_native_intent(record,sizeof record);assert(!ac4_native_sprite.active);
    puts("AC4 sprite GPU: world records, piece geometry/UV checks, malformed input and state/capacity fallback passed");
}
