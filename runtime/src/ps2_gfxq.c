#include "ps2_gfxq.h"
#include "rn.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

void ps2_vif_transfer(int which, const ps2_reg128 *q, u32 qwc);
void ps2_vif_write(int which, u32 word);
void ps2_vif_clear_stall(int which);
void ps2_vif_fbrst(int which, u32 val);
void ps2_gif_transfer(const ps2_reg128 *data, u32 qwc);
void ps2_gs_priv_write_now(u32 addr, u64 val);
void ps2_gif_ctrl_write(u32 addr, u32 val);
int  ps2_gs_present_now(void);
void ps2_request_exit(void);

int ps2_gfxq_on;

enum {
    GQ_WRAP = 0,
    GQ_VIF_QW,
    GQ_GIF_QW,
    GQ_VIF_W,
    GQ_FBRST,
    GQ_VIFCLR,
    GQ_GS_PRIV,
    GQ_MMIO,
    GQ_FIELD,
    GQ_TAG,
    GQ_INTENT,
    GQ_FIELD_TRACE
};

static u8 *q_buf;
static size_t q_cap;
static size_t q_head, q_tail;
static int q_busy;
static int q_running;
static pthread_t q_thread;
static _Thread_local int q_is_worker;
static pthread_mutex_t q_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t q_not_empty = PTHREAD_COND_INITIALIZER;
static pthread_cond_t q_not_full  = PTHREAD_COND_INITIALIZER;
/* Ring progress does not complete a field. Keep field-depth waiters asleep
 * until the worker reaches a field marker instead of waking every batch. */
static pthread_cond_t q_field_ready = PTHREAD_COND_INITIALIZER;
static pthread_cond_t q_idle      = PTHREAD_COND_INITIALIZER;

static u64 st_records, st_bytes, st_producer_waits, st_drains, st_drain_waits;
static size_t st_peak_fill;
static int q_fields_pending;
static int q_field_depth = 2;
static int q_split_field_wait=1;
static u64 st_field_waits;
static u64 st_field_wait_ns;

static u64 gq_now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (u64)ts.tv_sec * 1000000000ull + (u64)ts.tv_nsec;
}
/* Optional field diagnostics. No file IO or per-packet clocks on the hot
 * path; timestamps distinguish producer gaps, queue backlog, CPU and idle.
 * Rows are written only after worker shutdown by the ordinary report path. */
#define GQ_TRACE_MAX 32768u
static struct {
    u64 field,ready,cpu,queued,worker,worker_cpu,idle,previous_present,bytes,producer_tid,cycles,lock,apply,notify;
} q_trace_rows[GQ_TRACE_MAX];
/* Optional packet clocks, only when explicitly requested with field tracing.
 * Accumulate in memory; no packet logging or IO in the worker. */
