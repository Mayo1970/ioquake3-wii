/* ASND-based audio backend for ioquake3-wii. */

#include <asndlib.h>
#include <ogc/cache.h>
#include <ogc/lwp_watchdog.h>   /* gettime(), ticks_to_millisecs() */
#include <string.h>
#include <stdio.h>
#include <malloc.h>

#include "wii_snd.h"
#include "client/snd_local.h"   /* dma_t */

#define SND_VOICE       0
#define SND_FREQ        22050
#define SND_CHANNELS    2
#define SND_SAMPLEBITS  16
#define SND_SAMPLES     2048    /* ~93 ms at 22 kHz; submission_chunk = half */
#define SND_BYTES       (SND_SAMPLES * SND_CHANNELS * (SND_SAMPLEBITS / 8))

static qboolean s_snd_init   = qfalse;
static qboolean s_asnd_ready = qfalse;
static qboolean s_first_submit = qtrue;

/* ioQ3 mixer writes here as a ring; ASND reads it linearly and loops. */
static u8 *s_buf = NULL;

/* TB timestamp at last ASND callback; used by GetDMAPos to estimate read pos. */
static volatile u32 s_cb_ticks = 0;

/* Re-queue ring buffer on ASND boundary, record TB tick for interpolation. */
static void SndCallback(s32 voice)
{
    (void)voice;
    ASND_AddVoice(SND_VOICE, s_buf, SND_BYTES);
    s_cb_ticks = (u32)gettime();    /* sample: lower 32 bits, wraps ~52 s */
}

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

qboolean SNDDMA_Init(void)
{
    if (!s_asnd_ready)
        return qfalse;

    s_buf = (u8 *)memalign(32, SND_BYTES);
    if (!s_buf)
        return qfalse;
    memset(s_buf, 0, SND_BYTES);

    dma.samplebits       = SND_SAMPLEBITS;
    dma.isfloat          = 0;
    dma.speed            = SND_FREQ;
    dma.channels         = SND_CHANNELS;
    dma.samples          = SND_SAMPLES * SND_CHANNELS; /* total interleaved */
    dma.fullsamples      = SND_SAMPLES;                /* sample-pairs      */
    dma.submission_chunk = SND_SAMPLES / 2;            /* 1024 pairs        */
    dma.buffer           = s_buf;

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

/*
 * Estimate hardware read position using TB ticks since last ASND callback.
 * Returns interleaved sample count so S_GetSoundtime's /dma.channels yields
 * the correct sample-pair offset in [0, SND_SAMPLES).
 */
int SNDDMA_GetDMAPos(void)
{
    if (!s_snd_init)
        return 0;

    u32 ticks_since  = (u32)gettime() - s_cb_ticks;
    u32 ms_since     = (u32)ticks_to_millisecs((u64)ticks_since);

    u32 pairs = ms_since * (u32)SND_FREQ / 1000u;
    if (pairs >= (u32)SND_SAMPLES)
        pairs = (u32)SND_SAMPLES - 1u;

    return (int)(pairs * (u32)SND_CHANNELS);
}

void SNDDMA_BeginPainting(void)
{
    /* no-op: dma.buffer never changes */
}

/* Writeback dcache so ASND DMA sees freshly mixed samples. */
void SNDDMA_Submit(void)
{
    if (!s_snd_init)
        return;

    DCFlushRange(s_buf, SND_BYTES);
}

void SNDDMA_Shutdown(void)
{
    Wii_Snd_Shutdown();
}

qboolean Wii_Snd_SNDDMA_Init(void)         { return SNDDMA_Init(); }
int      Wii_Snd_SNDDMA_GetDMAPos(void)    { return SNDDMA_GetDMAPos(); }
void     Wii_Snd_SNDDMA_BeginPainting(void){ SNDDMA_BeginPainting(); }
void     Wii_Snd_SNDDMA_Submit(void)       { SNDDMA_Submit(); }
void     Wii_Snd_SNDDMA_Shutdown(void)     { SNDDMA_Shutdown(); }
