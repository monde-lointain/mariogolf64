# Mario Golf (North America) matching decompilation

TARGET      := mariogolf64
BASEROM     := baserom.us.z64
BUILD_DIR   := build
ROM         := $(BUILD_DIR)/$(TARGET).z64
ELF         := $(BUILD_DIR)/$(TARGET).elf
MAP         := $(BUILD_DIR)/$(TARGET).map
LD_SCRIPT   := $(BUILD_DIR)/splat/$(TARGET).ld
SPLAT_YAML  := $(TARGET).yaml

# Set NON_MATCHING=1 to build with decompiled-but-unmatched C instead of asm
NON_MATCHING ?= 0
# Set COMPARE=0 to skip the checksum comparison at the end of the build
COMPARE      ?= 1

CROSS       := mips-linux-gnu-
AS          := $(CROSS)as
LD          := $(CROSS)ld
OBJCOPY     := $(CROSS)objcopy
OBJDUMP     := $(CROSS)objdump
CPP         := $(CROSS)cpp
PYTHON      := .venv/bin/python3

KMC_DIR     := tools/gcc_kmc
CC          := COMPILER_PATH=$(KMC_DIR) $(KMC_DIR)/gcc

INCLUDES    := -Iinclude -Iinclude/libc -Ilib/ultralib/include -Ilib/ultralib/include/PR -Isrc -I$(BUILD_DIR)
DEFINES     := -D_LANGUAGE_C -D_MIPS_SZLONG=32 -D_MIPS_SZINT=32 -DF3DEX_GBI -DNDEBUG -D_FINALROM -DBUILD_VERSION=VERSION_J -D__IS_KMC__=1
ifneq ($(NON_MATCHING),0)
DEFINES     += -DNON_MATCHING
endif

OPTFLAGS    := -O2
CFLAGS      := -G0 -mips3 -mgp32 -mfp32 -mno-abicalls -fno-PIC -nostdinc -funsigned-char -w $(OPTFLAGS)
ASFLAGS     := -march=vr4300 -mabi=32 -G0 -EB -no-pad-sections -Iinclude
LDFLAGS     := -T $(LD_SCRIPT) -T undefined_syms.txt -T $(BUILD_DIR)/splat/undefined_syms_auto.txt \
               -T $(BUILD_DIR)/splat/undefined_funcs_auto.txt -Map $(MAP) --no-check-sections

# Host-side syntax checking of C sources with clang, pretending to be 32-bit MIPS
CC_CHECK       := clang -fsyntax-only -fno-builtin -funsigned-char -std=gnu89 -m32 -nostdinc \
                  -D_MIPS_SZLONG=32 -D_MIPS_SZINT=32 -DCC_CHECK=1 -Wall -Wno-unknown-pragmas \
                  -Wno-unused-variable -Wno-unused-but-set-variable -Wno-missing-braces
RUN_CC_CHECK   ?= 0

# Every object the linker script asks for
O_FILES := $(shell test -f $(LD_SCRIPT) && grep -oE '$(BUILD_DIR)/[A-Za-z0-9_./-]+\.o' $(LD_SCRIPT) | sort -u)

# libultra 2.0J, built the way ultralib builds libgultra_rom except that the
# game's copy was compiled with signed chars
ULTRALIB_DIR      := lib/ultralib
ULTRALIB_INCLUDES := -I$(ULTRALIB_DIR)/include -I$(ULTRALIB_DIR)/include/compiler/gcc -I$(ULTRALIB_DIR)/include/PR
ULTRALIB_DEFINES  := -D_MIPS_SZLONG=32 -D__USE_ISOC99 -DF3DEX_GBI -DBUILD_VERSION=VERSION_J \
                     -DBUILD_VERSION_STRING=\"2.0J\" -DNDEBUG -D_FINALROM
