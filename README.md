# ioquake3-wii

A port of [ioQuake3](https://github.com/ioquake/ioq3) to the Nintendo Wii,
using devkitPPC + libogc, and OpenGX.

## Prerequisites (Windows)

### 1. Install devkitPro

1. Download the devkitPro installer:
   https://github.com/devkitPro/installer/releases/latest
2. Run it. When asked which packages to install, select:
   - **devkitPPC** (the PowerPC cross-compiler)
   - **Wii Libraries** (`wii-dev`)  
   - **libfat-ogc**, **libogc**, **wiiuse**, **asndlib** are included in wii-dev
3. Accept the default install path (`C:/devkitPro`).
4. The installer sets `DEVKITPRO` and `DEVKITPPC` environment variables
   automatically. Open a new terminal and verify:
   ```
   echo %DEVKITPRO%    → C:/devkitPro
   echo %DEVKITPPC%    → C:/devkitPro/devkitPPC
   ```

### 2. Clone ioQuake3

Clone the upstream ioQ3 repo **alongside** this folder (not inside it):

```
projects/
├── ioq3/             ← git clone https://github.com/ioquake/ioq3.git
└── ioquake3-wii/     ← this repo
```

If you want a different layout, adjust `IOQ3_DIR` in the Makefile.

### 3. Install MSYS2 or use Git Bash

The devkitPro installer ships MSYS2. Use the **MSYS2 devkitPro shell**
(Start menu → devkitPro → MSYS2) for all build commands.

---

## Building

### Step 1 — Patch ioQ3 (run once)

```bash
cd ioquake3-wii
bash apply_patches.sh
```

This adds a `#elif defined(GEKKO)` block to `../ioq3/code/qcommon/q_platform.h`
so ioQ3 recognises the Wii as a valid target platform. It only needs to be run
once. You can always revert with `cd ../ioq3 && git checkout code/qcommon/q_platform.h`.

### Step 2 — Build

```bash
# Wiimote + Nunchuk (default)
make dol

# GameCube controller
make INPUT_BACKEND=gamecube dol
```

---

## SD card layout

```
SD:/
├── apps/
│   └── ioquake3/
│       ├── boot.dol      ← your compiled binary
│       └── meta.xml      ← from this repo
└── quake3/
    └── baseq3/
        ├── pak0.pk3      ← from your Quake III Arena disc / purchase
        ├── pak1.pk3
        ├── ...
        └── pak8.pk3
```

**You need the original Quake III Arena data files.** The demo pk3 files
will also work for testing if you own the game.

---

## Controls

### Wiimote (sideways) + Nunchuk
#### Not working ATM

| Input | Action |
|---|---|
| Nunchuk stick | Move forward/back + strafe |
| D-pad ↑↓ | Look up / down |
| D-pad ←→ | Turn left / right |
| **B** (trigger) | Fire |
| **A** | Use / activate |
| **C** (Nunchuk) | Jump |
| **Z** (Nunchuk) | Crouch / walk |
| **+** | Next weapon |
| **-** | Previous weapon |
| **1** | Toggle console |
| **2** | Escape / menu |
| **HOME** | Quit to Homebrew Channel |

### GameCube controller (`make INPUT_BACKEND=gamecube`)

| Input | Action |
|---|---|
| Left stick | Move forward/back + strafe |
| C-stick | Look (mouselook) |
| **Z** | Fire |
| **A** | Jump |
| **B** | Crouch / walk |
| **X** | Use / activate |
| **Y** | Toggle console |
| **L** (trigger) | Previous weapon |
| **R** (trigger) | Next weapon |
| D-pad | Menu navigation |
| **Start** | Escape / menu |

> Note: the GC controller has no HOME button. Use Start → Quit in the in-game menu to exit.

---

## Renderer path (important!)

The Wii has no OpenGL driver. ioQuake3's `ref_gl` renderer calls OpenGL 1.x.
There are two approaches — start with Path A:

### ReqglGX wrapper

[glGX](https://github.com/Crayon2000/glGX) translates a subset of OpenGL 1.x
into GX calls. This lets ref_gl run with minimal changes.

```bash
git clone https://github.com/Crayon2000/glGX.git
# Copy glGX.c and glGX.h into code/renderer/
# Add -DUSE_GLGX=1 to CFLAGS in Makefile
# Add code/renderer/glGX.c to the source list
```

glGX doesn't implement everything, but it covers enough for Q3's fixed-function
pipeline (texturing, blending, fog, alpha test).

## Memory budget (88 MB)

| Region | Size | Notes |
|---|---|---|
| com_hunkMegs | 48 MB | Main heap (maps, shaders, models) |
| com_zoneMegs | 8 MB | Dynamic allocs |
| GX FIFO | 256 KB | Command buffer |
| Audio buffers | ~64 KB | Two ASND buffers |
| Framebuffers | ~2.4 MB | Two XFB at 640×480 |
| Stack + misc | ~2 MB | |
| **Total** | **~61 MB** | Leaves ~27 MB headroom |

If you hit MEM1 exhaustion (crash / black screen), reduce `com_hunkMegs`
first, then lower `r_picmip` (reduces texture memory).

---
## Known missing pieces (v0.1)

- [ ] IR (Wiimote pointer) mouselook
- [ ] On-screen keyboard for console input
- [ ] Classic Controller support

---

## License

ioQuake3 is GPLv2. This port layer is also GPLv2. See `LICENSE`.
