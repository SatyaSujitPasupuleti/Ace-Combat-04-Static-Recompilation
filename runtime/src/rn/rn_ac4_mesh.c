#include "rn_int.h"
#include "rn_ac4_mesh.h"
#include <math.h>
#include <stdio.h>

/* Up to two verified material passes, scoped to one XGKICK. */
typedef struct {
    int active,secondary,terrain,clipped,cockpit;
    ps2_vf constants[32],records[3][4];
    rn_ac4_model_vertex expected[3];
} model_candidate;
static model_candidate pending,second_pass,terrain_queue[16];
static u32 terrain_next,terrain_count;
static u64 terrain_draws[2],terrain_clipped_draws[2];
static u64 cockpit_draws,cockpit_secondary_draws,cockpit_clipped_draws;
static struct { u64 prepared,draws,attributes,state,capacity,unused,secondary_draws; } mesh_stats;
static struct { u64 converted,failed,unsupported,atlases,supplied,filtered,anisotropic; } texture_stats;
static int atlas_texture_on=-1,supplied_mips_on=-1,supplied_filter=-1;

static void atlas_dump(u32 index,u32 w,u32 h) {
    static int done;
    const char *path=getenv("PS2_AC4_ATLAS_DUMP");
    if(done || !path || !*path)return;
    done=1;
    const u8 *pixels=ps2_vk_texture_pixels(index);
    FILE *f=pixels?fopen(path,"wb"):NULL;
    if(!f)return;
    fprintf(f,"P6\n%u %u\n255\n",w,h);
    for(size_t i=0;i<(size_t)w*h;i++)fwrite(pixels+i*4u,1,3,f);
    fclose(f);
    ps2_log("ac4 native atlas diagnostic: source %u saved to %s",index,path);
}

/* Reuse AC5's native mip-chain allocator, but start from the already decoded
 * AC4 texture. Do not replace the source image or refill mutable GS state. */
