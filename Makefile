#---------------------------------------------------------------------------------
# ioquake3-wii Makefile
# Requires devkitPPC + libogc (install via devkitPro pacman)
#   pacman -S wii-dev
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITPRO)),)
  $(error "Set DEVKITPRO in your environment. export DEVKITPRO=/opt/devkitpro")
endif
ifeq ($(strip $(DEVKITPPC)),)
  $(error "Set DEVKITPPC in your environment. export DEVKITPPC=/opt/devkitpro/devkitPPC")
endif

include $(DEVKITPPC)/wii_rules

#---------------------------------------------------------------------------------
# Input backend: wiimote (default) or gamecube
#   make                        → Wiimote + Nunchuk
#   make INPUT_BACKEND=gamecube → GameCube controller
#---------------------------------------------------------------------------------
INPUT_BACKEND ?= wiimote

#---------------------------------------------------------------------------------
# Game mode: baseq3 (default) or baseoa (OpenArena)
#   make                     → Quake III Arena (reads sd:/quake3/baseq3/)
#   make GAMEMODE=baseoa     → OpenArena       (reads sd:/quake3/baseoa/)
#---------------------------------------------------------------------------------
GAMEMODE ?= baseq3

#---------------------------------------------------------------------------------
# Project identity
#---------------------------------------------------------------------------------
TARGET      := ioquake3_wii
BUILD       := build
SOURCES     := code \
               code/renderer \
               code/audio \
               code/sys
PORTDIR     := $(CURDIR)

ifeq ($(INPUT_BACKEND),gamecube)
  WII_INPUT_SRC := code/input/wii_input_gc.c
else
  WII_INPUT_SRC := code/input/wii_input.c
endif
INCLUDES    := code

#---------------------------------------------------------------------------------
# OpenGX — OpenGL 1.x over GX (devkitPro portlib: pacman -S wii-opengx)
# Header at $(DEVKITPRO)/portlibs/wii/include/GL/gl.h
# Library  at $(DEVKITPRO)/portlibs/wii/lib/libopengx.a
#---------------------------------------------------------------------------------
OPENGX_INC  := $(DEVKITPRO)/portlibs/wii/include
OPENGX_LIB  := $(DEVKITPRO)/portlibs/wii/lib
OPENGX_SRC  := ../opengx/src

#---------------------------------------------------------------------------------
# ioQuake3 source directories
#---------------------------------------------------------------------------------
IOQ3_DIR    := ../ioq3

