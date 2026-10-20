/* AC4 EE sky evidence and separately gated band replacement. */
#include "rn_ac4_sky_dome.h"
#include "rn_ac4_sky_haze.h"
#include "rn_ac4_sky_continuation.h"

static struct {
    unsigned depth;
    int ready;
    u32 packet;
    size_t count;
    ac4_sky_qword expected[AC4_SKY_BAND_QWORDS];
    u64 calls, matched, mismatched, declined, domes, hazes;
    u64 dome_matched, dome_mismatched, dome_declined;
    const char *snapshot;
    int saved;
    int dome_ready;
    u32 dome_packet;
    ac4_sky_dome_input dome_input;
    int haze_ready;
    u32 haze_packet;
    ac4_sky_haze_input haze_input;
    u64 haze_matched,haze_mismatched,haze_declined;
    ps2_ctx band_entry,band_continuation;
    ac4_sky_point band_a[33],band_b[33];u32 band_colors[2];
    int band_replaced;
    u64 native_bands,native_declined;
    u64 band_context_matched,band_context_mismatched;
} ac4_sky_audit;
static int ac4_sky_band_native_on;

static int ac4_sky_range(u32 address, size_t bytes) {
    if (address < PS2_RAM_SIZE) return bytes <= PS2_RAM_SIZE - address;
    if (address >= 0x70000000u && address < 0x70004000u)
        return bytes <= 0x70004000u - address;
    return 0;
}

static int ac4_sky_band_before(ps2_ctx *ctx, void *user) {
    ac4_sky_point a[33], b[33];
    (void)user;
    ac4_sky_audit.calls++;
    if (ac4_sky_audit.depth++) { ac4_sky_audit.declined++; return 0; }
    ac4_sky_audit.ready = 0;
    ac4_sky_audit.band_replaced=0;
    u32 pa = ctx->r[6].uw[0], pb = ctx->r[7].uw[0], colors = ctx->r[8].uw[0];
    if (!ac4_sky_range(pa, sizeof a) || !ac4_sky_range(pb, sizeof b)
        || !ac4_sky_range(colors, 8)) { ac4_sky_audit.declined++; return 0; }
    ps2_get_mem(a, pa, sizeof a); ps2_get_mem(b, pb, sizeof b);
    ac4_sky_audit.packet = ctx->r[4].uw[0];
    ac4_sky_audit.band_entry=ac4_sky_audit.band_continuation=*ctx;
    memcpy(ac4_sky_audit.band_a,a,sizeof a);memcpy(ac4_sky_audit.band_b,b,sizeof b);
    ac4_sky_audit.band_colors[0]=ps2_r32(colors);ac4_sky_audit.band_colors[1]=ps2_r32(colors+4);
    ac4_sky_audit.ready = ac4_sky_band_plan(&ac4_sky_audit.band_continuation,a,b,
        ps2_r32(colors), ps2_r32(colors + 4),
        ac4_sky_audit.expected, AC4_SKY_BAND_QWORDS, &ac4_sky_audit.count);
    if (!ac4_sky_audit.ready) ac4_sky_audit.declined++;
    return 0;
}

static int ac4_sky_band_replace(ps2_ctx *ctx,void *user){
    (void)user;u32 caller=ctx->r[31].uw[0];
    if(!ac4_sky_band_native_on || (caller!=0x001112b4 && caller!=0x00111538))return 0;
    size_t bytes=ac4_sky_audit.count*16;u32 out=ac4_sky_audit.packet;
    if(!ac4_sky_audit.ready || ac4_sky_audit.depth!=1 || ac4_sky_audit.band_replaced
       || memcmp(ctx,&ac4_sky_audit.band_entry,sizeof *ctx) || !ac4_sky_range(out,bytes))goto decline;
    /* Snapshot planning cannot reproduce writes aliasing later input reads. */
    const u32 input[3]={ctx->r[6].uw[0],ctx->r[7].uw[0],ctx->r[8].uw[0]};
    const size_t sizes[3]={sizeof ac4_sky_audit.band_a,sizeof ac4_sky_audit.band_b,8};
    for(unsigned i=0;i<3;i++)if(bytes && (u64)out<(u64)input[i]+sizes[i] && (u64)input[i]<(u64)out+bytes)goto decline;
    ac4_sky_point a[33],b[33];u32 colors[2];
    ps2_get_mem(a,input[0],sizeof a);ps2_get_mem(b,input[1],sizeof b);ps2_get_mem(colors,input[2],8);
    if(memcmp(a,ac4_sky_audit.band_a,sizeof a) || memcmp(b,ac4_sky_audit.band_b,sizeof b)
       || memcmp(colors,ac4_sky_audit.band_colors,8))goto decline;
    if(bytes)ps2_put_mem(out,ac4_sky_audit.expected,bytes);
    *ctx=ac4_sky_audit.band_continuation;ac4_sky_audit.band_replaced=1;ac4_sky_audit.native_bands++;return 1;
decline:
    ac4_sky_audit.native_declined++;return 0;
}

