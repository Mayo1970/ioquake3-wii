/*
 * ioquake3-wii: audio/wii_snd.h
 * ASND-based audio backend declarations.
 */
#pragma once
#include "qcommon/q_shared.h"

/* Defined in wii_main.c; available after fatInitDefault() succeeds. */
extern void boot_mark(const char *msg);

void Wii_Snd_Init(void);
void Wii_Snd_Shutdown(void);

/* Called from ioQ3's SNDDMA_* interface (see wii_snd.c) */
qboolean Wii_Snd_SNDDMA_Init(void);
int      Wii_Snd_SNDDMA_GetDMAPos(void);
void     Wii_Snd_SNDDMA_BeginPainting(void);
void     Wii_Snd_SNDDMA_Submit(void);
void     Wii_Snd_SNDDMA_Shutdown(void);