static struct { u64 ns[12],count[12],max_ns[12],max_bytes[12]; } q_jobs[GQ_TRACE_MAX];
static int q_jobs_on;
static u32 q_trace_n;
static u64 q_trace_dropped,q_trace_idle,q_trace_previous_present;
static u64 q_trace_lock,q_trace_apply,q_trace_notify;
static int q_trace_on;
static const char *q_trace_path;
static u64 q_cpu_ns(void) {
#ifdef _WIN32
    FILETIME created,exited,kernel,user;
    if(GetThreadTimes(GetCurrentThread(),&created,&exited,&kernel,&user))
        return (((u64)kernel.dwHighDateTime<<32)+kernel.dwLowDateTime
              +((u64)user.dwHighDateTime<<32)+user.dwLowDateTime)*100ull;
#endif
    return 0;
}
static u64 q_cpu_cycles(void) {
#ifdef _WIN32
    ULONG64 cycles=0;
    if(QueryThreadCycleTime(GetCurrentThread(),&cycles))return (u64)cycles;
#endif
    return 0;
}
static u64 q_thread_id(void) {
#ifdef _WIN32
    return GetCurrentThreadId();
#else
    return 0;
#endif
}
static void q_trace_capture(const u8 *r,size_t tail) {
    if(q_trace_n>=GQ_TRACE_MAX){q_trace_dropped++;return;}
    u32 i=q_trace_n++;
    q_trace_rows[i].field=(u64)i+1;
    memcpy(&q_trace_rows[i].ready,r+1,8);
    memcpy(&q_trace_rows[i].cpu,r+9,8);
    memcpy(&q_trace_rows[i].queued,r+17,8);
    memcpy(&q_trace_rows[i].producer_tid,r+25,8);
    q_trace_rows[i].worker=gq_now_ns();
    q_trace_rows[i].worker_cpu=q_cpu_ns();
    q_trace_rows[i].cycles=q_cpu_cycles();
    q_trace_rows[i].lock=q_trace_lock;
    q_trace_rows[i].apply=q_trace_apply;
    q_trace_rows[i].notify=q_trace_notify;
    q_trace_rows[i].idle=q_trace_idle;
    q_trace_rows[i].previous_present=q_trace_previous_present;
    q_trace_rows[i].bytes=__atomic_load_n(&q_head,__ATOMIC_ACQUIRE)-tail;
}
static void q_trace_report(void) {
    if(!q_trace_on)return;
    FILE *f=fopen(q_trace_path,"w");
    if(!f){ps2_log("gfx timeline: cannot write %s",q_trace_path);return;}
    fprintf(f,"field,producer_ready_ns,producer_cpu_ns,queued_ns,worker_ready_ns,worker_cpu_ns,worker_idle_ns,previous_present_ns,queue_bytes,producer_tid,worker_cycles,worker_lock_ns,worker_apply_ns,worker_notify_ns\n");
    for(u32 i=0;i<q_trace_n;i++) {
        const u64 v[14]={q_trace_rows[i].field,q_trace_rows[i].ready,q_trace_rows[i].cpu,
            q_trace_rows[i].queued,q_trace_rows[i].worker,q_trace_rows[i].worker_cpu,
            q_trace_rows[i].idle,q_trace_rows[i].previous_present,q_trace_rows[i].bytes,q_trace_rows[i].producer_tid,
            q_trace_rows[i].cycles,q_trace_rows[i].lock,q_trace_rows[i].apply,q_trace_rows[i].notify};
        for(u32 k=0;k<14;k++)fprintf(f,k?",%llu":"%llu",(unsigned long long)v[k]);
        fputc('\n',f);
    }
    fclose(f);
    if(q_jobs_on) {
        char path[4096];
        if(snprintf(path,sizeof(path),"%s.jobs.csv",q_trace_path)<(int)sizeof(path)) {
            f=fopen(path,"w");
            if(f) {
                fprintf(f,"field,opcode,count,total_ns,max_ns,max_bytes\n");
                for(u32 i=0;i<q_trace_n;i++)for(u32 op=1;op<12;op++)
                    if(q_jobs[i].count[op])fprintf(f,"%u,%u,%llu,%llu,%llu,%llu\n",i+1,op,
                        (unsigned long long)q_jobs[i].count[op],(unsigned long long)q_jobs[i].ns[op],
                        (unsigned long long)q_jobs[i].max_ns[op],(unsigned long long)q_jobs[i].max_bytes[op]);
                fclose(f);
            } else ps2_log("gfx jobs: cannot write %s",path);
        }
    }
    ps2_log("gfx timeline: %u fields, %llu overflow rows; %s",q_trace_n,
        (unsigned long long)q_trace_dropped,q_trace_path);
}

static u64 st_producer_ns, st_drain_ns;

