MAKEFLAGS += --no-builtin-rules

# Run recipes under bash with pipefail. The asm rules pipe $(CPP) into $(AS); a
# missing $(CPP) would feed empty input to $(AS), which "succeeds" and emits a
# silent 0-byte object. pipefail fails the whole pipe instead.
SHELL := /bin/bash
.SHELLFLAGS := -o pipefail -c

# A failed recipe must not leave a half-written target behind: a truncated object
# with a fresh timestamp would be treated as up to date and silently break the ROM.
.DELETE_ON_ERROR:

# The build cannot run without the ROM it reproduces.
ifeq (,$(wildcard baserom.z64))
$(error baserom.z64 not found. Place your ROM in the project root.)
endif

BASENAME := mariogolf64
TARGET := $(BASENAME).z64
LD_SCRIPT := $(BASENAME).ld

# NONMATCHING compiles WIP C and skips the md5 check.
NONMATCHING ?= 0

# Directories. NONMATCHING objects build into a separate tree so a WIP build never
# poisons the matching build/ that the md5 check and verify-rom.sh read.
BUILD_DIR := build$(if $(filter 1,$(NONMATCHING)),/nonmatching)
ASM_DIR := asm
ASSETS_DIR := assets
SRC_DIR := src

# Verbosity: `make V=1` echoes full command lines; the default prints one terse
# status line per artifact and runs the command quietly.
V ?= 0
ifeq ($(V),0)
  Q := @
else
  Q :=
endif

# Toolchain: system binutils assembles the splat-extracted asm; KMC GCC 2.7.2 +
# KMC binutils 2.6 compile C (downloaded into tools/cc by `make -C tools`).
MIPS_BINUTILS_PREFIX ?= mips-linux-gnu-
AS      := $(MIPS_BINUTILS_PREFIX)as
LD      := $(MIPS_BINUTILS_PREFIX)ld
OBJCOPY := $(MIPS_BINUTILS_PREFIX)objcopy
STRIP   := $(MIPS_BINUTILS_PREFIX)strip
CPP     := $(MIPS_BINUTILS_PREFIX)cpp

# Fail early and clearly if the asm preprocessor is missing, rather than leaving
# pipefail to catch the empty-pipe case later.
ifeq (,$(shell command -v $(CPP) 2>/dev/null))
$(error $(CPP) not found; install it, e.g. `sudo apt install cpp-mips-linux-gnu`)
endif

# KMC_PREFIX is overridable for a relocated toolchain. COMPILER_PATH points GCC at
# its runtime support libs in tools/cc.
KMC_PREFIX ?= tools/cc
CC    := COMPILER_PATH=$(KMC_PREFIX) $(KMC_PREFIX)/gcc
KMC_AS := $(KMC_PREFIX)/as

# Base flags. The per-tree C profiles (libultra/libkmc/libnusys) live in mk/*.mk
# and override C_PROFILE_CFLAGS for their output paths.
ASFLAGS := -march=vr4300 -32 -I include --no-pad-sections
CPPFLAGS := -fno-dollars-in-identifiers -P
AS_DEFINES := -DMIPSEB -D_LANGUAGE_ASSEMBLY -D_ULTRA64
CFLAGS := -G 0 -mips3 -mgp32 -mfp32 -mno-abicalls -O2 -I include -I include/libultra -I include/libultra/internal -I include/libkmc -I include/libnusys -DINCLUDE_ASM_USE_MACRO_INC -D_LANGUAGE_C -D_FINALROM
ifeq ($(NONMATCHING),1)
CFLAGS += -DNONMATCHING
endif

# The default C profile; mk/lib*.mk override it per output tree.
C_PROFILE_CFLAGS = $(CFLAGS)

# Shared section-alignment flags for objcopy, and the modern-GAS assemble recipe
# used by both the splat-extracted asm/ and the generic src/ entry stub.
OBJCOPY_ALIGN := --set-section-alignment .text=4 --set-section-alignment .data=4 --set-section-alignment .rodata=4 --set-section-alignment .bss=4

define assemble-modern
@echo '  AS      $<'
$(Q)$(CPP) $(CPPFLAGS) -I include $(AS_DEFINES) $< | $(AS) $(ASFLAGS) -o $@.tmp
$(Q)$(OBJCOPY) $(OBJCOPY_ALIGN) $@.tmp $@
$(Q)rm $@.tmp
endef

