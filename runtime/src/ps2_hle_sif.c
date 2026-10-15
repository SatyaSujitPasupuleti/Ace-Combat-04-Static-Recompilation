#include "ps2_runtime.h"
#include "ps2_hle.h"
#include "ps2_vfs.h"
#include <stdio.h>
#include <string.h>

#define CD_PADDR    0
#define CD_PID      4
#define CD_TID      8
#define CD_MODE    12
#define CD_COMMAND 16
#define CD_BUFF    20
#define CD_GP      24
#define CD_FUNC    28
#define CD_PARA    32
#define CD_SERVE   36

#define SIF_RPCM_NOWAIT 0x01
#define SIF_RPCM_NOWBDC 0x02

#define SIF_RPCE_GETP   1
#define SIF_RPCE_SENDP  2
#define SIF_RPCE_CSEMA  3

#define MAX_SERVICES 32

typedef struct {
    u32 sid;
    ps2_rpc_fn fn;
    const char *name;
    u64 calls;
} rpc_service;

static rpc_service services[MAX_SERVICES];
static unsigned nservices;
static u32 ac4_rpc_epoch;

static u64 bind_count, call_count, unknown_calls;

#define MAX_UNKNOWN 64
static u32 unknown_sids[MAX_UNKNOWN];
static unsigned nunknown;

static int seen_unknown(u32 sid) {
    for (unsigned i = 0; i < nunknown; i++)
        if (unknown_sids[i] == sid) return 1;
    if (nunknown < MAX_UNKNOWN) unknown_sids[nunknown++] = sid;
    return 0;
}

int ps2_rpc_call(u32 sid, u32 fno, u32 send, int ssize, u32 recv, int rsize) {
    unsigned i;
    for (i = 0; i < nservices; i++)
        if (services[i].sid == sid)
            return services[i].fn(NULL, fno, send, ssize, recv, rsize);
    return -1;
}

void ps2_rpc_register(u32 sid, ps2_rpc_fn fn, const char *name) {
    if (nservices >= MAX_SERVICES) {
        ps2_log("rpc: service table full, dropping %08X (%s)", sid, name);
        return;
    }
    services[nservices].sid = sid;
    services[nservices].fn = fn;
    services[nservices].name = name;
    services[nservices].calls = 0;
    nservices++;
}

static rpc_service *find_service(u32 sid) {
    for (unsigned i = 0; i < nservices; i++)
        if (services[i].sid == sid) return &services[i];
    return NULL;
}

void hle_sceSifBindRpc(ps2_ctx *ctx) {
    u32 cd = ps2_arg(ctx, 0);
    u32 sid = ps2_arg(ctx, 1);
    u32 mode = ps2_arg(ctx, 2);
    rpc_service *s = find_service(sid);

    if (PS2_GAME_AC4 && !s)
        ps2_fatal("AC4_UNSUPPORTED_RPC_BIND sid=%08X ra=%08X", sid, ctx->r[31].uw[0]);
    bind_count++;
    if (!s && !seen_unknown(sid))
        ps2_log("rpc: bind to unserviced id %08X -- answering with an empty "
                "server; calls on it will return zero-filled replies", sid);

    ps2_w32(cd + CD_SERVE, sid ? sid : 1u);
    ps2_w32(cd + CD_COMMAND, sid);
    ps2_w32(cd + CD_PADDR, 0);
    ps2_w32(cd + CD_PID, PS2_GAME_AC4 ? ac4_rpc_epoch : 0);
    ps2_w32(cd + CD_TID, (u32)-1);
    ps2_w32(cd + CD_MODE, mode);
    HRET(0);
}

