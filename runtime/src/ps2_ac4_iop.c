#include "ps2_runtime.h"
#include "ps2_hle.h"
#include <math.h>
#include <stdio.h>
#include <SDL3/SDL.h>
int ps2_memcard_rpc(ps2_ctx *,u32,u32,int,u32,int);
static void zero_reply(u32 recv,int size) {
    u32 p=recv&0x1fffffffu;
    if(size<0 || !recv || p>=PS2_RAM_SIZE || (u32)size>PS2_RAM_SIZE-p)
        ps2_fatal("AC4_IOP_BAD_REPLY addr=%08X size=%d",recv,size);
    for(int i=0;i<size;++i) ps2_w8(recv+(u32)i,0);
}
static int ac4_usb_rpc(ps2_ctx *ctx,u32 fno,u32 send,int ssize,u32 recv,int rsize) {
    (void)ctx;(void)send;
    if(fno==2 && ssize==128 && rsize==128) { zero_reply(recv,rsize); return 0; }
    ps2_fatal("AC4_UNSUPPORTED_USB fno=%u sizes=%d/%d",fno,ssize,rsize);
    return -1;
}
/* SOUND.IRX packet fields follow sifrpc_server/set_tone_attr_all. */
void ps2_spu2_command_lock(void);
void ps2_spu2_command_unlock(void);
void ps2_spu2_write(u32, u32, u32);
void ps2_spu2_set_voice(u32, u32, u32, u32, u32, u32, u32, int);
void ps2_spu2_stream_volume(u32, u32, u32);
void ps2_spu2_voice_pitch(u32, u32);
void ps2_spu2_key_on_mask(u32, u32);
void ps2_spu2_key_off_mask(u32, u32);
void ps2_spu2_key_off(u32);
u8 ps2_spu2_voice_status(u32);
void ps2_spu2_master_volume(u32, s32, s32);

static s32 ac4_voice_volume[48][2];
static s32 ac4_master[2][2] = {{32766,32766},{32766,32766}};
static s32 ac4_data_volume[2];
static FILE *ac4_pcm_trace;
static u32 ac4_stream_voice[4] = {42,44,46,47};
static u32 ac4_data_src, ac4_data_size, ac4_data_position, ac4_vag_mask;
u64 ps2_kernel_vblank_count(void);
static void ac4_vag_produce(u64);

static u32 ac4_note_pitch(u32 p) {
    int center = ps2_r8(p + 0x30), fine = ps2_r8(p + 0x31);
    int note = ps2_r8(p + 0x32), bend = (s8)ps2_r8(p + 0x33);
    double semitones = note - center + (bend + fine) / 128.0;
    double pitch = 4096.0 * exp2(semitones / 12.0) * ps2_r16(p + 0x3A) / 48000.0;
    return pitch > 16383.0 ? 16383u : (pitch > 0 ? (u32)pitch : 0u);
}

static u32 ac4_volume(u32 p) {
    int v = (s16)ps2_r16(p);
    if (v > 16383) v = 16383;
    if (v < -16384) v = -16384;
    return (u32)v & 0x7FFFu;
}

static void ac4_sound_voice(u32 cmd, u32 p) {
    u32 voice = ps2_r8(p + 14);
    if (voice >= 48) return;
    if (cmd == 1 || cmd == 3 || (cmd == 2 && (ps2_r32(p+0x10) & 1))) {
        ac4_voice_volume[voice][0] = (s16)(ac4_volume(p+0x2C) << 1);
        ac4_voice_volume[voice][1] = (s16)(ac4_volume(p+0x2E) << 1);
    }
    if (getenv("PS2_AC4_AUDIO_CONTROL_TRACE") && voice >= 42)
        ps2_log("ac4 audio control: field=%llu voice=%u cmd=%u mask=%08X pitch=%u note_pitch=%u volume=%d/%d",
            (unsigned long long)ps2_kernel_vblank_count(), voice, cmd, ps2_r32(p+0x10),
            ps2_r16(p+0x38), ac4_note_pitch(p), ac4_voice_volume[voice][0], ac4_voice_volume[voice][1]);
    ps2_spu2_command_lock();
    if (cmd == 1 || cmd == 3) {
        ps2_spu2_set_voice(voice, ps2_r32(p + 0x40), ac4_note_pitch(p),
            ps2_r16(p + 0x24), ps2_r16(p + 0x26),
            ac4_volume(p + 0x2C), ac4_volume(p + 0x2E), 0);
        ps2_spu2_key_on_mask(voice < 24 ? 1u << voice : 0,
                            voice >= 24 ? 1u << (voice - 24) : 0);
    } else if (cmd == 2) {
        u32 mask = ps2_r32(p + 0x10);
        if (mask & 1u) ps2_spu2_stream_volume(voice, ac4_volume(p + 0x2C), ac4_volume(p + 0x2E));
        if (mask & 2u) ps2_spu2_voice_pitch(voice, ps2_r16(p + 0x38));
        if (mask & 4u) ps2_spu2_voice_pitch(voice, ac4_note_pitch(p));
    } else if (cmd == 4) ps2_spu2_key_off(voice);
    ps2_spu2_command_unlock();
}

