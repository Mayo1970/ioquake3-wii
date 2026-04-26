/* wii_input.c -- Unified controller input (GC pad + optional Wiimote+Nunchuk). */

#ifndef WPAD_ENABLED
#define WPAD_ENABLED  0
#endif

#include <gccore.h>
#include <ogc/pad.h>
#if WPAD_ENABLED
#include <wiiuse/wpad.h>
#endif
#include <string.h>
#include <stdio.h>

#include "wii_input.h"
#include "qcommon/q_shared.h"
#include "qcommon/qcommon.h"
#include "keycodes.h"

extern int  Key_GetCatcher(void);
extern void Key_SetBinding(int keynum, const char *binding);

#define STICK_DEADZONE       20
#define MENU_SENSITIVITY_F   2.0f
#define TRIGGER_THRESHOLD    100

/* Axis indices matching j_*_axis cvars in wii_main.c */
#define AXIS_SIDE     0
#define AXIS_FORWARD  1
#define AXIS_PITCH    3
#define AXIS_YAW      4

/* GC stick +-127 -> Q3 joystick +-32767 */
#define GC_AXIS_SCALE  258

typedef struct { u32 bit; int q3key; } btn_map_t;

/* ---------- GC controller tables ---------- */

static const btn_map_t s_gc_buttons[] = {
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

#define K_JOY_LTRIG  K_JOY11
#define K_JOY_RTRIG  K_JOY12

static const btn_map_t s_gc_menu_buttons[] = {
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
#define GC_MENU_BTN_COUNT (sizeof(s_gc_menu_buttons) / sizeof(s_gc_menu_buttons[0]))

/* ---------- Wiimote+Nunchuk tables ---------- */

#if WPAD_ENABLED

/* Wiimote in-game buttons */
static const btn_map_t s_wm_buttons[] = {
    { WPAD_BUTTON_B,              K_JOY1  },   /* trigger = fire */
    { WPAD_BUTTON_A,              K_JOY2  },   /* A = jump */
    { WPAD_NUNCHUK_BUTTON_Z,      K_JOY3  },   /* Z = zoom */
    { WPAD_NUNCHUK_BUTTON_C,      K_JOY4  },   /* C = crouch */
    { WPAD_BUTTON_PLUS,           K_JOY5  },   /* + = menu */
    { WPAD_BUTTON_MINUS,          K_JOY6  },   /* - = scores */
    { WPAD_BUTTON_UP,             K_JOY7  },   /* D-up = weapnext */
    { WPAD_BUTTON_DOWN,           K_JOY8  },   /* D-down = weapprev */
    { WPAD_BUTTON_LEFT,           K_JOY9  },   /* D-left = weapprev (alt) */
    { WPAD_BUTTON_RIGHT,          K_JOY10 },   /* D-right = weapnext (alt) */
    { WPAD_BUTTON_1,              K_JOY11 },   /* 1 = walk */
};
#define WM_BTN_COUNT (sizeof(s_wm_buttons) / sizeof(s_wm_buttons[0]))

static const btn_map_t s_wm_menu_buttons[] = {
    { WPAD_BUTTON_A,              K_ENTER      },
    { WPAD_BUTTON_B,              K_ESCAPE     },
    { WPAD_BUTTON_PLUS,           K_ESCAPE     },
    { WPAD_BUTTON_1,              K_MOUSE1     },
    { WPAD_BUTTON_UP,             K_UPARROW    },
    { WPAD_BUTTON_DOWN,           K_DOWNARROW  },
    { WPAD_BUTTON_LEFT,           K_LEFTARROW  },
    { WPAD_BUTTON_RIGHT,          K_RIGHTARROW },
};
#define WM_MENU_BTN_COUNT (sizeof(s_wm_menu_buttons) / sizeof(s_wm_menu_buttons[0]))

/* IR aiming constants */
#define IR_CENTER_X       320.0f
#define IR_CENTER_Y       240.0f
#define IR_DEADZONE       40.0f    /* pixels from center before aim registers */
#define IR_SENSITIVITY    0.15f    /* scale factor for IR delta -> mouse delta */
#define IR_MAX_DELTA      25.0f    /* clamp per-frame delta to prevent snaps */

/* Nunchuk joystick: mag is 0.0-1.0, ang is degrees from up clockwise */
#define NUNCHUK_DEADZONE  0.15f    /* magnitude below which stick is ignored */
#define NUNCHUK_SCALE     32767.0f /* scale to Q3 joystick range */

#endif /* WPAD_ENABLED */

/* ---------- Shared state ---------- */

typedef struct {
    qboolean key_held[256];
} input_state_t;

static input_state_t  s_input;
static qboolean       s_home_pressed   = qfalse;
static qboolean       s_in_game        = qfalse;
static float          s_accum_x        = 0.0f;
static float          s_accum_y        = 0.0f;
static float          s_accum_cx       = 0.0f;
static float          s_accum_cy       = 0.0f;
static short          s_old_axis[4];
static qboolean       s_old_ltrig      = qfalse;
static qboolean       s_old_rtrig      = qfalse;
static qboolean       s_bindings_set   = qfalse;

#if WPAD_ENABLED
static qboolean       s_wm_bindings_set = qfalse;
static float          s_ir_last_x       = IR_CENTER_X;
static float          s_ir_last_y       = IR_CENTER_Y;
static qboolean       s_ir_was_valid    = qfalse;
#ifdef WII_DEBUG
static int            s_wpad_diag_count = 0;
#endif
#endif

/* ---------- Shared helpers ---------- */

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
    s_old_axis[0] = s_old_axis[1] = s_old_axis[2] = s_old_axis[3] = 0;
}

static short GC_FilterAxis(s8 raw, int deadzone)
{
    int v = (int)raw;
    if (v > -deadzone && v < deadzone)
        return 0;
    if (v > 127) v = 127;
    if (v < -127) v = -127;
    return (short)(v * GC_AXIS_SCALE);
}

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

/* ---------- GC controller bindings ---------- */

static void SetGCBindings(void)
{
    if (s_bindings_set)
        return;
    s_bindings_set = qtrue;

    Key_SetBinding(K_JOY1,      "+moveup");     /* A = jump */
    Key_SetBinding(K_JOY2,      "+movedown");   /* B = crouch */
    Key_SetBinding(K_JOY3,      "weapprev");    /* X */
    Key_SetBinding(K_JOY4,      "weapnext");    /* Y */
    Key_SetBinding(K_JOY5,      "+zoom");       /* Z */
    Key_SetBinding(K_JOY6,      "togglemenu");  /* Start */
    Key_SetBinding(K_JOY7,      "+scores");     /* D-up */
    Key_SetBinding(K_JOY8,      "+attack");     /* D-down */
    Key_SetBinding(K_JOY9,      "weapprev");    /* D-left */
    Key_SetBinding(K_JOY10,     "weapnext");    /* D-right */
    Key_SetBinding(K_JOY_LTRIG, "+speed");      /* L = walk */
    Key_SetBinding(K_JOY_RTRIG, "+attack");     /* R = fire */
}

/* ---------- Wiimote+Nunchuk bindings ---------- */

#if WPAD_ENABLED
static void SetWiimoteBindings(void)
{
    if (s_wm_bindings_set)
        return;
    s_wm_bindings_set = qtrue;

    Key_SetBinding(K_JOY1,  "+attack");     /* B trigger = fire */
    Key_SetBinding(K_JOY2,  "+moveup");     /* A = jump */
    Key_SetBinding(K_JOY3,  "+zoom");       /* Nunchuk Z = zoom */
    Key_SetBinding(K_JOY4,  "+movedown");   /* Nunchuk C = crouch */
    Key_SetBinding(K_JOY5,  "togglemenu");  /* + = menu */
    Key_SetBinding(K_JOY6,  "+scores");     /* - = scores */
    Key_SetBinding(K_JOY7,  "weapnext");    /* D-up */
    Key_SetBinding(K_JOY8,  "weapprev");    /* D-down */
    Key_SetBinding(K_JOY9,  "weapprev");    /* D-left */
    Key_SetBinding(K_JOY10, "weapnext");    /* D-right */
    Key_SetBinding(K_JOY11, "+speed");      /* 1 = walk */
}
#endif

/* ---------- GC controller frame ---------- */

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
        for (i = 0; i < (int)GC_MENU_BTN_COUNT; i++)
            InjectKey(s_gc_menu_buttons[i].q3key,
                      (held & s_gc_menu_buttons[i].bit) ? qtrue : qfalse);

        InjectKey(K_MOUSE1, r_ana > TRIGGER_THRESHOLD ? qtrue : qfalse);

        InjectCursorStick(lx, ly, MENU_SENSITIVITY_F,
                          &s_accum_x, &s_accum_y);
        InjectCursorStick(cx, cy, MENU_SENSITIVITY_F,
                          &s_accum_cx, &s_accum_cy);
    } else {
        SetGCBindings();

        for (i = 0; i < (int)GC_BTN_COUNT; i++)
            InjectKey(s_gc_buttons[i].q3key,
                      (held & s_gc_buttons[i].bit) ? qtrue : qfalse);

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

        short ax_lx = GC_FilterAxis(lx, STICK_DEADZONE);
        short ax_ly = GC_FilterAxis(ly, STICK_DEADZONE);
        short ax_cx = GC_FilterAxis(cx, STICK_DEADZONE);
        short ax_cy = GC_FilterAxis(cy, STICK_DEADZONE);

        if (ax_lx != s_old_axis[0]) {
            Com_QueueEvent(0, SE_JOYSTICK_AXIS, AXIS_SIDE, ax_lx, 0, NULL);
            s_old_axis[0] = ax_lx;
        }
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

/* ---------- Wiimote+Nunchuk frame ---------- */

#if WPAD_ENABLED

/* Nunchuk stick -> Q3 joystick axes (mag/ang to X/Y) */
static void WM_NunchukMovement(const struct joystick_t *js)
{
    float mag = js->mag;
    if (mag < NUNCHUK_DEADZONE) {
        /* Center — send zero if we were previously non-zero */
        if (s_old_axis[0] != 0) {
            Com_QueueEvent(0, SE_JOYSTICK_AXIS, AXIS_SIDE, 0, 0, NULL);
            s_old_axis[0] = 0;
        }
        if (s_old_axis[1] != 0) {
            Com_QueueEvent(0, SE_JOYSTICK_AXIS, AXIS_FORWARD, 0, 0, NULL);
            s_old_axis[1] = 0;
        }
        return;
    }
    if (mag > 1.0f) mag = 1.0f;

    float rad = js->ang * 3.14159265f / 180.0f;
    float sin_a = __builtin_sinf(rad);
    float cos_a = __builtin_cosf(rad);

    short side = (short)(sin_a * mag * NUNCHUK_SCALE);
    short fwd  = (short)(-cos_a * mag * NUNCHUK_SCALE);

    if (side != s_old_axis[0]) {
        Com_QueueEvent(0, SE_JOYSTICK_AXIS, AXIS_SIDE, side, 0, NULL);
        s_old_axis[0] = side;
    }
    if (fwd != s_old_axis[1]) {
        Com_QueueEvent(0, SE_JOYSTICK_AXIS, AXIS_FORWARD, fwd, 0, NULL);
        s_old_axis[1] = fwd;
    }
}

/* IR pointer -> SE_MOUSE deltas (center-relative in-game, position-relative in menus) */
static void WM_IRAiming(const struct ir_t *ir, qboolean in_game)
{
    if (!ir->valid && !ir->smooth_valid) {
        s_ir_was_valid = qfalse;
        return;
    }

    float ix = ir->smooth_valid ? ir->sx : ir->x;
    float iy = ir->smooth_valid ? ir->sy : ir->y;

    if (in_game) {
        float dx = ix - IR_CENTER_X;
        float dy = iy - IR_CENTER_Y;

        if (dx > -IR_DEADZONE && dx < IR_DEADZONE) dx = 0.0f;
        if (dy > -IR_DEADZONE && dy < IR_DEADZONE) dy = 0.0f;

        if (dx == 0.0f && dy == 0.0f)
            return;

        dx *= IR_SENSITIVITY;
        dy *= IR_SENSITIVITY;

        if (dx > IR_MAX_DELTA) dx = IR_MAX_DELTA;
        if (dx < -IR_MAX_DELTA) dx = -IR_MAX_DELTA;
        if (dy > IR_MAX_DELTA) dy = IR_MAX_DELTA;
        if (dy < -IR_MAX_DELTA) dy = -IR_MAX_DELTA;

        int mx = (int)dx;
        int my = (int)dy;
        if (mx != 0 || my != 0)
            Com_QueueEvent(0, SE_MOUSE, mx, my, 0, NULL);
    } else {
        if (s_ir_was_valid) {
            float dx = ix - s_ir_last_x;
            float dy = iy - s_ir_last_y;
            int mx = (int)dx;
            int my = (int)dy;
            if (mx != 0 || my != 0)
                Com_QueueEvent(0, SE_MOUSE, mx, my, 0, NULL);
        }
        s_ir_last_x = ix;
        s_ir_last_y = iy;
        s_ir_was_valid = qtrue;
    }
}

static void WM_Input_Frame(void)
{
    int i;

    WPAD_ScanPads();
    s_home_pressed = qfalse;

    /* WPAD_Data returns stale data after disconnect; use Probe to detect it */
    u32 probe_type;
    if (WPAD_Probe(WPAD_CHAN_0, &probe_type) != WPAD_ERR_NONE) {
        GC_Input_Frame();
        return;
    }

    WPADData *data = WPAD_Data(WPAD_CHAN_0);
    if (!data || data->err != WPAD_ERR_NONE) {
        GC_Input_Frame();
        return;
    }

    u32 held = data->btns_h;

    if (held & WPAD_BUTTON_HOME)
        s_home_pressed = qtrue;

    PAD_ScanPads();

    qboolean in_game = (Key_GetCatcher() == 0) ? qtrue : qfalse;

    if (in_game != s_in_game) {
        ReleaseAllKeys();
        s_in_game = in_game;
        s_ir_was_valid = qfalse;
    }

    u32 exp_type = WPAD_EXP_NONE;
    WPAD_Probe(WPAD_CHAN_0, &exp_type);
    qboolean has_nunchuk = (exp_type == WPAD_EXP_NUNCHUK) ? qtrue : qfalse;

#ifdef WII_DEBUG
    if (++s_wpad_diag_count >= 300) {
        s_wpad_diag_count = 0;
        wii_diag("[wpad] exp=%d ir_valid=%d ir_smooth=%d ir=(%.0f,%.0f) btns=0x%08x\n",
                 (int)exp_type, data->ir.valid, data->ir.smooth_valid,
                 data->ir.x, data->ir.y, held);
        if (has_nunchuk) {
            wii_diag("[wpad] nunchuk mag=%.2f ang=%.1f btns=0x%02x\n",
                     data->exp.nunchuk.js.mag,
                     data->exp.nunchuk.js.ang,
                     data->exp.nunchuk.btns);
        }
    }
#endif

    if (!in_game) {
        /* ---- Menu mode ---- */
        for (i = 0; i < (int)WM_MENU_BTN_COUNT; i++)
            InjectKey(s_wm_menu_buttons[i].q3key,
                      (held & s_wm_menu_buttons[i].bit) ? qtrue : qfalse);

        if (has_nunchuk)
            InjectKey(K_MOUSE1,
                      (held & WPAD_NUNCHUK_BUTTON_Z) ? qtrue : qfalse);

        WM_IRAiming(&data->ir, qfalse);

        /* Nunchuk stick as fallback cursor */
        if (has_nunchuk) {
            struct joystick_t *js = &data->exp.nunchuk.js;
            if (js->mag > NUNCHUK_DEADZONE) {
                float rad = js->ang * 3.14159265f / 180.0f;
                float fx = __builtin_sinf(rad) * js->mag * MENU_SENSITIVITY_F;
                float fy = -__builtin_cosf(rad) * js->mag * MENU_SENSITIVITY_F;
                s_accum_x += fx;
                s_accum_y += fy;
                int ox = (int)s_accum_x;
                int oy = (int)s_accum_y;
                s_accum_x -= (float)ox;
                s_accum_y -= (float)oy;
                if (ox != 0 || oy != 0)
                    Com_QueueEvent(0, SE_MOUSE, ox, oy, 0, NULL);
            } else {
                s_accum_x = s_accum_y = 0.0f;
            }
        }
    } else {
        /* ---- Game mode ---- */
        SetWiimoteBindings();

        for (i = 0; i < (int)WM_BTN_COUNT; i++)
            InjectKey(s_wm_buttons[i].q3key,
                      (held & s_wm_buttons[i].bit) ? qtrue : qfalse);

        WM_IRAiming(&data->ir, qtrue);

        if (has_nunchuk) {
            WM_NunchukMovement(&data->exp.nunchuk.js);
        }
    }

}

#endif /* WPAD_ENABLED */

/* ---------- Public API ---------- */

void Wii_Input_Init(void)
{
    memset(&s_input, 0, sizeof(s_input));
    s_home_pressed  = qfalse;
    s_in_game       = qfalse;
    s_bindings_set  = qfalse;
    s_accum_x = s_accum_y = 0.0f;
    s_accum_cx = s_accum_cy = 0.0f;
    s_old_ltrig = s_old_rtrig = qfalse;
    memset(s_old_axis, 0, sizeof(s_old_axis));

    PAD_Init();

#if WPAD_ENABLED
    s_wm_bindings_set = qfalse;
    s_ir_last_x = IR_CENTER_X;
    s_ir_last_y = IR_CENTER_Y;
    s_ir_was_valid = qfalse;

    WPAD_Init();
    WPAD_SetDataFormat(WPAD_CHAN_0, WPAD_FMT_BTNS_ACC_IR);
    WPAD_SetVRes(WPAD_CHAN_0, 640, 480);
    WPAD_SetIdleTimeout(300); /* 5 minutes before auto-disconnect */

#ifdef WII_DEBUG
    printf("[input] Wiimote+Nunchuk initialised (IR aiming)\n");
#endif
#else
#ifdef WII_DEBUG
    printf("[input] GameCube PAD initialised (dual-stick)\n");
#endif
#endif
}

void Wii_Input_Frame(void)
{
#if WPAD_ENABLED
    WM_Input_Frame();
#else
    s_home_pressed = qfalse;
    GC_Input_Frame();
#endif
}

qboolean Wii_Input_HomePressed(void)
{
    return s_home_pressed;
}
