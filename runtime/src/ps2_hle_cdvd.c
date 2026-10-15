#include <SDL3/SDL.h>
#include "ps2_runtime.h"
#include "ps2_hle.h"
#include "ps2_vfs.h"
#include <stdio.h>
#include <string.h>

#define SCECdComplete       0x02
#define SCECdNotReady       0x06
#define SCECdStatStop       0x00
#define SCECdStatSpin       0x02
#define SCECdStatPause      0x0a
#define SCECdErNO           0x00
#define SCECdErREAD         0x30
#define SCECdErNODISC       0x12
#define SCECdPS2DVD         0x14
#define SCECdCD             1
#define SCECdDVD            2
#define SCECdFuncRead       1
#define SCECdFuncGetToc     3
#define SCECdFuncSeek       4

#define CDVD_SECTOR 2048

static u32 cd_callback;
static u32 cd_poff_callback, cd_poff_arg;
static int cd_error = SCECdErNO;
static int cd_media = SCECdDVD;
static u32 cd_read_pos;
static u64 cd_reads, cd_sectors_read, cd_searches, cd_search_misses;
static int cd_inited;

static void raise_callback(ps2_ctx *ctx, int reason) {
    if (!cd_callback) return;
    {
        ps2_ctx sub = *ctx;
        sub.r[4].sd[0] = reason;
        ps2_dispatch(&sub, cd_callback);
    }
}

void ps2_cdvd_init(void) {
    cd_callback = cd_poff_callback = cd_poff_arg = 0;
    cd_error = SCECdErNO;
    cd_media = SCECdDVD;
    cd_read_pos = 0;
    cd_reads = cd_sectors_read = cd_searches = cd_search_misses = 0;
    cd_inited = 0;
}

int ps2_cdvd_sync_pending(void) { return 0; }

void hle_sceCdInit(ps2_ctx *ctx) {
    if (!cd_inited) {
        cd_inited = 1;
        ps2_log("cdvd: init mode=%u, %u files on disc",
                ps2_arg(ctx, 0), ps2_vfs_file_count());
    }
    HRET(1);
}

void hle_sceCdInitEeCB(ps2_ctx *ctx) { HRET(1); }

void hle_sceCdCallback(ps2_ctx *ctx) {
    u32 prev = cd_callback;
    cd_callback = ps2_arg(ctx, 0);
    HRET(prev);
}

void hle_sceCdPOffCallback(ps2_ctx *ctx) {
    u32 prev = cd_poff_callback;
    cd_poff_callback = ps2_arg(ctx, 0);
    cd_poff_arg = ps2_arg(ctx, 1);
    HRET(prev);
}

static void fill_cdlfile(u32 fp, const ps2_disc_file *f) {
    const char *base = f->name, *p;
    char nm[16];
    u32 i;
    for (p = f->name; *p; p++)
        if (*p == '/') base = p + 1;
    memset(nm, 0, sizeof(nm));
    for (i = 0; i < 15 && base[i]; i++) nm[i] = base[i];
    ps2_w32(fp + 0, f->lsn);
    ps2_w32(fp + 4, f->size);
    for (i = 0; i < 16; i++) ps2_w8(fp + 8 + i, (u8)nm[i]);
    for (i = 0; i < 8; i++) ps2_w8(fp + 24 + i, f->date[i]);
    ps2_w32(fp + 32, f->flag);
}

static void search_file(ps2_ctx *ctx, u32 fp, u32 name_addr) {
    char name[256];
    const ps2_disc_file *f;
    ps2_get_str(name_addr, name, sizeof(name));
    cd_searches++;
    f = ps2_vfs_find(name);
    if (ps2_verbose)
        ps2_log("cdvd: search '%s' -> %s", name,
                f ? "found" : "MISSING");
    if (!f) {
        cd_search_misses++;
        ps2_log("cdvd: '%s' not on disc", name);
        HRET(0);
        return;
    }
    fill_cdlfile(fp, f);
    HRET(1);
}

void hle_sceCdSearchFile(ps2_ctx *ctx) {
    search_file(ctx, ps2_arg(ctx, 0), ps2_arg(ctx, 1));
}

