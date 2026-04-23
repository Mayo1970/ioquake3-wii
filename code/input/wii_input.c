/*
 * ioquake3-wii: input/wii_input.c
 *
 * Dual-stick gamepad input adapted from quake360's XInput scheme.
 *
 * When WPAD_ENABLED is 0, uses GameCube controller only.
 * When WPAD_ENABLED is 1, uses Wiimote + Nunchuk with GC fallback.
 *
 * In-game: both sticks emit SE_JOYSTICK_AXIS events, processed by Q3's
 * CL_JoystickMove() with j_pitch/j_yaw/j_forward/j_side cvars.
 * In menus: left stick emits SE_MOUSE for cursor movement.
 * Buttons emit SE_KEY with K_JOY* keycodes, bound via Key_SetBinding().
 *
 * GC Controller layout (in-game):
 *   Left stick         = move (forward/back/strafe)
 *   C-stick            = look (yaw/pitch)
 *   R trigger          = attack (+attack)
 *   L trigger          = walk (+speed)
 *   A                  = jump (+moveup)
 *   B                  = crouch (+movedown)
 *   X                  = prev weapon
 *   Y                  = next weapon
 *   Z                  = zoom (+zoom)
 *   D-pad up           = scoreboard (+scores)
 *   D-pad down         = attack (+attack, alt)
 *   D-pad left/right   = prev/next weapon
 *   Start              = menu (K_ESCAPE)
 *
 * GC Controller layout (menus):
 *   Left stick / C-stick = cursor (SE_MOUSE)
 *   A                    = confirm (K_ENTER)
 *   B                    = back (K_ESCAPE)
 *   D-pad                = arrow keys
 *   Start                = K_ESCAPE
 *   Y                    = console toggle
 */

/* ---- DIAGNOSTIC SWITCH -------------------------------------------------- */
#define WPAD_ENABLED  0

/* ---- Includes ----------------------------------------------------------- */
#include <gccore.h>
#include <ogc/pad.h>
#if WPAD_ENABLED
#include <wiiuse/wpad.h>
#endif
#include <math.h>
#include <string.h>
#include <stdio.h>

#include "wii_input.h"

#include "qcommon/q_shared.h"
#include "qcommon/qcommon.h"
#include "keycodes.h"

extern int  Key_GetCatcher(void);
extern void Key_SetBinding(int keynum, const char *binding);

/* -------------------------------------------------------------------------- */
#define STICK_DEADZONE       20      /* raw -128..127 deadzone for GC sticks */
#define MENU_SENSITIVITY_F   2.0f    /* cursor pixels/frame at full deflection */
#define TRIGGER_THRESHOLD    100     /* 0-255 analog trigger -> digital */

/* Axis indices matching ioq3 defaults (cl_main.c):
 *   j_side_axis=0  j_forward_axis=1  j_yaw_axis=4  j_pitch_axis=3
 * The quake360 port uses the same assignment. */
#define AXIS_SIDE     0   /* left stick X  -> strafe */
#define AXIS_FORWARD  1   /* left stick Y  -> forward/back */
#define AXIS_PITCH    3   /* C-stick Y     -> pitch (look up/down) */
#define AXIS_YAW      4   /* C-stick X     -> yaw (look left/right) */

/* GC stick raw range is -128..127; SE_JOYSTICK_AXIS expects -32767..32767.
 * Scale factor: 32767/127 ~ 258. */
#define GC_AXIS_SCALE  258

/*
 * GC button -> K_JOY* mapping.
 * PAD_ButtonsHeld returns a bitmask; we map each bit to a K_JOY keycode.
 * Menu-mode buttons (A=K_ENTER, B=K_ESCAPE, D-pad=arrows, Start=K_ESCAPE)
 * are hardcoded in the menu path, not via Key_SetBinding.
 */
typedef struct { u32 bit; int q3key; } gc_btn_t;

