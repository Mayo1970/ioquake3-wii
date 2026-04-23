/*
 * wii_platform.h — force-included before every TU via -include
 *
 * This is the LAST RESORT fallback. Ideally you run patch_q_platform.py
 * first so q_platform.h handles Wii natively. If for any reason the patch
 * didn't apply, this header defines everything q_platform.h would have set.
 *
 * All defines use #ifndef so they don't conflict if the patch is in place.
 */

#ifndef WII_PLATFORM_H
#define WII_PLATFORM_H

/* ----------------------------------------------------------------
 * Tell q_platform.h we are a known OS so it doesn't #error.
 * We pick __linux__ because that branch has the least harmful
 * side effects on a POSIX-ish newlib system.
 * NOTE: if patch_q_platform.py was run, the GEKKO block fires
 * instead and __linux__ is never needed.
 * ---------------------------------------------------------------- */
#if !defined(__linux__) && !defined(WIN32) && !defined(MACOS_X) && \
    !defined(__FreeBSD__) && !defined(__OpenBSD__) && !defined(GEKKO)
#  define __linux__
#endif

/* ----------------------------------------------------------------
 * Endianness — define BEFORE q_platform.h evaluates it.
 * We deliberately do NOT define __LITTLE_ENDIAN so q_platform.h's
 * "both endians" check never fires.
 * ---------------------------------------------------------------- */
#ifndef __BIG_ENDIAN
#  define __BIG_ENDIAN 4321
#endif
#ifndef __BYTE_ORDER
#  define __BYTE_ORDER __BIG_ENDIAN
#endif
/* Force the Q3 endian macros regardless of what q_platform.h does */
#undef  Q3_LITTLE_ENDIAN
#define Q3_BIG_ENDIAN

/* ----------------------------------------------------------------
 * Platform strings — needed if the Linux branch didn't set them
 * ---------------------------------------------------------------- */
#ifndef OS_STRING
#  define OS_STRING "wii"
#endif
#ifndef ARCH_STRING
#  define ARCH_STRING "ppc"
#endif
#ifndef PATH_SEP
#  define PATH_SEP '/'
#endif
#ifndef DLL_EXT
#  define DLL_EXT ".so"
#endif

/* ----------------------------------------------------------------
 * ID_INLINE — define early; q_platform.h will see the #define and
 * skip its own (its block uses #ifndef ID_INLINE or plain #define,
 * so we use a pragma to suppress the redefinition warning).
 * ---------------------------------------------------------------- */
#ifndef ID_INLINE
#  define ID_INLINE __inline__
#endif
#pragma GCC diagnostic ignored "-Wattributes"

/* ----------------------------------------------------------------
 * Misc ioQ3 / newlib compatibility
 * ---------------------------------------------------------------- */
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#ifndef MAP_FAILED
#  define MAP_FAILED ((void *)-1)
#endif

/* Constants for vm_powerpc.c compiled QVM */
#ifndef PROT_READ
#  define PROT_READ     1
#  define PROT_WRITE    2
#  define PROT_EXEC     4
#  define MAP_SHARED    1
#  define MAP_ANONYMOUS 2
#  define MAP_ANON      MAP_ANONYMOUS
#endif

/* mprotect stub — all Wii memory is executable */
static inline int mprotect(void *addr, size_t len, int prot) {
    (void)addr; (void)len; (void)prot; return 0;
}

#define IOAPI_NO_64BIT

/* ----------------------------------------------------------------
 * Warning suppression — keep build log readable
 * ---------------------------------------------------------------- */
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wmissing-braces"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"

/* COLOR_* undefs are handled in wii_net.h after network.h is included */

/* ----------------------------------------------------------------
 * Network stubs — newlib/Wii has no ifaddrs.h, no getifaddrs,
 * no IPv6 stack. Tell ioQ3's net_ip.c to skip all of that.
 * ---------------------------------------------------------------- */
#define HAVE_SA_LEN          0
#undef  HAVE_SOCKADDR_SA_LEN
#define NET_ENABLE_IPV6      0
/* ifaddrs.h is in our stub include dir and defines everything needed */

/* Wii network socket shim — only include for net_ip.c to avoid
 * network.h's send() declaration conflicting with huffman.c etc. */
#ifdef WII_INCLUDE_NET
#include "sys/wii_net.h"
#endif