void hle_sceCdLayerSearchFile(ps2_ctx *ctx) {
    if (ps2_arg(ctx, 2) != 0) { HRET(0); return; }
    search_file(ctx, ps2_arg(ctx, 0), ps2_arg(ctx, 1));
}

void hle_sceCdRead(ps2_ctx *ctx) {
    u32 lbn = ps2_arg(ctx, 0);
    u32 sectors = ps2_arg(ctx, 1);
    u32 buf = ps2_arg(ctx, 2);
    int rc;
    if (!ps2_vfs_ready()) {
        cd_error = SCECdErNODISC;
        HRET(0);
        return;
    }
    rc = ps2_vfs_read_sectors_guest(lbn, sectors, buf);
    if (rc < 0) {
        cd_error = SCECdErREAD;
        ps2_log("cdvd: read failed lbn=%u sectors=%u", lbn, sectors);
        HRET(0);
        return;
    }
    if (ps2_verbose || PS2_ENV("PS2_TRACE_MPEG"))
        ps2_log("cdvd: read lbn=%u sectors=%u (%u bytes) -> %08X  "
                "[%08X %08X %08X %08X]",
                lbn, sectors, sectors * 2048u, buf, ps2_r32(buf),
                ps2_r32(buf + 4), ps2_r32(buf + 8), ps2_r32(buf + 12));
    cd_error = SCECdErNO;
    cd_reads++;
    cd_sectors_read += sectors;
    cd_read_pos = lbn + sectors;
    HRET(1);
    raise_callback(ctx, SCECdFuncRead);
}

void hle_sceCdReadIOPm(ps2_ctx *ctx) { hle_sceCdRead(ctx); }

void hle_sceCdSync(ps2_ctx *ctx)  { HRET(0); }
void hle_sceCdSyncS(ps2_ctx *ctx) { HRET(0); }

void hle_sceCdGetError(ps2_ctx *ctx) { HRET(cd_error); }

void hle_sceCdStatus(ps2_ctx *ctx) { HRET(SCECdStatPause); }

void hle_sceCdDiskReady(ps2_ctx *ctx) {
    HRET(ps2_vfs_ready() ? SCECdComplete : SCECdNotReady);
}

void hle_sceCdGetDiskType(ps2_ctx *ctx) { HRET(ps2_vfs_ready() ? SCECdPS2DVD : 0); }

void hle_sceCdMmode(ps2_ctx *ctx) { cd_media = (int)ps2_arg(ctx, 0); HRET(1); }

void hle_sceCdSeek(ps2_ctx *ctx) {
    cd_read_pos = ps2_arg(ctx, 0);
    HRET(1);
    raise_callback(ctx, SCECdFuncSeek);
}

void hle_sceCdGetReadPos(ps2_ctx *ctx) { HRET(cd_read_pos); }

void hle_sceCdBreak(ps2_ctx *ctx) { HRET(1); }

void hle_sceCdGetToc(ps2_ctx *ctx) {
    u32 toc = ps2_arg(ctx, 0);
    for (u32 i = 0; i < 1024; i++) ps2_w8(toc + i, 0);
    ps2_w8(toc + 0, 0x01);
    ps2_w8(toc + 1, 0x01);
    ps2_w8(toc + 2, 0x01);
    HRET(1);
}

void hle_sceCdStandby(ps2_ctx *ctx) { HRET(1); }
void hle_sceCdStop(ps2_ctx *ctx)    { HRET(1); }
void hle_sceCdPause(ps2_ctx *ctx)   { HRET(1); }
void hle_sceCdTrayReq(ps2_ctx *ctx) {
    u32 chk = ps2_arg(ctx, 1);
    if (chk) ps2_w32(chk, 0);
    HRET(1);
}
void hle_sceCdChangeThreadPriority(ps2_ctx *ctx) { HRET(1); }
void hle_sceCdSetEEReadMode(ps2_ctx *ctx) { HRET(ps2_arg(ctx, 0)); }
void hle_sceCdPowerOff(ps2_ctx *ctx) { HRET(1); }

void hle_sceCdReadClock(ps2_ctx *ctx) {
    u32 p = ps2_arg(ctx, 0);
    ps2_w8(p + 0, 0x00);
    ps2_w8(p + 1, 0x00);
    ps2_w8(p + 2, 0x00);
    ps2_w8(p + 3, 0x12);
    ps2_w8(p + 4, 0x00);
    ps2_w8(p + 5, 0x01);
    ps2_w8(p + 6, 0x01);
    ps2_w8(p + 7, 0x05);
    HRET(1);
}