static u32 ac4_sound_mode,ac4_sound_transfer;
/* LIBSD reserves effect RAM below each core's EEA (0x1DFFFF/0x1FFFFF).
 * SOUND's command 6 returns the lower ESA to the EE sample allocator. */
static u32 ac4_effect_start[2] = {0x1DFFF0u, 0x1FFFF0u};
static const u32 ac4_effect_words[10] = {
    1, 0x26C, 0x1F4, 0x484, 0x6FE, 0xADE, 0xF6C, 0x1804, 0x1804, 0x3C0
};
static int ac4_sound_rpc(ps2_ctx *ctx,u32 fno,u32 send,int ssize,u32 recv,int rsize) {
    (void)ctx;
    if (fno>1 || ssize!=9216 || rsize!=1536 || !send || !recv)
        ps2_fatal("AC4_UNSUPPORTED_SOUND fno=%u sizes=%d/%d",fno,ssize,rsize);
    ac4_vag_produce(ps2_kernel_vblank_count());
    u32 n=fno?ps2_r32(send):0;
    if(n>128) ps2_fatal("AC4_SOUND_PACKET_COUNT %u",n);
    zero_reply(recv,rsize);
    u32 in=4,out=4,replies=0;
    if(!fno) { ac4_sound_mode=0; ac4_sound_transfer=0; ac4_effect_start[0]=0x1DFFF0u; ac4_effect_start[1]=0x1FFFF0u; }
    static const u32 sizes[15]={0,72,72,72,72,12,16,16,16,16,20,12,12,12,8};
    for(u32 i=0;i<n;++i) {
        if(in+4>(u32)ssize) ps2_fatal("AC4_SOUND_PACKET_TRUNCATED");
        u32 cmd=ps2_r32(send+in);
        if(cmd>14 || in+sizes[cmd]>(u32)ssize) ps2_fatal("AC4_SOUND_COMMAND %u",cmd);
        if (getenv("PS2_AC4_SOUND_TRACE")) {
            static u32 seen[15];
            if (seen[cmd]++ < 12) ps2_log("ac4 sound: cmd=%u data=%08X %08X %08X voice=%u", cmd,
                ps2_r32(send+in+4), ps2_r32(send+in+8), ps2_r32(send+in+12), ps2_r8(send+in+14));
        }
        if(cmd>=1 && cmd<=4) {
            ac4_sound_voice(cmd, send + in);
            ps2_w32(recv+out,cmd); ps2_w32(recv+out+4,ps2_r32(send+in+4));
            ps2_w8(recv+out+8,ps2_r8(send+in+14)); ps2_w8(recv+out+9,1);
            out+=12;replies++;
        } else if(cmd>=6 && cmd<=9) {
            if (cmd == 6) {
                u32 core = ps2_r8(send+in+4), mode = ps2_r32(send+in+8) & 0xFFu;
                if (core < 2 && mode < 10) {
                    ac4_effect_start[core] = (core ? 0x200000u : 0x1E0000u) - (ac4_effect_words[mode] << 4);
                    ps2_w32(recv+out+4, ac4_effect_start[0] < ac4_effect_start[1] ? ac4_effect_start[0] : ac4_effect_start[1]);
                }
            }
            ps2_w32(recv+out,cmd);ps2_w8(recv+out+8,1);out+=12;replies++;
        } else if(cmd==5) {
            ps2_spu2_command_lock();
            ps2_spu2_key_off_mask(ps2_r32(send+in+4), ps2_r32(send+in+8));
            ps2_spu2_command_unlock();
        } else if(cmd==10) {
            u32 src=ps2_r32(send+in+4), dst=ps2_r32(send+in+8), bytes=ps2_r32(send+in+12);
            if (dst && src < PS2_IOP_RAM_SIZE && bytes <= PS2_IOP_RAM_SIZE - src) {
                ps2_spu2_command_lock();
                ps2_spu2_write(dst, PS2_IOP_RAM_BASE + src, bytes);
                ps2_spu2_command_unlock();
            }
            if (!dst) {
                if(getenv("PS2_AC4_AUDIO_CONTROL_TRACE"))
                    ps2_log("ac4 movie control: field=%llu src=%08X bytes=%u old_position=%u transfer=%u",
                        (unsigned long long)ps2_kernel_vblank_count(),src,bytes,ac4_data_position,ps2_r32(send+in+16));
                ac4_data_src = src;
                ac4_data_size = src && src < PS2_IOP_RAM_SIZE && bytes <= PS2_IOP_RAM_SIZE-src && !(bytes & 2047u) ? bytes : 0;
                ac4_data_position = 0;
                extern void ac4_movie_audio_start(u32,u32);
                ac4_movie_audio_start(ac4_data_src,ac4_data_size);
            }
            ac4_sound_transfer=ps2_r32(send+in+16);
        } else if (cmd == 11) {
            u32 core = ps2_r8(send+in+4);
            if (core < 2) {
                ac4_master[core][0] = (s16)(ac4_volume(send+in+6) << 1);
                ac4_master[core][1] = (s16)(ac4_volume(send+in+8) << 1);
                ps2_spu2_command_lock();
                ps2_spu2_master_volume(core, ac4_master[core][0], ac4_master[core][1]);
                ps2_spu2_command_unlock();
            }
        } else if (cmd == 13 && ps2_r8(send+in+4) == 0) {
            ac4_data_volume[0] = (s16)ps2_r16(send+in+6);
            ac4_data_volume[1] = (s16)ps2_r16(send+in+8);
            if(getenv("PS2_AC4_AUDIO_CONTROL_TRACE"))
                ps2_log("ac4 movie volume: field=%llu volume=%d/%d position=%u",
                    (unsigned long long)ps2_kernel_vblank_count(),ac4_data_volume[0],ac4_data_volume[1],ac4_data_position);
        } else if(cmd==14) ac4_sound_mode=ps2_r32(send+in+4);
        in+=sizes[cmd];
        if(out+68>(u32)rsize) ps2_fatal("AC4_SOUND_REPLY_OVERFLOW");
    }
    ps2_w32(recv+out,10);ps2_w32(recv+out+4,ac4_sound_transfer);
    ps2_w32(recv+out+8, ac4_data_size ? (ac4_data_position * 4u / (ac4_data_size / 2u)) & 1u : 0);
    ps2_w32(recv+out+12,15);ps2_w32(recv+out+16,ac4_sound_mode);
    ps2_spu2_command_lock();
    for (u32 v = 0; v < 48; ++v) {
        u8 status = ps2_spu2_voice_status(v);
        ps2_w8(recv+out+20+v, status == 2 ? 4 : status ? 1 : 0);
    }
    ps2_spu2_command_unlock();
    ps2_w32(recv,replies+2);
    return 0;
}

