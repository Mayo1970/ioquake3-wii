/* SDL_audio.h — Wii stub. SDL is not used on Wii; this satisfies includes. */
#ifndef WII_SDL_STUB_H
#define WII_SDL_STUB_H
#ifndef SDL_VERSION_H
#define SDL_VERSION_H
#define SDL_MAJOR_VERSION 0
#define SDL_MINOR_VERSION 0
#define SDL_PATCHLEVEL    0
typedef struct { unsigned char major, minor, patch; } SDL_version;
#define SDL_VERSION(x) do{(x)->major=0;(x)->minor=0;(x)->patch=0;}while(0)
#define SDL_VERSIONNUM(a,b,c) 0
#define SDL_COMPILEDVERSION 0
#define SDL_VERSION_ATLEAST(a,b,c) 0
static inline void SDL_GetVersion(SDL_version *v){SDL_VERSION(v);}
typedef unsigned int Uint32;
typedef unsigned short Uint16;
typedef unsigned char Uint8;
typedef int SDL_bool;
#define SDL_TRUE  1
#define SDL_FALSE 0
/* CPU info stubs */
static inline int SDL_GetCPUCount(void) { return 1; }
static inline int SDL_GetCPUCacheLineSize(void) { return 32; }
static inline SDL_bool SDL_HasMMX(void)    { return SDL_FALSE; }
static inline SDL_bool SDL_HasSSE(void)    { return SDL_FALSE; }
static inline SDL_bool SDL_HasSSE2(void)   { return SDL_FALSE; }
static inline SDL_bool SDL_HasAltiVec(void){ return SDL_FALSE; }
static inline int SDL_GetSystemRAM(void)   { return 88; }
static inline const char *SDL_GetPlatform(void) { return "Wii"; }
static inline const char *SDL_GetError(void) { return ""; }
#endif /* SDL_VERSION_H */
#endif /* WII_SDL_STUB_H */
