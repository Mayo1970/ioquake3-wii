/*
 * ioquake3-wii: sys/wii_main.c
 * Wii platform entry point.
 * Initialises hardware, mounts SD card, then hands off to ioQ3's common init.
 */

/* Override the main thread stack size.
 * libogc (tuxedo) sets the main stack via the weak symbol __ppc_main_sp,
 * which points to the top of s_mainStack[0x20000] (128 KB default).
 * ioQ3 has functions with 16 KB+ local buffers (e.g. char systemInfo[16384]
 * in sv_init.c) that overflow 128 KB on a deep call stack.
 * Defining __ppc_main_sp here overrides the weak default. */
static unsigned char s_mainStack[512 * 1024] __attribute__((aligned(8)));
/* Must be in .sdata — common_crt0.S accesses it via @sda21 relocation. */
void *__ppc_main_sp __attribute__((section(".sdata"))) = &s_mainStack[sizeof(s_mainStack)];

#include <gccore.h>
#include <wiiuse/wpad.h>
#include <fat.h>
#include <asndlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ioQuake3 qcommon interface */
#include "qcommon/q_shared.h"
#include "qcommon/qcommon.h"

/* Our Wii-specific subsystems */
#include "../sys/wii_glimp.h"
#include "../input/wii_input.h"
#include "../audio/wii_snd.h"
#include "../sys/wii_net.h"
#include "keycodes.h"
/* Renderer interface for pre-init */
#include "GL/gl.h"
#include "renderercommon/tr_types.h"
#include "renderercommon/tr_public.h"
extern refexport_t *GetRefAPI(int apiVersion, refimport_t *rimp);

/* --------------------------------------------------------------------------
 * Console / video init (used before the GX renderer is up, for debug text)
 * -------------------------------------------------------------------------- */
static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

static void Wii_InitConsole(void)
{
    VIDEO_Init();
    rmode = VIDEO_GetPreferredMode(NULL);
    xfb   = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    console_init(xfb, 20, 20, rmode->fbWidth, rmode->xfbHeight,
                 rmode->fbWidth * VI_DISPLAY_PIX_SZ);
    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(xfb);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if (rmode->viTVMode & VI_NON_INTERLACE)
        VIDEO_WaitVSync();
}

/* --------------------------------------------------------------------------
 * SD card mount
 * -------------------------------------------------------------------------- */
static qboolean Wii_MountSD(void)
{
    if (!fatInitDefault()) {
        printf("[wii] fatInitDefault() failed – no SD card?\n");
        return qfalse;
    }
    printf("[wii] SD card mounted OK\n");
    /* Set working directory so ioQ3 file code finds baseq3 */
    chdir("sd:/quake3");
    return qtrue;
}

/* --------------------------------------------------------------------------
 * Main
 * -------------------------------------------------------------------------- */

extern void Wii_MEM2_Init(void);

/* Power/reset button callback — invoked by SYS_DoPowerCB / SYS_DoResetCB.
 * libogc-3.0.4 calls __sys_power_cb without a NULL check, so passing NULL
 * crashes when any IOS IPC completion triggers the STM event handler.
 * exit(0) returns cleanly to the Homebrew Channel. */
static void wii_power_cb(void)              { exit(0); }
static void wii_reset_cb(u32 irq, void *ctx){ (void)irq; (void)ctx; exit(0); }

/* Diagnostic log functions — only active with WII_DEBUG (make WII_DEBUG=1). */
#ifdef WII_DEBUG
void crash_mark(const char *msg)
{
    FILE *f = fopen("sd:/quake3/crash.txt", "a");
    if (f) { fprintf(f, "%s\n", msg); fclose(f); }
}
#define CRASHLOG(fmt, ...) do { char _cb[256]; snprintf(_cb, sizeof(_cb), fmt, ##__VA_ARGS__); crash_mark(_cb); } while(0)
void boot_mark(const char *msg)
{
    FILE *f = fopen("sd:/quake3/boot.txt", "a");
    if (f) { fprintf(f, "%s\n", msg); fclose(f); }
}
#define WII_DBG_PRINTF(...) do { printf(__VA_ARGS__); fflush(stdout); } while(0)
#else
void crash_mark(const char *msg) { (void)msg; }
#define CRASHLOG(...) ((void)0)
void boot_mark(const char *msg) { (void)msg; }
#define WII_DBG_PRINTF(...) ((void)0)
#endif

