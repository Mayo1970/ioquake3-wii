/*
 * ioquake3-wii: input/wii_input.c
 *
 * Wiimote (held sideways) + Nunchuk input layer.
 *
 * Control mapping (Wiimote sideways + Nunchuk):
 *   Nunchuk stick        → SE_MOUSE (camera aim + menu cursor)
 *   Wiimote D-pad up/dn  → Look up / look down   (pitch)
 *   Wiimote D-pad lt/rt  → Turn left / turn right (yaw)
 *   Nunchuk C            → Jump
 *   Nunchuk Z            → Crouch / walk
 *   Wiimote B (trigger)  → Attack (fire)
 *   Wiimote A            → Use / activate
 *   Wiimote +            → Next weapon
 *   Wiimote -            → Previous weapon
 *   Wiimote 1            → Score / console
 *   Wiimote 2            → Map / objectives
 *   Wiimote HOME         → Quit (handled in main loop)
 *
 * IR (pointing) is not used in v1 – pure gamepad style.
 * Add IR-based mouselook as a second phase.
 */

#include <gccore.h>
#include <wiiuse/wpad.h>
#include <string.h>
#include <stdio.h>

#include "wii_input.h"

/* ioQ3 event injection */
#include "qcommon/q_shared.h"
#include "qcommon/qcommon.h"   /* Com_QueueEvent */
/* Key codes from ioQ3 — found under code/client/ */
#include "keycodes.h"

/* --------------------------------------------------------------------------
 * Internal state
 * -------------------------------------------------------------------------- */
#define MAX_WIIMOTES  1          /* We only need player 1 */
#define STICK_DEADZONE       20    /* dead zone applied to raw -127..127 stick range */
#define STICK_SENSITIVITY_F  1.0f  /* pixels/frame at full stick deflection */

typedef struct {
    /* Which Q3 keys are currently held (so we emit releases properly) */
    qboolean key_held[MAX_KEY];
} wii_input_state_t;

static wii_input_state_t s_input;
static qboolean          s_home_pressed = qfalse;
static float             s_accum_x      = 0.0f;
static float             s_accum_y      = 0.0f;

/* --------------------------------------------------------------------------
 * Helper: inject a key down/up event only on state change
 * -------------------------------------------------------------------------- */
static void InjectKey(int q3key, qboolean down)
{
    if (s_input.key_held[q3key] == down)
        return;   /* no change */
    s_input.key_held[q3key] = down;
    Com_QueueEvent(0, SE_KEY, q3key, down, 0, NULL);
}

/* --------------------------------------------------------------------------
 * Helper: convert Nunchuk analogue stick → SE_MOUSE (camera yaw/pitch + menu cursor).
 * The Nunchuk stick axes are -128..127.
 * We scale to a -127..127 range and emit SE_MOUSE for X (yaw) and Y (pitch).
 * -------------------------------------------------------------------------- */
static void InjectStick(s8 x, s8 y)
{
    int ix = (int)x;
    int iy = (int)y;

    /* 1. Dead zone */
    if (ix > -STICK_DEADZONE && ix < STICK_DEADZONE) ix = 0;
    if (iy > -STICK_DEADZONE && iy < STICK_DEADZONE) iy = 0;

    /* 2. Reset accumulator on axis when stick returns to centre */
    if (ix == 0) s_accum_x = 0.0f;
    else s_accum_x += (float)ix * STICK_SENSITIVITY_F / 127.0f;

    if (iy == 0) s_accum_y = 0.0f;
    else s_accum_y += (float)iy * STICK_SENSITIVITY_F / 127.0f;

    /* 3. Emit only whole pixels; carry the remainder into the next frame */
    int emit_x = (int)s_accum_x;
    int emit_y = (int)s_accum_y;
    s_accum_x -= (float)emit_x;
    s_accum_y -= (float)emit_y;

    if (emit_x != 0 || emit_y != 0)
        Com_QueueEvent(0, SE_MOUSE, emit_x, -emit_y, 0, NULL);
}

/* --------------------------------------------------------------------------
 * Map WPAD button bitmask → Q3 key, emit events on changes.
 * We iterate a lookup table for clarity.
 * -------------------------------------------------------------------------- */
