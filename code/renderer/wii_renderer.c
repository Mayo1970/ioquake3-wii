/*
 * wii_renderer.c — Wii renderer glue for ioquake3-wii.
 *
 * Responsibilities:
 *
 *   • Define the refimport_t ri global that all ioQ3 renderer modules use
 *     to call back into the engine (Printf, Malloc, FS_ReadFile, …).
 *
 *   • Provide a lightweight pre-boot refexport_t (wii_re) that is returned
 *     by GetRefAPI() before Com_Init() runs.  This stub makes early
 *     SCR_UpdateScreen() / re.BeginFrame() calls safe with no-ops; it does
 *     NOT call any GX or OpenGL functions so it cannot conflict with OpenGX's
 *     internal state.
 *
 *   • When GetRefAPI() is called with a valid refimport_t pointer (from
 *     CL_InitRef after the file system is up), call QGL_Init() to wire
 *     qgl* → OpenGX, then delegate to ioQ3's real renderer via
 *     tr_init_GetRefAPI_unused() (the renamed GetRefAPI in tr_init.c).
 *     From that point on the full renderergl1 pipeline is active:
 *       tr_backend + tr_shade + tr_surface + tr_image + tr_sky + tr_flares
 *     … all translated to GX hardware through OpenGX.
 *
 * tr_main.c's refimport_t ri is renamed to tr_main_ri_unused by
 * apply_patches.sh, so this file is the sole definition of ri.
 */

#undef COLOR_BLACK
#undef COLOR_RED
#undef COLOR_GREEN
#undef COLOR_YELLOW
#undef COLOR_BLUE
#undef COLOR_CYAN
#undef COLOR_MAGENTA
#undef COLOR_WHITE
#undef COLOR_ORANGE

#include <gccore.h>
#include <stdio.h>

#include "qcommon/q_shared.h"
#include "qcommon/qcommon.h"
#include "renderercommon/tr_types.h"
#include "renderercommon/tr_public.h"
#include "sys/wii_glimp.h"

/* ---- refimport_t ri -------------------------------------------------------
 * Defined here (sole definition — tr_main.c's copy is renamed by the patch).
 * Populated by wii_ri_init() from manual callbacks, then overwritten by the
 * real rimp when CL_InitRef calls GetRefAPI with a non-NULL rimp.
 * -------------------------------------------------------------------------- */
refimport_t ri;

static void QDECL wii_ri_Printf(int level, const char *fmt, ...)
{
    va_list ap;
    char buf[1024];
    va_start(ap, fmt);
    Q_vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    Com_Printf("%s", buf);
    (void)level;
}

static Q_NO_RETURN void QDECL wii_ri_Error(int level, const char *fmt, ...)
{
    va_list ap;
    char buf[1024];
    va_start(ap, fmt);
    Q_vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    Com_Error(level, "%s", buf);
}

static void *wii_ri_Hunk_Alloc(int size, ha_pref pref, char *label,
                                char *file, int line)
{
    return Hunk_AllocDebug(size, pref, label, file, line);
}

/* Z_Malloc may be a debug macro (Z_MallocDebug) in non-NDEBUG builds;
 * we cannot take its address directly, so wrap it. */
static void *wii_ri_Malloc(int size)
{
    return Z_Malloc(size);
}

static void wii_ri_init(void)
{
    ri.Printf                    = wii_ri_Printf;
    ri.Error                     = wii_ri_Error;
    ri.Milliseconds              = Sys_Milliseconds;
    ri.Hunk_AllocDebug           = wii_ri_Hunk_Alloc;
    ri.Hunk_AllocateTempMemory   = Hunk_AllocateTempMemory;
    ri.Hunk_FreeTempMemory       = Hunk_FreeTempMemory;
    ri.Malloc                    = wii_ri_Malloc;
    ri.Free                      = Z_Free;
    ri.Cvar_Get                  = Cvar_Get;
    ri.Cvar_Set                  = Cvar_Set;
    ri.Cvar_SetValue             = Cvar_SetValue;
    ri.Cvar_CheckRange           = Cvar_CheckRange;
    ri.Cvar_SetDescription       = Cvar_SetDescription;
    ri.Cvar_VariableIntegerValue = Cvar_VariableIntegerValue;
    ri.Cmd_AddCommand            = Cmd_AddCommand;
    ri.Cmd_RemoveCommand         = Cmd_RemoveCommand;
    ri.Cmd_Argc                  = Cmd_Argc;
    ri.Cmd_Argv                  = Cmd_Argv;
    ri.Cmd_ExecuteText           = Cbuf_ExecuteText;
    ri.CM_ClusterPVS             = CM_ClusterPVS;
    ri.CM_DrawDebugSurface       = CM_DrawDebugSurface;
    ri.FS_FileIsInPAK            = FS_FileIsInPAK;
    ri.FS_ReadFile               = FS_ReadFile;
    ri.FS_FreeFile               = FS_FreeFile;
    ri.FS_ListFiles              = FS_ListFiles;
    ri.FS_FreeFileList           = FS_FreeFileList;
    ri.FS_WriteFile              = FS_WriteFile;
}

