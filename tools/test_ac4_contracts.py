"""Exercise strict boot failures against the real runtime in child processes."""
from pathlib import Path
import os, subprocess, json
ROOT=Path(__file__).resolve().parents[1]
out=ROOT/'work/contract_tests'; out.mkdir(parents=True,exist_ok=True)
prefix=ROOT.parent/'toolchain/msys64/ucrt64'
env=os.environ.copy(); env['PATH']=str(prefix/'bin')+os.pathsep+env['PATH']
source=out/'strict.c'
source.write_text(r'''
#include "ps2_runtime.h"
#include "ps2_hle.h"
#include <string.h>
#include "ps2_vfs.h"
void ac4_movie_audio_capture(u32 a,u32 b) { (void)a;(void)b; }
void ac4_movie_audio_start(u32 a,u32 b) { (void)a;(void)b; }
void ac4_movie_audio_reset(void) {}
int ac4_movie_audio_sample(s16 v[2]) { (void)v;return 0; }
static int disc_ready=1;
ps2_pad_state ps2_pad_host[PS2_PAD_PORTS];
u16 ps2_pad_buttons(int port,const ps2_pad_state *st) { return st->buttons; }
int ps2_nustream_selftest(void) { abort(); }
u64 ps2_nustream_voice_mask(void) { return 0; }
void ps2_nustream_mix(s16 *out,u32 n,const s16 volumes[48][2]) { (void)out;(void)n;(void)volumes; }
void ps2_spu2_mix(s16 *,u32);
void ps2_ac4_audio_mix(s16 *,u32);
static u64 test_tick;
u64 ps2_kernel_vblank_count(void) { return test_tick; }
static u32 field_samples(void) { return (u32)(test_tick*4004/5-(test_tick-1)*4004/5); }
int ps2_vfs_ready(void) { return disc_ready; }
u32 ps2_vfs_file_count(void) { return 0; }
static int image_present=1;
const ps2_disc_file *ps2_vfs_find(const char *name) {
    static const ps2_disc_file image={.name="BIN/TEST.BIN",.lsn=123,.size=4096};
    if (strcmp(name,"IOPRP23.IMG") && !strstr(name,"SIO2MAN") && strcmp(name,"TEST.BIN")) return NULL;
    return image_present ? &image : NULL;
}
void ps2_ac4_base_services_register(void);
void ps2_sif_init(void);
void ps2_sif_set_reg(u32 reg,u32 val);
u32 ps2_sif_get_reg(u32 reg);
u32 ps2_sif_reg_read(u32 addr);
int ps2_sif_ac4_dma(u32 src,u32 dst,u32 size,u32 attr);
static void reset_packet(void) {
    const char *arg="rom0:UDNL cdrom0:\\IOPRP23.IMG;1";
    ps2_w32(0x5000,104); ps2_w32(0x5008,0x80000003);
    ps2_w32(0x5010,(u32)strlen(arg)); ps2_w32(0x5014,0);
    for (u32 i=0;i<strlen(arg);++i) ps2_w8(0x5018+i,(u8)arg[i]);
}
u32 ps2_vfs_sector_count(void) { return disc_ready?10000:0; }
void ps2_ac4_module_services(unsigned module);
u32 ps2_ac4_iop_alloc(u32);
int ps2_memcard_rpc(ps2_ctx *c,u32 f,u32 s,int ss,u32 r,int rs) { abort(); }
int ps2_vfs_read_sectors_guest(u32 lsn, u32 sectors, u32 addr) {
    if (lsn!=123 || sectors!=2) abort();
    for(u32 i=0;i<4096;++i) ps2_w8(addr+i,(u8)i);
    return 2;
}
void hle_sceSifBindRpc(ps2_ctx *ctx);
void hle_sceSifCallRpc(ps2_ctx *ctx);
/* Unused hardware routes deliberately abort if a rejection touches them. */
const ps2_func_entry ps2_func_table[1] = {{0}};
const unsigned ps2_func_count = 0;
const ps2_symbol ps2_symbols[1] = {{0}};
const unsigned ps2_symbol_count = 0;
u32 ps2_loop_ctr;
void ps2_loop_service(void) { abort(); }
void ps2_preempt(void) { abort(); }
void ps2_vu_control_stop(ps2_ctx *ctx, u32 val) { abort(); }
u16 ps2_mmio_r16(u32 a) { abort(); }
u8 ps2_mmio_r8(u32 a) { abort(); }
u32 ps2_mmio_r32(u32 a) { abort(); }
u64 ps2_mmio_r64(u32 a) { abort(); }
void ps2_mmio_r128(ps2_reg128 *v, u32 a) { abort(); }
void ps2_mmio_w16(u32 a,u16 v) { abort(); }
void ps2_mmio_w8(u32 a, u8 v) { abort(); }
void ps2_mmio_w32(u32 a, u32 v) { abort(); }
void ps2_mmio_w64(u32 a, u64 v) { abort(); }
void ps2_mmio_w128(u32 a, const ps2_reg128 *v) { abort(); }
int main(int argc, char **argv) {
    ps2_ctx ctx = {0};
    ctx.r[31].uw[0] = 0x00123458u;
    if (argc != 2) return 90;
    if (!strcmp(argv[1], "target")) {
        ps2_unknown_target(&ctx, 0x00123450u);
    } else if (!strcmp(argv[1], "bind")) {
        ctx.r[5].uw[0] = 0x12345678u;
        hle_sceSifBindRpc(&ctx);
    } else if (!strcmp(argv[1], "call")) {
        ps2_mem_init();
        ctx.r[4].uw[0] = 0x1000u;
        ctx.r[5].uw[0] = 7u;
        ctx.r[29].uw[0] = 0x2000u;
        ps2_w32(0x1010u, 0x12345678u);
        hle_sceSifCallRpc(&ctx);

    } else if (!strcmp(argv[1],"pad-disconnected")) {
        ps2_mem_init();ps2_sif_hle_init();ps2_ac4_module_services(3);
        ps2_w32(0x10000,18);
        ps2_rpc_call(0x80000100,1,0x10000,128,0x20000,128);
        if(ps2_r32(0x2000c)!=0x400) return 47;
        ps2_w32(0x10000,1);ps2_w32(0x10010,0x30000);ps2_w32(0x30100,0xabcdef01);
        ps2_rpc_call(0x80000100,1,0x10000,128,0x20000,128);
        if(ps2_r8(0x30070) || ps2_r8(0x300f0)) return 48;
        return ps2_r32(0x2000c)!=1 || ps2_r32(0x20014)<0x40000 || ps2_r32(0x30100)!=0xabcdef01;
    } else if (!strcmp(argv[1],"pad-connected")) {
        ps2_mem_init();ps2_sif_hle_init();ps2_ac4_module_services(3);
        ps2_pad_host[0]=(ps2_pad_state){.connected=1,.buttons=0x4008,.lx=40,.ly=60,.rx=80,.ry=100};
        ps2_w32(0x10000,1);ps2_w32(0x10010,0x30000);ps2_w32(0x30100,0xabcdef01);
        ps2_rpc_call(0x80000100,1,0x10000,128,0x20000,128);
        if(ps2_r16(0x30002)!=0xbff7 || ps2_r8(0x30004)!=80 || ps2_r8(0x30007)!=60 || ps2_r8(0x30070)!=6 || ps2_r32(0x30058)!=1 || ps2_r32(0x30100)!=0xabcdef01) return 53;
        ps2_w32(0x10000,6);ps2_w32(0x1000c,0);
        ps2_rpc_call(0x80000100,1,0x10000,128,0x20000,128);
        return ps2_r32(0x20014)!=1 || ps2_r8(0x30001)!=0x41;
    } else if (!strcmp(argv[1],"usb-disconnected")) {
        ps2_mem_init();ps2_sif_hle_init();ps2_ac4_module_services(8);
        for(u32 i=0;i<132;++i) ps2_w8(0x20000+i,0xff);
        ps2_rpc_call(0x80000210,2,0x10000,128,0x20000,128);
        for(u32 i=0;i<128;++i) if(ps2_r8(0x20000+i)) return 49;
        return ps2_r32(0x20080)!=0xffffffff;
    } else if (!strncmp(argv[1],"audio-",6)) {
        ps2_mem_init(); ps2_sif_hle_init();
        if(!strcmp(argv[1],"audio-sound-pcm") || !strcmp(argv[1],"audio-sound-reset")) {
            ps2_ac4_module_services(5);
            u32 src=ps2_ac4_iop_alloc(4096);
            for(u32 i=0;i<4096;i+=16) {
                ps2_iop_ram[src+i]=0x08;
                for(u32 k=2;k<16;++k) ps2_iop_ram[src+i+k]=0x33;
            }
            u32 p=0x10004;
            ps2_w32(0x10000,1);ps2_w32(p,10);ps2_w32(p+4,src);ps2_w32(p+8,0x1000);ps2_w32(p+12,4096);
            ps2_rpc_call(0x81000101,1,0x10000,9216,0x20000,1536);
            memset(ps2_ram+0x10004,0,72);ps2_w32(p,1);ps2_w8(p+14,0);
            ps2_w16(p+0x24,15);ps2_w16(p+0x2c,0x3fff);ps2_w16(p+0x2e,0x3fff);
            ps2_w8(p+0x30,60);ps2_w8(p+0x32,60);ps2_w16(p+0x3a,48000);ps2_w32(p+0x40,0x1000);
            ps2_rpc_call(0x81000101,1,0x10000,9216,0x20000,1536);
            s16 out[4096];ps2_spu2_mix(out,2048);int nonzero=0;
            for(int i=0;i<4096;++i) nonzero|=out[i];
            if(!nonzero) return 70;
            if(!strcmp(argv[1],"audio-sound-reset")) {
                ps2_sif_hle_init();ps2_spu2_mix(out,2048);
                for(int i=0;i<4096;++i) if(out[i]) return 79;
            }
            return 0;
        } else if(!strcmp(argv[1],"audio-vag-pcm") || !strcmp(argv[1],"audio-idle-queue") || !strcmp(argv[1],"audio-field-pump")) {
            ps2_ac4_module_services(6);ps2_ac4_module_services(5);
            ps2_rpc_call(0x81000102,0,0x10000,256,0x20000,256);
            for (u32 voice=24;voice<=25;++voice) {
                ps2_w32(0x30000,1);ps2_w32(0x30004,2);ps2_w8(0x30012,voice);
                ps2_w32(0x30014,1);ps2_w16(0x30030,0x2000);ps2_w16(0x30032,0x2000);
                ps2_rpc_call(0x81000101,1,0x30000,9216,0x40000,1536);
            }
            u32 base=ps2_r32(0x20000)+0x180+ps2_r32(0x20024)*0x20000;
            for(u32 i=0;i<0x20000;i+=16) {
                ps2_iop_ram[base+i]=0x0c;
                for(u32 k=2;k<16;++k) ps2_iop_ram[base+i+k]=(i/2048)%2?0xdd:0x33;
            }
            /* Host already supplied silence during idle initialization. It must
             * not be queued again ahead of the first audible stream. */
            int idle_case=!strcmp(argv[1],"audio-idle-queue");
            if(idle_case) { test_tick=11; ps2_w32(0x10000,0);
                ps2_rpc_call(0x81000102,1,0x10000,256,0x20000,256); }
            ps2_w32(0x10000,1);ps2_w32(0x10004,1);ps2_w32(0x10008,1);ps2_w32(0x1000c,0);
            ps2_w32(0x10010,65536);ps2_w32(0x10014,2);ps2_w32(0x10018,4096);
            ps2_rpc_call(0x81000102,1,0x10000,256,0x20000,256);
            if(!strcmp(argv[1],"audio-field-pump")) {
                extern void ps2_ac4_audio_flush(void);
                /* No RPCs: one field produces exactly its fractional sample
                 * count. Repeated same-field flushes must not duplicate PCM. */
                for(int field=0;field<20;field++) {
                    ++test_tick; ps2_ac4_audio_flush(); ps2_ac4_audio_flush();
                    s16 field_out[1602]={0}; u32 n=field_samples();
                    ps2_ac4_audio_mix(field_out,n);
                    for(u32 i=0;i<n;i++) if(field_out[i*2]!=0 || field_out[i*2+1]!=-2) return 94;
                    s16 tail[2]={0};ps2_ac4_audio_mix(tail,1);
                    if(tail[0] || tail[1]) return 95;
                }
                return 0;
            }
            test_tick++;ps2_w32(0x10000,0);
            ps2_rpc_call(0x81000102,1,0x10000,256,0x20000,256);
            s16 out[1602]={0};ps2_ac4_audio_mix(out,field_samples());
            if(out[0]!=0 || out[1]!=-2 || out[1598]!=0 || out[1599]!=-2) return 71;
            test_tick++;ps2_w32(0x10004,0);
            ps2_rpc_call(0x81000102,1,0x10000,256,0x20000,256);
            memset(out,0,sizeof out);ps2_ac4_audio_mix(out,field_samples());
            /* A refill-mask removal must not cut the current ring short. */
            if(out[0]!=0 || out[1]!=-2 || out[1598]!=0 || out[1599]!=-2) return 72;
            return 0;
        } else if(!strcmp(argv[1],"audio-vag-ring-order")) {
            ps2_ac4_module_services(6);ps2_ac4_module_services(5);
            ps2_rpc_call(0x81000102,0,0x10000,256,0x20000,256);
            for(u32 j=0;j<4;++j) if(ps2_r32(0x20014+4*j)!=1) return 80;
            u32 base=ps2_r32(0x20000)+0x180;
            /* Three distinguishable mono slots: old slot zero must not be
             * heard when the guest has filled advertised slot one. */
            for(u32 r=0;r<3;++r) for(u32 i=0;i<65536;i+=16) {
                u8 *b=ps2_iop_ram+base+r*65536+i;b[0]=0;b[1]=0;
                memset(b+2,(r+1)*0x11,14);
            }
            ps2_w32(0x30000,1);ps2_w32(0x30004,2);ps2_w8(0x30012,24);
            ps2_w32(0x30014,1);ps2_w16(0x30030,0x3fff);ps2_w16(0x30032,0x3fff);
            ps2_rpc_call(0x81000101,1,0x30000,9216,0x40000,1536);
            ps2_w32(0x10000,1);ps2_w32(0x10004,63);ps2_w32(0x10008,1);ps2_w32(0x1000c,2);
            ps2_w32(0x10010,0x50000);ps2_w32(0x10014,1);ps2_w32(0x10018,4096);
            ps2_rpc_call(0x81000102,1,0x10000,256,0x20000,256);
            if(ps2_r32(0x2002c)!=2) return 81;
            s16 out[1602]={0};
            for(test_tick=1;test_tick<=145;++test_tick) {
                ps2_w32(0x10000,0);ps2_rpc_call(0x81000102,1,0x10000,256,0x20000,256);
                memset(out,0,sizeof out);ps2_ac4_audio_mix(out,field_samples());
                if(test_tick==1 && (out[0]!=8190 || out[1]!=8190)) return 82;
            }
            if(out[1598]!=12286 || out[1599]!=12286 || ps2_r32(0x2002c)!=0) return 83;
            --test_tick;
            ps2_w32(0x10000,1);ps2_w32(0x10008,3);
            ps2_rpc_call(0x81000102,1,0x10000,256,0x20000,256);
            ps2_w32(0x10008,1);
            ps2_rpc_call(0x81000102,1,0x10000,256,0x20000,256);
            ++test_tick;ps2_w32(0x10000,0);
            ps2_rpc_call(0x81000102,1,0x10000,256,0x20000,256);
            memset(out,0,sizeof out);ps2_ac4_audio_mix(out,field_samples());
            return out[0]!=4094 || out[1]!=4094;
        } else if(!strcmp(argv[1],"audio-sound-esa")) {
            ps2_ac4_module_services(5);
            ps2_rpc_call(0x81000101,0,0x10000,9216,0x20000,1536);
            ps2_w32(0x10000,1);ps2_w32(0x10004,6);ps2_w32(0x10008,1);ps2_w32(0x1000c,2);
            ps2_rpc_call(0x81000101,1,0x10000,9216,0x20000,1536);
            if(ps2_r32(0x20008)!=0x1dfff0) return 73;
            ps2_w32(0x10008,0);ps2_w32(0x1000c,3);
            ps2_rpc_call(0x81000101,1,0x10000,9216,0x20000,1536);
            return ps2_r32(0x20008)!=0x1db7c0;
        } else if(!strcmp(argv[1],"audio-pcm-ring") || !strcmp(argv[1],"audio-pcm-transfer") || !strcmp(argv[1],"audio-reset")) {
            ps2_ac4_module_services(5);
            u32 src=ps2_ac4_iop_alloc(4096);
            for(u32 i=0;i<4096;i+=2) {
                s16 value=(i/512)%2?-12000:8000;
                memcpy(ps2_iop_ram+src+i,&value,2);
            }
            ps2_w32(0x10000,2);ps2_w32(0x10004,13);ps2_w16(0x1000a,0x4000);ps2_w16(0x1000c,0x4000);
            ps2_w32(0x10010,10);ps2_w32(0x10014,src);ps2_w32(0x10018,0);ps2_w32(0x1001c,4096);
            ps2_rpc_call(0x81000101,1,0x10000,9216,0x20000,1536);
            test_tick=1;
            if(!strcmp(argv[1],"audio-pcm-transfer")) {
                /* DMA replaces the ring before the next sound RPC. Already
                 * elapsed playback must still contain the original samples. */
                for(u32 i=0;i<4096;++i) ps2_w8(0x30000+i,0);
                ps2_sif_ac4_dma(0x30000,src,4096,0);
            }
            ps2_w32(0x10000,0);
            ps2_rpc_call(0x81000101,1,0x10000,9216,0x20000,1536);
            s16 out[1602]={0};ps2_ac4_audio_mix(out,field_samples());
            for(int i=0;i<800;++i) if(out[2*i]!=3998 || out[2*i+1]!=-6000) return 74;
            if(ps2_r32(0x2000c)!=1) return 75;
            test_tick=2;ps2_rpc_call(0x81000101,1,0x10000,9216,0x20000,1536);
            if(ps2_r32(0x2000c)!=1) return 76;
            if(!strcmp(argv[1],"audio-reset")) {
                /* Reset while PCM is queued: old IOP data must not leak. */
                ps2_sif_hle_init();ps2_ac4_module_services(5);
                test_tick=3;ps2_rpc_call(0x81000101,1,0x10000,9216,0x20000,1536);
                memset(out,0,sizeof out);ps2_ac4_audio_mix(out,field_samples());
                for(int i=0;i<1600;++i) if(out[i]) return 77;
                if(ps2_r32(0x2000c)) return 78;
            }
            return 0;
        } else if(!strcmp(argv[1],"audio-sound-init")) {
            ps2_ac4_module_services(5);
            if(ps2_rpc_call(0x81000101,0,0x10000,9216,0x20000,1536)) return 40;
            return ps2_r32(0x20000)!=2 || ps2_r32(0x20004)!=10 || ps2_r32(0x20010)!=15;

        } else if(!strcmp(argv[1],"audio-sound-commands")) {
            ps2_ac4_module_services(5);
            ps2_w32(0x10000,2);ps2_w32(0x10004,10);ps2_w32(0x10014,57);
            ps2_w32(0x10018,14);ps2_w32(0x1001c,1);ps2_w32(0x20600,0xabcdef01);
            ps2_rpc_call(0x81000101,1,0x10000,9216,0x20000,1536);
            return ps2_r32(0x20000)!=2 || ps2_r32(0x20008)!=57 || ps2_r32(0x20014)!=1 || ps2_r32(0x20600)!=0xabcdef01;
        } else if((!strcmp(argv[1],"audio-vag-stream") || !strcmp(argv[1],"audio-vag-alternate") || !strcmp(argv[1],"audio-vag-refill"))) {
            ps2_ac4_module_services(6);
            ps2_rpc_call(0x81000102,0,0x10000,256,0x20000,256);
            ps2_w32(0x10000,1);ps2_w32(0x10004,63);ps2_w32(0x10008,!strcmp(argv[1],"audio-vag-alternate")?2:1);
            u32 expected_status=!strcmp(argv[1],"audio-vag-alternate")?0x40:0x20;
            ps2_w32(0x10010,!strcmp(argv[1],"audio-vag-refill")?1000000:96000);ps2_w32(0x10014,2);ps2_w32(0x10018,4096);
            ps2_rpc_call(0x81000102,1,0x10000,256,0x20000,256);
            if(ps2_r32(0x20014)!=expected_status || ps2_r32(0x20004)) return 50;
            if(!strcmp(argv[1],"audio-vag-refill")) {
                ps2_w32(0x10000,0);
                for(test_tick=1;test_tick<=600;++test_tick) {
                    ps2_w32(0x10004,test_tick%150==70 || test_tick%150==71?62:63);
                    ps2_rpc_call(0x81000102,1,0x10000,256,0x20000,256);
                    u32 expected=(u32)((test_tick*4004/5)*16/28);
                    if(ps2_r32(0x20004)!=expected) return 85;
                    s16 out[1602]={0};ps2_ac4_audio_mix(out,field_samples());
                }
                return 0;
            }
            ps2_w32(0x10000,0);test_tick=60;
            ps2_rpc_call(0x81000102,1,0x10000,256,0x20000,256);
            if(ps2_r32(0x20004)!=27456 || ps2_r32(0x20014)!=expected_status) return 51;
            ps2_w32(0x10004,0);test_tick=120;ps2_rpc_call(0x81000102,1,0x10000,256,0x20000,256);
            if(ps2_r32(0x20004)!=54912) return 52;
            test_tick=180;ps2_rpc_call(0x81000102,1,0x10000,256,0x20000,256);
            if(ps2_r32(0x20004)!=65536) return 86;
            test_tick=240;ps2_rpc_call(0x81000102,1,0x10000,256,0x20000,256);
            if(ps2_r32(0x20004)!=65536) return 87;
            ps2_w32(0x10004,63);ps2_rpc_call(0x81000102,1,0x10000,256,0x20000,256);
            test_tick=300;ps2_rpc_call(0x81000102,1,0x10000,256,0x20000,256);
            if(ps2_r32(0x20004)!=92992) return 88;
            test_tick=360;ps2_rpc_call(0x81000102,1,0x10000,256,0x20000,256);
            return ps2_r32(0x20004)!=96000 || ps2_r32(0x20014)!=1;
        } else if(!strcmp(argv[1],"audio-vag-init")) {
            ps2_ac4_module_services(6);
            if(ps2_rpc_call(0x81000102,0,0x10000,256,0x20000,256)) return 41;
            u32 addr=ps2_r32(0x20000);
            if(addr<0x40000 || addr+0xc0000>PS2_IOP_RAM_SIZE) return 42;
            for(u32 i=0;i<4;++i) if(ps2_r32(0x20024+i*4)!=1) return 43;
            return 0;
        } else return 94;
    } else if (!strcmp(argv[1],"iop-dma-copy")) {
        ps2_mem_init();
        for(u32 i=0;i<512;++i) { ps2_w8(0x10000+i,(u8)(i*37));ps2_w8(0x40180+i,0xa5); }
        if(!ps2_sif_ac4_dma(0x80010000,0x40180,512,0)) return 84;
        for(u32 i=0;i<512;++i) if(ps2_iop_ram[0x40180+i]!=(u8)(i*37) || ps2_r8(0x40180+i)!=0xa5) return 85;
        if(!ps2_sif_ac4_dma(0x10000,PS2_IOP_RAM_BASE+0x60000,512,0)) return 86;
        return memcmp(ps2_iop_ram+0x40180,ps2_iop_ram+0x60000,512)!=0;
    } else if (!strcmp(argv[1],"iop-dma-bounds")) {
        ps2_mem_init();ps2_sif_ac4_dma(0x10000,PS2_IOP_RAM_SIZE-16,32,0);return 87;
    } else if (!strncmp(argv[1],"module-",7)) {
        ps2_mem_init(); ps2_sif_hle_init(); ps2_ac4_base_services_register();
        if (!strcmp(argv[1],"module-version")) {
            ps2_w32(0x4004,0x12345678);
            if (ps2_rpc_call(0x80000006,255,0,0,0x4000,4)) return 31;
            return ps2_r32(0x4000)!=0x30303332 || ps2_r32(0x4004)!=0x12345678;
        }
        const char *path="cdrom0:\\IRX\\SIO2MAN.IRX;1";
        if (!strcmp(argv[1],"module-unimplemented")) path="UNKNOWN.IRX";
        for (u32 i=0;i<strlen(path);++i) ps2_w8(0x3008+i,(u8)path[i]);
        if (!strcmp(argv[1],"module-bad-path")) for(u32 i=0;i<252;++i) ps2_w8(0x3008+i,'X');
        if (!strcmp(argv[1],"module-bad-args")) ps2_w32(0x3000,253);
        ps2_rpc_call(0x80000006,0,0x3000,512,0x4000,8);
        if (!strcmp(argv[1],"module-load")) return ps2_r32(0x4000)!=3 || ps2_r32(0x4004)!=0;
        return 99;
    } else if (!strncmp(argv[1], "iop-", 4)) {
        ps2_mem_init(); ps2_sif_init(); ps2_sif_hle_init(); ps2_cdvd_rpc_register();
        reset_packet();
        ctx.r[4].uw[0]=0x1000; ctx.r[5].uw[0]=0x80000592;
        hle_sceSifBindRpc(&ctx);
        if (!strcmp(argv[1],"iop-missing-image")) image_present=0;
        if (!strcmp(argv[1],"iop-wrong-image")) ps2_w8(0x5018,'X');
        if (!strcmp(argv[1],"iop-bad-length")) ps2_w32(0x5010,81);
        if (!strcmp(argv[1],"iop-unknown-command")) ps2_w32(0x5008,0x80000099);
        if (!strcmp(argv[1],"iop-flags")) {
            ps2_sif_set_reg(4,0x40000);
            if (ps2_sif_get_reg(4)!=0x30000 || ps2_sif_reg_read(0x1000f230)!=0x30000) return 20;
            ps2_sif_set_reg(0x80000004,0x123);
            if (ps2_sif_get_reg(4)!=0x30000 || ps2_sif_get_reg(0x80000004)!=0x123) return 21;
            return 0;
        }
        ps2_w32(PS2_IOP_RAM_BASE+0x40000,0x1234);
        ps2_sif_set_reg(4,0x40000);
        if (ps2_sif_ac4_dma(0x5000,PS2_IOP_RAM_BASE,104,0x44)!=1) return 22;
        if (ps2_rpc_call(0x80000592,0,0x3000,4,0x4000,16)!=-1) return 23;
        ps2_sif_set_reg(4,0x10000); ps2_sif_set_reg(4,0x20000);
        ps2_sif_set_reg(0x80000000,0);
        if (ps2_sif_get_reg(4)!=0x70000 || ps2_r32(PS2_IOP_RAM_BASE+0x40000)) return 24;
        if (ps2_sif_get_reg(0x80000000)!=PS2_IOP_RAM_BASE) return 25;
        if (!strcmp(argv[1],"iop-stale-bind")) {
            ctx.r[5].uw[0]=0; ctx.r[7].uw[0]=0x3000; ctx.r[8].uw[0]=4;
            ctx.r[9].uw[0]=0x4000; ctx.r[10].uw[0]=16; ctx.r[29].uw[0]=0x2000;
            hle_sceSifCallRpc(&ctx); return 99;
        }
        if (!strcmp(argv[1],"iop-rebind")) {
            ctx.r[4].uw[0]=0x1000; ctx.r[5].uw[0]=0x80000592;
            hle_sceSifBindRpc(&ctx);
            ctx.r[5].uw[0]=0; ctx.r[7].uw[0]=0x3000; ctx.r[8].uw[0]=4;
            ctx.r[9].uw[0]=0x4000; ctx.r[10].uw[0]=16; ctx.r[29].uw[0]=0x2000;
            hle_sceSifCallRpc(&ctx);
            return ctx.r[2].uw[0] || ps2_r32(0x4000)!=1;
        }
        if (!strcmp(argv[1],"iop-heap")) {
            ps2_w32(0x3000,257); ps2_rpc_call(0x80000003,1,0x3000,4,0x3000,4);
            u32 first=ps2_r32(0x3000);
            ps2_w32(0x3000,256); ps2_rpc_call(0x80000003,1,0x3000,4,0x3000,4);
            u32 second=ps2_r32(0x3000);
            if (first<0x40000 || second<first+512 || second>=PS2_IOP_RAM_SIZE) return 26;
            ps2_w32(0x3000,first); ps2_rpc_call(0x80000003,2,0x3000,4,0x3000,4);
            if (ps2_r32(0x3000)) return 27;
            ps2_w32(0x3000,first); ps2_rpc_call(0x80000003,2,0x3000,4,0x3000,4);
            if (ps2_r32(0x3000)!=0xffffffff) return 28;
            ps2_w32(0x3000,257); ps2_rpc_call(0x80000003,1,0x3000,4,0x3000,4);
            if (ps2_r32(0x3000)!=first) return 29;
        } else if (strcmp(argv[1],"iop-reset")) return 93;
        if (ps2_rpc_call(0x80000592,0,0x3004,4,0x4000,16) || ps2_r32(0x4000)!=1) return 30;
        return 0;
    } else if (!strncmp(argv[1], "cd-", 3)) {
        ps2_mem_init(); ps2_cdvd_rpc_register();
        u32 send=0x3000, recv=0x4000;
        ps2_w32(recv+16, 0xDEADBEEF);
        if (!strcmp(argv[1],"cd-toc")) {
            ps2_w32(send,0x6000); ps2_w32(0x6810,0x12345678);
            ps2_rpc_call(0x80000595,4,send,12,recv,8);
            if(ps2_r32(recv)!=1 || ps2_r32(recv+4)!=1 || ps2_r8(0x600e)!=1 || ps2_r32(0x6810)!=0x12345678) return 32;
        } else if (!strcmp(argv[1],"cd-read")) {
            ps2_w32(send,123);ps2_w32(send+4,2);ps2_w32(send+8,0x6001);
            ps2_w32(send+16,0x5000);ps2_w32(send+20,0x5100);
            ps2_w8(0x6000,0xab);ps2_w8(0x7001,0xcd);ps2_w32(0x5000,42);
            if(ps2_rpc_call(0x80000595,1,send,24,0,0)) return 33;
            for(u32 i=0;i<4096;++i) if(ps2_r8(0x6001+i)!=(u8)i) return 34;
            if(ps2_r8(0x6000)!=0xab || ps2_r8(0x7001)!=0xcd || ps2_r32(0x5000) || ps2_r32(0x5100)!=4096) return 35;
        } else if (!strcmp(argv[1],"cd-read-iop") || !strcmp(argv[1],"cd-read-iop-bounds")) {
            u32 dst=!strcmp(argv[1],"cd-read-iop")?0x6001:PS2_IOP_RAM_SIZE-2048;
            ps2_w32(send,123);ps2_w32(send+4,2);ps2_w32(send+8,dst);
            ps2_w32(send+20,0x5100);ps2_w32(dst,0xdeadbeef);
            ps2_w8(PS2_IOP_RAM_BASE+0x6000,0xab);ps2_w8(PS2_IOP_RAM_BASE+0x7001,0xcd);
            if(ps2_rpc_call(0x80000595,13,send,24,0,0)) return 44;
            for(u32 i=0;i<4096;++i) if(ps2_r8(PS2_IOP_RAM_BASE+dst+i)!=(u8)i) return 45;
            if(ps2_r32(dst)!=0xdeadbeef || ps2_r32(0x5100)!=4096 || ps2_r8(PS2_IOP_RAM_BASE+0x6000)!=0xab || ps2_r8(PS2_IOP_RAM_BASE+0x7001)!=0xcd) return 46;
        } else if (!strcmp(argv[1],"cd-search")) {
            const char *name="TEST.BIN";
            for(u32 i=0;i<9;++i) ps2_w8(send+36+i,name[i]);
            ps2_w32(send+292,0x5000);ps2_w32(0x5024,0x12345678);
            ps2_rpc_call(0x80000597,0,send,296,recv,4);
            if(ps2_r32(recv)!=1 || ps2_r32(0x5000)!=123 || ps2_r32(0x5004)!=4096 || ps2_r32(0x5024)!=0x12345678) return 36;
        } else if (!strcmp(argv[1], "cd-init")) {
            if (ps2_rpc_call(0x80000592,0,send,4,recv,16)) return 10;
            if (ps2_r32(recv)!=1 || ps2_r32(recv+4) || ps2_r32(recv+8) || ps2_r32(recv+12) || ps2_r32(recv+16)!=0xDEADBEEF) return 11;
        } else if (!strcmp(argv[1], "cd-no-disc")) {
            disc_ready=0;
            ps2_rpc_call(0x80000592,0,send,4,recv,16);
            if (ps2_r32(recv)!=0) return 12;
            ps2_rpc_call(0x80000593,3,0,0,recv,4);
            if (ps2_r32(recv)!=0) return 13;
            ps2_rpc_call(0x8000059A,0,send,4,recv,4);
            if (ps2_r32(recv)!=6) return 14;
        } else if (!strcmp(argv[1], "cd-dvd")) {
            ps2_rpc_call(0x80000593,3,0,0,recv,4);
            if (ps2_r32(recv)!=0x14) return 15;
            ps2_w32(send,2); ps2_rpc_call(0x80000593,0x22,send,4,recv,4);
            if (ps2_r32(recv)!=1) return 16;
            ps2_w32(send,0); ps2_rpc_call(0x8000059A,0,send,4,recv,4);
            if (ps2_r32(recv)!=2) return 17;
        } else if (!strcmp(argv[1], "cd-unsupported")) {
            ps2_rpc_call(0x80000593,99,send,4,recv,4); return 99;
        } else if (!strcmp(argv[1], "cd-short-reply")) {
            ps2_rpc_call(0x80000592,0,send,4,recv,4); return 99;
        } else if (!strcmp(argv[1], "cd-unregistered")) {
            if (ps2_rpc_call(0x80000598,0,send,4,recv,4)!=-1) return 18;
        } else return 92;
        return 0;
    } else return 91;
    return 99; /* a rejected operation must never return */
}
''')
exe=out/'strict.exe'
cmd=[str(prefix/'bin/gcc.exe'),'-std=c23','-O2','-DPS2_GAME_AC4=1','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-I'+str(ROOT/'runtime/include'),str(source),str(ROOT/'runtime/src/ps2_core.c'),str(ROOT/'runtime/src/ps2_hle_sif.c'),str(ROOT/'runtime/src/ps2_hle_cdvd.c'),str(ROOT/'runtime/src/ps2_sif.c'),str(ROOT/'runtime/src/ps2_ac4_iop.c'),str(ROOT/'runtime/src/ps2_spu2.c'),'-lSDL3','-lm','-o',str(exe)]
subprocess.run(cmd,env=env,check=True)
results=[]
for case,message in [('target','indirect branch to 00123450'),('bind','AC4_UNSUPPORTED_RPC_BIND sid=12345678'),('call','AC4_UNSUPPORTED_RPC_CALL sid=12345678 fno=7'),('cd-unsupported','AC4_UNSUPPORTED_CD_SCMD'),('cd-short-reply','AC4_UNSUPPORTED_CD_INIT'),('cd-init',''),('cd-no-disc',''),('cd-dvd',''),('cd-unregistered',''),('audio-sound-pcm',''),('audio-sound-reset',''),('audio-vag-pcm',''),('audio-idle-queue',''),('audio-field-pump',''),('audio-vag-ring-order',''),('audio-sound-esa',''),('audio-pcm-ring',''),('audio-pcm-transfer',''),('audio-reset',''),('audio-sound-init',''),('audio-sound-commands',''),('pad-disconnected',''),('pad-connected',''),('usb-disconnected',''),('audio-vag-init',''),('audio-vag-stream',''),('audio-vag-refill',''),('audio-vag-alternate',''),('module-version',''),('module-unimplemented','AC4_UNIMPLEMENTED_IOP_MODULE path=UNKNOWN.IRX'),('module-load',''),('cd-toc',''),('cd-read',''),('cd-read-iop',''),('cd-read-iop-bounds','AC4_CD_INVALID_IOP_BUFFER'),('cd-search',''),('module-bad-path','AC4_INVALID_MODULE_PATH'),('module-bad-args','AC4_INVALID_MODULE_ARGS'),('iop-rebind',''),('iop-reset',''),('iop-dma-copy',''),('iop-dma-bounds','AC4_INVALID_IOP_DMA'),('iop-flags',''),('iop-heap',''),('iop-stale-bind','AC4_STALE_RPC_BIND'),('iop-missing-image','AC4_IOP_IMAGE_MISSING'),('iop-wrong-image','AC4_UNSUPPORTED_IOP_IMAGE'),('iop-bad-length','AC4_INVALID_IOP_RESET_LENGTH'),('iop-unknown-command','AC4_UNSUPPORTED_SIF_COMMAND')]:
    p=subprocess.run([str(exe),case],capture_output=True,text=True,env=env,timeout=10)
    passed=p.returncode==(1 if message else 0) and message in p.stderr
    results.append(dict(case=case,passed=passed,exit_code=p.returncode,stderr=p.stderr))
(out/'results.json').write_text(json.dumps(results,indent=2))
print(json.dumps(results,indent=2))
raise SystemExit(int(not all(x['passed'] for x in results)))
