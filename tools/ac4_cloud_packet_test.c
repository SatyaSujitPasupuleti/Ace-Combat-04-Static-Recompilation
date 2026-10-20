#include "rn_ac4_cloud_packet.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 uint64_t packet[16]={UINT64_C(0xe400000000008001),UINT64_C(0x00512512512512f0),0x5c,123};
 ac4_cloud_record records[5]={0};uint32_t indices[4],before[4];
 for(unsigned k=0;k<4;++k){
  records[k].input.write_st=records[k].input.write_q=1;records[k].input.position[0]=(float)k;
  records[k].output.xyz=0x123400000000ull+k;records[k].output.st[0]=k*.25f;
  records[k].output.st[1]=.5f;records[k].output.q=1;
  unsigned v=3-k;memcpy(packet+4+3*v,records[k].output.st,8);
  packet[5+3*v]=UINT64_C(0x3f80000080808080);packet[6+3*v]=records[k].output.xyz;
 }
 assert(ac4_cloud_quad_map(packet,128,records,4,indices));
 for(unsigned k=0;k<4;++k)assert(indices[k]==3-k);
 memcpy(before,indices,sizeof before);packet[6]^=16;
 assert(!ac4_cloud_quad_map(packet,128,records,4,indices) && !memcmp(before,indices,sizeof before));packet[6]^=16;
 records[4]=records[3];assert(ac4_cloud_quad_map(packet,128,records,5,indices));
 records[4].input.position[0]+=1;memcpy(before,indices,sizeof before);
 assert(!ac4_cloud_quad_map(packet,128,records,5,indices) && !memcmp(before,indices,sizeof before));
 packet[2]=0x54;assert(!ac4_cloud_quad_map(packet,128,records,4,indices));packet[2]=0x5c;
 packet[5]^=UINT64_C(1)<<32;assert(!ac4_cloud_quad_map(packet,128,records,4,indices));packet[5]^=UINT64_C(1)<<32;
 assert(!ac4_cloud_quad_map(packet,127,records,4,indices));
 puts("AC4 cloud quad: exact XYZ/ST/Q association, ordering, ambiguity and atomic rejection passed");
}
