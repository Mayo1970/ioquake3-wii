/*
 * ioquake3-wii: audio/wii_snd.c
 *
 * ASND-based audio backend.
 *
 * ioQ3's software mixer (snd_dma.c / snd_mix.c) fills a DMA ring buffer at
 * 22 050 Hz, 16-bit stereo. We expose that ring directly to ASND: the same
 * physical buffer is used by both the mixer (writes) and ASND (reads).
 *
 * Ring-buffer invariant maintained by ioQ3's mixer:
 *   write head  =  read head  +  submission_chunk  (= 1024 sample-pairs, ~46 ms)
 * ASND reads the ring linearly; the callback re-queues the same buffer so it
 * loops seamlessly. We never swap buffers — dma.buffer always points to s_buf.
 *
 * ASND API quick reference:
 *   ASND_Init()                                        – start audio system
 *   ASND_Pause(0)                                      – resume playback
 *   ASND_SetVoice(voice,fmt,freq,delay,buf,size,vl,vr,cb) – start voice
 *   ASND_AddVoice(voice, buf, size)                    – queue next buffer
 *   ASND_End()                                         – shut down
 *
 * Call chain:
 *   main()  → Wii_Snd_Init()       — ASND_Init + ASND_Pause(0)
 *   S_Init  → SNDDMA_Init()        — alloc ring, set dma struct, start voice
 *   S_Update_ → SNDDMA_BeginPainting() + S_PaintChannels() + SNDDMA_Submit()
 */

#include <asndlib.h>
#include <ogc/cache.h>
#include <ogc/lwp_watchdog.h>   /* gettime(), ticks_to_millisecs() */
#include <string.h>
#include <stdio.h>
#include <malloc.h>

#include "wii_snd.h"
#include "client/snd_local.h"   /* dma_t */

/* --------------------------------------------------------------------------
 * Configuration
 * -------------------------------------------------------------------------- */
#define SND_VOICE       0
#define SND_FREQ        22050
#define SND_CHANNELS    2
#define SND_SAMPLEBITS  16
/* Ring buffer size in sample-pairs (per channel).  Power of two, ~93 ms at
 * 22 kHz.  submission_chunk = SND_SAMPLES/2 = 1024 pairs (~46 ms ahead). */
#define SND_SAMPLES     2048
#define SND_BYTES       (SND_SAMPLES * SND_CHANNELS * (SND_SAMPLEBITS / 8))

/* --------------------------------------------------------------------------
 * State
 * -------------------------------------------------------------------------- */
static qboolean s_snd_init   = qfalse;
static qboolean s_asnd_ready = qfalse;
static qboolean s_first_submit = qtrue;

/*
 * Single ring buffer — dma.buffer always points here.
 * ioQ3's mixer writes into it as a ring; ASND reads it linearly and loops.
 */
static u8 *s_buf = NULL;

/*
 * Gekko TB timestamp (lower 32 bits) captured at the start of each ASND
 * pass.  SNDDMA_GetDMAPos() uses this to estimate the hardware read position
 * within the current pass without relying on Com_Milliseconds() (which
 * drifts with CPU load).
 */
static volatile u32 s_cb_ticks = 0;

/*
 * Voice callback — IRQ context.
 * Re-queue the same ring buffer so ASND loops seamlessly, then record the
 * TB tick so GetDMAPos can interpolate within the new pass.
 */
static void SndCallback(s32 voice)
{
    (void)voice;
    ASND_AddVoice(SND_VOICE, s_buf, SND_BYTES);
    s_cb_ticks = (u32)gettime();    /* sample: lower 32 bits, wraps ~52 s */
}

/* --------------------------------------------------------------------------
 * Wii_Snd_Init — called from main() before Com_Init.
 * -------------------------------------------------------------------------- */
void Wii_Snd_Init(void)
{
    ASND_Init();
    ASND_Pause(0);
    s_asnd_ready = qtrue;
}

void Wii_Snd_Shutdown(void)
{
    if (s_snd_init) {
        ASND_StopVoice(SND_VOICE);
        if (s_buf) { free(s_buf); s_buf = NULL; }
        s_snd_init = qfalse;
    }
    if (s_asnd_ready) {
        ASND_End();
        s_asnd_ready = qfalse;
    }
}

/* --------------------------------------------------------------------------
 * SNDDMA_Init
 * -------------------------------------------------------------------------- */
