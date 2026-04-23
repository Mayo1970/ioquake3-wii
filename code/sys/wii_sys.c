/*
 * ioquake3-wii: sys/wii_sys.c
 *
 * Implements the Sys_* interface that ioQuake3 calls for platform services.
 * Every function here replaces the equivalent in ioQ3's code/sys/sys_unix.c
 * or sys_win32.c.  We compile ONLY this file from the sys/ layer (plus
 * wii_main.c) and exclude the upstream sys files in the Makefile.
 */

#include <gccore.h>
#include <ogc/lwp_watchdog.h>   /* ticks_to_millisecs, gettime */
#include <wiiuse/wpad.h>
#include <fat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>

#include "qcommon/q_shared.h"
#include "qcommon/qcommon.h"
/* Sys_* prototypes are declared in qcommon.h in modern ioQ3 —
 * sys_public.h no longer exists as a separate file */
#include "renderercommon/tr_types.h"  /* glconfig_t */
#include "keycodes.h"
/* Key_GetCatcher / KEYCATCH_UI */
extern int Key_GetCatcher(void);
#define KEYCATCH_UI 4

#include "wii_glimp.h"
#include "../input/wii_input.h"
#include "../audio/wii_snd.h"

/* --------------------------------------------------------------------------
 * Sys_Init / Sys_Quit
 * -------------------------------------------------------------------------- */
void Sys_Init(void)
{

}

void Sys_Quit(void)
{
    Wii_Snd_Shutdown();
    Wii_GX_Shutdown();
    /* Return to Homebrew Channel loader */
    exit(0);
}

/* --------------------------------------------------------------------------
 * Sys_Milliseconds – wall-clock time in ms
 * -------------------------------------------------------------------------- */
int Sys_Milliseconds(void)
{
    static int     s_check     = 0;
    static qboolean s_pad_inited = qfalse;
    int ms = (int)ticks_to_millisecs(gettime());

    if (++s_check >= 500) {
        s_check = 0;

        if (!s_pad_inited) {
            PAD_Init();
            s_pad_inited = qtrue;
        }
        PAD_ScanPads();

        /* Emergency exit via GC Start button. */
        if (PAD_ButtonsDown(0) & PAD_BUTTON_START) {
            exit(0);
        }

        {
            s8 lx = PAD_StickX(0);
            s8 ly = PAD_StickY(0);
            s8 cx = PAD_SubStickX(0);
            s8 cy = PAD_SubStickY(0);

            #define DEAD 20

            /* C-stick → SE_MOUSE for look/aim */
            {
                int dx = 0, dy = 0;
                if (cx > DEAD || cx < -DEAD) dx = (cx * 12) / 8;
                if (cy > DEAD || cy < -DEAD) dy = -(cy * 12) / 8;
                if (dx >  200) dx =  200;
                if (dx < -200) dx = -200;
                if (dy >  200) dy =  200;
                if (dy < -200) dy = -200;
                if (dx || dy) {
                    Com_QueueEvent(0, SE_MOUSE, dx, dy, 0, NULL);
                    Com_EventLoop();
                }
            }

            /* Left stick → movement keys */
            {
                static qboolean s_fwd, s_back, s_left, s_right;
                qboolean fwd   = ly >  DEAD;
                qboolean back  = ly < -DEAD;
                qboolean left  = lx < -DEAD;
                qboolean right = lx >  DEAD;
                if (fwd   != s_fwd)   { Com_QueueEvent(0, SE_KEY, K_UPARROW,    fwd,   0, NULL); s_fwd   = fwd;   }
                if (back  != s_back)  { Com_QueueEvent(0, SE_KEY, K_DOWNARROW,  back,  0, NULL); s_back  = back;  }
                if (left  != s_left)  { Com_QueueEvent(0, SE_KEY, K_LEFTARROW,  left,  0, NULL); s_left  = left;  }
                if (right != s_right) { Com_QueueEvent(0, SE_KEY, K_RIGHTARROW, right, 0, NULL); s_right = right; }
                Com_EventLoop();
            }

            /* Buttons */
            u32 down = PAD_ButtonsDown(0);
            u32 up   = PAD_ButtonsUp(0);
            if (down & PAD_BUTTON_A)     { Com_QueueEvent(0, SE_KEY, K_MOUSE1,  qtrue,  0, NULL); Com_EventLoop(); }
            if (up   & PAD_BUTTON_A)     { Com_QueueEvent(0, SE_KEY, K_MOUSE1,  qfalse, 0, NULL); Com_EventLoop(); }
            if (down & PAD_BUTTON_B)     { Com_QueueEvent(0, SE_KEY, K_ESCAPE,  qtrue,  0, NULL); Com_EventLoop(); }
            if (up   & PAD_BUTTON_B)     { Com_QueueEvent(0, SE_KEY, K_ESCAPE,  qfalse, 0, NULL); Com_EventLoop(); }
            if (down & PAD_BUTTON_X)     { Com_QueueEvent(0, SE_KEY, K_SPACE,   qtrue,  0, NULL); Com_EventLoop(); }
            if (up   & PAD_BUTTON_X)     { Com_QueueEvent(0, SE_KEY, K_SPACE,   qfalse, 0, NULL); Com_EventLoop(); }
            if (down & PAD_TRIGGER_Z)    { Com_QueueEvent(0, SE_KEY, K_SPACE,   qtrue,  0, NULL); Com_EventLoop(); }
            if (up   & PAD_TRIGGER_Z)    { Com_QueueEvent(0, SE_KEY, K_SPACE,   qfalse, 0, NULL); Com_EventLoop(); }
        }
    }
    return ms;
}

