/*
 * ioquake3-wii: sys/wii_glimp.c
 *
 * GX surface management + minimal GL-over-GX shim.
 *
 * Architecture overview
 * ---------------------
 * ioQuake3's renderer (ref_gl) calls OpenGL. The Wii has no OpenGL driver;
 * it exposes the GX API instead. We have two paths:
 *
 *   PATH A (recommended to start): Use the glGX wrapper library.
 *            glGX translates a useful subset of OpenGL 1.x into GX calls.
 *            Source: https://github.com/Crayon2000/glGX
 *            Add glGX.c / glGX.h to this project and set USE_GLGX=1.
 *
 *   PATH B (full rewrite): Replace ref_gl with a native GX renderer.
 *            More work, better performance. Do this after PATH A is working.
 *
 * This file handles VIDEO / GX init that is needed regardless of path.
 */

#include <gccore.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>

#include "wii_glimp.h"

/* --------------------------------------------------------------------------
 * Constants
 * -------------------------------------------------------------------------- */
#define GX_FIFO_SIZE    (256 * 1024)   /* 256 KB command FIFO */
#define NUM_FRAMEBUFFERS 2

/* --------------------------------------------------------------------------
 * Module state
 * -------------------------------------------------------------------------- */
static GXRModeObj  *s_rmode       = NULL;
static void        *s_framebuf[NUM_FRAMEBUFFERS] = { NULL, NULL };
static void        *s_gp_fifo     = NULL;
static int          s_fb_index    = 0;  /* which framebuffer is being drawn */
static qboolean     s_initialised = qfalse;

/* --------------------------------------------------------------------------
 * Wii_GX_Init
 * -------------------------------------------------------------------------- */
qboolean Wii_GX_Init(void)
{
    if (s_initialised)
        return qtrue;

    /* ---- Video ---- */
    VIDEO_Init();
    s_rmode = VIDEO_GetPreferredMode(NULL);

    /* Allocate two framebuffers for double-buffering */
    s_framebuf[0] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(s_rmode));
    s_framebuf[1] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(s_rmode));

    VIDEO_Configure(s_rmode);
    VIDEO_SetNextFramebuffer(s_framebuf[0]);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if (s_rmode->viTVMode & VI_NON_INTERLACE)
        VIDEO_WaitVSync();

    /* ---- GX ---- */
    s_gp_fifo = memalign(32, GX_FIFO_SIZE);
    if (!s_gp_fifo) {
        printf("[glimp] FATAL: could not allocate GX FIFO\n");
        return qfalse;
    }
    memset(s_gp_fifo, 0, GX_FIFO_SIZE);
    GX_Init(s_gp_fifo, GX_FIFO_SIZE);

    /* Clear to black */
    GXColor bg = { 0, 0, 0, 255 };
    GX_SetCopyClear(bg, 0x00FFFFFF);

    /* Copy-filter / display-copy setup required before ogx_initialize */
    float yscale = GX_GetYScaleFactor(s_rmode->efbHeight, s_rmode->xfbHeight);
    u32   xfbHeight = GX_SetDispCopyYScale(yscale);
    GX_SetScissor(0, 0, s_rmode->fbWidth, s_rmode->efbHeight);
    GX_SetDispCopySrc(0, 0, s_rmode->fbWidth, s_rmode->efbHeight);
    GX_SetDispCopyDst(s_rmode->fbWidth, xfbHeight);
    GX_SetCopyFilter(s_rmode->aa, s_rmode->sample_pattern, GX_TRUE,
                     s_rmode->vfilter);
    GX_SetFieldMode(s_rmode->field_rendering,
                    ((s_rmode->viHeight == 2 * s_rmode->xfbHeight)
                        ? GX_ENABLE : GX_DISABLE));
    GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);
    GX_CopyDisp(s_framebuf[s_fb_index], GX_TRUE);
    GX_SetDispCopyGamma(GX_GM_1_0);

    /* ---- OpenGX ---- */
    /* ogx_initialize() sets up OpenGX's GL→GX translation layer on top of the
     * GX context we just created.  It owns the vertex attribute format, TEV
     * stages, and all GL state — do NOT call GX_ClearVtxDesc / GX_SetVtxDesc
     * after this point or OpenGX's internal assumptions will be violated. */
    extern void ogx_initialize(void);
    ogx_initialize();

    s_initialised = qtrue;
    printf("[glimp] GX + OpenGX initialised (%dx%d)\n",
           s_rmode->fbWidth, s_rmode->efbHeight);
    return qtrue;
}

