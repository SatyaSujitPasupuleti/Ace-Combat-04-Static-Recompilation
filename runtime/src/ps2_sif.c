#include "ps2_runtime.h"
#include "ps2_hle.h"
#include "ps2_vfs.h"
#include <stdio.h>
#include <string.h>

#define SIF_MSCOM  0x1000F200u
#define SIF_SMCOM  0x1000F210u
#define SIF_MSFLG  0x1000F220u
#define SIF_SMFLG  0x1000F230u
#define SIF_CTRL   0x1000F240u
#define SIF_BD6    0x1000F260u

#define SIF_STAT_SIFINIT   0x10000u
#define SIF_STAT_CMDINIT   0x20000u
#define SIF_STAT_BOOTEND   0x40000u

static u32 sif_mscom, sif_smcom, sif_msflg, sif_smflg, sif_ctrl, sif_bd6;
static u32 sif_regs[32];
static u64 sif_rpc_calls;
static u32 ac4_sysregs[32];
static int ac4_reset_pending;

/* AC4 uses the original 104-byte SIF reset packet. This is a native
 * replacement lifecycle, not execution of the IOP image or its modules. */
int ps2_sif_ac4_dma(u32 src, u32 dst, u32 size, u32 attr) {
    if (!PS2_GAME_AC4) return 0;
    if (!(attr & 0x40u)) {
        /* SifSetDma descriptors address the remote IOP, not the EE's RAM
         * at the same numeric address. Movie PCM uses unaliased IOP dst. */
        u32 remote=dst & 0x1FFFFFFFu;
        if (remote>=PS2_IOP_RAM_BASE && remote<PS2_IOP_RAM_BASE+PS2_IOP_RAM_SIZE)
            remote-=PS2_IOP_RAM_BASE;
        if (remote>=PS2_IOP_RAM_SIZE || size>PS2_IOP_RAM_SIZE-remote)
            ps2_fatal("AC4_INVALID_IOP_DMA dst=%08X size=%u",dst,size);
        void ps2_ac4_audio_flush(void);
        ps2_ac4_audio_flush();
        if (getenv("PS2_AC4_MOVIE_REFILL_TRACE")) {
            extern void ps2_ac4_movie_refill_trace(u32,u32,u32);
            ps2_ac4_movie_refill_trace(src,remote,size);
        }
        for (u32 i=0;i<size;++i) ps2_iop_ram[remote+i]=ps2_r8(src+i);
        return 1;
    }
    u32 addr = src & 0x1fffffffu;
    if (size < 16 || size > 128 || addr >= PS2_RAM_SIZE || size > PS2_RAM_SIZE - addr)
        ps2_fatal("AC4_INVALID_SIF_COMMAND size=%u src=%08X", size, src);
    u32 cid = ps2_r32(src + 8);
    if (cid != 0x80000003u)
        ps2_fatal("AC4_UNSUPPORTED_SIF_COMMAND cid=%08X size=%u", cid, size);
    if (size != 104 || (ps2_r32(src) & 255u) != size || ps2_r32(src + 20) != 0)
        ps2_fatal("AC4_INVALID_IOP_RESET size=%u mode=%u", size, ps2_r32(src + 20));
    u32 len = ps2_r32(src + 16);
    if (len > 80) ps2_fatal("AC4_INVALID_IOP_RESET_LENGTH %u", len);
    char args[81];
    for (u32 i=0; i<len; ++i) args[i]=(char)ps2_r8(src+24+i);
    args[len]=0;
    static const char supported[] = "rom0:UDNL cdrom0:\\IOPRP23.IMG;1";
    if (len != sizeof(supported)-1 || memcmp(args,supported,len))
        ps2_fatal("AC4_UNSUPPORTED_IOP_IMAGE args=%s", args);
    if (!ps2_vfs_find("IOPRP23.IMG"))
        ps2_fatal("AC4_IOP_IMAGE_MISSING IOPRP23.IMG");
    if (ac4_reset_pending) ps2_fatal("AC4_IOP_RESET_ALREADY_PENDING");
    ps2_sif_hle_init(); /* discards registered services, heap and module IDs */
    ps2_cdvd_init();
    memset(ps2_iop_ram, 0, PS2_IOP_RAM_SIZE);
    sif_smflg = 0;
    memset(ac4_sysregs, 0, sizeof(ac4_sysregs));
    ac4_reset_pending = 1;
    ps2_log("ac4 iop: reset accepted: %s (native base services)",args);
    return 1;
}

