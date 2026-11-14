#---------------------------------------------------------------------------------
# ANGRY BIRDS RELOADED -- Switch homebrew loader
# (forked from the FRUIT NINJA CLASSIC+ port; Unity 2022.3.7f1 / IL2CPP, arm64)
# Switch 1 requires switch-mesa and switch-libdrm_nouveau; Switch 2 uses
# ounce-devkit/OunceVK. Both builds use switch-sdl2, switch-zlib and switch-libpng.
#
# All .c files in source/ are compiled automatically, so imports_abr_extra.c
# and imports_ndk_extra.c are picked up without editing this list.
# nx_patch_abr.h and unity_entrypoints.h are headers included by main.c.
#---------------------------------------------------------------------------------
.SUFFIXES:
ifeq ($(strip $(DEVKITPRO)),)
$(error "Set DEVKITPRO in your environment. (export DEVKITPRO=/opt/devkitpro)")
endif
TOPDIR ?= $(CURDIR)
SWITCH2 ?= 0
ifeq ($(SWITCH2),1)
include $(DEVKITPRO)/libounce/switch2_rules
else
include $(DEVKITPRO)/libnx/switch_rules
endif

TARGET    := abreloaded_nx
APP_TITLE := Angry Birds Reloaded
APP_AUTHOR := aks796, Rovio Entertainment
APP_VERSION := 1.0.0
APP_ICON  := $(TOPDIR)/reloaded.jpg
export APP_TITLE APP_AUTHOR APP_VERSION APP_ICON
BUILD     := build
SOURCES   := source
INCLUDES  := source

ifeq ($(SWITCH2),1)
ARCH    := $(SWITCH2_CPU) -mtp=soft -fPIE
else
ARCH    := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE
endif

CFLAGS  := -g -Wall -O2 -ffunction-sections $(ARCH) $(DEFINES) \
           $(INCLUDE) -D__SWITCH__
CFLAGS  += -DLOAD_ADDRESS=0xC0000000
CXXFLAGS := $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++17
ASFLAGS := -g $(ARCH)
LDFLAGS  = -specs=$(DEVKITPRO)/libnx/switch.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map) \
           -Wl,--wrap,free -Wl,--wrap,malloc -Wl,--wrap,memalign -Wl,--wrap,calloc -Wl,--wrap,realloc -Wl,--wrap,memmove -Wl,--wrap,memcpy

# mesa GLES3 + EGL + nouveau, SDL2 for window/HID/audio, libpng for the two
# cursor state PNGs (nx_pointer.c), zlib. -lpng must precede -lz.
ifeq ($(SWITCH2),1)
# OunceVK provides EGL/GLES in libEGL and does not use Switch 1's Nouveau
# driver or Mesa's private ETC2 decoder. GLES2+ needs no separate libraries.
CFLAGS += -D__SWITCH2__
# libkaihen supplies libounce's weak code-mapping hooks through ldr:ro.
LIBS := -lSDL2 -lEGL -lpng -lz -lkaihen -lnx -lm
else
LDFLAGS += -Wl,--defsym,abr_mesa_unpack_etc2=_mesa_texstore_etc2_rgba8_eac+0x80
LIBS := -lSDL2 -lGLESv2 -lEGL -lglapi -ldrm_nouveau -lpng -lz -lnx -lm
endif

LIBDIRS := $(PORTLIBS) $(LIBNX)

ifneq ($(BUILD),$(notdir $(CURDIR)))
export OUTPUT  := $(CURDIR)/$(TARGET)
export TOPDIR  := $(CURDIR)
export VPATH   := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)

CFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))

export LD := $(CXX)
export OFILES := $(addsuffix .o,$(SFILES)) $(CPPFILES:.cpp=.o) $(CFILES:.c=.o)
export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                  $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                  -I$(PORTLIBS)/include/SDL2 -I$(CURDIR)/$(BUILD)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: all clean
all: $(BUILD)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
$(BUILD):
	@mkdir -p $@
clean:
	@rm -fr $(BUILD) $(TARGET).nro $(TARGET).nacp $(TARGET).elf
else
DEPENDS := $(OFILES:.o=.d)
# embed the icon + NACP (title/author/version) into the NRO asset section
NROFLAGS := --icon=$(APP_ICON) --nacp=$(OUTPUT).nacp
all : $(OUTPUT).nro
$(OUTPUT).nro : $(OUTPUT).elf $(OUTPUT).nacp
$(OUTPUT).elf : $(OFILES)
-include $(DEPENDS)
endif
