#include "../runtime/src/rn/rn_int.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#define ps2_log sky_test_log
void sky_test_log(const char *s,...);
#include "../runtime/src/rn/rn_ac4_sky_native.h"
#include "../runtime/src/rn/rn_ac4_cloud_native.h"
static int room=1,fail,draws;static u8 saved[192];static ps2_vk_mesh_block block;
void ps2_log(const char *s,...){(void)s;}
void ps2_gs_native_frame(float*x,float*y,float*z){*x=2;*y=3;*z=16777215;}
u64 ps2_gs_cur_zbuf(void){return 1ull<<24;}
void rn_screen_flush(void){}
int ps2_vk_mesh_room(u32 a,u32 b,u32 c){assert(a==3&&b==3&&c==1);return room;}
int ps2_vk_mesh_store(const u8*r,u32 n,const u32*i,u32 ni,u32*vb,u32*ib){
 assert(n==3&&ni==3&&i[0]==0&&i[1]==1&&i[2]==2);if(fail)return -1;
 memcpy(saved,r,192);*vb=1;*ib=2;return 0;
}
void ps2_vk_draw_mesh(const ps2_vk_state*s,const ps2_vk_mesh_block*b,u32 vb,u32 ib,u32 n,int flags){
 assert(s&&vb==1&&ib==2&&n==3&&flags==PS2_VK_MESH_GSFRAG);block=*b;draws++;
}
static void prepare(ps2_vk_vertex v[3]){
 ac4_native_sky.active=1;ac4_native_sky.tri=0;ac4_native_sky.nv=4;
 for(unsigned i=0;i<4;++i){
  ac4_native_sky.vertices[i]=(ac4_sky_mesh_vertex){3,i,0x14000a0+i*16,123,0x80402010,0};
  ac4_native_sky.input.segments[i][0]=10+i;ac4_native_sky.input.segments[i][3]=1;
  if(i<3){v[i]=(ps2_vk_vertex){0};v[i].x=8.5f+i;v[i].y=17.5f;v[i].z=(float)(123.0/16777215.0);
   v[i].r=16;v[i].g=32;v[i].b=64;v[i].a=128;}
 }
 ac4_native_sky.input.rings[3][0]=.5f;ac4_native_sky.input.rings[3][1]=.75f;
}
int main(void){
 ps2_vk_state st={0};ps2_vk_vertex v[3];prepare(v);
 assert(rn_ac4_sky_native_claim(&st,RN_PRIM_TRI,v,3));assert(draws==1 && block.prog==8);
 float pos[4],uv[4];memcpy(pos,saved,16);memcpy(uv,saved+48,16);
 assert(pos[0]==10 && pos[3]==1 && uv[0]==.5f && uv[1]==.75f);
 rn_ac4_sky_native_end();assert(!rn_ac4_sky_native_claim(&st,RN_PRIM_TRI,v,3));
 prepare(v);v[0].r++;assert(!rn_ac4_sky_native_claim(&st,RN_PRIM_TRI,v,3) && !ac4_native_sky.active);
 prepare(v);room=0;assert(!rn_ac4_sky_native_claim(&st,RN_PRIM_TRI,v,3));room=1;
 prepare(v);fail=1;assert(!rn_ac4_sky_native_claim(&st,RN_PRIM_TRI,v,3));fail=0;
 prepare(v);st.tme=1;assert(!rn_ac4_sky_native_claim(&st,RN_PRIM_TRI,v,3));st.tme=0;
 prepare(v);ps2_vk_vertex t=v[0];v[0]=v[2];v[2]=t;
 assert(rn_ac4_sky_native_claim(&st,RN_PRIM_TRI,v,3));memcpy(pos,saved,16);assert(pos[0]==12);
 assert(draws==2);
 rn_ac4_sky_mesh_on=1;u8 bad[sizeof(rn_intent_hdr)]={0};rn_ac4_sky_native_intent(bad,sizeof bad);
 assert(!ac4_native_sky.active);
 /* Exercise the real serialized intent -> planner -> GPU bridge, rather
  * than only manually injected triangle references. */
 u8 record[sizeof(rn_intent_hdr)+sizeof(ac4_sky_dome_input)]={0};
 ac4_sky_dome_input d={0};d.matrix[0]=d.matrix[5]=d.matrix[10]=d.matrix[15]=1;
 d.matrix[12]=d.matrix[13]=2048;d.bounds[0]=d.bounds[1]=2048;d.bounds[2]=256;d.bounds[3]=224;d.depth=100;
 for(unsigned k=0;k<32;++k)d.segments[k][3]=1;
 for(unsigned r=0;r<29;++r){d.rings[r][0]=d.rings[r][1]=1;d.colors[r]=0x80402010;}
 memcpy(record+sizeof(rn_intent_hdr),&d,sizeof d);
 rn_ac4_sky_native_intent(record,sizeof record);
 assert(ac4_native_sky.active && ac4_native_sky.nv==AC4_SKY_DOME_VERTICES);
 for(unsigned i=0;i<3;++i){v[i]=(ps2_vk_vertex){0};v[i].x=2046.5f;v[i].y=2045.5f;
  v[i].z=(float)(92.0/16777215.0);v[i].r=16;v[i].g=32;v[i].b=64;v[i].a=128;}
 assert(rn_ac4_sky_native_claim(&st,RN_PRIM_TRI,v,3));
 assert(rn_ac4_sky_native_claim(&st,RN_PRIM_TRI,v,3));
 assert(ac4_native_sky.tri==2 && draws==4);
 rn_ac4_sky_native_end();
 rn_ac4_sky_mesh_on=0;rn_ac4_sky_native_intent(record,sizeof record);assert(!ac4_native_sky.active);
 ac4_sky_haze_input h={0};h.sky=d;h.near_distance=4096;h.far_distance=8192;h.depth_offset=100;h.depth_scale=4096;
 u8 haze_record[sizeof(rn_intent_hdr)+sizeof h];rn_intent_hdr hdr={0};hdr.kind=RN_INT_AC4_HAZE;hdr.len=sizeof haze_record;
 memcpy(haze_record,&hdr,sizeof hdr);memcpy(haze_record+sizeof hdr,&h,sizeof h);
 rn_ac4_sky_mesh_on=1;rn_ac4_sky_native_intent(haze_record,sizeof haze_record);
 assert(ac4_native_sky.haze && ac4_native_sky.nv==AC4_SKY_HAZE_VERTICES);
 for(unsigned i=0;i<3;++i)v[i].z=(float)(101.0/16777215.0);
 st.abe=1;assert(rn_ac4_sky_native_claim(&st,RN_PRIM_TRI,v,3));
 assert(ac4_native_sky.haze_intents==1 && ac4_native_sky.haze_draws==1);
 ac4_cloud_mesh_vertex cloud[4]={0};
 for(unsigned k=0;k<4;++k){
  ac4_cloud_point_input *in=&cloud[k].point.input;
  in->matrix[0]=in->matrix[5]=in->matrix[10]=in->matrix[15]=1;
  in->position[0]=10+k;in->position[1]=20;in->position[3]=1;
  in->bounds[2]=256;in->bounds[3]=224;in->depth=123;in->scale=.5f;
  in->uv[0]=.25f;in->uv[1]=.75f;in->write_st=in->write_q=1;
  assert(ac4_cloud_point_project(in,&cloud[k].point.output));cloud[k].rgba=0x80402010;
 }
 u8 cr[sizeof(rn_intent_hdr)+4+sizeof cloud];u32 cn=4;hdr.kind=RN_INT_AC4_CLOUD;hdr.len=sizeof cr;
 memcpy(cr,&hdr,sizeof hdr);memcpy(cr+sizeof hdr,&cn,4);memcpy(cr+sizeof hdr+4,cloud,sizeof cloud);
 rn_ac4_cloud_mesh_on=1;rn_ac4_cloud_native_intent(cr,sizeof cr);prepare(v);
 for(unsigned k=0;k<3;++k){v[k].s=.125f;v[k].t=.375f;v[k].q=.5f;}
 st.tme=1;assert(rn_ac4_cloud_native_claim(&st,RN_PRIM_TRI,v,3));assert(block.prog==9);
 memcpy(pos,saved,16);memcpy(uv,saved+48,16);assert(pos[0]==10 && uv[0]==.25f && uv[2]==.5f);
 rn_ac4_cloud_native_intent(cr,sizeof cr);v[0].q=1;
 assert(!rn_ac4_cloud_native_claim(&st,RN_PRIM_TRI,v,3) && !ac4_native_cloud.active);v[0].q=.5f;
 rn_ac4_cloud_native_intent(cr,sizeof cr);room=0;assert(!rn_ac4_cloud_native_claim(&st,RN_PRIM_TRI,v,3));room=1;
 rn_ac4_cloud_native_intent(cr,sizeof cr);st.tme=0;assert(!rn_ac4_cloud_native_claim(&st,RN_PRIM_TRI,v,3));
 rn_ac4_cloud_native_intent(cr,sizeof cr-1);assert(!ac4_native_cloud.active);
 rn_ac4_sky_native_report();rn_ac4_cloud_native_report();
 puts("AC4 sky/cloud GPU bridge: raw records, triangle order, state/attribute and capacity fallback passed");
}
