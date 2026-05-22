MAKEFLAGS += --no-builtin-rules

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

# Compiler - KMC GCC 2.7.2
CC := COMPILER_PATH=tools/cc tools/cc/gcc

# Flags
ASFLAGS := -march=vr4300 -32 -I include --no-pad-sections
CPPFLAGS := -fno-dollars-in-identifiers -P
AS_DEFINES := -DMIPSEB -D_LANGUAGE_ASSEMBLY -D_ULTRA64
CFLAGS := -nostdinc -G 0 -mips2 -mgp32 -mfp32 -mno-abicalls -O2 -I include -DINCLUDE_ASM_USE_MACRO_INC -D_LANGUAGE_C
ifeq ($(NONMATCHING),1)
CFLAGS += -DNONMATCHING
endif

# Directories
SRC_DIR := src

# Find all source files
ASM_FILES := $(wildcard $(ASM_DIR)/*.s) $(wildcard $(ASM_DIR)/data/*.s)
BIN_FILES := $(wildcard $(ASSETS_DIR)/*.bin)
C_FILES := $(wildcard $(SRC_DIR)/*.c)

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
	@tools/mcp_lock.py acquire --command sync-names --identifier export
	-GHIDRA_REPO="$${MARIOGOLF64_GHIDRA_REPO:-$$HOME/development/reversing/ghidra/mariogolf64}"; \
	  SYNC_PY="$$GHIDRA_REPO/venv/bin/python3"; \
	  [ -x "$$SYNC_PY" ] || SYNC_PY=./venv/bin/python3; \
	  [ -x "$$SYNC_PY" ] || SYNC_PY=python3; \
	  "$$SYNC_PY" "$$GHIDRA_REPO/scripts/sync_decomp_names.py" \
	    --export-to-decomp --decomp-root . --write-in-place
	@tools/mcp_lock.py release --identifier export

.PHONY: all clean distclean setup extract sync-names nonmatching-func spotcheck-build clean-nonmatchings

# Create build directories
$(shell mkdir -p $(BUILD_DIR)/$(ASM_DIR)/data $(BUILD_DIR)/$(ASSETS_DIR) $(BUILD_DIR)/$(SRC_DIR))

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

# === /decomp loop targets =====================================================
# Driven by tools/decomp_loop.py inside the iteration loop, and by /decomp's
# in-tree spot-check step. Mirrors decomp.me's KMC GCC compile so the resulting
# object reflects what decomp.me would produce (single-step `gcc -c`).

# Per-function compile of nonmatchings/$(FUNC)/base.c -> nonmatchings/$(FUNC)/current.o.
# base.c has no INCLUDE_ASM directives, so single-step is safe (no need to route
# through system `as`). Always defines NONMATCHING.
nonmatching-func:
	@test -n "$(FUNC)" || { echo "Usage: make nonmatching-func FUNC=<func_XXXXXXXX>" >&2; exit 1; }
	@test -f nonmatchings/$(FUNC)/base.c || { echo "nonmatchings/$(FUNC)/base.c not found; run tools/seed_c.py first" >&2; exit 1; }
	$(CC) -c $(CFLAGS) -DNONMATCHING -o nonmatchings/$(FUNC)/current.o nonmatchings/$(FUNC)/base.c

# In-tree spot-check build. Compiles src/$(SEG).c.spotcheck (a temporary copy
# of the parent with the target function's INCLUDE_ASM wrapped in
# `#ifndef NONMATCHING / INCLUDE_ASM / #else <C body> #endif`) with
# NONMATCHING=1 into build/src/$(SEG).spotcheck.o. Two-step compile because
# the spotcheck source inherits the parent's other INCLUDE_ASM directives.
# /decomp step 10 creates and removes the .spotcheck file; this target only
# builds it.
spotcheck-build:
	@test -n "$(SEG)" || { echo "Usage: make spotcheck-build SEG=<seg> FUNC=<func_XXXXXXXX>" >&2; exit 1; }
	@test -n "$(FUNC)" || { echo "Usage: make spotcheck-build SEG=<seg> FUNC=<func_XXXXXXXX>" >&2; exit 1; }
	@test -f src/$(SEG).c.spotcheck || { echo "src/$(SEG).c.spotcheck not found (created by /decomp step 10)" >&2; exit 1; }
	$(CC) -x c -S $(CFLAGS) -DNONMATCHING -o $(BUILD_DIR)/$(SRC_DIR)/$(SEG).spotcheck.s src/$(SEG).c.spotcheck
	tools/cc/as -EB -mips2 -I include -o $(BUILD_DIR)/$(SRC_DIR)/$(SEG).spotcheck.tmp $(BUILD_DIR)/$(SRC_DIR)/$(SEG).spotcheck.s
	cp $(BUILD_DIR)/$(SRC_DIR)/$(SEG).spotcheck.tmp $(BUILD_DIR)/$(SRC_DIR)/$(SEG).spotcheck.o
	$(STRIP) $(BUILD_DIR)/$(SRC_DIR)/$(SEG).spotcheck.o -N dummy-symbol-name
	rm $(BUILD_DIR)/$(SRC_DIR)/$(SEG).spotcheck.s $(BUILD_DIR)/$(SRC_DIR)/$(SEG).spotcheck.tmp

# Wipe per-function scratch dirs. Leaves the build dir alone.
clean-nonmatchings:
	rm -rf nonmatchings/*/
