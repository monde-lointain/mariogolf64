# Mario Golf (North America) matching decompilation

TARGET      := mariogolf64
SPLAT_YAML  := $(TARGET).yaml
# The original ROM, which splat reads (the yaml's target_path)
BASEROM     := baserom.us.z64
SPLAT_DIR   := build/splat
SPLAT_LD    := $(SPLAT_DIR)/$(TARGET).ld
# The objects that splat's script names, as a makefile that sets SPLAT_O_FILES
SPLAT_O_LIST := $(SPLAT_DIR)/objects.mk
KMC_DIR     := tools/gcc_kmc

# Set NON_MATCHING=1 to build a ROM that needn't match the original: it
# avoids the original's undefined behavior and is laid out to let code and
# data move (see the layouts below).
NON_MATCHING ?= 0
# Set MODERN=1 to build with a modern GCC instead of KMC GCC 2.7.2. Its code
# differs in size from the original's, so its ROM doesn't match either.
MODERN      ?= 0
ifneq ($(MODERN),0)
override NON_MATCHING := 1
endif
# Set COMPARE=0 to skip the checksum comparison at the end of the build
COMPARE      ?= 1
ifneq ($(NON_MATCHING),0)
override COMPARE := 0
endif
# libdragon's toolchain, built for the VR4300 with an o32 multilib, and its
# own binutils, whose linker takes its objects' flags
N64_INST    ?= /opt/libdragon
MODERN_CROSS := $(N64_INST)/bin/mips64-elf-
MODERN_CC   := $(MODERN_CROSS)gcc
# The code was written for a compiler that took it as it reads. Where it
# indexes past an array, such as a row of a 2D array taken as the whole
# array, or overflows a signed integer, modern GCC would assume it doesn't
# and cut loops short or drop checks. These keep KMC GCC's meaning.
KMC_SEMANTICS := -fno-aggressive-loop-optimizations -fwrapv -fno-delete-null-pointer-checks
# GNU C89 keeps the meaning of the sources' old-style declarations.
# -fno-common makes a second definition of a variable an error, where KMC
# GCC would merge it. -fno-toplevel-reorder keeps the statics that only
# included asm uses.
MODERN_TARGETFLAGS := -G0 -march=vr4300 -mabi=32 -mfix4300 -mno-abicalls -fno-PIC -std=gnu89 \
               -fno-common -ffreestanding -fno-strict-aliasing -mdivide-breaks \
               -fno-toplevel-reorder -fno-math-errno $(KMC_SEMANTICS)
# The helpers, such as 64-bit shifts, that modern GCC calls rather than
# inlines. It is only found when a link needs it.
MODERN_LIBS  = $(shell $(MODERN_CC) $(MODERN_TARGETFLAGS) -print-libgcc-file-name)

# The two toolchains, and what differs between their builds. Each build has
# its own directory, so that no build links another's objects.
ifeq ($(MODERN),0)
BUILD_DIR   := build$(if $(filter-out 0,$(NON_MATCHING)),/nonmatching)
CROSS       := mips-linux-gnu-
BIN_FORMAT  := elf32-tradbigmips
CC          := COMPILER_PATH=$(KMC_DIR) $(KMC_DIR)/gcc
# KMC GCC reads VR4300MUL from the environment to work around the VR4300
# multiplication bug, as ultralib's build does
LIB_CC      := VR4300MUL=ON $(CC)
TARGETFLAGS := -G0 -mips3 -mgp32 -mfp32 -mno-abicalls -fno-PIC
# Lays out statics the way the KMC assembler did, giving 8-byte alignment
# to anything of 8 bytes or more
KMC_ALIGN   := -Wa,--force-n64align
# Lets the sources keep the original's quirks that only KMC GCC accepts
GAME_DEFINES := -D__IS_KMC__=1
# The game's code was built with -ffast-math
FAST_MATH   := -ffast-math
# KMC GCC checks the code with -Wall, less what the matching forms trip by
# design or what it checks worse than modern GCC:
#   -Wunused: statics that only the included assembly reads
#   -Wuninitialized: locals that the original reads unset, or that KMC GCC
#     can't tell are set
#   -Wreturn-type: the original's missing return values (NO_RESULT,
#     UNSET_RESULT)
#   -Wformat: the original's format strings, and s32, a long, passed to %d
#   -Wmissing-braces: the zero initializer {0}
# The modern build compiles the same functions, with drafts and real return
# types, and its GCC checks all of these but the formats, as it is
# freestanding and so doesn't know sprintf.
WARNINGS    ?= -Wall -Wno-unused -Wno-uninitialized -Wno-return-type -Wno-format \
               -Wno-missing-braces
