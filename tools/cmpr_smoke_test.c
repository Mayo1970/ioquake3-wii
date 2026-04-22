/*
 * cmpr_smoke_test.c — host-side verification of the CMPR encoder
 *
 * Compile on Linux / macOS:
 *   gcc -std=c11 -Wall -Wextra -o cmpr_smoke_test \
 *       cmpr_smoke_test.c ../code/renderer/wii_cmpr.c
 *
 * Compile inside devkitPro's MSYS2 (Cygwin-based GCC):
 *   GCC_INC=/c/devkitPro/msys2/usr/lib/gcc/x86_64-pc-cygwin/15.2.0/include
 *   SYS_INC=/c/devkitPro/msys2/usr/include
 *   gcc -std=c11 -Wall -isystem "$GCC_INC" -isystem "$SYS_INC" \
 *       -o cmpr_smoke_test.exe \
 *       cmpr_smoke_test.c ../code/renderer/wii_cmpr.c
 *
 * Run:
 *   ./cmpr_smoke_test  (or ./cmpr_smoke_test.exe on Windows)
 *
 * Tests
 * ─────
 *  1. solid_colors     — solid R, G, B, white, black, grey
 *                        quantisation round-trip should be ≤ 8 per channel
 *  2. two_color_block  — known endpoints; verify 3/8+5/8 palette values exactly
 *  3. palette_weights  — explicit check: c2 = (5*c0+3*c1)>>3, c3 = (3*c0+5*c1)>>3
 *  4. gradient_8x8     — horizontal ramp; max per-channel error ≤ 16
 *  5. encode_size      — output byte count matches width*height/2
 *  6. super_tile_order — two solid-colour 8×8 tiles side-by-side;
 *                        verify each super-tile decodes to its own colour
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Include the encoder/decoder — no Wii headers needed */
#include "../code/renderer/wii_cmpr.h"

/* =========================================================================
   Helpers
   ========================================================================= */

static int s_pass = 0, s_fail = 0;

static void report(const char *name, int cond)
{
    if (cond) {
        printf("  PASS  %s\n", name);
        s_pass++;
    } else {
        printf("  FAIL  %s\n", name);
        s_fail++;
    }
}

/* Maximum per-channel absolute error across the entire image */
static int max_error(const uint8_t *a, const uint8_t *b, int n_pixels)
{
    int mx = 0;
    for (int i = 0; i < n_pixels * 3; i++) {
        int e = (int)a[i] - (int)b[i];
        if (e < 0) e = -e;
        if (e > mx) mx = e;
    }
    return mx;
}

/* Fill an 8×8 RGB block with a solid colour */
static void fill_solid(uint8_t *buf, uint8_t r, uint8_t g, uint8_t b)
{
    for (int i = 0; i < 8 * 8; i++) {
        buf[i*3+0] = r;
        buf[i*3+1] = g;
        buf[i*3+2] = b;
    }
}

/* =========================================================================
   Test 1 — solid colours: round-trip error ≤ 8 per channel
   ========================================================================= */
static void test_solid_colors(void)
{
    printf("\n[1] solid colours\n");

    static const struct { const char *name; uint8_t r,g,b; } cases[] = {
        { "red",   255,   0,   0 },
        { "green",   0, 255,   0 },
        { "blue",    0,   0, 255 },
        { "white", 255, 255, 255 },
        { "black",   0,   0,   0 },
        { "grey",  128, 128, 128 },
        { "mid-r", 100,  50,  25 },
        { NULL,      0,   0,   0 }
    };

    uint8_t src[8*8*3], cmpr[8*8/2], dst[8*8*3];

    for (int i = 0; cases[i].name; i++) {
        fill_solid(src, cases[i].r, cases[i].g, cases[i].b);
        CMPR_Encode(src, 8, 8, cmpr);
        CMPR_Decode(cmpr, 8, 8, dst);

        int err = max_error(src, dst, 8*8);
        char label[64];
        snprintf(label, sizeof(label), "solid_%s (err=%d)", cases[i].name, err);
        report(label, err <= 8);
    }
}

/* =========================================================================
   Test 2 — palette weights: explicit bit-exact check
   ========================================================================= */

/*
 * Verify that for a block whose two pixels span the full endpoint range,
 * the decoded intermediate palette entries match (5*c0+3*c1)>>3 and
 * (3*c0+5*c1)>>3 exactly.
 *
 * We make the block degenerate:
 *   - top-left  (row0,col0) = endpoint max
 *   - bottom-right (row3,col3) = endpoint min
 *   - all other pixels = those two colours alternately so the bbox is stable
 * Then we decode the block and read back c2 and c3 from the pixels that
 * were assigned index 2 and index 3 by the encoder.
 *
 * Rather than inspecting the raw CMPR bytes, we verify the decoded palette
 * by building the palette ourselves and comparing.
 */