qboolean SNDDMA_Init(void)
{
    if (!s_asnd_ready)
        return qfalse;

    s_buf = (u8 *)memalign(32, SND_BYTES);
    if (!s_buf)
        return qfalse;
    memset(s_buf, 0, SND_BYTES);

    /*
     * Describe the ring to ioQ3's mixer.
     * dma.buffer is fixed — it never changes after this point.
     * submission_chunk = SND_SAMPLES/2: the mixer paints ~1024 sample-pairs
     * per call, always staying one half-ring ahead of the ASND read head.
     */
    dma.samplebits       = SND_SAMPLEBITS;
    dma.isfloat          = 0;
    dma.speed            = SND_FREQ;
    dma.channels         = SND_CHANNELS;
    dma.samples          = SND_SAMPLES * SND_CHANNELS; /* total interleaved */
    dma.fullsamples      = SND_SAMPLES;                /* sample-pairs      */
    dma.submission_chunk = SND_SAMPLES / 2;            /* 1024 pairs        */
    dma.buffer           = s_buf;

    /* Record start time before the first ASND pass begins. */
    s_cb_ticks = (u32)gettime();

    ASND_SetVoice(SND_VOICE,
                  VOICE_STEREO_16BIT,
                  SND_FREQ,
                  0,            /* delay ms */
                  s_buf, SND_BYTES,
                  255, 255,     /* left/right volume (full) */
                  SndCallback);

    s_snd_init = qtrue;
    return qtrue;
}

/* --------------------------------------------------------------------------
 * SNDDMA_GetDMAPos — hardware read position in interleaved samples.
 *
 * ioQ3's S_GetSoundtime (snd_dma.c) computes:
 *
 *   s_soundtime = buffers * dma.fullsamples + samplepos / dma.channels
 *
 * so samplepos must be in [0, dma.fullsamples * dma.channels) = [0, 4096).
 * Returning [0, 2048) (sample-pairs) halves the effective range and causes
 * s_soundtime to jump by ~1026 at every wrap — the primary cause of choppiness.
 *
 * We estimate the read position using the Gekko TB register captured at the
 * last ASND callback (start of the current pass).  This avoids the drift that
 * accumulates when using Com_Milliseconds() (which varies with frame load).
 * -------------------------------------------------------------------------- */
int SNDDMA_GetDMAPos(void)
{
    if (!s_snd_init)
        return 0;

    u32 ticks_since  = (u32)gettime() - s_cb_ticks;
    u32 ms_since     = (u32)ticks_to_millisecs((u64)ticks_since);

    /* Convert elapsed ms → sample-pairs consumed in this pass. */
    u32 pairs = ms_since * (u32)SND_FREQ / 1000u;
    if (pairs >= (u32)SND_SAMPLES)
        pairs = (u32)SND_SAMPLES - 1u;

    /*
     * Return interleaved sample count so S_GetSoundtime's  /dma.channels
     * division yields the correct sample-pair offset in [0, SND_SAMPLES).
     */
    return (int)(pairs * (u32)SND_CHANNELS);
}

/* --------------------------------------------------------------------------
 * SNDDMA_BeginPainting — point ioQ3's mixer at the DMA ring.
 *
 * dma.buffer is always s_buf; nothing to flip.
 * -------------------------------------------------------------------------- */
void SNDDMA_BeginPainting(void)
{
    /* no-op: dma.buffer never changes */
}

/* --------------------------------------------------------------------------
 * SNDDMA_Submit — flush the ring so ASND DMA sees freshly mixed samples.
 *
 * ASND reads physical RAM directly (bypassing dcache), so we must writeback
 * the entire ring after the mixer writes.  We do NOT call ASND_AddVoice here;
 * SndCallback handles re-queuing at the hardware boundary.
 * -------------------------------------------------------------------------- */
void SNDDMA_Submit(void)
{
    if (!s_snd_init)
        return;

    DCFlushRange(s_buf, SND_BYTES);
}

/* --------------------------------------------------------------------------
 * SNDDMA_Shutdown
 * -------------------------------------------------------------------------- */
void SNDDMA_Shutdown(void)
{
    Wii_Snd_Shutdown();
}

/* --------------------------------------------------------------------------
 * Backward-compat wrappers (wii_snd.h declarations).
 * -------------------------------------------------------------------------- */
qboolean Wii_Snd_SNDDMA_Init(void)         { return SNDDMA_Init(); }
int      Wii_Snd_SNDDMA_GetDMAPos(void)    { return SNDDMA_GetDMAPos(); }
void     Wii_Snd_SNDDMA_BeginPainting(void){ SNDDMA_BeginPainting(); }
void     Wii_Snd_SNDDMA_Submit(void)       { SNDDMA_Submit(); }
void     Wii_Snd_SNDDMA_Shutdown(void)     { SNDDMA_Shutdown(); }