ULTRALIB_COMPILER := gcc
# The ABI that ultralib's assembly tests for, which modern GCC defines itself
ULTRALIB_ASM_ABI := -D_MIPS_SIM=1
LIB_MODERN  :=
LIBS        :=
else
BUILD_DIR   := build/modern
CROSS       := $(MODERN_CROSS)
BIN_FORMAT  := elf32-bigmips
CC          := $(MODERN_CC)
LIB_CC      := $(CC)
TARGETFLAGS := $(MODERN_TARGETFLAGS)
KMC_ALIGN   :=
GAME_DEFINES :=
# Modern GCC's -ffast-math reorders the float math enough that the ball
# lands elsewhere, which throws the opening and the demos off their scripts
FAST_MATH   :=
# All of the code builds cleanly with these
WARNINGS    ?= -Wall -Wextra
# ultralib's own settings for modern GCC. The libraries also take its
# MODERN_CC where only KMC GCC builds the original's form as the original.
ULTRALIB_COMPILER := modern_gcc
LIB_MODERN  := -DMODERN_CC
ULTRALIB_ASM_ABI :=
LIBS        := $(MODERN_LIBS)
endif

# main_bss.c only pins main's common objects at the original's addresses.
# Without it, each object is placed with the module that defines it.
MATCHING_ONLY_O := build/src/system/main_bss.c.o
# The symbols that the objects use and none defines
SYMBOL_SCRIPTS := undefined_syms.txt

# The two layouts. The matching build places everything where the original
# has it, and gives the asm the original's addresses. The shiftable layout
# packs the overlays (see tools/shiftable_ld.py) and keeps only the symbols
# outside the original's segments (see tools/fixed_syms.py), so that a
# reference that holds only at the original's addresses fails to link.
SHIFTABLE_LD      := $(BUILD_DIR)/shiftable.ld
SHIFTABLE_SYMBOLS := $(BUILD_DIR)/fixed_syms.ld
ifeq ($(NON_MATCHING),0)
LD_SCRIPTS  := $(SPLAT_LD) $(SYMBOL_SCRIPTS)
LINKED_O_FILTER :=
else
LD_SCRIPTS  := $(SHIFTABLE_LD) $(SHIFTABLE_SYMBOLS)
LINKED_O_FILTER := $(MATCHING_ONLY_O)
endif

ROM         := $(BUILD_DIR)/$(TARGET).z64
# The cartridge's size, which 0xFF fills after the last asset
ROM_SIZE    := 0x2000000
# Where the asset archive's assets are split to, one file each
ARCHIVE_DIR := assets/asset_archive
# The files of messages that are built from text, a message a line
TEXT_FILES  := assets/text/messages assets/text/staff_roll
ELF         := $(BUILD_DIR)/$(TARGET).elf
MAP         := $(BUILD_DIR)/$(TARGET).map

AS          := $(CROSS)as
LD          := $(CROSS)ld
OBJCOPY     := $(CROSS)objcopy
PYTHON      := .venv/bin/python3

# Each library's headers are in its own include directory. NuSystem's is
# included through its converted copy, as the rule below explains. The
# game reaches libultra's PR/ headers through <ultra64.h>, so PR/ isn't
# searched, and none of its headers, such as PR/rcp.h, can stand in for a
# game header of the same name.
INCLUDES    := -Iinclude -Ilib/libkmc/include -Ilib/ultralib/include \
               -Ilib/libmus/include -I$(BUILD_DIR)/lib/libnusys/include -Ilib/libnusys/include -Isrc -I$(BUILD_DIR)
DEFINES     := -D_LANGUAGE_C -D_MIPS_SZLONG=32 -D_MIPS_SZINT=32 -DF3DEX_GBI_2 -DNDEBUG -D_FINALROM -DBUILD_VERSION=VERSION_J \
               $(GAME_DEFINES)
# A ROM that doesn't match moves the code and data, so it must also avoid
# the original's undefined behavior, such as accesses past the end of an
# object, which only hit what happened to follow it
ifneq ($(NON_MATCHING),0)
DEFINES     += -DNON_MATCHING -DAVOID_UB
# nusys.h sizes the main thread's stack, which the game overruns, for both
LIB_AVOID_UB := -DAVOID_UB
endif

