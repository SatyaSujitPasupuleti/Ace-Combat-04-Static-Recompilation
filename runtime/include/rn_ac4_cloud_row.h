#ifndef RN_AC4_CLOUD_ROW_H
#define RN_AC4_CLOUD_ROW_H
#include "ps2_runtime.h"
#define AC4_CLOUD_ROW_BYTES (16u+64u*32u)
/* AC4 001128D8: split a projected sprite at 64-pixel X boundaries.
 * Preserve the complete high lanes of packed XY/depth and UV arguments. */
static inline int ac4_cloud_row_build(uint64_t xy0,uint64_t xyz1,uint64_t uv0,uint64_t uv1,
                                     void *output,size_t capacity,size_t *bytes){
    if(!bytes)return 0;
    uint32_t x0=xy0&65535u,x1=xyz1&65535u,u0=uv0&65535u,u1=uv1&65535u;
    if(x1<=x0){*bytes=0;return 1;}
    uint32_t pieces=((x1-1)>>10)-(x0>>10)+1;
    size_t need=16u+32u*pieces;
    if(!output || capacity<need)return 0;
    uint64_t words[AC4_CLOUD_ROW_BYTES/8];
    words[0]=(UINT64_C(0x8800)<<47)|0x8000u|pieces;words[1]=0x53d3;
    float ratio=ps2_fdiv((float)((int32_t)u1-(int32_t)u0),(float)(x1-x0));
    uint32_t x=x0,u=u0;
    for(uint32_t i=0;i<pieces;++i){
        uint32_t end=(x+1024u)&~1023u;if(end>x1)end=x1;
        uint32_t next=u0+(uint32_t)ps2_cvt_w_s(ps2_fmul(ratio,(float)(end-x0)));
        words[2+4*i]=u|(uv0&~UINT64_C(65535));
        words[3+4*i]=x|(xy0&~UINT64_C(65535));
        words[4+4*i]=(uint64_t)(int64_t)(int32_t)next|(uv1&~UINT64_C(65535));
        words[5+4*i]=end|(xyz1&~UINT64_C(65535));
        x=end;u=next;
    }
    memcpy(output,words,need);*bytes=need;return 1;
}
/* Complete scalar continuation of 001128D8, separate from committing guest
 * memory. Capacity failure leaves both context and output untouched. */
static inline int ac4_cloud_row_plan(ps2_ctx *ctx,void *output,size_t capacity,size_t *bytes){
    if(!ctx || !bytes)return 0;
    uint64_t start=ctx->r[4].ud[0],xy0=ctx->r[5].ud[0],xyz1=ctx->r[6].ud[0];
    uint64_t uv0=ctx->r[7].ud[0],uv1=ctx->r[8].ud[0];
    if(!ac4_cloud_row_build(xy0,xyz1,uv0,uv1,output,capacity,bytes))return 0;
    uint32_t x0=xy0&65535u,x1=xyz1&65535u,u0=uv0&65535u,u1=uv1&65535u;
    int32_t width=(int32_t)x1-(int32_t)x0;
    float ratio=ps2_fdiv((float)((int32_t)u1-(int32_t)u0),(float)width);
    uint32_t pieces=*bytes?(uint32_t)(*bytes-16)/32:0;
    ctx->r[6].ud[0]=uv0&~UINT64_C(65535);
    ctx->r[7].ud[0]=pieces;ctx->r[8].ud[0]=uv1&~UINT64_C(65535);
    ctx->r[13].ud[0]=x0;ctx->r[14].ud[0]=xyz1&~UINT64_C(65535);
    ctx->r[15].ud[0]=xy0&~UINT64_C(65535);ctx->r[24].ud[0]=u0;
    ctx->f[1].f=ratio;
    if(pieces){
        uint64_t end=(uint64_t)(int64_t)(int32_t)((uint32_t)start+(uint32_t)*bytes);
        ctx->r[2].ud[0]=ctx->r[5].ud[0]=ctx->r[25].ud[0]=end;
        ctx->r[3].ud[0]=UINT64_C(0x4400000000008000)|pieces;ctx->r[4].ud[0]=0x53d3;
        ctx->r[9].ud[0]=1024;ctx->r[10].ud[0]=x1;ctx->r[11].ud[0]=0;ctx->r[12].ud[0]=(uint32_t)width;
        ctx->f[0].f=ps2_fmul(ratio,(float)width);ctx->f[2].u=(uint32_t)ps2_cvt_w_s(ctx->f[0].f);
    }else{
        ctx->r[2].ud[0]=ctx->r[25].ud[0]=start;ctx->r[3].ud[0]=x0&1023u;
        ctx->r[4].ud[0]=xy0>>16;
        ctx->r[5].sd[0]=(int32_t)((uint32_t)start+16);
        ctx->r[9].ud[0]=1024-(x0&1023u);ctx->r[11].sd[0]=width;ctx->r[12].ud[0]=0;
        ctx->f[0].f=(float)width;
    }
    /* The first five-slot advance drains every pending flag. No new vector
       operations occur, so the remaining advances have no observable effect. */
    ps2_vu0_advance(ctx,5);
    return 1;
}
#endif
