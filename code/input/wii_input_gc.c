/*
 * ioquake3-wii: input/wii_input_gc.c
 *
 * GameCube controller input layer — dual-mode mapping.
 *
 * Mode is selected each frame by Key_GetCatcher():
 *
 * MENU mode  (Key_GetCatcher() != 0  — UI/console has focus):
 *   Left stick + C-stick → SE_MOUSE (cursor movement)
 *   A  → K_MOUSE1   B  → K_ESCAPE   X  → K_ENTER   Y  → K_CONSOLE
 *   Z  → K_MOUSE1   Start → K_ESCAPE
 *   D-pad ↑↓←→ → K_UPARROW / K_DOWNARROW / K_LEFTARROW / K_RIGHTARROW
 *   L/R triggers → no-op
 *
 * GAME mode  (Key_GetCatcher() == 0  — player has full control):
 *   Left stick → movement keys (+forward/+back/+moveleft/+moveright)
 *   C-stick    → SE_MOUSE (camera look)
 *   A  → K_SPACE (jump)      B  → K_CTRL (crouch)
 *   X  → ']' (next weapon)   Y  → '[' (prev weapon)
 *   Z  → K_ENTER (use item)  Start → K_ESCAPE (open menu)
 *   D-pad ↑↓←→ → movement (same keys as left stick)
 *   R trigger → K_MOUSE1 (primary fire)
 *   L trigger → K_MOUSE2 (secondary fire / zoom)
 *
 * All held keys are released on every mode transition to prevent stuck inputs.
 *
 * Build with -DINPUT_GAMECUBE to select this backend.
 * Default (no flag) = Wiimote + Nunchuk (wii_input.c).
 */

#include <gccore.h>
#include <ogc/pad.h>
#include <string.h>
#include <stdio.h>

#include "wii_input.h"

#include "qcommon/q_shared.h"
#include "qcommon/qcommon.h"
#include "keycodes.h"

/* Key_GetCatcher() is defined in client/cl_keys.c */
extern int Key_GetCatcher(void);

/* --------------------------------------------------------------------------
 * Configuration
 * -------------------------------------------------------------------------- */
#define GC_PORT            PAD_CHAN0
#define STICK_DEADZONE      20     /* dead zone on raw -127..127 stick range */
#define CSTICK_DEADZONE     20
#define TRIGGER_THRESHOLD  100     /* 0-255 analogue trigger digital threshold */

/* Pixels / frame at full deflection for SE_MOUSE events */
#define MENU_SENSITIVITY_F  2.0f   /* cursor speed in menus */
#define LOOK_SENSITIVITY_F  5.0f   /* camera look speed in-game */

/* --------------------------------------------------------------------------
 * State
 * -------------------------------------------------------------------------- */
typedef struct {
    qboolean key_held[512];
} gc_input_state_t;

static gc_input_state_t s_input;
static float            s_mouse_accum_x = 0.0f, s_mouse_accum_y = 0.0f;
static float            s_look_accum_x  = 0.0f, s_look_accum_y  = 0.0f;
static qboolean         s_in_game = qfalse;   /* last known mode */

/* --------------------------------------------------------------------------
 * Helper: inject key on state change only
 * -------------------------------------------------------------------------- */
static void InjectKey(int q3key, qboolean down)
{
    if (s_input.key_held[q3key] == down)
        return;
    s_input.key_held[q3key] = down;
    Com_QueueEvent(0, SE_KEY, q3key, down, 0, NULL);
}

/* Release every key that is currently held and drain both accumulators.
 * Called whenever the mode switches to prevent stuck inputs. */
static void ReleaseAllKeys(void)
{
    int i;
    for (i = 0; i < 512; i++) {
        if (s_input.key_held[i]) {
            s_input.key_held[i] = qfalse;
            Com_QueueEvent(0, SE_KEY, i, qfalse, 0, NULL);
        }
    }
    s_mouse_accum_x = s_mouse_accum_y = 0.0f;
    s_look_accum_x  = s_look_accum_y  = 0.0f;
}

/* --------------------------------------------------------------------------
 * Stick helpers
 * -------------------------------------------------------------------------- */

/* SE_MOUSE: used for cursor movement in menus and camera look in-game.
 * Float accumulator carries over sub-pixel fractional movement. */
static void InjectMouseStick(s8 x, s8 y, int deadzone, float sensitivity,
                              float *accum_x, float *accum_y)
{
    int ix = (int)x;
    int iy = (int)y;

    if (ix > -deadzone && ix < deadzone) ix = 0;
    if (iy > -deadzone && iy < deadzone) iy = 0;

    if (ix == 0 && iy == 0) {
        *accum_x = *accum_y = 0.0f;
        return;
    }

    *accum_x += ((float)ix / 127.0f) * sensitivity;
    *accum_y += ((float)iy / 127.0f) * sensitivity;

    int out_x = (int)*accum_x;
    int out_y = (int)*accum_y;
    *accum_x -= (float)out_x;
    *accum_y -= (float)out_y;

    if (out_x == 0 && out_y == 0) return;
    /* Q3: +x = right, +y = down; GC stick: +y = up, so negate y */
    Com_QueueEvent(0, SE_MOUSE, out_x, -out_y, 0, NULL);
}

/* Key injection for left-stick movement in game mode.
 * GC stick: +Y = up = forward, +X = right = strafe right. */
static void InjectMoveStick(s8 x, s8 y)
{
    InjectKey(K_UPARROW,   (int)y >  STICK_DEADZONE ? qtrue : qfalse);
    InjectKey(K_DOWNARROW, (int)y < -STICK_DEADZONE ? qtrue : qfalse);
    InjectKey('d',         (int)x >  STICK_DEADZONE ? qtrue : qfalse);
    InjectKey('a',         (int)x < -STICK_DEADZONE ? qtrue : qfalse);
}