static int ac4_sky_band_after(ps2_ctx *ctx, void *user) {
    ac4_sky_qword actual[AC4_SKY_BAND_QWORDS];
    (void)user;
    if (!ac4_sky_audit.depth || --ac4_sky_audit.depth) return 0;
    if (!ac4_sky_audit.ready) return 0;
    ac4_sky_audit.ready = 0;
    if(memcmp(ctx,&ac4_sky_audit.band_continuation,sizeof *ctx)){
        if(ac4_sky_audit.band_context_mismatched<3){
            const u8 *a=(const u8*)ctx,*b=(const u8*)&ac4_sky_audit.band_continuation;
            for(size_t i=0;i<sizeof *ctx;i++)if(a[i]!=b[i]){ps2_log("ac4 sky band context mismatch: byte %u actual=%02x expected=%02x",(unsigned)i,a[i],b[i]);break;}
        }
        ac4_sky_audit.band_context_mismatched++;
    }else if(!ac4_sky_audit.band_replaced)ac4_sky_audit.band_context_matched++;
    size_t bytes = ac4_sky_audit.count * sizeof actual[0];
    u32 start = ac4_sky_audit.packet;
    if (!ac4_sky_range(start, bytes)) { ac4_sky_audit.declined++; return 0; }
    ps2_get_mem(actual, start, bytes);
    if (ctx->r[2].ud[0] != (u64)start + bytes
        || memcmp(actual, ac4_sky_audit.expected, bytes)) ac4_sky_audit.mismatched++;
    else ac4_sky_audit.matched++;
    return 0;
}

static int ac4_sky_dome_before(ps2_ctx *ctx, void *user) {
    (void)user;
    u8 object[0x440];u32 address=ctx->r[5].uw[0],scratch=ctx->r[6].uw[0];
    ac4_sky_audit.dome_ready=0;
    if (!ac4_sky_range(address,sizeof object)
        || !ac4_sky_range(scratch,sizeof ac4_sky_audit.dome_input.initial_rows)) return 0;
    ps2_get_mem(object,address,sizeof object);
    if (!ac4_sky_dome_read(object,sizeof object,ctx->r[7].uw[0],&ac4_sky_audit.dome_input)) return 0;
    ps2_get_mem(ac4_sky_audit.dome_input.initial_rows,scratch,sizeof ac4_sky_audit.dome_input.initial_rows);
    ac4_sky_audit.dome_packet=ctx->r[4].uw[0];ac4_sky_audit.dome_ready=1;
    return 0;
}

static int ac4_sky_haze_before(ps2_ctx *ctx,void *user) {
    (void)user;u8 object[0x440];
    u32 address=ctx->r[5].uw[0],scratch=ctx->r[6].uw[0],camera=ctx->r[7].uw[0];
    u32 global=ctx->r[28].uw[0]-0x551cu;
    ac4_sky_audit.haze_ready=0;
    if(!ac4_sky_range(address,sizeof object) || !ac4_sky_range(scratch,528)
       || !ac4_sky_range(camera,0x9c) || !ac4_sky_range(global,4))return 0;
    u32 distances=ps2_r32(global);if(!ac4_sky_range(distances,16))return 0;
    ac4_sky_haze_input *h=&ac4_sky_audit.haze_input;
    ps2_get_mem(object,address,sizeof object);
    if(!ac4_sky_dome_read(object,sizeof object,0,&h->sky))return 0;
    ps2_get_mem(h->sky.initial_rows,scratch,sizeof h->sky.initial_rows);
    ps2_get_mem(&h->near_distance,distances+8,4);ps2_get_mem(&h->far_distance,distances+12,4);
    ps2_get_mem(&h->depth_offset,camera+0x88,4);ps2_get_mem(&h->depth_scale,camera+0x98,4);
    ac4_sky_audit.haze_packet=ctx->r[4].uw[0];ac4_sky_audit.haze_ready=1;return 0;
}

