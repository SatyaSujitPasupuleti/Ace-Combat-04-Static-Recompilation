#ifndef RN_AC4_CLOUD_SPRITE_PACKET_H
#define RN_AC4_CLOUD_SPRITE_PACKET_H
#include "rn_ac4_cloud_sprite.h"
typedef struct {
    ac4_cloud_sprite_input input;
    ac4_cloud_sprite_output output;
} ac4_cloud_sprite_record;
typedef struct {ac4_cloud_sprite_record sprite;uint32_t rgba;} ac4_cloud_sprite_mesh;
/* Sorting can move a projected 32-byte entry. Associate its exact emitted
 * geometry/UV values, not its former memory address. Ambiguous world inputs
 * deliberately remain on the compatibility path. */
static inline int ac4_cloud_sprite_find(const ac4_cloud_sprite_record *records,size_t count,
                                        uint64_t xy0,uint64_t xyz1,uint64_t uv0,uint64_t uv1,
                                        size_t *index){
    if(!records || !index)return 0;
    size_t found=SIZE_MAX;
    for(size_t i=0;i<count;++i){
        const ac4_cloud_sprite_record *r=records+i;uint32_t x,u,v;uint64_t z;
        if(!(r->output.result>0))continue;
        memcpy(&x,r->output.bytes+16,4);memcpy(&z,r->output.bytes+24,8);
        memcpy(&u,r->output.bytes+8,4);memcpy(&v,r->output.bytes+12,4);
        if(xy0!=x || xyz1!=z || uv0!=u || uv1!=v)continue;
        if(found!=SIZE_MAX && memcmp(&records[found].input,&r->input,sizeof r->input))return 0;
        found=i;
    }
    if(found==SIZE_MAX)return 0;
    *index=found;return 1;
}
#endif
