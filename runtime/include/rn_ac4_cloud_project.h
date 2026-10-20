#ifndef RN_AC4_CLOUD_PROJECT_H
#define RN_AC4_CLOUD_PROJECT_H
#include "ps2_runtime.h"
#include <math.h>
typedef struct {
    float matrix[16],position[4],bounds[4],scale,uv[2];
    uint32_t depth,write_st,write_q;
} ac4_cloud_point_input;
typedef struct {uint64_t xyz;float st[2],q;uint32_t clip;} ac4_cloud_point_output;

/* AC4 plane helper geometry only: preserve outputs not written by rejection
 * or null guest pointers. No guest register/pipeline continuation replacement. */
static inline int ac4_cloud_point_project(const ac4_cloud_point_input *d,ac4_cloud_point_output *out){
    if(!d || !out || !isfinite(d->scale))return 0;
    for(unsigned i=0;i<16;++i)if(!isfinite(d->matrix[i]))return 0;
    for(unsigned i=0;i<4;++i)if(!isfinite(d->position[i]) || !isfinite(d->bounds[i]))return 0;
    if(d->write_st && (!isfinite(d->uv[0]) || !isfinite(d->uv[1])))return 0;
    float p[4];
    for(unsigned i=0;i<4;++i){
        float a=ps2_fmul(d->matrix[i],d->position[0]);
        for(unsigned j=1;j<4;++j)a=ps2_fadd(a,ps2_fmul(d->matrix[j*4+i],d->position[j]));
        p[i]=a;
    }
    ac4_cloud_point_output result=*out;result.clip=UINT32_MAX;
    /* The original delayed MAC read observes VF0 * bounds.z, not projected
     * W. Negative W can therefore still produce accepted screen geometry. */
    if(ps2_ftz(d->bounds[2])>0){
        float reciprocal=ps2_fdiv(1.0f,p[3]);
        float x=ps2_fmul(p[0],reciprocal),y=ps2_fmul(p[1],reciprocal);
        uint32_t xi=(uint32_t)ps2_cvt_w_s(x*16),yi=(uint32_t)ps2_cvt_w_s(y*16);
        result.xyz=((uint64_t)d->depth<<32)|(xi&65535u)|((yi&65535u)<<16);
        if(x>0 && x<4096 && y>0 && y<4096){
            float dx=ps2_fsub(x,d->bounds[0]),dy=ps2_fsub(y,d->bounds[1]);
            float hx=fabsf(d->bounds[2]),hy=fabsf(d->bounds[3]);
            result.clip=(dx>hx?1u:0u)|(dx< -hx?2u:0u)|(dy>hy?0x100u:0u)|(dy< -hy?0x200u:0u);
            float q=ps2_fmul(d->scale,reciprocal);
            if(d->write_q)result.q=q;
            if(d->write_st)for(unsigned i=0;i<2;++i)result.st[i]=ps2_fmul(d->uv[i],q);
        }
    }
    *out=result;return 1;
}
#endif