static int ac4_sky_producer_after(ps2_ctx *ctx, void *user) {
    int ok0 = 0, ok1 = 0;
    u32 packet = ps2_hook_entry_arg(0, &ok0), object = ps2_hook_entry_arg(1, &ok1);
    u32 end = ctx->r[2].uw[0];
    if (user) {
        ac4_sky_audit.hazes++;
        int ready=ac4_sky_audit.haze_ready;ac4_sky_audit.haze_ready=0;
        static ac4_sky_qword expected[AC4_SKY_HAZE_QWORDS],actual[AC4_SKY_HAZE_QWORDS];size_t count=0;
        if(!ok0 || !ok1 || !ready || packet!=ac4_sky_audit.haze_packet || end<packet || (end-packet)%16
           || end-packet>sizeof actual || !ac4_sky_range(packet,end-packet)
           || !ac4_sky_haze_build(&ac4_sky_audit.haze_input,expected,AC4_SKY_HAZE_QWORDS,&count))
            ac4_sky_audit.haze_declined++;
        else {
            ps2_get_mem(actual,packet,end-packet);
            if(count*16!=end-packet || memcmp(expected,actual,end-packet))ac4_sky_audit.haze_mismatched++;
            else {
                ac4_sky_audit.haze_matched++;
                ac4_sky_queue(packet,end,&ac4_sky_audit.haze_input,sizeof ac4_sky_audit.haze_input);
            }
        }
        return 0;
    }
    ac4_sky_audit.domes++;
    {
        int ready=ac4_sky_audit.dome_ready;ac4_sky_audit.dome_ready=0;
        static ac4_sky_qword expected[AC4_SKY_DOME_QWORDS], actual[AC4_SKY_DOME_QWORDS];
        size_t count=0;
        if (!ok0 || !ok1 || !ready || packet!=ac4_sky_audit.dome_packet || end<packet || (end-packet)%16
            || end-packet>sizeof actual
            || !ac4_sky_range(packet,end-packet)) ac4_sky_audit.dome_declined++;
        else {
            ps2_get_mem(actual,packet,end-packet);
            /* Seed only for untouched PRIM payload padding. All meaningful
             * packet words must be generated by the independent native plan. */
            memset(expected,0,sizeof expected);memcpy(expected,actual,end-packet);
            if (!ac4_sky_dome_build(&ac4_sky_audit.dome_input,expected,AC4_SKY_DOME_QWORDS,&count))
                ac4_sky_audit.dome_declined++;
            else if (count*16 != end-packet || memcmp(expected,actual,end-packet))
                ac4_sky_audit.dome_mismatched++;
            else {
                ac4_sky_audit.dome_matched++;
                ac4_sky_queue(packet,end,&ac4_sky_audit.dome_input,sizeof ac4_sky_audit.dome_input);
            }
        }
    }
    if (!ac4_sky_audit.snapshot || ac4_sky_audit.saved || !ok0 || !ok1
        || end < packet || end - packet > 65536u
        || !ac4_sky_range(object, 0x440) || !ac4_sky_range(packet, end-packet)) return 0;
    /* A bounded private fixture: magic, object size, packet size, guest addresses,
     * then original object and original GIF packet. Never needed by normal play. */
    u32 header[6] = {0x31594b53u, 0x440u, end-packet, object, packet, end};
    u8 *data = malloc(0x440u + end-packet);
    if (!data) return 0;
    ps2_get_mem(data, object, 0x440u);
    ps2_get_mem(data + 0x440u, packet, end-packet);
    FILE *f = fopen(ac4_sky_audit.snapshot, "wb");
    if (f) {
        int ok = fwrite(header, sizeof header, 1, f) == 1;
        ok = (fwrite(data, 0x440u + end-packet, 1, f) == 1) && ok;
        ok = (fclose(f) == 0) && ok;
        if (ok) { ac4_sky_audit.saved = 1; ps2_log("ac4 sky audit: saved dome fixture to %s", ac4_sky_audit.snapshot); }
    }
    free(data);
    return 0;
}

