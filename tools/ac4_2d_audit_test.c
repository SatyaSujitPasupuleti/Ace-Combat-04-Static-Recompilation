#include "ps2_runtime.h"
#include "ps2_hook.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int registrations,removed,fail_at=-1,bad_signature;
static u32 signature_read(u32 a);
#define ps2_r32 signature_read
#define ps2_log audit_log
static void audit_log(const char *fmt,...){(void)fmt;}
#include "../runtime/src/rn/rn_ac4_2d_audit.h"
static u32 signature_read(u32 a) {
    for(size_t i=0;i<AC4_2D_SITE_COUNT;i++)for(unsigned w=0;w<4;w++)
        if(a==ac4_2d_sites[i].address+4*w)return ac4_2d_sites[i].words[w]^(bad_signature?1u:0u);
    assert(0);return 0;
}
int ps2_hook_before(u32 a,ps2_hook_fn fn,void *user,int priority,const char *owner) {
    assert(a==ac4_2d_sites[(size_t)user].address && fn==ac4_2d_audit_before);
    assert(priority==110 && !strcmp(owner,"ac4-2d-audit"));
    int n=registrations++;return n==fail_at?-1:n+1;
}
int ps2_hook_remove(int h){assert(h>0);removed++;return 0;}
int main(void) {
    _putenv_s("PS2_AC4_2D_AUDIT","0");ac4_2d_audit_init();assert(!registrations);
    _putenv_s("PS2_AC4_2D_AUDIT","1");bad_signature=1;ac4_2d_audit_init();assert(!registrations);
    bad_signature=0;
    for(int i=0;i<(int)AC4_2D_SITE_COUNT;i++) {
        registrations=removed=0;fail_at=i;ac4_2d_audit_init();
        assert(registrations==i+1 && removed==i && !ac4_2d_audit.enabled);
    }
    registrations=removed=0;fail_at=-1;ac4_2d_audit_init();ac4_2d_audit_init();
    assert(registrations==AC4_2D_SITE_COUNT && !removed && ac4_2d_audit.enabled);
    ps2_ctx ctx;memset(&ctx,0x5a,sizeof ctx);ctx.r[31].uw[0]=0x123456;
    ps2_ctx saved=ctx;
    assert(!ac4_2d_audit_before(&ctx,0));assert(!memcmp(&ctx,&saved,sizeof ctx));
    assert(ac4_2d_audit.count==1 && ac4_2d_audit.calls==1);
    assert(ac4_2d_audit.entries[0].args[0]==ctx.r[4].ud[0]);
    ctx.r[4].ud[0]++;ac4_2d_audit_before(&ctx,0);
    assert(ac4_2d_audit.count==1 && ac4_2d_audit.entries[0].calls==2);
    assert(ac4_2d_audit.entries[0].args[0]==saved.r[4].ud[0]);
    ac4_2d_audit_before(&ctx,(void*)1);assert(ac4_2d_audit.count==2);
    ac4_2d_audit_before(&ctx,(void*)AC4_2D_SITE_COUNT);assert(ac4_2d_audit.calls==3);
    for(unsigned i=2;i<AC4_2D_CALLERS;i++){ctx.r[31].uw[0]=i;ac4_2d_audit_before(&ctx,0);}
    assert(ac4_2d_audit.count==AC4_2D_CALLERS && !ac4_2d_audit.overflow);
    ctx.r[31].uw[0]=0xffffffff; saved=ctx;ac4_2d_audit_before(&ctx,0);
    assert(ac4_2d_audit.overflow==1 && !memcmp(&ctx,&saved,sizeof ctx));
    ctx.r[31].uw[0]=0x123456;ac4_2d_audit_before(&ctx,0);
    assert(ac4_2d_audit.entries[0].calls==3 && ac4_2d_audit.overflow==1);
    ac4_2d_audit_report();puts("AC4 2D audit: immutable context, signatures, rollback and bounded counters passed");
    return 0;
}