static const gc_btn_t s_gc_buttons[] = {
    { PAD_BUTTON_A,      K_JOY1  },
    { PAD_BUTTON_B,      K_JOY2  },
    { PAD_BUTTON_X,      K_JOY3  },
    { PAD_BUTTON_Y,      K_JOY4  },
    { PAD_TRIGGER_Z,     K_JOY5  },
    { PAD_BUTTON_START,  K_JOY6  },
    { PAD_BUTTON_UP,     K_JOY7  },
    { PAD_BUTTON_DOWN,   K_JOY8  },
    { PAD_BUTTON_LEFT,   K_JOY9  },
    { PAD_BUTTON_RIGHT,  K_JOY10 },
};
#define GC_BTN_COUNT (sizeof(s_gc_buttons) / sizeof(s_gc_buttons[0]))

/* L/R analog triggers mapped to K_JOY keycodes */
#define K_JOY_LTRIG  K_JOY11
#define K_JOY_RTRIG  K_JOY12

/* Menu-mode button table: hardcoded keycodes (not rebindable) */
typedef struct { u32 bit; int q3key; } menu_btn_t;

static const menu_btn_t s_menu_buttons[] = {
    { PAD_BUTTON_A,      K_ENTER      },
    { PAD_BUTTON_B,      K_ESCAPE     },
    { PAD_BUTTON_X,      K_MOUSE1     },
    { PAD_BUTTON_Y,      K_CONSOLE    },
    { PAD_BUTTON_START,  K_ESCAPE     },
    { PAD_BUTTON_UP,     K_UPARROW    },
    { PAD_BUTTON_DOWN,   K_DOWNARROW  },
    { PAD_BUTTON_LEFT,   K_LEFTARROW  },
    { PAD_BUTTON_RIGHT,  K_RIGHTARROW },
};
#define MENU_BTN_COUNT (sizeof(s_menu_buttons) / sizeof(s_menu_buttons[0]))

/* -------------------------------------------------------------------------- */
typedef struct {
    qboolean key_held[256];
} input_state_t;

static input_state_t  s_input;
static qboolean       s_home_pressed  = qfalse;
static qboolean       s_in_game       = qfalse;
static float          s_accum_x       = 0.0f;
static float          s_accum_y       = 0.0f;
static float          s_accum_cx      = 0.0f;
static float          s_accum_cy      = 0.0f;
static short          s_old_axis[4];         /* LX, LY, CX, CY */
static qboolean       s_old_ltrig     = qfalse;
static qboolean       s_old_rtrig     = qfalse;
static qboolean       s_bindings_set  = qfalse;

/* -------------------------------------------------------------------------- */
static void InjectKey(int q3key, qboolean down)
{
    if (q3key < 0 || q3key >= 256)
        return;
    if (s_input.key_held[q3key] == down)
        return;
    s_input.key_held[q3key] = down;
    Com_QueueEvent(0, SE_KEY, q3key, down, 0, NULL);
}

static void ReleaseAllKeys(void)
{
    int i;
    for (i = 0; i < 256; i++) {
        if (s_input.key_held[i]) {
            s_input.key_held[i] = qfalse;
            Com_QueueEvent(0, SE_KEY, i, qfalse, 0, NULL);
        }
    }
    s_accum_x  = s_accum_y  = 0.0f;
    s_accum_cx = s_accum_cy = 0.0f;
    /* Reset joystick axes to centre */
    s_old_axis[0] = s_old_axis[1] = s_old_axis[2] = s_old_axis[3] = 0;
}

/* --------------------------------------------------------------------------
 * Stick helpers
 * -------------------------------------------------------------------------- */

/* Apply square deadzone and scale GC raw (-128..127) to Q3 range (-32767..32767) */
static short GC_FilterAxis(s8 raw, int deadzone)
{
    int v = (int)raw;
    if (v > -deadzone && v < deadzone)
        return 0;
    /* Clamp to +-127 then scale */
    if (v > 127) v = 127;
    if (v < -127) v = -127;
    return (short)(v * GC_AXIS_SCALE);
}

/* GC stick -> SE_MOUSE with float accumulator (for menu cursor) */
static void InjectCursorStick(s8 x, s8 y, float sensitivity,
                               float *ax, float *ay)
{
    int ix = (int)x;
    int iy = (int)y;
    if (ix > -STICK_DEADZONE && ix < STICK_DEADZONE) ix = 0;
    if (iy > -STICK_DEADZONE && iy < STICK_DEADZONE) iy = 0;
    if (ix == 0 && iy == 0) { *ax = *ay = 0.0f; return; }

    *ax += ((float)ix / 127.0f) * sensitivity;
    *ay += ((float)iy / 127.0f) * sensitivity;

    int out_x = (int)*ax;
    int out_y = (int)*ay;
    *ax -= (float)out_x;
    *ay -= (float)out_y;

    if (out_x != 0 || out_y != 0)
        Com_QueueEvent(0, SE_MOUSE, out_x, -out_y, 0, NULL);
}