u32 ps2_ac4_iop_alloc(u32 size);
static u32 ac4_vag_buffer;
u64 ps2_kernel_vblank_count(void);
/* Stream positions count ADPCM bytes per channel. Reply offsets, 2 KiB
 * interleave and triple-buffer layout follow VAG.IRX VagMain/VagAllInit. */
static struct {
    u32 size, pitch, position, status, channel, channels, decoded, first_ring;
    u64 tick, remainder, phase;
    u32 ready, drain_end;
    int h1[2], h2[2];
    s16 pcm[2][28];
} ac4_stream[4];

void ps2_spu2_decode_block(const u8 *, s16 *, int *, int *, u32 *);
#define AC4_PCM_FRAMES 131072u
static s16 ac4_pcm[AC4_PCM_FRAMES][2];
/* Guest-owned staging: callback never waits for ADPCM decode or trace I/O. */
static s16 ac4_decode_pcm[96096][2];
static u64 ac4_pcm_read, ac4_pcm_write, ac4_audio_tick;
static SDL_SpinLock ac4_pcm_lock;
static u32 ac4_sample_remainder;
/* Queue telemetry is protected by ac4_pcm_lock, including callback counters. */
static u64 ac4_sync_produced, ac4_sync_consumed, ac4_sync_starved, ac4_sync_overflow;
static u64 ac4_sync_peak, ac4_sync_max_fields, ac4_sync_last_report;
static int ac4_sync_active;
static int ac4_sync_trace = -1;
static void ac4_sync_report(u64 tick) {
    if (ac4_sync_trace < 0) ac4_sync_trace = getenv("PS2_AC4_AUDIO_SYNC_TRACE") != NULL;
    if (!ac4_sync_trace || tick - ac4_sync_last_report < 300u) return;
    ac4_sync_last_report = tick;
    SDL_LockSpinlock(&ac4_pcm_lock);
    u64 queued=ac4_pcm_write-ac4_pcm_read, peak=ac4_sync_peak;
    u64 produced=ac4_sync_produced, consumed=ac4_sync_consumed;
    u64 starved=ac4_sync_starved, overflow=ac4_sync_overflow, gap=ac4_sync_max_fields;
    SDL_UnlockSpinlock(&ac4_pcm_lock);
    ps2_log("ac4 audio sync: field=%llu queued_ms=%.3f peak_ms=%.3f produced=%llu consumed=%llu active_starved_ms=%.3f overflow=%llu max_producer_gap_fields=%llu",
        (unsigned long long)tick, queued/48.0, peak/48.0,
        (unsigned long long)produced, (unsigned long long)consumed,
        starved/48.0, (unsigned long long)overflow, (unsigned long long)gap);
}