/* ----------------------------------------------------------------
 * client_t / netchan memory reduction
 *
 * SV_Startup() allocates sizeof(client_t) × sv_maxclients.
 * At stock values one client_t ≈ 114 KB, so 8 clients ≈ 913 KB
 * from the zone heap — far too much for the Wii's small zone.
 *
 * Breakdown of the three biggest arrays inside client_t:
 *
 *   reliableCommands[MAX_RELIABLE_COMMANDS][MAX_STRING_CHARS]
 *     stock: 64 × 1024 = 64 KB/client  (56 % of client_t)
 *     wii:   16 × 1024 = 16 KB/client
 *
 *   netchan_t.fragmentBuffer + unsentBuffer  (2 × MAX_MSGLEN)
 *     stock: 2 × 16384 = 32 KB/client  (28 %)
 *     wii:   unchanged — gamestate can legitimately reach 12 KB
 *
 *   frames[PACKET_BACKUP] × clientSnapshot_t (~284 bytes each)
 *     stock: 32 × 284  =  9 KB/client  (8 %)
 *     wii:   16 × 284  =  4.5 KB/client
 *
 *   downloadBlocks[MAX_DOWNLOAD_WINDOW] + sizes
 *     stock: 48 × 8    =  384 B/client
 *     wii:    4 × 8    =   32 B/client
 *
 * After reductions: ~44 KB/client × 8 = ~350 KB  (was 913 KB).
 *
 * All reduced constants remain powers of two where the engine
 * uses bitmask arithmetic (& (CONSTANT-1)).
 *
 * These #defines are set here (force-included first) and the
 * upstream headers are patched with #ifndef guards by
 * apply_patches.sh so they don't redefine them.
 * ---------------------------------------------------------------- */
#ifndef MAX_RELIABLE_COMMANDS
#define MAX_RELIABLE_COMMANDS   16      /* stock: 64  — must be power-of-2 */
#endif
#ifndef PACKET_BACKUP
#define PACKET_BACKUP           16      /* stock: 32  — must be power-of-2 */
#endif
#ifndef PACKET_MASK
#define PACKET_MASK             (PACKET_BACKUP-1)
#endif
#ifndef MAX_DOWNLOAD_WINDOW
#define MAX_DOWNLOAD_WINDOW     4       /* stock: 48  — no net downloads on Wii */
#endif

/* Override ioQ3 minimum hunk size — Wii only has 88MB total RAM.
 * common.c checks MIN_COMHUNKMEGS before allocating. */
#undef  MIN_DEDICATED_COMHUNKMEGS
#undef  MIN_COMHUNKMEGS
#undef  DEF_COMHUNKMEGS
#undef  DEF_COMZONEMEGS
#define MIN_DEDICATED_COMHUNKMEGS 8
#define MIN_COMHUNKMEGS           8
#define DEF_COMHUNKMEGS           "32"
#define DEF_COMZONEMEGS           "4"

/* Reduce audio streaming BSS from 16 MB to 16 KB.
 * s_rawsamples[MAX_RAW_STREAMS][MAX_RAW_SAMPLES] in snd_dma.c occupies
 * MAX_RAW_STREAMS × MAX_RAW_SAMPLES × 8 bytes.  Stock values (129 × 16384)
 * = 16 MB — overflowing MEM1 so fatInitDefault() gets no malloc heap.
 * Wii has no OGG/MP3 background music and no cinematics, so one stream
 * with a small ring buffer is sufficient.
 * snd_local.h is patched by apply_patches.sh to use #ifndef guards. */
#ifndef MAX_RAW_STREAMS
#define MAX_RAW_STREAMS  1      /* stock: MAX_CLIENTS*2+1 = 129 */
#endif
#ifndef MAX_RAW_SAMPLES
#define MAX_RAW_SAMPLES  2048   /* stock: 16384 */
#endif

/* Undef libogc COLOR_* macros — ioQ3 redefines them as char literals */
#undef COLOR_BLACK
#undef COLOR_RED
#undef COLOR_GREEN
#undef COLOR_YELLOW
#undef COLOR_BLUE
#undef COLOR_CYAN
#undef COLOR_MAGENTA
#undef COLOR_WHITE
#undef COLOR_ORANGE

/* ----------------------------------------------------------------
 * Diagnostic logging — writes to sd:/quake3/diag.txt.
 * Only active when WII_DEBUG is defined (make WII_DEBUG=1).
 * ---------------------------------------------------------------- */
#include <stdio.h>
#include <stdarg.h>
#ifdef WII_DEBUG
static inline void wii_diag(const char *fmt, ...) __attribute__((format(printf,1,2)));
static inline void wii_diag(const char *fmt, ...) {
    FILE *f = fopen("sd:/quake3/diag.txt", "a");
    if (f) {
        va_list ap;
        va_start(ap, fmt);
        vfprintf(f, fmt, ap);
        va_end(ap);
        fflush(f);
        fclose(f);
    }
}
#else
#define wii_diag(...) ((void)0)
#endif

/* ----------------------------------------------------------------
 * GL compatibility — intercept SDL_opengl.h include from qgl.h.
 * We define USE_INTERNAL_SDL_HEADERS so qgl.h tries to include
 * "SDL_opengl.h" (quoted), then we satisfy that with our stub
 * via the -I include path pointing to our sys/include directory.
 * ---------------------------------------------------------------- */
#define USE_INTERNAL_SDL_HEADERS
/* SDL_opengl.h is provided by wii_gl_compat.h in our include dir */

/* ----------------------------------------------------------------
 * OpenGX GL compatibility note:
 * OpenGX's gl.h declares glPolygonMode, glDrawBuffer, and GL_CLAMP
 * as real symbols, so no stubs are needed here.  If a future OpenGX
 * version drops any of these, add a macro guard here.
 * ---------------------------------------------------------------- */

#endif /* WII_PLATFORM_H */