static void ac4_sky_audit_init(void) {
    const char *enabled = getenv("PS2_AC4_SKY_AUDIT");
    const char *mesh=getenv("PS2_AC4_SKY_MESH");
    const char *native=getenv("PS2_AC4_SKY_BAND_NATIVE");ac4_sky_band_native_on=native && !strcmp(native,"1");
    if ((!enabled || strcmp(enabled, "1")) && (!mesh || strcmp(mesh,"1")) && !ac4_sky_band_native_on) return;
    static const u32 addresses[3] = {0x00111050u, 0x00111138u, 0x00111310u};
    static const u32 words[3][2] = {{0x0080702du,0x9d0d0004u},
        {0x27bdff30u,0x3c031000u},{0x27bdfed0u,0x8f82aae4u}};
    for (unsigned i=0;i<3;++i)
        if (ps2_r32(addresses[i]) != words[i][0] || ps2_r32(addresses[i]+4) != words[i][1]) {
            ps2_log("ac4 sky audit: guest code signature mismatch; disabled"); return;
        }
    int handles[7],count=6;
    handles[0] = ps2_hook_before(addresses[0], ac4_sky_band_before, NULL, 100, "ac4-sky-audit");
    handles[1] = ps2_hook_after(addresses[0], ac4_sky_band_after, NULL, 100, "ac4-sky-audit");
    handles[2] = ps2_hook_after(addresses[1], ac4_sky_producer_after, NULL, 100, "ac4-sky-audit");
    handles[3] = ps2_hook_after(addresses[2], ac4_sky_producer_after, (void *)(uintptr_t)1, 100, "ac4-sky-audit");
    handles[4] = ps2_hook_before(addresses[1], ac4_sky_dome_before, NULL, 100, "ac4-sky-audit");
    handles[5] = ps2_hook_before(addresses[2], ac4_sky_haze_before, NULL, 100, "ac4-sky-audit");
    if(ac4_sky_band_native_on)handles[count++]=ps2_hook_replace(addresses[0],ac4_sky_band_replace,NULL,100,"ac4-native-band");
    for (int i=0;i<count;++i) if (handles[i]<0) {
        for (int j=0;j<count;++j) if (handles[j]>=0) ps2_hook_remove(handles[j]);
        ps2_log("ac4 sky audit: hook registration failed; disabled"); return;
    }
    ac4_sky_audit.snapshot = getenv("PS2_AC4_SKY_SNAPSHOT");
    ps2_log("ac4 sky audit: observing original dome, haze and band writers%s", mesh && !strcmp(mesh,"1") ? "; native mesh submission requested" : "; no rendering replacement");
}

static void ac4_sky_audit_report(void) {
    if(ac4_sky_band_native_on)ps2_log("ac4 native sky bands: %llu replaced, %llu declined",
        (unsigned long long)ac4_sky_audit.native_bands,(unsigned long long)ac4_sky_audit.native_declined);
    if(ac4_sky_audit.calls)ps2_log("ac4 sky band continuation: %llu matched, %llu mismatched",
        (unsigned long long)ac4_sky_audit.band_context_matched,(unsigned long long)ac4_sky_audit.band_context_mismatched);
    if(ac4_sky_audit.hazes)
        ps2_log("ac4 sky haze plan: %llu matched, %llu mismatched, %llu declined",
            (unsigned long long)ac4_sky_audit.haze_matched,(unsigned long long)ac4_sky_audit.haze_mismatched,
            (unsigned long long)ac4_sky_audit.haze_declined);
    if (ac4_sky_audit.domes)
        ps2_log("ac4 sky dome plan: %llu matched, %llu mismatched, %llu declined",
            (unsigned long long)ac4_sky_audit.dome_matched,
            (unsigned long long)ac4_sky_audit.dome_mismatched,
            (unsigned long long)ac4_sky_audit.dome_declined);
    if (!ac4_sky_audit.calls && !ac4_sky_audit.domes && !ac4_sky_audit.hazes) return;
    ps2_log("ac4 sky audit: %llu bands, %llu matched, %llu mismatched, %llu declined; %llu domes, %llu hazes",
        (unsigned long long)ac4_sky_audit.calls, (unsigned long long)ac4_sky_audit.matched,
        (unsigned long long)ac4_sky_audit.mismatched, (unsigned long long)ac4_sky_audit.declined,
        (unsigned long long)ac4_sky_audit.domes, (unsigned long long)ac4_sky_audit.hazes);
}