static u64 speed_present_ns, speed_present_n, speed_work_ns;
static u64 speed_vu_runs, speed_vu_insns, speed_vu_guards;
void ps2_vu1_speed_counters(u64 *, u64 *, u64 *);
int ps2_speed_enabled(void) {
    static int enabled = -1;
    int value = __atomic_load_n(&enabled, __ATOMIC_RELAXED);
    if (value < 0) {
        const char *e = getenv("PS2_SPEED_LOG"); value = e && atoi(e) != 0;
        __atomic_store_n(&enabled, value, __ATOMIC_RELAXED);
    }
    return value;
}
void ps2_scene_speed_report(double dt);
void ps2_speed_sample(u64 fields, u64 dropped) {
    static u64 prev_ns, prev_fields, prev_dropped, prev_present, prev_n, prev_work;
    static u64 prev_field_wait, prev_ring_wait, prev_drain, prev_bytes;
    static u64 prev_vu_runs, prev_vu_insns, prev_vu_guards;
    if (!ps2_speed_enabled()) return;
    u64 now = gq_now_ns();
    if (!prev_ns) {
        ps2_log("speed: diagnostics v1; 5s intervals, scene dispatch is not simulation FPS; collision/VIF fixes 2026-09-06");
        ps2_log("speed: EE/present/worker ms are interval totals; present_ms is GS display-list handoff, not Vulkan GPU time. See vk-speed for renderer host stages.");
        prev_ns = now; prev_fields = fields; prev_dropped = dropped;
        prev_field_wait=st_field_wait_ns; prev_ring_wait=st_producer_ns;
        prev_drain=st_drain_ns; prev_bytes=st_bytes;
        pthread_mutex_lock(&q_lock);
        prev_present=speed_present_ns; prev_n=speed_present_n; prev_work=speed_work_ns;
        prev_vu_runs=speed_vu_runs; prev_vu_insns=speed_vu_insns; prev_vu_guards=speed_vu_guards;
        pthread_mutex_unlock(&q_lock);
        ps2_scene_speed_report(0);
        return;
    }
    if (now - prev_ns < 5000000000ull) return;
    double dt = (now - prev_ns) / 1e9;
    pthread_mutex_lock(&q_lock);
    u64 pn = speed_present_n, pt = speed_present_ns, work = speed_work_ns;
    u64 vr=speed_vu_runs, vi=speed_vu_insns, vg=speed_vu_guards;
    size_t fill = q_head - q_tail;
    int pending = q_fields_pending;
    pthread_mutex_unlock(&q_lock);
    ps2_log("speed: dt=%.2fs fields=%.2f/s dropped=%llu gfxq=%d pending=%d ring_KB=%zu queued_MB=%.2f",
        dt, (fields-prev_fields)/dt, (unsigned long long)(dropped-prev_dropped),
        ps2_gfxq_on, pending, fill >> 10, (st_bytes-prev_bytes)/1048576.0);
    ps2_log("speed: EE wait_ms field=%.2f ring=%.2f drain=%.2f; present=%.2f/s present_ms=%.2f worker_active_ms=%.2f",
        (st_field_wait_ns-prev_field_wait)/1e6, (st_producer_ns-prev_ring_wait)/1e6,
        (st_drain_ns-prev_drain)/1e6, (pn-prev_n)/dt, (pt-prev_present)/1e6, (work-prev_work)/1e6);
    ps2_scene_speed_report(dt);
    ps2_log("speed: VU1 runs=%.2f/s instructions_M=%.3f guard_hits=%llu",
        (vr-prev_vu_runs)/dt, (vi-prev_vu_insns)/1e6, (unsigned long long)(vg-prev_vu_guards));
    prev_vu_runs=vr; prev_vu_insns=vi; prev_vu_guards=vg;
    prev_ns=now; prev_fields=fields; prev_dropped=dropped; prev_present=pt; prev_n=pn;
    prev_field_wait=st_field_wait_ns; prev_ring_wait=st_producer_ns; prev_drain=st_drain_ns;
    prev_bytes=st_bytes;
    prev_work=work;
}

static size_t q_producer_tail, q_reserved_end;
static u8 *q_reserve(u32 n) {
    size_t off = q_head & (q_cap - 1);
    size_t pad = off + n > q_cap ? q_cap - off : 0;
    size_t need = pad + n;
    if (q_cap - (q_head - q_producer_tail) < need) {
        pthread_mutex_lock(&q_lock);
        q_producer_tail = q_tail;
        while (q_cap - (q_head - q_producer_tail) < need) {
            u64 t0 = gq_now_ns();
            st_producer_waits++;
            pthread_cond_wait(&q_not_full, &q_lock);
            st_producer_ns += gq_now_ns() - t0;
            q_producer_tail = q_tail;
        }
        pthread_mutex_unlock(&q_lock);
    }
    q_reserved_end = q_head + need;
    if (pad) {
        q_buf[off] = GQ_WRAP;
        off = 0;
    }
    return q_buf + off;
}

static void q_commit(u32 n) {
    size_t fill;
    __atomic_store_n(&q_head, q_reserved_end, __ATOMIC_SEQ_CST);
    st_records++;
    st_bytes += n;
    fill = q_reserved_end - __atomic_load_n(&q_tail, __ATOMIC_RELAXED);
    if (fill > st_peak_fill) st_peak_fill = fill;
    if (!__atomic_load_n(&q_busy, __ATOMIC_SEQ_CST)) {
        pthread_mutex_lock(&q_lock);
        pthread_cond_signal(&q_not_empty);
        pthread_mutex_unlock(&q_lock);
    }
}