static s16 ac4_clip_sample(int v) { return (s16)(v > 32767 ? 32767 : v < -32768 ? -32768 : v); }

/* Guest-thread reset; the callback sees only the locked PCM queue and SPU. */
void ps2_ac4_audio_reset(void) {
    if (ac4_pcm_trace) { fclose(ac4_pcm_trace); ac4_pcm_trace=NULL; }
    ac4_vag_buffer=ac4_vag_mask=0;
    ac4_data_src=ac4_data_size=ac4_data_position=0;
    extern void ac4_movie_audio_reset(void);
    ac4_movie_audio_reset();
    ac4_sound_mode=ac4_sound_transfer=0;
    ac4_effect_start[0]=0x1DFFF0u;ac4_effect_start[1]=0x1FFFF0u;
    memset(ac4_stream,0,sizeof ac4_stream);
    memset(ac4_voice_volume,0,sizeof ac4_voice_volume);
    memset(ac4_data_volume,0,sizeof ac4_data_volume);
    static const u32 voices[4]={42,44,46,47};
    memcpy(ac4_stream_voice,voices,sizeof voices);
    ac4_audio_tick=ps2_kernel_vblank_count();
    ac4_sample_remainder=0;
    SDL_LockSpinlock(&ac4_pcm_lock);
    ac4_pcm_read=ac4_pcm_write=0;
    ac4_sync_produced=ac4_sync_consumed=ac4_sync_starved=ac4_sync_overflow=0;
    ac4_sync_peak=ac4_sync_max_fields=ac4_sync_last_report=0;
    ac4_sync_active=0;
    SDL_UnlockSpinlock(&ac4_pcm_lock);
    ps2_spu2_command_lock();
    void ps2_spu2_reset_voices(void);
    ps2_spu2_reset_voices();
    for(u32 c=0;c<2;++c) {
        ac4_master[c][0]=ac4_master[c][1]=32766;
        ps2_spu2_master_volume(c,32766,32766);
    }
    ps2_spu2_command_unlock();
}

/* Produce from the guest's clock and IOP ring on the guest thread. The host
 * audio callback reads only this PCM queue, never concurrently changing IOP RAM. */
