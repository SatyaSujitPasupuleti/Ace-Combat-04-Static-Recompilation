#include "ps2_runtime.h"
#include "ps2_hook.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static u8 memory[32768];
static int hook_calls, removed, fail_hook=-1, signature_ok=1;
static u32 audit_r32(u32 a) {
    const u32 addr[]={0x111050,0x111054,0x111138,0x11113c,0x111310,0x111314,0x114ed8,0x114edc,0x116060,0x116064,0x116320,0x116324,0x1128d8,0x1128dc,0x1152d8,0x1152dc};
    const u32 words[]={0x0080702d,0x9d0d0004,0x27bdff30,0x3c031000,0x27bdfed0,0x8f82aae4,0xd8890000,0x4bc14b2c,0xd8e80000,0x4be821bc,0x27bdfec0,0xffbe0100,0x00a0782d,0x00c0702d,0x27bdff10,0xffb40050};
    for(unsigned i=0;i<16;++i) if(a==addr[i]) return signature_ok?words[i]:0;
    u32 v;assert(a+4<=sizeof memory);memcpy(&v,memory+a,4);return v;
}
static void audit_get(void *dst,u32 a,size_t n) {assert(a+n<=sizeof memory);memcpy(dst,memory+a,n);}
static void audit_put(u32 a,const void *src,size_t n) {assert(a+n<=sizeof memory);memcpy(memory+a,src,n);}
#define ps2_r32 audit_r32
#define ps2_get_mem audit_get
#define ps2_put_mem audit_put
#define ps2_log audit_log
void ps2_log(const char *fmt,...) {(void)fmt;}
int ps2_hook_before(u32 a,ps2_hook_fn f,void *u,int p,const char *o) {
    (void)a;(void)f;(void)u;(void)p;(void)o;
    int n=hook_calls++;return n==fail_hook?-1:n+1;
}
int ps2_hook_after(u32 a,ps2_hook_fn f,void *u,int p,const char *o) {return ps2_hook_before(a,f,u,p,o);}
int ps2_hook_replace(u32 a,ps2_hook_fn f,void *u,int p,const char *o) {return ps2_hook_before(a,f,u,p,o);}
int ps2_hook_remove(int h) {assert(h>0);removed++;return 0;}
static int entry_ok;static u32 entry_args[2];
u32 ps2_hook_entry_arg(int n,int *ok) {*ok=entry_ok;return n<2?entry_args[n]:0;}
static void ac4_sky_queue(u32 start,u32 end,const void *p,u32 n){(void)start;(void)end;(void)p;(void)n;}
static void ac4_cloud_queue(u32 start,u32 end,const void *p,u32 n){(void)start;(void)end;(void)p;(void)n;}
static void ac4_sprite_queue(u32 start,u32 end,const void *p,u32 n){(void)start;(void)end;(void)p;(void)n;}
#include "../runtime/src/rn/rn_ac4_sky_audit.h"
#include "../runtime/src/rn/rn_ac4_cloud_audit.h"