IOQ3_SRCS   := \
  $(IOQ3_DIR)/code/qcommon/cmd.c \
  $(IOQ3_DIR)/code/qcommon/cm_load.c \
  $(IOQ3_DIR)/code/qcommon/cm_patch.c \
  $(IOQ3_DIR)/code/qcommon/cm_polylib.c \
  $(IOQ3_DIR)/code/qcommon/cm_test.c \
  $(IOQ3_DIR)/code/qcommon/cm_trace.c \
  $(IOQ3_DIR)/code/qcommon/common.c \
  $(IOQ3_DIR)/code/qcommon/cvar.c \
  $(IOQ3_DIR)/code/qcommon/files.c \
  $(IOQ3_DIR)/code/qcommon/huffman.c \
  $(IOQ3_DIR)/code/qcommon/md4.c \
  $(IOQ3_DIR)/code/qcommon/msg.c \
  $(IOQ3_DIR)/code/qcommon/net_chan.c \
  $(IOQ3_DIR)/code/qcommon/net_ip.c \
  $(IOQ3_DIR)/code/qcommon/q_math.c \
  $(IOQ3_DIR)/code/qcommon/q_shared.c \
  $(IOQ3_DIR)/code/qcommon/unzip.c \
  $(IOQ3_DIR)/code/qcommon/vm.c \
  $(IOQ3_DIR)/code/qcommon/vm_interpreted.c \
  $(IOQ3_DIR)/code/qcommon/vm_none.c \
  $(IOQ3_DIR)/code/client/cl_cgame.c \
  $(IOQ3_DIR)/code/client/cl_cin.c \
  $(IOQ3_DIR)/code/client/cl_console.c \
  $(IOQ3_DIR)/code/client/cl_input.c \
  $(IOQ3_DIR)/code/client/cl_keys.c \
  $(IOQ3_DIR)/code/client/cl_main.c \
  $(IOQ3_DIR)/code/client/cl_net_chan.c \
  $(IOQ3_DIR)/code/client/cl_parse.c \
  $(IOQ3_DIR)/code/client/cl_scrn.c \
  $(IOQ3_DIR)/code/client/cl_ui.c \
  $(IOQ3_DIR)/code/client/snd_dma.c \
  $(IOQ3_DIR)/code/client/snd_mem.c \
  $(IOQ3_DIR)/code/client/snd_mix.c \
  $(IOQ3_DIR)/code/client/snd_codec.c \
  $(IOQ3_DIR)/code/client/snd_codec_wav.c \
  $(IOQ3_DIR)/code/client/snd_adpcm.c \
  $(IOQ3_DIR)/code/client/snd_wavelet.c \
  $(IOQ3_DIR)/code/server/sv_bot.c \
  $(IOQ3_DIR)/code/server/sv_ccmds.c \
  $(IOQ3_DIR)/code/server/sv_client.c \
  $(IOQ3_DIR)/code/server/sv_game.c \
  $(IOQ3_DIR)/code/server/sv_init.c \
  $(IOQ3_DIR)/code/server/sv_main.c \
  $(IOQ3_DIR)/code/server/sv_net_chan.c \
  $(IOQ3_DIR)/code/server/sv_snapshot.c \
  $(IOQ3_DIR)/code/server/sv_world.c \
  $(IOQ3_DIR)/code/botlib/be_aas_bspq3.c \
  $(IOQ3_DIR)/code/botlib/be_aas_cluster.c \
  $(IOQ3_DIR)/code/botlib/be_aas_debug.c \
  $(IOQ3_DIR)/code/botlib/be_aas_entity.c \
  $(IOQ3_DIR)/code/botlib/be_aas_file.c \
  $(IOQ3_DIR)/code/botlib/be_aas_main.c \
  $(IOQ3_DIR)/code/botlib/be_aas_move.c \
  $(IOQ3_DIR)/code/botlib/be_aas_optimize.c \
  $(IOQ3_DIR)/code/botlib/be_aas_reach.c \
  $(IOQ3_DIR)/code/botlib/be_aas_route.c \
  $(IOQ3_DIR)/code/botlib/be_aas_routealt.c \
  $(IOQ3_DIR)/code/botlib/be_aas_sample.c \
  $(IOQ3_DIR)/code/botlib/be_ai_char.c \
  $(IOQ3_DIR)/code/botlib/be_ai_chat.c \
  $(IOQ3_DIR)/code/botlib/be_ai_gen.c \
  $(IOQ3_DIR)/code/botlib/be_ai_goal.c \
  $(IOQ3_DIR)/code/botlib/be_ai_move.c \
  $(IOQ3_DIR)/code/botlib/be_ai_weap.c \
  $(IOQ3_DIR)/code/botlib/be_ai_weight.c \
  $(IOQ3_DIR)/code/botlib/be_ea.c \
  $(IOQ3_DIR)/code/botlib/be_interface.c \
  $(IOQ3_DIR)/code/botlib/l_crc.c \
  $(IOQ3_DIR)/code/botlib/l_libvar.c \
  $(IOQ3_DIR)/code/botlib/l_log.c \
  $(IOQ3_DIR)/code/botlib/l_memory.c \
  $(IOQ3_DIR)/code/botlib/l_precomp.c \
  $(IOQ3_DIR)/code/botlib/l_script.c \
  $(IOQ3_DIR)/code/botlib/l_struct.c \
  $(IOQ3_DIR)/code/renderergl1/tr_animation.c \
  $(IOQ3_DIR)/code/renderergl1/tr_bsp.c \
  $(IOQ3_DIR)/code/renderergl1/tr_curve.c \
  $(IOQ3_DIR)/code/renderergl1/tr_init.c \
  $(IOQ3_DIR)/code/renderergl1/tr_light.c \
  $(IOQ3_DIR)/code/renderergl1/tr_main.c \
  $(IOQ3_DIR)/code/renderergl1/tr_marks.c \
  $(IOQ3_DIR)/code/renderergl1/tr_mesh.c \
  $(IOQ3_DIR)/code/renderergl1/tr_model.c \
  $(IOQ3_DIR)/code/renderergl1/tr_model_iqm.c \
  $(IOQ3_DIR)/code/renderergl1/tr_scene.c \
  $(IOQ3_DIR)/code/renderergl1/tr_shade_calc.c \
  $(IOQ3_DIR)/code/renderergl1/tr_shader.c \
  $(IOQ3_DIR)/code/renderergl1/tr_backend.c \
  $(IOQ3_DIR)/code/renderergl1/tr_cmds.c \
  $(IOQ3_DIR)/code/renderergl1/tr_flares.c \
  $(IOQ3_DIR)/code/renderergl1/tr_image.c \
  $(IOQ3_DIR)/code/renderergl1/tr_shade.c \
  $(IOQ3_DIR)/code/renderergl1/tr_shadows.c \
  $(IOQ3_DIR)/code/renderergl1/tr_sky.c \
  $(IOQ3_DIR)/code/renderergl1/tr_surface.c \
  $(IOQ3_DIR)/code/renderergl1/tr_world.c \
  $(IOQ3_DIR)/code/renderercommon/puff.c \
  $(IOQ3_DIR)/code/renderercommon/tr_font.c \
  $(IOQ3_DIR)/code/renderercommon/tr_image_bmp.c \
  $(IOQ3_DIR)/code/renderercommon/tr_image_jpg.c \
  $(IOQ3_DIR)/code/renderercommon/tr_image_pcx.c \
  $(IOQ3_DIR)/code/renderercommon/tr_image_png.c \
  $(IOQ3_DIR)/code/renderercommon/tr_image_pvr.c \
  $(IOQ3_DIR)/code/renderercommon/tr_image_tga.c \
  $(IOQ3_DIR)/code/renderercommon/tr_noise.c