void hle_sceSifCallRpc(ps2_ctx *ctx) {
    u32 cd     = ps2_arg(ctx, 0);
    u32 fno    = ps2_arg(ctx, 1);
    u32 mode   = ps2_arg(ctx, 2);
    u32 send   = ps2_arg(ctx, 3);
    s32 ssize  = (s32)ps2_arg(ctx, 4);
    u32 recv   = ps2_arg(ctx, 5);
    s32 rsize  = (s32)ps2_arg(ctx, 6);
    u32 endfn  = ps2_arg(ctx, 7);
    u32 para   = ps2_arg(ctx, 8);
    u32 sid    = ps2_r32(cd + CD_COMMAND);
    rpc_service *s = find_service(sid);
    int rc;

    if (PS2_GAME_AC4 && !s)
        ps2_fatal("AC4_UNSUPPORTED_RPC_CALL sid=%08X fno=%u ra=%08X", sid, fno, ctx->r[31].uw[0]);
    if (PS2_GAME_AC4 && ps2_r32(cd + CD_PID) != ac4_rpc_epoch)
        ps2_fatal("AC4_STALE_RPC_BIND sid=%08X",sid);
    call_count++;
    { static int tr = -1;
      if (tr < 0) { const char *e = getenv("PS2_TRACE_RPC"); tr = e ? (int)strtol(e, NULL, 0) : 0; }
      if (tr)
          ps2_log("rpc call sid=%08X fno=%u mode=%08X endfn=%08X para=%08X "
                  "ssize=%d rsize=%d", sid, fno, mode, endfn, para, ssize, rsize);
      if (tr > 1 && send && ssize > 0) {
          char line[160];
          int n = 0, w = ssize / 4;
          if (w > 12) w = 12;
          for (int i = 0; i < w && n < (int)sizeof(line) - 10; i++)
              n += snprintf(line + n, sizeof line - (size_t)n, " %08X",
                            ps2_r32(send + (u32)i * 4u));
          line[n] = 0;
          ps2_log("    send%s", line);
      }
    }
    ps2_w32(cd + CD_GP, (u32)ctx->r[28].ud[0]);
    ps2_w32(cd + CD_FUNC, endfn);
    ps2_w32(cd + CD_PARA, para);
    ps2_w32(cd + CD_TID, (u32)-1);
    ps2_w32(cd + CD_MODE, mode);
    ps2_w32(cd + CD_PADDR, 0);

    if (s) {
        s->calls++;
        rc = s->fn(ctx, fno, send, ssize, recv, rsize);
    } else {
        unknown_calls++;
        if (!seen_unknown(sid))
            ps2_log("rpc: call on unserviced id %08X fno=%u ssize=%d rsize=%d",
                    sid, fno, ssize, rsize);
        if (recv && rsize > 0)
            for (s32 i = 0; i < rsize; i++) ps2_w8(recv + (u32)i, 0);
        rc = 0;
    }

    if ((mode & SIF_RPCM_NOWAIT) && endfn) {
        ps2_ctx sub = *ctx;
        sub.r[4].ud[0] = para;
        ps2_dispatch(&sub, endfn);
    }
    HRET(rc);
}

void hle_sceSifCheckStatRpc(ps2_ctx *ctx) {
    HRET(0);
}

void hle_sceSifInitRpc(ps2_ctx *ctx) {
    static int done;
    if (!done) { ps2_log("rpc: sceSifInitRpc -- native RPC layer active"); done = 1; }
    HRET(0);
}

void hle_sceSifExitRpc(ps2_ctx *ctx) { HRET(0); }

void hle_sceSifSetRpcQueue(ps2_ctx *ctx) {
    u32 q = ps2_arg(ctx, 0);
    ps2_w32(q + 0, (u32)ps2_arg(ctx, 1));
    ps2_w32(q + 4, 0);
    ps2_w32(q + 8, 0);
    ps2_w32(q + 12, 0);
    ps2_w32(q + 16, 0);
    ps2_w32(q + 20, 0);
    HRET(0);
}

void hle_sceSifGetOtherData(ps2_ctx *ctx) {
    u32 rd   = ps2_arg(ctx, 0);
    u32 src  = ps2_arg(ctx, 1);
    u32 dest = ps2_arg(ctx, 2);
    s32 size = (s32)ps2_arg(ctx, 3);
    for (s32 i = 0; i < size; i++) ps2_w8(dest + (u32)i, ps2_r8(src + (u32)i));
    ps2_w32(rd + CD_PADDR, 0);
    ps2_w32(rd + CD_TID, (u32)-1);
    HRET(0);
}

