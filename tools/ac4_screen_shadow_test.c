#include "ps2_runtime.h"
#include "ps2_hook.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static u8 mem[65536];static int hooks,removed,fail=-1,bad;
static u32 rd(u32 a){
 const u32 addresses[]={0x15fee0,0x15fee4,0x15fee8,0x15feec,0x1041e8,0x1041ec,0x1041f0,0x1041f4};
 const u32 words[]={0x27bdff80,0x8f82aab8,0xffb60060,0x3c0c0001,0x8f8baab8,0x00061100,0x8c8a0004,0x24420010};
 for(unsigned i=0;i<8;i++)if(a==addresses[i])return words[i]^(bad?1u:0u);
 const u32 hud_addresses[]={0x162958,0x1633c8,0x163818,0x163c08,0x162d98,0x1609d8,0x161250,0x160ff8,0x161890,0x160e78,0x161720,0x169010,0x16bc68};
 const u32 hud_words[][4]={{0x27bdff40,0x3c020040,0xffb700a0,0x3c030040},{0x27bdff20,0x3c020040,0xffb60090,0x3c030040},{0x27bdff50,0x3c020040,0xffb40080,0x3c060040},{0x27bdff10,0x3c020041,0xffb40090,0x3c030041},{0x27bdff70,0x3c020041,0xffb40070,0x3c030041},{0x27bdff20,0x8f82aab8,0xffb60090,0x3c060001},{0x27bdff20,0x8f82aab8,0xffb700a0,0x3c060001},{0x27bdff40,0x8f82aab8,0xffbe00a0,0x3c070001},{0x27bdff40,0x8f82aab8,0xffb60080,0x3c060001},{0x27bdffa0,0x8f82aab8,0xffb30040,0x3c060001},{0x27bdffa0,0x8f82aab8,0xffb30040,0x3c060001},{0x27bdfdb0,0x3c020040,0xffb201a0,0x27ab0020},{0x27bdfe90,0x3c020041,0xffb00080,0x3c030041}};
 for(unsigned i=0;i<sizeof hud_addresses/sizeof hud_addresses[0];i++)for(unsigned j=0;j<4;j++)if(a==hud_addresses[i]+4*j)return hud_words[i][j]^(bad?1u:0u);
 assert(a+4<=sizeof mem);u32 v;memcpy(&v,mem+a,4);return v;
}
static void get(void *v,u32 a,size_t n){assert(a+n<=sizeof mem);memcpy(v,mem+a,n);}
static void wr(u32 a,u32 v){assert(a+4<=sizeof mem);memcpy(mem+a,&v,4);}
#define ps2_r32 rd
#define ps2_get_mem get
#define ps2_log log_stub
static unsigned external_tags;
static void log_stub(const char *f,...){if(strstr(f,"ac4 HUD external tag:"))external_tags++;}
static unsigned queued;
static void ac4_screen_queue(u32 packet,u32 caller){assert(packet==0x8010 && caller==0);queued++;}
#include "../runtime/src/rn/rn_ac4_screen_sprite_audit.h"
#include "rn_ac4_hud_group.h"
static int ac4_hud_queue(u32 start,u32 end,u32 site,const ac4_hud_group *plan){(void)start;(void)end;(void)site;(void)plan;assert(0);return 0;}
#include "../runtime/src/rn/rn_ac4_hud_group_audit.h"
int ps2_hook_before(u32 a,ps2_hook_fn f,void *u,int p,const char *o){(void)a;(void)f;(void)u;(void)p;(void)o;int i=hooks++;return i==fail?-1:i+1;}
int ps2_hook_after(u32 a,ps2_hook_fn f,void *u,int p,const char *o){return ps2_hook_before(a,f,u,p,o);}
int ps2_hook_remove(int h){assert(h>0);removed++;return 0;}
static void reset(ps2_ctx *c){
 memset(&ac4_screen_shadow,0,sizeof ac4_screen_shadow);ac4_screen_shadow.enabled=1;
 memset(c,0,sizeof *c);memset(mem,0,sizeof mem);c->r[28].uw[0]=0x6000;c->r[29].uw[0]=0xf000;
 wr(0x6000-0x5548,0x1000);wr(0x1038,0x8000);
 c->r[6].uw[0]=0x8000;c->r[7].uw[0]=0x8100;c->r[8].uw[0]=0x100;c->r[9].uw[0]=0x200;c->r[10].uw[0]=0x80;c->r[11].uw[0]=0x40;
}
static void original_packet(void){
 const u64 packet[]={UINT64_C(0x5400000000008001),0x53530,0x156,0x02080108,
 UINT64_C(0x00ffffff81008000),0x02480188,UINT64_C(0x00ffffff81408080),0};
 memcpy(mem+0x8010,packet,sizeof packet);wr(0x8000,0x20000004);wr(0x8004,0x12345678);wr(0x8008,0);wr(0x800c,0x50000004);wr(0x1038,0x8050);
}
static void hud_tests(void){
 _putenv_s("PS2_AC4_HUD_GROUP_NATIVE","0");
 _putenv_s("PS2_AC4_HUD_CANDIDATE_DUMP","");
 _putenv_s("PS2_AC4_HUD_GROUP_AUDIT","0");ac4_hud_init();assert(!hooks);
 _putenv_s("PS2_AC4_HUD_GROUP_AUDIT","1");bad=1;ac4_hud_init();assert(!hooks);bad=0;
 for(int i=0;i<(int)(2*AC4_HUD_SITES);i++){hooks=removed=0;fail=i;ac4_hud_init();assert(hooks==i+1 && removed==i && !ac4_hud_audit.enabled);}
 hooks=removed=0;fail=-1;ac4_hud_init();ac4_hud_init();assert(hooks==2*AC4_HUD_SITES && !removed && ac4_hud_audit.enabled);
 ac4_hud_report();
 memset(mem,0,sizeof mem);wr(0x2000,0x20000000);wr(0x2004,0x2000);
 ac4_hud_trace_chain(0x2000);assert(external_tags==16); /* Cyclic NEXT is bounded. */
 ac4_hud_trace_chain(PS2_RAM_SIZE);ac4_hud_trace_chain(0x2001);assert(external_tags==16);
 wr(0x2000,0x50000000);ac4_hud_trace_chain(0x2000);assert(external_tags==17); /* CALL is not followed. */
 ac4_hud_group plan,saved;unsigned char packet[2080]={0};
 const u32 tag[]={0x20000001,0xdeadbeef,0,0x50000001};
 for(unsigned i=0;i<65;i++)memcpy(packet+i*32,tag,16);
 assert(ac4_hud_group_plan(packet,64,&plan) && plan.count==2);
 assert(plan.payload[0].offset==16 && plan.payload[0].bytes==16 && plan.payload[1].offset==48);
 assert(ac4_hud_group_plan(packet,2048,&plan) && plan.count==64);saved=plan;
 assert(!ac4_hud_group_plan(packet,2080,&plan) && !memcmp(&plan,&saved,sizeof plan));
 for(unsigned n=0;n<32;n++)assert(!ac4_hud_group_plan(packet,n,&plan));
 assert(!ac4_hud_group_plan(NULL,32,&plan) && !ac4_hud_group_plan(packet,32,NULL));
 for(unsigned fault=0;fault<5;fault++){
  u32 wrong[4];memcpy(wrong,tag,16);
  if(fault==0)wrong[0]=0x10000001;
  if(fault==1)wrong[0]=0x20000000;
  if(fault==2)wrong[0]=0x20000002;
  if(fault==3)wrong[2]=1;
  if(fault==4)wrong[3]=0x50000002;
  memcpy(packet+32,wrong,16);
  assert(!ac4_hud_group_plan(packet,64,&plan) && !memcmp(&plan,&saved,sizeof plan));
 }
 /* External chains require a separately validated, geometry-free texture upload. */
 unsigned char allocation[64]={0};u32 call[4]={0x50000000,0x2000,0,0},link[4]={0x20000000,0,0,0};
 memcpy(allocation,tag,16);memcpy(allocation+32,call,16);memcpy(allocation+48,link,16);
 assert(ac4_hud_group_plan(allocation,64,&plan) && plan.count==1 && plan.external_count==1 && plan.external[0].address==0x2000);
 allocation[56]=1;assert(!ac4_hud_group_plan(allocation,64,&plan));
 u64 upload[22]={0};u32 dma[4]={0x10000009,0,0,0x51000009};memcpy(upload,dma,16);
 upload[2]=UINT64_C(0x4000000000000001);upload[3]=0xeeee;
 for(unsigned i=0;i<4;i++)upload[5+2*i]=0x50+i;
 upload[12]=UINT64_C(0x0800000000000001);upload[16]=UINT64_C(0x1000000000008001);upload[17]=14;upload[18]=1;upload[19]=0x3f;upload[20]=0x60000000;
 assert(ac4_hud_upload_valid(upload,sizeof upload));
 for(unsigned i=0;i<22;i++) {
  if(i==4 || i==6 || i==8 || i==14 || i==15)continue; /* Transfer parameters/image data. */
  upload[i]^=1;assert(!ac4_hud_upload_valid(upload,sizeof upload));upload[i]^=1;
 }
 u64 multi[36]={0};memcpy(multi,upload,128);memcpy(multi+16,upload+2,112);memcpy(multi+30,upload+16,48);
 ((u32*)multi)[0]=0x10000010;((u32*)multi)[3]=0x51000010;
 assert(ac4_hud_upload_valid(multi,sizeof multi));
 multi[19]=0x05;assert(!ac4_hud_upload_valid(multi,sizeof multi)); /* No geometry AD register. */
 assert(!ac4_hud_upload_valid(upload,sizeof upload-16) && !ac4_hud_upload_valid(NULL,sizeof upload));
 ps2_ctx c,cs;u8 ms[sizeof mem];reset(&c);memset(&ac4_hud_audit,0,sizeof ac4_hud_audit);ac4_hud_audit.enabled=1;
 ac4_hud_before(&c,0);original_packet();cs=c;memcpy(ms,mem,sizeof mem);ac4_hud_after(&c,0);
 assert(ac4_hud_audit.site[0].matched==1 && ac4_hud_audit.site[0].packets==1);
 assert(!memcmp(&cs,&c,sizeof c) && !memcmp(ms,mem,sizeof mem));
 ac4_hud_before(&c,0);ac4_hud_after(&c,0);assert(ac4_hud_audit.site[0].empty==1);
 reset(&c);ac4_hud_before(&c,0);original_packet();wr(0x8008,1);ac4_hud_after(&c,0);assert(ac4_hud_audit.site[0].declined==1);
 reset(&c);ac4_hud_before(&c,0);ac4_hud_after(&c,(void*)1);assert(ac4_hud_audit.unpaired==1);
 ac4_hud_after(&c,0);assert(ac4_hud_audit.unpaired==2 && !ac4_hud_audit.depth);
 reset(&c);memset(&ac4_hud_audit,0,sizeof ac4_hud_audit);ac4_hud_audit.enabled=1;
 for(unsigned i=0;i<9;i++)ac4_hud_before(&c,0);
 original_packet();for(unsigned i=0;i<9;i++)ac4_hud_after(&c,0);
 assert(ac4_hud_audit.site[0].matched==8 && ac4_hud_audit.site[0].declined==1 && !ac4_hud_audit.depth);
 reset(&c);ac4_hud_before(&c,0);wr(0x1038,0x7ff0);ac4_hud_after(&c,0);assert(ac4_hud_audit.site[0].declined==2);
 reset(&c);wr(0x1038,0x8001);ac4_hud_before(&c,0);ac4_hud_after(&c,0);assert(ac4_hud_audit.site[0].declined==3);
 reset(&c);ac4_hud_audit.native=1;ac4_hud_before(&c,(void*)12);original_packet();ac4_hud_after(&c,(void*)12);
 assert(ac4_hud_audit.site[12].matched==1 && !ac4_hud_audit.queued); /* Candidate must never invoke native queue. */
 ac4_hud_audit.native=0;
 puts("AC4 HUD group: multi-packet preflight, atomic rejection, bounds, empty groups and observer immutability passed");
}
int main(void){
 hud_tests();hooks=removed=0;memset(&ac4_screen_shadow,0,sizeof ac4_screen_shadow);

 _putenv_s("PS2_AC4_SCREEN_SPRITE_AUDIT","0");ac4_screen_shadow_init();assert(!hooks);
 _putenv_s("PS2_AC4_SCREEN_SPRITE_AUDIT","1");bad=1;ac4_screen_shadow_init();assert(!hooks);bad=0;
 fail=0;ac4_screen_shadow_init();assert(hooks==1 && !removed && !ac4_screen_shadow.enabled);
 hooks=0;fail=1;ac4_screen_shadow_init();assert(hooks==2 && removed==1 && !ac4_screen_shadow.enabled);
 hooks=removed=0;fail=-1;ac4_screen_shadow_init();ac4_screen_shadow_init();assert(hooks==2 && !removed && ac4_screen_shadow.enabled);
 ps2_ctx c,saved;u8 savedmem[sizeof mem];reset(&c);saved=c;memcpy(savedmem,mem,sizeof mem);
 assert(!ac4_screen_before(&c,0));assert(!memcmp(&saved,&c,sizeof c) && !memcmp(savedmem,mem,sizeof mem));
 original_packet();memcpy(savedmem,mem,sizeof mem);assert(!ac4_screen_after(&c,0));
 assert(ac4_screen_shadow.matched==1 && !ac4_screen_shadow.depth && !ac4_screen_shadow.mismatched);
 assert(!memcmp(&saved,&c,sizeof c) && !memcmp(savedmem,mem,sizeof mem));
 for(unsigned fault=0;fault<6;fault++){
  reset(&c);ac4_screen_before(&c,0);original_packet();
  if(fault==0)mem[0x8020]^=1;
  if(fault==1)wr(0x1038,0x8060);
  if(fault==2)wr(0x8000,0x20000003);
  if(fault==3)wr(0x8008,1);
  if(fault==4)wr(0x800c,0x50000003);
  if(fault==5)c.r[2].ud[0]=1;
  ac4_screen_after(&c,0);assert(ac4_screen_shadow.mismatched==1 && !ac4_screen_shadow.matched);
 }
 for(unsigned fault=0;fault<4;fault++){
  reset(&c);
  if(fault==0)c.r[28].uw[0]=0;
  if(fault==1)c.r[29].uw[0]=UINT32_MAX;
  if(fault==2)wr(0x6000-0x5548,PS2_RAM_SIZE-4);
  if(fault==3)wr(0x1038,0x8001);
  ac4_screen_before(&c,0);ac4_screen_after(&c,0);assert(ac4_screen_shadow.declined==1 && !ac4_screen_shadow.matched && !ac4_screen_shadow.depth);
 }
 reset(&c);for(unsigned i=0;i<AC4_SCREEN_SHADOW_DEPTH+1;i++)ac4_screen_before(&c,0);
 original_packet();for(unsigned i=0;i<AC4_SCREEN_SHADOW_DEPTH+1;i++)ac4_screen_after(&c,0);
 assert(ac4_screen_shadow.matched==AC4_SCREEN_SHADOW_DEPTH && ac4_screen_shadow.declined==1 && !ac4_screen_shadow.depth);
 ac4_screen_after(&c,0);assert(ac4_screen_shadow.unpaired==1);
 reset(&c);ac4_screen_shadow.native=1;ac4_screen_before(&c,0);original_packet();ac4_screen_after(&c,0);assert(queued==1);
 reset(&c);ac4_screen_shadow.native=1;ac4_screen_before(&c,0);original_packet();mem[0x8020]^=1;ac4_screen_after(&c,0);assert(queued==1);
 ac4_screen_shadow_report();puts("AC4 screen shadow: packet/allocator evidence, immutability, bounds, nested calls and hook rollback passed");
}
