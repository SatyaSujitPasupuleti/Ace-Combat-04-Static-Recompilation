#include "rn_ac4_cloud_sprite.h"
#include <assert.h>
#include <stdio.h>
static uint32_t word(const uint8_t *p){uint32_t v;memcpy(&v,p,4);return v;}
int main(void){
    ac4_cloud_sprite_input d={0};ac4_cloud_sprite_output o;
    d.matrix[0]=d.matrix[5]=d.matrix[10]=d.matrix[15]=1;
    d.matrix[12]=d.matrix[13]=2048;d.position[2]=d.position[3]=1;
    d.scale[0]=d.scale[1]=1;d.size=100;
    d.lower[0]=d.lower[1]=d.clip[0]=d.clip[1]=1998;
    d.upper[0]=d.upper[1]=d.clip[2]=d.clip[3]=2098;
    memset(&o,0x5a,sizeof o);uint16_t uv[4]={0,0,1024,1024};memcpy(o.bytes+8,uv,8);
    assert(ac4_cloud_sprite_project(&d,&o) && o.result==1);
    assert(ac4_sprite_u16(o.bytes+8)==256 && ac4_sprite_u16(o.bytes+10)==256);
    assert(ac4_sprite_u16(o.bytes+12)==768 && ac4_sprite_u16(o.bytes+14)==768);
    assert(word(o.bytes+16)==(1998u*16|(1998u*16<<16)));
    assert(word(o.bytes+24)==(2098u*16|(2098u*16<<16)) && word(o.bytes+28)==2);
    assert(word(o.bytes+4)==0x5a5a5a5a && word(o.bytes+20)==0x5a5a5a5a);
    d.minimum_depth=17;assert(ac4_cloud_sprite_project(&d,&o) && word(o.bytes+28)==17);
    ac4_cloud_sprite_output before=o;d.position[2]=-1;
    assert(ac4_cloud_sprite_project(&d,&o) && o.result==-1 && !memcmp(o.bytes+4,before.bytes+4,28));
    d.position[2]=1;d.position[0]=500;
    assert(ac4_cloud_sprite_project(&d,&o) && o.result==-1 && !memcmp(o.bytes+4,before.bytes+4,28));
    before=o;d.size=NAN;assert(!ac4_cloud_sprite_project(&d,&o) && !memcmp(&o,&before,sizeof o));
    puts("AC4 cloud sprite: clipping UVs/XY, depth floor, rejection writes and invalid-input atomicity passed");
}
