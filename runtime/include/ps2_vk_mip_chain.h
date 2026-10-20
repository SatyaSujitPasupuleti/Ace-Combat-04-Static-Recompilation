#ifndef PS2_VK_MIP_CHAIN_H
#define PS2_VK_MIP_CHAIN_H
#include "ps2_runtime.h"
#include <stddef.h>
#include <string.h>

/* Only aligned power-of-two crops have the same footprint at every level.
 * Reject other crops rather than shifting the supplied terrain artwork. */
typedef struct {
    u32 w[7],h[7],sx[7],sy[7],levels;
    size_t offset[7],bytes;
} ps2_vk_mip_layout;
static inline int ps2_vk_mip_layout_make(ps2_vk_mip_layout *out,
    u32 aw,u32 ah,u32 bw,u32 bh,u32 x,u32 y,u32 w,u32 h,u32 mxl) {
    if(!out || !bw || !bh || bw>16384 || bh>16384 || aw!=bw*2u || ah!=bh+8u
       || !w || !h || (w&(w-1u)) || (h&(h-1u)) || !mxl || mxl>6u
       || x>=bw || y>=bh || w>bw-x || h>bh-y)return 0;
    u32 align=1u<<mxl;
    if((x&(align-1u)) || (y&(align-1u)) || w<align || h<align)return 0;
    ps2_vk_mip_layout p={0};p.levels=mxl+1u;u32 row=0;
    for(u32 i=0;i<p.levels;i++) {
        p.w[i]=w>>i;p.h[i]=h>>i;
        p.sx[i]=(i?bw:0)+(x>>i);p.sy[i]=(i?row:0)+(y>>i);
        if(p.sx[i]>aw || p.w[i]>aw-p.sx[i] || p.sy[i]>ah || p.h[i]>ah-p.sy[i])return 0;
        p.offset[i]=p.bytes;p.bytes+=(size_t)p.w[i]*p.h[i]*4u;
        if(i)row+=(bh>>i)?(bh>>i):1u;
    }
    *out=p;return 1;
}
static inline void ps2_vk_mip_extract(u8 *dst,const u8 *src,u32 aw,
                                     const ps2_vk_mip_layout *p) {
    for(u32 i=0;i<p->levels;i++)for(u32 y=0;y<p->h[i];y++)
        memcpy(dst+p->offset[i]+(size_t)y*p->w[i]*4u,
               src+((size_t)(p->sy[i]+y)*aw+p->sx[i])*4u,(size_t)p->w[i]*4u);
}
#endif