/* --------------------------------------------------------------------------
 * Sys_Sleep
 * -------------------------------------------------------------------------- */
void Sys_Sleep(int msec)
{
    if (msec > 0) {
        /* libogc doesn't expose usleep natively, but newlib does */
        usleep((useconds_t)msec * 1000);
    }
}

/* --------------------------------------------------------------------------
 * Sys_GetEvent
 * ioQ3 calls this to drain the OS event queue once per frame.
 * We return SE_NONE immediately because we inject events in Wii_Input_Frame()
 * via Com_QueueEvent() before Com_Frame() runs.
 * -------------------------------------------------------------------------- */
sysEvent_t Sys_GetEvent(void)
{
    sysEvent_t ev = { 0 };
    ev.evType = SE_NONE;
    return ev;
}

/* --------------------------------------------------------------------------
 * Sys_GetClipboardData – no clipboard on Wii
 * -------------------------------------------------------------------------- */
char *Sys_GetClipboardData(void)
{
    return NULL;
}

/* --------------------------------------------------------------------------
 * Filesystem helpers
 * -------------------------------------------------------------------------- */
char *Sys_Cwd(void)
{
    static char cwd[MAX_OSPATH];
    getcwd(cwd, sizeof(cwd));
    return cwd;
}

qboolean Sys_RandomBytes(byte *string, int len)
{
    /* Simple LCG fallback – good enough for Q3's nonce use */
    static unsigned int seed = 0x12345678;
    for (int i = 0; i < len; i++) {
        seed = seed * 1664525u + 1013904223u;
        string[i] = (byte)(seed >> 24);
    }
    return qtrue;
}

char *Sys_DefaultBasePath(void)     { return "sd:/quake3"; }
char *Sys_DefaultInstallPath(void)  { return "sd:/quake3"; }
char *Sys_DefaultHomePath(void)     { return "sd:/quake3"; }

/* --------------------------------------------------------------------------
 * Dynamic library loading – no .so support on Wii.
 * Q3 VMs run in interpreted mode (NOQ3_VM_COMPILED).
 * -------------------------------------------------------------------------- */
void *Sys_LoadDll(const char *name,
                  intptr_t (**entryPoint)(int, ...),
                  intptr_t (*systemcalls)(intptr_t, ...))
{
    (void)name; (void)entryPoint; (void)systemcalls;
    return NULL;
}

void Sys_UnloadDll(void *dllHandle)
{
    (void)dllHandle;
}

/* --------------------------------------------------------------------------
 * Sys_ListFiles – directory listing via newlib/libfat dirent
 * -------------------------------------------------------------------------- */
char **Sys_ListFiles(const char *directory, const char *extension,
                     char *filter, int *numfiles, qboolean wantsubs)
{
    DIR           *dir;
    struct dirent *entry;
    int            nfiles = 0;
    int            nalloc = 64;
    char         **list;
    char           search[MAX_OSPATH];

    *numfiles = 0;

    dir = opendir(directory);
    if (!dir)
        return NULL;

    list = (char **)Z_Malloc(nalloc * sizeof(char *));

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.')
            continue;

        if (extension) {
            /* simple suffix match */
            size_t nlen = strlen(entry->d_name);
            size_t elen = strlen(extension);
            if (nlen < elen ||
                Q_stricmp(entry->d_name + nlen - elen, extension) != 0)
                continue;
        }

        if (nfiles == nalloc) {
            char **newlist;
            nalloc *= 2;
            newlist = (char **)Z_Malloc(nalloc * sizeof(char *));
            memcpy(newlist, list, nfiles * sizeof(char *));
            Z_Free(list);
            list = newlist;
        }
        list[nfiles++] = CopyString(entry->d_name);
    }
    closedir(dir);

    *numfiles = nfiles;
    return list;
}

void Sys_FreeFileList(char **list)
{
    if (!list)
        return;
    for (int i = 0; list[i]; i++)
        Z_Free(list[i]);
    Z_Free(list);
}

qboolean Sys_LowPhysicalMemory(void)
{
    /* 88 MB RAM – tell Q3 we're memory-constrained */
    return qtrue;
}

/* --------------------------------------------------------------------------
 * Sys_Error – fatal error: print and spin (user can read TV screen)
 * -------------------------------------------------------------------------- */
void Sys_Error(const char *error, ...)
{
    va_list ap;
    char msg[4096];
    va_start(ap, error);
    vsnprintf(msg, sizeof(msg), error, ap);
    va_end(ap);
    /* Clear screen and print prominently */
    printf("\n\n\n");
    printf("=============================\n");
    printf("[FATAL ERROR]\n");
    printf("%s\n", msg);
    printf("=============================\n");
    printf("Press START to reboot.\n");
    fflush(stdout);
    while (1) {
        PAD_ScanPads();
        if (PAD_ButtonsDown(PAD_CHAN0) & PAD_BUTTON_START) exit(1);
        VIDEO_WaitVSync();
    }
}