void hle_sceSifResetIop(ps2_ctx *ctx)   { HRET(1); }
void hle_sceSifSyncIop(ps2_ctx *ctx)    { HRET(1); }
void hle_sceSifIsAliveIop(ps2_ctx *ctx) { HRET(1); }
void hle_sceSifRebootIop(ps2_ctx *ctx)  { HRET(1); }
void hle_sceSifInitCmd(ps2_ctx *ctx)    { HRET(0); }
void hle_sceSifExitCmd(ps2_ctx *ctx)    { HRET(0); }
void hle_sceSifInitIopHeap(ps2_ctx *ctx){ HRET(0); }

#define IOP_HEAP_START (PS2_IOP_RAM_BASE + 0x40000u)
#define IOP_HEAP_PAGE 256u
#define IOP_HEAP_COUNT ((PS2_IOP_RAM_SIZE - 0x40000u) / IOP_HEAP_PAGE)
static u16 iop_heap[IOP_HEAP_COUNT];

static u32 iop_alloc(u32 mode, u32 size, u32 address) {
    if (!size || size > IOP_HEAP_COUNT * IOP_HEAP_PAGE || mode > 2u) return 0;
    u32 count = (size + IOP_HEAP_PAGE - 1u) / IOP_HEAP_PAGE;
    u32 first = 0, last = IOP_HEAP_COUNT - count;
    if (mode == 2u) {
        if (address < PS2_IOP_RAM_SIZE) address += PS2_IOP_RAM_BASE;
        if (address < IOP_HEAP_START || address >= PS2_IOP_RAM_BASE + PS2_IOP_RAM_SIZE
            || (address & (IOP_HEAP_PAGE - 1u))) return 0;
        first = (address - IOP_HEAP_START) / IOP_HEAP_PAGE;
        if (first > last) return 0;
        last = first;
    }
    for (u32 step = 0; step <= last - first; step++) {
        u32 start = mode == 1u ? last - step : first + step;
        u32 j = 0;
        while (j < count && !iop_heap[start + j]) j++;
        if (j != count) continue;
        iop_heap[start] = (u16)count;
        for (j = 1; j < count; j++) iop_heap[start + j] = 0xffff;
        return IOP_HEAP_START + start * IOP_HEAP_PAGE;
    }
    return 0;
}

static int iop_free(u32 address) {
    if (address < PS2_IOP_RAM_SIZE) address += PS2_IOP_RAM_BASE;
    if (address < IOP_HEAP_START || address >= PS2_IOP_RAM_BASE + PS2_IOP_RAM_SIZE
        || (address & (IOP_HEAP_PAGE - 1u))) return -1;
    u32 start = (address - IOP_HEAP_START) / IOP_HEAP_PAGE;
    u32 count = iop_heap[start];
    if (!count || count == 0xffff) return -1;
    memset(iop_heap + start, 0, count * sizeof(iop_heap[0]));
    return 0;
}

u32 ps2_ac4_iop_alloc(u32 size) {
    u32 result=iop_alloc(0,size,0);
    return result?result-PS2_IOP_RAM_BASE:0;
}

static int ac4_heap_rpc(ps2_ctx *ctx, u32 fno, u32 send, int ssize,
                         u32 recv, int rsize) {
    (void)ctx;
    if (!send || !recv || ssize != 4 || rsize != 4 || (fno != 1 && fno != 2))
        ps2_fatal("AC4_UNSUPPORTED_IOP_HEAP fno=%u sizes=%d/%d",fno,ssize,rsize);
    u32 value=ps2_r32(send), result;
    if (fno == 1) {
        result=iop_alloc(0,value,0);
        if (result) result -= PS2_IOP_RAM_BASE; /* RPC returns an IOP address */
    } else result=(u32)iop_free(value);
    ps2_w32(recv,result);
    return 0;
}

void ps2_ac4_module_services(unsigned module);
static unsigned ac4_modules;
/* Prototype native modules. Loading records dependencies; actual EE RPC
 * operations remain independently checked and may fail as unsupported. */
