/* qgl_wii.c — Wire ioQ3's qgl* function pointers to OpenGX. */

#include "tr_local.h"

static void noop_DrawBuffer  (GLenum m)             { (void)m; }
static void noop_PolygonMode (GLenum f, GLenum m)   { (void)f; (void)m; }

static void noop_CopyTexSubImage2D(GLenum tgt, GLint lvl,
    GLint xo, GLint yo, GLint x, GLint y, GLsizei w, GLsizei h)
{ (void)tgt;(void)lvl;(void)xo;(void)yo;(void)x;(void)y;(void)w;(void)h; }

void QGL_Init(void)
{
    /* ---- QGL_1_1_PROCS ---- */
    qglBindTexture        = glBindTexture;
    qglBlendFunc          = glBlendFunc;
    qglClear              = glClear;
    qglClearColor         = glClearColor;
    qglClearStencil       = glClearStencil;
    qglColorMask          = glColorMask;
    qglCopyTexSubImage2D  = noop_CopyTexSubImage2D;
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
    qglDrawBuffer         = noop_DrawBuffer;
    qglPolygonMode        = noop_PolygonMode;

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

    /* ---- ARB multitexture ---- */
    qglActiveTextureARB       = (void(*)(GLenum))glActiveTexture;
    qglClientActiveTextureARB = (void(*)(GLenum))glClientActiveTexture;
    qglMultiTexCoord2fARB     = (void(*)(GLenum,GLfloat,GLfloat))glMultiTexCoord2f;

    /* CVA — OpenGX doesn't expose these; NULL lets tr_init.c skip the extension. */
    qglLockArraysEXT   = NULL;
    qglUnlockArraysEXT = NULL;

}

void QGL_Shutdown(void)
{
}