static inline void put8(u8 *p, u32 off, u8 v)   { p[off] = v; }
static inline void put32(u8 *p, u32 off, u32 v) { memcpy(p + off, &v, 4); }
static inline void put64(u8 *p, u32 off, u64 v) { memcpy(p + off, &v, 8); }
static inline u32  get32(const u8 *p, u32 off)  { u32 v; memcpy(&v, p + off, 4); return v; }
static inline u64  get64(const u8 *p, u32 off)  { u64 v; memcpy(&v, p + off, 8); return v; }

static u32 q_apply(const u8 *r) {
    switch (r[0]) {
    case GQ_VIF_QW: {
        int ch = (int)r[1];
        u32 qwc = get32(r, 2);
        const ps2_reg128 *qw = (const ps2_reg128 *)(r + 6);
        ps2_vif_transfer(ch, qw, qwc);
        return 6u + qwc * 16u;
    }
    case GQ_GIF_QW: {
        u32 qwc = get32(r, 1);
        ps2_gif_transfer((const ps2_reg128 *)(r + 5), qwc);
        return 5u + qwc * 16u;
    }
    case GQ_VIF_W:   ps2_vif_write((int)r[1], get32(r, 2));        return 6u;
    case GQ_FBRST:   ps2_vif_fbrst((int)r[1], get32(r, 2));        return 6u;
    case GQ_VIFCLR:  ps2_vif_clear_stall((int)r[1]);               return 2u;
    case GQ_GS_PRIV: ps2_gs_priv_write_now(get32(r, 1), get64(r, 5)); return 13u;
    case GQ_MMIO:    ps2_gif_ctrl_write(get32(r, 1), get32(r, 5)); return 9u;
    case GQ_TAG:     rn_gs_tag(get32(r, 1), get32(r, 5), get32(r, 9)); return 13u;
    case GQ_INTENT: {
        u32 len = get32(r, 1);
        rn_gs_intent(r + 5, len);
        return 5u + len;
    }
    case GQ_FIELD:
    case GQ_FIELD_TRACE:
        if (!ps2_gs_present_now()) ps2_request_exit();
        return r[0]==GQ_FIELD_TRACE ? 33u : 1u;
    default:
        ps2_log("gfxq: corrupt record opcode %u -- stopping the worker", r[0]);
        return 0u;
    }
}