static int model_texture(const ps2_vk_state *st,ps2_vk_state *out) {
    if(supplied_mips_on<0) {
        const char *e=getenv("PS2_AC4_SUPPLIED_MIPS");
        supplied_mips_on=e && *e && *e!='0';
    }
    if(supplied_filter<0) {
        const char *e=getenv("PS2_AC4_SUPPLIED_FILTER");
        supplied_filter=e && e[0]=='2' && !e[1]?2:e && e[0]=='1' && !e[1]?1:0;
    }
    ps2_vk_state material=*st;
    /* Packed GS mip atlases are not native sampler images. Validate the
     * remaining material state before extracting their base level below. */
    material.tex_mip=0;
    if(!st->tme || st->tex_rt || st->tex_lod || st->tex_index==~(u32)0
       || !ps2_vk_mesh_material_ok(&material)) {texture_stats.unsupported++;return 0;}
    u32 iw,ih,x=0,y=0,w,h;
    ps2_vk_texture_size(st->tex_index,&iw,&ih);w=iw;h=ih;
    if(st->tex_mip) {
        if(atlas_texture_on<0) {
            const char *e=getenv("PS2_AC4_ATLAS_TEXTURE");
            atlas_texture_on=e && *e && *e!='0';
        }
        if(!atlas_texture_on && !supplied_mips_on) {texture_stats.unsupported++;return 0;}
        /* current_mip_texture stores level 0 at (0,0) in a 2*TW by TH+8
         * atlas. Never generate mips from the packed lower levels/padding. */
        u32 mxl=st->tex_mip&7u, mode=(st->tex_mip>>3)&7u;
        if(!(st->tex_mip&(1u<<20)) || !mxl || mxl>6 || mode<2 || mode>5
           || !isfinite(st->tex_w) || !isfinite(st->tex_h)
           || st->tex_w<1 || st->tex_h<1 || st->tex_w>16384 || st->tex_h>16384
           || floorf(st->tex_w)!=st->tex_w || floorf(st->tex_h)!=st->tex_h
           || iw!=(u32)st->tex_w*2u || ih!=(u32)st->tex_h+8u) {
            texture_stats.unsupported++;return 0;
        }
        atlas_dump(st->tex_index,iw,ih);
        iw=w=(u32)st->tex_w;ih=h=(u32)st->tex_h;
        static unsigned samples;
        if(samples<4u) {
            samples++;
            ps2_log("ac4 native atlas sample: point=%d mip=%08X base=%ux%u clamp=%u,%u:%u..%u,%u..%u",
                    st->tex_point,st->tex_mip,iw,ih,st->wms,st->wmt,
                    st->minu,st->maxu,st->minv,st->maxv);
        }
    }
    if(st->wms==2) {
        if(st->minu>st->maxu) {texture_stats.unsupported++;return 0;}
        x=st->minu;w=st->maxu-st->minu+1;
    }
    if(st->wmt==2) {
        if(st->minv>st->maxv) {texture_stats.unsupported++;return 0;}
        y=st->minv;h=st->maxv-st->minv+1;
    }
    if(x>=iw || y>=ih) {texture_stats.unsupported++;return 0;}
    /* Cropping an out-of-image clamp changes the original wrap semantics. */
    if(w>iw-x || h>ih-y) {texture_stats.unsupported++;return 0;}
    u32 clamp=(st->wms==1 || st->wms==2 ? 1u : 0u)
             |(st->wmt==1 || st->wmt==2 ? 2u : 0u);
    int supplied=st->tex_mip && supplied_mips_on;
    u32 index=supplied
        ? ps2_vk_texture_native_atlas(st->tex_index,iw,ih,x,y,w,h,st->tex_mip&7u,clamp)
        : ps2_vk_texture_native_wrap(st->tex_index,x,y,w,h,clamp);
    if(index==~(u32)0) {texture_stats.failed++;return 0;}
    *out=material;out->tex_index=index;
    out->native_region=(st->wms==2 || st->wmt==2) | (supplied?2:0);
    if(supplied) {out->tex_mip=st->tex_mip;texture_stats.supplied++;}
    if(supplied && supplied_filter && ((st->tex_mip>>3)&7u)==5u && (st->tex_mip&(1u<<21))) {
        out->native_region|=4;
        texture_stats.filtered++;
        if(supplied_filter==2) {out->native_region|=8;texture_stats.anisotropic++;}
    }
    texture_stats.converted++;texture_stats.atlases+=st->tex_mip!=0;return 1;
}
int rn_ac4_mesh_pending(void) { return pending.active || terrain_next<terrain_count; }
int rn_ac4_mesh_cockpit_pending(void) {
    return pending.active ? pending.cockpit : (terrain_next<terrain_count && terrain_queue[terrain_next].cockpit);
}