static int cdvd_rpc(ps2_ctx *ctx, u32 fno, u32 send, int ssize,
                    u32 recv, int rsize) {
    (void)send; (void)ssize;
    if (recv && rsize > 0)
        for (int i = 0; i < rsize; i++) ps2_w8(recv + (u32)i, 0);
    if (recv && rsize >= 4) ps2_w32(recv, 1);
    ps2_log("cdvd: RPC fno=%u served generically (%d/%d bytes)",
            fno, ssize, rsize);
    return 0;
}

static int cdvd_scmd_rpc(ps2_ctx *ctx, u32 fno, u32 send, int ssize,
                         u32 recv, int rsize) {
    if (fno != 3u) return cdvd_rpc(ctx, fno, send, ssize, recv, rsize);
    if (!recv || rsize < 4) return -1;
    for (int i = 0; i < rsize; i++) ps2_w8(recv + (u32)i, 0);
    ps2_w32(recv, ps2_vfs_ready() ? SCECdPS2DVD : 0);
    return 0;
}

static int cdvd_ready_rpc(ps2_ctx *ctx, u32 fno, u32 send, int ssize,
                          u32 recv, int rsize) {
    (void)ctx; (void)fno; (void)send; (void)ssize;
    if (recv && rsize > 0) {
        int i;
        for (i = 0; i < rsize; i++) ps2_w8(recv + (u32)i, 0);
    }
    if (recv && rsize >= 4)
        ps2_w32(recv, ps2_vfs_ready() ? SCECdComplete : SCECdNotReady);
    return 0;
}

/* AC4 keeps guest libcdvd; implement only verified wire requests.
 * ABI: ps2sdk ee/rpc/cdvd/src/libcdvd.c, corroborated by AC4 0x0026F858.
 * Zero version fields describe the pre-2.0 response; do not invent IOP versions.
 */
static int ac4_cd_init_rpc(ps2_ctx *ctx, u32 fno, u32 send, int ssize,
                           u32 recv, int rsize) {
    if (fno != 0 || ssize != 4 || rsize != 16 || !send || !recv)
        ps2_fatal("AC4_UNSUPPORTED_CD_INIT fno=%u sizes=%d/%d", fno, ssize, rsize);
    u32 mode = ps2_r32(send);
    if (mode != 0 && mode != 1 && mode != 5)
        ps2_fatal("AC4_UNSUPPORTED_CD_INIT_MODE %u", mode);
    cd_inited = mode != 5 && ps2_vfs_ready();
    ps2_w32(recv, mode == 5 ? 1u : (u32)cd_inited);
    ps2_w32(recv + 4, 0); ps2_w32(recv + 8, 0); ps2_w32(recv + 12, 0);
    ps2_log("ac4 cdvd: init mode=%u ready=%d", mode, cd_inited);
    return 0;
}

static int ac4_cd_scmd_rpc(ps2_ctx *ctx, u32 fno, u32 send, int ssize,
                           u32 recv, int rsize) {
    if (!recv || rsize != 4)
        ps2_fatal("AC4_UNSUPPORTED_CD_SCMD_REPLY fno=%u size=%d", fno, rsize);
    if (fno == 4u && ssize == 0) {
        ps2_w32(recv,(u32)cd_error);
    } else if (fno == 3u && ssize == 0) {
        ps2_w32(recv, ps2_vfs_ready() ? SCECdPS2DVD : 0);
    } else if (fno == 0x22u && send && ssize == 4) {
        u32 mode = ps2_r32(send);
        if (mode != SCECdCD && mode != SCECdDVD)
            ps2_fatal("AC4_UNSUPPORTED_CD_MEDIA %u", mode);
        cd_media = (int)mode;
        ps2_w32(recv, 1);
    } else {
        ps2_fatal("AC4_UNSUPPORTED_CD_SCMD fno=%u sizes=%d/%d", fno, ssize, rsize);
    }
    ps2_log("ac4 cdvd: scmd=%u result=%u", fno, ps2_r32(recv));
    return 0;
}