ULTRALIB_CFLAGS   := -w -nostdinc -G 0 -mgp32 -mfp32 -mips3 -D_LANGUAGE_C -O3
ULTRALIB_ASFLAGS  := -x assembler-with-cpp -w -nostdinc -G 0 -mgp32 -mfp32 -mips3 -P \
                     -DMIPSEB -D_LANGUAGE_ASSEMBLY -D_MIPS_SIM=1 -D_ULTRA64

# libmus, the n_audio build of the Software Creations music library, as its
# makefile built libn_gmus. The assembler option lays out statics the way the
# KMC assembler did, giving 8-byte alignment to anything of 8 bytes or more.
LIBMUS_DIR        := lib/libmus
LIBMUS_INCLUDES   := -I$(LIBMUS_DIR)/src -I$(LIBMUS_DIR)/include $(ULTRALIB_INCLUDES)
LIBMUS_DEFINES    := -D_MIPS_SZLONG=32 -D_MIPS_SZINT=32 -DF3DEX_GBI -DNDEBUG -DSUPPORT_NAUDIO
LIBMUS_CFLAGS     := -w -nostdinc -G 0 -mgp32 -mfp32 -mips3 -D_LANGUAGE_C -O3 -Wa,--force-n64align

# n_audio, built from the SDK's n_audio_sc sources for the n_aspMain
# microcode as other games of the time built it
LIBNAUDIO_DIR      := lib/libnaudio
LIBNAUDIO_INCLUDES := -I$(LIBNAUDIO_DIR)/src -I$(LIBNAUDIO_DIR)/include $(ULTRALIB_INCLUDES)
LIBNAUDIO_DEFINES  := -D_MIPS_SZLONG=32 -D_MIPS_SZINT=32 -DF3DEX_GBI -DNDEBUG -DN_MICRO
LIBNAUDIO_CFLAGS   := $(LIBMUS_CFLAGS)

# NuSystem, with the defines of the SDK makefile's ROM library but at -O2,
# the level the game's copy was built at. Like the SDK's nusys.o, the boot
# code is built without optimization. The sources keep the SDK's CRLF line
# endings, and KMC's preprocessor doesn't join lines that end in a backslash
# and a carriage return, so they're compiled from copies without the CRs.
LIBNUSYS_DIR       := lib/libnusys
LIBNUSYS_INCLUDES  := -I$(BUILD_DIR)/$(LIBNUSYS_DIR) $(ULTRALIB_INCLUDES)
LIBNUSYS_DEFINES   := -D_MIPS_SZLONG=32 -D_MIPS_SZINT=32 -DUSE_EPI -DBUILD_VERSION=VERSION_J -D_FINALROM
LIBNUSYS_OPTFLAGS  := -O2
LIBNUSYS_CFLAGS     = -w -nostdinc -G 0 -mgp32 -mfp32 -mips3 -D_LANGUAGE_C $(LIBNUSYS_OPTFLAGS) -Wa,--force-n64align

# Per-directory compiler flags
$(BUILD_DIR)/src/libkmc/%.o:   OPTFLAGS := -O1
$(BUILD_DIR)/$(LIBNUSYS_DIR)/nuboot.c.o: LIBNUSYS_OPTFLAGS :=

.PHONY: all extract clean distclean setup tools

all: $(ROM)
ifneq ($(COMPARE),0)
	@sha1sum -c $(TARGET).sha1
endif

extract:
	$(PYTHON) -m splat split $(SPLAT_YAML)

setup: tools extract

tools: $(KMC_DIR)/gcc $(KMC_DIR)/as

$(KMC_DIR)/gcc:
	@mkdir -p $(KMC_DIR)
	wget -q -O - https://github.com/decompals/mips-gcc-2.7.2/releases/latest/download/gcc-2.7.2-linux.tar.gz | tar xz -C $(KMC_DIR)

$(KMC_DIR)/as:
	@mkdir -p $(KMC_DIR)
	wget -q -O - https://github.com/decompals/mips-binutils-2.6/releases/latest/download/binutils-2.6-linux.tar.gz | tar xz -C $(KMC_DIR)

