/*
 * endian.h — devkitPPC/Wii stub
 *
 * q_platform.h (Linux branch) does:
 *   #if defined(__BYTE_ORDER) && defined(__BIG_ENDIAN) && __BYTE_ORDER==__BIG_ENDIAN
 *     define Q3_BIG_ENDIAN
 *   #if defined(__BYTE_ORDER) && defined(__LITTLE_ENDIAN) && __BYTE_ORDER==__LITTLE_ENDIAN
 *     define Q3_LITTLE_ENDIAN
 *
 * The Wii is big-endian. We MUST define __BIG_ENDIAN (as a value) and
 * __BYTE_ORDER (== __BIG_ENDIAN). We must NOT define __LITTLE_ENDIAN
 * as a bare symbol, or q_platform.h will fire the "both endians" error
 * even though __BYTE_ORDER != __LITTLE_ENDIAN.
 */

#ifndef _ENDIAN_H
#define _ENDIAN_H

/* Only these two — deliberately no __LITTLE_ENDIAN, no __PDP_ENDIAN */
#define __BIG_ENDIAN  4321
#define __BYTE_ORDER  __BIG_ENDIAN

/* Byte-swap helpers used by ioQ3 network/file code */
#define __bswap_16(x) \
    ((unsigned short)(((x) >> 8) | ((x) << 8)))

#define __bswap_32(x) \
    ((((x) & 0xFF000000u) >> 24) | \
     (((x) & 0x00FF0000u) >>  8) | \
     (((x) & 0x0000FF00u) <<  8) | \
     (((x) & 0x000000FFu) << 24))

/* htobe/betoh — no-ops on a big-endian host */
#define htobe16(x)  (x)
#define htobe32(x)  (x)
#define be16toh(x)  (x)
#define be32toh(x)  (x)

/* htole/letoh — swap on a big-endian host */
#define htole16(x)  __bswap_16(x)
#define htole32(x)  __bswap_32(x)
#define le16toh(x)  __bswap_16(x)
#define le32toh(x)  __bswap_32(x)

#endif /* _ENDIAN_H */
