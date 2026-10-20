/* Original-helper packet shadow. No native ownership or guest writes. */
#include "rn_ac4_screen_sprite.h"
#include "ps2_hook.h"
#include <stdlib.h>
#include <string.h>
#define AC4_SCREEN_SHADOW_DEPTH 8u
static struct {
    int enabled,native;
    unsigned depth;
    u64 calls,matched,mismatched,declined,unpaired;
    struct {int ready;u32 packet,control,caller;u8 expected[64];} stack[AC4_SCREEN_SHADOW_DEPTH];
} ac4_screen_shadow;
static int ac4_screen_range(u32 a,size_t n){return a<PS2_RAM_SIZE && n<=PS2_RAM_SIZE-a;}
static int ac4_screen_before(ps2_ctx *ctx,void *user) {
    (void)user;if(!ac4_screen_shadow.enabled)return 0;
    unsigned d=ac4_screen_shadow.depth++;ac4_screen_shadow.calls++;
    if(d>=AC4_SCREEN_SHADOW_DEPTH){ac4_screen_shadow.declined++;return 0;}
    ac4_screen_shadow.stack[d].ready=0;
    u32 gp=ctx->r[28].uw[0],sp=ctx->r[29].uw[0];
    if(gp<0x5548u || !ac4_screen_range(gp-0x5548u,4) || !ac4_screen_range(sp,4))goto decline;
    u32 control=ps2_r32(gp-0x5548u);
    if(!ac4_screen_range(control,0x3c))goto decline;
    u32 start=ps2_r32(control+0x38);
    if((start&15u) || !ac4_screen_range(start,80))goto decline;
    ac4_screen_sprite in={ctx->r[6].uh[0],ctx->r[7].uh[0],ctx->r[8].uh[0],ctx->r[9].uh[0],
        ctx->r[10].uh[0],ctx->r[11].uh[0],ps2_r32(control),ps2_r32(sp)};
    if(!ac4_screen_sprite_build(&in,ac4_screen_shadow.stack[d].expected,64))goto decline;
    ac4_screen_shadow.stack[d].caller=ctx->r[31].uw[0];
    ac4_screen_shadow.stack[d].control=control;ac4_screen_shadow.stack[d].packet=start+16;
    ac4_screen_shadow.stack[d].ready=1;return 0;
 decline:ac4_screen_shadow.declined++;return 0;
}
static int ac4_screen_after(ps2_ctx *ctx,void *user) {
    (void)user;if(!ac4_screen_shadow.enabled)return 0;
    if(!ac4_screen_shadow.depth){ac4_screen_shadow.unpaired++;return 0;}
    unsigned d=--ac4_screen_shadow.depth;
    if(d>=AC4_SCREEN_SHADOW_DEPTH || !ac4_screen_shadow.stack[d].ready)return 0;
    ac4_screen_shadow.stack[d].ready=0;
    u32 p=ac4_screen_shadow.stack[d].packet;u8 actual[64];ps2_get_mem(actual,p,64);
    if(ctx->r[2].ud[0]!=0 || ps2_r32(ac4_screen_shadow.stack[d].control+0x38)!=p+64
       || ps2_r32(p-16)!=0x20000004u || ps2_r32(p-8)!=0 || ps2_r32(p-4)!=0x50000004u
       || memcmp(actual,ac4_screen_shadow.stack[d].expected,64)) {
        ac4_screen_shadow.mismatched++;
        if(ac4_screen_shadow.mismatched<=3)ps2_log("ac4 screen sprite mismatch: packet=%08x",p);
    }else {
        ac4_screen_shadow.matched++;
        if(ac4_screen_shadow.native)ac4_screen_queue(p,ac4_screen_shadow.stack[d].caller);
    }
    return 0;
}
static void ac4_screen_shadow_init(void) {
    const char *e=getenv("PS2_AC4_SCREEN_SPRITE_AUDIT"),*native=getenv("PS2_AC4_SCREEN_2D");
    if(ac4_screen_shadow.enabled)return;
    ac4_screen_shadow.native=native && !strcmp(native,"1");
    if((!e || strcmp(e,"1")) && !ac4_screen_shadow.native)return;
    const u32 addresses[]={0x15fee0,0x1041e8};
    const u32 words[2][4]={{0x27bdff80,0x8f82aab8,0xffb60060,0x3c0c0001},
                         {0x8f8baab8,0x00061100,0x8c8a0004,0x24420010}};
    for(unsigned i=0;i<2;i++)for(unsigned j=0;j<4;j++)if(ps2_r32(addresses[i]+4*j)!=words[i][j]) {
        ps2_log("ac4 screen sprite shadow: signature mismatch; disabled");return;
    }
    int before=ps2_hook_before(0x15fee0,ac4_screen_before,NULL,100,"ac4-screen-shadow");
    if(before<0){ps2_log("ac4 screen sprite shadow: hook registration failed; disabled");return;}
    int after=ps2_hook_after(0x15fee0,ac4_screen_after,NULL,100,"ac4-screen-shadow");
    if(after<0){ps2_hook_remove(before);ps2_log("ac4 screen sprite shadow: hook registration failed; disabled");return;}
    ac4_screen_shadow.enabled=1;
    ps2_log("ac4 screen sprite shadow: original helper and allocator retained");
}
static void ac4_screen_shadow_report(void) {
    if(ac4_screen_shadow.enabled)ps2_log("ac4 screen sprite shadow: %llu calls, %llu matched, %llu mismatched, %llu declined, %llu unpaired, %u open",
        (unsigned long long)ac4_screen_shadow.calls,(unsigned long long)ac4_screen_shadow.matched,
        (unsigned long long)ac4_screen_shadow.mismatched,(unsigned long long)ac4_screen_shadow.declined,
        (unsigned long long)ac4_screen_shadow.unpaired,ac4_screen_shadow.depth);
}
