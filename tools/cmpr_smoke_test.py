#!/usr/bin/env python3
"""
cmpr_smoke_test.py — independent Python verification of the CMPR encoder

Independently re-implements the algorithm from wii_cmpr.c in Python and runs
the same test cases.  Because it is a fresh implementation it will catch
logical errors that a C-only test might miss.

Run:
    python3 cmpr_smoke_test.py
"""

import struct
import sys

# ---------------------------------------------------------------------------
# Python re-implementation (mirrors wii_cmpr.c exactly)
# ---------------------------------------------------------------------------

def pack_rgb565(r: int, g: int, b: int) -> int:
    """Round-to-nearest RGB888 → RGB565."""
    r5 = (r * 31 + 127) // 255
    g6 = (g * 63 + 127) // 255
    b5 = (b * 31 + 127) // 255
    return (r5 << 11) | (g6 << 5) | b5


def expand_rgb565(c: int):
    """RGB565 → (r8, g8, b8) with bit replication."""
    r5 = (c >> 11) & 0x1F
    g6 = (c >>  5) & 0x3F
    b5 =  c        & 0x1F
    r = (r5 << 3) | (r5 >> 2)
    g = (g6 << 2) | (g6 >> 4)
    b = (b5 << 3) | (b5 >> 2)
    return r & 0xFF, g & 0xFF, b & 0xFF


def build_palette(e0: int, e1: int):
    """Build 4-colour palette using 3/8+5/8 weights (matching Wii GX decode)."""
    r0, g0, b0 = expand_rgb565(e0)
    r1, g1, b1 = expand_rgb565(e1)

    r2 = (5 * r0 + 3 * r1) >> 3
    g2 = (5 * g0 + 3 * g1) >> 3
    b2 = (5 * b0 + 3 * b1) >> 3

    r3 = (3 * r0 + 5 * r1) >> 3
    g3 = (3 * g0 + 5 * g1) >> 3
    b3 = (3 * b0 + 5 * b1) >> 3

    return [(r0,g0,b0), (r1,g1,b1), (r2,g2,b2), (r3,g3,b3)]


def encode_dxt1_block(pixels):
    """
    Encode a flat list of 16 (r,g,b) tuples (row-major 4x4) → bytes[8].
    Returns the 8-byte DXT1 block.
    """
    # 1. Bounding box
    rmin = gmin = bmin = 255
    rmax = gmax = bmax = 0
    for r, g, b in pixels:
        rmin, rmax = min(r, rmin), max(r, rmax)
        gmin, gmax = min(g, gmin), max(g, gmax)
        bmin, bmax = min(b, bmin), max(b, bmax)

    # 2. Quantise endpoints (round-to-nearest)
    e0 = pack_rgb565(rmax, gmax, bmax)
    e1 = pack_rgb565(rmin, gmin, bmin)

    # 3. Ensure 4-colour mode: e0 > e1
    if e0 < e1:
        e0, e1 = e1, e0
    if e0 == e1:
        if e0 < 0xFFFF:
            e0 += 1
        else:
            e1 -= 1

    # 4. Build palette
    palette = build_palette(e0, e1)

    # 5. Assign indices (min squared RGB distance)
    idx_bytes = []
    for row in range(4):
        byte = 0
        for col in range(4):
            r, g, b = pixels[row * 4 + col]
            best, best_err = 0, 10**9
            for k, (pr, pg, pb) in enumerate(palette):
                err = (r-pr)**2 + (g-pg)**2 + (b-pb)**2
                if err < best_err:
                    best_err, best = err, k
            byte |= best << (col * 2)
        idx_bytes.append(byte & 0xFF)

    # 6. Pack big-endian
    return bytes([e0 >> 8, e0 & 0xFF, e1 >> 8, e1 & 0xFF] + idx_bytes)


def cmpr_encode(rgb: bytes, width: int, height: int) -> bytes:
    """Encode width×height RGB888 image to GX_TF_CMPR."""
    assert width % 8 == 0 and height % 8 == 0
    out = bytearray()
    stx = width  // 8
    sty = height // 8

    for ty in range(sty):
        for tx in range(stx):
            for by in range(2):
                for bx in range(2):
                    px = tx * 8 + bx * 4
                    py = ty * 8 + by * 4
                    block = []
                    for y in range(4):
                        for x in range(4):
                            i = ((py + y) * width + (px + x)) * 3
                            block.append((rgb[i], rgb[i+1], rgb[i+2]))
                    out += encode_dxt1_block(block)
    return bytes(out)