static void ac4_vag_produce(u64 tick) {
    u64 fields = tick - ac4_audio_tick;
    ac4_audio_tick = tick;
    if (fields > 120) fields = 120;
    /* NTSC fields are 1001/60000 seconds: 800.8 samples at 48 kHz.
     * Carry the fraction across RPCs, independently of host benchmark pacing. */
    u64 samples=fields*4004u+ac4_sample_remainder;
    u32 frames=(u32)(samples/5u);
    ac4_sample_remainder=(u32)(samples%5u);
    const char *pcm_trace_path=getenv("PS2_AC4_PCM_TRACE");
    int active = ac4_data_size != 0;
    for (u32 j=0;j<4;j++)
        if (ac4_stream[j].status>1 && ac4_stream[j].ready) active=1;
    /* The host callback already supplies silence when this queue is empty.
     * Re-queuing elapsed idle silence delays the next cue (about 180 ms at
     * boot). Keep the guest clock/fraction advancing, but enqueue only when
     * a VAG stream or AutoDMA source is active. Existing audible tails drain. */
    if (!active) frames = 0;
    for (u32 f = 0; f < frames; ++f) {
        int mix[2] = {0, 0};
        for (u32 j = 0; j < 4; ++j) {
            if (ac4_stream[j].status <= 1 || !ac4_stream[j].ready) continue;
            u64 sample = ac4_stream[j].phase >> 12;
            u32 block = (u32)(sample / 28), byte = block * 16u;
            if (ac4_stream[j].ready==2 && byte>=ac4_stream[j].drain_end) {
                ac4_stream[j].ready=0;
                continue;
            }
            u32 channels = ac4_stream[j].channels;
            if (byte >= ac4_stream[j].size || !channels || channels > 2) continue;
            if (ac4_stream[j].decoded != block + 1u) {
                u32 ch = ac4_stream[j].channel;
                u32 base = ac4_vag_buffer + 0x180u + (ch < 2 ? ch * 0x60000u : (ch - 2) * 0x30000u);
                u32 local = byte % 65536u;
                u32 ring = (ac4_stream[j].first_ring + byte / 65536u) % 3u;
                for (u32 c = 0; c < channels; ++c) {
                    u32 off = base + ring * (ch < 2 ? 0x20000u : 0x10000u)
                        + (local / 2048u) * 2048u * channels + c * 2048u + local % 2048u;
                    if (off > PS2_IOP_RAM_SIZE - 16u) { memset(ac4_stream[j].pcm[c], 0, sizeof ac4_stream[j].pcm[c]); continue; }
                    ps2_spu2_decode_block(ps2_iop_ram + off, ac4_stream[j].pcm[c],
                        &ac4_stream[j].h1[c], &ac4_stream[j].h2[c], NULL);
                }
                ac4_stream[j].decoded = block + 1u;
            }
            u32 voice = ac4_stream_voice[j];
            for (u32 c = 0; c < 2; ++c) {
                s32 gain = ac4_voice_volume[voice + (channels == 2 ? c : 0)][c];
                s32 value = (ac4_stream[j].pcm[c && channels == 2][sample % 28] * gain) >> 15;
                mix[c] += (value * ac4_master[1][c]) >> 15;
            }
            ac4_stream[j].phase += ac4_stream[j].pitch;
        }
        if (ac4_data_size) {
            /* AutoDMA PCM is split into 256 left samples then 256 right
             * samples. The EE refills alternating halves of this ring. */
            u32 sample = ac4_data_position++ % (ac4_data_size / 4u);
            u32 off = ac4_data_src + (sample / 256u) * 1024u + (sample % 256u) * 2u;
            s16 value[2];
            extern int ac4_movie_audio_sample(s16 [2]);
            if (!ac4_movie_audio_sample(value)) {
                memcpy(&value[0],ps2_iop_ram+off,2);
                memcpy(&value[1],ps2_iop_ram+off+512u,2);
            }
            if (pcm_trace_path) {
                if (!ac4_pcm_trace) ac4_pcm_trace=fopen(pcm_trace_path,"wb");
                if (ac4_pcm_trace) {
                    fwrite(value,sizeof(value),1,ac4_pcm_trace);
                }
            }
            for (u32 c = 0; c < 2; ++c) {
                s32 scaled = (value[c] * ac4_data_volume[c]) >> 15;
                scaled = (scaled * ac4_master[0][c]) >> 15;
                mix[c] += (scaled * ac4_master[1][c]) >> 15;
            }
        }
        ac4_decode_pcm[f][0] = ac4_clip_sample(mix[0]);
        ac4_decode_pcm[f][1] = ac4_clip_sample(mix[1]);
    }
    SDL_LockSpinlock(&ac4_pcm_lock);
    ac4_sync_active=active;
    if (fields>ac4_sync_max_fields) ac4_sync_max_fields=fields;
    ac4_sync_produced += frames;
    for (u32 f=0;f<frames;f++) {
        if (ac4_pcm_write-ac4_pcm_read>=AC4_PCM_FRAMES) { ++ac4_pcm_read; ++ac4_sync_overflow; }
        u32 out=(u32)(ac4_pcm_write++ % AC4_PCM_FRAMES);
        ac4_pcm[out][0]=ac4_decode_pcm[f][0];
        ac4_pcm[out][1]=ac4_decode_pcm[f][1];
    }
    if (ac4_pcm_write-ac4_pcm_read > ac4_sync_peak) ac4_sync_peak=ac4_pcm_write-ac4_pcm_read;
    SDL_UnlockSpinlock(&ac4_pcm_lock);
    ac4_sync_report(tick);
    for(u32 j=0;j<4;++j) {
        u64 pos=ac4_stream[j].phase*16u/(4096u*28u);
        if(pos>=ac4_stream[j].size) {
            pos=ac4_stream[j].size;
            if(ac4_stream[j].status>1) ac4_stream[j].status=1;
        }
        ac4_stream[j].position=(u32)pos;
    }
}

