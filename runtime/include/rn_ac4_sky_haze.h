#ifndef RN_AC4_SKY_HAZE_H
#define RN_AC4_SKY_HAZE_H
#include "rn_ac4_sky_dome.h"
#define AC4_SKY_HAZE_QWORDS (5u+6u*8u*(2u+AC4_SKY_BAND_QWORDS))
#define AC4_SKY_HAZE_VERTICES (6u*8u*32u*4u)
typedef struct {
    ac4_sky_dome_input sky;
    float near_distance,far_distance,depth_offset,depth_scale;
} ac4_sky_haze_input;

/* Original AC4 haze packet proposal. Not enabled for guest bypass or drawing;
 * requires original-producer oracle/live acceptance before integration. */
static inline int ac4_sky_haze_plan(const ac4_sky_haze_input *h,
    ac4_sky_qword *out,size_t capacity,size_t *count,
    ac4_sky_mesh_vertex *vertices,size_t vertex_capacity,size_t *vertex_count) {
    if(!h || !out || !count || !isfinite(h->near_distance) || !isfinite(h->far_distance)
       || !isfinite(h->depth_offset) || !isfinite(h->depth_scale) || (vertices && !vertex_count))return 0;
    ac4_sky_mesh_vertex mesh[AC4_SKY_HAZE_VERTICES];size_t nv=0;
    const ac4_sky_dome_input *d=&h->sky;
    ac4_sky_qword packet[AC4_SKY_HAZE_QWORDS];
    ac4_sky_point rows[2][33],previous[33]={{0}};
    memcpy(rows,d->initial_rows,sizeof rows);
    float step=ps2_fsub(h->far_distance,h->near_distance);
    if(step>4096.0f)step=4096.0f;
    step=ps2_fmul(step,0.125f);
    float first=ps2_fsub(h->far_distance,4096.0f);
    uint64_t previous_mask=0;size_t used=5;
    packet[0]=(ac4_sky_qword){UINT64_C(0x1000000000008004),14};
    packet[1]=(ac4_sky_qword){UINT64_C(0x1310000a8),0x4e};
    packet[2]=(ac4_sky_qword){UINT64_C(0x70000),0x47};
    packet[3]=(ac4_sky_qword){UINT64_C(0x44),0x42};
    packet[4]=(ac4_sky_qword){UINT64_C(0xff00000000080000),0x4c};
    for(unsigned r=14;r<21;++r){
        ac4_sky_point *current=rows[r&1u];float matrix[16];uint64_t mask=0;
        if(!ac4_sky_ring_matrix(d->matrix,d->rings[r][0],d->rings[r][1],matrix))return 0;
        for(unsigned k=0;k<32;++k){
            if(!ac4_sky_project_point(matrix,d->segments[k],d->bounds,current+k))return 0;
            if(!(current[k].clip&0x80000000u))mask|=UINT64_C(1)<<k;
        }
        if(mask&UINT64_C(0x80000000))mask|=UINT64_C(0xffffffff00000000);
        current[32]=current[0];mask|=(mask&1)<<32;
        if(previous_mask&mask){
            float distance=first;
            for(unsigned layer=0;layer<8;++layer){
                float z=ps2_fadd(ps2_fdiv(h->depth_scale,distance),h->depth_offset);
                uint64_t depth=(uint64_t)(uint32_t)ps2_cvt_w_s(z)<<32;
                packet[used++]=(ac4_sky_qword){UINT64_C(0x1026400000008001),14};
                packet[used++]=(ac4_sky_qword){((uint64_t)(16+8*layer)<<32)|0x64,0x42};
                size_t n;
                if(!ac4_sky_band_build(previous,current,previous_mask&mask,d->colors[r-1],d->colors[r],
                    depth,depth,packet+used,AC4_SKY_HAZE_QWORDS-used,&n))return 0;
                used+=n;distance=ps2_fadd(distance,step);
                if(vertices)for(unsigned k=0;k<32;++k){
                    if((((previous_mask&mask)>>k)&3)!=3
                       || (previous[k].clip&previous[k+1].clip&current[k].clip&current[k+1].clip))continue;
                    for(unsigned v=0;v<4;++v){
                        unsigned ring=v<2?r-1:r,segment=k+(v&1);
                        const ac4_sky_point *p=v<2?previous+segment:current+segment;
                        mesh[nv++]=(ac4_sky_mesh_vertex){ring,segment%32,p->xy,(uint32_t)(depth>>32),d->colors[ring],p->clip==UINT32_MAX};
                    }
                }
            }
        }
        memcpy(previous,current,sizeof previous);previous_mask=mask;
    }
    if(capacity<used || (vertices && vertex_capacity<nv))return 0;
    memcpy(out,packet,used*sizeof *out);
    if(vertices){memcpy(vertices,mesh,nv*sizeof *mesh);*vertex_count=nv;}
    *count=used;return 1;
}
static inline int ac4_sky_haze_build(const ac4_sky_haze_input *h,
    ac4_sky_qword *out,size_t capacity,size_t *count) {
    return ac4_sky_haze_plan(h,out,capacity,count,NULL,0,NULL);
}
#endif