static void *q_main(void *unused) {
    (void)unused;
    q_is_worker = 1;
    ps2_host_prof_attach("gfx");
    u64 speed_burst=0;
    int speed_on=ps2_speed_enabled();
    pthread_mutex_lock(&q_lock);
    for (;;) {
        while (q_running && __atomic_load_n(&q_head, __ATOMIC_SEQ_CST) == q_tail) {
            if (speed_burst) { speed_work_ns += gq_now_ns()-speed_burst; speed_burst=0; }
            __atomic_store_n(&q_busy, 0, __ATOMIC_SEQ_CST);
            pthread_cond_broadcast(&q_idle);
            if (__atomic_load_n(&q_head, __ATOMIC_SEQ_CST) != q_tail) break;
            u64 idle_start=q_trace_on?gq_now_ns():0;
            pthread_cond_wait(&q_not_empty, &q_lock);
            if(q_trace_on)q_trace_idle+=gq_now_ns()-idle_start;
        }
        if (!q_running && __atomic_load_n(&q_head, __ATOMIC_SEQ_CST) == q_tail) break;
        __atomic_store_n(&q_busy, 1, __ATOMIC_SEQ_CST);
        if (!speed_burst && speed_on) speed_burst=gq_now_ns();
        size_t tail = q_tail, head = __atomic_load_n(&q_head, __ATOMIC_ACQUIRE);
        unsigned records = 0;
        int field = 0, corrupt = 0;
        u64 speed_elapsed = 0;
        pthread_mutex_unlock(&q_lock);
        u64 batch_start=q_trace_on?gq_now_ns():0;
        while (tail != head && records < 256) {
            const u8 *r = q_buf + (tail & (q_cap - 1));
            if (*r == GQ_WRAP) {
                tail += q_cap - (tail & (q_cap - 1));
                continue;
            }
            field = *r == GQ_FIELD || *r == GQ_FIELD_TRACE;
            if(*r==GQ_FIELD_TRACE) {
                u64 now=gq_now_ns();q_trace_apply+=now-batch_start;batch_start=now;
                q_trace_capture(r,tail);
            }
            u64 start = field && (speed_on || q_trace_on) ? gq_now_ns() : 0;
            u64 job_row=(u64)q_trace_n+q_trace_dropped;
            if(field && job_row)job_row--;
            u32 job_op=*r;
            u64 job_start=q_jobs_on && job_row<GQ_TRACE_MAX ? gq_now_ns():0;
            u32 n;
            { PS2_PHASE_BEGIN(PS2_PH_GFXQ);
              n = q_apply(r);
              PS2_PHASE_END(PS2_PH_GFXQ); }
            if(job_start && job_op<12) {
                u64 elapsed=gq_now_ns()-job_start;
                q_jobs[job_row].count[job_op]++;q_jobs[job_row].ns[job_op]+=elapsed;
                if(elapsed>q_jobs[job_row].max_ns[job_op]) {
                    q_jobs[job_row].max_ns[job_op]=elapsed;q_jobs[job_row].max_bytes[job_op]=n;
                }
            }
            if (start) {
                speed_elapsed = gq_now_ns() - start;
                if(q_trace_on)q_trace_previous_present=speed_elapsed;
            }
            if (!n) { corrupt = 1; break; }
            tail += n;
            records++;
            if (field) break;
        }
        u64 lock_start=0;
        if(q_trace_on) {lock_start=gq_now_ns();q_trace_apply+=lock_start-batch_start;}
        pthread_mutex_lock(&q_lock);
        if(q_trace_on)q_trace_lock+=gq_now_ns()-lock_start;
        __atomic_store_n(&q_tail, tail, __ATOMIC_RELEASE);
        if (field && !corrupt) {
            if (speed_on) {
                speed_present_ns += speed_elapsed; speed_present_n++;
                ps2_vu1_speed_counters(&speed_vu_runs,&speed_vu_insns,&speed_vu_guards);
            }
            if (speed_burst) { u64 now=gq_now_ns(); speed_work_ns+=now-speed_burst; speed_burst=now; }
            q_fields_pending--;
        }
        u64 notify_start=q_trace_on?gq_now_ns():0;
        pthread_cond_broadcast(&q_not_full);
        if(q_split_field_wait && (field || corrupt))pthread_cond_broadcast(&q_field_ready);
        if(q_trace_on)q_trace_notify+=gq_now_ns()-notify_start;
        if (corrupt) { q_running = 0; break; }
    }
    __atomic_store_n(&q_busy, 0, __ATOMIC_SEQ_CST);
    pthread_cond_broadcast(&q_idle);
    pthread_mutex_unlock(&q_lock);
    return NULL;
}

static const char *q_forbidden;
void ps2_gfxq_forbid(const char *why) { if (!q_forbidden) q_forbidden = why; }

void ps2_gfxq_init(void) {
    const char *e = getenv("PS2_ASYNC_GFX");
    size_t mb;
    if (e && *e == '0') {
        ps2_log("gfxq: PS2_ASYNC_GFX=0 -- the graphics machine runs on the EE "
                "thread, as it did before.");
        return;
    }

    if (q_forbidden) {
        ps2_log("gfxq: PS2_ASYNC_GFX is set but %s, so the graphics machine "
                "stays on the EE thread.", q_forbidden);
        return;
    }

    mb = 64;
    { const char *m = getenv("PS2_ASYNC_MB");
      if (m && *m) { double v = atof(m); if (v >= 1.0 && v <= 1024.0) mb = (size_t)v; } }
    { const char *d = getenv("PS2_ASYNC_FIELDS");
      if (d && *d) { int v = atoi(d); if (v >= 1 && v <= 8) q_field_depth = v; } }
    q_cap = 1;
    while (q_cap < mb * 1024u * 1024u) q_cap <<= 1;
    q_buf = (u8 *)malloc(q_cap);
    if (!q_buf) {
        ps2_log("gfxq: cannot allocate a %zu MB ring; staying synchronous",
                q_cap >> 20);
        return;
    }
    q_trace_path=getenv("PS2_GFX_TIMELINE");
    q_trace_on=q_trace_path && *q_trace_path;
    {const char *jobs=getenv("PS2_GFX_JOB_TIMING");q_jobs_on=q_trace_on && jobs && atoi(jobs)!=0;}
    if(q_jobs_on)memset(q_jobs,0,sizeof(q_jobs));
    q_trace_n=0;q_trace_dropped=q_trace_idle=q_trace_previous_present=0;
    q_trace_lock=q_trace_apply=q_trace_notify=0;
    {const char *legacy=getenv("PS2_GFX_FIELD_WAKE_LEGACY");q_split_field_wait=!(legacy && atoi(legacy)!=0);}
    q_head = q_tail = q_producer_tail = q_reserved_end = 0;
    q_running = 1;
    if (pthread_create(&q_thread, NULL, q_main, NULL) != 0) {
        ps2_log("gfxq: cannot start the worker thread; staying synchronous");
        free(q_buf); q_buf = NULL; q_running = 0;
        return;
    }
    ps2_gfxq_on = 1;
    ps2_log("gfxq: the graphics machine (VIF, VU1, GIF, GS) runs on its own "
            "thread; %zu MB ring", q_cap >> 20);
}

