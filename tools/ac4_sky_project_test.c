#include "rn_ac4_sky_project.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    float m[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    float bounds[4]={2048,2048,256,224},p[4]={2048,2048,0,1};
    ac4_sky_point out={0xabcdef01,0};
    assert(ac4_sky_project_point(m,p,bounds,&out));
    assert(out.xy==0x80008000 && !out.clip);
    float coords[][2]={{2304,2048},{2305,2048},{1791,2048},{2048,2273},{2048,1823},{2305,2273}};
    unsigned masks[]={0,0x40,0x80,4,8,0x44};
    for(unsigned i=0;i<6;++i) {
        p[0]=coords[i][0];p[1]=coords[i][1];
        assert(ac4_sky_project_point(m,p,bounds,&out) && out.clip==masks[i]);
    }
    for(unsigned axis=0;axis<2;++axis) for(unsigned edge=0;edge<2;++edge) {
        p[0]=p[1]=2048;p[axis]=edge?4096:0;
        assert(ac4_sky_project_point(m,p,bounds,&out) && out.clip==UINT32_MAX);
    }
    p[0]=p[1]=2048;p[3]=-1;out.xy=123;
    assert(ac4_sky_project_point(m,p,bounds,&out) && out.xy==123 && out.clip==UINT32_MAX);
    p[3]=0;assert(ac4_sky_project_point(m,p,bounds,&out) && out.xy==123);
    ac4_sky_point before=out;p[0]=NAN;
    assert(!ac4_sky_project_point(m,p,bounds,&out) && !memcmp(&out,&before,sizeof out));
    float scaled[16];assert(ac4_sky_ring_matrix(m,2,3,scaled));
    assert(scaled[0]==2 && scaled[5]==3 && scaled[10]==2 && scaled[15]==1);
    float old[16];memcpy(old,scaled,sizeof old);m[15]=INFINITY;
    assert(!ac4_sky_ring_matrix(m,2,3,scaled) && !memcmp(old,scaled,sizeof old));
    puts("AC4 sky projection: screen bounds, clip bits, W rejection and atomic input guards passed");
}