/* ---- Pre-boot stub refexport_t -------------------------------------------
 * All functions are pure no-ops.  s_EndFrame flips the display so the Wii
 * console text (from Wii_InitConsole) stays visible during loading.
 * IMPORTANT: no qgl* / GX calls here — OpenGX may not be fully set up yet
 * and calling raw GX functions at this stage corrupts its internal state.
 * -------------------------------------------------------------------------- */

static void s_Shutdown(qboolean d)  { (void)d; }

static void s_BeginRegistration(glconfig_t *c)
{
    if (!c) return;
    GXRModeObj *r = Wii_GX_GetRMode();
    c->vidWidth             = r ? (int)r->fbWidth   : 640;
    c->vidHeight            = r ? (int)r->efbHeight : 480;
    c->windowAspect         = (float)c->vidWidth / (float)c->vidHeight;
    c->colorBits            = 24;
    c->depthBits            = 24;
    c->stencilBits          = 0;
    c->deviceSupportsGamma  = qfalse;
    c->textureCompression   = TC_NONE;
    c->textureEnvAddAvailable = qfalse;
    Q_strncpyz(c->renderer_string, "GX/Wii (pre-boot)", sizeof(c->renderer_string));
    Q_strncpyz(c->vendor_string,   "Nintendo",          sizeof(c->vendor_string));
    Q_strncpyz(c->version_string,  "GX 1.0",            sizeof(c->version_string));
}

static qhandle_t s_RegisterModel(const char *n)       { (void)n; return 0; }
static qhandle_t s_RegisterSkin(const char *n)        { (void)n; return 0; }
static qhandle_t s_RegisterShader(const char *n)      { (void)n; return 0; }
static qhandle_t s_RegisterShaderNoMip(const char *n) { (void)n; return 0; }
static void      s_LoadWorld(const char *n)            { (void)n; }
static void      s_SetWorldVisData(const byte *v)      { (void)v; }
static void      s_EndRegistration(void)               { }
static void      s_ClearScene(void)                    { }
static void      s_AddRefEntityToScene(const refEntity_t *e) { (void)e; }
static void      s_AddPolyToScene(qhandle_t h, int n,
                     const polyVert_t *v, int p)
    { (void)h;(void)n;(void)v;(void)p; }
static int       s_LightForPoint(vec3_t p, vec3_t a,
                     vec3_t d, vec3_t dir)
    { (void)p;(void)a;(void)d;(void)dir; return 0; }
static void      s_AddLightToScene(const vec3_t o, float r,
                     float i, float g, float b)
    { (void)o;(void)r;(void)i;(void)g;(void)b; }
static void      s_AddAdditiveLightToScene(const vec3_t o, float r,
                     float i, float g, float b)
    { (void)o;(void)r;(void)i;(void)g;(void)b; }
static void      s_RenderScene(const refdef_t *rd)     { (void)rd; }
static void      s_SetColor(const float *rgba)         { (void)rgba; }
static void      s_DrawStretchPic(float x, float y, float w, float h,
                     float s1, float t1, float s2, float t2, qhandle_t sh)
    { (void)x;(void)y;(void)w;(void)h;(void)s1;(void)t1;(void)s2;(void)t2;(void)sh; }
static void      s_DrawStretchRaw(int x, int y, int w, int h,
                     int cols, int rows, const byte *d, int c, qboolean dirty)
    { (void)x;(void)y;(void)w;(void)h;(void)cols;(void)rows;(void)d;(void)c;(void)dirty; }