/* --------------------------------------------------------------------------
 * Default bindings (quake360 style)
 * -------------------------------------------------------------------------- */
static void SetDefaultBindings(void)
{
    if (s_bindings_set)
        return;
    s_bindings_set = qtrue;

    /* Face buttons */
    Key_SetBinding(K_JOY1,      "+moveup");     /* A = jump */
    Key_SetBinding(K_JOY2,      "+movedown");   /* B = crouch */
    Key_SetBinding(K_JOY3,      "weapprev");    /* X = prev weapon */
    Key_SetBinding(K_JOY4,      "weapnext");    /* Y = next weapon */

    /* Shoulder */
    Key_SetBinding(K_JOY5,      "+zoom");       /* Z = zoom */
    Key_SetBinding(K_JOY6,      "");            /* Start = handled as K_ESCAPE in menu; no game bind needed */

    /* D-pad */
    Key_SetBinding(K_JOY7,      "+scores");     /* D-pad up = scoreboard */
    Key_SetBinding(K_JOY8,      "+attack");     /* D-pad down = attack (alt) */
    Key_SetBinding(K_JOY9,      "weapprev");    /* D-pad left = prev weapon */
    Key_SetBinding(K_JOY10,     "weapnext");    /* D-pad right = next weapon */

    /* Analog triggers */
    Key_SetBinding(K_JOY_LTRIG, "+speed");      /* L trigger = walk */
    Key_SetBinding(K_JOY_RTRIG, "+attack");     /* R trigger = fire */
}

/* --------------------------------------------------------------------------
 * GC Controller frame
 * -------------------------------------------------------------------------- */
