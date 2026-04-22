#!/bin/bash
# apply_patches.sh

IOQ3="$( cd .. && pwd )/ioq3"
QPLAT="$IOQ3/code/qcommon/q_platform.h"

if [ ! -f "$QPLAT" ]; then
    echo "ERROR: Cannot find $QPLAT"
    echo "Make sure ioq3 is cloned alongside ioquake3-wii/"
    exit 1
fi

# Don't apply twice
if grep -q "GEKKO" "$QPLAT"; then
    echo "Patch already applied to q_platform.h — skipping."
else
    echo "Patching $QPLAT ..."
    python3 - "$QPLAT" <<'PYEOF'
import sys, re

path = sys.argv[1]
with open(path, 'r') as f:
    src = f.read()

WII_BLOCK = r"""
//===========================================================================
// Nintendo Wii (Gekko/Broadway PowerPC) — ioquake3-wii port
//===========================================================================
#elif defined(GEKKO)

#define OS_STRING "wii"
#define OS_LITTLE_ENDIAN_UNDEF  // Wii is big-endian; do not set OS_LITTLE_ENDIAN
#undef  Q3_LITTLE_ENDIAN
#define Q3_BIG_ENDIAN

#define ARCH_STRING "ppc"
#define PATH_SEP    '/'
#define DLL_EXT     ".so"   // unused — no dlopen on Wii

#ifndef ID_INLINE
#  define ID_INLINE __inline__
#endif

// No pthread, no dlopen, no mmap on Wii
#define IOAPI_NO_64BIT

"""

# Insert before the closing #else ... #error "Operating system not supported"
# That else is reliably preceded by a blank line or a #endif comment block.
# We match the exact string that appears in ioq3 main.
marker = '#else\n\n\t#error "Operating system not supported"'
if marker not in src:
    # Try alternate form (tabs vs spaces)
    marker = '#else\n\n#error "Operating system not supported"'
if marker not in src:
    # Looser: just find the OS-not-supported error line
    lines = src.split('\n')
    for i, line in enumerate(lines):
        if '"Operating system not supported"' in line:
            # Back up to find the #else
            for j in range(i, max(i-5, 0), -1):
                if lines[j].strip() == '#else':
                    lines.insert(j, WII_BLOCK)
                    break
            break
    src = '\n'.join(lines)
else:
    src = src.replace(marker, WII_BLOCK + marker)

with open(path, 'w') as f:
    f.write(src)

print("q_platform.h patched successfully.")
PYEOF
fi

echo ""
echo "Patching qcommon.h to add #ifndef guards for Wii memory overrides..."
QCOMMON_H="$IOQ3/code/qcommon/qcommon.h"
if grep -q "ifndef PACKET_BACKUP" "$QCOMMON_H"; then
    echo "qcommon.h #ifndef guards already applied — skipping."
else
    # Wrap PACKET_BACKUP
    sed -i 's/^#define\tPACKET_BACKUP\t32/#ifndef PACKET_BACKUP\n#define\tPACKET_BACKUP\t32\n#endif/' "$QCOMMON_H"
    # Wrap PACKET_MASK
    sed -i 's/^#define\tPACKET_MASK\t\t(PACKET_BACKUP-1)/#ifndef PACKET_MASK\n#define\tPACKET_MASK\t\t(PACKET_BACKUP-1)\n#endif/' "$QCOMMON_H"
    # Wrap MAX_RELIABLE_COMMANDS
    sed -i 's/^#define\tMAX_RELIABLE_COMMANDS\t64/#ifndef MAX_RELIABLE_COMMANDS\n#define\tMAX_RELIABLE_COMMANDS\t64\n#endif/' "$QCOMMON_H"
    # Wrap MAX_DOWNLOAD_WINDOW
    sed -i 's/^#define MAX_DOWNLOAD_WINDOW\t\t48/#ifndef MAX_DOWNLOAD_WINDOW\n#define MAX_DOWNLOAD_WINDOW\t\t48\n#endif/' "$QCOMMON_H"
    echo "qcommon.h patched with #ifndef guards."