# Each compile lists the headers it read in a .d file beside its object, so
# that changing a header rebuilds what includes it. Both compilers'
# preprocessors take the file and target from SUNPRO_DEPENDENCIES, KMC's
# having no -MF or -MT. Unlike DEPENDENCIES_OUTPUT, it also lists headers
# included with angle brackets, such as NuSystem's. KMC's appends to the
# file, so the old list goes first.
DEPS         = rm -f $(@:.o=.d); SUNPRO_DEPENDENCIES='$(@:.o=.d) $@'

CFLAGS       = $(TARGETFLAGS) $(FAST_MATH) $(KMC_ALIGN) -nostdinc -funsigned-char $(WARNINGS) $(OPTFLAGS)
ASFLAGS     := -march=vr4300 -mabi=32 -G0 -EB -no-pad-sections -Iinclude
# ld lays out each object's common symbols in the order of its symbol hash
# table's buckets. The table grows with the symbol count unless it starts big
# enough, so fix its size to keep the commons where the names put them.
LD_HASH_SIZE := 8191
LAYOUT_FLAGS   := --no-check-sections --hash-size=$(LD_HASH_SIZE) -G 0
LDFLAGS         = $(addprefix -T ,$(LD_SCRIPTS)) -Map $(MAP) $(LAYOUT_FLAGS)

# Host-side syntax checking of C sources with clang, pretending to be 32-bit MIPS
CC_CHECK       := clang -fsyntax-only -fno-builtin -funsigned-char -std=gnu89 -m32 -nostdinc \
                  -D_MIPS_SZLONG=32 -D_MIPS_SZINT=32 -DCC_CHECK=1 -Wall -Wno-unknown-pragmas \
                  -Wno-unused-variable -Wno-unused-but-set-variable -Wno-missing-braces
RUN_CC_CHECK   ?= 0

# Every object the linker script asks for, under this build's directory.
# When a change to the yaml makes the list again, make reads it in afresh
# before building anything, so a file added to the yaml is compiled.
-include $(SPLAT_O_LIST)
O_FILES := $(patsubst build/%,$(BUILD_DIR)/%,$(filter-out $(LINKED_O_FILTER),$(SPLAT_O_FILES)))

# libultra 2.0J, built the way ultralib builds libgultra_rom except that the
# game's copy was compiled with signed chars
ULTRALIB_DIR      := lib/ultralib
ULTRALIB_INCLUDES := -I$(ULTRALIB_DIR)/include -I$(ULTRALIB_DIR)/include/compiler/$(ULTRALIB_COMPILER) -I$(ULTRALIB_DIR)/include/PR
ULTRALIB_DEFINES  := $(LIB_MODERN) -D_MIPS_SZLONG=32 -D__USE_ISOC99 -DF3DEX_GBI -DBUILD_VERSION=VERSION_J \
                     -DBUILD_VERSION_STRING=\"2.0J\" -DNDEBUG -D_FINALROM
ULTRALIB_CFLAGS    = $(WARNINGS) -nostdinc $(TARGETFLAGS) -D_LANGUAGE_C $(OPTFLAGS)
ULTRALIB_ASFLAGS  := -x assembler-with-cpp -nostdinc $(TARGETFLAGS) -P \
                     -DMIPSEB -D_LANGUAGE_ASSEMBLY $(ULTRALIB_ASM_ABI) -D_ULTRA64

# libmus, the n_audio build of the Software Creations music library, as its
# makefile built libn_gmus.
LIBMUS_DIR        := lib/libmus
LIBMUS_INCLUDES   := -I$(LIBMUS_DIR)/src -I$(LIBMUS_DIR)/include $(ULTRALIB_INCLUDES)
LIBMUS_DEFINES    := $(LIB_MODERN) -D_MIPS_SZLONG=32 -D_MIPS_SZINT=32 -DF3DEX_GBI -DNDEBUG -DSUPPORT_NAUDIO
LIBMUS_CFLAGS      = $(WARNINGS) -nostdinc $(TARGETFLAGS) -D_LANGUAGE_C $(OPTFLAGS) $(KMC_ALIGN)

# n_audio, built from the SDK's n_audio_sc sources for the n_aspMain
# microcode as other games of the time built it
LIBNAUDIO_DIR      := lib/libnaudio
LIBNAUDIO_INCLUDES := -I$(LIBNAUDIO_DIR)/src -I$(LIBNAUDIO_DIR)/include $(ULTRALIB_INCLUDES)
LIBNAUDIO_DEFINES  := $(LIB_MODERN) -D_MIPS_SZLONG=32 -D_MIPS_SZINT=32 -DF3DEX_GBI -DNDEBUG -DN_MICRO
LIBNAUDIO_CFLAGS    = $(LIBMUS_CFLAGS)

