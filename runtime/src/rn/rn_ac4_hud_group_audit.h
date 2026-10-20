/* Fixed-HUD allocation census; optional validated ownership, no guest writes. */
#include "rn_ac4_hud_group.h"
static const struct {u32 address,words[4];int audit_only;} ac4_hud_sites[]={
    {0x162958,{0x27bdff40,0x3c020040,0xffb700a0,0x3c030040},0},
    {0x1633c8,{0x27bdff20,0x3c020040,0xffb60090,0x3c030040},0},
    {0x163818,{0x27bdff50,0x3c020040,0xffb40080,0x3c060040},0},
    {0x163c08,{0x27bdff10,0x3c020041,0xffb40090,0x3c030041},0},
    {0x162d98,{0x27bdff70,0x3c020041,0xffb40070,0x3c030041},0},
    {0x1609d8,{0x27bdff20,0x8f82aab8,0xffb60090,0x3c060001},0},
    {0x161250,{0x27bdff20,0x8f82aab8,0xffb700a0,0x3c060001},0},
    {0x160ff8,{0x27bdff40,0x8f82aab8,0xffbe00a0,0x3c070001},0},
    {0x161890,{0x27bdff40,0x8f82aab8,0xffb60080,0x3c060001},0},
    {0x160e78,{0x27bdffa0,0x8f82aab8,0xffb30040,0x3c060001},0},
    {0x161720,{0x27bdffa0,0x8f82aab8,0xffb30040,0x3c060001},0},
    {0x169010,{0x27bdfdb0,0x3c020040,0xffb201a0,0x27ab0020},0},
    {0x16bc68,{0x27bdfe90,0x3c020041,0xffb00080,0x3c030041},1}
};

