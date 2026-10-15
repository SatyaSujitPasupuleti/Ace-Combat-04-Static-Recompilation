#include "ps2_runtime.h"
#include <SDL3/SDL.h>

u64 ps2_kernel_vblank_count(void);
static u64 movie_next_field, movie_first_field, movie_picture;
static u32 movie_owner, movie_rate_num, movie_rate_den;
static int movie_clock_started;
void ac4_movie_audio_reset(void);

/* NTSC-U SMV pictures use their MPEG rate; the field clock is 59.94 Hz.
 * 00213048 decodes AND draws a new picture and advances subtitle time.
 * Running it every field releases its two 1.5 MiB input buffers before
 * the 48 kHz PCM reader has consumed their audio. Pace the original guest
 * routine, keeping its audio refill/state-machine calls and skip handling.
 * Verified against SLUS_201.52: 00212508 initializes the movie object;
 * its state is at +90 and the two active decode states are 3 and 4. */
void ac4_movie_clock_reset(ps2_ctx *ctx) {
    ac4_movie_audio_reset();
    movie_owner=ctx->r[4].uw[0];
    movie_clock_started=0;
    movie_next_field=movie_first_field=movie_picture=0;
}

void ac4_movie_clock_wait(ps2_ctx *ctx) {
    u32 owner=ctx->r[4].uw[0];
    u32 state=ps2_r32(owner+0x90u);
    if (state!=3u && state!=4u) return;
    if (getenv("PS2_AC4_MOVIE_UNPACED")) return;
    u64 field=ps2_kernel_vblank_count();
    if (!movie_clock_started || movie_owner!=owner) {
        u32 index=ps2_r32(owner+0x68u);
        if (index>1u) return;
        u32 chunk=ps2_r32(owner+8u+index*4u);
        if (ps2_r32(chunk+16u)!=0xB3010000u) return;
        u32 rate=ps2_r8(chunk+23u)&15u;
        /* Exact ratios of 60000/1001 guest fields to MPEG pictures. */
        static const u32 num[9]={0,5,2500,2400,2,2000,1200,1,1000};
        static const u32 den[9]={0,2,1001,1001,1,1001,1001,1,1001};
        if (!rate || rate>8u) return;
        movie_rate_num=num[rate];movie_rate_den=den[rate];
        ps2_log("ac4 movie clock: MPEG rate=%u fields/picture=%u/%u",rate,movie_rate_num,movie_rate_den);
        movie_owner=owner;
        movie_next_field=movie_first_field=field;
        movie_picture=0;
        movie_clock_started=1;
    }
    while (field<movie_next_field) {
        ps2_preempt();
        field=ps2_kernel_vblank_count();
        if (field<movie_next_field) SDL_Delay(1);
    }
    /* Opt-in test: let the audio clock advance while the movie renderer
     * cannot refill the guest ring. Never enabled by a normal launcher. */
    const char *stall=getenv("PS2_AC4_MOVIE_TEST_STALL_FIELDS");
    if (stall && movie_picture==100u) {
        unsigned fields=(unsigned)strtoul(stall,NULL,10);
        if (fields>60u) fields=60u;
        u64 end=field+fields;
        while (field<end) {
            ps2_preempt();field=ps2_kernel_vblank_count();
            if (field<end) SDL_Delay(1);
        }
    }
    if (getenv("PS2_AC4_MOVIE_LIFETIME_TRACE"))
        ps2_log("movie draw: field=%llu due=%llu frame=%u state=%u",
            (unsigned long long)field,(unsigned long long)movie_next_field,
            ps2_r32(owner+0x94u),state);
    /* Round each picture deadline up to a field, retaining the exact
     * rational time base. INTRO uses 29.97 Hz; the story movies use 30. */
    ++movie_picture;
    movie_next_field=movie_first_field+
        (movie_picture*movie_rate_num+movie_rate_den-1u)/movie_rate_den;
}

/* Own the PCM payload as soon as a complete SMV disc chunk arrives. The
 * guest's 32 KiB AutoDMA ring provides only 171 ms of storage and can be
 * refilled late while MPEG decode is preempted. Four owned chunks retain
 * disc ordering independently of both EE reuse and IOP refill timing.
 * All access is on the EE thread, before the mixed callback queue. */