# NuSystem, with the defines of the SDK makefile's ROM library. The
# scheduler is built with NU_DEBUG, which keeps its task performance
# measurements. The sources keep the SDK's CRLF line
# endings, and KMC's preprocessor doesn't join lines that end in a backslash
# and a carriage return, so they're compiled from copies without the CRs.
LIBNUSYS_DIR       := lib/libnusys
LIBNUSYS_INCLUDES  := -I$(BUILD_DIR)/$(LIBNUSYS_DIR)/include -I$(LIBNUSYS_DIR)/include $(ULTRALIB_INCLUDES)
LIBNUSYS_DEFINES   := $(LIB_MODERN) $(LIB_AVOID_UB) -D_MIPS_SZLONG=32 -D_MIPS_SZINT=32 -DUSE_EPI -DBUILD_VERSION=VERSION_J -D_FINALROM
LIBNUSYS_CFLAGS     = $(WARNINGS) -nostdinc $(TARGETFLAGS) -D_LANGUAGE_C $(OPTFLAGS) $(KMC_ALIGN)

# libkmc, KMC's C library, built like the game's code except without
# -ffast-math, which turns divisions by constants into multiplications
LIBKMC_DIR := lib/libkmc
$(BUILD_DIR)/$(LIBKMC_DIR)/%.o: FAST_MATH :=

# The matching build gives each part of the game the optimization the
# original's was built with. The modern build optimizes everything for size.
ifeq ($(MODERN),0)
OPTFLAGS := -O2
$(BUILD_DIR)/$(ULTRALIB_DIR)/%.o: OPTFLAGS := -O3
$(BUILD_DIR)/$(LIBMUS_DIR)/%.o: OPTFLAGS := -O3
$(BUILD_DIR)/$(LIBNAUDIO_DIR)/%.o: OPTFLAGS := -O3
# Like the SDK's nusys.o, NuSystem's boot code is built without optimization
$(BUILD_DIR)/$(LIBNUSYS_DIR)/src/nuboot.c.o: OPTFLAGS :=
$(BUILD_DIR)/$(LIBKMC_DIR)/%.o: OPTFLAGS := -O1
else
# The modern build keeps every routine and object, used or not. Collecting
# unreferenced sections (-ffunction-sections -fdata-sections --gc-sections)
# would save about 55 KB of code and 80 KB of base RAM, which buys nothing
# while the heap keeps the original's size; and the collector drops every
# segment reached only through its ROM address, such as IPL3, the assets,
# the sound banks and the build date, unless the script keeps each one.
OPTFLAGS := -Os
endif

# Per-object compiler flags
$(BUILD_DIR)/$(LIBNUSYS_DIR)/src/nusched.c.o: LIBNUSYS_DEFINES += -DNU_DEBUG

.PHONY: all extract clean distclean setup tools format lint typecheck pycheck check-shiftable \
        progress equivalence test target-tests

# A recipe that fails after writing its target, such as a check of what it
# wrote, would otherwise leave a target that the next run takes as built
.DELETE_ON_ERROR:

all: $(ROM)
ifneq ($(COMPARE),0)
	@sha1sum -c $(TARGET).sha1
endif

extract:
	$(PYTHON) -m splat split $(SPLAT_YAML)
	$(PYTHON) tools/extract_textures.py
	$(PYTHON) tools/asset_archive.py split assets/asset_archive.bin $(ARCHIVE_DIR)
	for text in $(TEXT_FILES); do \
		$(PYTHON) tools/messages.py decode $$text.bin $$text.txt || exit; \
	done

setup: tools extract

CLANG_FORMAT ?= clang-format-23
FORMAT_FILES := $(shell git ls-files 'src/*.c' 'src/*.h' 'include/*.h' '$(LIBKMC_DIR)/*.[ch]')