int main(int argc, char *argv[])
{
    /* Basic Wii hardware init */
    Wii_InitConsole();
    WII_DBG_PRINTF("ioquake3-wii starting...\n");
    Wii_MEM2_Init();
    WII_DBG_PRINTF("[wii] MEM2 init done\n");

    /* SD card — must succeed before any file I/O */
    if (!Wii_MountSD()) {
        printf("FATAL: could not mount SD. Halting.\n");
        while (1) VIDEO_WaitVSync();
    }

#ifdef WII_DEBUG
    { FILE *f = fopen("sd:/quake3/boot.txt", "w"); if (f) fclose(f); }
    boot_mark("main() reached, SD mounted");
    { FILE *f = fopen("sd:/quake3/crash.txt", "w"); if (f) fclose(f); }
    CRASHLOG("main() started");
#endif

    /* Malloc thread-safety — must be set up before WPAD_Init starts
     * the Bluetooth background thread that calls malloc/free. */
    extern void Wii_InitMallocLock(void);
    Wii_InitMallocLock();

    /* Wiimote / input */
    Wii_Input_Init();
    WII_DBG_PRINTF("[wii] Input OK\n");
    boot_mark("Input init done");

    /* Network init — connect to Wi-Fi via libogc.
     * Wii_Net_Init polls net_get_status for up to ~3 s.
     * If no Wi-Fi AP is configured in Wii System Settings this will
     * time out and NET_Init will just log warnings about bind failures,
     * which is harmless for offline play. */
    {
        int net_result = Wii_Net_Init();
        (void)net_result;
        WII_DBG_PRINTF("[wii] Network %s\n", net_result == 0 ? "OK" : "failed");
        boot_mark(net_result == 0 ? "Network OK" : "Network failed");
    }

    /* Audio (ASND) */
    Wii_Snd_Init();
    WII_DBG_PRINTF("[wii] Audio OK\n");
    boot_mark("Audio init done");

    /*
     * Hand off to ioQuake3.
     * We pass a minimal argv that sets the fs_basepath to where we chdir'd.
     * Additional cvars can be added here (e.g. reduced texture quality).
     */
    /*
     * Build command line for Com_Init.
     * Format: +set key value +set key value ...
     * Each token must be space-separated; Com_Init tokenizes on spaces.
     */
    /* Wii memory budget:
     *   MEM1: ~24MB physical. After video/system/stack/code: ~18MB for heap.
     *     Used for: zone, server (svs.clients), OpenGX texture buffers, BSS.
     *   MEM2: ~52MB, claimed by Wii_MEM2_Init.
     *     Used for: hunk (mmap → wii_mem2_alloc), large calloc, overflow.
     *   Moving the hunk to MEM2 keeps MEM1 free for zone + server + textures. */
    static char cmdline[1024];
    snprintf(cmdline, sizeof(cmdline),
        "+set fs_basepath sd:/quake3 "
        "+set fs_homepath sd:/quake3 "
        "+set fs_steampath \"\" "
        "+set fs_gogpath \"\" "
        "+set com_basegame " WII_BASEGAME " "
        "+set com_hunkMegs 32 "
        "+set com_zoneMegs 8 "
        /* --- display --- */
        "+set r_mode -1 "
        "+set r_picmip 2 "          /* halve textures twice */
        /* --- rendering quality --- */
        "+set r_dynamic 0 "         /* no dynamic lights (very expensive) */
        "+set r_flares 0 "          /* no lens flares */
        "+set r_fastsky 1 "         /* solid colour sky instead of box */
        "+set r_lodbias 2 "         /* aggressively drop LOD */
        "+set r_subdivisions 20 "   /* less tessellation on curved surfaces */
        "+set r_simpleMipMaps 1 "   /* skip fancy mip generation */
        "+set r_drawSun 0 "         /* no sun glare */
        "+set r_primitives 2 "      /* GL_TRIANGLES only; no strip generation (safer on GX) */
        /* --- frame rate --- */
        "+set com_maxfps 30 "       /* cap at 30 — consistent > stuttering 60 */
        /* pmove_fixed: both server and client use identical fixed Pmove steps,
         * eliminating prediction divergence (rollback) when sv_fps steps don't
         * match the clamped client frame time. */
        "+set pmove_fixed 1 "       /* both client+server use identical 8ms steps */
        /* --- audio --- */
        "+set s_khz 22 "
        "+set com_soundMegs 4 " /* default 8 → malloc(~25MB); 4 → ~12MB */
        /* --- server / misc --- */
        "+set sv_pure 0 "
        "+set sv_maxclients 8 "
        "+set in_joystick 1 "
        "+set in_joystickUseAnalog 1 "
        /* Dual-stick layout: left stick = move, C-stick = look.
         * Axis indices match SE_JOYSTICK_AXIS events from wii_input.c. */
        "+set j_side_axis 0 "       /* left stick X -> strafe */
        "+set j_forward_axis 1 "    /* left stick Y -> forward/back */
        "+set j_pitch_axis 3 "      /* C-stick Y -> look up/down */
        "+set j_yaw_axis 4 "        /* C-stick X -> look left/right */
        /* Sensitivity tuned for GC stick range (lower than desktop defaults) */
        "+set j_pitch 0.015 "
        "+set j_yaw -0.015 "
        "+set j_forward -0.25 "
        "+set j_side 0.25 "
#if WII_STANDALONE
        "+set com_standalone 1 "
#else
        "+set com_standalone 0 "
#endif
        /* Network: IPv4 only — Wii has no IPv6 stack */
        "+set net_enabled 1 "
        /* Use a non-server port so NAT hairpinning doesn't get confused
         * by source-port == dest-port (27960) when looping back. */
        "+set net_port 27961 "
        /* Prevent match end → map_restart → RE_LoadWorldMap crash */
        "+set fraglimit 0 "
        "+set timelimit 0 "
        /* Force-flush qconsole.log after every write — ensures crash tail isn't lost */
        "+set com_logfile 2 "
        /* Default player name — overridden once the user sets their own in the menu */
        "+set name Quake3Wii "
        /* Hide any on-screen debug/perf text */
        "+set cg_drawFPS 0 "
        "+set cg_drawTimer 0 "
        "+set cg_drawSnapshot 0 "
        "+set com_speeds 0 "
        "+set r_speeds 0"
    );

    /* Power/Reset buttons always return to HBC */
    SYS_SetPowerCallback(wii_power_cb);
    SYS_SetResetCallback(wii_reset_cb);

#if !WII_STANDALONE
    /* Create qkey file (2048 bytes) if missing — bypasses CD key dialog.
     * Only needed for Quake III Arena; standalone games (OA) skip this. */
    {
        FILE *kf = fopen("sd:/quake3/qkey", "rb");
        if (kf) {
            fclose(kf);
        } else {
            kf = fopen("sd:/quake3/qkey", "wb");
            if (kf) {
                unsigned char buf[2048];
                for (int i = 0; i < 2048; i++) buf[i] = (unsigned char)(i & 0xFF);
                fwrite(buf, 1, 2048, kf);
                fclose(kf);
                WII_DBG_PRINTF("[wii] Created qkey file\n");
            }
        }
    }
#endif

    boot_mark("Calling GX init");
    WII_DBG_PRINTF("[wii] Calling Wii_GX_Init...\n");

    /* GX must be up before Com_Init, because Com_Init → CL_Init →
     * BeginRegistration → BeginFrame start firing immediately.
     * Without GX initialised those frames render to nothing → black screen. */
    Wii_GX_Init();
    WII_DBG_PRINTF("[wii] GX init done\n");
    boot_mark("GX init done");

    /* Pre-initialize the renderer export table before Com_Init so that
     * Com_Printf -> SCR_UpdateScreen -> re.BeginFrame doesn't crash.
     * re is the global refexport_t in cl_main.c. */
    extern refexport_t re;
    refexport_t *ref = GetRefAPI(REF_API_VERSION, NULL);
    if (ref) re = *ref;
    boot_mark("GetRefAPI done");

    WII_DBG_PRINTF("[wii] Calling Com_Init...\n");
    boot_mark("Calling Com_Init");
    Com_Init(cmdline);
    WII_DBG_PRINTF("[wii] Com_Init done\n");
    boot_mark("Com_Init done");
    NET_Init();



    /* Main loop */
    int frame_count = 0;
    while (1) {
        Wii_Input_Frame();

        if (Wii_Input_HomePressed()) {
            Com_Quit_f();
            break;
        }

        Com_Frame();
    }

    /* Cleanup (usually unreachable – Q3 calls exit() itself) */
    Wii_Snd_Shutdown();
    NET_Shutdown();
    return 0;
}