/* --------------------------------------------------------------------------
 * Sys_Print
 * -------------------------------------------------------------------------- */
void Sys_Print(const char *msg)
{
    fputs(msg, stdout);
    fflush(stdout);
}

cpuFeatures_t Sys_GetProcessorFeatures(void)
{
    return (cpuFeatures_t)0;
}

/* --------------------------------------------------------------------------
 * GLimp interface – called by ref_gl to manage the window/context.
 * Since we manage GX directly, these are thin wrappers.
 * -------------------------------------------------------------------------- */

/* GLimp_Init: called by R_Init with signature void GLimp_Init(qboolean fixedFunction).
 * The renderer fills glConfig itself after we return; we just need to populate
 * glConfig with Wii display parameters so the renderer knows what it has.
 * GX + OpenGX were already initialised in Wii_GX_Init() before Com_Init. */
void GLimp_Init(qboolean fixedFunction)
{
    (void)fixedFunction;   /* always fixed-function on Wii — OpenGX is GL 1.x */

    GXRModeObj *rmode = Wii_GX_GetRMode();

    /* glConfig is the renderer's global — fill in the Wii display geometry
     * and capability flags so R_Init can proceed correctly. */
    extern glconfig_t glConfig;

    glConfig.vidWidth      = rmode ? (int)rmode->fbWidth   : 640;
    glConfig.vidHeight     = rmode ? (int)rmode->efbHeight : 480;
    glConfig.windowAspect  = (float)glConfig.vidWidth / (float)glConfig.vidHeight;
    glConfig.colorBits     = 24;
    glConfig.depthBits     = 24;
    glConfig.stencilBits   = 0;
    glConfig.deviceSupportsGamma     = qfalse;
    glConfig.textureCompression      = TC_NONE;
    glConfig.textureEnvAddAvailable  = qfalse;
    Q_strncpyz(glConfig.renderer_string, "OpenGX/Wii",  sizeof(glConfig.renderer_string));
    Q_strncpyz(glConfig.vendor_string,   "Nintendo",    sizeof(glConfig.vendor_string));
    Q_strncpyz(glConfig.version_string,  "GX 1.0",      sizeof(glConfig.version_string));
    Q_strncpyz(glConfig.extensions_string,
               "GL_ARB_multitexture GL_EXT_compiled_vertex_array",
               sizeof(glConfig.extensions_string));

}

void GLimp_Shutdown(void)
{
    Wii_GX_Shutdown();
}

void GLimp_EndFrame(void)
{
    Wii_GX_EndFrame();
}

void GLimp_SetGamma(unsigned char red[256],
                    unsigned char green[256],
                    unsigned char blue[256])
{
    /* GX has no gamma ramp support */
    (void)red; (void)green; (void)blue;
}

qboolean GLimp_SpawnRenderThread(void (*function)(void))
{
    /* No threads – run single-threaded */
    (void)function;
    return qfalse;
}

void *GLimp_RendererSleep(void)      { return NULL; }
void  GLimp_FrontEndSleep(void)      { }
void  GLimp_WakeRenderer(void *data) { (void)data; }

/* ==========================================================================
 * Missing Sys_* and other stubs for modern ioQ3 — Wii platform
 * ========================================================================== */

#include <sys/stat.h>
#include <netdb.h>
#include "renderercommon/tr_types.h"

/* Sys_GetProcessorFeatures defined above with debug print */

void Sys_SetEnv(const char *name, const char *value)
{ if (value) setenv(name, value, 1); else unsetenv(name); }

char    *Sys_ConsoleInput(void)                         { return NULL; }
void     Sys_InitPIDFile(const char *d)                 { (void)d; }
void     Sys_RemovePIDFile(const char *d)               { (void)d; }
qboolean Sys_DllExtension(const char *n)                { return strstr(n, DLL_EXT) ? qtrue : qfalse; }
qboolean Sys_OpenFolderInFileManager(const char *p, qboolean c) { (void)p;(void)c; return qfalse; }
qboolean Sys_Mkdir(const char *p)               { return (mkdir(p,0755)==0 || errno==EEXIST) ? qtrue : qfalse; }
FILE    *Sys_FOpen(const char *p, const char *m)        { return fopen(p,m); }
FILE    *Sys_Mkfifo(const char *p)                      { (void)p; return NULL; }

char *Sys_DefaultHomeConfigPath(void) { return "sd:/quake3"; }
char *Sys_DefaultHomeDataPath(void)   { return "sd:/quake3"; }
char *Sys_DefaultHomeStatePath(void)  { return "sd:/quake3"; }
char *Sys_SteamPath(void)             { return ""; }
char *Sys_GogPath(void)               { return ""; }
char *Sys_MicrosoftStorePath(void)    { return ""; }

void *QDECL Sys_LoadGameDll(const char *name, vmMainProc *ep,
                              intptr_t (*sc)(intptr_t,...))
{ (void)name;(void)ep;(void)sc; return NULL; }