void ps2_gfxq_drain(void) {
    u64 t0;
    if (!ps2_gfxq_on || q_is_worker) return;
    t0 = gq_now_ns();
    pthread_mutex_lock(&q_lock);
    st_drains++;
    if (q_head != q_tail || q_busy || q_fields_pending) {
        st_drain_waits++;
        pthread_cond_signal(&q_not_empty);
        while (q_running && (q_head != q_tail || q_busy || q_fields_pending))
            pthread_cond_wait(&q_idle, &q_lock);
    }
    pthread_mutex_unlock(&q_lock);
    u64 elapsed=gq_now_ns()-t0;
    st_drain_ns += elapsed;
    if (elapsed>25000000ull && ps2_speed_enabled())
        ps2_log("ac4 spike: op=gfx-drain ms=%.3f",elapsed/1e6);
}

void ps2_gfxq_shutdown(void) {
    if (!ps2_gfxq_on) return;
    ps2_gfxq_drain();
    pthread_mutex_lock(&q_lock);
    q_running = 0;
    pthread_cond_broadcast(&q_not_empty);
    pthread_mutex_unlock(&q_lock);
    pthread_join(q_thread, NULL);
    q_trace_report();
    ps2_gfxq_on = 0;
}

int ps2_gfxq_blocks_capture(void) { return ps2_gfxq_on; }

void ps2_gfxq_report(void) {
    if (!st_records) return;
    ps2_log("gfxq: %llu records, %llu MB queued, peak fill %zu KB of %zu MB",
            (unsigned long long)st_records,
            (unsigned long long)(st_bytes >> 20),
            st_peak_fill >> 10, q_cap >> 20);
    ps2_log("gfxq: %llu drains, %llu of them actually waited (%.1f ms total); "
            "the EE waited on a full ring %llu times (%.1f ms total)",
            (unsigned long long)st_drains, (unsigned long long)st_drain_waits,
            (double)st_drain_ns / 1e6,
            (unsigned long long)st_producer_waits,
            (double)st_producer_ns / 1e6);
    ps2_log("gfxq: the EE waited for the graphics machine to catch up on "
            "fields %llu times (%.1f ms total), at a depth of %d",
            (unsigned long long)st_field_waits,
            (double)st_field_wait_ns / 1e6, q_field_depth);
    if (st_producer_waits)
        ps2_log("gfxq:   a non-zero full-ring count means the graphics machine "
                "is the constraint, not the EE -- a bigger ring will not help.");
}

/* VIF0 shares memory and registers with EE macro-mode VU0. Complete its
 * uploads on the EE thread before guest DMA polling or VCALLMS can observe
 * completion; only VIF1 belongs to the graphics worker. */
void ps2_gfxq_vif_qw(int ch, const ps2_reg128 *qw, u32 qwc) {
    if (!(ch & 1) || !ps2_gfxq_on) {
        ps2_vif_transfer(ch, qw, qwc);
        return;
    }
    if (!qwc) return;
    {
        u32 n = 6u + qwc * 16u;
        u8 *r = q_reserve(n);
        put8(r, 0, GQ_VIF_QW);
        put8(r, 1, (u8)ch);
        put32(r, 2, qwc);
        memcpy(r + 6, qw, (size_t)qwc * 16u);
        q_commit(n);
    }
}