/* Optional diagnostics before SIF overwrites an active movie ring. */
void ps2_ac4_movie_refill_trace(u32 src,u32 dst,u32 bytes) {
    if (!ac4_data_size || dst >= ac4_data_src+ac4_data_size ||
        dst+bytes <= ac4_data_src) return;
    ps2_log("ac4 movie refill: field=%llu src=%08X dst=%08X bytes=%u position=%u half=%u",
        (unsigned long long)ps2_kernel_vblank_count(),src,dst,bytes,ac4_data_position,
        (ac4_data_position*4u/(ac4_data_size/2u))&1u);
}

/* Finish elapsed playback before a guest transfer replaces ring contents. */
void ps2_ac4_audio_flush(void) {
    ac4_vag_produce(ps2_kernel_vblank_count());
}

void ps2_ac4_audio_mix(s16 *out, u32 frames) {
    SDL_LockSpinlock(&ac4_pcm_lock);
    u64 available=ac4_pcm_write-ac4_pcm_read;
    ac4_sync_consumed += available < frames ? available : frames;
    if (ac4_sync_active && available < frames) ac4_sync_starved += frames-available;
    for (u32 i = 0; i < frames && ac4_pcm_read < ac4_pcm_write; ++i) {
        u32 pos = (u32)(ac4_pcm_read++ % AC4_PCM_FRAMES);
        out[2*i] = ac4_clip_sample((int)out[2*i] + ac4_pcm[pos][0]);
        out[2*i+1] = ac4_clip_sample((int)out[2*i+1] + ac4_pcm[pos][1]);
    }
    SDL_UnlockSpinlock(&ac4_pcm_lock);
}