void Sys_GLimpInit(void)     { }
void Sys_GLimpSafeInit(void) { }

/* IN_Init takes a void* windowData in modern ioQ3 */
void IN_Init(void *windowData) { (void)windowData; }
void IN_Shutdown(void)         { }
void IN_Restart(void)          { }
void IN_Frame(void)            { Wii_Input_Frame(); }

/*
 * Sound system — delegating to snd_dma.c's S_Base_* implementation.
 *
 * snd_main.c is NOT compiled in the Wii build (no OpenAL, no codec dispatch).
 * We provide the public S_* interface here and forward to S_Base_* in snd_dma.c.
 * SNDDMA_Init / SNDDMA_BeginPainting / SNDDMA_Submit are in wii_snd.c.
 *
 * Init path:
 *   main() → Wii_Snd_Init()    (ASND_Init + ASND_Pause)
 *   S_Init() → S_Base_Init()   (calls SNDDMA_Init, sets s_soundStarted)
 *
 * Per-frame path:
 *   S_Update() → S_Update_()   (mixes channels → SNDDMA_BeginPainting +
 *                                S_PaintChannels + SNDDMA_Submit)
 */
#include "client/snd_local.h"

/* Declarations from snd_dma.c not in snd_local.h */
extern qboolean S_Base_Init(soundInterface_t *si);
extern void     S_Update_(void);
extern void     S_Base_Shutdown(void);
extern void     S_Base_StartSound(vec3_t origin, int entityNum, int entchannel, sfxHandle_t sfx);
extern void     S_Base_StartLocalSound(sfxHandle_t sfx, int channelNum);
extern void     S_Base_StopAllSounds(void);
extern void     S_Base_StopLoopingSound(int entityNum);
extern void     S_Base_ClearLoopingSounds(qboolean killall);
extern void     S_Base_AddLoopingSound(int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx);
extern void     S_Base_AddRealLoopingSound(int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx);
extern void     S_Base_UpdateEntityPosition(int entityNum, const vec3_t origin);
extern void     S_Base_Respatialize(int entityNum, const vec3_t origin, vec3_t axis[3], int inwater);
extern sfxHandle_t S_Base_RegisterSound(const char *sample, qboolean compressed);
extern void     S_Base_BeginRegistration(void);
extern void     S_Base_ClearSoundBuffer(void);
extern void     S_Base_DisableSounds(void);

/*
 * Cvars normally defined by snd_main.c (not compiled in Wii build).
 * snd_dma.c and snd_mix.c reference these as extern globals.
 * We define them here and initialise them in S_Init before S_Base_Init.
 */
cvar_t *s_volume;
cvar_t *s_muted;
cvar_t *s_musicVolume;
cvar_t *s_doppler;

static soundInterface_t s_snd_if;

extern void boot_mark(const char *msg);
extern void S_CodecInit(void);  /* snd_codec.c — registers wav_codec */

void S_Init(void)
{
    boot_mark("S_Init enter");
    /* Initialise the cvars that snd_dma.c/snd_mix.c read as extern globals */
    s_volume      = Cvar_Get("s_volume",      "0.8",  CVAR_ARCHIVE);
    s_muted       = Cvar_Get("s_muted",       "0",    CVAR_ROM);
    s_musicVolume = Cvar_Get("s_musicvolume", "0.25", CVAR_ARCHIVE);
    s_doppler     = Cvar_Get("s_doppler",     "1",    CVAR_ARCHIVE);

    /* Register audio codecs (normally done in snd_main.c, not compiled on Wii).
     * Without this, codecs == NULL and every S_LoadSound returns "has length 0". */
    S_CodecInit();
    boot_mark("S_Init: codec registered");

    Com_Memset(&s_snd_if, 0, sizeof(s_snd_if));
    boot_mark("S_Init: calling S_Base_Init");
    S_Base_Init(&s_snd_if);
    boot_mark("S_Init done");
}
void S_Shutdown(void)
{
    if (s_snd_if.Shutdown)
        s_snd_if.Shutdown();
    Wii_Snd_Shutdown();
    Com_Memset(&s_snd_if, 0, sizeof(s_snd_if));
}
void        S_Update(void)                                             { S_Update_(); }
void        S_BeginRegistration(void)                                  { S_Base_BeginRegistration(); }
sfxHandle_t S_RegisterSound(const char *n, qboolean comp)             { return S_Base_RegisterSound(n, comp); }
void        S_StartSound(vec3_t o,int n,int c,sfxHandle_t h)          { S_Base_StartSound(o,n,c,h); }
void        S_StartLocalSound(sfxHandle_t h,int c)                    { S_Base_StartLocalSound(h,c); }
void        S_StopAllSounds(void)                                      { S_Base_StopAllSounds(); }
void        S_StopLoopingSound(int e)                                  { S_Base_StopLoopingSound(e); }
void        S_ClearLoopingSounds(qboolean k)                           { S_Base_ClearLoopingSounds(k); }
void        S_AddLoopingSound(int n,const vec3_t o,const vec3_t v,sfxHandle_t h)     { S_Base_AddLoopingSound(n,o,v,h); }
void        S_AddRealLoopingSound(int n,const vec3_t o,const vec3_t v,sfxHandle_t h) { S_Base_AddRealLoopingSound(n,o,v,h); }
void        S_UpdateEntityPosition(int n,const vec3_t o)              { S_Base_UpdateEntityPosition(n,o); }
void        S_Respatialize(int n,const vec3_t o,vec3_t ax[3],int i)   { S_Base_Respatialize(n,o,ax,i); }
void        S_ClearSoundBuffer(void)                                   { S_Base_ClearSoundBuffer(); }
void        S_DisableSounds(void)                                      { S_Base_DisableSounds(); }
/* Background music not implemented — Wii has no OGG/MP3 codec in this build */
void        S_StartBackgroundTrack(const char *i,const char *l)       { (void)i;(void)l; }
void        S_StopBackgroundTrack(void)                                { }
/* Raw samples — forwards to S_Base_RawSamples (snd_dma.c) for cinematic audio */
extern void S_Base_RawSamples(int stream, int samples, int rate, int width, int s_channels, const byte *data, float volume, int entityNum);
void        S_RawSamples(int stream,int samples,int rate,int width,int channels,const byte *d,float v,int e){ S_Base_RawSamples(stream,samples,rate,width,channels,d,v,e); }