void ps2_gfxq_gif_qw(const ps2_reg128 *qw, u32 qwc) {
    if (!ps2_gfxq_on) { ps2_gif_transfer(qw, qwc); return; }
    if (!qwc) return;
    {
        u32 n = 5u + qwc * 16u;
        u8 *r = q_reserve(n);
        put8(r, 0, GQ_GIF_QW);
        put32(r, 1, qwc);
        memcpy(r + 5, qw, (size_t)qwc * 16u);
        q_commit(n);
    }
}

void ps2_gfxq_vif_word(int ch, u32 word) {
    if (!(ch & 1) || !ps2_gfxq_on) { ps2_vif_write(ch, word); return; }
    { u8 *r = q_reserve(6);
      put8(r, 0, GQ_VIF_W); put8(r, 1, (u8)ch); put32(r, 2, word);
      q_commit(6); }
}

void ps2_gfxq_fbrst(int ch, u32 val) {
    if (!(ch & 1) || !ps2_gfxq_on) { ps2_vif_fbrst(ch, val); return; }
    { u8 *r = q_reserve(6);
      put8(r, 0, GQ_FBRST); put8(r, 1, (u8)ch); put32(r, 2, val);
      q_commit(6); }
}

void ps2_gfxq_vif_clear(int ch) {
    if (!(ch & 1) || !ps2_gfxq_on) { ps2_vif_clear_stall(ch); return; }
    { u8 *r = q_reserve(2);
      put8(r, 0, GQ_VIFCLR); put8(r, 1, (u8)ch);
      q_commit(2); }
}

void ps2_gfxq_gs_priv(u32 addr, u64 val) {
    if (!ps2_gfxq_on) { ps2_gs_priv_write_now(addr, val); return; }
    { u8 *r = q_reserve(13);
      put8(r, 0, GQ_GS_PRIV); put32(r, 1, addr); put64(r, 5, val);
      q_commit(13); }
}

int ps2_gfxq_field(void) {
    u64 producer_ready=q_trace_on?gq_now_ns():0;
    u64 producer_cpu=q_trace_on?q_cpu_ns():0;
    if (!ps2_gfxq_on) {
        u64 start = ps2_speed_enabled() ? gq_now_ns() : 0;
        int result = ps2_gs_present_now();
        if (start) {
            speed_present_ns += gq_now_ns()-start; speed_present_n++;
            ps2_vu1_speed_counters(&speed_vu_runs,&speed_vu_insns,&speed_vu_guards);
        }
        return result;
    }
    pthread_mutex_lock(&q_lock);
    if (q_fields_pending >= q_field_depth) {
        u64 t0 = gq_now_ns();
        st_field_waits++;
        while (q_running && q_fields_pending >= q_field_depth)
            pthread_cond_wait(q_split_field_wait?&q_field_ready:&q_not_full, &q_lock);
        st_field_wait_ns += gq_now_ns() - t0;
    }
    q_fields_pending++;
    pthread_mutex_unlock(&q_lock);
    { u32 size=q_trace_on?33u:1u;
      u8 *r = q_reserve(size);
      put8(r, 0, q_trace_on?GQ_FIELD_TRACE:GQ_FIELD);
      if(q_trace_on) {
          put64(r,1,producer_ready);put64(r,9,producer_cpu);put64(r,17,gq_now_ns());put64(r,25,q_thread_id());
      }
      q_commit(size); }
    return 1;
}

void ps2_gfxq_mmio(u32 addr, u32 val) {
    if (!ps2_gfxq_on) { ps2_gif_ctrl_write(addr, val); return; }
    { u8 *r = q_reserve(9);
      put8(r, 0, GQ_MMIO); put32(r, 1, addr); put32(r, 5, val);
      q_commit(9); }
}

void ps2_gfxq_tag(u32 kind, u32 a, u32 b) {
    if (!ps2_gfxq_on) { rn_gs_tag(kind, a, b); return; }
    { u8 *r = q_reserve(13);
      put8(r, 0, GQ_TAG); put32(r, 1, kind); put32(r, 5, a); put32(r, 9, b);
      q_commit(13); }
}

void ps2_gfxq_intent(const u8 *rec, u32 len) {
    if (!ps2_gfxq_on) { rn_gs_intent(rec, len); return; }
    { u8 *r = q_reserve(5u + len);
      put8(r, 0, GQ_INTENT); put32(r, 1, len);
      memcpy(r + 5, rec, len);
      q_commit(5u + len); }
}