#---------------------------------------------------------------------------------
# zlib: auto-detect ioQ3 internal, else use devkitPro portlibs.
#
# Internal zlib may live at:
#   code/libs/zlib/       (older ioq3)
#   code/zlib/            (some forks)
#
# devkitPro portlibs fallback:
#   pacman -S ppc-zlib
#---------------------------------------------------------------------------------
IOQ3_ZLIB_A := $(IOQ3_DIR)/code/libs/zlib/zlib.h
IOQ3_ZLIB_B := $(IOQ3_DIR)/code/zlib/zlib.h

ifneq ($(wildcard $(IOQ3_ZLIB_A)),)
  ZLIB_DIR      := $(IOQ3_DIR)/code/libs/zlib
  ZLIB_CFLAGS   := -DUSE_INTERNAL_ZLIB -I$(ZLIB_DIR) \
                   -DZLIB_H_PATH=\"$(ZLIB_DIR)/zlib.h\"
  IOQ3_ZLIB_SRCS := $(wildcard $(ZLIB_DIR)/*.c)
  ZLIB_LIBS     :=
else ifneq ($(wildcard $(IOQ3_ZLIB_B)),)
  ZLIB_DIR      := $(IOQ3_DIR)/code/zlib
  ZLIB_CFLAGS   := -DUSE_INTERNAL_ZLIB -I$(ZLIB_DIR) \
                   -DZLIB_H_PATH=\"$(ZLIB_DIR)/zlib.h\"
  IOQ3_ZLIB_SRCS := $(wildcard $(ZLIB_DIR)/*.c)
  ZLIB_LIBS     :=
else
  # No internal zlib found — use devkitPro portlibs (pacman -S ppc-zlib)
  # Search common portlibs locations across devkitPro versions
  PORTLIBS_WII  := $(DEVKITPRO)/portlibs/wii
  PORTLIBS_PPC  := $(DEVKITPRO)/portlibs/ppc
  ifneq ($(wildcard $(PORTLIBS_WII)/include/zlib.h),)
    PORTLIBS    := $(PORTLIBS_WII)
  else ifneq ($(wildcard $(PORTLIBS_PPC)/include/zlib.h),)
    PORTLIBS    := $(PORTLIBS_PPC)
  else
    $(error zlib.h not found. Run: pacman -S ppc-zlib)
  endif
  ZLIB_DIR      := $(PORTLIBS)/include
  ZLIB_CFLAGS   := -I$(PORTLIBS)/include
  IOQ3_ZLIB_SRCS :=
  ZLIB_LIBS     := -L$(PORTLIBS)/lib -lz
  $(info >>> Using devkitPro portlibs zlib: $(PORTLIBS))
endif

# Copy zlib.h (and zconf.h if present) next to unzip.h so that
# GCC resolves the quoted #include "zlib.h" before checking -I paths.
ZLIB_H_COPY  := $(IOQ3_DIR)/code/qcommon/zlib.h
ZCONF_H_COPY := $(IOQ3_DIR)/code/qcommon/zconf.h

#---------------------------------------------------------------------------------
# Compiler flags
#   make WII_DEBUG=1 → enables SD card logging (boot.txt, crash.txt, comlog.txt)
#---------------------------------------------------------------------------------
ifeq ($(WII_DEBUG),1)
  WII_DEBUG_FLAG := -DWII_DEBUG
else
  WII_DEBUG_FLAG :=
endif

ifeq ($(GAMEMODE),baseq3)
  GAMEMODE_FLAGS := -DWII_BASEGAME=\"baseq3\" -DWII_STANDALONE=0
else
  GAMEMODE_FLAGS := -DWII_BASEGAME=\"$(GAMEMODE)\" -DWII_STANDALONE=1
endif

CFLAGS  = $(MACHDEP) \
          -pipe -O2 -Wall -Wno-unused-variable -Wno-missing-braces -Wno-cpp \
          $(WII_DEBUG_FLAG) \
          $(GAMEMODE_FLAGS) \
          -msdata=none -G 0 \
          -DGEKKO -DWII \
          -DMAX_CLIENTS=8 \
          -DMAX_RAW_SAMPLES=4096 \
          -DBOTLIB -DUSE_CODEC_VORBIS=0 -DUSE_CODEC_OPUS=0 -DUSE_OPENAL=0 \
          -DUSE_LOCAL_HEADERS \
          -DMIN_DEDICATED_COMHUNKMEGS=24 -DMIN_COMHUNKMEGS=24 \
          $(ZLIB_CFLAGS) \
          -include $(PORTDIR)/code/sys/wii_platform.h \
          -I$(PORTDIR)/code/sys/include \
          $(foreach dir,$(INCLUDES),-I$(dir)) \
          -I$(IOQ3_DIR)/code \
          -I$(IOQ3_DIR)/code/sys \
          -I$(IOQ3_DIR)/code/qcommon \
          -I$(IOQ3_DIR)/code/client \
          -I$(IOQ3_DIR)/code/renderercommon \
          -I$(IOQ3_DIR)/code/renderergl1 \
          -I$(IOQ3_DIR)/code/botlib \
          -I$(LIBOGC_INC) \
          -DOPENGX_AVAILABLE -I$(OPENGX_INC)

CXXFLAGS = $(CFLAGS)

LDFLAGS = $(MACHDEP) -Wl,-Map,$(BUILD)/ioquake3_wii.elf.map -Wl,--wrap,SV_Init -Wl,--wrap,CL_GenerateQKey -Wl,--wrap,VM_Call -Wl,--wrap,Com_Printf -Wl,--wrap,calloc -Wl,--wrap,__malloc_lock -Wl,--wrap,__malloc_unlock -G 0 -T rvl.ld

ifeq ($(INPUT_BACKEND),gamecube)
  LIBS  = -L$(LIBOGC_LIB) -L$(OPENGX_LIB) -lopengx -Wl,--start-group -lasnd -logc -ldi -lfat -lm -Wl,--end-group $(ZLIB_LIBS) -L$(PORTLIBS)/lib -ljpeg
else
  LIBS  = -L$(LIBOGC_LIB) -L$(OPENGX_LIB) -lopengx -lwiiuse -lbte -lwiikeyboard -Wl,--start-group -lasnd -logc -ldi -lfat -lm -Wl,--end-group $(ZLIB_LIBS) -L$(PORTLIBS)/lib -ljpeg
endif

#---------------------------------------------------------------------------------
# Source collection
#---------------------------------------------------------------------------------
# Patched OpenGX source — disabled for now to match the working backup exactly.
# The format-change realloc fix is correct but we need to confirm the stock
# library works first before re-enabling.
#---------------------------------------------------------------------------------
#OGX_PATCHED_SRCS := $(OPENGX_SRC)/texture.c
#OGX_PATCHED_OBJS := $(patsubst $(OPENGX_SRC)/%.c,$(BUILD)/opengx/%.o,$(OGX_PATCHED_SRCS))
OGX_PATCHED_OBJS :=

SOURCES_NO_INPUT := $(filter-out code/input,$(SOURCES))
WII_C_SRCS   := $(foreach dir,$(SOURCES_NO_INPUT),$(wildcard $(dir)/*.c)) \
                $(WII_INPUT_SRC)
WII_CPP_SRCS := $(foreach dir,$(SOURCES_NO_INPUT),$(wildcard $(dir)/*.cpp))
ALL_SRCS     := $(WII_C_SRCS) $(WII_CPP_SRCS) $(IOQ3_SRCS) $(IOQ3_ZLIB_SRCS)

OBJS := $(patsubst %.c,$(BUILD)/%.o,$(filter %.c,$(ALL_SRCS))) \
        $(patsubst %.cpp,$(BUILD)/%.o,$(filter %.cpp,$(ALL_SRCS))) \
        $(OGX_PATCHED_OBJS)

#---------------------------------------------------------------------------------
# Build rules
#---------------------------------------------------------------------------------
.PHONY: all clean dol prebuild

all: $(BUILD)/$(TARGET).elf

# Copy zlib headers next to unzip.h before compiling.
# ZLIB_DIR always points to the right include directory (internal or portlibs).
prebuild:
	@cp $(ZLIB_DIR)/zlib.h $(ZLIB_H_COPY)
	@test -f $(ZLIB_DIR)/zconf.h && cp $(ZLIB_DIR)/zconf.h $(ZCONF_H_COPY) || true

$(BUILD)/$(TARGET).elf: prebuild $(OBJS)
	@echo "Linking $@"
	$(CC) $(LDFLAGS) $(filter %.o,$^) $(LIBS) -o $@

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "CC $<"
	$(CC) $(CFLAGS) -c $< -o $@

# Port-specific files (code/*) need the network shim
$(BUILD)/code/%.o: code/%.c
	@mkdir -p $(dir $@)
	@echo "CC $<"
	$(CC) $(CFLAGS) -DWII_INCLUDE_NET -c $< -o $@

# Patched OpenGX sources — built with OpenGX's own internal include path.
# The -include wii_platform.h is removed (not compatible with OpenGX internals)
# and replaced with the minimal flags OpenGX needs.
$(BUILD)/opengx/%.o: $(OPENGX_SRC)/%.c
	@mkdir -p $(dir $@)
	@echo "CC (opengx) $<"
	$(CC) $(MACHDEP) -pipe -O2 -Wall -Wno-unused-variable -msdata=none -G 0 -DGEKKO -I$(OPENGX_SRC) -I$(OPENGX_INC) -I$(LIBOGC_INC) -c $< -o $@

$(BUILD)/$(IOQ3_DIR)/code/client/cl_ui.o: $(IOQ3_DIR)/code/client/cl_ui.c
	@mkdir -p $(dir $@)
	@echo "CC $< [wii-patched]"
	$(CC) $(CFLAGS) -c $< -o $@

# cl_main.c specific build rule
$(BUILD)/$(IOQ3_DIR)/code/client/cl_main.o: $(IOQ3_DIR)/code/client/cl_main.c
	@mkdir -p $(dir $@)
	@echo "CC $< [wii-patched]"
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/$(IOQ3_DIR)/code/qcommon/common.o: $(IOQ3_DIR)/code/qcommon/common.c
	@mkdir -p $(dir $@)
	@echo "CC $< [wii-patched]"
	$(CC) $(CFLAGS) \
	      -UMIN_COMHUNKMEGS -DMIN_COMHUNKMEGS=8 \
	      -UMIN_DEDICATED_COMHUNKMEGS -DMIN_DEDICATED_COMHUNKMEGS=8 \
	      -c $< -o $@

# net_ip.c and wii_main.c both inline wii_net.h; rebuild both when the shim changes.
WII_NET_H := code/sys/wii_net.h
$(BUILD)/code/sys/wii_main.o: code/sys/wii_main.c $(WII_NET_H)
$(BUILD)/../ioq3/code/qcommon/net_ip.o: ../ioq3/code/qcommon/net_ip.c $(WII_NET_H)
	@mkdir -p $(dir $@)
	@echo "CC $<"
	$(CC) $(CFLAGS) -DWII_INCLUDE_NET -c $< -o $@

# huffman.c has its own internal send() — no special flags needed now that
# network.h is excluded from the force-included platform header

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	@echo "CXX $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

dol: $(BUILD)/$(TARGET).elf
	@echo "Converting to .dol"
	elf2dol $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).dol
	@echo "Done! Copy $(BUILD)/$(TARGET).dol to your SD card: /apps/ioquake3/boot.dol"

clean:
	@rm -rf $(BUILD)
	@find ../ioq3/code -name "*.o" -delete 2>/dev/null || true
	@rm -f $(ZLIB_H_COPY) $(ZCONF_H_COPY)
	@echo "Cleaned."