void ps2_ac4_base_services_register(void);

static void ac4_finish_reset(void) {
    /* A SIF status observation services the pending native reset. Guest
     * cleanup writes between SetDma and Sync cannot erase completion. */
    if (!ac4_reset_pending) return;
    ps2_cdvd_rpc_register();
    ps2_ac4_base_services_register();
    ac4_sysregs[0] = PS2_IOP_RAM_BASE;
    sif_smflg = SIF_STAT_SIFINIT | SIF_STAT_CMDINIT | SIF_STAT_BOOTEND;
    ac4_reset_pending = 0;
    ps2_log("ac4 iop: reset complete; CDVD/heap available, external modules unavailable");
}

void ps2_sif_init(void) {
    sif_smflg = SIF_STAT_SIFINIT | SIF_STAT_CMDINIT | SIF_STAT_BOOTEND;
    sif_msflg = SIF_STAT_SIFINIT | SIF_STAT_CMDINIT | SIF_STAT_BOOTEND;
    sif_ctrl = 0xF0000102u;
    sif_bd6 = 0;
    ac4_reset_pending = 0;
    memset(ac4_sysregs, 0, sizeof(ac4_sysregs));
    ac4_sysregs[0] = PS2_IOP_RAM_BASE;
    memset(sif_regs, 0, sizeof(sif_regs));
    sif_regs[1] = 0x00000000u;
    sif_regs[2] = PS2_IOP_RAM_BASE;
    sif_regs[3] = SIF_STAT_SIFINIT | SIF_STAT_CMDINIT | SIF_STAT_BOOTEND;
    sif_regs[4] = SIF_STAT_SIFINIT | SIF_STAT_CMDINIT | SIF_STAT_BOOTEND;
}

u32 ps2_sif_reg_read(u32 addr) {
    if (PS2_GAME_AC4 && addr == SIF_SMFLG) ac4_finish_reset();
    switch (addr) {
    case SIF_MSCOM: return sif_mscom;
    case SIF_SMCOM: return sif_smcom;
    case SIF_MSFLG: return sif_msflg;
    case SIF_SMFLG: return sif_smflg;
    case SIF_CTRL:  return sif_ctrl;
    case SIF_BD6:   return sif_bd6;
    default: return 0;
    }
}

void ps2_sif_reg_write(u32 addr, u32 val) {
    switch (addr) {
    case SIF_MSCOM: sif_mscom = val; break;
    case SIF_SMCOM: sif_smcom = val; break;
    case SIF_MSFLG: sif_msflg |= val; break;
    case SIF_SMFLG: sif_smflg &= ~val; break;
    case SIF_CTRL:  sif_ctrl = (sif_ctrl & ~val) | 0xF0000002u; break;
    case SIF_BD6:   sif_bd6 = val; break;
    default: break;
    }
}

void ps2_sif_set_reg(u32 reg, u32 val) {
    if (PS2_GAME_AC4) {
        if (reg & 0x80000000u) { ac4_sysregs[reg & 31] = val; return; }
        switch (reg) {
        case 1: sif_mscom=val; return;
        case 2: sif_smcom=val; return;
        case 3: sif_msflg |= val; return;
        case 4: sif_smflg &= ~val; return;
        default: ps2_fatal("AC4_UNSUPPORTED_SIF_SETREG %08X",reg);
        }
    }
    sif_regs[reg & 31] = val;
}

u32 ps2_sif_get_reg(u32 reg) {
    if (PS2_GAME_AC4) {
        if (reg & 0x80000000u) return ac4_sysregs[reg & 31];
        switch (reg) {
        case 1: return sif_mscom;
        case 2: return sif_smcom;
        case 3: return sif_msflg;
        case 4: ac4_finish_reset(); return sif_smflg;
        default: ps2_fatal("AC4_UNSUPPORTED_SIF_GETREG %08X",reg);
        }
    }
    return sif_regs[reg & 31];
}

void ps2_sif_stats(u64 *rpc_calls) { *rpc_calls = sif_rpc_calls; }
