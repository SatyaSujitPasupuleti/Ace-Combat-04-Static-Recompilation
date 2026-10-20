#ifndef RN_AC4_SCREEN_SPRITE_H
#define RN_AC4_SCREEN_SPRITE_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
/* Packet produced by AC4 0015FEE0. Ownership/HUD identity is separate.
 * Values are GS fixed-point coordinates; the original narrows all six to u16.
 * This builds only GIF data, not allocator side effects or CPU continuation. */
typedef struct {
    uint16_t x,y,u,v,width,height;
    uint32_t display_flags,flags;
} ac4_screen_sprite;
static inline int ac4_screen_sprite_build(const ac4_screen_sprite *in,void *output,size_t capacity) {
    if(!in || !output || capacity<64)return 0;
    uint64_t x=in->x,y=in->y,u=in->u,v=in->v,sw=in->width,tw=in->width,h=in->height;
    uint32_t flags=in->flags;
    if(in->display_flags&0x10000u){sw=(sw*3)/4;flags|=0x100u;}
    if(flags&0x100u) {
        x=(x-16)&65535u;y=(y-16)&65535u;
        sw=(sw+16)&65535u;tw=(tw+16)&65535u;h=(h+16)&65535u;
        u=(u-8)&4095u;v=(v-8)&4095u;
    }else {u=(u+8)&65535u;v=(v+8)&65535u;}
    uint64_t words[8]={UINT64_C(0x5400000000008001),0x53530u,0x156u,
        u|(v<<16),UINT64_C(0xffffff00000000)|x|(y<<16),
        (u+tw)|((v+h)<<16),UINT64_C(0xffffff00000000)|(x+sw)|((y+h)<<16),0};
    memcpy(output,words,sizeof words);return 1;
}
#endif
