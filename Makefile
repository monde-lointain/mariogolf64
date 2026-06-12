MAKEFLAGS += --no-builtin-rules

# Use bash with pipefail so a failure anywhere in a recipe pipe aborts the build
# LOUDLY. The asm rule is `$(CPP) ... | $(AS) ...`; without pipefail a missing
# $(CPP) feeds empty input to $(AS), which "succeeds" and silently emits 0-byte
# objects (cost a full session on 2026-06-11 when mips-linux-gnu-cpp was removed).
SHELL := /bin/bash
.SHELLFLAGS := -o pipefail -c

# Fail if no baserom
ifeq (,$(wildcard baserom.z64))
$(error baserom.z64 not found. Place your ROM in the project root.)
endif

BASENAME := mariogolf64
TARGET := $(BASENAME).z64
LD_SCRIPT := $(BASENAME).ld

# NONMATCHING mode - compiles WIP C code and skips md5 check
NONMATCHING ?= 0

# Directories
BUILD_DIR := build
ASM_DIR := asm
ASSETS_DIR := assets

# Tools - system binutils for assembly, KMC for C compilation
MIPS_BINUTILS_PREFIX ?= mips-linux-gnu-
AS      := $(MIPS_BINUTILS_PREFIX)as
LD      := $(MIPS_BINUTILS_PREFIX)ld
OBJCOPY := $(MIPS_BINUTILS_PREFIX)objcopy
STRIP   := $(MIPS_BINUTILS_PREFIX)strip
CPP     := $(MIPS_BINUTILS_PREFIX)cpp

# Abort early + clearly if the asm preprocessor is missing, rather than letting the
# cpp|as pipe emit empty objects (belt-and-suspenders with .SHELLFLAGS pipefail above).
ifeq (,$(shell command -v $(CPP) 2>/dev/null))
$(error $(CPP) not found — install it, e.g. `sudo apt install cpp-mips-linux-gnu`)
endif

# Compiler - KMC GCC 2.7.2
CC := COMPILER_PATH=tools/cc tools/cc/gcc

# Flags
ASFLAGS := -march=vr4300 -32 -I include --no-pad-sections
CPPFLAGS := -fno-dollars-in-identifiers -P
AS_DEFINES := -DMIPSEB -D_LANGUAGE_ASSEMBLY -D_ULTRA64
CFLAGS := -G 0 -mips3 -mgp32 -mfp32 -mno-abicalls -O2 -I include -I include/libultra -I include/libultra/internal -I include/libkmc -I include/libnusys -DINCLUDE_ASM_USE_MACRO_INC -D_LANGUAGE_C -D_FINALROM
ifeq ($(NONMATCHING),1)
CFLAGS += -DNONMATCHING
endif

# libultra upstream (ultralib gcc.mk, VERSION_J libgultra_rom):
#   OPTFLAGS=-O3, MIPS_VERSION=-mips3, CFLAGS includes -funsigned-char for VERSION <= J,
#   CPPFLAGS includes -DBUILD_VERSION=VERSION_J -D_FINALROM.
# -mips3 is now the project default (see CFLAGS above). -O3 and -funsigned-char are
# libultra-specific overrides; -DBUILD_VERSION=VERSION_J guards version-conditional code.
LIBULTRA_CFLAGS := $(subst -O2,-O3,$(CFLAGS)) -funsigned-char -DBUILD_VERSION=VERSION_J

# libkmc upstream was built with `gcc -O` (not -O2) per ~/development/repos/libkmc/src/genn64.bat
# (env var gccsw=-mips3 -mgp32 -mfp32 -G0; explicit -O per file compile). -mips3 is now the
# project default. -O (vs -O2) is the only remaining libkmc-specific override.
LIBKMC_CFLAGS := $(subst -O2,-O,$(CFLAGS))

# libnusys: -DUSE_EPI enables nuPi* function bodies (upstream Makefile sets USE_EPI=1 for all nusys).
LIBNUSYS_CFLAGS := $(CFLAGS) -DUSE_EPI

# Directories
SRC_DIR := src

