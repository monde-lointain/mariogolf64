BASENAME  = mariogolf64
.DEFAULT_GOAL := all

ifndef QUIET
  PRINTF = @printf
else
  PRINTF = @true
endif

VERBOSE ?= 0
ifeq ($(VERBOSE),0)
  V := @
endif

# Colours

NO_COL  := \033[0m
RED     := \033[0;31m
RED2    := \033[1;31m
GREEN   := \033[0;32m
YELLOW  := \033[0;33m
BLUE    := \033[0;34m
PINK    := \033[0;35m
CYAN    := \033[0;36m

# Directories and files

BUILD_DIR = build
TOOLS_DIR = tools
BASE_ROM  = baserom.z64
CONFIG    = $(BASENAME).yaml
SHA1_FILE = $(BASENAME).sha1
LD_SCRIPT = $(BASENAME).ld
TARGET    = $(BUILD_DIR)/$(BASENAME)

# Tools

find-command = $(shell which $(1) 2>/dev/null)

ifneq      ($(call find-command,mips-linux-gnu-ld),)
  CROSS := mips-linux-gnu-
else ifneq ($(call find-command,mips64-linux-gnu-ld),)
  CROSS := mips64-linux-gnu-
else ifneq ($(call find-command,mips64-elf-ld),)
  CROSS := mips64-elf-
else
  $(error Unable to detect a suitable MIPS cross toolchain installed.)
endif

AS      = $(CROSS)as
LD      = $(CROSS)ld
OBJCOPY = $(CROSS)objcopy
OBJDUMP = $(CROSS)objdump
STRIP   = $(CROSS)strip
# Assembly is preprocessed by the host C preprocessor; target defines are explicit.
CPP     = cpp
PYTHON ?= $(if $(wildcard .venv/bin/python3),.venv/bin/python3,python3)
SPLAT   = $(PYTHON) -m splat split
N64CRC  = $(PYTHON) $(TOOLS_DIR)/n64crc.py
LLVM_MAJOR_VERSION := 23
CLANG_FORMAT ?= clang-format-$(LLVM_MAJOR_VERSION)
CLANG_TIDY   ?= clang-tidy-$(LLVM_MAJOR_VERSION)

# KMC GCC 2.7.2 compiles C to assembly; KMC binutils 2.6 assembles it. KMC `as`
# emits the original `move` encoding and pads .text to its 16-byte alignment.
# tools/kmc-cc runs that pipeline for the ROM build, decomp environments, and
# the permuter alike.
COMPILER_DIR = $(TOOLS_DIR)/gcc_kmc
CC     := COMPILER_PATH=$(COMPILER_DIR) $(COMPILER_DIR)/gcc
KMC_CC := STRIP=$(STRIP) $(TOOLS_DIR)/kmc-cc
KMC_AS := $(COMPILER_DIR)/as

# Flags

CFLAGS_BASE := -G 0 -mips3 -mgp32 -mfp32 -mno-abicalls
OPT_FLAGS   := -O2
IINC        := -I include -I include/libultra -I include/libultra/internal -I include/libkmc \
               -I include/libnusys -I include/libmus -I include/libnualstl -I include/libnaudio
MACROS      := -DINCLUDE_ASM_USE_MACRO_INC -D_LANGUAGE_C -D_FINALROM
ifeq ($(NON_MATCHING),1)
  MACROS += -DNON_MATCHING
endif

# Per-library profiles override these through the target-specific rules below.
# PROFILE_PREPEND flags precede the base flags so its include paths win.
PROFILE_PREPEND :=
PROFILE_APPEND  :=
C_PROFILE_CFLAGS = $(PROFILE_PREPEND) $(CFLAGS_BASE) $(OPT_FLAGS) $(IINC) $(MACROS) $(PROFILE_APPEND)

ASFLAGS      := -march=vr4300 -32 -I include --no-pad-sections
AS_CPPFLAGS  := -fno-dollars-in-identifiers -P -I include -DMIPSEB -D_LANGUAGE_ASSEMBLY -D_ULTRA64
OBJCOPY_ALIGN := --set-section-alignment .text=4 --set-section-alignment .data=4 \
                 --set-section-alignment .rodata=4 --set-section-alignment .bss=4

# Vendored libultra hand-written assembly goes through the KMC driver: KMC pads
# each function's .text to the 16-byte slot the ROM expects; modern GAS does not.
LIBULTRA_ASFLAGS := -x assembler-with-cpp -w -nostdinc -c -G 0 -mips3 -mgp32 -mfp32 \
                    -DMIPSEB -D_LANGUAGE_ASSEMBLY -D_MIPS_SIM=1 -D_ULTRA64 -D_MIPS_SZLONG=32 \
                    -D__USE_ISOC99 -DBUILD_VERSION=VERSION_J -D_FINALROM \
                    -I include -I include/libultra -I include/libultra/PR -I include/libultra/compiler/gcc

LINKER_SCRIPTS := linker_scripts/hardware_regs.ld linker_scripts/n64_os_syms.ld \
                  undefined_funcs_auto.txt undefined_syms_auto.txt
LDFLAGS        := -T $(LD_SCRIPT) -Map $(TARGET).map $(foreach ld,$(LINKER_SCRIPTS),-T $(ld)) \
                  --no-check-sections

#######################
## COMPILER PROFILES ##
#######################

# The pattern-specific variables match both `<file>.o` and `<file>.cflags`, so
# `make print-cflags` reports exactly what the ROM build uses. Longer stems win:
# a file-specific override beats its library profile.

# Main game code uses the F3DEX2 GBI (MG64 runs gspF3DEX2.fifo).
$(BUILD_DIR)/src/main/%: PROFILE_APPEND = -DF3DEX_GBI_2

# These TUs inline sqrtf()/sqrt() as bare sqrt.s/sqrt.d; without -ffast-math
# KMC GCC adds a NaN guard and a library fallback that the ROM lacks.
FAST_MATH_SRCS := \
	src/main/vec3f_normalize \
	src/main/raycast_terrain \
	src/main/get_tile_attribute \
	src/main/set_camera_matrices_fixed \
	src/main/func_8006A000 \
	src/main/func_80078910 \
	src/main/func_80080220 \
	src/main/func_800453E0 \
	src/main/func_80054900 \
	src/main/func_80059BA0 \
	src/main/func_80095A10
$(foreach src,$(FAST_MATH_SRCS),$(eval $(BUILD_DIR)/$(src).%: PROFILE_APPEND = -DF3DEX_GBI_2 -ffast-math))

# func_ovl7_802068B4 inlines sqrtf() as bare sqrt.s, so this overlay TU is fast-math. Only this file: the rest of overlay_7 stays on the base profile (no F3DEX_GBI_2 here, unlike FAST_MATH_SRCS).
$(BUILD_DIR)/src/overlay_7/func_ovl7_80206680.%: PROFILE_APPEND = -ffast-math

# libkmc was built with `gcc -O` (genn64.bat).
$(BUILD_DIR)/src/libkmc/%: OPT_FLAGS = -O

# libultra is VERSION_J at -O3. This ROM's libultra was built with signed chars.
# Its compiler headers precede include/libkmc so stdlib.h resolves to libultra's.
$(BUILD_DIR)/src/libultra/%: OPT_FLAGS = -O3
$(BUILD_DIR)/src/libultra/%: PROFILE_PREPEND = -I include/libultra/compiler/gcc
$(BUILD_DIR)/src/libultra/%: PROFILE_APPEND = -fsigned-char -DBUILD_VERSION=VERSION_J -DF3DEX_GBI_2 -I include/libultra/PR

# libnusys enables the nuPi* bodies with USE_EPI.
$(BUILD_DIR)/src/libnusys/%: PROFILE_APPEND = -DUSE_EPI
# The game-embedded nusys boot shipped at -O0 without USE_EPI.
$(BUILD_DIR)/src/libnusys/nuboot.%: OPT_FLAGS = -O0
$(BUILD_DIR)/src/libnusys/nuboot.%: PROFILE_APPEND =

# libnaudio (n_audio_sc) is -O3 with the micro command stream; its private
# headers precede the libultra-internal copies of the same names.
$(BUILD_DIR)/src/libnaudio/%: OPT_FLAGS = -O3
$(BUILD_DIR)/src/libnaudio/%: PROFILE_PREPEND = -I src/libnaudio
$(BUILD_DIR)/src/libnaudio/%: PROFILE_APPEND = -I include/libultra/PR -DN_MICRO=1

# libmus is the n_audio variant at -O3, built without _FINALROM (OSScTask keeps
# its trailing timing fields).
$(BUILD_DIR)/src/libmus/%: OPT_FLAGS = -O3
$(BUILD_DIR)/src/libmus/%: PROFILE_PREPEND = -I src/libmus -I include/libmus/PR
$(BUILD_DIR)/src/libmus/%: PROFILE_APPEND = -I include/libultra/PR -DSUPPORT_NAUDIO -U_FINALROM

###########
## FILES ##
###########

# The extracted linker script names every object in the ROM.
O_FILES := $(shell grep -oE '$(BUILD_DIR)/[^ ()]+\.o' $(LD_SCRIPT) 2>/dev/null | sort -u)
DEP_FILES := $(patsubst %.o,%.d,$(filter $(BUILD_DIR)/src/%,$(O_FILES)))

# Formatting and tidy cover project-owned sources; include/ holds vendored SDK headers.
PROJECT_C_FILES := $(shell git ls-files 'src/*.c' 'src/**/*.c' 2>/dev/null)
PROJECT_H_FILES := $(shell git ls-files 'src/*.h' 'src/**/*.h' 2>/dev/null)
FORMAT_FILES    := $(PROJECT_C_FILES) $(PROJECT_H_FILES)

#############
## TARGETS ##
#############

all: $(TARGET).z64 $(if $(filter 1,$(NON_MATCHING)),,verify)

# Build the ROM without SHA1 verification (useful while a build is non-matching).
nonmatching: $(TARGET).z64
	$(V)$(N64CRC) $(TARGET).z64
	$(PRINTF) "[$(YELLOW) nonmatch $(NO_COL)]  Built $(TARGET).z64 (SHA1 verification skipped)\n"

# splat skips textbin .s files that already exist, and then never marks their
# symbols as defined, so they leak into undefined_syms_auto.txt. Start from an
# empty asm/ so re-extracting gives the same output as a clean tree.
extract: check
	$(PRINTF) "[$(CYAN) splat  $(NO_COL)]  Extracting $(CONFIG)\n"
	$(V)rm -rf asm
	$(V)$(SPLAT) $(CONFIG) --make-full-disasm-for-code

verify: $(TARGET).z64
	$(PRINTF) "[$(GREEN) verify $(NO_COL)]  Checking $(SHA1_FILE)\n"
	$(V)shasum --check $(SHA1_FILE)

check:
	@if [ ! -f $(BASE_ROM) ]; then echo "Error: $(BASE_ROM) not found" >&2; exit 1; fi

# Print the compiler flags the ROM build uses for a C file:
#   make -s print-cflags SRC=src/main/func_80025D30.c
print-cflags:
	@test -n "$(SRC)" || { echo "Usage: make -s print-cflags SRC=src/<path>.c" >&2; exit 2; }
	@$(MAKE) -s --no-print-directory $(BUILD_DIR)/$(SRC:.c=.cflags)

$(BUILD_DIR)/%.cflags: FORCE
	@echo $(C_PROFILE_CFLAGS)

check-clang-format-version:
	@command -v $(CLANG_FORMAT) >/dev/null 2>&1 || { echo "Error: $(CLANG_FORMAT) was not found" >&2; exit 1; }

check-clang-tidy-version:
	@command -v $(CLANG_TIDY) >/dev/null 2>&1 || { echo "Error: $(CLANG_TIDY) was not found" >&2; exit 1; }

format: check-clang-format-version
	$(CLANG_FORMAT) -i -style=file $(FORMAT_FILES)

format-check: check-clang-format-version
	$(CLANG_FORMAT) --dry-run --Werror -style=file $(FORMAT_FILES)

# Tidy is diagnostic-only: automatic fixes can alter matching code generation.
tidy: check-clang-tidy-version
	$(CLANG_TIDY) $(PROJECT_C_FILES) -- -fsyntax-only -std=gnu89 -m32 -D_MIPS_SZLONG=32 -D_MIPS_SZINT=32 \
		$(IINC) $(MACROS) -DF3DEX_GBI_2

diff-line:
	$(V)(diff --old-line-format='OLD: %L' --new-line-format='NEW: %L' <(xxd $(BASE_ROM)) <(xxd $(TARGET).z64) || true) > romdiff

install-hooks:
	@$(TOOLS_DIR)/install-git-hooks.sh

setup: install-hooks
	$(MAKE) -C $(TOOLS_DIR)

clean:
	$(PRINTF) "[$(YELLOW) clean  $(NO_COL)]  Removing generated files\n"
	$(V)rm -rf asm assets $(BUILD_DIR)
	$(V)rm -f $(LD_SCRIPT) $(BASENAME).d undefined_funcs_auto.txt undefined_syms_auto.txt

#################
## COMPILATION ##
#################

# *.c -> *.o (and a header dependency file)
$(BUILD_DIR)/src/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(PRINTF) "[$(GREEN)   c    $(NO_COL)]  $<\n"
	$(V)$(KMC_CC) $(C_PROFILE_CFLAGS) $< -o $@
	$(V)$(CC) -M $(C_PROFILE_CFLAGS) $< | sed '1s#^[^:]*:#$@:#' > $(@:.o=.d)

# Vendored libkmc soft-float assembly keeps KMC register conventions.
$(BUILD_DIR)/src/libkmc/%.o: src/libkmc/%.s
	@mkdir -p $(dir $@)
	$(PRINTF) "[$(GREEN)   as   $(NO_COL)]  $<\n"
	$(V)$(KMC_AS) -EB -mips2 -I src/libkmc -o $@ $<

$(BUILD_DIR)/src/libkmc/mcvtld.o: src/libkmc/mips_as.h

$(BUILD_DIR)/src/libultra/%.o: src/libultra/%.s
	@mkdir -p $(dir $@)
	$(PRINTF) "[$(GREEN)   as   $(NO_COL)]  $<\n"
	$(V)$(CC) $(LIBULTRA_ASFLAGS) -o $(@:.o=.tmp.o) $<
	$(V)$(OBJCOPY) $(OBJCOPY_ALIGN) $(@:.o=.tmp.o) $@
	$(V)rm $(@:.o=.tmp.o)

# Extracted assembly and the entry stub: host cpp, then modern GAS.
$(BUILD_DIR)/%.o: %.s
	@mkdir -p $(dir $@)
	$(PRINTF) "[$(GREEN)   as   $(NO_COL)]  $<\n"
	$(V)$(CPP) $(AS_CPPFLAGS) $< | $(AS) $(ASFLAGS) -o $(@:.o=.tmp.o)
	$(V)$(OBJCOPY) $(OBJCOPY_ALIGN) $(@:.o=.tmp.o) $@
	$(V)rm $(@:.o=.tmp.o)

# *.bin -> *.o
$(BUILD_DIR)/%.o: %.bin
	@mkdir -p $(dir $@)
	$(PRINTF) "[$(PINK) linker $(NO_COL)]  $<\n"
	$(V)$(OBJCOPY) -I binary -O elf32-tradbigmips $< $@

# *.o -> *.elf
$(TARGET).elf: $(O_FILES) $(LD_SCRIPT) $(LINKER_SCRIPTS)
	$(PRINTF) "[$(PINK) linker $(NO_COL)]  Linking $@\n"
	$(V)$(LD) $(LDFLAGS) -o $@

# *.elf -> *.z64
$(TARGET).z64: $(TARGET).elf
	$(PRINTF) "[$(CYAN) objcpy $(NO_COL)]  $<\n"
	$(V)$(OBJCOPY) -O binary $< $@

-include $(DEP_FILES)

### Settings
.SECONDARY:
.DELETE_ON_ERROR:
.PHONY: all clean extract nonmatching verify check print-cflags format format-check tidy \
	check-clang-format-version check-clang-tidy-version diff-line install-hooks setup FORCE
FORCE:
SHELL = /bin/bash -e -o pipefail

# Print target for debugging
print-% : ; $(info $* is a $(flavor $*) variable set to [$($*)]) @true