void rn_ac4_mesh_end(void) {
    mesh_stats.unused+=(u64)pending.active+(u64)second_pass.active;
    pending.active=second_pass.active=0;terrain_next=terrain_count=0;
}
void rn_ac4_mesh_prepare(const ps2_vf c[32],const ps2_vf r[3][4],
                         const rn_ac4_model_vertex expected[3],u32 prim) {
    rn_ac4_mesh_end();
    if(!rn_ac4_model_mesh_on || !c || !r || !expected) return;
    /* Smooth shaded, perspective STQ triangle/list/strip/fan only. */
    if(!(prim&8u) || (prim&256u) || (prim&7u)<3 || (prim&7u)>5) return;
    memcpy(pending.constants,c,sizeof pending.constants);
    memcpy(pending.records,r,sizeof pending.records);
    memcpy(pending.expected,expected,sizeof pending.expected);
    pending.active=1;pending.secondary=0;pending.terrain=0;pending.cockpit=0;pending.clipped=0;mesh_stats.prepared++;
}
void rn_ac4_mesh_prepare_cockpit(const ps2_vf c[32],const ps2_vf r[3][4],
                               const rn_ac4_model_vertex expected[3],u32 prim) {
    rn_ac4_mesh_prepare(c,r,expected,prim);
    if(pending.active)pending.cockpit=1;
}
void rn_ac4_mesh_prepare_secondary(const ps2_vf c[32],const ps2_vf r[3][4],
                                   const rn_ac4_model_vertex expected[3],u32 prim) {
    if(!rn_ac4_model_mesh_on || !pending.active || !(prim&8u) || (prim&256u) || (prim&7u)<3 || (prim&7u)>5)return;
    memcpy(second_pass.constants,c,sizeof second_pass.constants);
    memcpy(second_pass.records,r,sizeof second_pass.records);
    memcpy(second_pass.expected,expected,sizeof second_pass.expected);
    second_pass.active=second_pass.secondary=1;second_pass.terrain=0;second_pass.cockpit=pending.cockpit;second_pass.clipped=0;mesh_stats.prepared++;
}
void rn_ac4_mesh_terrain_append(const ps2_vf c[32],const ps2_vf r[3][4],
                                const rn_ac4_model_vertex expected[3],int secondary,int clipped) {
    if(!rn_ac4_model_mesh_on || !c || !r || !expected || terrain_count==16)return;
    model_candidate *p=&terrain_queue[terrain_count++];
    memcpy(p->constants,c,sizeof p->constants);memcpy(p->records,r,sizeof p->records);
    memcpy(p->expected,expected,sizeof p->expected);
    p->active=p->terrain=1;p->secondary=secondary!=0;p->clipped=clipped!=0;p->cockpit=0;
}
void rn_ac4_mesh_cockpit_append(const ps2_vf c[32],const ps2_vf r[3][4],
                              const rn_ac4_model_vertex expected[3],int secondary) {
    if(!rn_ac4_model_mesh_on || !c || !r || !expected || terrain_count==16)return;
    model_candidate *p=&terrain_queue[terrain_count++];
    memcpy(p->constants,c,sizeof p->constants);memcpy(p->records,r,sizeof p->records);
    memcpy(p->expected,expected,sizeof p->expected);
    p->active=p->cockpit=p->clipped=1;p->terrain=0;p->secondary=secondary!=0;mesh_stats.prepared++;
}
int rn_ac4_mesh_claim(const ps2_vk_state *st,int kind,const ps2_vk_vertex *v,int n) {
    if(!rn_ac4_mesh_pending()) return 0;
    model_candidate selected;
    if(pending.active) {selected=pending;pending=second_pass;second_pass.active=0;}
    else selected=terrain_queue[terrain_next++];
    if(!st || !v || kind!=RN_PRIM_TRI || n!=3 || st->fst) {mesh_stats.state++;return 0;}
    ps2_vk_mesh_block b;
    rn_mesh_block_init(&b);
    u32 zpsm=(u32)((ps2_gs_cur_zbuf()>>24)&15u);
    double zm=zpsm==1?16777215.0:zpsm>=2?65535.0:4294967295.0;
    for(int i=0;i<3;i++) {
        const rn_ac4_model_vertex *p=&selected.expected[i];
        float x=(float)((u32)p->xyzf[0]&65535u)/16.0f-b.xoff+0.5f;
        float y=(float)((u32)p->xyzf[1]&65535u)/16.0f-b.yoff+0.5f;
        float z=(float)fmin((double)(((u32)p->xyzf[2]>>4)&0xffffffu)/zm,1.0);
        float fog=(float)(((u32)p->xyzf[3]>>4)&255u)/255.0f;
        if(v[i].x!=x || v[i].y!=y || v[i].z!=z || v[i].fog!=fog || v[i].round_uv
           || v[i].s!=p->stq[0] || v[i].t!=p->stq[1] || v[i].q!=p->stq[2]
           || v[i].r!=(float)(p->rgba[0]&255) || v[i].g!=(float)(p->rgba[1]&255)
           || v[i].b!=(float)(p->rgba[2]&255) || v[i].a!=(float)(p->rgba[3]&255)) {
            mesh_stats.attributes++;return rn_ac4_mesh_pending()?rn_ac4_mesh_claim(st,kind,v,n):0;
        }
    }
    /* Flush earlier compatibility mesh batches before reserving arena space. */
    rn_screen_flush();
    if(!ps2_vk_mesh_room(3,3,1)) {mesh_stats.capacity++;return 0;}
    u8 records[3*64];u32 idx[3]={0,1,2},vb,ib;
    for(int i=0;i<3;i++) {
        if(selected.terrain) {
            s32 color[4];for(int k=0;k<4;k++)color[k]=ps2_cvt_w_s(selected.records[i][1].f[k]);
            rn_mesh_rec(records+i*64,selected.records[i][2].f,selected.records[i][1].s,color,
                        selected.records[i][selected.secondary?3:0].f);
            continue;
        }
        float uv[4];for(int k=0;k<4;k++) uv[k]=(float)selected.records[i][0].s[k]*(1.0f/4096.0f);
        rn_mesh_rec(records+i*64,selected.records[i][2].f,selected.records[i][3].s,selected.records[i][1].s,uv);
    }
    if(ps2_vk_mesh_store(records,3,idx,3,&vb,&ib)!=0) {mesh_stats.capacity++;return 0;}
    memcpy(b.m,selected.constants,64);
    memcpy(b.c,selected.constants+8,64);
    memcpy(b.l,selected.constants+12,48);
    memcpy(b.p,selected.constants+15,16); /* Fourth directional-light column. */
    memcpy(b.r,selected.constants+16,64);
    memcpy(b.e,selected.constants+20,16);
    b.fog0=selected.constants[30].f[0];b.fog1=selected.constants[30].f[1];
    b.mode=selected.secondary?2:(selected.constants[23].u[1]&2u)!=0;
    b.prog=selected.terrain?7:6;
    if(selected.terrain)b.mode=selected.secondary;
    if(selected.cockpit)b.mode|=4u;
    ps2_vk_state native;
    const ps2_vk_state *draw=st;
    if(rn_ac4_model_texture_on && rn_ac4_model_material_on && model_texture(st,&native))draw=&native;
    ps2_vk_draw_mesh(draw,&b,vb,ib,3,rn_ac4_model_material_on?0:PS2_VK_MESH_GSFRAG);
    if(selected.terrain) {terrain_draws[selected.secondary]++;if(selected.clipped)terrain_clipped_draws[selected.secondary]++;}
    else {mesh_stats.draws++;mesh_stats.secondary_draws+=selected.secondary;}
    cockpit_draws+=selected.cockpit!=0;
    cockpit_secondary_draws+=selected.cockpit && selected.secondary;
    cockpit_clipped_draws+=selected.cockpit && selected.clipped;
    return 1;
}
void rn_ac4_mesh_report(void) {
    if(cockpit_draws)ps2_log("ac4 cockpit GPU: %llu draws",(unsigned long long)cockpit_draws);
    if(cockpit_secondary_draws)ps2_log("ac4 cockpit secondary GPU: %llu draws",(unsigned long long)cockpit_secondary_draws);
    if(cockpit_clipped_draws)ps2_log("ac4 cockpit clipped GPU: %llu draws",(unsigned long long)cockpit_clipped_draws);
    ps2_log("ac4 clipped terrain GPU: %llu primary, %llu secondary draws",(unsigned long long)terrain_clipped_draws[0],(unsigned long long)terrain_clipped_draws[1]);
    if(terrain_draws[0] || terrain_draws[1])ps2_log("ac4 terrain GPU: %llu primary, %llu secondary draws",(unsigned long long)terrain_draws[0],(unsigned long long)terrain_draws[1]);
    if(rn_ac4_model_texture_on)
        ps2_log("ac4 model textures: %llu native mip draws, %llu allocation fallbacks, %llu unsupported states",
            (unsigned long long)texture_stats.converted,(unsigned long long)texture_stats.failed,
            (unsigned long long)texture_stats.unsupported);
    if(texture_stats.filtered)ps2_log("ac4 supplied filtering: %llu hardware, %llu anisotropic draws",(unsigned long long)texture_stats.filtered,(unsigned long long)texture_stats.anisotropic);
    if(texture_stats.supplied)ps2_log("ac4 supplied mip chains: %llu draws",(unsigned long long)texture_stats.supplied);
    if(texture_stats.atlases)
        ps2_log("ac4 native texture atlases: %llu base levels extracted",
                (unsigned long long)texture_stats.atlases);
    if(!mesh_stats.prepared)return;
    ps2_log("ac4 model second GPU: %llu draws",(unsigned long long)mesh_stats.secondary_draws);
    ps2_log("ac4 model GPU: %llu candidates, %llu model-space draws, %llu attribute / %llu state / %llu capacity fallbacks, %llu unused",
        (unsigned long long)mesh_stats.prepared,(unsigned long long)mesh_stats.draws,
        (unsigned long long)mesh_stats.attributes,(unsigned long long)mesh_stats.state,
        (unsigned long long)mesh_stats.capacity,(unsigned long long)mesh_stats.unused);
}