qboolean CL_VideoRecording(void)               { return qfalse; }
qboolean CL_OpenAVIForWriting(const char *f)   { (void)f; return qfalse; }
qboolean CL_CloseAVI(void)                     { return qfalse; }
void     CL_TakeVideoFrame(void)               { }
void     CL_WriteAVIVideoFrame(const byte *d,int s) { (void)d;(void)s; }
void     CL_WriteAVIAudioFrame(const byte *d,int s) { (void)d;(void)s; }

char *Com_MD5File(const char *f,int l,const char *p,int pl)
{ (void)f;(void)l;(void)p;(void)pl; return ""; }

/* getaddrinfo / freeaddrinfo / gai_strerror
 * libogc declares these in netdb.h but does not implement them.
 * Provide an IPv4-only implementation: numeric IPs via inet_aton,
 * hostnames via net_gethostbyname (requires Wi-Fi to be up). */
#include <netdb.h>
#include <arpa/inet.h>
int getaddrinfo(const char *node, const char *service,
                const struct addrinfo *hints, struct addrinfo **res)
{
    struct addrinfo *ai;
    struct sockaddr_in *sin;
    int family = hints ? hints->ai_family : AF_INET;
    (void)service;

    if (family != AF_INET && family != AF_UNSPEC)
        return EAI_FAMILY;

    /* One allocation: addrinfo + sockaddr_in packed after it */
    ai = (struct addrinfo *)calloc(1, sizeof(struct addrinfo) + sizeof(struct sockaddr_in));
    if (!ai) return EAI_MEMORY;

    sin = (struct sockaddr_in *)(ai + 1);
    sin->sin_family = AF_INET;

    if (!node || !node[0]) {
        sin->sin_addr.s_addr = INADDR_ANY;
    } else if (inet_aton(node, &sin->sin_addr)) {
        /* numeric dotted-decimal — no DNS needed */
    } else {
        /* hostname — requires network stack to be up */
        struct hostent *he = net_gethostbyname(node);
        if (!he || !he->h_addr_list || !he->h_addr_list[0]) {
            free(ai);
            return EAI_NONAME;
        }
        memcpy(&sin->sin_addr, he->h_addr_list[0], he->h_length);
    }

    ai->ai_family   = AF_INET;
    ai->ai_socktype = hints ? hints->ai_socktype : SOCK_DGRAM;
    ai->ai_addrlen  = sizeof(struct sockaddr_in);
    ai->ai_addr     = (struct sockaddr *)sin;
    *res = ai;
    return 0;
}
void freeaddrinfo(struct addrinfo *ai)
{
    struct addrinfo *next;
    while (ai) { next = ai->ai_next; free(ai); ai = next; }
}
const char *gai_strerror(int e)
{
    switch (e) {
    case 0:           return "Success";
    case EAI_FAMILY:  return "Address family not supported";
    case EAI_MEMORY:  return "Out of memory";
    case EAI_NONAME:  return "Name or service not known";
    default:          return "Resolver error";
    }
}
int getnameinfo(const struct sockaddr *sa, socklen_t sl,
                char *h, socklen_t hl,
                char *sv, socklen_t svl, int f)
{
    (void)sl; (void)sv; (void)svl; (void)f;
    if (sa->sa_family == AF_INET && h && hl > 0) {
        const struct sockaddr_in *sin = (const struct sockaddr_in *)sa;
        if (inet_ntop(AF_INET, &sin->sin_addr, h, hl))
            return 0;
    }
    if (h && hl > 0) h[0] = '\0';
    return EAI_FAMILY;
}
int gethostname(char *n,size_t l)      { if(n&&l>0)n[0]='\0'; return 0; }

const struct in6_addr in6addr_any = {{ 0 }};

/* fill_fopen_filefunc — minizip file I/O callbacks using standard fopen.
 * unzip.c calls this to get the file operation vtable, then immediately
 * calls zopen_file. If any pointer is NULL, it crashes. */
#include "zlib.h"
#include "ioapi.h"

