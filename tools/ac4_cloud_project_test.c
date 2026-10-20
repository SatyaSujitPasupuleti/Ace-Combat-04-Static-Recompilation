#include "rn_ac4_cloud_project.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 ac4_cloud_point_input d={0};d.matrix[0]=d.matrix[5]=d.matrix[10]=d.matrix[15]=1;
 d.matrix[12]=d.matrix[13]=2048;d.position[3]=1;
 d.bounds[0]=d.bounds[1]=2048;d.bounds[2]=256;d.bounds[3]=224;
 d.scale=2;d.uv[0]=.25f;d.uv[1]=.75f;d.depth=0x12345678;d.write_q=d.write_st=1;
 ac4_cloud_point_output o={0};assert(ac4_cloud_point_project(&d,&o));
 assert(o.xyz==UINT64_C(0x1234567880008000) && o.clip==0 && o.q==2 && o.st[0]==.5f && o.st[1]==1.5f);
 d.position[0]=257;assert(ac4_cloud_point_project(&d,&o) && o.clip==1);
 d.position[0]=-257;assert(ac4_cloud_point_project(&d,&o) && o.clip==2);
 d.position[0]=0;d.position[1]=225;assert(ac4_cloud_point_project(&d,&o) && o.clip==0x100);
 d.position[1]=-225;assert(ac4_cloud_point_project(&d,&o) && o.clip==0x200);
 d.position[1]=0;d.position[0]=-2048;o.q=77;o.st[0]=88;
 assert(ac4_cloud_point_project(&d,&o) && o.clip==UINT32_MAX && o.q==77 && o.st[0]==88 && (uint32_t)o.xyz==0x80000000);
 d.position[0]=2048;assert(ac4_cloud_point_project(&d,&o) && o.clip==UINT32_MAX);
 /* W sign alone does not reject: the delayed MAC read sees bounds.z. */
 d.position[0]=d.position[1]=-4096;d.matrix[15]=-1;
 assert(ac4_cloud_point_project(&d,&o) && o.clip==0 && o.q==-2 && o.st[1]==-1.5f);
 d.bounds[2]=0;ac4_cloud_point_output before=o;
 assert(ac4_cloud_point_project(&d,&o) && o.clip==UINT32_MAX && o.xyz==before.xyz && o.q==before.q);
 d.bounds[2]=0x1p-140f;assert(ac4_cloud_point_project(&d,&o) && o.clip==UINT32_MAX && o.xyz==before.xyz);
 d.bounds[2]=256;d.write_st=d.write_q=0;o.st[0]=91;o.q=92;
 assert(ac4_cloud_point_project(&d,&o) && o.clip==0 && o.st[0]==91 && o.q==92);
 before=o;d.position[2]=NAN;
 assert(!ac4_cloud_point_project(&d,&o) && !memcmp(&o,&before,sizeof o));
 puts("AC4 cloud point: projection, clip masks, delayed bound check, edge rejection and optional writes passed");
}