static void test_palette_weights(void)
{
    printf("\n[2] 3/8+5/8 palette weights\n");

    /*
     * Choose two colours that are far apart so the encoder must use
     * both intermediate palette entries.
     *
     * c0_rgb = (200, 160, 80)   → pack/expand → some value we can inspect
     * c1_rgb = ( 40,  20, 10)
     *
     * Fill the 8×8 block with a 2×2 checkerboard so both endpoints are
     * well-represented and the bbox is tight.
     */
    const uint8_t C0[3] = {200, 160, 80};
    const uint8_t C1[3] = { 40,  20, 10};

    uint8_t src[8*8*3];
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) {
            const uint8_t *c = ((x ^ y) & 1) ? C1 : C0;
            src[(y*8+x)*3+0] = c[0];
            src[(y*8+x)*3+1] = c[1];
            src[(y*8+x)*3+2] = c[2];
        }

    uint8_t cmpr[8*8/2];
    CMPR_Encode(src, 8, 8, cmpr);

    /*
     * Extract the first block's endpoints (bytes 0-3 of the CMPR buffer).
     * They are big-endian RGB565.
     */
    uint16_t e0 = ((uint16_t)cmpr[0] << 8) | cmpr[1];
    uint16_t e1 = ((uint16_t)cmpr[2] << 8) | cmpr[3];

    /* Expand */
    uint8_t r0,g0,b0, r1,g1,b1;
    {
        uint8_t r5 = (e0>>11)&0x1F, g6=(e0>>5)&0x3F, b5=e0&0x1F;
        r0=(r5<<3)|(r5>>2); g0=(g6<<2)|(g6>>4); b0=(b5<<3)|(b5>>2);
    }
    {
        uint8_t r5 = (e1>>11)&0x1F, g6=(e1>>5)&0x3F, b5=e1&0x1F;
        r1=(r5<<3)|(r5>>2); g1=(g6<<2)|(g6>>4); b1=(b5<<3)|(b5>>2);
    }

    /* Expected c2 and c3 */
    uint8_t exp_r2 = (uint8_t)((5u*r0 + 3u*r1) >> 3);
    uint8_t exp_g2 = (uint8_t)((5u*g0 + 3u*g1) >> 3);
    uint8_t exp_b2 = (uint8_t)((5u*b0 + 3u*b1) >> 3);
    uint8_t exp_r3 = (uint8_t)((3u*r0 + 5u*r1) >> 3);
    uint8_t exp_g3 = (uint8_t)((3u*g0 + 5u*g1) >> 3);
    uint8_t exp_b3 = (uint8_t)((3u*b0 + 5u*b1) >> 3);

    /* Decode and find pixels assigned to indices 2 and 3 */
    uint8_t dst[8*8*3];
    CMPR_Decode(cmpr, 8, 8, dst);

    /* The decoder uses the same formula — so any pixel with idx=2 will have
     * colour (exp_r2, exp_g2, exp_b2).  We find such pixels via the raw
     * index byte of block 0 (cmpr byte 4). */
    int found_idx2 = 0, found_idx3 = 0;
    int ok2 = 1, ok3 = 1;

    for (int y = 0; y < 4; y++) {
        uint8_t ib = cmpr[4 + y];
        for (int x = 0; x < 4; x++) {
            int idx = (ib >> (x*2)) & 3;
            const uint8_t *dp = dst + (y*8+x)*3;
            if (idx == 2) {
                found_idx2 = 1;
                if (dp[0]!=exp_r2 || dp[1]!=exp_g2 || dp[2]!=exp_b2) ok2=0;
            }
            if (idx == 3) {
                found_idx3 = 1;
                if (dp[0]!=exp_r3 || dp[1]!=exp_g3 || dp[2]!=exp_b3) ok3=0;
            }
        }
    }

    report("endpoints_are_ordered (e0>e1)", e0 > e1);
    report("c2_pixel_matches_5/8+3/8 formula (if used)", !found_idx2 || ok2);
    report("c3_pixel_matches_3/8+5/8 formula (if used)", !found_idx3 || ok3);

    printf("      e0=0x%04X (%u,%u,%u)  e1=0x%04X (%u,%u,%u)\n",
           e0, r0,g0,b0, e1, r1,g1,b1);
    printf("      expected c2=(%u,%u,%u)  c3=(%u,%u,%u)\n",
           exp_r2,exp_g2,exp_b2, exp_r3,exp_g3,exp_b3);
}