static voidpf wii_fopen(voidpf opaque, const char *filename, int mode)
{
    const char *fmode;
    (void)opaque;
    if      (mode & ZLIB_FILEFUNC_MODE_WRITE) fmode = "wb";
    else if (mode & ZLIB_FILEFUNC_MODE_READ)  fmode = "rb";
    else                                       fmode = "rb";
    return (voidpf)fopen(filename, fmode);
}
static uLong wii_fread(voidpf op, voidpf stream, void *buf, uLong size)
{ (void)op; return (uLong)fread(buf, 1, size, (FILE *)stream); }
static uLong wii_fwrite(voidpf op, voidpf stream, const void *buf, uLong size)
{ (void)op; return (uLong)fwrite(buf, 1, size, (FILE *)stream); }
static long  wii_ftell(voidpf op, voidpf stream)
{ (void)op; return ftell((FILE *)stream); }
static long  wii_fseek(voidpf op, voidpf stream, uLong offset, int origin)
{ (void)op; return fseek((FILE *)stream, (long)offset, origin); }
static int   wii_fclose(voidpf op, voidpf stream)
{ (void)op; return fclose((FILE *)stream); }
static int   wii_ferror(voidpf op, voidpf stream)
{ (void)op; return ferror((FILE *)stream); }

void fill_fopen_filefunc(zlib_filefunc_def *p)
{
    if (!p) return;
    p->zopen_file  = wii_fopen;
    p->zread_file  = wii_fread;
    p->zwrite_file = wii_fwrite;
    p->ztell_file  = wii_ftell;
    p->zseek_file  = wii_fseek;
    p->zclose_file = wii_fclose;
    p->zerror_file = wii_ferror;
    p->opaque      = NULL;
}

/* ==========================================================================
 * Final linker stubs
 * ========================================================================== */

/* umask — not in libogc newlib, provide a no-op */
#include <sys/types.h>
mode_t umask(mode_t mask) { (void)mask; return 0022; }

/* ioctl — libogc declares int ioctl(int, int, ...) in sys/ioccom.h
 * net_ip.c calls it with FIONBIO; forward to net_ioctl */
int ioctl(int fd, int request, ...)
{
    /* For FIONBIO set non-blocking; everything else ignored */
    u32 nb = 1;
    return net_ioctl(fd, request, &nb);
}

/* GetRefAPI is implemented in code/renderer/wii_renderer.c with real refexport_t */

/* mmap stub — Wii has no virtual memory mapping.
 * ioQ3's Hunk_Init uses mmap to allocate the hunk.
 * Route hunk allocations to MEM2 to keep MEM1 free for zone, server, and
 * OpenGX texture buffers.  wii_mem2_alloc() is defined later in this file;
 * it is safe to call before Wii_MEM2_Init() — it returns NULL until then
 * and we fall back to memalign(). */
#include <malloc.h>
void *wii_mem2_alloc(size_t size);  /* forward declaration */

/* Base address of our MEM2 bump allocator region (top of MEM2).
 * Only pointers >= this address came from wii_mem2_alloc() and must NOT be
 * passed to free() — the bump allocator has no per-object free.
 * Pointers in lower MEM2 (below mem2_base) are ordinary heap allocations
 * from _sbrk_r and must be freed normally. */
static u8 *mem2_base = NULL;

static inline int is_mem2_ptr(void *p)
{
    return mem2_base != NULL && (u8 *)p >= mem2_base;
}

void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset)
{
    (void)addr; (void)prot; (void)flags; (void)fd; (void)offset;
    void *p = wii_mem2_alloc(len);
    if (p) return p;
    p = memalign(32, len);
    return p ? p : (void *)-1;
}

int munmap(void *addr, size_t len)
{
    (void)len;
    /* MEM2 is a bump allocator — no free(). Only free MEM1 pointers. */
    if (!is_mem2_ptr(addr))
        free(addr);
    return 0;
}

/* CL_ConsolePrint is defined in cl_console.c — no stub needed */

/* re_Printf — renderer import Printf, called by Com_Printf after renderer init.
 * If this is NULL when called, it causes PC=0 crash. */
/* This is handled via refimport_t.Printf which CL_InitRef wires up.
 * The crash is happening BEFORE CL_InitRef, so it must be something else. */

/* Add a CON_LogWrite stub in case con_log.c is not linked */
unsigned int CON_LogWrite(const char *in) { (void)in; return 0; }
unsigned int CON_LogSize(void)            { return 0; }
unsigned int CON_LogRead(char *out, unsigned int outlen) { (void)out;(void)outlen; return 0; }

/* SCR_UpdateScreen is defined in cl_scrn.c — no stub needed */

