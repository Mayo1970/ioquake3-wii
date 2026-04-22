/*
 * wii_gl_stubs.c
 *
 * Two responsibilities:
 *
 *   1. Define the qgl* function-pointer globals that ioQ3's renderer modules
 *      reference as extern.  They are NULL at start-up and wired to real
 *      OpenGX functions by QGL_Init() (qgl_wii.c) when the renderer is
 *      initialised properly.
 *
 *   2. Platform GL helper stubs that the renderer calls but that are not
 *      part of the OpenGL API itself (GLimp_LogComment, GLimp_Minimize).
 *
 * Everything that was here before (GL_Bind, GL_State, backEnd/tess globals,
 * RB_StageIterator*, R_CreateImage, etc.) is now provided by the real ioQ3
 * renderer files compiled in the Makefile:
 *   tr_backend.c  — GL_Bind, GL_State, backEnd, tess, R_GetCommandBuffer, …
 *   tr_shade.c    — RB_StageIterator*
 *   tr_surface.c  — surface drawing helpers
 *   tr_image.c    — R_CreateImage, R_FindImageFile, R_InitImages, …
 *   tr_sky.c      — R_InitSkyTexCoords, sky rendering
 *   tr_flares.c   — RB_AddFlare, RB_RenderFlares
 */

#include <gccore.h>
#include <string.h>
#include "tr_local.h"

/* =========================================================================
   qgl* function-pointer definitions.
   The GLE macro is used here only to DEFINE (allocate) each pointer to NULL.
   The typedefs (name##proc) and extern declarations come from qgl.h which
   is included transitively through tr_local.h above.
   ========================================================================= */

#define GLE(ret, name, ...) name##proc * qgl##name = NULL;
QGL_1_1_PROCS;
QGL_1_1_FIXED_FUNCTION_PROCS;
QGL_DESKTOP_1_1_PROCS;
QGL_DESKTOP_1_1_FIXED_FUNCTION_PROCS;
QGL_3_0_PROCS;
#undef GLE

/* ARB extension pointers used by multitexture and compiled-vertex-array paths */
void (APIENTRYP qglActiveTextureARB)      (GLenum texture) = NULL;
void (APIENTRYP qglClientActiveTextureARB)(GLenum texture) = NULL;
void (APIENTRYP qglMultiTexCoord2fARB)    (GLenum target, GLfloat s, GLfloat t) = NULL;
void (APIENTRYP qglLockArraysEXT)         (GLint first, GLsizei count) = NULL;
void (APIENTRYP qglUnlockArraysEXT)       (void) = NULL;

/* GL version integers referenced by the renderer */
int qglMajorVersion = 1;
int qglMinorVersion = 1;
int qglesMajorVersion = 0;
int qglesMinorVersion = 0;

/* =========================================================================
   Platform GL stubs
   ========================================================================= */

/* GLimp_LogComment — called by the renderer to annotate the GL command stream.
 * No-op on Wii: we have no GL debug output or apitrace. */
void GLimp_LogComment(char *comment)
{
    (void)comment;
}

/* GLimp_Minimize — called when the application window is iconified.
 * Not applicable on Wii; no-op. */
void GLimp_Minimize(void)
{
}
