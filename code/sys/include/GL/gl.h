/* GL/gl.h -- Wii wrapper. Forwards to OpenGX or provides type stubs. */
#ifdef OPENGX_AVAILABLE
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