fi

echo ""
echo "Patching sv_init.c to add crash checkpoints..."
SV_INIT="$IOQ3/code/server/sv_init.c"
if grep -q "after BotInitCvars" "$SV_INIT"; then
    echo "sv_init.c checkpoints already patched — skipping."
else
    sed -i 's/SV_BotInitCvars();/{ FILE *_f=fopen("sd:\/quake3\/svinit.txt","a"); if(_f){fprintf(_f,"before BotInitCvars\\n");fclose(_f);} } SV_BotInitCvars(); { FILE *_f=fopen("sd:\/quake3\/svinit.txt","a"); if(_f){fprintf(_f,"after BotInitCvars\\n");fclose(_f);} }/' "$SV_INIT"
    sed -i 's/SV_BotInitBotLib();/{ FILE *_f=fopen("sd:\/quake3\/svinit.txt","a"); if(_f){fprintf(_f,"before BotInitBotLib\\n");fclose(_f);} } SV_BotInitBotLib(); { FILE *_f=fopen("sd:\/quake3\/svinit.txt","a"); if(_f){fprintf(_f,"after BotInitBotLib\\n");fclose(_f);} }/' "$SV_INIT"
    echo "sv_init.c checkpoints patched."
fi

echo ""
echo "Patching sv_init.c to move large stack array to BSS..."
SV_INIT="$IOQ3/code/server/sv_init.c"
if grep -q "static char.*systemInfo\[16384\]" "$SV_INIT"; then
    echo "sv_init.c already patched — skipping."
else
    sed -i 's/char\s*systemInfo\[16384\]/static char systemInfo[16384]/' "$SV_INIT"
    echo "sv_init.c patched."
fi

echo ""
echo "Patching tr_main.c to rename ri..."
TR_MAIN="$IOQ3/code/renderergl1/tr_main.c"
if grep -q "tr_main_ri_unused" "$TR_MAIN"; then
    echo "tr_main.c already patched — skipping."
else
    sed -i 's/^refimport_t\s*ri\s*;/refimport_t tr_main_ri_unused;/' "$TR_MAIN"
    echo "tr_main.c patched."
fi

echo ""
echo "Patching tr_init.c to rename GetRefAPI..."
TR_INIT="$IOQ3/code/renderergl1/tr_init.c"
if grep -q "tr_init_GetRefAPI_unused" "$TR_INIT"; then
    echo "tr_init.c already patched — skipping."
else
    sed -i 's/Q_EXPORT refexport_t\* QDECL GetRefAPI/Q_EXPORT refexport_t* QDECL tr_init_GetRefAPI_unused/g' "$TR_INIT"
    sed -i 's/refexport_t \*GetRefAPI/refexport_t *tr_init_GetRefAPI_unused/g' "$TR_INIT"
    echo "tr_init.c patched."
fi

echo ""
echo "Patching sv_game.c to log G_LOCATE_GAME_DATA call (crash diagnostic)..."
SV_GAME="$IOQ3/code/server/sv_game.c"
if grep -q "WII_LOCGAME_DEBUG" "$SV_GAME"; then
    echo "sv_game.c locate-game debug already patched — skipping."
else
    python3 - "$SV_GAME" <<'PYEOF'
import sys, re

path = sys.argv[1]
with open(path, 'r') as f:
    src = f.read()