int main(void) {
    assert(ac4_sky_range(0,PS2_RAM_SIZE));
    assert(!ac4_sky_range(PS2_RAM_SIZE-4,8));
    assert(ac4_sky_range(0x70003ff0,16));
    assert(!ac4_sky_range(0x70003ff0,17));
    assert(!ac4_sky_range(0xffffffff,16));
    assert(!ac4_sky_range(0x70000000,(size_t)-1));
    _putenv_s("PS2_AC4_SKY_AUDIT", "0");ac4_sky_audit_init();assert(!hook_calls);
    _putenv_s("PS2_AC4_SKY_AUDIT", "1");signature_ok=0;ac4_sky_audit_init();assert(!hook_calls);
    signature_ok=1;fail_hook=2;ac4_sky_audit_init();assert(hook_calls==6 && removed==5);
    hook_calls=0;fail_hook=-1;removed=0;ac4_sky_audit_init();assert(hook_calls==6 && !removed);
    ps2_ctx ctx={0};ac4_sky_point a[33]={0},b[33]={0};
    for(unsigned i=0;i<33;++i) {a[i].xy=0x70007000+i;b[i].xy=0x71007100+i;}
    u32 colors[]={0x80abcdef,0x80897654};
    memcpy(memory+0x1000,a,sizeof a);memcpy(memory+0x1200,b,sizeof b);memcpy(memory+0x1400,colors,8);
    ctx.r[4].ud[0]=0x3000;ctx.r[5].ud[0]=3;ctx.r[6].ud[0]=0x1000;ctx.r[7].ud[0]=0x1200;ctx.r[8].ud[0]=0x1400;
    ctx.r[9].ud[0]=UINT64_C(0x123400000000);ctx.r[10].ud[0]=UINT64_C(0x567800000000);
    ps2_ctx before=ctx;u8 original[sizeof memory];memcpy(original,memory,sizeof memory);
    ac4_sky_band_before(&ctx,NULL);
    assert(!memcmp(&ctx,&before,sizeof ctx) && !memcmp(memory,original,sizeof memory));
    assert(ac4_sky_audit.ready && ac4_sky_audit.count==5);
    memcpy(memory+0x3000,ac4_sky_audit.expected,80);ctx.r[2].ud[0]=0x3050;before=ctx;
    memcpy(original,memory,sizeof memory);ac4_sky_band_after(&ctx,NULL);
    assert(ac4_sky_audit.matched==1 && !ac4_sky_audit.mismatched);
    assert(!memcmp(&ctx,&before,sizeof ctx) && !memcmp(memory,original,sizeof memory));
    ac4_sky_band_before(&ctx,NULL);memory[0x3010]^=1;ac4_sky_band_after(&ctx,NULL);
    assert(ac4_sky_audit.mismatched==1);
    ac4_sky_band_before(&ctx,NULL);memcpy(memory+0x3000,ac4_sky_audit.expected,80);ctx.r[2].ud[0]++;
    ac4_sky_band_after(&ctx,NULL);assert(ac4_sky_audit.mismatched==2);
    ctx.r[6].ud[0]=0xffffffff;ac4_sky_band_before(&ctx,NULL);ac4_sky_band_after(&ctx,NULL);
    assert(ac4_sky_audit.declined==1 && !ac4_sky_audit.depth);
    ctx.r[4].ud[0]=0x5000;ctx.r[5].ud[0]=0x1000;ctx.r[6].ud[0]=0x2000;ctx.r[7].ud[0]=77;
    memset(memory+0x1000,0,0x440);memset(memory+0x2000,0x6b,528);
    before=ctx;memcpy(original,memory,sizeof memory);ac4_sky_dome_before(&ctx,NULL);
    assert(ac4_sky_audit.dome_ready && ac4_sky_audit.dome_packet==0x5000);
    assert(ac4_sky_audit.dome_input.depth==77 && ac4_sky_audit.dome_input.initial_rows[1][32].xy==0x6b6b6b6b);
    assert(!memcmp(&ctx,&before,sizeof ctx) && !memcmp(memory,original,sizeof memory));
    memset(memory+0x2000,0,528);assert(ac4_sky_audit.dome_input.initial_rows[0][0].xy==0x6b6b6b6b);
    ctx.r[6].ud[0]=0xffffffff;ac4_sky_dome_before(&ctx,NULL);assert(!ac4_sky_audit.dome_ready);
    ac4_sky_producer_after(&ctx,NULL);ac4_sky_producer_after(&ctx,(void*)1);
    assert(ac4_sky_audit.domes==1 && ac4_sky_audit.hazes==1 && !ac4_sky_audit.saved);
    ctx.r[4].ud[0]=0x5000;ctx.r[5].ud[0]=0x1000;ctx.r[6].ud[0]=0x2000;
    ctx.r[7].ud[0]=0x3000;ctx.r[28].ud[0]=0x6000;
    u32 distances=0x4000;memcpy(memory+0x6000-0x551c,&distances,4);
    float values[2]={1000,5000};memcpy(memory+0x4008,values,8);
    float offset=123,scale=456;memcpy(memory+0x3088,&offset,4);memcpy(memory+0x3098,&scale,4);
    before=ctx;memcpy(original,memory,sizeof memory);ac4_sky_haze_before(&ctx,NULL);
    assert(ac4_sky_audit.haze_ready && ac4_sky_audit.haze_input.near_distance==1000
       && ac4_sky_audit.haze_input.far_distance==5000 && ac4_sky_audit.haze_input.depth_offset==123
       && ac4_sky_audit.haze_input.depth_scale==456);
    assert(!memcmp(&ctx,&before,sizeof ctx) && !memcmp(memory,original,sizeof memory));
    memset(memory+0x4008,0,8);assert(ac4_sky_audit.haze_input.far_distance==5000);
    ac4_sky_qword haze[5];size_t count=0;
    assert(ac4_sky_haze_build(&ac4_sky_audit.haze_input,haze,5,&count) && count==5);
    memcpy(memory+0x5000,haze,sizeof haze);entry_ok=1;entry_args[0]=0x5000;entry_args[1]=0x1000;
    ctx.r[2].ud[0]=0x5050;before=ctx;memcpy(original,memory,sizeof memory);
    ac4_sky_producer_after(&ctx,(void*)1);assert(ac4_sky_audit.haze_matched==1);
    assert(!memcmp(&ctx,&before,sizeof ctx) && !memcmp(memory,original,sizeof memory));
    ac4_sky_haze_before(&ctx,NULL);memory[0x5010]^=1;
    ac4_sky_producer_after(&ctx,(void*)1);assert(ac4_sky_audit.haze_mismatched==1);
    ctx.r[7].ud[0]=0xffffffff;ac4_sky_haze_before(&ctx,NULL);assert(!ac4_sky_audit.haze_ready);
    ac4_sky_audit_report();
    hook_calls=0;removed=0;_putenv_s("PS2_AC4_CLOUD_AUDIT","0");ac4_cloud_audit_init();assert(!hook_calls);
    _putenv_s("PS2_AC4_CLOUD_AUDIT","1");signature_ok=0;ac4_cloud_audit_init();assert(!hook_calls);
    signature_ok=1;fail_hook=1;ac4_cloud_audit_init();assert(hook_calls==10 && removed==9);
    hook_calls=0;removed=0;fail_hook=-1;ac4_cloud_audit_init();assert(hook_calls==10 && !removed);
    ctx.r[4].ud[0]=0x5000;ctx.r[7].ud[0]=0x1000;before=ctx;memcpy(original,memory,sizeof memory);
    ac4_cloud_sprite_before(&ctx,NULL);ac4_cloud_vertex_before(&ctx,NULL);
    assert(!memcmp(&ctx,&before,sizeof ctx) && !memcmp(memory,original,sizeof memory));
    ctx.r[2].ud[0]=0;before=ctx;ac4_cloud_vertex_after(&ctx,NULL);
    assert(ac4_cloud_audit.sprites==1 && ac4_cloud_audit.vertices==1 && ac4_cloud_audit.visible==1);
    assert(!memcmp(&ctx,&before,sizeof ctx) && !memcmp(memory,original,sizeof memory));
    ac4_cloud_vertex_before(&ctx,NULL);ctx.r[2].ud[0]=UINT32_MAX;ac4_cloud_vertex_after(&ctx,NULL);
    assert(ac4_cloud_audit.visible==1);
    /* Matched projected geometry and optional ST/Q with independent guest
     * writes, plus a corrupt output and an invalid-pointer decline. */
    ctx.r[4].ud[0]=0x5000;ctx.r[5].ud[0]=0x5100;ctx.r[6].ud[0]=0x5200;
    ctx.r[7].ud[0]=0x1000;ctx.r[8].ud[0]=0x1100;ctx.r[9].ud[0]=123;
    memset(&ctx.vu0,0,sizeof ctx.vu0);
    ctx.vu0.vf[4].f[0]=ctx.vu0.vf[5].f[1]=ctx.vu0.vf[6].f[2]=ctx.vu0.vf[7].f[3]=1;
    ctx.vu0.vf[7].f[0]=ctx.vu0.vf[7].f[1]=2048;
    ctx.vu0.vf[3].f[0]=ctx.vu0.vf[3].f[1]=2048;ctx.vu0.vf[3].f[2]=256;ctx.vu0.vf[3].f[3]=224;
    ctx.vu0.vf[1].f[0]=2;float pos[4]={0,0,0,1},uv[2]={.25f,.75f};
    memcpy(memory+0x1000,pos,16);memcpy(memory+0x1100,uv,8);
    ac4_cloud_vertex_before(&ctx,NULL);
    u64 xyz=UINT64_C(0x0000007b80008000);float st[2]={.5f,1.5f},q=2;
    memcpy(memory+0x5000,&xyz,8);memcpy(memory+0x5100,st,8);memcpy(memory+0x5200,&q,4);ctx.r[2].ud[0]=0;
    u64 matched=ac4_cloud_audit.matched;before=ctx;memcpy(original,memory,sizeof memory);
    ac4_cloud_vertex_after(&ctx,NULL);assert(ac4_cloud_audit.matched==matched+1);
    assert(!memcmp(&ctx,&before,sizeof ctx) && !memcmp(memory,original,sizeof memory));
    u64 bad=ac4_cloud_audit.mismatched;ac4_cloud_vertex_before(&ctx,NULL);memory[0x5100]^=1;
    ac4_cloud_vertex_after(&ctx,NULL);assert(ac4_cloud_audit.mismatched==bad+1);
    u64 declined=ac4_cloud_audit.declined;ctx.r[8].ud[0]=0xffffffff;
    ac4_cloud_vertex_before(&ctx,NULL);assert(!ac4_cloud_audit.ready && ac4_cloud_audit.declined==declined+1);
    ac4_cloud_audit_report();
    ctx.r[4].ud[0]=0x6000;ac4_cloud_planes_before(&ctx,NULL);
    ac4_cloud_packets.count=1;ac4_cloud_record *rec=ac4_cloud_packets.records;memset(rec,0,sizeof *rec);
    rec->input.write_st=rec->input.write_q=1;rec->output.xyz=123;rec->output.q=1;
    u64 packet[28]={UINT64_C(0x1000000000008005),14,0,0x42,0,0x47,0,0x4e,0,0x14,0,6,
        UINT64_C(0xe400000000008001),UINT64_C(0x00512512512512f0),0x5c};
    for(unsigned k=0;k<4;++k){packet[17+k*3]=UINT64_C(0x3f80000080808080);packet[18+k*3]=123;}
    memcpy(memory+0x6000,packet,sizeof packet);ctx.r[2].ud[0]=0x6000+sizeof packet;
    before=ctx;memcpy(original,memory,sizeof memory);ac4_cloud_planes_after(&ctx,NULL);
    assert(ac4_cloud_packets.matched==1 && ac4_cloud_packets.quads==1 && !ac4_cloud_packets.declined);
    assert(!memcmp(&ctx,&before,sizeof ctx) && !memcmp(memory,original,sizeof memory));
    ac4_cloud_planes_before(&ctx,NULL);ac4_cloud_packets.count=1;memory[0x6060]^=1;
    ac4_cloud_planes_after(&ctx,NULL);assert(ac4_cloud_packets.declined==1);
    memset(&ctx,0,sizeof ctx);ctx.r[4].ud[0]=0x4000;ctx.r[5].ud[0]=0x5000;ctx.r[6].ud[0]=0x4100;
    ctx.vu0.vf[5].f[0]=ctx.vu0.vf[6].f[1]=ctx.vu0.vf[7].f[2]=ctx.vu0.vf[8].f[3]=1;
    ctx.vu0.vf[8].f[0]=ctx.vu0.vf[8].f[1]=2048;
    ctx.vu0.vf[2].f[0]=ctx.vu0.vf[2].f[1]=1;ctx.f[12].f=10;
    ctx.vu0.vf[3].f[0]=ctx.vu0.vf[3].f[1]=1792;
    ctx.vu0.vf[4].f[0]=ctx.vu0.vf[4].f[1]=2304;
    float sprite_pos[4]={0,0,1,1},sprite_clip[4]={1792,1792,2304,2304};
    memcpy(memory+0x4000,sprite_pos,16);memcpy(memory+0x4100,sprite_clip,16);
    ac4_cloud_sprites_begin(&ctx,NULL);assert(ac4_sprite_records.active && !ac4_sprite_records.count);
    before=ctx;memcpy(original,memory,sizeof memory);ac4_cloud_sprite_before(&ctx,NULL);
    assert(ac4_sprite_audit.ready && !memcmp(&ctx,&before,sizeof ctx) && !memcmp(memory,original,sizeof memory));
    memcpy(memory+0x5000,ac4_sprite_audit.expected.bytes,32);ctx.f[0].f=ac4_sprite_audit.expected.result;
    before=ctx;memcpy(original,memory,sizeof memory);u64 sprite_matched=ac4_sprite_audit.matched;
    ac4_cloud_sprite_after(&ctx,NULL);assert(ac4_sprite_audit.matched==sprite_matched+1);
    assert(ac4_sprite_records.count==1 && !ac4_sprite_records.failed);
    assert(!memcmp(&ctx,&before,sizeof ctx) && !memcmp(memory,original,sizeof memory));
    ac4_cloud_sprite_before(&ctx,NULL);memory[0x5008]^=1;u64 sprite_bad=ac4_sprite_audit.mismatched;
    ac4_cloud_sprite_after(&ctx,NULL);assert(ac4_sprite_audit.mismatched==sprite_bad+1);
    assert(ac4_sprite_records.failed);ac4_cloud_sprites_end(&ctx,NULL);assert(!ac4_sprite_records.active);
    ctx.r[6].ud[0]=0xffffffff;ac4_cloud_sprite_before(&ctx,NULL);assert(!ac4_sprite_audit.ready);
    memset(&ctx,0,sizeof ctx);ctx.r[4].ud[0]=0x6000;ctx.r[5].ud[0]=1000;ctx.r[6].ud[0]=1100;
    ctx.r[7].ud[0]=0;ctx.r[8].ud[0]=100;ac4_cloud_row_before(&ctx,NULL);assert(!ac4_row_audit.ready);
    ctx.r[31].ud[0]=0x115ab0;before=ctx;memcpy(original,memory,sizeof memory);
    ac4_cloud_row_before(&ctx,NULL);assert(ac4_row_audit.ready && ac4_row_audit.bytes==80);
    assert(!memcmp(&ctx,&before,sizeof ctx) && !memcmp(memory,original,sizeof memory));
    ps2_ctx row_entry=ctx;
    memcpy(memory+0x6000,ac4_row_audit.expected,ac4_row_audit.bytes);ctx=ac4_row_audit.continuation;
    before=ctx;memcpy(original,memory,sizeof memory);ac4_cloud_row_after(&ctx,NULL);
    assert(ac4_row_audit.matched==1 && ac4_row_audit.pieces==2);
    assert(ac4_row_audit.context_matched==1 && !ac4_row_audit.context_mismatched);
    assert(!memcmp(&ctx,&before,sizeof ctx) && !memcmp(memory,original,sizeof memory));
    ctx=row_entry;ac4_cloud_row_before(&ctx,NULL);memory[0x6010]^=1;
    ctx=ac4_row_audit.continuation;ctx.f[2].u^=1;before=ctx;
    ac4_cloud_row_after(&ctx,NULL);assert(ac4_row_audit.mismatched==1 && ac4_row_audit.context_mismatched==1);
    assert(!memcmp(&ctx,&before,sizeof ctx));
    ctx=row_entry;ac4_cloud_row_before(&ctx,NULL);before=ctx;memcpy(original,memory,sizeof memory);
    ac4_row_native_on=0;assert(!ac4_cloud_row_replace(&ctx,NULL));
    ac4_row_native_on=1;assert(!ac4_cloud_row_replace(&ctx,NULL)); /* unmapped */
    assert(!memcmp(&ctx,&before,sizeof ctx)&&!memcmp(memory,original,sizeof memory));
    ac4_row_audit.mapped=1;ctx.f[3].u^=1;ps2_ctx changed=ctx;
    assert(!ac4_cloud_row_replace(&ctx,NULL)&&!memcmp(&ctx,&changed,sizeof ctx)&&!memcmp(memory,original,sizeof memory));
    ctx=before;assert(ac4_cloud_row_replace(&ctx,NULL));
    assert(!memcmp(&ctx,&ac4_row_audit.continuation,sizeof ctx));
    assert(!memcmp(memory+0x6000,ac4_row_audit.expected,80));
    assert(!memcmp(memory,original,0x6000)&&!memcmp(memory+0x6050,original+0x6050,sizeof memory-0x6050));
    u64 shadow=ac4_row_audit.context_matched;
    ac4_cloud_row_after(&ctx,NULL);assert(ac4_row_audit.context_matched==shadow && ac4_row_audit.native_rows==1);
    before=ctx;memcpy(original,memory,sizeof memory);assert(!ac4_cloud_row_replace(&ctx,NULL));
    assert(!memcmp(&ctx,&before,sizeof ctx)&&!memcmp(memory,original,sizeof memory));
    ctx=row_entry;ctx.r[31].ud[0]=0;before=ctx;ac4_cloud_row_before(&ctx,NULL);
    assert(!ac4_cloud_row_replace(&ctx,NULL)&&!memcmp(&ctx,&before,sizeof ctx));
    ctx=row_entry;ac4_cloud_row_before(&ctx,NULL);ac4_row_audit.mapped=1;
    ac4_row_audit.address=PS2_RAM_SIZE-16;before=ctx;
    assert(!ac4_cloud_row_replace(&ctx,NULL)&&!memcmp(&ctx,&before,sizeof ctx)&&!memcmp(memory,original,sizeof memory));
    _putenv_s("PS2_AC4_ROW_NATIVE","1");hook_calls=removed=0;fail_hook=10;
    ac4_cloud_audit_init();assert(hook_calls==11 && removed==10);
    hook_calls=removed=0;fail_hook=-1;ac4_cloud_audit_init();assert(hook_calls==11 && !removed);
    _putenv_s("PS2_AC4_ROW_NATIVE","0");
    memset(&ctx,0,sizeof ctx);ctx.r[4].ud[0]=0x3000;ctx.r[5].ud[0]=3;ctx.r[6].ud[0]=0x1000;ctx.r[7].ud[0]=0x1200;ctx.r[8].ud[0]=0x1400;
    memcpy(memory+0x1000,a,sizeof a);memcpy(memory+0x1200,b,sizeof b);memcpy(memory+0x1400,colors,8);
    ps2_ctx band_entry=ctx;u64 good_context=ac4_sky_audit.band_context_matched,bad_context=ac4_sky_audit.band_context_mismatched;
    ac4_sky_band_before(&ctx,NULL);assert(ac4_sky_audit.ready && !memcmp(&ctx,&band_entry,sizeof ctx));
    memcpy(memory+0x3000,ac4_sky_audit.expected,ac4_sky_audit.count*16);ctx=ac4_sky_audit.band_continuation;before=ctx;
    ac4_sky_band_after(&ctx,NULL);assert(ac4_sky_audit.band_context_matched==good_context+1 && !memcmp(&ctx,&before,sizeof ctx));
    ctx=band_entry;ac4_sky_band_before(&ctx,NULL);ctx=ac4_sky_audit.band_continuation;ctx.r[6].ud[1]^=1;before=ctx;
    ac4_sky_band_after(&ctx,NULL);assert(ac4_sky_audit.band_context_mismatched==bad_context+1 && !memcmp(&ctx,&before,sizeof ctx));
    ctx=band_entry;ctx.r[31].ud[0]=0x1112b4;band_entry=ctx;ac4_sky_band_before(&ctx,NULL);
    before=ctx;memcpy(original,memory,sizeof memory);
    assert(!ac4_sky_band_replace(&ctx,NULL));ac4_sky_band_native_on=1;
    memory[0x1000]^=1;assert(!ac4_sky_band_replace(&ctx,NULL));memory[0x1000]^=1;
    ctx.f[0].u^=1;assert(!ac4_sky_band_replace(&ctx,NULL));ctx=before;
    ac4_sky_audit.packet=0x1000;assert(!ac4_sky_band_replace(&ctx,NULL));ac4_sky_audit.packet=0x3000;
    assert(!memcmp(&ctx,&before,sizeof ctx)&&!memcmp(memory,original,sizeof memory));
    assert(ac4_sky_band_replace(&ctx,NULL));assert(!memcmp(&ctx,&ac4_sky_audit.band_continuation,sizeof ctx));
    assert(!memcmp(memory+0x3000,ac4_sky_audit.expected,ac4_sky_audit.count*16));
    good_context=ac4_sky_audit.band_context_matched;ac4_sky_band_after(&ctx,NULL);
    assert(ac4_sky_audit.native_bands==1 && ac4_sky_audit.band_context_matched==good_context);
    assert(!ac4_sky_band_replace(&ctx,NULL));
    _putenv_s("PS2_AC4_SKY_BAND_NATIVE","1");hook_calls=removed=0;fail_hook=6;
    ac4_sky_audit_init();assert(hook_calls==7 && removed==6);
    hook_calls=removed=0;fail_hook=-1;ac4_sky_audit_init();assert(hook_calls==7&&!removed);
    _putenv_s("PS2_AC4_SKY_BAND_NATIVE","0");
    puts("AC4 sky audit: observer immutability, mismatches, bounds, signatures and hook rollback passed");
}
