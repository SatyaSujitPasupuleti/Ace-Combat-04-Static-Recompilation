#ifndef RN_AC4_CLOUD_SPRITE_H
#define RN_AC4_CLOUD_SPRITE_H
#include "ps2_runtime.h"
#include <math.h>
typedef struct {
    float position[4],camera[4],matrix[16],scale[4];
    float lower[4],upper[4],clip[4],size;
    uint32_t minimum_depth;
} ac4_cloud_sprite_input;
typedef struct {uint8_t bytes[32];float result;} ac4_cloud_sprite_output;
static inline uint16_t ac4_sprite_u16(const uint8_t *p){uint16_t v;memcpy(&v,p,2);return v;}
static inline void ac4_sprite_clip_uv(uint8_t *a,uint8_t *b,float lo,float hi,float edge,int upper){
    float x=ps2_fmul(ps2_fsub(edge,hi),(float)ac4_sprite_u16(a));
    float y=ps2_fmul(ps2_fsub(lo,edge),(float)ac4_sprite_u16(b));
    uint16_t value=(uint16_t)ps2_cvt_w_s(ps2_fdiv(ps2_fadd(x,y),ps2_fsub(lo,hi)));
    memcpy(upper?b:a,&value,2);
}
/* Geometry/output reconstruction of 00114ED8. Guest continuation is separate. */
static inline int ac4_cloud_sprite_project(const ac4_cloud_sprite_input *d,ac4_cloud_sprite_output *out){
    if(!d || !out || !isfinite(d->size))return 0;
    for(unsigned i=0;i<4;++i)if(!isfinite(d->position[i]) || !isfinite(d->camera[i])
       || !isfinite(d->scale[i]) || !isfinite(d->lower[i]) || !isfinite(d->upper[i]) || !isfinite(d->clip[i]))return 0;
    for(unsigned i=0;i<16;++i)if(!isfinite(d->matrix[i]))return 0;
    ac4_cloud_sprite_output r=*out;r.result=-1;
    float delta[3],p[4],lo[4],hi[4];
    for(unsigned i=0;i<3;++i){float v=ps2_fsub(d->position[i],d->camera[i]);delta[i]=ps2_fmul(v,v);}
    float distance=ps2_fsqrt(ps2_fadd(ps2_fadd(delta[0],delta[1]),delta[2]));
    memcpy(r.bytes,&distance,4);
    for(unsigned i=0;i<4;++i){
        p[i]=ps2_fmul(d->matrix[i],d->position[0]);
        for(unsigned j=1;j<4;++j)p[i]=ps2_fadd(p[i],ps2_fmul(d->matrix[j*4+i],d->position[j]));
    }
    float q=ps2_fdiv(1,p[3]);
    if(!(p[2]>0)){*out=r;return 1;}
    for(unsigned i=0;i<3;++i)p[i]=ps2_fmul(p[i],q);
    float extent=ps2_fmul(d->size,q);
    for(unsigned i=0;i<4;++i){float e=ps2_fmul(d->scale[i],extent);lo[i]=ps2_fsub(p[i],e);hi[i]=ps2_fadd(p[i],e);}
    for(unsigned i=0;i<2;++i)if(!(ps2_fsub(hi[i],d->lower[i])>0) || !(ps2_fsub(d->upper[i],lo[i])>0)){*out=r;return 1;}
    float scalar_lo[2]={lo[0],lo[1]},scalar_hi[2]={hi[0],hi[1]};
    for(unsigned i=0;i<2;++i)if(scalar_lo[i]<d->clip[i]){
        ac4_sprite_clip_uv(r.bytes+8+2*i,r.bytes+12+2*i,scalar_lo[i],scalar_hi[i],d->clip[i],0);
        scalar_lo[i]=d->clip[i];lo[i]=ps2_fadd(d->lower[i],0);
    }
    for(unsigned i=0;i<2;++i)if(d->clip[i+2]<scalar_hi[i]){
        ac4_sprite_clip_uv(r.bytes+8+2*i,r.bytes+12+2*i,scalar_lo[i],scalar_hi[i],d->clip[i+2],1);
        hi[i]=ps2_fadd(d->upper[i],0);
    }
    uint32_t xy0=((uint32_t)ps2_cvt_w_s(lo[0]*16)&65535u)|((uint32_t)ps2_cvt_w_s(lo[1]*16)<<16);
    uint32_t xy1=((uint32_t)ps2_cvt_w_s(hi[0]*16)&65535u)|((uint32_t)ps2_cvt_w_s(hi[1]*16)<<16);
    uint32_t z=(uint32_t)ps2_cvt_w_s(hi[2]*16)>>3;
    z=z<d->minimum_depth?d->minimum_depth:z>0xffffff?0xffffff:z;
    memcpy(r.bytes+16,&xy0,4);memcpy(r.bytes+24,&xy1,4);memcpy(r.bytes+28,&z,4);
    r.result=ps2_fadd(0,p[3]);*out=r;return 1;
}
#endif