#define AC4_HUD_SITES (sizeof ac4_hud_sites/sizeof ac4_hud_sites[0])
static struct {
    int enabled,native;u32 depth;
    struct {u32 control,start,site;int valid;} stack[8];
    struct {u64 calls,matched,empty,declined,packets;int dumped;} site[AC4_HUD_SITES];
    u64 unpaired,queued,queue_declined;
} ac4_hud_audit;
static int ac4_hud_before(ps2_ctx *ctx,void *user) {
    u32 site=(u32)(uintptr_t)user;
    if(!ac4_hud_audit.enabled || site>=AC4_HUD_SITES)return 0;
    ac4_hud_audit.site[site].calls++;
    u32 d=ac4_hud_audit.depth++;
    if(d>=8){ac4_hud_audit.site[site].declined++;return 0;}
    ac4_hud_audit.stack[d].site=site;ac4_hud_audit.stack[d].valid=0;
    u32 gp=ctx->r[28].uw[0];
    if(gp<0x5548 || !ac4_screen_range(gp-0x5548,4))goto decline;
    u32 control=ps2_r32(gp-0x5548);
    if(!ac4_screen_range(control,0x3c))goto decline;
    u32 start=ps2_r32(control+0x38);
    if((start&15) || !ac4_screen_range(start,16))goto decline;
    ac4_hud_audit.stack[d].control=control;ac4_hud_audit.stack[d].start=start;
    ac4_hud_audit.stack[d].valid=1;return 0;
decline:ac4_hud_audit.site[site].declined++;return 0;
}
/* Bounded diagnostics for an unclaimed external chain. Never follows CALLs. */
static void ac4_hud_trace_chain(u32 address) {
    for(u32 i=0;i<16;i++) {
        if((address&15) || !ac4_screen_range(address,16))return;
        u32 tag[4];ps2_get_mem(tag,address,16);u32 id=(tag[0]>>28)&7,qwc=tag[0]&65535;
        ps2_log("ac4 HUD external tag: %08x %08x %08x %08x %08x",address,tag[0],tag[1],tag[2],tag[3]);
        u32 data=(id==0 || id==3 || id==4)?tag[1]:address+16;
        for(u32 q=0;q<qwc && q<4;q++) {
            if(!ac4_screen_range(data+q*16,16))return;
            u32 v[4];ps2_get_mem(v,data+q*16,16);
            ps2_log("ac4 HUD external data: %08x %08x %08x %08x %08x",data+q*16,v[0],v[1],v[2],v[3]);
        }
        if(id==1)address+=16+qwc*16;
        else if(id==2)address=tag[1];
        else if(id==3 || id==4)address+=16;
        else return;
    }
}
static void ac4_hud_trace_allocation(u32 start,u32 end) {
    u32 at=start;
    for(u32 i=0;i<64 && at<end;i++) {
        if(!ac4_screen_range(at,16))return;
        u32 tag[4];ps2_get_mem(tag,at,16);
        ps2_log("ac4 HUD allocation tag: %08x %08x %08x %08x %08x",at,tag[0],tag[1],tag[2],tag[3]);
        if((tag[0]>>28)==5)ac4_hud_trace_chain(tag[1]);
        u32 bytes=16+(tag[0]&65535)*16;if(bytes>end-at)return;
        at+=bytes;
    }
}
/* Private, opt-in packet evidence for unclassified producers; never ownership. */
static void ac4_hud_dump_candidate(u32 site,u32 start,const u8 *bytes,u32 count) {
    const char *dir=getenv("PS2_AC4_HUD_CANDIDATE_DUMP");
    if(!ac4_hud_sites[site].audit_only || ac4_hud_audit.site[site].dumped || !dir || !*dir)return;
    ac4_hud_audit.site[site].dumped=1;char path[1024];
    int n=snprintf(path,sizeof path,"%s/hud-%08x.bin",dir,ac4_hud_sites[site].address);
    if(n<0 || (size_t)n>=sizeof path)return;
    FILE *f=fopen(path,"wb");if(!f)return;
    int ok=fwrite(bytes,1,count,f)==count;if(fclose(f))ok=0;
    ps2_log("ac4 HUD candidate dump: site %08x start %08x bytes %u saved %d",ac4_hud_sites[site].address,start,count,ok);
}
static int ac4_hud_after(ps2_ctx *ctx,void *user) {
    (void)ctx;
    if(!ac4_hud_audit.enabled)return 0;
    if(!ac4_hud_audit.depth){ac4_hud_audit.unpaired++;return 0;}
    u32 d=--ac4_hud_audit.depth;
    if(d>=8 || !ac4_hud_audit.stack[d].valid)return 0;
    u32 site=ac4_hud_audit.stack[d].site,start=ac4_hud_audit.stack[d].start;
    if(site!=(u32)(uintptr_t)user){ac4_hud_audit.unpaired++;ac4_hud_audit.site[site].declined++;return 0;}
    u32 end=ps2_r32(ac4_hud_audit.stack[d].control+0x38);
    if(end==start){ac4_hud_audit.site[site].empty++;return 0;}
    u8 bytes[32768];ac4_hud_group plan;
    if(end<start || end-start>sizeof bytes || !ac4_screen_range(start,end-start))goto decline;
    ps2_get_mem(bytes,start,end-start);
    ac4_hud_dump_candidate(site,start,bytes,end-start);
    if(!ac4_hud_group_plan(bytes,end-start,&plan))goto decline;
    for(u32 i=0;i<plan.external_count;i++) {
        u32 a=plan.external[i].address;
        if(!ac4_screen_range(a,16))goto decline;
        u32 n=32+(ps2_r32(a)&65535)*16;u8 upload[16416];
        if(n>sizeof upload || !ac4_screen_range(a,n))goto decline;
        ps2_get_mem(upload,a,n);
        if(!ac4_hud_upload_valid(upload,n))goto decline;
        plan.external[i].bytes=n;
    }
    ac4_hud_audit.site[site].matched++;ac4_hud_audit.site[site].packets+=plan.count;
    if(ac4_hud_audit.native && !ac4_hud_sites[site].audit_only) {
        if(ac4_hud_queue(start,end,ac4_hud_sites[site].address,&plan))ac4_hud_audit.queued++;
        else ac4_hud_audit.queue_declined++;
    }
    return 0;
decline:
    if(ac4_hud_audit.site[site].declined++<2) {
        if(end>=start && end-start<=32768)ac4_hud_trace_allocation(start,end);
        ps2_log("ac4 HUD group decline: site %08x range %08x..%08x",ac4_hud_sites[site].address,start,end);
    }
    return 0;
}
static void ac4_hud_init(void) {
    const char *e=getenv("PS2_AC4_HUD_GROUP_AUDIT");
    const char *native=getenv("PS2_AC4_HUD_GROUP_NATIVE");
    int enabled=native && !strcmp(native,"1");
    if(ac4_hud_audit.enabled || (!enabled && (!e || strcmp(e,"1"))))return;
    for(u32 i=0;i<AC4_HUD_SITES;i++)for(u32 j=0;j<4;j++)if(ps2_r32(ac4_hud_sites[i].address+4*j)!=ac4_hud_sites[i].words[j])return;
    int handles[2*AC4_HUD_SITES];u32 count=0;
    for(u32 i=0;i<AC4_HUD_SITES;i++) {
        int h=ps2_hook_before(ac4_hud_sites[i].address,ac4_hud_before,(void*)(uintptr_t)i,100,"ac4-hud-group");
        if(h<0)goto rollback;
        handles[count++]=h;
        h=ps2_hook_after(ac4_hud_sites[i].address,ac4_hud_after,(void*)(uintptr_t)i,100,"ac4-hud-group");
        if(h<0)goto rollback;
        handles[count++]=h;
    }
    ac4_hud_audit.native=enabled;ac4_hud_audit.enabled=1;return;
rollback:while(count)ps2_hook_remove(handles[--count]);
}
static void ac4_hud_report(void) {
    if(!ac4_hud_audit.enabled)return;
    for(u32 i=0;i<AC4_HUD_SITES;i++)ps2_log("ac4 HUD %s: site %08x calls %llu matched %llu empty %llu declined %llu packets %llu",
        ac4_hud_sites[i].audit_only?"candidate":"group",ac4_hud_sites[i].address,(unsigned long long)ac4_hud_audit.site[i].calls,(unsigned long long)ac4_hud_audit.site[i].matched,
        (unsigned long long)ac4_hud_audit.site[i].empty,(unsigned long long)ac4_hud_audit.site[i].declined,(unsigned long long)ac4_hud_audit.site[i].packets);
    if(ac4_hud_audit.native)ps2_log("ac4 HUD group ownership: %llu queued, %llu declined",(unsigned long long)ac4_hud_audit.queued,(unsigned long long)ac4_hud_audit.queue_declined);
    ps2_log("ac4 HUD group balance: %u open, %llu unpaired",ac4_hud_audit.depth,(unsigned long long)ac4_hud_audit.unpaired);
}
