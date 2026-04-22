/*
 * ioquake3-wii: sys/wii_glimp.h
 * GX window / surface management declarations.
 */
#pragma once
#include <gccore.h>
#include "qcommon/q_shared.h"

/* Initialise GX, allocate FIFO, set up the projection stack. */
qboolean Wii_GX_Init(void);

/* Called at the start of every rendered frame. */
void Wii_GX_BeginFrame(void);

/* Swap buffers / flush GX FIFO. */
void Wii_GX_EndFrame(void);

/* Tear down GX (called on shutdown). */
void Wii_GX_Shutdown(void);

/* Returns a pointer to the active GXRModeObj (used by the renderer). */
GXRModeObj *Wii_GX_GetRMode(void);

/* GX helper stubs — satisfy GL_Bind / GL_State calls from wii_gl_stubs.c */
void Wii_GX_BindTexture(int texnum, int tmu);
void Wii_GX_SetBlend(unsigned int src, unsigned int dst);
void Wii_GX_SetDepthTest(int enable);
void Wii_GX_SetDepthMask(int enable);
