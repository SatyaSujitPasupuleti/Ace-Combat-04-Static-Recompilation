#include "rn_ac4_sky_haze.h"
#include <assert.h>
#include <stdio.h>
static ac4_sky_qword out[AC4_SKY_HAZE_QWORDS],before[AC4_SKY_HAZE_QWORDS];
int main(void){
 ac4_sky_haze_input h={0};ac4_sky_dome_input *d=&h.sky;
 d->matrix[0]=d->matrix[5]=d->matrix[10]=d->matrix[15]=1;
 d->matrix[12]=d->matrix[13]=2048;d->bounds[0]=d->bounds[1]=2048;d->bounds[2]=256;d->bounds[3]=224;
 for(unsigned k=0;k<32;++k)d->segments[k][3]=1;
 for(unsigned r=0;r<29;++r){d->rings[r][0]=d->rings[r][1]=1;d->colors[r]=0x80000000+r;}
 h.near_distance=4096;h.far_distance=8192;h.depth_scale=4096;h.depth_offset=100;
 size_t n=123;memset(out,0x5a,sizeof out);memcpy(before,out,sizeof out);
 assert(!ac4_sky_haze_build(&h,out,5,&n) && n==123 && !memcmp(out,before,sizeof out));
 assert(ac4_sky_haze_build(&h,out,AC4_SKY_HAZE_QWORDS,&n) && n==AC4_SKY_HAZE_QWORDS);
 assert(out[0].lo==UINT64_C(0x1000000000008004) && out[4].hi==0x4c);
 static ac4_sky_mesh_vertex mesh[AC4_SKY_HAZE_VERTICES],saved[AC4_SKY_HAZE_VERTICES];size_t nv=999;
 memset(mesh,0x5a,sizeof mesh);memcpy(saved,mesh,sizeof mesh);memcpy(before,out,sizeof out);n=123;
 assert(!ac4_sky_haze_plan(&h,out,AC4_SKY_HAZE_QWORDS,&n,mesh,1,&nv));
 assert(n==123 && nv==999 && !memcmp(saved,mesh,sizeof mesh) && !memcmp(before,out,sizeof out));
 assert(ac4_sky_haze_plan(&h,out,AC4_SKY_HAZE_QWORDS,&n,mesh,AC4_SKY_HAZE_VERTICES,&nv));
 assert(nv==AC4_SKY_HAZE_VERTICES);
 for(unsigned band=0;band<6;++band)for(unsigned layer=0;layer<8;++layer){
  size_t i=5+(band*8+layer)*131;
  assert(out[i].lo==UINT64_C(0x1026400000008001));
  assert(out[i+1].lo==(((uint64_t)(16+8*layer)<<32)|0x64));
  assert(out[i+3].lo==d->colors[14+band] && out[i+5].lo==d->colors[15+band]);
  assert((out[i+3].hi>>32)==(layer?100:101) && out[i+3].hi==out[i+5].hi);
  for(unsigned k=0;k<32;++k)for(unsigned v=0;v<4;++v){
   const ac4_sky_mesh_vertex *p=mesh+(band*8+layer)*128+k*4+v;
   const ac4_sky_qword *q=out+i+3+k*4+v;
   assert(p->ring==14+band+(v>=2) && p->segment==(k+(v&1))%32);
   assert(p->xy==(uint32_t)q->hi && p->depth==(uint32_t)(q->hi>>32) && p->color==q->lo && !p->fallback);
  }
 }
 memcpy(before,out,sizeof out);n=777;d->rings[20][0]=NAN;
 assert(!ac4_sky_haze_build(&h,out,AC4_SKY_HAZE_QWORDS,&n) && n==777 && !memcmp(out,before,sizeof out));
 d->rings[20][0]=1;d->matrix[15]=-1;
 assert(ac4_sky_haze_build(&h,out,5,&n) && n==5);
 puts("AC4 haze plan: layers, colour/depth ordering, setup and atomic rejection passed (oracle pending)");
}