static int ac4_vag_rpc(ps2_ctx *ctx,u32 fno,u32 send,int ssize,u32 recv,int rsize) {
    (void)ctx;
    if(fno>1 || ssize!=256 || rsize!=256 || !send || !recv)
        ps2_fatal("AC4_UNSUPPORTED_VAG fno=%u sizes=%d/%d first=%08X",fno,ssize,rsize,send?ps2_r32(send):0);
    u64 tick=ps2_kernel_vblank_count();
    if(fno==0) { ac4_vag_buffer=ps2_ac4_iop_alloc(0xc0180); memset(ac4_stream,0,sizeof ac4_stream);
        for (u32 j=0;j<4;++j) {
            u32 voice=ps2_r32(send+4*j);
            ac4_stream[j].status=1;
            ac4_stream[j].first_ring=0;
            ac4_stream_voice[j]=voice < (j<2?23u:24u) ? 24u+voice : 24u;
        }
        ac4_audio_tick=tick; ac4_sample_remainder=0; SDL_LockSpinlock(&ac4_pcm_lock); ac4_pcm_read=ac4_pcm_write=0; SDL_UnlockSpinlock(&ac4_pcm_lock); }
    if(!ac4_vag_buffer) ps2_fatal("AC4_VAG_ALLOCATION_FAILED");
    u32 count=fno?ps2_r32(send):0,mask=fno?ps2_r32(send+4):0;
    if(count>12) ps2_fatal("AC4_VAG_PACKET_COUNT %u",count);
    if (getenv("PS2_AC4_AUDIO_CONTROL_TRACE") && (mask != ac4_vag_mask || count)) {
        SDL_LockSpinlock(&ac4_pcm_lock);
        u32 queued=(u32)(ac4_pcm_write-ac4_pcm_read);
        SDL_UnlockSpinlock(&ac4_pcm_lock);
        ps2_log("ac4 vag mask: field=%llu old=%02X new=%02X queued=%u position=%u/%u/%u/%u",
            (unsigned long long)tick,ac4_vag_mask,mask,queued,ac4_stream[0].position,
            ac4_stream[1].position,ac4_stream[2].position,ac4_stream[3].position);
    }
    /* Complete the interval under the previous controls. VAG.IRX VagCtrl
     * moves readiness 1 -> 2 on mask removal; VagMain drains the current
     * 64-KiB IOP buffer before setting it to 0. This is refill permission,
     * not an immediate mute/pause command. */
    ac4_vag_produce(tick);
    ac4_vag_mask=mask;
    for(u32 j=0;j<4;++j) {
        if(mask&(1u<<ac4_stream[j].channel)) ac4_stream[j].ready=1;
        else if(ac4_stream[j].ready==1) {
            ac4_stream[j].ready=2;
            ac4_stream[j].drain_end=(ac4_stream[j].position/65536u+1u)*65536u;
        }
    }
    for(u32 i=0;i<count;++i) {
        u32 packet=send+8+20*i,cmd=ps2_r32(packet),ch=ps2_r32(packet+4);
        if(ch>5) ps2_fatal("AC4_VAG_CHANNEL %u",ch);
        u32 j=ch>=4?ch-2:ch;
        if (getenv("PS2_AC4_AUDIO_CONTROL_TRACE"))
            ps2_log("ac4 vag control: field=%llu cmd=%u ch=%u data=%u/%u/%u mask=%02X",
                (unsigned long long)tick,cmd,ch,ps2_r32(packet+8),ps2_r32(packet+12),ps2_r32(packet+16),mask);
        /* VAG.IRX begin_PlayVag1 (command 2) shares the start packet
         * with command 1 and enters alternate playback (0x40).
         * Queued SPU crossfades remain TODO. */
        if(cmd==1 || cmd==2) {
            /* Consume the slot advertised to the EE, rather than restarting
             * at physical ring zero (which can contain an unrelated stream).
             * chg_NextIopBuf preserves the ring cursor across stream stops. */
            u32 first_ring=(ac4_stream[j].first_ring+1+ac4_stream[j].position/65536u)%3u;
            memset(&ac4_stream[j], 0, sizeof ac4_stream[j]);
            ac4_stream[j].first_ring=first_ring;
            ac4_stream[j].channels=ps2_r32(packet+12);
            ac4_stream[j].size=ps2_r32(packet+8);ac4_stream[j].pitch=ps2_r32(packet+16);
            ac4_stream[j].position=0;ac4_stream[j].remainder=0;ac4_stream[j].status=cmd==2?0x40:0x20;
            ac4_stream[j].tick=tick;ac4_stream[j].channel=ch;
            ac4_stream[j].ready=(mask>>ch)&1u;
            ps2_log("ac4 vag: field=%llu start cmd=%u ring=%u channel=%u channels=%u bytes=%u pitch=%u mask=%02X voice=%u gain=%d/%d", (unsigned long long)tick,cmd,first_ring,ch,ac4_stream[j].channels,ac4_stream[j].size,ac4_stream[j].pitch,mask,ac4_stream_voice[j],ac4_voice_volume[ac4_stream_voice[j]][0],ac4_voice_volume[ac4_stream_voice[j]][1]);
        } else if(cmd==3) { ac4_stream[j].status=1; }
        else if(cmd==4) { ac4_stream[j].status=0x40; }
        else ps2_fatal("AC4_UNSUPPORTED_VAG_COMMAND %u",cmd);
    }
    zero_reply(recv,rsize); ps2_w32(recv,ac4_vag_buffer);
    for(u32 j=0;j<4;++j) {
        ps2_w32(recv+4+4*j,ac4_stream[j].position);
        ps2_w32(recv+20+4*j,ac4_stream[j].status);
        ps2_w32(recv+36+4*j,(ac4_stream[j].first_ring+1+ac4_stream[j].position/65536)%3);
    }
    if(!fno) ps2_log("ac4 vag: initialization buffer=%08X",ac4_vag_buffer);
    return 0;
}