static int ac4_load_native_module(const char *path) {
    static const struct { const char *path; unsigned dependencies; } modules[] = {
        {"cdrom0:\\IRX\\SIO2MAN.IRX;1",0},
        {"cdrom0:\\IRX\\MCMAN.IRX;1",1},
        {"cdrom0:\\IRX\\MCSERV.IRX;1",2},
        {"cdrom0:\\IRX\\PADMAN.IRX;1",1},
        {"cdrom0:\\IRX\\LIBSD.IRX;1",0},
        {"cdrom0:\\IRX\\SOUND.IRX;1",16},
        {"cdrom0:\\IRX\\VAG.IRX;1",16},
        {"cdrom0:\\IRX\\USBD.IRX;1",0},
        {"cdrom0:\\IRX\\USBJOY.IRX;1",128},
    };
    for (unsigned i=0;i<sizeof(modules)/sizeof(modules[0]);++i) {
        if (strcmp(path,modules[i].path)) continue;
        if ((ac4_modules & modules[i].dependencies)!=modules[i].dependencies)
            ps2_fatal("AC4_MODULE_DEPENDENCY path=%s",path);
        if (!ps2_vfs_find(path)) ps2_fatal("AC4_MODULE_FILE_MISSING path=%s",path);
        if (!(ac4_modules & (1u<<i))) ps2_ac4_module_services(i);
        ac4_modules |= 1u<<i;
        ps2_log("ac4 module: native prototype id=%u path=%s; service coverage incomplete",i+3,path);
        return (int)i+3;
    }
    ps2_fatal("AC4_UNIMPLEMENTED_IOP_MODULE path=%s",path);
    return -1;
}

/* Diagnostic LOADFILE endpoint: never claims that an IRX was executed.
 * AC4 0x0026A9F0 sends a 512-byte packet, path at +8, reply length 8. */
static int ac4_loadfile_rpc(ps2_ctx *ctx, u32 fno, u32 send, int ssize,
                             u32 recv, int rsize) {
    (void)ctx;
    /* AC4 compares these four protocol-tag bytes with "2300" at
     * 0x003FABCC. This identifies the packet ABI, not module support. */
    if (fno == 255 && ssize == 0 && recv && rsize == 4) {
        ps2_w32(recv,0x30303332u);
        return 0;
    }
    u32 address=send & 0x1fffffffu;
    if (fno != 0 || !send || !recv || ssize != 512 || rsize != 8 ||
        address >= PS2_RAM_SIZE || 512u > PS2_RAM_SIZE-address)
        ps2_fatal("AC4_UNSUPPORTED_LOADFILE fno=%u sizes=%d/%d",fno,ssize,rsize);
    u32 arglen=ps2_r32(send);
    if (arglen>252) ps2_fatal("AC4_INVALID_MODULE_ARGS length=%u",arglen);
    char path[252];
    unsigned i;
    for (i=0;i<sizeof(path);++i) {
        path[i]=(char)ps2_r8(send+8+i);
        if (!path[i]) break;
    }
    if (i==sizeof(path) || !i) ps2_fatal("AC4_INVALID_MODULE_PATH");
    if (arglen) ps2_fatal("AC4_UNSUPPORTED_MODULE_ARGS path=%s length=%u",path,arglen);
    int module=ac4_load_native_module(path);
    ps2_w32(recv,(u32)module); ps2_w32(recv+4,0);
    return 0;
}

void ps2_ac4_base_services_register(void) {
    ps2_rpc_register(0x80000003u,ac4_heap_rpc,"ac4-iop-heap");
    ps2_rpc_register(0x80000006u,ac4_loadfile_rpc,"ac4-loadfile-diagnostic");
}

void hle_sceSifAllocIopHeap(ps2_ctx *ctx) {
    HRET(iop_alloc(0, ps2_arg(ctx, 0), 0));
}
void hle_sceSifFreeIopHeap(ps2_ctx *ctx) { HRET(iop_free(ps2_arg(ctx, 0))); }
void hle_sceSifAllocSysMemory(ps2_ctx *ctx) {
    HRET(iop_alloc(ps2_arg(ctx, 0), ps2_arg(ctx, 1), ps2_arg(ctx, 2)));
}
void hle_sceSifFreeSysMemory(ps2_ctx *ctx) { HRET(iop_free(ps2_arg(ctx, 0))); }
void hle_sceSifQueryMemSize(ps2_ctx *ctx) { HRET(PS2_IOP_RAM_SIZE); }
void hle_sceSifQueryMaxFreeMemSize(ps2_ctx *ctx) {
    u32 run = 0, best = 0;
    for (u32 i = 0; i < IOP_HEAP_COUNT; i++) {
        run = iop_heap[i] ? 0 : run + 1;
        if (run > best) best = run;
    }
    HRET(best * IOP_HEAP_PAGE);
}
void hle_sceSifQueryTotalFreeMemSize(ps2_ctx *ctx) {
    u32 count = 0;
    for (u32 i = 0; i < IOP_HEAP_COUNT; i++) count += !iop_heap[i];
    HRET(count * IOP_HEAP_PAGE);
}

