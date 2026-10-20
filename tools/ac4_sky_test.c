#include "rn_ac4_sky.h"
#include "rn_ac4_sky_continuation.h"
#include <assert.h>
#include <stdio.h>

static uint32_t rng = 17;
static uint32_t next(void) { rng = rng * 1664525u + 1013904223u; return rng; }

int main(void) {
    ac4_sky_point a[33], b[33];
    ac4_sky_qword out[AC4_SKY_BAND_QWORDS], before[AC4_SKY_BAND_QWORDS];
    for (unsigned i=0; i<33; ++i) {
        a[i]=(ac4_sky_point){0x70007000u+i,0};
        b[i]=(ac4_sky_point){0x72007000u+i,0};
    }
    size_t n=999;
    ps2_ctx ctx={0};ctx.r[4].ud[0]=0x3000;ctx.r[5].ud[0]=3;ctx.r[6].ud[0]=0x1000;ctx.r[7].ud[0]=0x1200;
    ctx.r[6].ud[1]=123;ctx.f[0].u=456;ctx.vu0_flag_slot=3;ctx.vu0_flag_pending=1;ctx.vu0_flags[0].mac=16;
    ps2_ctx entry=ctx;
    assert(ac4_sky_band_plan(&ctx,a,b,10,20,out,129,&n) && n==5);
    assert(ctx.r[2].ud[0]==0x3050 && ctx.r[14].ud[0]==0x3050 && ctx.r[8].ud[0]==0x3050);
    assert(ctx.r[6].ud[0]==0x1010 && ctx.r[7].ud[0]==0x1210 && ctx.r[15].ud[0]==2 && ctx.r[24].ud[0]==1);
    assert(ctx.r[3].ud[0]==UINT64_C(0x8400000000008001) && ctx.r[4].ud[0]==0x5151d1d1);
    assert(ctx.r[6].ud[1]==123 && ctx.f[0].u==456 && !ctx.vu0_flag_pending && ctx.vu0_flag_slot==3 && ctx.vu0.status==130);
    ctx=entry;ctx.r[5].ud[0]=0;ctx.r[25].ud[0]=99;
    assert(ac4_sky_band_plan(&ctx,a,b,10,20,NULL,0,&n) && !n);
    assert(ctx.r[2].ud[0]==0x3000 && ctx.r[8].ud[0]==0x3010 && ctx.r[6].ud[0]==0x1000 && ctx.r[25].ud[0]==99);
    memset(out,0x5a,sizeof out); memcpy(before,out,sizeof out);
    ctx=entry;n=999;assert(!ac4_sky_band_plan(&ctx,a,b,10,20,out,4,&n));
    assert(n==999 && !memcmp(&ctx,&entry,sizeof ctx) && !memcmp(out,before,sizeof out));
    assert(ac4_sky_band_build(a,b,0,1,2,0,0,out,0,&n) && !n);
    assert(!memcmp(out,before,sizeof out));
    n=999;
    assert(!ac4_sky_band_build(a,b,3,1,2,0,0,out,4,&n));
    assert(n==999 && !memcmp(out,before,sizeof out));
    assert(!ac4_sky_band_build(NULL,b,3,1,2,0,0,out,129,&n));
    assert(!ac4_sky_band_build(a,b,3,1,2,0,0,NULL,129,&n));
    assert(n==999);
    assert(ac4_sky_band_build(a,b,3,0x89abcdef,0x12345678,
        UINT64_C(0x123400000000),UINT64_C(0x567800000000),out,129,&n));
    assert(n==5 && out[0].lo==UINT64_C(0x8400000000008001));
    assert(out[0].hi==0x5151d1d1u);
    assert(out[1].lo==0x89abcdefu && out[2].lo==0x89abcdefu);
    assert(out[3].lo==0x12345678u && out[4].lo==0x12345678u);
    assert(out[1].hi==UINT64_C(0x123470007000));
    assert(out[4].hi==UINT64_C(0x567872007001));
    assert(ac4_sky_band_build(a,b,UINT64_C(0x1ffffffff),1,2,0,0,out,129,&n));
    assert(n==129 && (out[0].lo&0x7fff)==32);
    /* Segment 31 requires the explicit closure bit 32. */
    assert(ac4_sky_band_build(a,b,UINT64_C(3)<<31,1,2,0,0,out,129,&n));
    assert(n==5 && out[1].hi==a[31].xy && out[4].hi==b[32].xy);
    assert(ac4_sky_band_build(a,b,UINT64_C(1)<<31,1,2,0,0,out,129,&n) && !n);
    /* Independently enumerate visible quads, then decode every emitted word. */
    for (unsigned trial=0;trial<1000;++trial) {
        uint64_t mask=((uint64_t)(next()&1)<<32)|next();
        uint32_t ca=next(),cb=next();
        uint64_t za=(uint64_t)next()<<32,zb=(uint64_t)next()<<32;
        for (unsigned i=0;i<33;++i) {a[i].clip=next()&0xcc; b[i].clip=next()&0xcc;}
        unsigned expected[32], q=0;
        for(unsigned i=0;i<32;++i) {
            int left=(mask&(UINT64_C(1)<<i))!=0;
            int right=(mask&(UINT64_C(1)<<(i+1)))!=0;
            uint32_t shared=~0u;
            const ac4_sky_point *points[]={&a[i],&a[i+1],&b[i],&b[i+1]};
            for(unsigned j=0;j<4;++j) shared &= points[j]->clip;
            if(left && right && !shared) expected[q++]=i;
        }
        assert(ac4_sky_band_build(a,b,mask,ca,cb,za,zb,out,129,&n));
        assert(n==(q?1u+4u*q:0u));
        for(unsigned j=0;j<q;++j) {
            unsigned i=expected[j];
            assert(out[1+4*j].lo==ca && out[2+4*j].lo==ca);
            assert(out[3+4*j].lo==cb && out[4+4*j].lo==cb);
            assert(out[1+4*j].hi==(za|a[i].xy));
            assert(out[2+4*j].hi==(za|a[i+1].xy));
            assert(out[3+4*j].hi==(zb|b[i].xy));
            assert(out[4+4*j].hi==(zb|b[i+1].xy));
        }
    }
    puts("AC4 sky band: visibility, closure, clipping, packet order and atomic capacity passed");
    return 0;
}
