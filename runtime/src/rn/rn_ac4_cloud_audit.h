/* AC4 cloud observers and separately gated, validated row replacement. */
#include "rn_ac4_cloud_project.h"
#include "rn_ac4_cloud_packet.h"
#include "rn_ac4_cloud_sprite.h"
#include "rn_ac4_cloud_row.h"
#include "rn_ac4_cloud_sprite_packet.h"
static struct {
    int active,failed;size_t count;u64 mapped,unmapped;
    ac4_cloud_sprite_record records[4096];
} ac4_sprite_records;
static int ac4_cloud_sprites_begin(ps2_ctx *ctx,void *user){
    (void)ctx;(void)user;ac4_sprite_records.active=1;ac4_sprite_records.failed=0;ac4_sprite_records.count=0;return 0;
}
static int ac4_cloud_sprites_end(ps2_ctx *ctx,void *user){
    (void)ctx;(void)user;ac4_sprite_records.active=0;return 0;
}
static struct {
    int ready,mapped;u32 address;size_t bytes,index;
    u8 expected[AC4_CLOUD_ROW_BYTES];
    u64 calls,matched,mismatched,declined,pieces;
    ps2_ctx entry,continuation;
    u64 context_matched,context_mismatched;
    int replaced;
    u64 native_rows,native_declined;
} ac4_row_audit;
static int ac4_row_native_on;
static int ac4_cloud_row_before(ps2_ctx *ctx,void *user){
    (void)user;ac4_row_audit.ready=0;ac4_row_audit.mapped=0;ac4_row_audit.replaced=0;
    if(ctx->r[31].uw[0]!=0x00115ab0)return 0;
    ac4_row_audit.calls++;ac4_row_audit.address=ctx->r[4].uw[0];
    size_t index;
    if(ac4_sprite_records.active && !ac4_sprite_records.failed
       && ac4_cloud_sprite_find(ac4_sprite_records.records,ac4_sprite_records.count,
           ctx->r[5].ud[0],ctx->r[6].ud[0],ctx->r[7].ud[0],ctx->r[8].ud[0],&index)){
        ac4_sprite_records.mapped++;ac4_row_audit.mapped=1;ac4_row_audit.index=index;
    }
    else ac4_sprite_records.unmapped++;
    ac4_row_audit.entry=ac4_row_audit.continuation=*ctx;
    if(!ac4_cloud_row_plan(&ac4_row_audit.continuation,
          ac4_row_audit.expected,sizeof ac4_row_audit.expected,&ac4_row_audit.bytes)
       || !ac4_sky_range(ac4_row_audit.address,ac4_row_audit.bytes)){
        ac4_row_audit.declined++;return 0;
    }
    ac4_row_audit.ready=1;return 0;
}
static int ac4_cloud_row_replace(ps2_ctx *ctx,void *user){
    (void)user;
    if(!ac4_row_native_on || ctx->r[31].uw[0]!=0x00115ab0)return 0;
    if(!ac4_row_audit.ready || !ac4_row_audit.mapped || ac4_row_audit.replaced
       || memcmp(ctx,&ac4_row_audit.entry,sizeof *ctx)
       || !ac4_sky_range(ac4_row_audit.address,ac4_row_audit.bytes)){
        ac4_row_audit.native_declined++;return 0;
    }
    if(ac4_row_audit.bytes)ps2_put_mem(ac4_row_audit.address,ac4_row_audit.expected,ac4_row_audit.bytes);
    *ctx=ac4_row_audit.continuation;
    ac4_row_audit.replaced=1;ac4_row_audit.native_rows++;return 1;
}
static int ac4_cloud_row_after(ps2_ctx *ctx,void *user){
    (void)user;if(!ac4_row_audit.ready)return 0;ac4_row_audit.ready=0;
    if(memcmp(ctx,&ac4_row_audit.continuation,sizeof *ctx)) {
        if(ac4_row_audit.context_mismatched<3) {
            const u8 *a=(const u8*)ctx,*b=(const u8*)&ac4_row_audit.continuation;
            for(size_t i=0;i<sizeof *ctx;i++)if(a[i]!=b[i]){ps2_log("ac4 row context mismatch: byte %u actual=%02x expected=%02x",(unsigned)i,a[i],b[i]);break;}
        }
        ac4_row_audit.context_mismatched++;
    }else if(!ac4_row_audit.replaced)ac4_row_audit.context_matched++;
    u8 actual[AC4_CLOUD_ROW_BYTES];ps2_get_mem(actual,ac4_row_audit.address,ac4_row_audit.bytes);
    if(ctx->r[2].uw[0]!=ac4_row_audit.address+ac4_row_audit.bytes
       || memcmp(actual,ac4_row_audit.expected,ac4_row_audit.bytes)){
        if(ac4_row_audit.mismatched<3)ps2_log("ac4 sprite row mismatch: start=%08x expected=%u actual=%08x",
            ac4_row_audit.address,(unsigned)ac4_row_audit.bytes,ctx->r[2].uw[0]);
        ac4_row_audit.mismatched++;
    }else {ac4_row_audit.matched++;if(ac4_row_audit.bytes){
        ac4_row_audit.pieces+=(ac4_row_audit.bytes-16)/32;
        if(ac4_row_audit.mapped)ac4_sprite_queue(ac4_row_audit.address,ctx->r[2].uw[0],
            ac4_sprite_records.records+ac4_row_audit.index,sizeof(ac4_cloud_sprite_record));
    }}
    return 0;
}
static struct {
    int active,failed;u32 start;size_t count;
    ac4_cloud_record records[1600];
    ac4_cloud_mesh_vertex mapped[1600];
    u64 batches,matched,declined,quads;
} ac4_cloud_packets;
static int ac4_cloud_planes_before(ps2_ctx *ctx,void *user){
    (void)user;ac4_cloud_packets.active=1;ac4_cloud_packets.failed=0;
    ac4_cloud_packets.start=ctx->r[4].uw[0];ac4_cloud_packets.count=0;return 0;
}
static int ac4_cloud_planes_after(ps2_ctx *ctx,void *user){
    (void)user;if(!ac4_cloud_packets.active)return 0;ac4_cloud_packets.active=0;
    ac4_cloud_packets.batches++;
    u32 start=ac4_cloud_packets.start,end=ctx->r[2].uw[0];size_t quads=0;
    if(ac4_cloud_packets.failed || end<start || !ac4_sky_range(start,end-start))goto decline;
    if(end==start){ac4_cloud_packets.matched++;return 0;}
    if(end-start<96 || (end-start-96)%128)goto decline;
    u64 setup[12];ps2_get_mem(setup,start,sizeof setup);
    if(setup[0]!=UINT64_C(0x1000000000008005) || setup[1]!=14
       || setup[3]!=0x42 || setup[5]!=0x47 || setup[7]!=0x4e || setup[9]!=0x14 || setup[11]!=6)goto decline;
    for(u32 p=start+96;p<end;p+=128){
        u64 packet[16];u32 indices[4];ps2_get_mem(packet,p,sizeof packet);
        if(!ac4_cloud_quad_map(packet,sizeof packet,ac4_cloud_packets.records,ac4_cloud_packets.count,indices))goto decline;
        if(quads>=400)goto decline;
        for(unsigned k=0;k<4;++k){
            ac4_cloud_packets.mapped[quads*4+k].point=ac4_cloud_packets.records[indices[k]];
            ac4_cloud_packets.mapped[quads*4+k].rgba=(u32)packet[5+3*k];
        }
        quads++;
    }
    ac4_cloud_packets.matched++;ac4_cloud_packets.quads+=quads;
    if(quads)ac4_cloud_queue(start,end,ac4_cloud_packets.mapped,(u32)quads*4);
    return 0;
decline:
    if(ac4_cloud_packets.declined<3)ps2_log("ac4 cloud packet unmapped: %08x..%08x, %u point records, %u quads matched",start,end,(unsigned)ac4_cloud_packets.count,(unsigned)quads);
    ac4_cloud_packets.declined++;return 0;
}
static struct {
    u64 sprites,vertices,visible,matched,mismatched,declined;
    u32 output,st,q;int ready;
    ac4_cloud_point_input input;
    ac4_cloud_point_output expected;
} ac4_cloud_audit;
static struct {
    u64 matched,mismatched,declined;u32 address;int ready;
    ac4_cloud_sprite_input input;ac4_cloud_sprite_output expected;
} ac4_sprite_audit;
static int ac4_cloud_sprite_before(ps2_ctx *ctx,void *user){
    (void)user;ac4_cloud_audit.sprites++;ac4_sprite_audit.ready=0;
    u32 position=ctx->r[4].uw[0],output=ctx->r[5].uw[0],clip=ctx->r[6].uw[0];
    if(!ac4_sky_range(position,16) || !ac4_sky_range(output,32) || !ac4_sky_range(clip,16)){
        ac4_sprite_audit.declined++;if(ac4_sprite_records.active)ac4_sprite_records.failed=1;return 0;
    }
    ac4_cloud_sprite_input *d=&ac4_sprite_audit.input;
    ps2_get_mem(d->position,position,16);ps2_get_mem(d->clip,clip,16);
    memcpy(d->camera,ctx->vu0.vf[1].f,16);memcpy(d->scale,ctx->vu0.vf[2].f,16);
    memcpy(d->lower,ctx->vu0.vf[3].f,16);memcpy(d->upper,ctx->vu0.vf[4].f,16);
    memcpy(d->matrix,ctx->vu0.vf[5].f,64);d->size=ctx->f[12].f;d->minimum_depth=ctx->r[7].uw[0];
    ac4_sprite_audit.address=output;ps2_get_mem(ac4_sprite_audit.expected.bytes,output,32);
    ac4_sprite_audit.expected.result=0;
    ac4_sprite_audit.ready=ac4_cloud_sprite_project(d,&ac4_sprite_audit.expected);
    if(!ac4_sprite_audit.ready){ac4_sprite_audit.declined++;if(ac4_sprite_records.active)ac4_sprite_records.failed=1;}
    return 0;
}
static int ac4_cloud_sprite_after(ps2_ctx *ctx,void *user){
    (void)user;if(!ac4_sprite_audit.ready)return 0;ac4_sprite_audit.ready=0;
    ac4_cloud_sprite_output actual;ps2_get_mem(actual.bytes,ac4_sprite_audit.address,32);actual.result=ctx->f[0].f;
    if(memcmp(actual.bytes,ac4_sprite_audit.expected.bytes,32) || memcmp(&actual.result,&ac4_sprite_audit.expected.result,4)){
        if(ac4_sprite_audit.mismatched<3)ps2_log("ac4 sprite mismatch: call=%llu return=%.9g/%.9g",
            (unsigned long long)ac4_cloud_audit.sprites,ac4_sprite_audit.expected.result,actual.result);
        ac4_sprite_audit.mismatched++;
        if(ac4_sprite_records.active)ac4_sprite_records.failed=1;
    }else {
        ac4_sprite_audit.matched++;
        if(ac4_sprite_records.active && actual.result>0){
            if(ac4_sprite_records.count>=4096)ac4_sprite_records.failed=1;
            else {ac4_cloud_sprite_record *r=ac4_sprite_records.records+ac4_sprite_records.count++;
                r->input=ac4_sprite_audit.input;r->output=actual;}
        }
    }
    return 0;
}
static int ac4_cloud_vertex_before(ps2_ctx *ctx,void *user){
    (void)user;ac4_cloud_audit.vertices++;
    ac4_cloud_audit.output=ctx->r[4].uw[0];
    ac4_cloud_audit.st=ctx->r[5].uw[0];ac4_cloud_audit.q=ctx->r[6].uw[0];
    ac4_cloud_audit.ready=0;
    if(!ac4_sky_range(ctx->r[4].uw[0],8) || !ac4_sky_range(ctx->r[7].uw[0],16)
       || (ac4_cloud_audit.st && (!ac4_sky_range(ac4_cloud_audit.st,8) || !ac4_sky_range(ctx->r[8].uw[0],8)))
       || (ac4_cloud_audit.q && !ac4_sky_range(ac4_cloud_audit.q,4))){
        ac4_cloud_audit.declined++;if(ac4_cloud_packets.active)ac4_cloud_packets.failed=1;return 0;
    }
    ac4_cloud_point_input *d=&ac4_cloud_audit.input;memset(d,0,sizeof *d);
    memcpy(d->matrix,ctx->vu0.vf[4].f,sizeof d->matrix);memcpy(d->bounds,ctx->vu0.vf[3].f,sizeof d->bounds);
    ps2_get_mem(d->position,ctx->r[7].uw[0],sizeof d->position);
    d->scale=ctx->vu0.vf[1].f[0];d->depth=ctx->r[9].uw[0];d->write_st=ac4_cloud_audit.st!=0;d->write_q=ac4_cloud_audit.q!=0;
    ac4_cloud_point_output *o=&ac4_cloud_audit.expected;memset(o,0,sizeof *o);
    ps2_get_mem(&o->xyz,ac4_cloud_audit.output,8);
    if(d->write_st){ps2_get_mem(d->uv,ctx->r[8].uw[0],8);ps2_get_mem(o->st,ac4_cloud_audit.st,8);}
    if(d->write_q)ps2_get_mem(&o->q,ac4_cloud_audit.q,4);
    ac4_cloud_audit.ready=ac4_cloud_point_project(d,o);
    if(!ac4_cloud_audit.ready){ac4_cloud_audit.declined++;if(ac4_cloud_packets.active)ac4_cloud_packets.failed=1;}
    return 0;
}
static int ac4_cloud_vertex_after(ps2_ctx *ctx,void *user){
    (void)user;
    if(ac4_cloud_audit.ready && ctx->r[2].uw[0]!=UINT32_MAX)ac4_cloud_audit.visible++;
    if(ac4_cloud_audit.ready){
        ac4_cloud_point_output actual=ac4_cloud_audit.expected;
        actual.clip=ctx->r[2].uw[0];ps2_get_mem(&actual.xyz,ac4_cloud_audit.output,8);
        if(ac4_cloud_audit.st)ps2_get_mem(actual.st,ac4_cloud_audit.st,8);
        if(ac4_cloud_audit.q)ps2_get_mem(&actual.q,ac4_cloud_audit.q,4);
        const ac4_cloud_point_output *e=&ac4_cloud_audit.expected;
        if(actual.clip!=e->clip || actual.xyz!=e->xyz || memcmp(actual.st,e->st,8) || memcmp(&actual.q,&e->q,4)){
            if(ac4_cloud_audit.mismatched<3)ps2_log("ac4 cloud mismatch: call=%llu clip=%08x/%08x xyz=%016llx/%016llx",
                (unsigned long long)ac4_cloud_audit.vertices,e->clip,actual.clip,(unsigned long long)e->xyz,(unsigned long long)actual.xyz);
            ac4_cloud_audit.mismatched++;if(ac4_cloud_packets.active)ac4_cloud_packets.failed=1;
        }else {
            ac4_cloud_audit.matched++;
            if(ac4_cloud_packets.active && actual.clip!=UINT32_MAX){
                if(ac4_cloud_packets.count>=1600)ac4_cloud_packets.failed=1;
                else {
                    ac4_cloud_record *r=ac4_cloud_packets.records+ac4_cloud_packets.count++;
                    r->input=ac4_cloud_audit.input;r->output=actual;
                }
            }
        }
        if(ac4_cloud_packets.active && actual.clip!=e->clip)ac4_cloud_packets.failed=1;
    }
    ac4_cloud_audit.ready=0;return 0;
}
static void ac4_cloud_audit_init(void){
    const char *e=getenv("PS2_AC4_CLOUD_AUDIT"),*mesh=getenv("PS2_AC4_CLOUD_MESH");
    const char *sprite=getenv("PS2_AC4_SPRITE_MESH");
    const char *native=getenv("PS2_AC4_ROW_NATIVE");
    ac4_row_native_on=native && !strcmp(native,"1");
    if((!e || strcmp(e,"1")) && (!mesh || strcmp(mesh,"1")) && (!sprite || strcmp(sprite,"1")) && !ac4_row_native_on)return;
    if(ps2_r32(0x00114ed8)!=0xd8890000 || ps2_r32(0x00114edc)!=0x4bc14b2c
       || ps2_r32(0x00116060)!=0xd8e80000 || ps2_r32(0x00116064)!=0x4be821bc
       || ps2_r32(0x00116320)!=0x27bdfec0 || ps2_r32(0x00116324)!=0xffbe0100
       || ps2_r32(0x001128d8)!=0x00a0782d || ps2_r32(0x001128dc)!=0x00c0702d
       || ps2_r32(0x001152d8)!=0x27bdff10 || ps2_r32(0x001152dc)!=0xffb40050){
        ps2_log("ac4 cloud audit: code signature mismatch; disabled");return;
    }
    int h[11],count=10;
    h[0]=ps2_hook_before(0x00114ed8,ac4_cloud_sprite_before,NULL,100,"ac4-cloud-audit");
    h[1]=ps2_hook_before(0x00116060,ac4_cloud_vertex_before,NULL,100,"ac4-cloud-audit");
    h[2]=ps2_hook_after(0x00116060,ac4_cloud_vertex_after,NULL,100,"ac4-cloud-audit");
    h[3]=ps2_hook_before(0x00116320,ac4_cloud_planes_before,NULL,100,"ac4-cloud-audit");
    h[4]=ps2_hook_after(0x00116320,ac4_cloud_planes_after,NULL,100,"ac4-cloud-audit");
    h[5]=ps2_hook_after(0x00114ed8,ac4_cloud_sprite_after,NULL,100,"ac4-cloud-audit");
    h[6]=ps2_hook_before(0x001128d8,ac4_cloud_row_before,NULL,100,"ac4-cloud-audit");
    h[7]=ps2_hook_after(0x001128d8,ac4_cloud_row_after,NULL,100,"ac4-cloud-audit");
    h[8]=ps2_hook_before(0x001152d8,ac4_cloud_sprites_begin,NULL,100,"ac4-cloud-audit");
    h[9]=ps2_hook_after(0x001152d8,ac4_cloud_sprites_end,NULL,100,"ac4-cloud-audit");
    if(ac4_row_native_on)h[count++]=ps2_hook_replace(0x001128d8,ac4_cloud_row_replace,NULL,100,"ac4-native-row");
    for(int i=0;i<count;++i)if(h[i]<0){
        for(int j=0;j<count;++j)if(h[j]>=0)ps2_hook_remove(h[j]);
        ps2_log("ac4 cloud audit: hook registration failed; disabled");return;
    }
    ps2_log("ac4 cloud audit: observing sprite projection and plane vertices; GPU replacement separately gated by PS2_AC4_CLOUD_MESH");
}
static void ac4_cloud_audit_report(void){
    if(ac4_row_native_on)
        ps2_log("ac4 native rows: %llu replaced, %llu declined",
            (unsigned long long)ac4_row_audit.native_rows,(unsigned long long)ac4_row_audit.native_declined);
    if(ac4_row_audit.calls)
        ps2_log("ac4 cloud row continuation: %llu matched, %llu mismatched",
            (unsigned long long)ac4_row_audit.context_matched,(unsigned long long)ac4_row_audit.context_mismatched);
    if(ac4_row_audit.calls)
        ps2_log("ac4 cloud sprite mapping: %llu mapped, %llu unmapped rows",
            (unsigned long long)ac4_sprite_records.mapped,(unsigned long long)ac4_sprite_records.unmapped);
    if(ac4_row_audit.calls)
        ps2_log("ac4 cloud sprite rows: %llu calls, %llu matched, %llu mismatched, %llu declined, %llu pieces",
            (unsigned long long)ac4_row_audit.calls,(unsigned long long)ac4_row_audit.matched,
            (unsigned long long)ac4_row_audit.mismatched,(unsigned long long)ac4_row_audit.declined,
            (unsigned long long)ac4_row_audit.pieces);
    if(ac4_cloud_audit.sprites)
        ps2_log("ac4 cloud sprite plan: %llu matched, %llu mismatched, %llu declined",
            (unsigned long long)ac4_sprite_audit.matched,(unsigned long long)ac4_sprite_audit.mismatched,
            (unsigned long long)ac4_sprite_audit.declined);
    if(ac4_cloud_packets.batches)
        ps2_log("ac4 cloud packets: %llu batches, %llu mapped, %llu declined, %llu quads",
            (unsigned long long)ac4_cloud_packets.batches,(unsigned long long)ac4_cloud_packets.matched,
            (unsigned long long)ac4_cloud_packets.declined,(unsigned long long)ac4_cloud_packets.quads);
    if(ac4_cloud_audit.vertices)
        ps2_log("ac4 cloud point plan: %llu matched, %llu mismatched, %llu declined",
            (unsigned long long)ac4_cloud_audit.matched,(unsigned long long)ac4_cloud_audit.mismatched,
            (unsigned long long)ac4_cloud_audit.declined);
    if(ac4_cloud_audit.sprites || ac4_cloud_audit.vertices)
        ps2_log("ac4 cloud audit: %llu sprite projections, %llu plane vertices, %llu non-rejected plane vertices",
            (unsigned long long)ac4_cloud_audit.sprites,(unsigned long long)ac4_cloud_audit.vertices,
            (unsigned long long)ac4_cloud_audit.visible);
}