# Default goal first.
.DEFAULT_GOAL := all
all: $(BUILD_DIR)/$(TARGET)
ifneq ($(NONMATCHING),1)
	@md5sum -c $(BASENAME).md5
endif

# Per-tree compile rules + flags.
include mk/asm.mk
include mk/assets.mk
include mk/libultra.mk
include mk/libkmc.mk
include mk/libnusys.mk
include mk/src.mk

# Source discovery. Recursive find, NOT $(wildcard): library/vendored code lives
# at nested paths (src/libultra/vi/*.c, src/libkmc/*.s, src/entry/entry.s).
# $(wildcard) is non-recursive and would silently drop them, surfacing only as
# link-time "undefined reference".
ASM_FILES := $(wildcard $(ASM_DIR)/*.s) $(wildcard $(ASM_DIR)/data/*.s)
BIN_FILES := $(wildcard $(ASSETS_DIR)/*.bin)
C_FILES := $(shell find $(SRC_DIR) -name '*.c')
SRC_ASM_FILES := $(shell find $(SRC_DIR) -name '*.s')

ASM_O_FILES := $(patsubst $(ASM_DIR)/%.s,$(BUILD_DIR)/$(ASM_DIR)/%.o,$(ASM_FILES))
BIN_O_FILES := $(patsubst $(ASSETS_DIR)/%.bin,$(BUILD_DIR)/$(ASSETS_DIR)/%.o,$(BIN_FILES))
C_O_FILES := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/$(SRC_DIR)/%.o,$(C_FILES))
SRC_ASM_O_FILES := $(patsubst $(SRC_DIR)/%.s,$(BUILD_DIR)/$(SRC_DIR)/%.o,$(SRC_ASM_FILES))

O_FILES := $(ASM_O_FILES) $(BIN_O_FILES) $(C_O_FILES) $(SRC_ASM_O_FILES)

# Pre-create every build output dir at parse time (race-free under -j). The
# $(sort $(dir ...)) over the object lists is REQUIRED: nested sources write to
# build/src/<tree>/<dir>/, and that parent must exist or the assemble step fails
# on a missing .o.tmp path.
$(shell mkdir -p $(BUILD_DIR)/$(ASM_DIR)/data $(BUILD_DIR)/$(ASSETS_DIR) $(BUILD_DIR)/$(SRC_DIR) $(sort $(dir $(C_O_FILES)) $(dir $(SRC_ASM_O_FILES))))

# Link the objects into the ELF, then strip it to the raw ROM image.
$(BUILD_DIR)/$(TARGET): $(BUILD_DIR)/$(BASENAME).elf
	@echo '  ROM     $@'
	$(Q)$(OBJCOPY) -O binary $< $@

$(BUILD_DIR)/$(BASENAME).elf: $(O_FILES) $(LD_SCRIPT) undefined_funcs_auto.txt undefined_syms_auto.txt hardware_regs.txt extern_syms.txt
	@echo '  LD      $@'
	$(Q)$(LD) --no-check-sections --omagic --allow-multiple-definition -T hardware_regs.txt -T undefined_funcs_auto.txt -T undefined_syms_auto.txt -T extern_syms.txt -T $(LD_SCRIPT) -Map $(BUILD_DIR)/$(BASENAME).map -o $@

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

# Characterization suite locking tools/*.py behavior. REGEN_GOLDEN=1 rewrites the
# golden snapshots after an intended behavior change.
test-tools:
	./venv/bin/python3 -m pytest tests/tooling

check: test-tools

# Fingerprint MG64's functions against ultralib VERSION_J and write
# tools/coddog/coddog_map.tsv, which pick_target.py reads to re-price
# none-classified verbatim mirrors. Needs a fresh `make`, a built ultralib, and
# coddog; the map is gitignored, so absent it leaves pick_target ranking unchanged.
coddog-sweep:
	./tools/coddog_sweep.sh

sync-names:
	-GHIDRA_REPO="$${MARIOGOLF64_GHIDRA_REPO:-$$HOME/development/reversing/ghidra/mariogolf64}"; \
	  SYNC_PY="$$GHIDRA_REPO/venv/bin/python3"; \
	  [ -x "$$SYNC_PY" ] || SYNC_PY=./venv/bin/python3; \
	  [ -x "$$SYNC_PY" ] || SYNC_PY=python3; \
	  "$$SYNC_PY" "$$GHIDRA_REPO/scripts/sync_decomp_names.py" \
	    --export-to-decomp --decomp-root . --write-in-place

help:
	@echo 'Targets:'
	@echo '  all (default)    build + verify the matching ROM'
	@echo '  extract          re-split the baserom with splat'
	@echo '  clean            remove the build tree'
	@echo '  distclean        also remove extracted asm/assets + generated scaffold'
	@echo '  setup            create venv + download the KMC toolchain'
	@echo '  test-tools/check run the tooling characterization suite'
	@echo '  coddog-sweep     fingerprint vs ultralib VERSION_J'
	@echo '  sync-names       import curated names from the Ghidra workspace'
	@echo 'Variables: V=1 (full commands), NONMATCHING=1 (WIP C, skip md5).'

# Per-function compile of nonmatchings/$(FUNC)/base.c into current.o, driven by
# tools/decomp_loop.py. base.c has no INCLUDE_ASM, so a single-step `gcc -c`
# mirrors what decomp.me produces. Always defines NONMATCHING.
nonmatching-func:
	@test -n "$(FUNC)" || { echo "Usage: make nonmatching-func FUNC=<func_XXXXXXXX> [LIBKMC=1|LIBULTRA=1]" >&2; exit 1; }
	@test -f nonmatchings/$(FUNC)/base.c || { echo "nonmatchings/$(FUNC)/base.c not found; run tools/seed_c.py first" >&2; exit 1; }
	$(CC) -c $(if $(LIBKMC),$(LIBKMC_CFLAGS),$(if $(LIBULTRA),$(LIBULTRA_CFLAGS),$(CFLAGS))) -DNONMATCHING -o nonmatchings/$(FUNC)/current.o nonmatchings/$(FUNC)/base.c

# In-tree spot-check build. Compiles src/$(SEG).c.spotcheck (the parent with the
# target function's INCLUDE_ASM swapped for its candidate C body under NONMATCHING)
# into build/src/$(SEG).spotcheck.o. Two-step because the source still carries the
# parent's other INCLUDE_ASM directives. The loop's finalize step creates and
# removes the .spotcheck file; this target only builds it.
spotcheck-build:
	@test -n "$(SEG)" || { echo "Usage: make spotcheck-build SEG=<seg> FUNC=<func_XXXXXXXX>" >&2; exit 1; }
	@test -n "$(FUNC)" || { echo "Usage: make spotcheck-build SEG=<seg> FUNC=<func_XXXXXXXX>" >&2; exit 1; }
	@test -f src/$(SEG).c.spotcheck || { echo "src/$(SEG).c.spotcheck not found (created during the execution loop's finalize step)" >&2; exit 1; }
	$(CC) -x c -S $(CFLAGS) -DNONMATCHING -o $(BUILD_DIR)/$(SRC_DIR)/$(SEG).spotcheck.s src/$(SEG).c.spotcheck
	$(KMC_AS) -EB -mips2 -I include -o $(BUILD_DIR)/$(SRC_DIR)/$(SEG).spotcheck.tmp $(BUILD_DIR)/$(SRC_DIR)/$(SEG).spotcheck.s
	cp $(BUILD_DIR)/$(SRC_DIR)/$(SEG).spotcheck.tmp $(BUILD_DIR)/$(SRC_DIR)/$(SEG).spotcheck.o
	$(STRIP) $(BUILD_DIR)/$(SRC_DIR)/$(SEG).spotcheck.o -N dummy-symbol-name
	rm $(BUILD_DIR)/$(SRC_DIR)/$(SEG).spotcheck.s $(BUILD_DIR)/$(SRC_DIR)/$(SEG).spotcheck.tmp

# Wipe per-function scratch dirs; leaves the build tree alone.
clean-nonmatchings:
	rm -rf nonmatchings/*/

.PHONY: all clean distclean setup extract test-tools check coddog-sweep sync-names help nonmatching-func spotcheck-build clean-nonmatchings
