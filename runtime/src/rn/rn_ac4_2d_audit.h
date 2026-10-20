/* Read-only candidate-writer census. These sites are NOT HUD ownership claims. */
#include "ps2_hook.h"
#include <stdlib.h>
#include <string.h>
static const struct {u32 address,words[4];} ac4_2d_sites[]={
    {0x0015fee0u, {0x27bdff80u,0x8f82aab8u,0xffb60060u,0x3c0c0001u}},
    {0x00104100u, {0x8f82aab8u,0x8c430038u,0x0060102du,0x03e00008u}},
    {0x00104118u, {0x8f82bdb4u,0x0080502du,0x14460003u,0x00a0182du}},
    {0x001041e8u, {0x8f8baab8u,0x00061100u,0x8c8a0004u,0x24420010u}},
    {0x00112820u, {0x30a2ffffu,0x30c3ffffu,0x0002483cu,0x0009483fu}},
    {0x001128d8u, {0x00a0782du,0x00c0702du,0x31e2ffffu,0x00e0302du}},
    {0x00112a10u, {0x00a0702du,0x00c0682du,0x31c2ffffu,0x00e0302du}},
    {0x00115cf8u, {0x27bdffb0u,0x0080c02du,0xffb40040u,0x3c02108bu}},
    {0x0014c310u, {0x27bdff20u,0x3c085000u,0xffb700b0u,0x24060002u}},
    {0x001db578u, {0x27bdff90u,0x8f85b1d4u,0xffb30050u,0xffb20040u}},
    {0x001dbe70u, {0x27bdff90u,0x3c02002cu,0xffb30050u,0x3c0400ffu}},
    {0x00200258u, {0x27bdfe70u,0xffb40100u,0xffb200e0u,0x00a0a02du}},
    {0x0020a6a0u, {0x00e0702du,0x3c0141e0u,0x44810800u,0xc5c00010u}},
    {0x0020cf40u, {0x27bdff70u,0x00c0702du,0xffbe0080u,0x3c0b002cu}},
    {0x00237808u, {0x0080502du,0x3c02003au,0x3c046400u,0x0004203cu}},
};
#define AC4_2D_SITE_COUNT (sizeof ac4_2d_sites/sizeof ac4_2d_sites[0])
#define AC4_2D_CALLERS 512u
static struct {
    int enabled;
    unsigned count;
    u64 calls,overflow;
    struct {u32 writer,caller;u64 calls,args[4];} entries[AC4_2D_CALLERS];
} ac4_2d_audit;
static int ac4_2d_audit_before(ps2_ctx *ctx,void *user) {
    size_t site=(size_t)(uintptr_t)user;
    if(!ac4_2d_audit.enabled || site>=AC4_2D_SITE_COUNT)return 0;
    u32 writer=ac4_2d_sites[site].address,caller=ctx->r[31].uw[0];
    ac4_2d_audit.calls++;
    unsigned i;
    for(i=0;i<ac4_2d_audit.count;i++)
        if(ac4_2d_audit.entries[i].writer==writer && ac4_2d_audit.entries[i].caller==caller)break;
    if(i==ac4_2d_audit.count) {
        if(i==AC4_2D_CALLERS){ac4_2d_audit.overflow++;return 0;}
        ac4_2d_audit.count++;
        ac4_2d_audit.entries[i].writer=writer;ac4_2d_audit.entries[i].caller=caller;
        for(unsigned a=0;a<4;a++)ac4_2d_audit.entries[i].args[a]=ctx->r[4+a].ud[0];
    }
    ac4_2d_audit.entries[i].calls++;
    return 0;
}
static void ac4_2d_audit_init(void) {
    const char *e=getenv("PS2_AC4_2D_AUDIT");
    if(ac4_2d_audit.enabled || !e || strcmp(e,"1"))return;
    for(size_t i=0;i<AC4_2D_SITE_COUNT;i++)for(unsigned w=0;w<4;w++)
        if(ps2_r32(ac4_2d_sites[i].address+4*w)!=ac4_2d_sites[i].words[w]) {
            ps2_log("ac4 2d audit: signature mismatch at %08x; disabled",ac4_2d_sites[i].address);return;
        }
    int handles[AC4_2D_SITE_COUNT];
    for(size_t i=0;i<AC4_2D_SITE_COUNT;i++) {
        handles[i]=ps2_hook_before(ac4_2d_sites[i].address,ac4_2d_audit_before,(void*)(uintptr_t)i,110,"ac4-2d-audit");
        if(handles[i]<0) {
            for(size_t j=0;j<i;j++)ps2_hook_remove(handles[j]);
            ps2_log("ac4 2d audit: hook registration failed; disabled");return;
        }
    }
    ac4_2d_audit.enabled=1;
    ps2_log("ac4 2d audit: read-only candidate writer census enabled; no HUD claims");
}
static void ac4_2d_audit_report(void) {
    if(!ac4_2d_audit.enabled)return;
    ps2_log("ac4 2d audit: %llu calls, %u caller pairs, %llu overflow calls",
        (unsigned long long)ac4_2d_audit.calls,ac4_2d_audit.count,(unsigned long long)ac4_2d_audit.overflow);
    for(unsigned i=0;i<ac4_2d_audit.count;i++)
        ps2_log("ac4 2d candidate: writer=%08x caller=%08x calls=%llu first_args=%016llx,%016llx,%016llx,%016llx",
            ac4_2d_audit.entries[i].writer,ac4_2d_audit.entries[i].caller,
            (unsigned long long)ac4_2d_audit.entries[i].calls,
            (unsigned long long)ac4_2d_audit.entries[i].args[0],(unsigned long long)ac4_2d_audit.entries[i].args[1],
            (unsigned long long)ac4_2d_audit.entries[i].args[2],(unsigned long long)ac4_2d_audit.entries[i].args[3]);
}
