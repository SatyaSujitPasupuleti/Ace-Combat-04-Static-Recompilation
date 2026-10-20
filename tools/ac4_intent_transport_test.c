#include "../runtime/src/rn/rn_intent.c"
#include <assert.h>
u8 *ps2_pt[PS2_PT_ENTRIES];
int g_cap_deep;
static unsigned sent,ended,ownership_warnings;
u32 rn_tap_open_emitter(u32 p){(void)p;return 0;}
void ps2_log(const char *format,...){if(strstr(format,"ac4 sky ownership:"))ownership_warnings++;}
u32 ps2_mmio_r32(u32 a){(void)a;assert(0);return 0;}
u64 ps2_mmio_r64(u32 a){(void)a;assert(0);return 0;}
void ps2_cap_intent(const u8 *rec,u32 len){(void)rec;(void)len;assert(0);}
void ps2_gfxq_intent(const u8 *rec,u32 len){
 assert(len==sizeof(rn_intent_hdr));rn_intent_hdr h;memcpy(&h,rec,sizeof h);
 if(h.kind==RN_INT_CLAIM_END)ended++;else{assert(h.kind==RN_INT_AC4_SKY || h.kind==RN_INT_AC4_SPRITE || h.kind==RN_INT_GROUP2D);sent++;}
}
static void add(u32 start,u32 end){
 rn_intent_hdr h={0};h.kind=RN_INT_AC4_SKY;h.len=sizeof h;
 u8 *rec=pend_add(start,end,sizeof h);assert(rec);memcpy(rec,&h,sizeof h);
}
int main(void){
 arena=malloc(ARENA_BYTES);assert(arena);
 add(0x200,0x280);add(0x100,0x180); /* Deliberately unsorted. */
 rn_dma_transfer_slow(1,0x80,1,0);assert(!sent);
 rn_dma_transfer_slow(1,0x100,4,0);assert(sent==1 && claim_open);
 rn_dma_transfer_slow(1,0x100,4,1);assert(!ended && claim_open);
 rn_dma_transfer_slow(1,0x140,4,0);assert(sent==1);
 rn_dma_transfer_slow(1,0x140,4,1);assert(ended==1 && !claim_open);
 rn_dma_transfer_slow(1,0x200,8,0);assert(sent==2);
 rn_dma_transfer_slow(1,0x200,8,1);assert(ended==2);
 rn_intent_frame_end();assert(!n_pend && !arena_n && !rn_intents_pending);
 assert(st_int.dropped==0 && st_int.emitted==2);
 u8 memory[4096]={0};ps2_pt[0]=memory;
 add(0x300,0x340);pends[0].ac4_packet_hash=ac4_hash_packet(0x300,0x340);
 assert(pends[0].ac4_packet_hash==ac4_hash_packet(0x300,0x340));
 memory[0x310]=1;assert(pends[0].ac4_packet_hash!=ac4_hash_packet(0x300,0x340));
 rn_dma_transfer_slow(1,0x300,4,0);assert(ownership_warnings==1);
 assert(sent==2 && !claim_open && ac4_stale_packets==1 && pends[0].emitted);
 rn_dma_transfer_slow(1,0x300,4,1);rn_intent_frame_end();
 add(0x300,0x340);pends[0].ac4_packet_hash=ac4_hash_packet(0x300,0x340);
 rn_dma_transfer_slow(1,0x310,3,0);assert(sent==2 && !claim_open && ac4_partial_packets==1);
 rn_intent_frame_end();
 add(0x300,0x340);pends[0].ac4_packet_hash=ac4_hash_packet(0x300,0x340);
 rn_dma_transfer_slow(1,0x300,4,0);assert(sent==3 && claim_open);
 rn_dma_transfer_slow(1,0x300,4,1);rn_intent_frame_end();
 /* Reclaim after a DMA sort: retain live bytes in allocation order, discard
  * delivered and overwritten records, and preserve the open claim. */
 add(0x500,0x540);add(0x100,0x140);add(0x700,0x740);
 pends[2].ac4_packet_hash=ac4_hash_packet(0x700,0x740);
 memory[0x710]=1;
 rn_dma_transfer_slow(1,0x100,1,0);assert(claim_open);
 ac4_reclaim_pending();assert(n_pend==1 && arena_n==sizeof(rn_intent_hdr));
 assert(pends[0].start==0x500 && pends[0].off==0 && claim_open);
 rn_dma_transfer_slow(1,0x500,4,0);assert(sent==5);
 rn_dma_transfer_slow(1,0x500,4,1);ac4_reclaim_pending();
 assert(!n_pend && !arena_n && !rn_intents_pending);
 u64 prefix[6]={UINT64_C(0x1000000000008002),14,0,8,0,0x40};
 memcpy(memory+0x900,prefix,sizeof prefix);assert(ac4_cloud_prefix(0x930));
 memory[0x928]=5;assert(!ac4_cloud_prefix(0x930)); /* Geometry in prefix. */
 assert(!ac4_cloud_prefix(16));
 add(0x200,0x240);add(0x100,0x140);
 for(u32 i=0;i<n_pend;++i){rn_intent_hdr h;memcpy(&h,arena+pends[i].off,sizeof h);h.kind=RN_INT_AC4_SPRITE;memcpy(arena+pends[i].off,&h,sizeof h);}
 const u32 chunks[5]={4,4,12,4,4};u32 address=0xc0,remaining=28;
 for(unsigned i=0;i<5;++i){
     u32 count=rn_dma_chunk_limit(address,remaining);assert(count==chunks[i]);
     rn_dma_transfer_slow(1,address,count,0);
     assert((i==1 || i==3)==!!claim_open);
     rn_dma_transfer_slow(1,address,count,1);assert(!claim_open);
     address+=count*16;remaining-=count;
 }
 assert(!remaining && address==0x280 && sent==7);
 assert(rn_dma_chunk_limit(0,0)==0);rn_intent_frame_end();
 add(0x100,0x140);assert(rn_dma_chunk_limit(0xc0,28)==28);rn_intent_frame_end();
 ac4_screen_queue(0x100,0x163660);assert(n_pend==1);
 rn_intent_hdr screen;memcpy(&screen,arena,sizeof screen);
 assert(screen.kind==RN_INT_GROUP2D && screen.site==0x163660 && screen.flags==RN_G_WORLD);
 assert(pends[0].start==0x100 && pends[0].end==0x140 && pends[0].ac4_packet_hash);
 assert(rn_dma_chunk_limit(0xc0,28)==4 && rn_dma_chunk_limit(0x100,24)==4);
 rn_dma_transfer_slow(1,0x100,4,0);assert(sent==8 && claim_open);
 rn_dma_transfer_slow(1,0x100,4,1);assert(!claim_open);rn_intent_frame_end();
 ac4_screen_queue(0x100,0x163660);memory[0x120]^=1;
 rn_dma_transfer_slow(1,0x100,4,0);assert(sent==8 && !claim_open);rn_intent_frame_end();
 /* Fixed parent claims include numeric payloads and replace child sprites atomically. */
 ac4_hud_group group={.count=2,.payload={{16,64},{96,32}}};
 ac4_screen_queue(0x110,0x163660);ac4_screen_queue(0x700,0x166488);
 assert(ac4_hud_queue(0x100,0x180,0x1633c8,&group) && n_pend==3);
 unsigned fixed=0,world=0;
 for(u32 i=0;i<n_pend;i++){rn_intent_hdr h;memcpy(&h,arena+pends[i].off,sizeof h);if(h.site==0x1633c8){assert(!h.flags);fixed++;}else{assert(h.site==0x166488 && h.flags==RN_G_WORLD);world++;}}
 assert(fixed==2 && world==1);
 unsigned before=sent;
 rn_dma_transfer_slow(1,0x160,2,0);assert(sent==before+1); /* Reverse traversal. */
 rn_dma_transfer_slow(1,0x160,2,1);assert(!claim_open);
 rn_dma_transfer_slow(1,0x110,4,0);assert(sent==before+2);
 rn_dma_transfer_slow(1,0x110,4,1);rn_intent_frame_end();
 ac4_screen_queue(0x110,0x163660);pend saved=pends[0];u8 savedrec[sizeof(rn_intent_hdr)];memcpy(savedrec,arena,sizeof savedrec);
 u32 used=arena_n;arena_n=ARENA_BYTES;
 assert(!ac4_hud_queue(0x100,0x180,0x1633c8,&group));arena_n=used;
 assert(n_pend==1 && !memcmp(&saved,pends,sizeof saved) && !memcmp(savedrec,arena,sizeof savedrec));
 group.payload[1].bytes=48;assert(!ac4_hud_queue(0x100,0x180,0x1633c8,&group));group.payload[1].bytes=32;
 group.payload[1].offset=32;assert(!ac4_hud_queue(0x100,0x180,0x1633c8,&group));group.payload[1].offset=96;
 rn_dma_transfer_slow(1,0x110,4,0);assert(!ac4_hud_queue(0x100,0x180,0x1633c8,&group));
 rn_dma_transfer_slow(1,0x110,4,1);assert(!ac4_hud_queue(0x100,0x180,0x1633c8,&group));rn_intent_frame_end();
 add(0x110,0x150);assert(!ac4_hud_queue(0x100,0x180,0x1633c8,&group));rn_intent_frame_end();
 assert(ac4_hud_queue(0x100,0x180,0x1633c8,&group));before=sent;memory[0x120]^=1;
 rn_dma_transfer_slow(1,0x110,4,0);assert(sent==before && !claim_open);rn_intent_frame_end();
 group.external_count=1;group.external[0]=(ac4_hud_external){0xa00,144};
 assert(ac4_hud_queue(0x100,0x180,0x1633c8,&group));before=sent;memory[0xa20]^=1;
 rn_dma_transfer_slow(1,0x110,4,0);assert(sent==before && !claim_open);rn_intent_frame_end();
 free(arena);puts("AC4 intent transport: unsorted ranges, split DMA, one-shot delivery, reclamation and closure passed");
}
