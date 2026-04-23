# ioquake3-wii

A port of [ioQuake3](https://github.com/ioquake/ioq3) to the Nintendo Wii,
using devkitPPC + libogc and [OpenGX](https://github.com/devkitPro/opengx)
(OpenGL 1.x to GX translation layer).

## Status

- Boots, connects to servers, loads maps, enters gameplay
- All textures render correctly (map, icons, models, particles)
- Networking works (Wi-Fi, LAN discovery, internet server browser)
- GameCube controller with dual-stick analog input
- Cinematic (intro video) playback with audio

**Known issues:**

- Wiimote + Nunchuk input is **not working** at this time
- Single player/Wii hosting is **not working** at this time. Only Online/LAN multiplayer servers work.

## Prerequisites (Windows)

### 1. Install devkitPro

1. Download the devkitPro installer:
   https://github.com/devkitPro/installer/releases/latest
2. Run it. When asked which packages to install, select:
   - **devkitPPC** (the PowerPC cross-compiler)
   - **Wii Libraries** (`wii-dev`)
   - **libfat-ogc**, **libogc**, **wiiuse**, **asndlib** are included in wii-dev
3. Install OpenGX and zlib:
   ```
   pacman -S wii-opengx ppc-zlib
   ```
4. Accept the default install path (`C:/devkitPro`).
5. The installer sets `DEVKITPRO` and `DEVKITPPC` environment variables
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

### Step 1 -- Patch ioQ3 (run once)

```bash
cd ioquake3-wii
bash apply_patches.sh
```

This renames conflicting symbols in the upstream ioQ3 source so they don't
collide with the Wii port's own definitions. It only needs to be run once.

### Step 2 -- Build

```bash
# Quake III Arena, GameCube controller (recommended)
make INPUT_BACKEND=gamecube dol

# Quake III Arena, Wiimote + Nunchuk (not working ATM)
make dol

# OpenArena, GameCube controller
make INPUT_BACKEND=gamecube GAMEMODE=baseoa dol

# OpenArena, Wiimote + Nunchuk (not working ATM)
make GAMEMODE=baseoa dol

# Debug build (enables SD card diagnostic logging)
make INPUT_BACKEND=gamecube WII_DEBUG=1 dol
```

**Build flags:**

| Flag | Values | Default | Description |
|------|--------|---------|-------------|
| `INPUT_BACKEND` | `wiimote`, `gamecube` | `wiimote` | Controller input backend |
| `GAMEMODE` | `baseq3`, `baseoa` | `baseq3` | Game data directory to use |
| `WII_DEBUG` | `0`, `1` | `0` | Enable diagnostic logging to SD card |

**Output:** `build/ioquake3_wii.dol`

> **Note:** When switching between `GAMEMODE` or `INPUT_BACKEND` values, run
> `make clean` first to ensure all objects are rebuilt with the correct flags.

---

## SD card layout

### Quake III Arena (default)

```
SD:/
├── apps/
│   └── ioquake3/
│       ├── boot.dol      ← build/ioquake3_wii.dol
│       └── meta.xml
└── quake3/
    └── baseq3/
        ├── pak0.pk3      ← from your Quake III Arena disc / purchase
        ├── pak1.pk3
        ├── ...
        └── pak8.pk3
```

**You need the original Quake III Arena data files** (`pak0.pk3` through
`pak8.pk3`). The demo pk3 files will also work for testing.

### OpenArena (`GAMEMODE=baseoa`)

```
SD:/
├── apps/
│   └── ioquake3/
│       ├── boot.dol      ← build/ioquake3_wii.dol (built with GAMEMODE=baseoa)
│       └── meta.xml
└── quake3/
    └── baseoa/
        ├── pak0.pk3      ← from OpenArena download
        ├── pak1.pk3
        ├── ...
        └── pak6.pk3
```

[OpenArena](http://www.openarena.ws) is a free, standalone game using the
Quake III engine. Download the game data and place the `baseoa/` pk3 files
on the SD card as shown above.

---

## Controls

### GameCube controller (`INPUT_BACKEND=gamecube`)

Dual-stick FPS layout with analog movement and look. Buttons are rebindable
from the Q3 menu.

#### In-game

| Input | Action |
|---|---|
| Left stick | Move (forward/back + strafe) |
| C-stick | Look (yaw + pitch) |
| **R** trigger | Fire |
| **L** trigger | Walk |
| **A** | Jump |
| **B** | Crouch |
| **X** | Previous weapon |
| **Y** | Next weapon |
| **Z** | Zoom |
| D-pad up | Scoreboard |
| D-pad down | Fire (alt) |
| D-pad left/right | Prev/next weapon |
| **Start** | Menu (Escape) |

#### Menus

| Input | Action |
|---|---|
| Left stick / C-stick | Move cursor |
| **A** | Confirm (Enter) |
| **B** | Back (Escape) |
| **X** | Click |
| **Y** | Toggle console |
| D-pad | Arrow keys |
| **R** trigger | Click |
| **Start** | Escape |

> The GC controller has no HOME button. Use Start to open the menu and quit
> from there, or use the Wii's Power/Reset buttons to return to the
> Homebrew Channel.

### Wiimote + Nunchuk (default, not working ATM)

The Wiimote + Nunchuk input path is currently non-functional. Use the
GameCube controller backend (`INPUT_BACKEND=gamecube`) instead.

---

## Connecting to a server

Use the in-game server browser (internet + LAN).

---
<<<<<<< HEAD

## Memory budget

| Region | Size | Location | Notes |
|---|---|---|---|
| Hunk (`com_hunkMegs`) | 32 MB | MEM2 (top) | Maps, shaders, models |
| Zone (`com_zoneMegs`) | 8 MB | MEM1 | Dynamic allocs, zlib inflate |
| Sound (`com_soundMegs`) | 4 MB | MEM1 | Audio buffers |
| sbrk heap | ~19 MB | MEM2 (bottom) | OpenGX textures, memalign |
| GX FIFO | 256 KB | MEM1 | Command buffer |
| Framebuffers | ~2.4 MB | MEM1 | Two XFB at 640x480 |
| Stack | 512 KB | MEM1 | Overridden from 16 KB default |
=======
## Known missing pieces (v0.1)

- [ ] IR (Wiimote pointer) mouselook
- [ ] On-screen keyboard for console input
- [ ] Classic Controller support
>>>>>>> f0fa6d25a615391b33e0c92e1ada715e29a8b8c1

---

## License

ioQuake3 is GPLv2. This port layer is also GPLv2. See `LICENSE`.