#define MOVIE_AUDIO_SLOTS 4u
static struct { u8 *data; u32 capacity, bytes; } movie_audio[MOVIE_AUDIO_SLOTS];
static u32 movie_audio_head, movie_audio_count, movie_audio_sample_pos;
static int movie_audio_active, movie_audio_failed;
static u64 movie_audio_underflow, movie_audio_captured, movie_audio_played;

void ac4_movie_audio_reset(void) {
    movie_audio_head=movie_audio_count=movie_audio_sample_pos=0;
    movie_audio_active=movie_audio_failed=0;
    movie_audio_underflow=movie_audio_captured=movie_audio_played=0;
}

void ac4_movie_audio_capture(u32 dst,u32 bytes) {
    if (!movie_owner || movie_audio_failed || getenv("PS2_AC4_MOVIE_UNPACED")) return;
    if (ps2_r32(movie_owner+0x84u)) return; /* movie explicitly has no audio */
    if (dst!=ps2_r32(movie_owner) && dst!=ps2_r32(movie_owner+4u)) return;
    if (dst>=PS2_RAM_SIZE || bytes>PS2_RAM_SIZE-dst || bytes<24u) return;
    if (ps2_r32(dst+16u)!=0xB3010000u) return; /* skip file header/end marker */
    u32 offset=ps2_r32(dst+8u),size=ps2_r32(dst+12u);
    if (offset<24u || offset>bytes || !size || (size&1023u) || size>bytes-offset) return;
    if (movie_audio_count==MOVIE_AUDIO_SLOTS) {
        movie_audio_failed=1;movie_audio_active=0;
        ps2_log("ac4 movie audio: owned queue overflow; reverting to guest ring");
        return;
    }
    u32 slot=(movie_audio_head+movie_audio_count)%MOVIE_AUDIO_SLOTS;
    if (size>movie_audio[slot].capacity) {
        u8 *data=realloc(movie_audio[slot].data,size);
        if (!data) {
            movie_audio_failed=1;movie_audio_active=0;
            ps2_log("ac4 movie audio: allocation failed; reverting to guest ring");
            return;
        }
        movie_audio[slot].data=data;movie_audio[slot].capacity=size;
    }
    memcpy(movie_audio[slot].data,ps2_ram+dst+offset,size);
    movie_audio[slot].bytes=size;
    ++movie_audio_count;movie_audio_captured+=size/4u;
    if (getenv("PS2_AC4_MOVIE_LIFETIME_TRACE"))
        ps2_log("movie audio owned: field=%llu dst=%08X frames=%u queued_chunks=%u captured=%llu",
            (unsigned long long)ps2_kernel_vblank_count(),dst,size/4u,movie_audio_count,
            (unsigned long long)movie_audio_captured);
}

void ac4_movie_audio_start(u32 src,u32 bytes) {
    if (!src || !bytes) {
        if (movie_audio_active)
            ps2_log("ac4 movie audio: played=%llu captured=%llu underflow=%llu",
                (unsigned long long)movie_audio_played,(unsigned long long)movie_audio_captured,
                (unsigned long long)movie_audio_underflow);
        movie_audio_active=0;
        return;
    }
    movie_audio_active=bytes==32768u && movie_audio_count && !movie_audio_failed;
    if (movie_audio_active) ps2_log("ac4 movie audio: owned PCM enabled (%u chunks)",movie_audio_count);
}

int ac4_movie_audio_sample(s16 value[2]) {
    if (!movie_audio_active) return 0;
    if (!movie_audio_count) {
        value[0]=value[1]=0; ++movie_audio_underflow;
        return 1; /* EOF/fade silence; never replay stale ring contents. */
    }
    u32 sample=movie_audio_sample_pos++,slot=movie_audio_head;
    u32 off=(sample/256u)*1024u+(sample%256u)*2u;
    memcpy(&value[0],movie_audio[slot].data+off,2);
    memcpy(&value[1],movie_audio[slot].data+off+512u,2);
    ++movie_audio_played;
    if (movie_audio_sample_pos==movie_audio[slot].bytes/4u) {
        movie_audio_sample_pos=0;--movie_audio_count;
        movie_audio_head=(movie_audio_head+1u)%MOVIE_AUDIO_SLOTS;
    }
    return 1;
}