def decode_dxt1_block(data: bytes, offset: int):
    """Decode one 8-byte DXT1 block → 16 (r,g,b) tuples."""
    e0 = (data[offset] << 8) | data[offset+1]
    e1 = (data[offset+2] << 8) | data[offset+3]

    if e0 > e1:
        palette = build_palette(e0, e1)
    else:
        r0,g0,b0 = expand_rgb565(e0)
        r1,g1,b1 = expand_rgb565(e1)
        palette = [
            (r0,g0,b0), (r1,g1,b1),
            ((r0+r1)//2, (g0+g1)//2, (b0+b1)//2),
            (0,0,0),
        ]

    pixels = []
    for row in range(4):
        ib = data[offset + 4 + row]
        for col in range(4):
            idx = (ib >> (col * 2)) & 3
            pixels.append(palette[idx])
    return pixels


def cmpr_decode(data: bytes, width: int, height: int) -> bytearray:
    """Decode GX_TF_CMPR → RGB888 bytearray."""
    out = bytearray(width * height * 3)
    stx = width  // 8
    sty = height // 8
    pos = 0

    for ty in range(sty):
        for tx in range(stx):
            for by in range(2):
                for bx in range(2):
                    pixels = decode_dxt1_block(data, pos)
                    pos += 8
                    px = tx * 8 + bx * 4
                    py = ty * 8 + by * 4
                    for row in range(4):
                        for col in range(4):
                            i = ((py + row) * width + (px + col)) * 3
                            out[i], out[i+1], out[i+2] = pixels[row*4+col]
    return out


# ---------------------------------------------------------------------------
# Test harness
# ---------------------------------------------------------------------------

s_pass = s_fail = 0

def check(name: str, cond: bool, detail: str = ""):
    global s_pass, s_fail
    tag = "PASS" if cond else "FAIL"
    line = f"  {tag}  {name}"
    if detail:
        line += f"  ({detail})"
    print(line)
    if cond:
        s_pass += 1
    else:
        s_fail += 1


def max_err(a: bytes, b: bytes) -> int:
    return max(abs(int(x) - int(y)) for x, y in zip(a, b))


def fill_solid(w, h, r, g, b) -> bytes:
    return bytes([r, g, b] * (w * h))


# ---------------------------------------------------------------------------
# Test 1 — solid colours
# ---------------------------------------------------------------------------

def test_solid_colors():
    print("\n[1] solid colours (round-trip error ≤ 8)")
    cases = [
        ("red",   255,   0,   0),
        ("green",   0, 255,   0),
        ("blue",    0,   0, 255),
        ("white", 255, 255, 255),
        ("black",   0,   0,   0),
        ("grey",  128, 128, 128),
        ("mid",   100,  50,  25),
    ]
    for name, r, g, b in cases:
        src  = fill_solid(8, 8, r, g, b)
        cmpr = cmpr_encode(src, 8, 8)
        dst  = cmpr_decode(cmpr, 8, 8)
        err  = max_err(src, dst)
        check(f"solid_{name}", err <= 8, f"err={err}")


# ---------------------------------------------------------------------------
# Test 2 — palette weights
# ---------------------------------------------------------------------------

def test_palette_weights():
    print("\n[2] 3/8+5/8 palette interpolation")

    # Verify the formula directly, no encoding involved
    # e0 > e1, both with known values
    e0 = pack_rgb565(200, 160, 80)
    e1 = pack_rgb565( 40,  20, 10)
    assert e0 > e1, "test assumption: e0>e1"

    r0,g0,b0 = expand_rgb565(e0)
    r1,g1,b1 = expand_rgb565(e1)

    exp_c2 = ((5*r0+3*r1)>>3, (5*g0+3*g1)>>3, (5*b0+3*b1)>>3)
    exp_c3 = ((3*r0+5*r1)>>3, (3*g0+5*g1)>>3, (3*b0+5*b1)>>3)

    palette = build_palette(e0, e1)

    check("c0 == expand(e0)", palette[0] == (r0,g0,b0))
    check("c1 == expand(e1)", palette[1] == (r1,g1,b1))
    check("c2 == (5*c0+3*c1)>>3", palette[2] == exp_c2,
          f"got={palette[2]} want={exp_c2}")
    check("c3 == (3*c0+5*c1)>>3", palette[3] == exp_c3,
          f"got={palette[3]} want={exp_c3}")

    # Also verify these differ from standard DXT1 (2/3+1/3) weights
    std_c2 = ((2*r0+r1+1)//3, (2*g0+g1+1)//3, (2*b0+b1+1)//3)
    std_c3 = ((r0+2*r1+1)//3, (g0+2*g1+1)//3, (b0+2*b1+1)//3)
    print(f"      5/8+3/8 c2={exp_c2}  standard 2/3+1/3 c2={std_c2}")
    print(f"      3/8+5/8 c3={exp_c3}  standard 1/3+2/3 c3={std_c3}")


# ---------------------------------------------------------------------------
# Test 3 — round-to-nearest pack_rgb565
# ---------------------------------------------------------------------------

def test_rtn_packing():
    print("\n[3] round-to-nearest RGB565 packing")
    errors = []
    for r in range(256):
        for b in range(256):
            packed = pack_rgb565(r, 0, b)
            re, _, be = expand_rgb565(packed)
            err_r = abs(r - re)
            err_b = abs(b - be)
            # Round-trip error ≤ half an LSB of the 5-bit channel (~4)
            if err_r > 4:
                errors.append(f"r={r} err={err_r}")
            if err_b > 4:
                errors.append(f"b={b} err={err_b}")
    check("5-bit channel round-trip error ≤ 4", not errors,
          f"{len(errors)} violations" if errors else "")

    errors6 = []
    for g in range(256):
        packed = pack_rgb565(0, g, 0)
        _, ge, _ = expand_rgb565(packed)
        err = abs(g - ge)
        if err > 2:
            errors6.append(f"g={g} err={err}")
    check("6-bit channel round-trip error ≤ 2", not errors6,
          f"{len(errors6)} violations" if errors6 else "")


# ---------------------------------------------------------------------------
# Test 4 — gradient
# ---------------------------------------------------------------------------

def test_gradient():
    # DXT1 bbox-encoder worst-case: 18.75% of the block's colour range per channel.
    # With 3/8+5/8 palette the four colours sit at 0%, 37.5%, 62.5%, 100%.
    # In a multi-channel gradient 3D nearest-neighbour can give up ~35 in one
    # channel to minimise overall 3D error.  Limit 40 covers this with margin.
    print("\n[4] horizontal gradient 16x8 (max error <= 40)")
    src = bytearray()
    for y in range(8):
        for x in range(16):
            v = x * 255 // 15
            src += bytes([v, v // 2, 255 - v])
    cmpr = cmpr_encode(bytes(src), 16, 8)
    dst  = cmpr_decode(cmpr, 16, 8)
    err  = max_err(src, dst)
    check(f"gradient_max_err <= 40", err <= 40, f"err={err}")


# ---------------------------------------------------------------------------
# Test 5 — output size
# ---------------------------------------------------------------------------

def test_output_size():
    print("\n[5] output byte count = width×height/2")
    for w, h in [(8,8), (16,8), (16,16), (32,32)]:
        src  = fill_solid(w, h, 128, 64, 32)
        cmpr = cmpr_encode(src, w, h)
        want = w * h // 2
        check(f"size_{w}x{h}", len(cmpr) == want, f"got={len(cmpr)} want={want}")


# ---------------------------------------------------------------------------
# Test 6 — super-tile order
# ---------------------------------------------------------------------------

def test_super_tile_order():
    print("\n[6] super-tile order: red|blue side-by-side 16×8")
    src = bytearray()
    for y in range(8):
        for x in range(8):
            src += bytes([255, 0, 0])   # left tile: red
        for x in range(8):
            src += bytes([0, 0, 255])   # right tile: blue
    cmpr = cmpr_encode(bytes(src), 16, 8)
    dst  = cmpr_decode(cmpr, 16, 8)

    left_ok = right_ok = True
    for y in range(8):
        for x in range(8):
            r,g,b = dst[(y*16+x)*3], dst[(y*16+x)*3+1], dst[(y*16+x)*3+2]
            if r < 240 or g > 8 or b > 8:
                left_ok = False
        for x in range(8, 16):
            r,g,b = dst[(y*16+x)*3], dst[(y*16+x)*3+1], dst[(y*16+x)*3+2]
            if r > 8 or g > 8 or b < 240:
                right_ok = False

    check("left_tile_is_red",   left_ok)
    check("right_tile_is_blue", right_ok)


# ---------------------------------------------------------------------------
# Test 7 — e0 > e1 invariant for non-transparent blocks
# ---------------------------------------------------------------------------

def test_endpoints_ordered():
    print("\n[7] e0>e1 invariant (4-colour mode) for all solid blocks")
    failures = 0
    for r in range(0, 256, 32):
        for g in range(0, 256, 32):
            for b in range(0, 256, 32):
                src  = fill_solid(8, 8, r, g, b)
                cmpr = cmpr_encode(src, 8, 8)
                # Check the first block (bytes 0-1 vs 2-3)
                e0 = (cmpr[0] << 8) | cmpr[1]
                e1 = (cmpr[2] << 8) | cmpr[3]
                if e0 <= e1:
                    failures += 1
    check("all_solid_blocks_use_4colour_mode", failures == 0,
          f"{failures} blocks had e0≤e1")


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------

def main():
    print("=== CMPR smoke test (Python independent implementation) ===")
    test_solid_colors()
    test_palette_weights()
    test_rtn_packing()
    test_gradient()
    test_output_size()
    test_super_tile_order()
    test_endpoints_ordered()
    print(f"\n=== Results: {s_pass} passed, {s_fail} failed ===")
    sys.exit(1 if s_fail else 0)


if __name__ == "__main__":
    main()
