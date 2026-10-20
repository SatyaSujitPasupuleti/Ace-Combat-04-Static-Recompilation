#include "rn_ac4_cloud_row.h"
#include "rn_ac4_cloud_sprite_packet.h"
#include <assert.h>
#include <stdio.h>
int main(void){
    uint64_t out[AC4_CLOUD_ROW_BYTES/8],before[AC4_CLOUD_ROW_BYTES/8];size_t bytes=99;
    memset(out,0x5a,sizeof out);memcpy(before,out,sizeof out);
    assert(ac4_cloud_row_build(1000,1100,0,100,out,sizeof out,&bytes) && bytes==80);
    assert(out[0]==((UINT64_C(0x8800)<<47)|0x8002) && out[1]==0x53d3);
    assert(out[2]==0 && out[3]==1000 && out[4]==24 && out[5]==1024);
    assert(out[6]==24 && out[7]==1024 && out[8]==100 && out[9]==1100);
    assert(!memcmp(out+10,before+10,sizeof out-80));
    memcpy(out,before,sizeof out);bytes=99;
    assert(!ac4_cloud_row_build(0,65535,0,65535,out,16,&bytes) && bytes==99 && !memcmp(out,before,sizeof out));
    assert(ac4_cloud_row_build(0,65535,0,65535,out,sizeof out,&bytes) && bytes==AC4_CLOUD_ROW_BYTES);
    memcpy(out,before,sizeof out);assert(ac4_cloud_row_build(100,100,0,99,out,0,&bytes) && !bytes && !memcmp(out,before,sizeof out));
    assert(ac4_cloud_row_build(200,100,0,99,NULL,0,&bytes) && !bytes);
    ps2_ctx ctx={0};ctx.r[4].ud[0]=0x1000;ctx.r[5].ud[0]=1000;ctx.r[6].ud[0]=1100;ctx.r[8].ud[0]=100;
    ctx.r[10].ud[1]=123;ctx.f[3].u=0x12345678;ctx.vu0_flag_slot=2;ctx.vu0_flag_pending=8;ctx.vu0_flags[3].mac=1;
    ps2_ctx saved=ctx;bytes=99;memcpy(out,before,sizeof out);
    assert(!ac4_cloud_row_plan(&ctx,out,16,&bytes) && bytes==99);
    assert(!memcmp(&ctx,&saved,sizeof ctx) && !memcmp(out,before,sizeof out));
    assert(ac4_cloud_row_plan(&ctx,out,sizeof out,&bytes) && bytes==80);
    assert(ctx.r[2].ud[0]==0x1050 && ctx.r[25].ud[0]==0x1050 && ctx.r[7].ud[0]==2);
    assert(ctx.r[10].ud[0]==1100 && ctx.r[10].ud[1]==123 && ctx.r[11].ud[0]==0);
    assert(ctx.f[0].f==100 && ctx.f[1].f==1 && ctx.f[2].u==100 && ctx.f[3].u==0x12345678);
    assert(!ctx.vu0_flag_pending && ctx.vu0_flag_slot==2 && ctx.vu0.mac==1 && ctx.vu0.status==65);
    ctx=saved;ctx.r[6].ud[0]=900;ctx.f[2].u=123;
    assert(ac4_cloud_row_plan(&ctx,NULL,0,&bytes) && !bytes);
    assert(ctx.r[2].ud[0]==0x1000 && ctx.r[5].ud[0]==0x1010 && ctx.r[11].sd[0]==-100);
    assert(ctx.f[0].f==-100 && ctx.f[1].f==-1 && ctx.f[2].u==123);
    ac4_cloud_sprite_record records[2]={0};size_t index=99;
    records[0].output.result=1;
    uint32_t x=1000,u=0,v=100;uint64_t z=1100;
    memcpy(records[0].output.bytes+16,&x,4);memcpy(records[0].output.bytes+24,&z,8);
    memcpy(records[0].output.bytes+8,&u,4);memcpy(records[0].output.bytes+12,&v,4);
    assert(ac4_cloud_sprite_find(records,2,1000,1100,0,100,&index) && index==0);
    records[1]=records[0];assert(ac4_cloud_sprite_find(records,2,1000,1100,0,100,&index));
    records[1].input.position[0]=1;index=99;
    assert(!ac4_cloud_sprite_find(records,2,1000,1100,0,100,&index) && index==99);
    assert(!ac4_cloud_sprite_find(records,1,1000,1100,0,101,&index) && index==99);
    records[0].output.result=-1;assert(!ac4_cloud_sprite_find(records,1,1000,1100,0,100,&index));
    puts("AC4 sprite row: boundary splits, UV interpolation, maximum width, empty rows and atomic capacity passed");
}
