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

/* Atomic crash log — open/write/close per entry so libfat commits each write
 * to SD immediately.  fflush() alone does not flush libfat's sector buffer. */
void crash_mark(const char *msg)
{
    FILE *f = fopen("sd:/quake3/crash.txt", "a");
    if (f) { fprintf(f, "%s\n", msg); fclose(f); }
}
#define CRASHLOG(fmt, ...) do { char _cb[256]; snprintf(_cb, sizeof(_cb), fmt, ##__VA_ARGS__); crash_mark(_cb); } while(0)

/* Write a single-line progress marker to sd:/quake3/boot.txt.
 * Only call after fatInitDefault() has succeeded.
 * Exported so wii_snd.c / wii_glimp.c can use it. */
void boot_mark(const char *msg)
{
    FILE *f = fopen("sd:/quake3/boot.txt", "a");
    if (f) { fprintf(f, "%s\n", msg); fclose(f); }
}

int main(int argc, char *argv[])
{
    /* Basic Wii hardware init */
    Wii_InitConsole();
    printf("ioquake3-wii starting...\n"); fflush(stdout);
    Wii_MEM2_Init();
    printf("[wii] MEM2 init done\n"); fflush(stdout);

    /* SD card — must succeed before any file I/O */
    if (!Wii_MountSD()) {
        printf("FATAL: could not mount SD. Halting.\n");
        while (1) VIDEO_WaitVSync();
    }

    /* SD is now accessible; write boot marker and open crash log */
    /* Truncate boot.txt so stale entries from a previous run are cleared */
    { FILE *f = fopen("sd:/quake3/boot.txt", "w"); if (f) fclose(f); }
    boot_mark("main() reached, SD mounted");

    /* Truncate crash.txt from previous run */
    { FILE *f = fopen("sd:/quake3/crash.txt", "w"); if (f) fclose(f); }
    CRASHLOG("main() started");

    /* Wiimote / input */
    Wii_Input_Init();
    printf("[wii] Input OK\n"); fflush(stdout);
    boot_mark("Input init done");

    /* Network init — connect to Wi-Fi via libogc.
     * Wii_Net_Init polls net_get_status for up to ~3 s.
     * If no Wi-Fi AP is configured in Wii System Settings this will
     * time out and NET_Init will just log warnings about bind failures,
     * which is harmless for offline play. */
    {
        int net_result = Wii_Net_Init();
        if (net_result == 0) {
            printf("[wii] Network OK — IP: %s\n", wii_net_local_ip);
            char ipbuf[64];
            snprintf(ipbuf, sizeof(ipbuf), "Network OK IP=%s", wii_net_local_ip);
            boot_mark(ipbuf);
        } else {
            printf("[wii] Network init failed (err=%d) — check Wii Wi-Fi settings\n", net_result);
            char errbuf[64];
            snprintf(errbuf, sizeof(errbuf), "Network failed err=%d", net_result);
            boot_mark(errbuf);
        }
    }

    /* Audio (ASND) */
    Wii_Snd_Init();
    printf("[wii] Audio OK\n"); fflush(stdout);
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
        "+set fs_game baseq3 "
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
        "+set j_pitch_axis 1 "
        "+set j_yaw_axis 0 "
        "+set com_standalone 0 "
        /* Network: IPv4 only — Wii has no IPv6 stack */
        "+set net_enabled 1 "
        /* Use a non-server port so NAT hairpinning doesn't get confused
         * by source-port == dest-port (27960) when looping back. */
        "+set net_port 27961 "
        /* Prevent match end → map_restart → RE_LoadWorldMap crash */
        "+set fraglimit 0 "
        "+set timelimit 0 "
        /* Force-flush qconsole.log after every write — ensures crash tail isn't lost */
        "+set com_logfile 2"
    );

    /* Power/Reset buttons always return to HBC */
    SYS_SetPowerCallback(wii_power_cb);
    SYS_SetResetCallback(wii_reset_cb);

    /* Create qkey file (2048 bytes) if missing — bypasses CD key dialog */
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
                printf("[wii] Created qkey file\n"); fflush(stdout);
            }
        }
    }

    boot_mark("Calling GX init");
    printf("[wii] Calling Wii_GX_Init...\n"); fflush(stdout);
    extern u32 SYS_GetArena1Size(void);
    printf("[wii] Free arena1: %u KB\n", (unsigned)(SYS_GetArena1Size() / 1024)); fflush(stdout);

    /* GX must be up before Com_Init, because Com_Init → CL_Init →
     * BeginRegistration → BeginFrame start firing immediately.
     * Without GX initialised those frames render to nothing → black screen. */
    Wii_GX_Init();
    printf("[wii] GX init done\n"); fflush(stdout);
    boot_mark("GX init done");

    /* Pre-initialize the renderer export table before Com_Init so that
     * Com_Printf -> SCR_UpdateScreen -> re.BeginFrame doesn't crash.
     * re is the global refexport_t in cl_main.c. */
    extern refexport_t re;
    refexport_t *ref = GetRefAPI(REF_API_VERSION, NULL);
    if (ref) re = *ref;
    boot_mark("GetRefAPI done");

    printf("[wii] Calling Com_Init...\n"); fflush(stdout);
    boot_mark("Calling Com_Init");
    Com_Init(cmdline);
    printf("[wii] Com_Init done\n"); fflush(stdout);
    boot_mark("Com_Init done");
    NET_Init();
    printf("[wii] NET_Init done\n"); fflush(stdout);

    /* Power button = emergency exit to HBC */
    SYS_SetPowerCallback(wii_power_cb); /* default power callback exits cleanly */
    SYS_SetResetCallback(wii_reset_cb);

    /* Main loop */
    int frame_count = 0;
    while (1) {
        Wii_Input_Frame();

        if (Wii_Input_HomePressed()) {
            Com_Quit_f();
            break;
        }

        /* Check START on GC pad to return to HBC */
        PAD_ScanPads();
        if (PAD_ButtonsDown(0) & PAD_BUTTON_START) {
            exit(0);
        }

        frame_count++;
        if (frame_count == 1) {
            printf("[wii] Entering Com_Frame #1\n"); fflush(stdout);
        }
        /* Write a marker every 100 frames so crash.txt shows which Com_Frame
         * the crash occurred in (crash.txt uses atomic writes that survive DSI). */
        if (frame_count % 100 == 0) {
            char _fm[64]; snprintf(_fm, sizeof(_fm), "pre-frame %d", frame_count);
            crash_mark(_fm);
        }

        Com_Frame();

        if (frame_count % 100 == 0) {
            char _fm[64]; snprintf(_fm, sizeof(_fm), "post-frame %d", frame_count);
            crash_mark(_fm);
        }
        if (frame_count == 1) {
            printf("[wii] Com_Frame #1 done\n"); fflush(stdout);
        }
    }

    /* Cleanup (usually unreachable – Q3 calls exit() itself) */
    Wii_Snd_Shutdown();
    NET_Shutdown();
    return 0;
}
