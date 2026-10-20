#ifndef RN_AC4_SKY_CONTINUATION_H
#define RN_AC4_SKY_CONTINUATION_H
#include "ps2_runtime.h"
#include "rn_ac4_sky.h"

/* Packet and complete scalar continuation of EE sky band helper 00111050.
 * Guest memory is not committed here; failure leaves context/output intact. */
static inline int ac4_sky_band_plan(ps2_ctx *ctx,const ac4_sky_point a[33],
    const ac4_sky_point b[33],u32 color_a,u32 color_b,
    ac4_sky_qword *out,size_t capacity,size_t *count){
    if(!ctx)return 0;
    u64 mask=ctx->r[5].ud[0],start=ctx->r[4].ud[0];
    if(!ac4_sky_band_build(a,b,mask,color_a,color_b,ctx->r[9].ud[0],ctx->r[10].ud[0],out,capacity,count))return 0;
    u32 steps=0,quads=*count?(u32)(*count-1)/4:0;
    if(mask)ctx->r[25].ud[0]=3;
    while(mask && steps<32){
        if((mask&3)==3){
            u32 clip=a[steps].clip & a[steps+1].clip & b[steps].clip & b[steps+1].clip;
            ctx->r[3].sd[0]=(s32)clip;
            ctx->r[4].sd[0]=(s32)a[steps+1].clip;
            ctx->r[5].sd[0]=(s32)b[steps+1].clip;
        }
        steps++;mask>>=1;
    }
    if(steps){
        ctx->r[6].sd[0]=(s32)(ctx->r[6].uw[0]+steps*8);
        ctx->r[7].sd[0]=(s32)(ctx->r[7].uw[0]+steps*8);
    }
    ctx->r[8].sd[0]=(s32)((u32)start+16+quads*64);
    ctx->r[11].ud[0]=mask;ctx->r[12].ud[0]=color_a;ctx->r[13].ud[0]=color_b;
    ctx->r[14].ud[0]=quads?ctx->r[8].ud[0]:start;
    ctx->r[15].ud[0]=steps;ctx->r[24].ud[0]=quads;
    ctx->r[2].ud[0]=ctx->r[14].ud[0];
    if(quads){ctx->r[3].ud[0]=UINT64_C(0x8400000000008000)|quads;ctx->r[4].ud[0]=UINT64_C(0x5151d1d1);}
    /* First advance drains flags; this scalar helper never schedules more. */
    ps2_vu0_advance(ctx,5);
    return 1;
}
#endif