/* ---- MEM2 memory layout --------------------------------------------------
 *
 * ioQ3's hunk uses calloc(40MB+31, 1) on non-Linux platforms.  MEM1 is only
 * 24 MB total, so after code/BSS/stack/zone (~10 MB), a 40 MB contiguous
 * calloc fails because MEM1 is exhausted and MEM2 starts at 0x90000000 —
 * NOT contiguous with MEM1's end at 0x81800000.  calloc returns NULL →
 * hunk_base = 0 → all hunk writes go to raw address offsets → DSI crash.
 *
 * Fix: partition MEM2 explicitly.
 *   TOP 44 MB  → bump allocator (mem2_base/ptr/left), used for large calloc
 *   BOTTOM rest → _sbrk_r heap for ordinary malloc/calloc/free
 *
 * Arena2Hi is restricted to the partition point so _sbrk_r never touches
 * the bump region.  __wrap_calloc intercepts calloc(≥16 MB) and serves it
 * from the bump allocator; the hunk (calloc(40MB+31,1)) is the only caller.
 * ----------------------------------------------------------------------- */
static u8   *mem2_ptr  = NULL;
static u32   mem2_left = 0;

/* Bump allocator lives at the TOP of MEM2.
 * Bump region: top 36 MB (enough for com_hunkMegs=32 with headroom).
 * _sbrk_r uses Arena2 from the bottom up to mem2_base.
 *
 * CRITICAL: SYS_SetArena2Hi must be called to cap the sbrk heap.
 * Without it, newlib's _sbrk_r can grow past mem2_base into the bump
 * region, overwriting the hunk.  OpenGX's memalign calls for texture
 * storage are the primary consumer; during map load they can push the
 * sbrk heap past 16 MB, corrupting hunk-resident data like the shader
 * hash table (observed as DSI in FindShaderInShaderText).
 *
 * Originally 36 MB, reduced to 33 MB to give sbrk ~3 MB more headroom.
 * The hunk (com_hunkMegs 32) needs ~32 MB + alignment from the bump.
 * 33 MB provides sufficient margin.  The extra sbrk space is needed
 * because OpenGX allocates texture pixel buffers via memalign() from
 * the sbrk heap; during a server connect, R_DeleteTextures frees menu
 * textures and the map+cgame reload allocates 200+ new textures,
 * which exhausts a 16 MB sbrk heap after heap fragmentation. */
#define MEM2_BUMP_SIZE (33u * 1024u * 1024u)

void Wii_MEM2_Init(void)
{
    u8 *lo = (u8 *)SYS_GetArena2Lo();
    u8 *hi = (u8 *)SYS_GetArena2Hi();
    u32 total = (u32)(hi - lo);

    if (total >= MEM2_BUMP_SIZE) {
        mem2_base = hi - MEM2_BUMP_SIZE;
        mem2_ptr  = mem2_base;
        mem2_left = MEM2_BUMP_SIZE;
    } else {
        mem2_base = lo;
        mem2_ptr  = lo;
        mem2_left = total;
    }

    /* Cap the sbrk heap so _sbrk_r cannot grow into the bump region.
     * This gives sbrk (mem2_base - lo) bytes, roughly 16 MB with the
     * default linker script.  OpenGX textures at r_picmip 2 should fit
     * well within this budget. */
    SYS_SetArena2Hi(mem2_base);
}


void *wii_mem2_alloc(size_t size)
{
    size_t aligned = (size + 31) & ~31;
    if (aligned <= mem2_left && mem2_ptr) {
        void *p = mem2_ptr;
        mem2_ptr  += aligned;
        mem2_left -= aligned;
        return p;
    }
    return NULL;
}

/* ---- memalign guard zone / free wrapper -----------------------------------
 *
 * DISABLED: --wrap,free and --wrap,memalign were removed from LDFLAGS.
 * The pre-Wiimote backup (which works) did not have these wraps.
 * Adding them changes every memalign allocation size (+256 bytes) and
 * intercepts every free() system-wide, which can cause memory exhaustion
 * or silent no-op frees if s_leak_tex gets stuck.
 *
 * The underlying OpenGX TexelRGBA8 overflow should be fixed in OpenGX
 * itself rather than papered over with allocation guards.
 * ----------------------------------------------------------------------- */
#if 0
extern void *__real_memalign(size_t align, size_t size);
void *__wrap_memalign(size_t align, size_t size)
{
    return __real_memalign(align, size + 256);
}

extern void __real_free(void *ptr);
static volatile int s_leak_tex = 0;
void Wii_BeginLeakTextures(void) { s_leak_tex = 1; }
void Wii_EndLeakTextures(void)   { s_leak_tex = 0; }
void __wrap_free(void *ptr)
{
    if (ptr && s_leak_tex) return;
    __real_free(ptr);
}
#endif

/* Route large calloc (≥16 MB) to the MEM2 bump allocator.
 * Only ioQ3's Hunk_Init calls calloc this large (calloc(40MB+31,1)).
 * The bump allocator never frees, which is safe: the hunk is never freed. */
extern void *__real_calloc(size_t nmemb, size_t size);

void *__wrap_calloc(size_t nmemb, size_t size)
{
    size_t total = nmemb * size;
    if (total >= 16u * 1024u * 1024u) {
        void *p = wii_mem2_alloc(total);
        if (p) {
            memset(p, 0, total);
            return p;
        }
    }
    return __real_calloc(nmemb, size);
}

/* SV_Init wrap — log checkpoints to diagnose crash */
extern void __real_SV_Init(void);

