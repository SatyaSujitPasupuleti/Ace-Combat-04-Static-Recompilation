#ifndef RN_AC4_SKY_DOME_H
#define RN_AC4_SKY_DOME_H
#include "rn_ac4_sky_project.h"
#define AC4_SKY_RINGS 29u
#define AC4_SKY_DOME_QWORDS (3u + 28u * (2u + AC4_SKY_BAND_QWORDS))
#define AC4_SKY_DOME_VERTICES (28u*32u*4u)
typedef struct { uint32_t ring, segment, xy, depth, color, fallback; } ac4_sky_mesh_vertex;
typedef struct {
    float matrix[16], bounds[4], segments[32][4], rings[AC4_SKY_RINGS][2];
    uint32_t colors[AC4_SKY_RINGS], depth;
    ac4_sky_point initial_rows[2][33];
} ac4_sky_dome_input;

static inline int ac4_sky_dome_read(const void *object, size_t bytes, uint32_t depth,
                                   ac4_sky_dome_input *out) {
    if (!object || !out || bytes<0x43cu) return 0;
    const unsigned char *p=object;ac4_sky_dome_input d={0};
    memcpy(d.matrix,p+0x40,sizeof d.matrix);memcpy(d.bounds,p+0xc0,sizeof d.bounds);
    memcpy(d.segments,p+0xd0,sizeof d.segments);memcpy(d.rings,p+0x2e0,sizeof d.rings);
    memcpy(d.colors,p+0x3c8,sizeof d.colors);d.depth=depth;*out=d;return 1;
}

/* Complete original dome packet geometry/state, but no EE continuation or
 * scratch writes. Preserve the unused high half of each PRIM REGLIST payload.
 * Invalid inputs/capacity leave output and count unchanged. */
static inline int ac4_sky_dome_plan(const ac4_sky_dome_input *d,
    ac4_sky_qword *out,size_t capacity,size_t *count,
    ac4_sky_mesh_vertex *vertices,size_t vertex_capacity,size_t *vertex_count) {
    if (!d || !out || !count || (vertices && !vertex_count)) return 0;
    ac4_sky_mesh_vertex mesh[AC4_SKY_DOME_VERTICES];size_t nv=0;
    ac4_sky_qword packet[AC4_SKY_DOME_QWORDS];
    unsigned char padding[AC4_SKY_DOME_QWORDS]={0};
    ac4_sky_point previous[33]={{0}},rows[2][33];
    memcpy(rows,d->initial_rows,sizeof rows);
    uint64_t prev_mask=0,prev_depth=0;
    size_t used=3;
    packet[0]=(ac4_sky_qword){UINT64_C(0x1000000000000002),14};
    packet[1]=(ac4_sky_qword){UINT64_C(0x310000a8),0x4e};
    packet[2]=(ac4_sky_qword){UINT64_C(0x30000),0x47};
    for (unsigned r=0;r<AC4_SKY_RINGS;++r) {
        ac4_sky_point *current=rows[r&1u];
        float m[16];uint64_t mask=0;
        if (!ac4_sky_ring_matrix(d->matrix,d->rings[r][0],d->rings[r][1],m)) return 0;
        unsigned points=(r==0 || r==28)?1:32;
        for (unsigned k=0;k<points;++k) {
            if (!ac4_sky_project_point(m,d->segments[k],d->bounds,&current[k])) return 0;
            if (!(current[k].clip & 0x80000000u)) mask|=UINT64_C(1)<<k;
        }
        if (points==1) {
            if (mask) mask=UINT64_C(0xffffffff);
            for (unsigned k=1;k<32;++k) current[k]=current[0];
        }
        /* EE SLL sign-extends bit 31 before ORing the 64-bit mask. Preserve
         * that inherited high-bit behaviour before adding the closure bit. */
        if (points==32 && (mask & UINT64_C(0x80000000))) mask|=UINT64_C(0xffffffff00000000);
        current[32]=current[0];mask|=(mask&1)<<32;
        uint64_t depth=(uint64_t)(r<15?d->depth-8u:d->depth)<<32;
        if (prev_mask & mask) {
            packet[used++]=(ac4_sky_qword){UINT64_C(0x1400000000000001),0};
            padding[used]=1;packet[used++]=(ac4_sky_qword){12,0};
            size_t n;
            if (!ac4_sky_band_build(previous,current,prev_mask & mask,
                    d->colors[r-1],d->colors[r],prev_depth,depth,
                    packet+used,AC4_SKY_DOME_QWORDS-used,&n)) return 0;
            used+=n;
            if (vertices) for (unsigned k=0;k<32;++k) {
                if ((((prev_mask & mask)>>k)&3)!=3
                    || (previous[k].clip & previous[k+1].clip & current[k].clip & current[k+1].clip)) continue;
                for (unsigned v=0;v<4;++v) {
                    unsigned ring=v<2?r-1:r,segment=k+(v&1);
                    const ac4_sky_point *point=v<2?previous+segment:current+segment;
                    mesh[nv++]=(ac4_sky_mesh_vertex){ring,(ring==0 || ring==28)?0:segment%32,
                        point->xy,(uint32_t)((v<2?prev_depth:depth)>>32),d->colors[ring],point->clip==UINT32_MAX};
                }
            }
        }
        memcpy(previous,current,sizeof previous);prev_mask=mask;prev_depth=depth;
    }
    if (capacity<used || (vertices && vertex_capacity<nv)) return 0;
    for (size_t i=0;i<used;++i) {
        out[i].lo=packet[i].lo;
        if (!padding[i]) out[i].hi=packet[i].hi;
    }
    if (vertices) {memcpy(vertices,mesh,nv*sizeof mesh[0]);*vertex_count=nv;}
    *count=used;return 1;
}
static inline int ac4_sky_dome_build(const ac4_sky_dome_input *d,
    ac4_sky_qword *out,size_t capacity,size_t *count) {
    return ac4_sky_dome_plan(d,out,capacity,count,NULL,0,NULL);
}
#endif
