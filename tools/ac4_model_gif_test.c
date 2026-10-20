/* Compare specialized payload decoding with the generic parser, including state. */
#include <assert.h>
#include <string.h>
#include "../runtime/src/ps2_hw.c"
int ps2_diag_armed;
static u32 wr[256],nw;static u64 val[256];
void ps2_gs_write_reg(u32 r,u64 v) {assert(nw<256);wr[nw]=r;val[nw++]=v;}
void ps2_gs_hwreg_pair(u64 a,u64 b) {(void)a;(void)b;assert(0);}
void ps2_log(const char *s,...) {(void)s;}
int main(void) {
    ps2_reg128 mem[64]={0};
    for(int i=0;i<30;i++) for(int k=0;k<4;k++)mem[(61+i)%64].uw[k]=(u32)(i*1234567+k*9876);
    for(int i=0;i<10;i++)if(i&1)mem[(63+3*i)%64].uw[3]|=0x8000;
    const u32 counts[]={3,4,9,10};
    for(u32 test=0;test<4;test++)for(u32 eop=0;eop<2;eop++) {
        u32 n=counts[test];nw=0;
        memset(&gif,0,sizeof gif);gif.active=1;gif.nreg=3;gif.nloop=n;gif.regs=0x412;gif.eop=eop;gif_q_latch=0;
        for(u32 i=0;i<3*n;i++)gif_feed(mem+(61+i)%64);
        ps2_gif_capstate expected={0};ps2_gif_cap_save(&expected);
        u32 er[256],count=nw;u64 ev[256];memcpy(er,wr,sizeof wr);memcpy(ev,val,sizeof val);
        memset(&gif,0,sizeof gif);gif.active=1;gif.nreg=3;gif.nloop=n;gif.regs=0x412;gif.eop=eop;gif_q_latch=0;nw=0;
        assert(!gif_model_vertices((u8*)mem,64,61,3*n-1));assert(!nw&&gif.active&&gif.n==0);
        assert(gif_model_vertices((u8*)mem,64,61,3*n)==(int)(3*n));
        ps2_gif_capstate actual={0};ps2_gif_cap_save(&actual);
        assert(nw==count&&!memcmp(er,wr,nw*sizeof(u32))&&!memcmp(ev,val,nw*sizeof(u64)));
        assert(!memcmp(&expected,&actual,sizeof expected));
        assert(!gif_model_vertices((u8*)mem,64,61,3*n));
    }
    gif.active=1;gif.n=0;gif.regs=0x512;assert(!gif_model_vertices((u8*)mem,64,61,30));
    gif.regs=0x412;gif.r=1;assert(!gif_model_vertices((u8*)mem,64,61,30));
    gif.r=0;gif.nloop=11;assert(!gif_model_vertices((u8*)mem,64,61,33));
    gif.nloop=2;assert(!gif_model_vertices((u8*)mem,64,61,6));
    /* Wrapped two-stream packet with A+D setup and PRE in both geometry tags. */
    ps2_reg128 packet[64]={0};u32 at=0;
    packet[at].ud[0]=2|((u64)1<<60);packet[at++].ud[1]=14;
    packet[at].ud[0]=0x12345678;packet[at++].ud[1]=6;
    packet[at].ud[0]=0x87654321;packet[at++].ud[1]=8;
    for(int pass=0;pass<2;pass++) {
        packet[at].ud[0]=4|((u64)3<<60)|((u64)1<<46)|((u64)(0x14+pass)<<47)|(pass?32768:0);
        packet[at++].ud[1]=0xabc0000000000412ull;
        for(int j=0;j<12;j++,at++)for(int k=0;k<4;k++)packet[at].uw[k]=(u32)(j*98765+k*321);
    }
    for(u32 j=0;j<at;j++)mem[(61+j)%64]=packet[j];
    memset(&gif,0,sizeof gif);gif_q_latch=123;nw=0;memset(gif_tag_hist,0,sizeof gif_tag_hist);memset(gif_tag_qwc,0,sizeof gif_tag_qwc);
    for(u32 j=0;j<at;j++)gif_feed(packet+j);
    ps2_gif_capstate expected,actual;ps2_gif_cap_save(&expected);
    u32 expected_regs[256],expected_count=nw;u64 expected_values[256];
    memcpy(expected_regs,wr,sizeof wr);memcpy(expected_values,val,sizeof val);
    u64 expected_hist[4],expected_qwc[4];memcpy(expected_hist,gif_tag_hist,sizeof expected_hist);memcpy(expected_qwc,gif_tag_qwc,sizeof expected_qwc);
    memset(&gif,0,sizeof gif);gif_q_latch=123;nw=0;memset(gif_tag_hist,0,sizeof gif_tag_hist);memset(gif_tag_qwc,0,sizeof gif_tag_qwc);
    /* Unsupported final tag must reject before the valid setup writes. */
    mem[(61+16)%64].ud[0]|=(u64)1<<58;
    assert(!gif_mesh_packet((u8*)mem,64,61)&&!nw&&!gif_tag_hist[0]&&gif_q_latch==123);
    mem[(61+16)%64].ud[0]&=~((u64)1<<58);
    assert(gif_mesh_packet((u8*)mem,64,61)==at);ps2_gif_cap_save(&actual);
    assert(nw==expected_count&&!memcmp(wr,expected_regs,nw*sizeof(u32))&&!memcmp(val,expected_values,nw*sizeof(u64)));
    assert(!memcmp(&expected,&actual,sizeof expected)&&!memcmp(expected_hist,gif_tag_hist,sizeof expected_hist)&&!memcmp(expected_qwc,gif_tag_qwc,sizeof expected_qwc));
    /* Cloud sprite rows: all writes, Q latch and parser continuation must
       match, including the maximum 64-piece row and every truncated prefix. */
    const u32 rows[]={1,2,64};
    for(unsigned test=0;test<3;test++) {
        ps2_reg128 row[129]={0};u32 loops=rows[test],length=1+2*loops;
        row[0].ud[0]=UINT64_C(0x4400000000008000)|loops;row[0].ud[1]=0x53d3;
        for(u32 j=1;j<length;j++){row[j].ud[0]=UINT64_C(0xabcdef1234000000)+j;row[j].ud[1]=UINT64_C(0x9876543210000000)+j;}
        memset(&gif,0,sizeof gif);gif_q_latch=123;nw=0;
        memset(gif_tag_hist,0,sizeof gif_tag_hist);memset(gif_tag_qwc,0,sizeof gif_tag_qwc);
        for(u32 j=0;j<length;j++)gif_feed(row+j);
        ps2_gif_cap_save(&expected);expected_count=nw;
        memcpy(expected_regs,wr,sizeof wr);memcpy(expected_values,val,sizeof val);
        memcpy(expected_hist,gif_tag_hist,sizeof expected_hist);memcpy(expected_qwc,gif_tag_qwc,sizeof expected_qwc);
        memset(&gif,0,sizeof gif);gif_q_latch=123;nw=0;
        memset(gif_tag_hist,0,sizeof gif_tag_hist);memset(gif_tag_qwc,0,sizeof gif_tag_qwc);
        ps2_gif_capstate initial;ps2_gif_cap_save(&initial);
        for(u32 j=0;j<length;j++) {
            assert(!gif_sprite_packet(row,j));ps2_gif_cap_save(&actual);
            assert(!nw&&!memcmp(&initial,&actual,sizeof initial)&&!gif_tag_hist[1]);
        }
        row[0].ud[1]^=1;assert(!gif_sprite_packet(row,length)&&!nw);row[0].ud[1]^=1;
        row[0].ud[0]^=32768;assert(!gif_sprite_packet(row,length)&&!nw);row[0].ud[0]^=32768;
        gif.active=1;assert(!gif_sprite_packet(row,length)&&!nw);gif.active=0;
        assert(gif_sprite_packet(row,length)==length);ps2_gif_cap_save(&actual);
        assert(nw==expected_count&&!memcmp(wr,expected_regs,nw*sizeof(u32))&&!memcmp(val,expected_values,nw*sizeof(u64)));
        assert(!memcmp(&expected,&actual,sizeof expected)&&!memcmp(expected_hist,gif_tag_hist,sizeof expected_hist)&&!memcmp(expected_qwc,gif_tag_qwc,sizeof expected_qwc));
    }
    puts("AC4 mesh GIF: model/terrain and sprite rows match generic registers and state");return 0;
}