static int next_module_id = 3;

static void load_module_common(ps2_ctx *ctx, const char *what, u32 name_addr) {
    char name[128];
    int id = next_module_id++;
    if (name_addr) {
        ps2_get_str(name_addr, name, sizeof(name));
        ps2_log("iop: %s '%s' -> id %d (serviced natively)", what, name, id);
    } else {
        ps2_log("iop: %s <buffer> -> id %d (serviced natively)", what, id);
    }
    HRET(id);
}

void hle_sceSifLoadModule(ps2_ctx *ctx) {
    load_module_common(ctx, "LoadModule", ps2_arg(ctx, 0));
}
void hle_sceSifLoadStartModule(ps2_ctx *ctx) {
    load_module_common(ctx, "LoadStartModule", ps2_arg(ctx, 0));
}
void hle_sceSifLoadModuleBuffer(ps2_ctx *ctx) {
    load_module_common(ctx, "LoadModuleBuffer", 0);
}
void hle_sceSifLoadStartModuleBuffer(ps2_ctx *ctx) {
    load_module_common(ctx, "LoadStartModuleBuffer", 0);
}
void hle_sceSifLoadElf(ps2_ctx *ctx)      { HRET(0); }
void hle_sceSifLoadElfPart(ps2_ctx *ctx)  { HRET(0); }
void hle_sceSifStopModule(ps2_ctx *ctx)   { HRET(0); }
void hle_sceSifUnloadModule(ps2_ctx *ctx) { HRET(0); }
void hle_sceSifSearchModuleByName(ps2_ctx *ctx)    { HRET(-1); }
void hle_sceSifSearchModuleByAddress(ps2_ctx *ctx) { HRET(-1); }
void hle_sceSifLoadFileReset(ps2_ctx *ctx) { HRET(0); }

void hle_sceSifWriteBackDCache(ps2_ctx *ctx) { HRET(0); }

void hle_sceDeci2Poll(ps2_ctx *ctx)    { HRET(0); }
void hle_sceDeci2Open(ps2_ctx *ctx)    { HRET(-1); }
void hle_sceDeci2Close(ps2_ctx *ctx)   { HRET(0); }
void hle_sceDeci2ReqSend(ps2_ctx *ctx) { HRET(0); }
void hle_sceDeci2ExRecv(ps2_ctx *ctx)  { HRET(0); }
void hle_sceDeci2ExSend(ps2_ctx *ctx)  { HRET(0); }
void hle_sceDeci2ExReqSend(ps2_ctx *ctx) { HRET(0); }

void ps2_sif_hle_init(void) {
#if PS2_GAME_AC4
    void ps2_ac4_audio_reset(void);
    ps2_ac4_audio_reset();
#endif
    if (++ac4_rpc_epoch == 0) ++ac4_rpc_epoch;
    ac4_modules = 0;
    nservices = 0;
    nunknown = 0;
    bind_count = call_count = unknown_calls = 0;
    memset(iop_heap, 0, sizeof(iop_heap));
    next_module_id = 3;
}

void ps2_sif_hle_report(void) {
    ps2_log("rpc: %llu binds, %llu calls (%llu on unserviced ids)",
            (unsigned long long)bind_count, (unsigned long long)call_count,
            (unsigned long long)unknown_calls);
    for (unsigned i = 0; i < nservices; i++)
        if (services[i].calls)
            ps2_log("   %08X %-12s %llu calls", services[i].sid,
                    services[i].name, (unsigned long long)services[i].calls);
    for (unsigned i = 0; i < nunknown; i++)
        ps2_log("   %08X <no service>", unknown_sids[i]);
}

void hle_kputs(ps2_ctx *ctx) {
    char line[512];
    size_t n;
    ps2_get_str(ps2_arg(ctx, 0), line, sizeof(line));
    n = strlen(line);
    while (n && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = 0;
    if (n) ps2_log("guest: %s", line);
    HRET(0);
}