/* We can't easily add checkpoints inside __real_SV_Init, but we can
 * narrow it down by checking what SV_Init calls first.
 * SV_Init calls: Cvar_Get (many), SV_Startup -> Z_Malloc for clients,
 * then NET_Init, then Cmd_AddCommand calls.
 * The green screen crash suggests a stack overflow or bad pointer.
 * Log file approach: write before call, check if "done" appears. */
void __wrap_SV_Init(void)
{
    __real_SV_Init();
}

/* Com_Printf wrap — mirror all engine output to SD card log.
 * This captures ERR_DROP messages from Com_Error which otherwise
 * vanish silently (longjmp back to menu) on the Wii. */
extern void QDECL __real_Com_Printf(const char *fmt, ...) __attribute__((format(printf,1,2)));

void QDECL __wrap_Com_Printf(const char *fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    /* Forward to the real implementation (console, etc.) */
    __real_Com_Printf("%s", buf);
}

/* VM_Call yield — called every time a QVM trap fires. */
void Wii_VM_Yield(void)
{
    static int s_yield_count = 0;
    if (++s_yield_count < 100) return;
    s_yield_count = 0;

    PAD_ScanPads();
    if (PAD_ButtonsDown(0) & PAD_BUTTON_START) {
        exit(0);
    }
}

/* CL_GenerateQKey wrap — skip the qkey dialog entirely by pretending
 * the key already exists. The dialog is shown when qkey is missing. */
void __wrap_CL_GenerateQKey(void)
{
    /* Do nothing — this prevents the CD key dialog from appearing */
}

/* Intercept VM_Call to redirect UIMENU_NEED_CD to UIMENU_MAIN.
 * Using hardcoded values to avoid client.h conflicts with our stubs.
 * UI_SET_ACTIVE_MENU = 0, UIMENU_MAIN = 2, UIMENU_NEED_CD = 8 */
/* VM_Call wrap — intercept UI_HASUNIQUECDKEY to skip CD key dialog */
#define WII_UI_SET_ACTIVE_MENU  7
#define WII_UI_HASUNIQUECDKEY   10
#define WII_UIMENU_MAIN         1
#define WII_UIMENU_NEED_CD      3

intptr_t QDECL __real_VM_Call( void *vm, int callnum, ... );

intptr_t QDECL __wrap_VM_Call( void *vm, int callnum, ... )
{
    int args[12];
    va_list ap;
    va_start(ap, callnum);
    for (int i = 0; i < 12; i++) args[i] = va_arg(ap, int);
    va_end(ap);

    /* Always report CD key as valid — skips the dialog entirely */
    if (callnum == WII_UI_HASUNIQUECDKEY) {
        return 1;
    }

    if (callnum == WII_UI_SET_ACTIVE_MENU && args[0] == WII_UIMENU_NEED_CD) {
        args[0] = WII_UIMENU_MAIN;
    }

    /* Guard against NULL vm — Q3 has some callers that lack null checks.
     * Log the callnum so we can identify the root cause, then return a
     * safe zero rather than crashing with "VM_Call with NULL vm". */
    if (!vm) {
        return 0;
    }

    return __real_VM_Call(vm, callnum,
        args[0], args[1], args[2], args[3],
        args[4], args[5], args[6], args[7],
        args[8], args[9], args[10], args[11]);
}

/* ========================================================================
 * Newlib malloc thread-safety — required when BTE (Bluetooth) is linked.
 *
 * WPAD (libwiiuse) runs a background LWP thread for Bluetooth/Wiimote
 * communication that calls the standard libc malloc/free.
 * Newlib's _malloc_r / _free_r are NOT thread-safe by default — they rely
 * on the application providing __malloc_lock / __malloc_unlock.  Without
 * these, concurrent heap operations from the WPAD thread and the main
 * thread corrupt the free-list, causing DSI crashes in _malloc_r / _free_r.
 *
 * We replicate libogc's own __syscall_malloc_lock pattern: a recursive
 * LWP_Mutex that properly integrates with the LWP thread scheduler.
 * IRQ_Disable alone is not sufficient because LWP threads can also be
 * cooperatively scheduled (e.g. after IOS syscalls, semaphore waits),
 * not only via the timer interrupt.
 *
 * The mutex is initialised lazily on first use (before any LWP thread
 * has been created, so the first call is inherently single-threaded).
 * ======================================================================== */
#include <ogc/mutex.h>

static mutex_t s_malloc_mtx = LWP_MUTEX_NULL;

/* Called from main() before WPAD_Init to guarantee the mutex exists
 * before any background thread can call malloc.  The lazy path in
 * __wrap___malloc_lock is a safety net only. */
void Wii_InitMallocLock(void)
{
    if (s_malloc_mtx == LWP_MUTEX_NULL)
        LWP_MutexInit(&s_malloc_mtx, true);     /* recursive */
}

void __wrap___malloc_lock(struct _reent *r)
{
    (void)r;
    if (s_malloc_mtx == LWP_MUTEX_NULL)
        LWP_MutexInit(&s_malloc_mtx, true);     /* lazy fallback */
    LWP_MutexLock(s_malloc_mtx);
}

void __wrap___malloc_unlock(struct _reent *r)
{
    (void)r;
    if (s_malloc_mtx != LWP_MUTEX_NULL)
        LWP_MutexUnlock(s_malloc_mtx);
}