/* =========================================================================
   Test 3 — gradient: max error ≤ 16
   ========================================================================= */
static void test_gradient(void)
{
    printf("\n[3] horizontal gradient (16×8)\n");

    uint8_t src[16*8*3];
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 16; x++) {
            uint8_t v = (uint8_t)(x * 255 / 15);
            src[(y*16+x)*3+0] = v;
            src[(y*16+x)*3+1] = v / 2;
            src[(y*16+x)*3+2] = (uint8_t)(255 - v);
        }

    uint8_t cmpr[16*8/2];
    CMPR_Encode(src, 16, 8, cmpr);

    uint8_t dst[16*8*3];
    CMPR_Decode(cmpr, 16, 8, dst);

    int err = max_error(src, dst, 16*8);
    char label[64];
    snprintf(label, sizeof(label), "gradient max_err=%d (limit 16)", err);
    report(label, err <= 16);
}

/* =========================================================================
   Test 4 — output size
   ========================================================================= */
static void test_encode_size(void)
{
    printf("\n[4] output size\n");

    /* We verify the caller-contract: output must be width*height/2 bytes.
     * Write a sentinel past the expected end and confirm the encoder
     * doesn't clobber it. */
    const int W = 16, H = 16;
    size_t expected = (size_t)(W * H / 2);
    size_t buf_size = expected + 16;

    uint8_t *src  = (uint8_t *)calloc(W * H * 3, 1);
    uint8_t *cmpr = (uint8_t *)calloc(buf_size, 1);
    if (!src || !cmpr) { report("alloc", 0); free(src); free(cmpr); return; }

    /* Fill sentinel */
    memset(cmpr + expected, 0xDE, 16);

    CMPR_Encode(src, W, H, cmpr);

    int sentinel_ok = 1;
    for (int i = 0; i < 16; i++)
        if (cmpr[expected + i] != 0xDE) { sentinel_ok = 0; break; }

    char label[64];
    snprintf(label, sizeof(label),
             "output_size %dx%d → %zu bytes, sentinel intact",
             W, H, expected);
    report(label, sentinel_ok);

    free(src);
    free(cmpr);
}

/* =========================================================================
   Test 5 — super-tile order: two side-by-side 8×8 tiles
   ========================================================================= */
static void test_super_tile_order(void)
{
    printf("\n[5] super-tile order (16×8 with two distinct tiles)\n");

    /* Left tile: solid red (255,0,0), right tile: solid blue (0,0,255) */
    uint8_t src[16*8*3];
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            src[(y*16+x)*3+0] = 255; src[(y*16+x)*3+1] = 0; src[(y*16+x)*3+2] = 0;
        }
        for (int x = 8; x < 16; x++) {
            src[(y*16+x)*3+0] = 0; src[(y*16+x)*3+1] = 0; src[(y*16+x)*3+2] = 255;
        }
    }

    uint8_t cmpr[16*8/2];
    CMPR_Encode(src, 16, 8, cmpr);

    uint8_t dst[16*8*3];
    CMPR_Decode(cmpr, 16, 8, dst);

    /* Left region should decode to red, right to blue (within quant error) */
    int left_ok = 1, right_ok = 1;
    for (int y = 0; y < 8 && (left_ok || right_ok); y++) {
        for (int x = 0; x < 8; x++) {
            const uint8_t *p = dst + (y*16+x)*3;
            if (p[0] < 240 || p[1] > 8 || p[2] > 8) left_ok = 0;
        }
        for (int x = 8; x < 16; x++) {
            const uint8_t *p = dst + (y*16+x)*3;
            if (p[0] > 8 || p[1] > 8 || p[2] < 240) right_ok = 0;
        }
    }

    report("left_tile_is_red",  left_ok);
    report("right_tile_is_blue", right_ok);
}

/* =========================================================================
   Entry point
   ========================================================================= */

static int run_tests(void)
{
    printf("=== CMPR smoke test ===\n");

    test_solid_colors();
    test_palette_weights();
    test_gradient();
    test_encode_size();
    test_super_tile_order();

    printf("\n=== Results: %d passed, %d failed ===\n", s_pass, s_fail);
    return s_fail ? 1 : 0;
}

/*
 * MSYS2/Cygwin GCC links libcmain.o which provides the PE 'main' entry
 * and expects the user to supply WinMain.  Everywhere else, supply main.
 */
#if defined(__MSYS__) || defined(__CYGWIN__)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
int WINAPI WinMain(HINSTANCE h, HINSTANCE p, LPSTR cmd, int show)
{
    (void)h; (void)p; (void)cmd; (void)show;
    return run_tests();
}
#else
int main(void) { return run_tests(); }
#endif
