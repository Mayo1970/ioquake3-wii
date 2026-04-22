/*
 * ioquake3-wii: input/wii_input.h
 * Wiimote + Nunchuk input declarations.
 */
#pragma once
#include "qcommon/q_shared.h"

/* Call once at startup */
void     Wii_Input_Init(void);

/* Call once per frame BEFORE Com_Frame() */
void     Wii_Input_Frame(void);

/* Returns qtrue if the HOME button was pressed this frame (triggers quit) */
qboolean Wii_Input_HomePressed(void);

/*
 * Called by ioQ3's Sys_GetEvent().
 * Drains the Wii input queue and injects sysEvent_t events into Q3's
 * event system via Com_QueueEvent().
 */
void     Wii_Input_GenerateEvents(void);