static int ac4_cd_ready_rpc(ps2_ctx *ctx, u32 fno, u32 send, int ssize,
                            u32 recv, int rsize) {
    if (fno != 0 || !send || ssize != 4 || !recv || rsize != 4 || ps2_r32(send) > 1)
        ps2_fatal("AC4_UNSUPPORTED_CD_READY fno=%u sizes=%d/%d", fno, ssize, rsize);
    ps2_w32(recv, ps2_vfs_ready() ? SCECdComplete : SCECdNotReady);
    return 0;
}

static void ac4_cd_ram(u32 addr,u32 size) {
    u32 p=addr & 0x1fffffffu;
    if (!addr || p>=PS2_RAM_SIZE || size>PS2_RAM_SIZE-p)
        ps2_fatal("AC4_CD_INVALID_BUFFER addr=%08X size=%u",addr,size);
}
static int ac4_cd_ncmd_rpc(ps2_ctx *ctx,u32 fno,u32 send,int ssize,u32 recv,int rsize) {
    (void)ctx;
    if (fno==14 && ssize==0 && recv && rsize==4) { ac4_cd_ram(recv,4); ps2_w32(recv,ps2_vfs_ready()?2:6); return 0; }
    if (fno==4 && send && ssize==12 && recv && rsize==8) {
        ac4_cd_ram(send,12); ac4_cd_ram(recv,8);
        u32 dest=ps2_r32(send); ac4_cd_ram(dest,2064);
        u32 sectors=ps2_vfs_sector_count();
        if (!sectors) return -1;
        for (u32 i=0;i<2064;++i) ps2_w8(dest+i,0);
        /* Single-layer DVD physical-format descriptor; capacity from virtual disc sector layout.
         * Layout corroborated by PCSX2 CDVDdiscReader.cpp. */
        ps2_w8(dest,0x24); ps2_w8(dest+4,0x86); ps2_w8(dest+5,0x72);
        ps2_w8(dest+12,1); ps2_w8(dest+13,2); ps2_w8(dest+14,1); ps2_w8(dest+17,3);
        u32 end=sectors+0x30000u-1;
        for (u32 i=0;i<4;++i) ps2_w8(dest+20+i,(u8)(end>>(24-8*i)));
        ps2_w32(recv,1); ps2_w32(recv+4,1);
        return 0;
    }
    if ((fno==1 || fno==13) && send && ssize==24 && rsize==0) {
        ac4_cd_ram(send,24);
        u32 lsn=ps2_r32(send),n=ps2_r32(send+4),dst=ps2_r32(send+8),mode=ps2_r32(send+12);
        if ((mode>>16)&255u) ps2_fatal("AC4_CD_READ_SECTOR_MODE %08X",mode);
        if (n>PS2_RAM_SIZE/2048) ps2_fatal("AC4_CD_READ_TOO_LARGE %u",n);
        if (fno==13) {
            if(dst>=PS2_IOP_RAM_SIZE || n*2048>PS2_IOP_RAM_SIZE-dst) ps2_fatal("AC4_CD_INVALID_IOP_BUFFER addr=%08X sectors=%u",dst,n);
            void ps2_ac4_audio_flush(void);
            ps2_ac4_audio_flush();
            dst+=PS2_IOP_RAM_BASE;
        } else ac4_cd_ram(dst,n*2048);
        u32 align=ps2_r32(send+16),pos=ps2_r32(send+20);
        if (fno==1 && align) { ac4_cd_ram(align,16); for(u32 i=0;i<16;++i) ps2_w8(align+i,0); }
        if (pos) ac4_cd_ram(pos,4);
        if (getenv("PS2_AC4_MOVIE_LIFETIME_TRACE")) {
            extern u64 ps2_kernel_vblank_count(void);
            ps2_log("movie read: field=%llu lsn=%u sectors=%u dst=%08X",
                (unsigned long long)ps2_kernel_vblank_count(),lsn,n,dst);
        }
        u64 read_start=SDL_GetTicksNS();
    int rc=ps2_vfs_read_sectors_guest(lsn,n,dst);
    if (rc>=0 && fno==1) {
        extern void ac4_movie_audio_capture(u32,u32);
        ac4_movie_audio_capture(dst,n*2048u);
    }
    extern u64 ps2_kernel_vblank_count(void);
    u64 read_ns=SDL_GetTicksNS()-read_start;
    if (read_ns>25000000ull && getenv("PS2_SPEED_LOG")) {
        ps2_log("ac4 spike: field=%llu op=disc-read ms=%.3f lsn=%u sectors=%u",
            (unsigned long long)ps2_kernel_vblank_count(),read_ns/1e6,lsn,n);
    }
        cd_error=rc<0?SCECdErREAD:SCECdErNO;
        cd_reads++; if(rc>=0) cd_sectors_read+=n;
        cd_read_pos=rc<0?0:n*2048;
        if(pos) ps2_w32(pos,cd_read_pos);
        ps2_log("ac4 cdvd: read lsn=%u sectors=%u dest=%08X result=%d",lsn,n,dst,rc);
        return rc<0?-1:0;
    }
    ps2_fatal("AC4_UNSUPPORTED_CD_NCMD fno=%u sizes=%d/%d",fno,ssize,rsize);
    return -1;
}
static int ac4_cd_search_rpc(ps2_ctx *ctx,u32 fno,u32 send,int ssize,u32 recv,int rsize) {
    (void)ctx;
    if(fno || ssize!=296 || rsize!=4) ps2_fatal("AC4_UNSUPPORTED_CD_SEARCH fno=%u sizes=%d/%d",fno,ssize,rsize);
    ac4_cd_ram(send,296); ac4_cd_ram(recv,4);
    char name[256]; unsigned i;
    for(i=0;i<256;++i) { name[i]=(char)ps2_r8(send+36+i); if(!name[i]) break; }
    if(i==256) ps2_fatal("AC4_CD_SEARCH_NAME_UNTERMINATED");
    u32 dst=ps2_r32(send+292); ac4_cd_ram(dst,36);
    const ps2_disc_file *f=ps2_vfs_find(name);
    cd_searches++;
    if(f) {
        for(i=0;i<36;++i) ps2_w8(dst+i,0);
        ps2_w32(dst,f->lsn); ps2_w32(dst+4,f->size);
        const char *base=strrchr(f->name,'/'); base=base?base+1:f->name;
        for(i=0;i<15 && base[i];++i) ps2_w8(dst+8+i,(u8)base[i]);
        for(i=0;i<8;++i) ps2_w8(dst+24+i,f->date[i]);
    } else cd_search_misses++;
    ps2_w32(recv,f?1:0);
    ps2_log("ac4 cdvd: search %s -> %s",name,f?"found":"missing");
    return 0;
}