clean:
	rm -rf $(BUILD_DIR)/asm $(BUILD_DIR)/src $(BUILD_DIR)/assets $(BUILD_DIR)/lib $(ROM) $(ELF)

distclean:
	rm -rf $(BUILD_DIR) asm assets

$(ROM): $(ELF)
	$(OBJCOPY) -O binary --gap-fill=0xFF $< $@

$(ELF): $(O_FILES) $(LD_SCRIPT) undefined_syms.txt
	$(LD) $(LDFLAGS) -o $@

$(BUILD_DIR)/%.s.o: %.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -o $@ $<

$(BUILD_DIR)/%.bin.o: %.bin
	@mkdir -p $(dir $@)
	$(OBJCOPY) -I binary -O elf32-tradbigmips -B mips $< $@

$(BUILD_DIR)/%.c.o: %.c
	@mkdir -p $(dir $@)
ifneq ($(RUN_CC_CHECK),0)
	$(CC_CHECK) $(INCLUDES) $(DEFINES) $<
endif
	$(CC) -c $(CFLAGS) $(INCLUDES) $(DEFINES) -o $@ $<

# KMC GCC reads VR4300MUL from the environment to work around the VR4300
# multiplication bug, as ultralib's build does
$(BUILD_DIR)/$(ULTRALIB_DIR)/%.c.o: $(ULTRALIB_DIR)/%.c
	@mkdir -p $(dir $@)
	VR4300MUL=ON $(CC) -c $(ULTRALIB_CFLAGS) $(ULTRALIB_INCLUDES) $(ULTRALIB_DEFINES) -o $@ $<

$(BUILD_DIR)/$(ULTRALIB_DIR)/%.s.o: $(ULTRALIB_DIR)/%.s
	@mkdir -p $(dir $@)
	$(CC) -c $(ULTRALIB_ASFLAGS) $(ULTRALIB_INCLUDES) $(ULTRALIB_DEFINES) -o $@ $<

$(BUILD_DIR)/$(LIBMUS_DIR)/%.c.o: $(LIBMUS_DIR)/%.c
	@mkdir -p $(dir $@)
	VR4300MUL=ON $(CC) -c $(LIBMUS_CFLAGS) $(LIBMUS_INCLUDES) $(LIBMUS_DEFINES) -o $@ $<

$(BUILD_DIR)/$(LIBNAUDIO_DIR)/%.c.o: $(LIBNAUDIO_DIR)/%.c
	@mkdir -p $(dir $@)
	VR4300MUL=ON $(CC) -c $(LIBNAUDIO_CFLAGS) $(LIBNAUDIO_INCLUDES) $(LIBNAUDIO_DEFINES) -o $@ $<

$(BUILD_DIR)/$(LIBNUSYS_DIR)/%.c.o: $(LIBNUSYS_DIR)/%.c $(BUILD_DIR)/$(LIBNUSYS_DIR)/nusys.h
	@mkdir -p $(dir $@)
	tr -d '\r' < $< > $(@:.o=)
	VR4300MUL=ON $(CC) -c $(LIBNUSYS_CFLAGS) $(LIBNUSYS_INCLUDES) $(LIBNUSYS_DEFINES) -o $@ $(@:.o=)

$(BUILD_DIR)/$(LIBNUSYS_DIR)/nusys.h: $(LIBNUSYS_DIR)/nusys.h
	@mkdir -p $(dir $@)
	tr -d '\r' < $< > $@

# splat names data-only objects as if they were C, so assembly files that
# only hold data (such as gu/libm_vals.s) are found under a .c.o name
$(BUILD_DIR)/$(ULTRALIB_DIR)/%.c.o: $(ULTRALIB_DIR)/%.s
	@mkdir -p $(dir $@)
	$(CC) -c $(ULTRALIB_ASFLAGS) $(ULTRALIB_INCLUDES) $(ULTRALIB_DEFINES) -o $@ $<

-include $(BUILD_DIR)/splat/$(TARGET).d
