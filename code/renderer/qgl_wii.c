/*
 * qgl_wii.c — Wire ioQ3's qgl* function pointers to OpenGX.
 *
 * QGL_Init() is called once from GetRefAPI (wii_renderer.c) when the engine
 * hands us a real refimport_t.  After this call every qgl* pointer is live
 * and the full renderergl1 pipeline (tr_backend, tr_shade, tr_image, …) works
 * through OpenGX → GX hardware.
 *
 * Only the qgl* pointers that exist in ioQ3's qgl.h GLE macro groups are
 * assigned here.  Functions that are not in those groups (qglFogf, qglRotatef,
 * qglNormal3f, etc.) are NOT referenced by renderergl1 and are therefore not
 * wired.
 *
 * No-op stubs cover GL features that GX hardware cannot express:
 *   DrawBuffer, PolygonMode            — no draw-buffer select / wireframe
 *   CopyTexImage2D / CopyTexSubImage2D — EFB-to-texture not needed by Q3
 */

#include "tr_local.h"   /* gives us qgl* externs + proc typedefs */

/* --------------------------------------------------------------------------
 * No-op stubs
 * -------------------------------------------------------------------------- */
static void noop_DrawBuffer  (GLenum m)             { (void)m; }
static void noop_PolygonMode (GLenum f, GLenum m)   { (void)f; (void)m; }

/* --------------------------------------------------------------------------
 * CMPR (DXT1) texture upload intercept
 *
 * OpenGX's glTexImage2D internally calls _ogx_convert_rgb_image_to_DXT1 when
 * the internal format is GL_COMPRESSED_RGB, producing GX_TF_CMPR textures
 * that the GX hardware decodes directly — 4× smaller than GX_TF_RGB565.
 *
 * We redirect opaque GL_RGB uploads to GL_COMPRESSED_RGB so every eligible
 * world and model texture gets compressed automatically at load time.
 * Textures with alpha (GL_RGBA / GL_LUMINANCE_ALPHA etc.) are left alone.
 * -------------------------------------------------------------------------- */
static void wii_TexImage2D(GLenum target, GLint level, GLint internalformat,
                           GLsizei width, GLsizei height, GLint border,
                           GLenum format, GLenum type, const GLvoid *pixels)
{
    if (format == GL_RGB && type == GL_UNSIGNED_BYTE &&
        internalformat == GL_RGB && pixels != NULL) {
        internalformat = GL_COMPRESSED_RGB;
    }
    glTexImage2D(target, level, internalformat, width, height, border,
                 format, type, pixels);
}

static void noop_CopyTexSubImage2D(GLenum tgt, GLint lvl,
    GLint xo, GLint yo, GLint x, GLint y, GLsizei w, GLsizei h)
{ (void)tgt;(void)lvl;(void)xo;(void)yo;(void)x;(void)y;(void)w;(void)h; }

/* OpenGX (Mesa 7.0 API) provides the standard double-precision signatures
 * for glFrustum/glOrtho/glDepthRange/glClearDepth — no float-variant wrappers
 * needed.  The assignments below go directly to the gl* symbols. */

/* --------------------------------------------------------------------------
 * QGL_Init — assign qgl* pointers.
 * Covers only the procs declared in qgl.h's GLE macro groups:
 *   QGL_1_1_PROCS, QGL_1_1_FIXED_FUNCTION_PROCS,
 *   QGL_DESKTOP_1_1_PROCS, QGL_DESKTOP_1_1_FIXED_FUNCTION_PROCS
 * plus the ARB multitexture / CVA pointers declared manually in
 * wii_gl_stubs.c.
 * -------------------------------------------------------------------------- */
