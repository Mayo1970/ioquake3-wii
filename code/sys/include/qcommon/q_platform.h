/*
 * qcommon/q_platform.h — Wii/Gekko override
 *
 * This file lives at code/sys/include/qcommon/q_platform.h and is found
 * FIRST because -I$(PORTDIR)/code/sys/include comes before
 * -I$(IOQ3_DIR)/code in CFLAGS.
 *
 * It defines everything the real q_platform.h would define for a
 * supported platform, tailored for Wii/Gekko/devkitPPC, then
 * includes the real q_platform.h with a guard so its body is skipped.
 *
 * This avoids ANY modification to the upstream ioQ3 source tree.
 */

#ifndef __Q_PLATFORM_H
/* Do NOT define __Q_PLATFORM_H here — we want the real file's guard
 * to fire when it is #included below, skipping its body. */

/*=======================================================================
 * Wii / Gekko platform definitions
 *=======================================================================*/

#define OS_STRING   "wii"
#define ARCH_STRING "ppc"
#define PATH_SEP    '/'
#define DLL_EXT     ".so"

/* Wii/Gekko PowerPC is big-endian */
#undef  Q3_LITTLE_ENDIAN
#define Q3_BIG_ENDIAN

/* ID_INLINE for GCC */
#define ID_INLINE __inline__

/* No CPU affinity, no mmap */
#define MAXPRINTMSG 4096

/* Architecture intrinsics available on GCC/PPC */
#if defined(__GNUC__)
#  define Q3_LITTLE_ENDIAN_UNDEF  /* marker only — do not define Q3_LITTLE_ENDIAN */
#endif

/* Needed by q_shared.h */
#include <stddef.h>
#include <stdint.h>
#include <float.h>

/* Now pull in the real q_platform.h, which will be skipped because
 * its include guard (__Q_PLATFORM_H) will already be set by the time
 * our #define below fires — except we set it HERE so the body is skipped. */
#define __Q_PLATFORM_H

/* Also define the guard name used in some ioQ3 versions */
#define __Q_PLATFORM_H__

#endif /* __Q_PLATFORM_H */
