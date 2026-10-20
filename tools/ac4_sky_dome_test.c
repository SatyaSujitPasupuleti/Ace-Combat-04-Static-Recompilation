#include "rn_ac4_sky_dome.h"
#include <assert.h>
#include <stdio.h>
static ac4_sky_qword out[AC4_SKY_DOME_QWORDS],before[AC4_SKY_DOME_QWORDS];
int main(void){
 ac4_sky_dome_input d={0};d.matrix[0]=d.matrix[5]=d.matrix[10]=d.matrix[15]=1;
 d.matrix[12]=d.matrix[13]=2048;d.bounds[0]=d.bounds[1]=2048;d.bounds[2]=256;d.bounds[3]=224;
 for(unsigned k=0;k<32;++k)d.segments[k][3]=1;
 for(unsigned r=0;r<29;++r){d.rings[r][0]=d.rings[r][1]=1;d.colors[r]=0x80000000+r;}
 d.depth=100;memset(out,0x5a,sizeof out);memcpy(before,out,sizeof out);size_t n=999;
 assert(!ac4_sky_dome_build(&d,out,3,&n) && n==999 && !memcmp(out,before,sizeof out));
 assert(ac4_sky_dome_build(&d,out,AC4_SKY_DOME_QWORDS,&n) && n==AC4_SKY_DOME_QWORDS);
 assert(out[0].lo==UINT64_C(0x1000000000000002) && out[1].hi==0x4e && out[2].hi==0x47);
 for(unsigned band=0;band<28;++band){size_t i=3+band*131;
  assert(out[i].lo==UINT64_C(0x1400000000000001) && out[i+1].lo==12);
  assert(out[i+1].hi==UINT64_C(0x5a5a5a5a5a5a5a5a));
  assert((out[i+2].lo&32767)==32 && out[i+2].hi==0x5151d1d1);
  assert(out[i+3].lo==d.colors[band] && out[i+5].lo==d.colors[band+1]);
  assert(out[i+3].hi==(((uint64_t)(band<15?92:100)<<32)|0x80008000));
  assert(out[i+5].hi==(((uint64_t)(band+1<15?92:100)<<32)|0x80008000));
 }
 /* Mesh references must describe exactly the packet's vertex stream. */
 static ac4_sky_mesh_vertex mesh[AC4_SKY_DOME_VERTICES],saved[AC4_SKY_DOME_VERTICES];
 size_t nv=777;memset(mesh,0x33,sizeof mesh);memcpy(saved,mesh,sizeof mesh);
 memcpy(before,out,sizeof out);n=123;
 assert(!ac4_sky_dome_plan(&d,out,AC4_SKY_DOME_QWORDS,&n,mesh,1,&nv));
 assert(n==123 && nv==777 && !memcmp(mesh,saved,sizeof mesh) && !memcmp(out,before,sizeof out));
 assert(ac4_sky_dome_plan(&d,out,AC4_SKY_DOME_QWORDS,&n,mesh,AC4_SKY_DOME_VERTICES,&nv));
 assert(nv==AC4_SKY_DOME_VERTICES);
 for(unsigned band=0;band<28;++band)for(unsigned k=0;k<32;++k)for(unsigned v=0;v<4;++v){
  const ac4_sky_mesh_vertex *p=mesh+band*128+k*4+v;
  const ac4_sky_qword *q=out+3+band*131+3+k*4+v;
  unsigned ring=band+(v>=2),seg=(k+(v&1))%32;
  assert(p->ring==ring && p->segment==((ring==0 || ring==28)?0:seg));
  assert(p->color==q->lo && p->xy==(uint32_t)q->hi && p->depth==(uint32_t)(q->hi>>32) && !p->fallback);
 }
 memcpy(before,out,sizeof out);n=123;d.rings[28][0]=NAN;
 assert(!ac4_sky_dome_build(&d,out,AC4_SKY_DOME_QWORDS,&n) && n==123 && !memcmp(out,before,sizeof out));
 d.rings[28][0]=1;d.matrix[15]=-1;
 assert(ac4_sky_dome_build(&d,out,3,&n) && n==3);
 unsigned char object[0x440]={0};ac4_sky_dome_input loaded;
 uint32_t marker=0x89abcdef;memcpy(object+0x3c8,&marker,4);
 assert(ac4_sky_dome_read(object,sizeof object,77,&loaded) && loaded.colors[0]==marker && loaded.depth==77);
 assert(!ac4_sky_dome_read(object,0x43b,77,&loaded));
 /* Bit 31 sign extension can select a rejected closing endpoint. Its XY
  * comes from the corresponding pre-existing alternating scratch row. */
 memset(&d,0,sizeof d);d.matrix[0]=d.matrix[1]=2048;d.matrix[3]=1;
 d.bounds[0]=d.bounds[1]=2048;d.bounds[2]=256;d.bounds[3]=224;
 for(unsigned k=0;k<32;++k){d.segments[k][0]=k==31?1:-1;d.segments[k][3]=1;}
 for(unsigned r=0;r<29;++r)d.rings[r][0]=d.rings[r][1]=1;
 d.initial_rows[0][0].xy=0x12345678;d.initial_rows[1][0].xy=0x89abcdef;
 assert(ac4_sky_dome_build(&d,out,AC4_SKY_DOME_QWORDS,&n));
 assert((out[5].lo&32767)==1 && (uint32_t)out[7].hi==0x89abcdef && (uint32_t)out[9].hi==0x12345678);
 puts("AC4 dome: full packet order, colours, depth split, padding, culling and atomic rejection passed");
}
