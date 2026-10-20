#ifndef PS2_VK_MESH_BLOCKS_H
#define PS2_VK_MESH_BLOCKS_H
#include "ps2_vk.h"
#include <string.h>

/* Packed atlas metadata accepted by the AC4 mesh fragment shader.
 * Preserve the supplied levels and GS LOD until native texture conversion is
 * independently accepted. Unknown bits and unsupported mip modes fall back. */
static inline int ps2_vk_mesh_mip_supported(u32 mip,int ac4) {
    if(!mip)return 1;
    u32 levels=mip&7u,mode=(mip>>3)&7u;
    return ac4 && (mip&(1u<<20)) && !(mip&~0x3fffffu)
        && levels>=1u && levels<=6u && mode>=2u && mode<=5u;
}

/* Arena-local immutable constants. Never retain an index across arena resets.
 * Only AC4 model program 6 opts in; other mesh programs keep their old policy. */
static inline u32 ps2_vk_mesh_block_store(ps2_vk_mesh_block *blocks,u32 *used,
                                         u32 capacity,const ps2_vk_mesh_block *b,
                                         int ac4,int *reused) {
    *reused=0;
    if(ac4 && b->prog==6 && *used && *used<=capacity
       && !memcmp(blocks+*used-1,b,sizeof *b)) {
        *reused=1;
        return *used-1;
    }
    if(*used>=capacity) return ~(u32)0;
    blocks[*used]=*b;
    return (*used)++;
}
#endif