void QGL_Init(void)
{
    printf("[qgl] QGL_Init: wiring qgl* -> OpenGX gl*\n"); fflush(stdout);

    /* ---- QGL_1_1_PROCS ---- */
    qglBindTexture        = glBindTexture;
    qglBlendFunc          = glBlendFunc;
    qglClear              = glClear;
    qglClearColor         = glClearColor;
    qglClearStencil       = glClearStencil;
    qglColorMask          = glColorMask;
    qglCopyTexSubImage2D  = noop_CopyTexSubImage2D; /* EFB readback not needed */
    qglCullFace           = glCullFace;
    qglDeleteTextures     = glDeleteTextures;
    qglDepthFunc          = glDepthFunc;
    qglDepthMask          = glDepthMask;
    qglDisable            = glDisable;
    qglDrawArrays         = glDrawArrays;
    qglDrawElements       = glDrawElements;
    qglEnable             = glEnable;
    qglFinish             = glFinish;
    qglFlush              = glFlush;
    qglGenTextures        = glGenTextures;
    qglGetBooleanv        = glGetBooleanv;
    qglGetError           = glGetError;
    qglGetIntegerv        = glGetIntegerv;
    qglGetString          = glGetString;
    qglLineWidth          = glLineWidth;
    qglPolygonOffset      = glPolygonOffset;
    qglReadPixels         = glReadPixels;
    qglScissor            = glScissor;
    qglStencilFunc        = glStencilFunc;
    qglStencilMask        = glStencilMask;
    qglStencilOp          = glStencilOp;
    qglTexImage2D         = glTexImage2D;
    qglTexParameterf      = glTexParameterf;
    qglTexParameteri      = glTexParameteri;
    qglTexSubImage2D      = glTexSubImage2D;
    qglViewport           = glViewport;

    /* ---- QGL_1_1_FIXED_FUNCTION_PROCS ---- */
    qglAlphaFunc          = glAlphaFunc;
    qglColor4f            = glColor4f;
    qglColorPointer       = glColorPointer;
    qglDisableClientState = glDisableClientState;
    qglEnableClientState  = glEnableClientState;
    qglLoadIdentity       = glLoadIdentity;
    qglLoadMatrixf        = glLoadMatrixf;
    qglMatrixMode         = glMatrixMode;
    qglPopMatrix          = glPopMatrix;
    qglPushMatrix         = glPushMatrix;
    qglShadeModel         = glShadeModel;
    qglTexCoordPointer    = glTexCoordPointer;
    qglTexEnvf            = glTexEnvf;
    qglTranslatef         = glTranslatef;
    qglVertexPointer      = glVertexPointer;

    /* ---- QGL_DESKTOP_1_1_PROCS ---- */
    qglClearDepth         = glClearDepth;
    qglDepthRange         = glDepthRange;
    qglDrawBuffer         = noop_DrawBuffer;  /* GX has no draw-buffer select */
    qglPolygonMode        = noop_PolygonMode; /* GX has no wireframe mode     */

    /* ---- QGL_DESKTOP_1_1_FIXED_FUNCTION_PROCS ---- */
    qglArrayElement       = glArrayElement;
    qglBegin              = glBegin;
    qglClipPlane          = glClipPlane;
    qglColor3f            = glColor3f;
    qglColor4ubv          = glColor4ubv;
    qglEnd                = glEnd;
    qglFrustum            = glFrustum;
    qglOrtho              = glOrtho;
    qglTexCoord2f         = glTexCoord2f;
    qglTexCoord2fv        = glTexCoord2fv;
    qglVertex2f           = glVertex2f;
    qglVertex3f           = glVertex3f;
    qglVertex3fv          = glVertex3fv;

    /* ---- ARB multitexture — OpenGX aliases to core GL 1.3 ---- */
    qglActiveTextureARB       = (void(*)(GLenum))glActiveTexture;
    qglClientActiveTextureARB = (void(*)(GLenum))glClientActiveTexture;
    qglMultiTexCoord2fARB     = (void(*)(GLenum,GLfloat,GLfloat))glMultiTexCoord2f;

    /* Compiled vertex arrays — OpenGX may not expose these; leave NULL so
     * the extension check in tr_init.c falls through cleanly.             */
    qglLockArraysEXT   = NULL;
    qglUnlockArraysEXT = NULL;

    printf("[qgl] QGL_Init done\n"); fflush(stdout);
}

void QGL_Shutdown(void)
{
    /* Nothing to unload on Wii — OpenGX is statically linked */
}