# Find the G_LOCATE_GAME_DATA case and add logging after SV_LocateGameData call.
# Pattern: SV_LocateGameData( VMA(1), args[2], args[3], VMA(4), args[5] );
# Insert log + WII_LOCGAME_DEBUG guard after the call.
OLD = 'SV_LocateGameData( VMA(1), args[2], args[3], VMA(4), args[5] );'
NEW = (
    'SV_LocateGameData( VMA(1), args[2], args[3], VMA(4), args[5] );\n'
    '#define WII_LOCGAME_DEBUG 1\n'
    '\t\t\t{ FILE *_gf=fopen("sd:/quake3/gamedata.txt","w");\n'
    '\t\t\t  if(_gf){ fprintf(_gf,"gentities=%p num=%d size=%d\\n",\n'
    '\t\t\t    (void*)sv.gentities, sv.num_entities, sv.gentitySize);\n'
    '\t\t\t    fclose(_gf); } }'
)
if OLD in src:
    src = src.replace(OLD, NEW, 1)
    with open(path, 'w') as f:
        f.write(src)
    print("sv_game.c patched with G_LOCATE_GAME_DATA logging.")
else:
    print("WARNING: could not find SV_LocateGameData call in sv_game.c — skipping.")
PYEOF
fi

echo ""
echo "Patching sv_init.c to log sv.gameEntities before SV_CreateBaseline..."
SV_INIT_CB="$IOQ3/code/server/sv_init.c"
if grep -q "WII_BASELINE_DEBUG" "$SV_INIT_CB"; then
    echo "sv_init.c baseline debug already patched — skipping."
else
    python3 - "$SV_INIT_CB" <<'PYEOF'
import sys

path = sys.argv[1]
with open(path, 'r') as f:
    src = f.read()

OLD = 'SV_CreateBaseline();'
NEW = (
    '{ FILE *_bf=fopen("sd:/quake3/gamedata.txt","a");\n'
    '\t  if(_bf){ fprintf(_bf,"pre-baseline: gentities=%p num=%d size=%d clients=%p\\n",\n'
    '\t    (void*)sv.gentities, sv.num_entities, sv.gentitySize, (void*)sv.gameClients);\n'
    '\t    fclose(_bf); } }\n'
    '#define WII_BASELINE_DEBUG 1\n'
    '\tSV_CreateBaseline();'
)
if OLD in src:
    src = src.replace(OLD, NEW, 1)
    with open(path, 'w') as f:
        f.write(src)
    print("sv_init.c patched with pre-SV_CreateBaseline logging.")
else:
    print("WARNING: could not find SV_CreateBaseline() call in sv_init.c — skipping.")
PYEOF
fi

echo ""
echo "Patching snd_local.h to add #ifndef guards for MAX_RAW_STREAMS and MAX_RAW_SAMPLES..."
SND_LOCAL="$IOQ3/code/client/snd_local.h"
if grep -q "ifndef MAX_RAW_STREAMS" "$SND_LOCAL"; then
    echo "snd_local.h #ifndef guards already applied — skipping."
else
    sed -i 's/^#define\tMAX_RAW_SAMPLES\t16384/#ifndef MAX_RAW_SAMPLES\n#define\tMAX_RAW_SAMPLES\t16384\n#endif/' "$SND_LOCAL"
    sed -i 's/^#define MAX_RAW_STREAMS.*$/#ifndef MAX_RAW_STREAMS\n#define MAX_RAW_STREAMS (MAX_CLIENTS * 2 + 1)\n#endif/' "$SND_LOCAL"
    echo "snd_local.h patched with #ifndef guards."
fi

echo ""
echo "Patching snd_codec.c to use #if instead of #ifdef for codec guards..."
SND_CODEC="$IOQ3/code/client/snd_codec.c"
if grep -q "#if USE_CODEC_VORBIS" "$SND_CODEC"; then
    echo "snd_codec.c already patched — skipping."
else
    sed -i 's/#ifdef USE_CODEC_VORBIS/#if USE_CODEC_VORBIS/' "$SND_CODEC"
    sed -i 's/#ifdef USE_CODEC_OPUS/#if USE_CODEC_OPUS/' "$SND_CODEC"
    echo "snd_codec.c patched (#ifdef → #if so =0 disables correctly)."
fi

echo ""
echo "Done! Now rebuild:"
echo "  make dol"
echo ""
echo "You can also revert with git:"
echo "  cd ../ioq3 && git checkout code/qcommon/q_platform.h"