void ps2_cdvd_rpc_register(void) {
    if (PS2_GAME_AC4) {
        ps2_rpc_register(0x80000595u,ac4_cd_ncmd_rpc,"ac4-cdvd-ncmd");
        ps2_rpc_register(0x80000597u,ac4_cd_search_rpc,"ac4-cdvd-search");
        ps2_rpc_register(0x80000592u, ac4_cd_init_rpc, "ac4-cdvd-init");
        ps2_rpc_register(0x80000593u, ac4_cd_scmd_rpc, "ac4-cdvd-scmd");
        ps2_rpc_register(0x8000059Au, ac4_cd_ready_rpc, "ac4-cdvd-ready");
        return;
    }
    ps2_rpc_register(0x80000592u, cdvd_rpc, "cdvd-scmd");
    ps2_rpc_register(0x80000593u, cdvd_scmd_rpc, "cdvd-scmd2");
    ps2_rpc_register(0x80000595u, cdvd_rpc, "cdvd-ncmd");
    ps2_rpc_register(0x80000597u, cdvd_rpc, "cdvd-srch");
    ps2_rpc_register(0x8000059Au, cdvd_rpc, "cdvd-init");
    ps2_rpc_register(0x8000059Cu, cdvd_ready_rpc, "cdvd-rdy");
}

void ps2_cdvd_report(void) {
    ps2_log("cdvd: %llu reads (%llu sectors, %.1f MB), %llu searches "
            "(%llu misses)",
            (unsigned long long)cd_reads,
            (unsigned long long)cd_sectors_read,
            (double)cd_sectors_read * CDVD_SECTOR / (1024.0 * 1024.0),
            (unsigned long long)cd_searches,
            (unsigned long long)cd_search_misses);
}