/* --------------------------------------------------------------------------
 * Wii_GX_BeginFrame
 * -------------------------------------------------------------------------- */
void Wii_GX_BeginFrame(void)
{
    /* OpenGX manages all GL/GX state internally.  The real renderer calls
     * qglClear / qglViewport / etc. at the start of each frame through the
     * OpenGX layer, so there is nothing for us to set up here. */
}

/* --------------------------------------------------------------------------
 * Wii_GX_EndFrame
 * -------------------------------------------------------------------------- */
void Wii_GX_EndFrame(void)
{
    /* Let OpenGX restore any EFB state it needs before we copy to XFB */
    extern int ogx_prepare_swap_buffers(void);
    ogx_prepare_swap_buffers();

    /* Signal GX that drawing is done, then immediately start the EFB→XFB
     * copy.  We do NOT call GX_DrawDone() (synchronous stall) — GX_CopyDisp
     * is queued into the FIFO and executes after all pending draw commands,
     * so the copy happens in hardware while the CPU is already running the
     * next frame's game logic.  We only stall at WaitVSync which we must do
     * to present the finished XFB to the display. */
    GX_SetDrawDone();           /* async "I'm done drawing" marker */
    s_fb_index ^= 1;
    GX_CopyDisp(s_framebuf[s_fb_index], GX_TRUE);  /* EFB → XFB (async) */
    GX_Flush();                 /* push FIFO contents to GP */
    VIDEO_SetNextFramebuffer(s_framebuf[s_fb_index]);
    VIDEO_Flush();
    VIDEO_WaitVSync();          /* only stall: wait for display refresh */
}

/* --------------------------------------------------------------------------
 * Wii_GX_Shutdown
 * -------------------------------------------------------------------------- */
void Wii_GX_Shutdown(void)
{
    if (!s_initialised)
        return;
    GX_AbortFrame();
    GX_Flush();
    VIDEO_SetBlack(TRUE);
    VIDEO_Flush();
    if (s_gp_fifo) { free(s_gp_fifo); s_gp_fifo = NULL; }
    s_initialised = qfalse;
}

/* --------------------------------------------------------------------------
 * Wii_GX_GetRMode
 * -------------------------------------------------------------------------- */
GXRModeObj *Wii_GX_GetRMode(void)
{
    return s_rmode;
}

/* --------------------------------------------------------------------------
 * GX helper stubs — called from wii_gl_stubs.c via GL_Bind, GL_State, etc.
 * These are needed to satisfy the linker even though the full ioQ3 GL backend
 * (tr_backend.c) is not used. In our pipeline GL_Bind / GL_State are compiled
 * but never reached from the active code paths; these stubs ensure the binary
 * links cleanly and won't jump to NULL if they are ever called unexpectedly.
 * -------------------------------------------------------------------------- */

void Wii_GX_BindTexture(int texnum, int tmu)
{
    /* Texture binding is handled directly in wii_renderer.c's draw_quad via
     * GX_LoadTexObj. This stub is here only for the linker. */
    (void)texnum; (void)tmu;
}

void Wii_GX_SetBlend(unsigned int src, unsigned int dst)
{
    /* Blend state is set in GL_State via GX_SetBlendMode directly. */
    (void)src; (void)dst;
}

void Wii_GX_SetDepthTest(int enable)
{
    /* Depth test is set in GL_State via GX_SetZMode directly. */
    (void)enable;
}

void Wii_GX_SetDepthMask(int enable)
{
    /* Depth mask is set in GL_State via GX_SetZMode directly. */
    (void)enable;
}
