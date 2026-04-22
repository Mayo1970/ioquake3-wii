/* SDL_loadso.h — Wii stub. No dynamic library loading on Wii. */
#ifndef SDL_LOADSO_H
#define SDL_LOADSO_H
static inline void *SDL_LoadObject(const char *f) { (void)f; return 0; }
static inline void *SDL_LoadFunction(void *h, const char *n) { (void)h; (void)n; return 0; }
static inline void  SDL_UnloadObject(void *h) { (void)h; }
#endif
