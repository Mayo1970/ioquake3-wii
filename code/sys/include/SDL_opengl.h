/* SDL_opengl.h — Wii stub.
 * qgl.h includes this when USE_INTERNAL_SDL_HEADERS is defined.
 * With OPENGX_AVAILABLE we pull in OpenGX's real GL header (which provides
 * actual function prototypes that QGL_Init can take addresses of).
 * Without it we fall back to the minimal type/constant stub. */
#ifndef WII_SDL_OPENGL_H
#define WII_SDL_OPENGL_H
#ifdef OPENGX_AVAILABLE
#  include <GL/gl.h>   /* OpenGX real header — glBegin, glBindTexture, … */
   /* GLchar is GL 2.0; OpenGX implements GL 1.x only — define it here so
    * ioQ3's qgl.h (which references it in QGL_2_0_PROCS) can compile. */
#  ifndef GL_CHAR_DEFINED
   typedef char GLchar;
#  define GL_CHAR_DEFINED
#  endif
#else
#  include "wii_gl_compat.h"
#endif
#endif /* WII_SDL_OPENGL_H */