static void      s_UploadCinematic(int w, int h, int cols, int rows,
                     const byte *d, int c, qboolean dirty)
    { (void)w;(void)h;(void)cols;(void)rows;(void)d;(void)c;(void)dirty; }
static void      s_BeginFrame(stereoFrame_t sf)  { (void)sf; }
static void      s_EndFrame(int *f, int *b)
{
    (void)f; (void)b;
    /* Flip the display so console text stays visible during boot/loading */
    Wii_GX_EndFrame();
}
static int       s_MarkFragments(int n, const vec3_t *o, const vec3_t proj,
                     int ms, vec_t *pts, int mf, markFragment_t *mfp)
    { (void)n;(void)o;(void)proj;(void)ms;(void)pts;(void)mf;(void)mfp; return 0; }
static int       s_LerpTag(orientation_t *tag, qhandle_t h, int sf,
                     int ef, float f, const char *name)
    { (void)tag;(void)h;(void)sf;(void)ef;(void)f;(void)name; return 0; }
static void      s_ModelBounds(qhandle_t h, vec3_t mn, vec3_t mx)
    { (void)h; VectorClear(mn); VectorClear(mx); }
static void      s_RegisterFont(const char *n, int ps, fontInfo_t *fi)
    { (void)n;(void)ps;(void)fi; }
static void      s_RemapShader(const char *o, const char *n, const char *off)
    { (void)o;(void)n;(void)off; }
static qboolean  s_GetEntityToken(char *buf, int s)
    { (void)buf;(void)s; return qfalse; }
static qboolean  s_inPVS(const vec3_t p1, const vec3_t p2)
    { (void)p1;(void)p2; return qtrue; }
static void      s_TakeVideoFrame(int h, int w, byte *cap, byte *enc,
                     qboolean mj)
    { (void)h;(void)w;(void)cap;(void)enc;(void)mj; }

static refexport_t wii_re = {
    s_Shutdown,            s_BeginRegistration,
    s_RegisterModel,       s_RegisterSkin,
    s_RegisterShader,      s_RegisterShaderNoMip,
    s_LoadWorld,           s_SetWorldVisData,    s_EndRegistration,
    s_ClearScene,          s_AddRefEntityToScene, s_AddPolyToScene,
    s_LightForPoint,       s_AddLightToScene,    s_AddAdditiveLightToScene,
    s_RenderScene,         s_SetColor,           s_DrawStretchPic,
    s_DrawStretchRaw,      s_UploadCinematic,
    s_BeginFrame,          s_EndFrame,
    s_MarkFragments,       s_LerpTag,            s_ModelBounds,
    s_RegisterFont,        s_RemapShader,        s_GetEntityToken,
    s_inPVS,               s_TakeVideoFrame,
};

/* ---- GetRefAPI ------------------------------------------------------------
 * Called twice during a normal boot:
 *
 *   1. From wii_main.c BEFORE Com_Init(), rimp == NULL.
 *      Return the pre-boot stub (wii_re) so any early SCR_UpdateScreen()
 *      calls find non-NULL function pointers.
 *
 *   2. From CL_InitRef() AFTER the file system is initialised, rimp != NULL.
 *      Wire qgl* → OpenGX, then delegate to ioQ3's real renderer
 *      (tr_init_GetRefAPI_unused).  From this point on the full renderergl1
 *      pipeline is active and 3D rendering works through OpenGX → GX.
 * -------------------------------------------------------------------------- */
extern void QGL_Init(void);
extern refexport_t *tr_init_GetRefAPI_unused(int apiVersion, refimport_t *rimp);

refexport_t *GetRefAPI(int apiVersion, refimport_t *rimp)
{
    /* Always refresh our manual ri callbacks first */
    wii_ri_init();

    /* If the engine passed a proper import table, use it */
    if (rimp)
        ri = *rimp;

    if (!rimp) {
        /* Pre-boot call: return the safe no-op stub */
        return &wii_re;
    }

    /* Real init: activate OpenGX and hand off to the real renderer */
    QGL_Init();

    return tr_init_GetRefAPI_unused(apiVersion, &ri);
}
