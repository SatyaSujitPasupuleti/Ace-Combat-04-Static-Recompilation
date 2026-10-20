#ifndef RN_AC4_SKY_PROJECT_H
#define RN_AC4_SKY_PROJECT_H
#include "ps2_runtime.h"
#include "rn_ac4_sky.h"
#include <math.h>

/* AC4 scales matrix columns before transforming its pre-scaled segment vector.
 * Keep these separate roundings: scaling a world point first is not equivalent. */
static inline int ac4_sky_ring_matrix(const float matrix[16], float radius,
                                      float height, float out[16]) {
    if (!matrix || !out || !isfinite(radius) || !isfinite(height)) return 0;
    float scaled[16];
    for (unsigned i=0;i<16;++i) {
        if (!isfinite(matrix[i])) return 0;
        scaled[i] = i<12 ? ps2_fmul(matrix[i], (i/4==1)?height:radius) : matrix[i];
    }
    memcpy(out,scaled,sizeof scaled);
    return 1;
}

/* Reconstruct the point writer's packed XY and returned clip word. A negative
 * clip rejects the point; W rejection leaves its previous XY unchanged, as the
 * original does. Return 0 only for unsupported input, without changing output.
 * Does not reconstruct guest registers/flags or replace guest execution. */
static inline int ac4_sky_project_point(const float matrix[16], const float point[4],
                                        const float bounds[4], ac4_sky_point *out) {
    if (!matrix || !point || !bounds || !out) return 0;
    for (unsigned i=0;i<16;++i) if (!isfinite(matrix[i])) return 0;
    for (unsigned i=0;i<4;++i) if (!isfinite(point[i]) || !isfinite(bounds[i])) return 0;
    float q[4];
    for (unsigned i=0;i<4;++i) {
        float a=ps2_fmul(matrix[i],point[0]);
        for (unsigned j=1;j<4;++j) a=ps2_fadd(a,ps2_fmul(matrix[4*j+i],point[j]));
        q[i]=a;
    }
    ac4_sky_point result=*out;
    result.clip=UINT32_MAX;
    if (q[3]>0.0f) {
        float reciprocal=ps2_fclamp(1.0/(double)q[3]);
        float x=ps2_fmul(q[0],reciprocal),y=ps2_fmul(q[1],reciprocal);
        uint32_t xi=(uint32_t)ps2_cvt_w_s(x*16.0f),yi=(uint32_t)ps2_cvt_w_s(y*16.0f);
        result.xy=(xi&0xffffu)|((yi&0xffffu)<<16);
        /* Guest sticky zero/sign checks reject the 0/4096 boundaries too. */
        if (x>0.0f && x<4096.0f && y>0.0f && y<4096.0f) {
            float dx=ps2_fsub(x,bounds[0]),dy=ps2_fsub(y,bounds[1]);
            float hx=fabsf(bounds[2]),hy=fabsf(bounds[3]);
            result.clip=(dx>hx?0x40u:0u)|(dx< -hx?0x80u:0u)
                       |(dy>hy?4u:0u)|(dy< -hy?8u:0u);
        }
    }
    *out=result;
    return 1;
}
#endif
