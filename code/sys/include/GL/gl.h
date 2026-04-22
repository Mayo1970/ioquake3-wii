/* GL/gl.h — Wii wrapper.
 *
 * When OPENGX_AVAILABLE, forward to OpenGX's real Mesa 7.0 GL header via
 * #include_next so this file does not shadow it.  Without OpenGX the stub
 * types below satisfy the renderer's type dependencies. */

#ifdef OPENGX_AVAILABLE
/* #include_next continues the search from the next -I directory, skipping
 * this file.  That finds /c/devkitPro/portlibs/wii/include/GL/gl.h. */
#  include_next <GL/gl.h>
#else
#ifndef WII_GL_STUB_H
#define WII_GL_STUB_H
typedef unsigned int GLenum;
typedef unsigned int GLbitfield;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;
typedef unsigned char GLboolean;
typedef signed char GLbyte;
typedef short GLshort;
typedef unsigned char GLubyte;
typedef unsigned short GLushort;
typedef unsigned long GLulong;
typedef float GLfloat;
typedef float GLclampf;
typedef double GLdouble;
typedef double GLclampd;
typedef void GLvoid;
typedef long GLintptr;
typedef long GLsizeiptr;
#define GL_FALSE 0
#define GL_TRUE  1
#define APIENTRY
#define APIENTRYP *
#endif /* WII_GL_STUB_H */
#endif /* OPENGX_AVAILABLE */
