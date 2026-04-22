# ioquake3-wii — Claude context

## What this project is

A port of [ioquake3](https://github.com/ioquake/ioq3) to the Nintendo Wii using
devkitPPC + libogc.  The Wii has no OpenGL driver; rendering goes through
**OpenGX** (an OpenGL 1.x→GX translation layer) so the unmodified ioQ3
renderergl1 pipeline works over GX hardware.

## Repository layout

```
ioquake3-wii/          ← this repo (Wii port glue)
../ioq3/               ← upstream ioquake3 source (sibling dir, not modified)
```

Key files in this repo:

| File | Role |
|------|------|
| `code/sys/wii_main.c` | Entry point — hardware init, SD mount, `Com_Init` |
| `code/sys/wii_glimp.c` | GX + OpenGX init; `Wii_GX_Init` calls `ogx_initialize()` after `GX_Init` |
| `code/sys/wii_platform.h` | Force-included (`-include`) before every TU; defines platform compat |
| `code/sys/include/SDL_opengl.h` | With `OPENGX_AVAILABLE`: `#include <GL/gl.h>` (OpenGX); else stub types |
| `code/sys/include/GL/gl.h` | With `OPENGX_AVAILABLE`: `#include_next <GL/gl.h>` to reach OpenGX header |
| `code/sys/include/wii_gl_compat.h` | Minimal GL type/constant stub (used when OpenGX is not available) |
| `code/renderer/wii_renderer.c` | Defines global `ri`; pre-boot stub `wii_re`; two-phase `GetRefAPI` |
| `code/renderer/wii_gl_stubs.c` | Defines `qgl*` NULL pointer globals using `QGL_*_PROCS` macros |
| `code/renderer/qgl_wii.c` | `QGL_Init()` — wires `qgl*` → OpenGX `gl*` after file system is up |
| `code/input/wii_input.c` | Wiimote+Nunchuk input; button mapping; analogue stick → SE_MOUSE |
| `Makefile` | Build system; see "Building" below |
| `build_wii.bat` | Windows batch file that sets TEMP/env and runs make + elf2dol |

## Architecture: two-phase renderer init

```
main()
  └─ Wii_GX_Init()          — GX_Init + ogx_initialize(); GX/OpenGX ready
  └─ GetRefAPI(ver, NULL)   — returns pre-boot stub (wii_re); all no-ops
  └─ re = *ref              — cl_main.c's `re` gets safe no-op function table
  └─ Com_Init()
       └─ CL_InitRef()
            └─ GetRefAPI(ver, rimp)  — rimp != NULL → real path:
                 └─ QGL_Init()       — wires qgl* → OpenGX gl*
                 └─ tr_init_GetRefAPI_unused()  — ioQ3's real renderer live
```

`tr_init_GetRefAPI_unused` is the original `GetRefAPI` from `tr_init.c`,
renamed by `apply_patches.sh` to avoid collision with our `GetRefAPI`.

## Building

**Prerequisites:** devkitPro with `wii-dev` + `wii-opengx` installed.

```
pacman -S wii-dev wii-opengx
```

**On Windows** (devkitPro at `C:\devkitPro`), use the batch file:

```
E:\Users\Matteo\Desktop\quake3\build_wii.bat
```

This sets `TEMP`, `DEVKITPRO`, `DEVKITPPC` correctly for native Windows
toolchain binaries (GCC and LD are `.exe` files that read Windows env vars)
and runs make + `elf2dol`.

**Key CFLAGS:**
- `-pipe` — avoids GCC writing temp files (Windows TEMP is `C:\Windows\` on this machine)
- `-DOPENGX_AVAILABLE` — switches `GL/gl.h` stub to real OpenGX header via `#include_next`
- `-I$(OPENGX_INC)` — `$(DEVKITPRO)/portlibs/wii/include`

**Output:** `build/ioquake3_wii.elf` + `build/ioquake3_wii.dol`

Deploy: copy `build/ioquake3_wii.dol` → `sd:/apps/ioquake3/boot.dol`

## ioQ3 renderer files compiled

The following renderergl1 files from `../ioq3/` are compiled in (in addition
to the standard set):

```
tr_backend.c    — GL_Bind, GL_State, backEnd, tess, R_GetCommandBuffer
tr_cmds.c       — R_IssuePendingRenderCommands, R_AddDrawSurfCmd, RE_BeginFrame, RE_EndFrame
tr_flares.c     — RB_AddFlare, RB_RenderFlares
tr_image.c      — R_CreateImage, R_FindImageFile, R_InitImages
tr_shade.c      — RB_StageIteratorGeneric, RB_StageIteratorLightmappedMultitexture
tr_shadows.c    — RB_ProjectionShadowDeform, RB_ShadowTessEnd
tr_sky.c        — R_InitSkyTexCoords, sky rendering
tr_surface.c    — surface drawing helpers
```

## Key gotchas

### GL header shadowing
`code/sys/include/GL/gl.h` comes BEFORE `$(DEVKITPRO)/portlibs/wii/include`
in the `-I` search order. Without `OPENGX_AVAILABLE` it's a type-only stub.
With `OPENGX_AVAILABLE` it uses `#include_next <GL/gl.h>` to forward to the
real OpenGX header. **Do not remove this file** — it must intercept the include
to handle both cases.

### GLchar
OpenGX is GL 1.x; it doesn't define `GLchar` (a GL 2.0 type). ioQ3's `qgl.h`
uses it in `QGL_2_0_PROCS`. We `typedef char GLchar` in `SDL_opengl.h` after
the OpenGX include.

### qgl* pointer coverage
Only the `qgl*` pointers declared in ioQ3's `qgl.h` GLE macro groups exist.
The renderergl1 does NOT call `qglFogf`, `qglRotatef`, `qglNormal3f`, etc. —
those were Wii-specific additions in the old renderer and are gone. Do not add
them back to `qgl_wii.c`.

### Z_Malloc is a debug macro
In non-NDEBUG builds `Z_Malloc(size)` expands to
`Z_MallocDebug(size, #size, __FILE__, __LINE__)`. You cannot take its address.
Use a wrapper: `static void *wii_ri_Malloc(int s) { return Z_Malloc(s); }`.

### OpenGX double-precision API
OpenGX provides the standard desktop GL double-signature functions
(`glFrustum`, `glOrtho`, `glDepthRange`, `glClearDepth`) — NOT the OpenGL ES
float variants (`glFrustumf` etc.). Assign directly; no wrappers needed.

### ogx_initialize()
Declared in `<opengx.h>`. Must be called after `GX_Init()`. It owns the
vertex attribute format pipeline — do NOT call `GX_ClearVtxDesc` /
`GX_SetVtxDesc` / `GX_SetVtxAttrFmt` after `ogx_initialize()`.

### Memory budget
- MEM1 ≈ 24 MB; hunk + zone must fit. Current: `com_hunkMegs 12` + `com_zoneMegs 4` = 16 MB.
- MEM2 = 64 MB, claimed by `Wii_MEM2_Init` for a custom allocator.
- Stack: `STACKSIZE = 256 KB` (devkitPPC default 16 KB overflows ioQ3 deep frames).

### apply_patches.sh
Patches the upstream ioq3 source before building:
- Renames `ri` in `tr_main.c` → `tr_main_ri_unused` (we define the sole `ri`)
- Renames `GetRefAPI` in `tr_init.c` → `tr_init_GetRefAPI_unused`
- Adds `#ifndef` guards around constants that `wii_platform.h` pre-defines

### GL_COMPRESSED_RGB → heap corruption — do NOT redirect glTexImage2D to GL_COMPRESSED_RGB
OpenGX has `_ogx_convert_rgb_image_to_DXT1` internally but its DXT1 path has a heap
corruption bug: passing `GL_COMPRESSED_RGB` as internal_format to `glTexImage2D` corrupts
the malloc free list, causing a DSI crash inside `_malloc_r` (PC in 0x801B3xxx range, DAR
at a garbage address like 0x55BD8F74) tens of seconds later.  `wii_cmpr.c` (our own encoder)
is compiled but not wired.  Leave `qglTexImage2D = glTexImage2D` (plain RGB565).

### r_texturebits / r_colorbits — do NOT set to 16
Setting `r_texturebits 16` causes `tr_image.c` to convert all textures to
16-bit formats (`GL_RGBA4` / `GL_RGB5_A1`) before `glTexImage2D`. Font and
cursor textures are RGBA with a meaningful alpha channel — the 16-bit
conversion mangles or strips alpha, causing OpenGX to render solid red/colored
blocks instead of transparent glyphs. Leave both cvars at their defaults (32).

### GLimp_Init signature
ioQ3 declares `void GLimp_Init(qboolean fixedFunction)` — NOT the older
`qboolean GLimp_Init(glconfig_t *config)` signature. ioQ3 calls
`GLimp_Init(qtrue)` (integer 1). If you use the wrong signature, r31=1 arrives
as a `glconfig_t*` pointer, writing to address 0x2C29 → DSI crash on boot.
The correct implementation fills the global `glConfig` struct directly.

### EndFrame pipeline (Wii_GX_EndFrame)
Do not use `GX_DrawDone()` (blocking stall) + `VIDEO_WaitVSync()` (stall) in
sequence — that double-stalls every frame causing severe choppiness. Correct
async pipeline:
```c
ogx_prepare_swap_buffers();  // OpenGX internal flush
GX_SetDrawDone();            // async GPU marker
s_fb_index ^= 1;
GX_CopyDisp(s_framebuf[s_fb_index], GX_TRUE);  // async EFB→XFB copy
GX_Flush();
VIDEO_SetNextFramebuffer(s_framebuf[s_fb_index]);
VIDEO_Flush();
VIDEO_WaitVSync();           // single stall, keeps frame timing
```

### SD card layout expected
```
sd:/quake3/
  baseq3/        ← pak0.pk3 … pak8.pk3
  qkey           ← auto-created on first boot (2048 bytes)
apps/ioquake3/boot.dol
```

## Input system

`code/input/wii_input.c` — Wiimote held sideways + Nunchuk:

| Control | Q3 action |
|---------|-----------|
| Nunchuk stick | SE_MOUSE (camera yaw/pitch + menu cursor) |
| D-pad up/down | Look up / look down |
| D-pad left/right | Turn left / turn right |
| B (trigger) | Attack (K_MOUSE1) |
| A | Use / Enter (K_ENTER) |
| Nunchuk C | Jump |
| Nunchuk Z | Crouch |
| + / − | Next / prev weapon |
| 1 | Console toggle |
| 2 | Escape / menu |
| HOME | Quit (handled in main loop) |

**Stick sensitivity** uses a float accumulator (`s_accum_x/y`) so fractional
pixels carry over between frames — gives smooth sub-pixel scaling at partial
deflection. `STICK_SENSITIVITY_F` (currently `1.0f`) = pixels/frame at full
deflection. Deadzone is `STICK_DEADZONE = 20` (raw -127..127 scale).
The accumulator resets to zero when the stick returns to centre so there is
no drift. To slow the cursor further reduce `STICK_SENSITIVITY_F`.

## Performance cvars (set in wii_main.c cmdline)

| Cvar | Value | Reason |
|------|-------|--------|
| `com_hunkMegs` | 12 | MEM1 budget |
| `com_zoneMegs` | 4 | MEM1 budget |
| `r_picmip` | 2 | Halve textures twice → 4× smaller |
| `r_dynamic` | 0 | No dynamic lights (very expensive on GX) |
| `r_flares` | 0 | No lens flares |
| `r_fastsky` | 1 | Solid colour sky instead of sky box |
| `r_lodbias` | 2 | Aggressive LOD reduction |
| `r_subdivisions` | 20 | Less curved-surface tessellation |
| `r_simpleMipMaps` | 1 | Skip fancy mip generation |
| `r_drawSun` | 0 | No sun glare overlay |
| `com_maxfps` | 30 | Cap at 30 — consistent beats stuttering 60 |
| `s_khz` | 22 | Reduce audio sample rate |

Do NOT add `r_texturebits 16` or `r_colorbits 16` — see gotcha above.

## Crash debugging

The binary writes two files on boot:
- `sd:/quake3/boot.txt` — written immediately when `main()` is reached
- `sd:/quake3/crash.txt` — persistent crash log, flushed on every write

A red screen on the Wii is a DSI exception (hardware fault — NULL deref,
misaligned access, or bad function pointer call). Check the crash log first.
Key registers: `DAR` = faulting address, `DSISR` encodes access type,
`PC` = instruction that faulted, `r31` = often a recently-passed argument.

### OpenGX scissor Y-flip bug with off-screen viewports

OpenGX `update_scissor()` (`opengx/src/gc_gl.c`) converts OpenGL (bottom-up) scissor
coords to GX (top-down) using **viewport height** as the flip reference:

```c
y = glparamstate.viewport[3] - (params[3] + params[1]);
GX_SetScissor(params[0], y, params[2], params[3]);
```

This is only correct when `viewport_h == screen_h`.  The Q3 player-model UI
viewport overhangs the screen: `x=400 y=-40 w=320 h=560` on a 640×480 screen.
Using viewport_h=560 instead of screen_h=480 shifts the GX scissor down by
80 rows, cutting off the top of the frame.

**Fix** (in `ioq3/code/renderergl1/tr_backend.c`, `SetViewportAndScissor`):
Back-solve the scissor_y to pass so OpenGX's formula produces the correct
`GX_SetScissor` output:

```c
// Visible intersection with screen
int gx_sx = max(0, vx);  int gx_sy = max(0, vy);
int sw = min(screen_w, vx+vw) - gx_sx;
int sh = min(screen_h, vy+vh) - gx_sy;
// Reverse the OpenGX formula: GX_y = vh - (h + y) → y = vh - h - GX_y
int scissor_y = vh - sh - gx_sy;
qglScissor(gx_sx, scissor_y, sw, sh);
```

This lives in a `#ifdef GEKKO` block.  The `#else` branch keeps the standard
`qglScissor(viewport...)` unchanged for desktop builds.

## Networking — libogc IOS socket quirks

The Wii networking goes through Nintendo's IOS (internal OS) socket layer, not
a standard POSIX stack. `wii_net.h` provides shims that adapt Q3's net_ip.c
to libogc's `net_*` API. Several non-obvious quirks apply:

### IOS socket init sequence
`if_config()` and `net_socket()` use **different IOS devices**:
- `if_config()` → `/dev/net/ncd/manage` (Wi-Fi association + DHCP)
- `net_socket()` → `/dev/net/ip/top` (socket creation)

`if_config()` does **not** open `/dev/net/ip/top`. You must call `net_init()`
explicitly first, or `net_socket()` will return a non-(-1) error code (e.g.
-123) that Q3's `== INVALID_SOCKET` check misses. `Wii_Net_Init()` in
`wii_net.h` now calls `net_init()` in a poll loop before `if_config()`.

### IOS error code normalisation
libogc's `net_socket`, `net_bind`, etc. return IOS-specific negative codes on
failure (e.g. -123, -81) — **not -1**. Q3 checks `== INVALID_SOCKET (-1)` so
all such errors are silently ignored. `wii_net.h` wraps `socket()` and `bind()`
to normalise any negative return → -1 so Q3's error paths fire correctly.

### net_socket protocol must be 0
`net_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP=17)` returns IOS error -123
("unknown protocol"). IOS infers UDP/TCP from the socket type; the protocol
argument must be **0**. `wii_net_socket()` unconditionally passes `p=0`.

### sockaddr sin_len must be set (BSD-style struct)
libogc's `<netinet/in.h>` defines:
```c
struct sockaddr_in { uint8_t sin_len; uint8_t sin_family; uint16_t sin_port; ... };
struct sockaddr     { uint8_t sa_len;  uint8_t sa_family; char sa_data[14]; };
```
IOS validates `sin_len == sizeof(struct sockaddr_in)` in `net_bind` and
`net_sendto`. Q3 (POSIX) `memset`s the struct to 0, so `sin_len` stays 0 →
IOS returns EINVAL (-22). `wii_net_bind()` and `wii_net_sendto()` copy the
address struct and set `sin_len = sizeof(struct sockaddr_in)` before every call.

### net_port must differ from server port
Q3 binds both client and server to `net_port` (default 27960). When connecting
to a server also on 27960, many home routers can't NAT-hairpin same
source/dest port. `wii_main.c` sets `+set net_port 27961` so the Wii client
socket uses port 27961, avoiding the conflict.

### Makefile: net_ip.o and wii_main.o must depend on wii_net.h
`wii_net_sendto`, `wii_net_socket`, etc. are `static inline` in `wii_net.h`,
baked into `net_ip.o` and `wii_main.o` at compile time. Both are listed as
explicit Makefile dependencies so changes trigger recompilation:
```makefile
WII_NET_H := code/sys/wii_net.h
$(BUILD)/code/sys/wii_main.o: code/sys/wii_main.c $(WII_NET_H)
$(BUILD)/../ioq3/code/qcommon/net_ip.o: ../ioq3/code/qcommon/net_ip.c $(WII_NET_H)
```

### Auto-connect via connect.txt
Place server address (`ip:port`) in `sd:/quake3/connect.txt`. `wii_main.c`
reads it and appends `+connect <addr>` to the cmdline — deferred until after
`CL_Init` so the `connect` command exists when it runs.

### IOS sendto address format — RESOLVED (2026-04-15)

**Root cause**: IOS `/dev/net/ip/top` sendto ioctlv reads `sa_family` as a
`uint16_t` at bytes 0–1 (POSIX style — no `sin_len` prefix).  We were passing
the BSD 16-byte `sockaddr_in` with `sin_len` at byte 0 and `tolen=16`.

- With `sin_len=16`, IOS reads `sa_family = (16<<8)|2 = 4098` → EINVAL.
- Even with `sin_len=0` (Q3's memset'd struct), `tolen=16` causes IOS to
  reject the call (IOS validates `tolen==8` for AF_INET).
- The previous `sin_len=16` fix was making it **worse** — it changed byte 0
  from 0 to 16, turning a merely-wrong-length call into an invalid-family call.

**Fix** (`wii_net_sendto`): build an 8-byte POSIX-style address from the BSD
struct and pass `tolen=8`:
```
ios_addr[0-1] = (uint16_t)AF_INET = [0x00, 0x02]  // sa_family
ios_addr[2-3] = sin_port                           // already network byte order
ios_addr[4-7] = sin_addr                           // already network byte order
```
Confirmed by diagnostic tests E (connect+send=4) and F (sendto tolen=8 = 4).

`net_setsockopt(SO_BROADCAST)` still returns -109 (IOS doesn't support the
option), but IOS allows broadcast packets to 255.255.255.255 by default —
LAN discovery broadcasts work fine without it.

### net_select blocks with {0,0} timeout — RESOLVED (2026-04-15)

libogc's `net_select` issues an IOS ioctlv that IOS does not honour with a
zero timeout — it blocks indefinitely, hanging the Q3 frame loop on the first
`NET_GetPacket` call after the LAN server scan.

**Fix**: two parts, both required:

1. **`wii_net_select`** — always returns 1, bypassing `net_select` entirely.

2. **`wii_net_recvfrom`** — normalises IOS "would block" codes to `-1 + errno=EAGAIN`.
   IOS returns -128 (not -1/EAGAIN) when no packet is waiting on a non-blocking
   socket. Q3 checks `ret == SOCKET_ERROR (-1)`, so -128 falls through as a
   "successful -128-byte receive" → `net_message->cursize = -128` → corruption.
   Previously, `select` returning 0 prevented `recvfrom` from being called at all;
   once `select` was bypassed this latent bug became fatal, hanging the game before
   the main menu appeared. The wrapper maps -128 and -11 → `errno=EAGAIN`, -1.

Q3's `NET_GetPacket` handles EAGAIN (returns `qfalse`). Both fixes together
produce correct behaviour: server list populated from 1 393 internet servers,
LAN scan finds local servers, gameplay works.

### getaddrinfo / getnameinfo / ioctl
Provided in `wii_sys.c`: real `getaddrinfo` (numeric IPs via `inet_aton`,
hostnames via `net_gethostbyname`), `getnameinfo` via `inet_ntop`, and
`ioctl()` forwarding FIONBIO to `net_ioctl`.

## Previous crash / bug fixes (for reference)

| Symptom | Root cause | Fix |
|---------|-----------|-----|
| Red screen on "Fight" | `s_LoadWorld` → `GL_Bind` → NULL jump | OpenGX integration (real renderer handles BSP loading) |
| Red screen on "Fight" | `Wii_GX_BindTexture` etc. declared extern but never defined | Stub impls in `wii_glimp.c` (superseded by OpenGX) |
| Red screen on "Fight" | `SCR_UpdateScreen` from `Sys_Milliseconds` → re-entrant GX | Removed that call from `wii_sys.c` |
| Red screen on "Fight" | `com_hunkMegs 32 + com_zoneMegs 16 = 48 MB` from 24 MB MEM1 | Reduced to 12 + 4 MB in `wii_main.c` |
| DSI crash on boot (r31=1, DAR=0x2C29) | `GLimp_Init` signature mismatch — `qtrue` (=1) received as `glconfig_t*` | Corrected to `void GLimp_Init(qboolean fixedFunction)`; fills global `glConfig` directly |
| Choppy framerate | Double-stall in `Wii_GX_EndFrame` (`GX_DrawDone` + `WaitVSync`) | Async pipeline: `GX_SetDrawDone` + `GX_CopyDisp` + single `WaitVSync` |
| Font/cursor renders as red blocks | `r_texturebits 16` mangled alpha channel of RGBA font textures in OpenGX | Removed `r_texturebits 16` and `r_colorbits 16` from cmdline |
| Cursor too fast in menus | Integer `STICK_SENSITIVITY` floored fractional movement | Float accumulator with `STICK_SENSITIVITY_F = 1.0f` |
| Player model invisible in Setup→Player | Q3 UI places player-model viewport off-screen (y=-40, h=560); OpenGX scissor formula uses viewport_h not screen_h, shifting GX scissor 80px down | Back-solve scissor_y in `SetViewportAndScissor` — see "OpenGX scissor Y-flip bug" gotcha above |
| net_socket returns -123 / "Unknown protocol" | Passed `IPPROTO_UDP=17`; IOS requires `protocol=0` | `wii_net_socket()` always passes 0 |
| net_sendto returns -22 (EINVAL) | IOS `/dev/net/ip/top` sendto reads `sa_family` as `uint16_t` at bytes 0–1 (POSIX, no `sin_len`); we passed BSD 16-byte struct with `sin_len=16` at byte 0 → IOS read family=4098; even with `sin_len=0`, `tolen=16` rejected (IOS requires `tolen=8` for AF_INET) | `wii_net_sendto()` builds 8-byte POSIX addr `[family(u16), port, addr]` and passes `tolen=8` |
| Game hangs on LAN server scan | `net_select({0,0})` blocks indefinitely in libogc — IOS ignores zero timeout in the ioctlv | `wii_net_select` bypasses `net_select`, always returns 1; non-blocking socket + recvfrom EAGAIN handles "no data" correctly |
| Game hangs before main menu after select bypass | IOS `net_recvfrom` returns -128 ("would block"), not -1/EAGAIN; Q3 checks `== SOCKET_ERROR (-1)` so -128 was treated as a valid 128-byte receive → `cursize=-128` → crash/hang | `wii_net_recvfrom` maps -128 and -11 → `-1 + errno=EAGAIN`; Q3's EAGAIN path then correctly returns `qfalse` from `NET_GetPacket` |
| Networking silently broken despite Wi-Fi connected | `if_config()` uses `/dev/net/ncd/manage`; `net_socket()` needs `/dev/net/ip/top` opened first by `net_init()` | `Wii_Net_Init()` calls `net_init()` poll loop before `if_config()` |
| Q3 misses IOS error codes (e.g. -123) | IOS returns non-(-1) negatives; Q3 checks `== INVALID_SOCKET (-1)` | `wii_net_socket()` / `wii_net_bind()` normalise any `< 0` → `-1` |
