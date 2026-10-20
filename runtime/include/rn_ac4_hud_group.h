#ifndef RN_AC4_HUD_GROUP_H
#define RN_AC4_HUD_GROUP_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#define AC4_HUD_GROUP_PACKETS 64u
typedef struct {uint32_t offset,bytes;} ac4_hud_payload;
typedef struct {uint32_t address,bytes;} ac4_hud_external;
typedef struct {uint32_t count,external_count;ac4_hud_payload payload[AC4_HUD_GROUP_PACKETS];ac4_hud_external external[8];} ac4_hud_group;
/* Allocation order is not DMA traversal order. Record individual payloads;
   NEXT link addresses are mutable and deliberately not treated as payload. */
static inline int ac4_hud_group_plan(const void *memory,size_t bytes,ac4_hud_group *out) {
    if(!memory || !out || !bytes || bytes>32768 || (bytes&15))return 0;
    ac4_hud_group result={0};size_t at=0;
    while(at<bytes) {
        uint32_t tag[4];memcpy(tag,(const unsigned char*)memory+at,16);
        if(tag[0]==0x50000000u && tag[1] && !(tag[1]&15) && !tag[2] && !tag[3]) {
            uint32_t next[4];
            if(bytes-at<32 || result.external_count==8)return 0;
            memcpy(next,(const unsigned char*)memory+at+16,16);
            if(next[0]!=0x20000000u || next[2] || next[3])return 0;
            result.external[result.external_count++]=(ac4_hud_external){tag[1],0};
            at+=32;continue;
        }
        uint32_t qwords=tag[0]&65535u;
        if((tag[0]&0xffff0000u)!=0x20000000u || !qwords || tag[2]
           || tag[3]!=(0x50000000u|qwords) || qwords>(bytes-at-16)/16
           || result.count==AC4_HUD_GROUP_PACKETS)return 0;
        result.payload[result.count++]=(ac4_hud_payload){(uint32_t)at+16,qwords*16};
        at+=16+(size_t)qwords*16;
    }
    if(!result.count)return 0;
    *out=result;return 1;
}
/* Recognized texture-only CALL target: CNT/DIRECTHL, four transfer registers,
 * one IMAGE packet per upload, TEXFLUSH, empty RET. Texture data stays on the original GS path. */
static inline int ac4_hud_upload_valid(const void *memory,size_t bytes) {
    if(!memory || bytes<144 || bytes>16416 || (bytes&15))return 0;
    const unsigned char *p=memory;uint32_t tag[4],ret[4];uint64_t gif[2],ad[2];
    memcpy(tag,p,16);uint32_t qwc=tag[0]&65535;
    if(tag[0]!=(0x10000000u|qwc) || tag[1] || tag[2] || tag[3]!=(0x51000000u|qwc)
       || bytes!=32+(size_t)qwc*16 || qwc<7)return 0;
    size_t at=16,end=16+(size_t)qwc*16;unsigned uploads=0;int flushed=0;
    while(at<end) {
        memcpy(gif,p+at,16);
        if(gif[0]==UINT64_C(0x1000000000008001) && gif[1]==14) {
            if(end-at!=32 || !uploads)return 0;
            memcpy(ad,p+at+16,16);
            if(ad[0]!=1 || ad[1]!=0x3f)return 0;
            flushed=1;at=end;break;
        }
        if(end-at<112 || ++uploads>8 || gif[0]!=UINT64_C(0x4000000000000001) || gif[1]!=0xeeee)return 0;
        for(unsigned i=0;i<4;i++) {
            memcpy(ad,p+at+16+16*i,16);
            if(ad[1]!=0x50u+i || (i==3 && ad[0]))return 0;
        }
        memcpy(gif,p+at+80,16);uint64_t image=gif[0]&0x7fff;
        if(!image || gif[0]!=(UINT64_C(0x0800000000000000)|image) || gif[1]
           || image>(end-at-96)/16)return 0;
        at+=96+(size_t)image*16;
    }
    if(!flushed)return 0;
    memcpy(ret,p+16+(size_t)qwc*16,16);
    return ret[0]==0x60000000u && !ret[1] && !ret[2] && !ret[3];
}
#endif