/* PADMAN v4 128-byte DMA records; guest libpad remains recompiled. */
u16 ps2_pad_buttons(int port,const ps2_pad_state *st);
static u32 ac4_pad_area[2],ac4_pad_stat;
static u8 ac4_pad_mode[2]={1,1};
void ps2_ac4_pad_tick(void) {
    u32 frame=(u32)ps2_kernel_vblank_count()+1;
    for(u32 port=0;port<2;++port) {
        const ps2_pad_state *st=&ps2_pad_host[port];
        if(!ac4_pad_area[port]) continue;
        for(u32 slot=0;slot<2;++slot) {
            u32 b=ac4_pad_area[port]+slot*128;
            zero_reply(b,128);
            if(st->connected) {
                u16 buttons=ps2_pad_buttons((int)port,st);
                ps2_w8(b+1,ac4_pad_mode[port]?0x73:0x41);
                ps2_w8(b+2,(u8)~buttons);ps2_w8(b+3,(u8)(~buttons>>8));
                ps2_w8(b+4,st->rx);ps2_w8(b+5,st->ry);ps2_w8(b+6,st->lx);ps2_w8(b+7,st->ly);
                ps2_w16(b+80,4);ps2_w16(b+82,7);
                ps2_w32(b+96,8);ps2_w8(b+100,2);ps2_w8(b+114,1);ps2_w8(b+101,ac4_pad_mode[port]?0x73:0x41);
                ps2_w8(b+102,3);ps2_w8(b+103,1);ps2_w8(b+104,2);ps2_w8(b+105,ac4_pad_mode[port]);
                ps2_w8(b+109,ac4_pad_mode[port]);ps2_w8(b+112,6);
            }
            ps2_w32(b+88,frame);
        }
    }
    if(ac4_pad_stat) for(u32 slot=0;slot<2;++slot) {
        u32 b=ac4_pad_stat+slot*128;
        ps2_w32(b+4,ac4_pad_area[0]?1:0);ps2_w32(b+8,ac4_pad_area[1]?1:0);ps2_w32(b,frame);
    }
}
static int ac4_pad_rpc(ps2_ctx *ctx,u32 fno,u32 send,int ssize,u32 recv,int rsize) {
    (void)ctx;
    if (fno!=1 || ssize!=128 || rsize!=128 || !send)
        ps2_fatal("AC4_UNSUPPORTED_PAD_RPC fno=%u sizes=%d/%d",fno,ssize,rsize);
    u32 command=ps2_r32(send),port=ps2_r32(send+4),slot=ps2_r32(send+8),area=ps2_r32(send+16),mode=ps2_r32(send+12);
    zero_reply(recv,rsize);
    switch(command) {
    case 18: ps2_w32(recv+12,0x400); break;
    case 16:
        if(area) zero_reply(area,256);
        ac4_pad_stat=area;
        ps2_w32(recv+12,1); break;
    case 1:
        if(port>1 || slot) ps2_fatal("AC4_PAD_PORT port=%u slot=%u",port,slot);
        zero_reply(area,256); ac4_pad_area[port]=area;
        ps2_w32(recv+12,1); ps2_w32(recv+20,ps2_ac4_iop_alloc(256)); break;
    case 6:
        if(port>1 || slot || mode>1) ps2_fatal("AC4_PAD_MODE_INVALID");
        ac4_pad_mode[port]=(u8)mode;ps2_w32(recv+20,1);break;
    case 7: case 8: ps2_w32(recv+20,1);break;
    case 9: ps2_w32(recv+12,0x3ffff);break;
    case 10: ps2_w32(recv+16,1);break;
    case 14: if(port<2) ac4_pad_area[port]=0; ps2_w32(recv+12,1);break;
    case 15: memset(ac4_pad_area,0,sizeof ac4_pad_area);ac4_pad_stat=0;ps2_w32(recv+12,1);break;
    case 12: ps2_w32(recv+12,2); break;
    case 13: ps2_w32(recv+12,1); break;
    default: ps2_fatal("AC4_UNSUPPORTED_PAD_COMMAND %u",command);
    }
    ps2_ac4_pad_tick();
    ps2_log("ac4 pad: command=%u port=%u slot=%u",command,port,slot);
    return 0;
}

void ps2_ac4_module_services(unsigned module) {
    if (module==3) { memset(ac4_pad_area,0,sizeof ac4_pad_area); ac4_pad_stat=0; ps2_rpc_register(0x80000100u,ac4_pad_rpc,"ac4-pad"); ps2_rpc_register(0x80000101u,ac4_pad_rpc,"ac4-pad-secondary"); }
    if (module==8) ps2_rpc_register(0x80000210u,ac4_usb_rpc,"ac4-usb-disconnected");
    if (module==6) ps2_rpc_register(0x81000102u,ac4_vag_rpc,"ac4-vag");
    if (module==5) ps2_rpc_register(0x81000101u,ac4_sound_rpc,"ac4-sound");
    if (module==2) ps2_rpc_register(0x80000400u,ps2_memcard_rpc,"ac4-mcserv");
}
