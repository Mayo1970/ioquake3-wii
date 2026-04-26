/* GX window / surface management declarations. */
#pragma once
#include <gccore.h>
#include "qcommon/q_shared.h"

qboolean    Wii_GX_Init(void);
void        Wii_GX_EndFrame(void);
void        Wii_GX_Shutdown(void);
GXRModeObj *Wii_GX_GetRMode(void);