# Find all source files
ASM_FILES := $(wildcard $(ASM_DIR)/*.s) $(wildcard $(ASM_DIR)/data/*.s)
BIN_FILES := $(wildcard $(ASSETS_DIR)/*.bin)
# IMPORTANT: this MUST be a recursive `find`, NOT `$(wildcard $(SRC_DIR)/*.c)`.
# Library code lives at nested paths: src/libultra/vi/<file>.c, src/libkmc/<file>.c,
# and future SDK upstream mirrors will follow the same pattern. `$(wildcard)` is
# non-recursive and silently drops those files from the build, producing
# linker-time "undefined reference" errors that are hard to trace back to a
# Makefile glob. Do NOT "simplify" this line back to wildcard.
C_FILES := $(shell find $(SRC_DIR) -name '*.c')

# Object files
ASM_O_FILES := $(patsubst $(ASM_DIR)/%.s,$(BUILD_DIR)/$(ASM_DIR)/%.o,$(ASM_FILES))
BIN_O_FILES := $(patsubst $(ASSETS_DIR)/%.bin,$(BUILD_DIR)/$(ASSETS_DIR)/%.o,$(BIN_FILES))
C_O_FILES := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/$(SRC_DIR)/%.o,$(C_FILES))

O_FILES := $(ASM_O_FILES) $(BIN_O_FILES) $(C_O_FILES)

# Default target
all: $(BUILD_DIR)/$(TARGET)
ifneq ($(NONMATCHING),1)
	@md5sum -c $(BASENAME).md5
endif

clean:
	rm -rf $(BUILD_DIR)

distclean: clean
	rm -rf asm assets $(LD_SCRIPT) symbol_addrs.txt reloc_addrs.txt

setup:
	python3 -m venv venv
	./venv/bin/pip install -r tools/requirements.txt
	$(MAKE) -C tools

extract:
	./venv/bin/python3 -m splat split $(BASENAME).yaml

sync-names:
	-GHIDRA_REPO="$${MARIOGOLF64_GHIDRA_REPO:-$$HOME/development/reversing/ghidra/mariogolf64}"; \
	  SYNC_PY="$$GHIDRA_REPO/venv/bin/python3"; \
	  [ -x "$$SYNC_PY" ] || SYNC_PY=./venv/bin/python3; \
	  [ -x "$$SYNC_PY" ] || SYNC_PY=python3; \
	  "$$SYNC_PY" "$$GHIDRA_REPO/scripts/sync_decomp_names.py" \
	    --export-to-decomp --decomp-root . --write-in-place

.PHONY: all clean distclean setup extract sync-names nonmatching-func spotcheck-build clean-nonmatchings

# Create build directories. The trailing `$(sort $(dir $(C_O_FILES)))` is
# REQUIRED: when C_FILES recursively picks up src/libultra/vi/<file>.c, the
# pattern rule writes to build/src/libultra/vi/<file>.o, and that nested
# parent dir must exist. Without this expansion, the as-step fails opaquely
# with "No such file or directory" on the .o.tmp output and the linker reports
# the missing .o as an undefined-reference error. Do NOT remove the $(sort)
# block.
$(shell mkdir -p $(BUILD_DIR)/$(ASM_DIR)/data $(BUILD_DIR)/$(ASSETS_DIR) $(BUILD_DIR)/$(SRC_DIR) $(sort $(dir $(C_O_FILES))))

# Link
$(BUILD_DIR)/$(TARGET): $(BUILD_DIR)/$(BASENAME).elf
	$(OBJCOPY) -O binary $< $@

$(BUILD_DIR)/$(BASENAME).elf: $(O_FILES) $(LD_SCRIPT) undefined_funcs_auto.txt undefined_syms_auto.txt hardware_regs.txt extern_syms.txt
	$(LD) --no-check-sections --omagic --allow-multiple-definition -T hardware_regs.txt -T undefined_funcs_auto.txt -T undefined_syms_auto.txt -T extern_syms.txt -T $(LD_SCRIPT) -Map $(BUILD_DIR)/$(BASENAME).map -o $@

# Assemble (CPP preprocess, then assemble with system as)
$(BUILD_DIR)/$(ASM_DIR)/%.o: $(ASM_DIR)/%.s
	$(CPP) $(CPPFLAGS) -I include $(AS_DEFINES) $< | $(AS) $(ASFLAGS) -o $@.tmp
	$(OBJCOPY) --set-section-alignment .text=4 --set-section-alignment .data=4 --set-section-alignment .rodata=4 --set-section-alignment .bss=4 $@.tmp $@
	rm $@.tmp

# Binary to object
$(BUILD_DIR)/$(ASSETS_DIR)/%.o: $(ASSETS_DIR)/%.bin
	$(OBJCOPY) -I binary -O elf32-tradbigmips $< $@

# Compile C (KMC GCC → .s, then KMC binutils 2.6 `as` → .o).
# Using the original assembler (tools/cc/as) gives us the KMC `move dst,zero`
# → `addu dst,zero,zero` encoding natively, AND auto-pads .text to its 16-byte
# section alignment. Modern `mips-linux-gnu-as` is kept for asm/% (it
# understands modern-only directives like .set gp=64 that raw asm uses).
$(BUILD_DIR)/$(SRC_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) -S $(CFLAGS) -o $@.s $<
	tools/cc/as -EB -mips2 -I include -o $@.tmp $@.s
	cp $@.tmp $@
	$(STRIP) $@ -N dummy-symbol-name
	rm $@.s $@.tmp

# libultra compile profile: -O3 -funsigned-char -DBUILD_VERSION=VERSION_J (see LIBULTRA_CFLAGS).
# Pattern-rule specificity beats the generic src/% rule.
$(BUILD_DIR)/$(SRC_DIR)/libultra/%.o: $(SRC_DIR)/libultra/%.c
	$(CC) -S $(LIBULTRA_CFLAGS) -o $@.s $<
	tools/cc/as -EB -mips2 -I include -o $@.tmp $@.s
	cp $@.tmp $@
	$(STRIP) $@ -N dummy-symbol-name
	rm $@.s $@.tmp

# libkmc compile profile: same pipeline as above, but with -O instead of -O2
# (see LIBKMC_CFLAGS comment). Pattern-rule specificity puts this above the
# generic src/% rule when both match.
$(BUILD_DIR)/$(SRC_DIR)/libkmc/%.o: $(SRC_DIR)/libkmc/%.c
	$(CC) -S $(LIBKMC_CFLAGS) -o $@.s $<
	tools/cc/as -EB -mips2 -I include -o $@.tmp $@.s
	cp $@.tmp $@
	$(STRIP) $@ -N dummy-symbol-name
	rm $@.s $@.tmp

# libnusys compile profile: adds -DUSE_EPI for nuPi* files that guard their body
# behind #ifdef USE_EPI (see LIBNUSYS_CFLAGS). Pattern-rule specificity beats generic src/%.
$(BUILD_DIR)/$(SRC_DIR)/libnusys/%.o: $(SRC_DIR)/libnusys/%.c
	$(CC) -S $(LIBNUSYS_CFLAGS) -o $@.s $<
	tools/cc/as -EB -mips2 -I include -o $@.tmp $@.s
	cp $@.tmp $@
	$(STRIP) $@ -N dummy-symbol-name
	rm $@.s $@.tmp

# === execution-loop targets ===================================================
# Driven by tools/decomp_loop.py inside the iteration loop, and by the execution
# loop's in-tree spot-check step. Mirrors decomp.me's KMC GCC compile so the resulting
# object reflects what decomp.me would produce (single-step `gcc -c`).

# Per-function compile of nonmatchings/$(FUNC)/base.c -> nonmatchings/$(FUNC)/current.o.
# base.c has no INCLUDE_ASM directives, so single-step is safe (no need to route
# through system `as`). Always defines NONMATCHING.
nonmatching-func:
	@test -n "$(FUNC)" || { echo "Usage: make nonmatching-func FUNC=<func_XXXXXXXX> [LIBKMC=1|LIBULTRA=1]" >&2; exit 1; }
	@test -f nonmatchings/$(FUNC)/base.c || { echo "nonmatchings/$(FUNC)/base.c not found; run tools/seed_c.py first" >&2; exit 1; }
	$(CC) -c $(if $(LIBKMC),$(LIBKMC_CFLAGS),$(if $(LIBULTRA),$(LIBULTRA_CFLAGS),$(CFLAGS))) -DNONMATCHING -o nonmatchings/$(FUNC)/current.o nonmatchings/$(FUNC)/base.c

# In-tree spot-check build. Compiles src/$(SEG).c.spotcheck (a temporary copy
# of the parent with the target function's INCLUDE_ASM wrapped in
# `#ifndef NONMATCHING / INCLUDE_ASM / #else <C body> #endif`) with
# NONMATCHING=1 into build/src/$(SEG).spotcheck.o. Two-step compile because
# the spotcheck source inherits the parent's other INCLUDE_ASM directives.
# The execution loop's finalize step creates and removes the .spotcheck file;
# this target only builds it.
spotcheck-build:
	@test -n "$(SEG)" || { echo "Usage: make spotcheck-build SEG=<seg> FUNC=<func_XXXXXXXX>" >&2; exit 1; }
	@test -n "$(FUNC)" || { echo "Usage: make spotcheck-build SEG=<seg> FUNC=<func_XXXXXXXX>" >&2; exit 1; }
	@test -f src/$(SEG).c.spotcheck || { echo "src/$(SEG).c.spotcheck not found (created during the execution loop's finalize step)" >&2; exit 1; }
	$(CC) -x c -S $(CFLAGS) -DNONMATCHING -o $(BUILD_DIR)/$(SRC_DIR)/$(SEG).spotcheck.s src/$(SEG).c.spotcheck
	tools/cc/as -EB -mips2 -I include -o $(BUILD_DIR)/$(SRC_DIR)/$(SEG).spotcheck.tmp $(BUILD_DIR)/$(SRC_DIR)/$(SEG).spotcheck.s
	cp $(BUILD_DIR)/$(SRC_DIR)/$(SEG).spotcheck.tmp $(BUILD_DIR)/$(SRC_DIR)/$(SEG).spotcheck.o
	$(STRIP) $(BUILD_DIR)/$(SRC_DIR)/$(SEG).spotcheck.o -N dummy-symbol-name
	rm $(BUILD_DIR)/$(SRC_DIR)/$(SEG).spotcheck.s $(BUILD_DIR)/$(SRC_DIR)/$(SEG).spotcheck.tmp

# Wipe per-function scratch dirs. Leaves the build dir alone.
clean-nonmatchings:
	rm -rf nonmatchings/*/

# Vendor override: build/asm/8EC50.o from src/libkmc/mmuldi3.s using KMC as.
# Explicit rule wins over the asm/%.s pattern rule for this specific target.
# mmuldi3.s uses KMC register-name conventions (move → addu encoding); must
# use tools/cc/as, not modern mips-linux-gnu-as.
$(BUILD_DIR)/$(ASM_DIR)/8EC50.o: src/libkmc/mmuldi3.s
	tools/cc/as -EB -mips2 -o $@.tmp $<
	cp $@.tmp $@
	rm $@.tmp