/* --------------------------------------------------------------------------
 * Button maps
 * -------------------------------------------------------------------------- */
typedef struct { u32 pad_btn; int q3key; } btn_map_t;

/* MENU mode */
static const btn_map_t s_menu_btns[] = {
    { PAD_BUTTON_A,     K_MOUSE1     },
    { PAD_BUTTON_B,     K_ESCAPE     },
    { PAD_BUTTON_X,     K_ENTER      },
    { PAD_BUTTON_Y,     K_CONSOLE    },
    { PAD_TRIGGER_Z,    K_MOUSE1     },
    { PAD_BUTTON_START, K_ESCAPE     },
    { PAD_BUTTON_UP,    K_UPARROW    },
    { PAD_BUTTON_DOWN,  K_DOWNARROW  },
    { PAD_BUTTON_LEFT,  K_LEFTARROW  },
    { PAD_BUTTON_RIGHT, K_RIGHTARROW },
};
#define MENU_BTN_COUNT (sizeof(s_menu_btns) / sizeof(s_menu_btns[0]))

/* GAME mode */
static const btn_map_t s_game_btns[] = {
    { PAD_BUTTON_A,     K_SPACE      },   /* jump              */
    { PAD_BUTTON_B,     'c'       },   /* crouch            */
    { PAD_BUTTON_X,     ']'          },   /* next weapon       */
    { PAD_BUTTON_Y,     '['          },   /* prev weapon       */
    { PAD_TRIGGER_Z,    K_ENTER      },   /* use item          */
    { PAD_BUTTON_START, K_ESCAPE     },   /* open menu         */
    { PAD_BUTTON_UP,    K_UPARROW    },   /* d-pad forward     */
    { PAD_BUTTON_DOWN,  K_DOWNARROW  },   /* d-pad backward    */
    { PAD_BUTTON_LEFT,  'a'          },   /* d-pad strafe left */
    { PAD_BUTTON_RIGHT, 'd'          },   /* d-pad strafe rgt  */
};
#define GAME_BTN_COUNT (sizeof(s_game_btns) / sizeof(s_game_btns[0]))

/* --------------------------------------------------------------------------
 * Wii_Input_Init
 * -------------------------------------------------------------------------- */
void Wii_Input_Init(void)
{
    memset(&s_input, 0, sizeof(s_input));
    s_mouse_accum_x = s_mouse_accum_y = 0.0f;
    s_look_accum_x  = s_look_accum_y  = 0.0f;
    s_in_game = qfalse;
    PAD_Init();
    printf("[input] GameCube PAD initialised (port 0)\n");
}

/* --------------------------------------------------------------------------
 * Wii_Input_Frame
 * -------------------------------------------------------------------------- */
void Wii_Input_Frame(void)
{
    PAD_ScanPads();

    /* Determine current mode */
    qboolean in_game = (Key_GetCatcher() == 0) ? qtrue : qfalse;

    /* Release all keys on mode transition */
    if (in_game != s_in_game) {
        ReleaseAllKeys();
        s_in_game = in_game;
    }

    u32 held = PAD_ButtonsHeld(GC_PORT);
    s8  lx   = PAD_StickX(GC_PORT);
    s8  ly   = PAD_StickY(GC_PORT);
    s8  cx   = PAD_SubStickX(GC_PORT);
    s8  cy   = PAD_SubStickY(GC_PORT);
    u8  l_ana = PAD_TriggerL(GC_PORT);
    u8  r_ana = PAD_TriggerR(GC_PORT);

    if (!in_game) {
        /* ---- MENU MODE ---- */
        int i;
        for (i = 0; i < (int)MENU_BTN_COUNT; i++)
            InjectKey(s_menu_btns[i].q3key,
                      (held & s_menu_btns[i].pad_btn) ? qtrue : qfalse);

        /* Triggers inactive in menus */
        InjectKey(K_MOUSE2, qfalse);
        InjectKey(K_MOUSE1, qfalse);

        /* Both sticks move the cursor */
        InjectMouseStick(lx, ly, STICK_DEADZONE,  MENU_SENSITIVITY_F,
                         &s_mouse_accum_x, &s_mouse_accum_y);
        InjectMouseStick(cx, cy, CSTICK_DEADZONE, MENU_SENSITIVITY_F,
                         &s_look_accum_x,  &s_look_accum_y);

    } else {
        /* ---- GAME MODE ---- */
        int i;
        for (i = 0; i < (int)GAME_BTN_COUNT; i++)
            InjectKey(s_game_btns[i].q3key,
                      (held & s_game_btns[i].pad_btn) ? qtrue : qfalse);

        /* Triggers → fire */
        InjectKey(K_MOUSE2, l_ana > TRIGGER_THRESHOLD ? qtrue : qfalse);
        InjectKey(K_MOUSE1, r_ana > TRIGGER_THRESHOLD ? qtrue : qfalse);

        /* Left stick → movement keys */
        InjectMoveStick(lx, ly);

        /* C-stick → camera look (SE_MOUSE) */
        InjectMouseStick(cx, cy, CSTICK_DEADZONE, LOOK_SENSITIVITY_F,
                         &s_look_accum_x, &s_look_accum_y);
    }
}

/* --------------------------------------------------------------------------
 * Wii_Input_HomePressed
 * The GC controller has no HOME button.
 * -------------------------------------------------------------------------- */
qboolean Wii_Input_HomePressed(void)
{
    return qfalse;
}

/* --------------------------------------------------------------------------
 * Wii_Input_GenerateEvents – events already injected in Frame()
 * -------------------------------------------------------------------------- */
void Wii_Input_GenerateEvents(void)
{
}