# The tests are formatted too, but not Unity, which is kept as upstream has
# it, and they aren't linted, as the lint's compile commands are the game's
TEST_FORMAT_FILES := $(wildcard tests/*.c tests/target/*.c tests/target/*.h)

format:
	$(CLANG_FORMAT) -i $(FORMAT_FILES) $(TEST_FORMAT_FILES)
	.venv/bin/ruff format --quiet

# Lints and type-checks the Python tools, with the settings in pyproject.toml
pycheck:
	.venv/bin/ruff format --check --quiet
	.venv/bin/ruff check --quiet
	.venv/bin/basedpyright --level warning
	.venv/bin/vulture

# Measures the matching build against the drafts' build and draws the
# progress map in build/progress. Each run adds the measured commit to the
# history kept there, which follows tools/progress_seed.json, the commits
# before the first run (see tools/progress_seed.py).
PROGRESS_DIR := build/progress
DRAFTS_DIR   := build/nonmatching
PROGRESS_O_FILES := $(filter $(BUILD_DIR)/src/% $(BUILD_DIR)/lib/%,$(O_FILES))

progress: $(ROM)
	$(MAKE) NON_MATCHING=1
	@mkdir -p $(PROGRESS_DIR)
	$(PYTHON) tools/progress_report.py $(PROGRESS_DIR) $(DRAFTS_DIR) \
	    $(PROGRESS_O_FILES)
	$(PYTHON) tools/progress_map.py $(PROGRESS_DIR)/report.json -o $(PROGRESS_DIR) \
	    --regs $(PROGRESS_DIR)/regs.json --history tools/progress_seed.json \
	    --history $(PROGRESS_DIR)/history.json \
	    --commit $$(git rev-parse HEAD) --date $$(git log -1 --format=%cI)

# Links this build's objects in the shiftable layout, which fails on each
# reference that holds only while the code and data keep their original
# addresses. The shiftable builds link that way already.
check-shiftable: $(O_FILES) $(SHIFTABLE_LD) $(SHIFTABLE_SYMBOLS)
	$(LD) $(addprefix -T ,$(SHIFTABLE_LD) $(SHIFTABLE_SYMBOLS)) -Map $(BUILD_DIR)/shiftable.map \
	    $(LAYOUT_FLAGS) -o $(BUILD_DIR)/shiftable.elf
	@$(PYTHON) tools/check_link.py $(BUILD_DIR)/shiftable.elf $(SHIFTABLE_LD) $(SHIFTABLE_SYMBOLS) -- $(O_FILES)

# Runs the matching ROM and the modern build's side by side in ares's
# n64-harness, which N64_HARNESS names, and compares the game's state at
# each of EQUIVALENCE_FRAMES frames (see tools/equivalence.py)
EQUIVALENCE_FRAMES ?= 3000

equivalence: all
	$(MAKE) MODERN=1
	$(PYTHON) tools/equivalence.py --frames $(EQUIVALENCE_FRAMES)

# The unit tests in tests/, which run on the console with Unity. Each build
# links them with its own objects into a test ROM, which boots into them
# instead of the game (see tests/target/). Both builds compile the tests
# with modern GCC, so the same tests face each toolchain's code. A build
# that links the game in the original's layout links its tests in the
# shiftable layout, which leaves room for them.
TEST_DIR      := $(BUILD_DIR)/tests
TEST_ELF      := $(TEST_DIR)/tests.elf
TEST_ROM      := $(TEST_DIR)/tests.z64
TEST_LD       := $(TEST_DIR)/tests.ld
TEST_SOURCES  := $(wildcard tests/*.c) tests/target/runner.c tests/target/support.c \
                 tests/unity/unity.c tests/unity/unity_fixture.c
TEST_O_FILES  := $(TEST_SOURCES:%=$(BUILD_DIR)/%.o)
# The boot stub goes into main's code, which takes it from an archive
TEST_BOOT     := $(TEST_DIR)/boot.a
# The tests see the declarations of the build they test, but not KMC GCC's
# forms, as modern GCC compiles them. Unity needs the compiler's own
# headers, such as stdarg.h, which the game's code doesn't include.
TEST_CFLAGS    = $(MODERN_TARGETFLAGS) -Os -nostdinc -funsigned-char -Wall -Wextra \
                 -isystem $(shell $(MODERN_CC) -print-file-name=include)
TEST_INCLUDES := -Itests/target -Itests/unity $(INCLUDES)
TEST_DEFINES  := $(filter-out $(GAME_DEFINES),$(DEFINES)) -DUNITY_INCLUDE_CONFIG_H

# Runs the tests against both toolchains' code, once the matching ROM has
# matched the original
test: all target-tests
	$(MAKE) MODERN=1 target-tests

# Runs this build's test ROM in ares's n64-harness, which N64_HARNESS names
target-tests: $(TEST_ROM)
	$(PYTHON) tools/run_target_tests.py $(TEST_ELF) $<

$(BUILD_DIR)/tests/%.c.o: tests/%.c | $(BUILD_DIR)/$(LIBNUSYS_DIR)/include/nusys.h
	@mkdir -p $(dir $@)
	$(DEPS) $(MODERN_CC) -c $(TEST_CFLAGS) $(TEST_INCLUDES) $(TEST_DEFINES) -o $@ $<

$(TEST_BOOT): $(TEST_DIR)/target/boot.c.o
	rm -f $@
	$(MODERN_CROSS)ar rcs $@ $<

# The shiftable layout with the tests' segment before the sections that it
# discards
$(TEST_LD): $(SHIFTABLE_LD) tests/target/segment.ld
	awk -v segment=tests/target/segment.ld \
	    '/^    \/DISCARD\/ :$$/ { while ((getline line < segment) > 0) print line } { print }' \
	    $< > $@

# NuSystem's boot starts the main thread in the boot stub instead of
# mainproc. Modern GCC's linker reads both toolchains' objects.
$(TEST_ELF): $(O_FILES) $(TEST_O_FILES) $(TEST_BOOT) $(TEST_LD) $(SHIFTABLE_SYMBOLS)
	$(MODERN_CROSS)ld $(addprefix -T ,$(TEST_LD) $(SHIFTABLE_SYMBOLS)) -Map $(TEST_DIR)/tests.map \
	    $(LAYOUT_FLAGS) --wrap=mainproc -o $@ $(TEST_O_FILES) $(TEST_BOOT) $(MODERN_LIBS)
	@$(PYTHON) tools/check_link.py $@ $(TEST_LD) $(SHIFTABLE_SYMBOLS) -- $(O_FILES)

CLANG_TIDY ?= run-clang-tidy-23
# libkmc is KMC's C library, which keeps the C library's names
LINT_FILES := $(filter-out $(LIBKMC_DIR)/%,\
	$(filter %.c,$(FORMAT_FILES)))

$(BUILD_DIR)/compile_commands.json: tools/compile_commands.py Makefile
	@mkdir -p $(BUILD_DIR)
	@python3 tools/compile_commands.py $@ $(INCLUDES) $(DEFINES) -- $(LINT_FILES)

LINT_JOBS ?= 4

# clang can't parse everything GCC 2.7 accepts, such as nested functions,
# so only the naming and macro diagnostics fail the check, each printed
# once though a header's repeats for every file that includes it. The
# rest stay in the log.
LINT_CHECKS := readability-identifier-naming|bugprone-macro-parentheses

lint: $(BUILD_DIR)/compile_commands.json
	@$(CLANG_TIDY) -p $(BUILD_DIR) -j $(LINT_JOBS) -quiet $(LINT_FILES) \
		> $(BUILD_DIR)/lint.log 2>&1 || true
	@! grep -E '\[($(LINT_CHECKS))' $(BUILD_DIR)/lint.log | sort -u | grep .

# Linking the game's and the libraries' code as LTO bytecode compares each
# declaration of a routine or object with its definition, across files,
# which no single compile can, including the libraries' declarations of
# the game's routines. The modern build compiles the bytecode into its own
# directory, and any mismatch fails the link.
TYPECHECK_DIR := build/typecheck
# The objects compiled from C, leaving out the data-only assembly that
# splat names as C (see the ultralib rule for those)
C_O_FILES = $(foreach object,$(filter %.c.o,$(O_FILES)), \
  $(if $(wildcard $(patsubst $(BUILD_DIR)/%.c.o,%.c,$(object))),$(object)))

typecheck:
	@$(MAKE) MODERN=1 BUILD_DIR=$(TYPECHECK_DIR) OPTFLAGS='-Os -flto' \
		$(TYPECHECK_DIR)/linked.o

$(BUILD_DIR)/linked.o: $(C_O_FILES)
	$(CC) -r -nostdlib $(TARGETFLAGS) $(OPTFLAGS) \
		-Werror=lto-type-mismatch -o $@ $^

tools: $(KMC_DIR)/gcc $(KMC_DIR)/as

$(KMC_DIR)/gcc:
	@mkdir -p $(KMC_DIR)
	wget -q -O - https://github.com/decompals/mips-gcc-2.7.2/releases/latest/download/gcc-2.7.2-linux.tar.gz | tar xz -C $(KMC_DIR)

$(KMC_DIR)/as:
	@mkdir -p $(KMC_DIR)
	wget -q -O - https://github.com/decompals/mips-binutils-2.6/releases/latest/download/binutils-2.6-linux.tar.gz | tar xz -C $(KMC_DIR)

clean:
	rm -rf $(BUILD_DIR)/asm $(BUILD_DIR)/src $(BUILD_DIR)/assets $(BUILD_DIR)/lib $(ROM) $(ELF) $(TEST_DIR)

distclean:
	rm -rf $(BUILD_DIR) asm assets

# splat writes the linker script from the yaml, so that a change to the
# layout, such as a new file, needs no make extract, which would write the
# extracted assets over any edits. Its ld mode leaves them be, apart from
# the microcode, which splat always cuts from the ROM again. It writes the
# script only if it changes, so the touch keeps an edit that changes none
# from running it at every make. Without the ROM, which splat reads, make
# has no way to make the script, and goals that need no link, such as
# pycheck, go on without the list of objects.
$(SPLAT_LD): $(SPLAT_YAML) symbol_addrs.txt reloc_addrs.txt $(BASEROM)
	mkdir -p $(SPLAT_DIR)
	$(PYTHON) -m splat split $(SPLAT_YAML) --modes ld > $(SPLAT_DIR)/split.log 2>&1 \
	    || { cat $(SPLAT_DIR)/split.log; exit 1; }
	touch $@

$(SPLAT_O_LIST): $(SPLAT_LD)
	grep -oE 'build/[A-Za-z0-9_./-]+\.o' $< | LC_ALL=C sort -u | sed 's/^/SPLAT_O_FILES += /' > $@

# splat's script names the objects under build/, so the shiftable script
# names them under this build's directory
$(SHIFTABLE_LD): $(SPLAT_LD) tools/shiftable_ld.py
	@mkdir -p $(dir $@)
	sed -e '\|$(MATCHING_ONLY_O)|d' -e 's|build/|$(BUILD_DIR)/|g' $< | $(PYTHON) tools/shiftable_ld.py > $@

$(SHIFTABLE_SYMBOLS): $(SPLAT_YAML) $(SYMBOL_SCRIPTS) tools/fixed_syms.py
	@mkdir -p $(dir $@)
	$(PYTHON) tools/fixed_syms.py $(SPLAT_YAML) $(SYMBOL_SCRIPTS) > $@

# IPL3 boots only a ROM whose header holds the checksum of the code after it
$(ROM) $(TEST_ROM): %.z64: %.elf
	$(OBJCOPY) -O binary --gap-fill=0xFF --pad-to=$(ROM_SIZE) $< $@
	$(PYTHON) -m ipl3checksum sum --update $@ > /dev/null

# tools/check_link.py checks the layout for what ld doesn't: scripts that
# override an object's symbol, overlays that the game can't load together,
# and sections with contents that the scripts discard
$(ELF): $(O_FILES) $(LD_SCRIPTS)
	$(LD) $(LDFLAGS) -o $@ $(LIBS)
	@$(PYTHON) tools/check_link.py $@ $(LD_SCRIPTS) -- $(O_FILES)

$(BUILD_DIR)/%.s.o: %.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -o $@ $<

# The RSP loads its microcode by DMA from addresses aligned to 8 bytes. The
# SDK's microcode objects align it to 16, which the original's layout keeps.
$(BUILD_DIR)/asm/data/%.s.o: asm/data/%.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -o $@ $<
	$(OBJCOPY) --set-section-alignment .text=16 --set-section-alignment .data=16 $@

$(BUILD_DIR)/%.bin.o: %.bin
	@mkdir -p $(dir $@)
	$(OBJCOPY) -I binary -O $(BIN_FORMAT) -B mips $< $@

# The asset archive is packed from the assets that asset_archive.py splits
# it into, so that one can change without the table being worked out again
$(BUILD_DIR)/assets/asset_archive.bin: $(wildcard $(ARCHIVE_DIR)/*.bin) tools/asset_archive.py
	$(PYTHON) tools/asset_archive.py pack $(ARCHIVE_DIR) $@

# The text files are encoded from the text that messages.py decodes them to
$(TEXT_FILES:%=$(BUILD_DIR)/%.bin): $(BUILD_DIR)/%.bin: %.txt tools/messages.py tools/asset_archive.py
	$(PYTHON) tools/messages.py encode $< $@

$(BUILD_DIR)/assets/asset_archive.bin.o $(TEXT_FILES:%=$(BUILD_DIR)/%.bin.o): %.o: %
	$(OBJCOPY) -I binary -O $(BIN_FORMAT) -B mips $< $@

# Textures in C data come from PNGs that extract_textures.py writes to
# assets/SOURCE/, where SOURCE is the path of the C file under src/ that
# includes them
TEXTURE_PNGS := $(shell sed 's/\#.*//' textures.txt | awk 'NF { print $$5 }')

$(BUILD_DIR)/assets/%.inc.c: assets/%.png tools/png_to_c.py
	$(PYTHON) tools/png_to_c.py $< $@

$(foreach png,$(TEXTURE_PNGS),$(eval \
  $(patsubst assets/%/,$(BUILD_DIR)/src/%.c.o,$(dir $(png))): \
      $(BUILD_DIR)/$(png:.png=.inc.c)))

# The sources are UTF-8, but the game's strings are Shift-JIS. KMC GCC
# compiles a Shift-JIS copy, and the line marker keeps its messages
# pointing at the source. -I adds back the source's own directory, which
# GCC searches first for quoted includes.
ICONV := iconv -f UTF-8 -t SHIFT_JIS

$(BUILD_DIR)/%.c.o: %.c
	@mkdir -p $(dir $@)
ifneq ($(RUN_CC_CHECK),0)
	$(CC_CHECK) $(INCLUDES) $(DEFINES) $<
endif
	{ echo '# 1 "$<"'; $(ICONV) $<; } > $(BUILD_DIR)/$*.sjis.c
	$(DEPS) $(CC) -c $(CFLAGS) -I$(dir $<) $(INCLUDES) $(DEFINES) -o $@ $(BUILD_DIR)/$*.sjis.c

# main_bss.c defines libmus's bss objects too, whose types only its sources
# declare. Those include libultra's headers from PR/ by their bare names.
$(BUILD_DIR)/src/system/main_bss.c.o: INCLUDES += -I$(LIBMUS_DIR)/src -I$(ULTRALIB_DIR)/include/PR

$(BUILD_DIR)/$(ULTRALIB_DIR)/%.c.o: $(ULTRALIB_DIR)/%.c
	@mkdir -p $(dir $@)
	$(DEPS) $(LIB_CC) -c $(ULTRALIB_CFLAGS) $(ULTRALIB_INCLUDES) $(ULTRALIB_DEFINES) -o $@ $<

$(BUILD_DIR)/$(ULTRALIB_DIR)/%.s.o: $(ULTRALIB_DIR)/%.s
	@mkdir -p $(dir $@)
	$(DEPS) $(CC) -c $(ULTRALIB_ASFLAGS) $(ULTRALIB_INCLUDES) $(ULTRALIB_DEFINES) -o $@ $<

$(BUILD_DIR)/$(LIBMUS_DIR)/%.c.o: $(LIBMUS_DIR)/%.c
	@mkdir -p $(dir $@)
	$(DEPS) $(LIB_CC) -c $(LIBMUS_CFLAGS) $(LIBMUS_INCLUDES) $(LIBMUS_DEFINES) -o $@ $<

$(BUILD_DIR)/$(LIBNAUDIO_DIR)/%.c.o: $(LIBNAUDIO_DIR)/%.c
	@mkdir -p $(dir $@)
	$(DEPS) $(LIB_CC) -c $(LIBNAUDIO_CFLAGS) $(LIBNAUDIO_INCLUDES) $(LIBNAUDIO_DEFINES) -o $@ $<

$(BUILD_DIR)/$(LIBNUSYS_DIR)/%.c.o: $(LIBNUSYS_DIR)/%.c $(BUILD_DIR)/$(LIBNUSYS_DIR)/include/nusys.h
	@mkdir -p $(dir $@)
	tr -d '\r' < $< > $(@:.o=)
	$(DEPS) $(LIB_CC) -c $(LIBNUSYS_CFLAGS) $(LIBNUSYS_INCLUDES) $(LIBNUSYS_DEFINES) -o $@ $(@:.o=)

# Game code includes NuSystem's header through its converted copy, so the
# copy must exist before any game file compiles. The .d files make the
# files that include it rebuild when it changes.
$(filter $(BUILD_DIR)/src/%,$(O_FILES)): | $(BUILD_DIR)/$(LIBNUSYS_DIR)/include/nusys.h

$(BUILD_DIR)/$(LIBNUSYS_DIR)/include/nusys.h: $(LIBNUSYS_DIR)/include/nusys.h
	@mkdir -p $(dir $@)
	tr -d '\r' < $< > $@

# splat names data-only objects as if they were C, so assembly files that
# only hold data (such as gu/libm_vals.s) are found under a .c.o name
$(BUILD_DIR)/$(ULTRALIB_DIR)/%.c.o: $(ULTRALIB_DIR)/%.s
	@mkdir -p $(dir $@)
	$(DEPS) $(CC) -c $(ULTRALIB_ASFLAGS) $(ULTRALIB_INCLUDES) $(ULTRALIB_DEFINES) -o $@ $<

-include $(O_FILES:.o=.d) $(TEST_O_FILES:.o=.d) $(TEST_DIR)/target/boot.c.d
# A header that a .d file lists but that has since gone only forces the
# rebuild that writes a new list
%.h: ;
