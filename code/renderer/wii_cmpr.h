/*
 * wii_cmpr.h — GX_TF_CMPR (DXT1 / S3TC) encoder for ioquake3-wii
 *
 * Design constraints (from session notes):
 *   - Round-to-nearest RGB565 endpoint quantisation (not truncating shifts)
 *   - 3/8 + 5/8 palette interpolation for the two intermediate colours,
 *     matching the shift-based arithmetic Wii GX uses when decoding CMPR
 *   - Big-endian RGB565 words (GX is PowerPC / big-endian)
 *   - 8×8 super-tile layout: four 4×4 DXT1 blocks per super-tile,
 *     stored TL → TR → BL → BR
 *
 * Only suitable for fully-opaque (no-alpha) textures.
 * Use GX_TF_RGBA8 when alpha is required.
 *
 * This file is plain C99 / stdint — no libogc or Wii headers needed —
 * so it can be compiled into the host-side smoke test as-is.
 */

#ifndef WII_CMPR_H
#define WII_CMPR_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CMPR_Encode
 *
 * Convert an RGB888 image to GX_TF_CMPR format.
 *
 *   rgb    - source pixels, 3 bytes per pixel, row-major, top-to-bottom
 *   width  - image width  in pixels (must be a multiple of 8)
 *   height - image height in pixels (must be a multiple of 8)
 *   out    - destination buffer; caller must provide (width * height / 2) bytes,
 *            aligned to 32 bytes for DCFlushRange on Wii
 *
 * Output size: width * height / 2 bytes  (4× smaller than GX_TF_RGB565)
 */
void CMPR_Encode(const uint8_t *rgb, int width, int height, uint8_t *out);

/*
 * CMPR_Decode
 *
 * Expand a GX_TF_CMPR buffer back to RGB888 (for smoke-test verification).
 * Uses the same 3/8+5/8 palette weights as the encoder so round-trip
 * error reflects only quantisation, not a weight mismatch.
 *
 *   cmpr   - CMPR data produced by CMPR_Encode
 *   width  - image width  (multiple of 8)
 *   height - image height (multiple of 8)
 *   out    - destination: (width * height * 3) bytes
 */
void CMPR_Decode(const uint8_t *cmpr, int width, int height, uint8_t *out);

#ifdef __cplusplus
}
#endif

#endif /* WII_CMPR_H */
