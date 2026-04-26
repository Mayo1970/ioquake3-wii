/* SDL_opengl.h -- Wii stub for qgl.h (USE_INTERNAL_SDL_HEADERS). */
#ifndef WII_SDL_OPENGL_H
#define WII_SDL_OPENGL_H
#ifdef OPENGX_AVAILABLE
#  include <GL/gl.h>
   /* GLchar is GL 2.0; OpenGX is GL 1.x only */
#  ifndef GL_CHAR_DEFINED
   typedef char GLchar;
#  define GL_CHAR_DEFINED
#  endif
#else
#  include "wii_gl_compat.h"
#endif
#endif