static void GC_Input_Frame(void)
{
    int i;

    PAD_ScanPads();

    u32 held  = PAD_ButtonsHeld(PAD_CHAN0);
    s8  lx    = PAD_StickX(PAD_CHAN0);
    s8  ly    = PAD_StickY(PAD_CHAN0);
    s8  cx    = PAD_SubStickX(PAD_CHAN0);
    s8  cy    = PAD_SubStickY(PAD_CHAN0);
    u8  l_ana = PAD_TriggerL(PAD_CHAN0);
    u8  r_ana = PAD_TriggerR(PAD_CHAN0);

    qboolean in_game = (Key_GetCatcher() == 0) ? qtrue : qfalse;

    if (in_game != s_in_game) {
        ReleaseAllKeys();
        s_in_game = in_game;
    }

    if (!in_game) {
        /* ---- MENU MODE ----
         * Buttons: hardcoded keycodes for menu navigation
         * Sticks:  SE_MOUSE for cursor movement */
        for (i = 0; i < (int)MENU_BTN_COUNT; i++)
            InjectKey(s_menu_buttons[i].q3key,
                      (held & s_menu_buttons[i].bit) ? qtrue : qfalse);

        /* R trigger = click in menus */
        InjectKey(K_MOUSE1, r_ana > TRIGGER_THRESHOLD ? qtrue : qfalse);

        InjectCursorStick(lx, ly, MENU_SENSITIVITY_F,
                          &s_accum_x, &s_accum_y);
        InjectCursorStick(cx, cy, MENU_SENSITIVITY_F,
                          &s_accum_cx, &s_accum_cy);
    } else {
        /* ---- GAME MODE ----
         * Buttons: K_JOY* keycodes, actions set by Key_SetBinding
         * Sticks:  SE_JOYSTICK_AXIS for analog movement and look
         * Triggers: binary threshold -> K_JOY keycodes */

        SetDefaultBindings();

        /* Digital buttons */
        for (i = 0; i < (int)GC_BTN_COUNT; i++)
            InjectKey(s_gc_buttons[i].q3key,
                      (held & s_gc_buttons[i].bit) ? qtrue : qfalse);

        /* Analog triggers -> binary press/release */
        qboolean l_pressed = l_ana > TRIGGER_THRESHOLD ? qtrue : qfalse;
        qboolean r_pressed = r_ana > TRIGGER_THRESHOLD ? qtrue : qfalse;

        if (l_pressed != s_old_ltrig) {
            s_old_ltrig = l_pressed;
            Com_QueueEvent(0, SE_KEY, K_JOY_LTRIG, l_pressed, 0, NULL);
        }
        if (r_pressed != s_old_rtrig) {
            s_old_rtrig = r_pressed;
            Com_QueueEvent(0, SE_KEY, K_JOY_RTRIG, r_pressed, 0, NULL);
        }

        /* Left stick -> movement axes */
        short ax_lx = GC_FilterAxis(lx, STICK_DEADZONE);
        short ax_ly = GC_FilterAxis(ly, STICK_DEADZONE);
        /* C-stick -> look axes */
        short ax_cx = GC_FilterAxis(cx, STICK_DEADZONE);
        short ax_cy = GC_FilterAxis(cy, STICK_DEADZONE);

        /* Only send axis events when the value changes (like quake360) */
        if (ax_lx != s_old_axis[0]) {
            Com_QueueEvent(0, SE_JOYSTICK_AXIS, AXIS_SIDE, ax_lx, 0, NULL);
            s_old_axis[0] = ax_lx;
        }
        /* Y axis negated: GC stick-up is positive, Q3 forward is negative */
        short neg_ly = -ax_ly;
        if (neg_ly != s_old_axis[1]) {
            Com_QueueEvent(0, SE_JOYSTICK_AXIS, AXIS_FORWARD, neg_ly, 0, NULL);
            s_old_axis[1] = neg_ly;
        }
        if (ax_cx != s_old_axis[2]) {
            Com_QueueEvent(0, SE_JOYSTICK_AXIS, AXIS_YAW, ax_cx, 0, NULL);
            s_old_axis[2] = ax_cx;
        }
        short neg_cy = -ax_cy;
        if (neg_cy != s_old_axis[3]) {
            Com_QueueEvent(0, SE_JOYSTICK_AXIS, AXIS_PITCH, neg_cy, 0, NULL);
            s_old_axis[3] = neg_cy;
        }
    }
}

/* --------------------------------------------------------------------------
 * Init
 * -------------------------------------------------------------------------- */
void Wii_Input_Init(void)
{
    memset(&s_input, 0, sizeof(s_input));
    s_home_pressed = qfalse;
    s_in_game      = qfalse;
    s_bindings_set = qfalse;
    s_accum_x = s_accum_y = 0.0f;
    s_accum_cx = s_accum_cy = 0.0f;
    s_old_ltrig = s_old_rtrig = qfalse;
    memset(s_old_axis, 0, sizeof(s_old_axis));

    PAD_Init();

#if WPAD_ENABLED
    WPAD_Init();
    WPAD_SetDataFormat(WPAD_CHAN_0, WPAD_FMT_BTNS_ACC_IR);
    WPAD_SetVRes(WPAD_CHAN_0, 640, 480);
    printf("[input] WPAD + GC PAD initialised\n");
#else
    printf("[input] GC PAD only\n");
#endif
}

/* --------------------------------------------------------------------------
 * Frame
 * -------------------------------------------------------------------------- */
void Wii_Input_Frame(void)
{
#if WPAD_ENABLED
    /* TODO: Wiimote + Nunchuk path — for now fall through to GC */
    WPAD_ScanPads();
    s_home_pressed = qfalse;

    WPADData *data = WPAD_Data(WPAD_CHAN_0);
    if (!data) {
        GC_Input_Frame();
        return;
    }

    u32 held = WPAD_ButtonsHeld(WPAD_CHAN_0);
    if (held & WPAD_BUTTON_HOME)
        s_home_pressed = qtrue;

    /* Fall back to GC for now; Wiimote dual-stick requires Nunchuk+Classic */
    GC_Input_Frame();
#else
    s_home_pressed = qfalse;
    GC_Input_Frame();
#endif
}

qboolean Wii_Input_HomePressed(void)
{
    return s_home_pressed;
}

void Wii_Input_GenerateEvents(void)
{
    /* Events were injected in Wii_Input_Frame() */
}