typedef struct { u32 wpad_btn; int q3key; } btn_map_t;

static const btn_map_t s_btn_map[] = {
    { WPAD_BUTTON_B,       K_MOUSE1       },  /* fire */
    { WPAD_BUTTON_A,       K_ENTER        },  /* use */
    { WPAD_BUTTON_UP,      K_UPARROW      },  /* look up  */
    { WPAD_BUTTON_DOWN,    K_DOWNARROW    },  /* look dn  */
    { WPAD_BUTTON_LEFT,    K_LEFTARROW    },  /* turn lt  */
    { WPAD_BUTTON_RIGHT,   K_RIGHTARROW   },  /* turn rt  */
    { WPAD_BUTTON_PLUS,    ']'            },  /* next weapon (]) */
    { WPAD_BUTTON_MINUS,   '['            },  /* prev weapon ([) */
    { WPAD_BUTTON_1,       K_CONSOLE        },  /* console toggle (~) */
    { WPAD_BUTTON_2,       K_ESCAPE       },  /* menu / escape */
    /* Nunchuk buttons */
    { WPAD_NUNCHUK_BUTTON_C, K_SPACE      },  /* jump */
    { WPAD_NUNCHUK_BUTTON_Z, K_SHIFT      },  /* crouch/walk */
};
#define BTN_MAP_SIZE  (sizeof(s_btn_map) / sizeof(s_btn_map[0]))

/* --------------------------------------------------------------------------
 * Wii_Input_Init
 * -------------------------------------------------------------------------- */
void Wii_Input_Init(void)
{
    memset(&s_input, 0, sizeof(s_input));
    s_home_pressed = qfalse;

    WPAD_Init();
    WPAD_SetDataFormat(WPAD_CHAN_0, WPAD_FMT_BTNS_ACC_IR);
    /* Attach Nunchuk extension listener */
    WPAD_SetVRes(WPAD_CHAN_0, 640, 480);  /* IR resolution (for future IR mode) */

    printf("[input] WPAD initialised (chan 0 = Wiimote+Nunchuk)\n");
}

/* --------------------------------------------------------------------------
 * Wii_Input_Frame
 * Called once per game loop iteration, before Com_Frame().
 * -------------------------------------------------------------------------- */
void Wii_Input_Frame(void)
{
    WPAD_ScanPads();
    s_home_pressed = qfalse;

    WPADData *data = WPAD_Data(WPAD_CHAN_0);
    if (!data)
        return;

    u32 held = WPAD_ButtonsHeld(WPAD_CHAN_0);

    /* HOME button is special – handled in main loop to quit cleanly */
    if (held & WPAD_BUTTON_HOME)
        s_home_pressed = qtrue;

    /* Inject digital button events */
    for (int i = 0; i < (int)BTN_MAP_SIZE; i++)
        InjectKey(s_btn_map[i].q3key, (held & s_btn_map[i].wpad_btn) ? qtrue : qfalse);

    /* Inject Nunchuk analogue stick */
    if (data->exp.type == WPAD_EXP_NUNCHUK) {
        joystick_t *js = &data->exp.nunchuk.js;
        /*
         * js->pos.x and js->pos.y are floats in roughly -1..1 range from
         * wiiuse. Multiply by 127 to get integer deltas for Q3.
         */
        s8 sx = (s8)(js->pos.x * 127.0f);   /* capture full raw -127..127 range */
        s8 sy = (s8)(js->pos.y * 127.0f);
        InjectStick(sx, sy);
    }
}

/* --------------------------------------------------------------------------
 * Wii_Input_HomePressed
 * -------------------------------------------------------------------------- */
qboolean Wii_Input_HomePressed(void)
{
    return s_home_pressed;
}

/* --------------------------------------------------------------------------
 * Wii_Input_GenerateEvents
 * Called by ioQ3's Sys_GetEvent() override (see wii_sys.c).
 * In our architecture Wii_Input_Frame() already queued all events via
 * Com_QueueEvent(), so this is effectively a no-op, but the symbol must exist.
 * -------------------------------------------------------------------------- */
void Wii_Input_GenerateEvents(void)
{
    /* Events were injected in Wii_Input_Frame() */
}
