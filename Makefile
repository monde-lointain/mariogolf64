# Mario Golf (North America) matching decompilation

TARGET      := mariogolf64
# What the ROM holds, in order, from which tools/layout.py writes the
# linker scripts
LAYOUT      := layout.txt
# The objects that the layout places, as a makefile that sets LAYOUT_OBJECTS
# and MATCHING_ONLY_OBJECTS to their sources' paths
LAYOUT_MK   := build/layout.mk
# What the linker scripts are written with
LAYOUT_TOOLS := tools/layout.py tools/heap_region.py include/system/heap.h
KMC_DIR     := tools/gcc_kmc

# Set NON_MATCHING=1 to build a ROM that needn't match the original: it
# avoids the original's undefined behavior and is laid out to let code and
# data move (see the layouts below).
NON_MATCHING ?= 0
# Set MODERN=1 to build with a modern GCC instead of KMC GCC 2.7.2. Its code
# differs in size from the original's, so its ROM doesn't match either.
MODERN      ?= 0
# Set SANITIZE=1 to build a modern ROM, in build/sanitize, whose code checks
# itself for what C leaves undefined and the layout or the compiler then
# decides: an index past an array, a shift by the width or more, a division
# by zero, and a float out of the integer's range that it becomes.
# tools/sanitize.py runs it and reports the checks that failed. The checks'
# code fits before the main heap only as LTO compiles it, so LTO=0,
# check-shiftable and the tests, which link the objects as each compiled,
# fail to link with it.
SANITIZE    ?= 0
ifneq ($(SANITIZE),0)
override MODERN := 1
endif
ifneq ($(MODERN),0)
override NON_MATCHING := 1
endif
# Set CHARACTERS to the package folders of custom characters, which the
# Blender add-on in tools/blender exports, to build each in place of the
# character it replaces. Their model files differ from the original's, so
# the ROM needn't match. See tools/build_character.py.
CHARACTERS  ?=
ifneq ($(strip $(CHARACTERS)),)
override NON_MATCHING := 1
endif
# The modern build compiles each segment's code together, with LTO. Set
# LTO=0 to link its objects as each compiled, such as to find whether LTO
# brings about a difference. Build with -B after changing it.
LTO         ?= 1
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
GC_FLAGS    :=
else
BUILD_DIR   := build/$(if $(filter-out 0,$(SANITIZE)),sanitize,modern)
CROSS       := $(MODERN_CROSS)
BIN_FORMAT  := elf32-bigmips
CC          := $(MODERN_CC)
LIB_CC      := $(CC)
# Each routine and object in a section of its own, which the link drops
# unless something refers to it (see tools/layout.py). The tests'
# link keeps every routine, for the tests of those that the game doesn't
# call.
COLLECTED_SECTIONS := -ffunction-sections -fdata-sections
GC_FLAGS    := --gc-sections
# Each object holds its bytecode, which the segments' LTO links compile
# (see tools/link_segments.py), and its code as compiled alone, which the
# links of the objects themselves take: the tests', check-shiftable's and,
# with LTO=0, the game's. The tools that read the objects read that code.
LTO_FLAGS   := -flto -ffat-lto-objects
TARGETFLAGS := $(MODERN_TARGETFLAGS) $(COLLECTED_SECTIONS) $(LTO_FLAGS)
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
# A build with custom characters has a directory of its own within its
# toolchain's, as its character models' objects differ
ifneq ($(strip $(CHARACTERS)),)
BUILD_DIR   := $(BUILD_DIR)/custom
endif

# GCC's undefined behavior sanitizer, whose failed checks call the handlers
# of src/debug/sanitizer.c. They come from an archive, as libgcc's helpers
# do, so that they go at the end of main's code, where every segment can
# call them. Their own file is compiled without the checks, and without
# LTO, as the archive holds code.
SANITIZE_FLAGS := -fsanitize=bounds,shift,integer-divide-by-zero,float-cast-overflow,vla-bound
SANITIZER_O    := $(BUILD_DIR)/src/debug/sanitizer.c.o
ifneq ($(SANITIZE),0)
TARGETFLAGS += $(SANITIZE_FLAGS)
SANITIZER_LIB := $(BUILD_DIR)/libsanitizer.a
LIBS        += $(SANITIZER_LIB)
$(SANITIZER_O): TARGETFLAGS += -fno-sanitize=all -fno-lto
else
SANITIZER_LIB :=
endif

# The symbols that the objects use and none defines
SYMBOL_SCRIPT := undefined_syms.txt

# The two layouts (see tools/layout.py). The matching build places
# everything where the original has it. The shiftable layout lets the code
# and data grow, and leaves out the objects that only the matching links,
# such as main_bss.c, which pins main's common objects at the original's
# addresses.
MATCHING_LD       := $(BUILD_DIR)/matching.ld
SHIFTABLE_LD      := $(BUILD_DIR)/shiftable.ld
# The LTO links' objects, one for each segment of code, and the shiftable
# script that places them in place of the segments' files
LTO_DIR           := $(BUILD_DIR)/lto
LTO_LD            := $(LTO_DIR)/segments.ld
ifeq ($(NON_MATCHING),0)
GAME_LD     := $(MATCHING_LD)
LINKED_SOURCES = $(LAYOUT_OBJECTS)
else
GAME_LD     := $(SHIFTABLE_LD)
LINKED_SOURCES = $(filter-out $(MATCHING_ONLY_OBJECTS),$(LAYOUT_OBJECTS))
endif
ifneq ($(MODERN),0)
ifneq ($(LTO),0)
GAME_LD     := $(LTO_LD)
endif
endif
LD_SCRIPTS  := $(GAME_LD) $(SYMBOL_SCRIPT)

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
LDFLAGS         = $(addprefix -T ,$(LD_SCRIPTS)) -Map $(MAP) $(LAYOUT_FLAGS) $(GC_FLAGS)

# Host-side syntax checking of C sources with clang, pretending to be 32-bit MIPS
CC_CHECK       := clang -fsyntax-only -fno-builtin -funsigned-char -std=gnu89 -m32 -nostdinc \
                  -D_MIPS_SZLONG=32 -D_MIPS_SZINT=32 -DCC_CHECK=1 -Wall -Wno-unknown-pragmas \
                  -Wno-unused-variable -Wno-unused-but-set-variable -Wno-missing-braces
RUN_CC_CHECK   ?= 0

# Every object that the build links, under its directory. When a change to
# the layout makes the list again, make reads it in afresh before building
# anything, so that a file added to the layout is built.
include $(LAYOUT_MK)
O_FILES := $(LINKED_SOURCES:%=$(BUILD_DIR)/%.o)

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
OPTFLAGS := -Os
endif

# Per-object compiler flags
$(BUILD_DIR)/$(LIBNUSYS_DIR)/src/nusched.c.o: LIBNUSYS_DEFINES += -DNU_DEBUG

.PHONY: all extract clean distclean setup tools format lint typecheck pycheck check-shiftable FORCE \
        check-alignment \
        equivalence sanitize test target-tests tool-tests blender-tests

# A recipe that fails after writing its target, such as a check of what it
# wrote, would otherwise leave a target that the next run takes as built
.DELETE_ON_ERROR:

all: $(ROM)
ifneq ($(COMPARE),0)
	@sha1sum -c $(TARGET).sha1
endif

extract:
	$(PYTHON) tools/extract_assets.py
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

# Tests the custom character tools; the tests that need the game's files
# skip until they're extracted
tool-tests:
	PYTHONPATH=tools $(PYTHON) -m unittest discover --start-directory tools/tests

# Tests the Blender add-on in a Blender without a window, which BLENDER names
BLENDER ?= blender
blender-tests:
	tools/blender/run_tests.sh $(BLENDER)

# Links this build's objects in the shiftable layout, which fails on each
# reference that holds only while the code and data keep their original
# addresses. The shiftable builds link that way already.
check-shiftable: $(O_FILES) $(SHIFTABLE_LD) $(SYMBOL_SCRIPT) $(SANITIZER_LIB)
	$(LD) $(addprefix -T ,$(SHIFTABLE_LD) $(SYMBOL_SCRIPT)) -Map $(BUILD_DIR)/shiftable.map \
	    $(LAYOUT_FLAGS) -o $(BUILD_DIR)/shiftable.elf $(LIBS)
	@$(PYTHON) tools/check_link.py $(BUILD_DIR)/shiftable.elf $(SHIFTABLE_LD) $(SYMBOL_SCRIPT) -- $(O_FILES)

# Fails on each static buffer whose address reaches the RCP or a DMA but
# which doesn't ask for 8-byte alignment, so that it lands aligned in any
# layout (see tools/check_alignment.py). It reads the alignment that each
# buffer asks for from the modern build's objects, whose -fdata-sections
# gives each its own section.
CLANG_QUERY ?= clang-query-23
ifneq ($(MODERN),0)
check-alignment: $(O_FILES) $(BUILD_DIR)/compile_commands.json
	@$(PYTHON) tools/check_alignment.py $(BUILD_DIR) --clang-query $(CLANG_QUERY) \
	    --include $(LIBMUS_DIR)/src --include $(ULTRALIB_DIR)/include/PR
else
check-alignment:
	$(MAKE) MODERN=1 check-alignment
endif

# Runs the matching ROM and the modern build's side by side in ares's
# n64-harness, which N64_HARNESS names, and compares the game's state at
# each of EQUIVALENCE_FRAMES frames (see tools/equivalence.py)
EQUIVALENCE_FRAMES ?= 3000

equivalence: all
	$(MAKE) MODERN=1
	$(PYTHON) tools/equivalence.py --frames $(EQUIVALENCE_FRAMES)

# Builds the sanitized ROM (SANITIZE=1), runs it in n64-harness for
# SANITIZER_FRAMES frames, and reports the checks that failed (see
# tools/sanitize.py)
SANITIZER_FRAMES ?= 3000

sanitize:
	$(MAKE) SANITIZE=1
	$(PYTHON) tools/sanitize.py --frames $(SANITIZER_FRAMES)

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
	$(MAKE) MODERN=1 check-alignment target-tests

# Runs this build's test ROM in ares's n64-harness, which N64_HARNESS names
target-tests: $(TEST_ROM)
	$(PYTHON) tools/run_target_tests.py $(TEST_ELF) $<

$(BUILD_DIR)/tests/%.c.o: tests/%.c | $(BUILD_DIR)/$(LIBNUSYS_DIR)/include/nusys.h
	@mkdir -p $(dir $@)
	$(DEPS) $(MODERN_CC) -c $(TEST_CFLAGS) $(TEST_INCLUDES) $(TEST_DEFINES) -o $@ $<

$(TEST_BOOT): $(TEST_DIR)/target/boot.c.o
	rm -f $@
	$(MODERN_CROSS)ar rcs $@ $<

# The shiftable layout with the tests' segment
$(TEST_LD): $(LAYOUT) tests/target/segment.ld $(LAYOUT_TOOLS)
	@mkdir -p $(dir $@)
	$(PYTHON) tools/layout.py shiftable $(LAYOUT) $(BUILD_DIR) tests/target/segment.ld > $@

# NuSystem's boot starts the main thread in the boot stub instead of
# mainproc. Modern GCC's linker reads both toolchains' objects. The link
# takes the modern objects' code as each compiled, not the segments' LTO
# objects: --wrap redirects only the references that an object leaves
# undefined, and a segment's object defines mainproc where nuboot.c's
# reference to it is.
$(TEST_ELF): $(O_FILES) $(TEST_O_FILES) $(TEST_BOOT) $(TEST_LD) $(SYMBOL_SCRIPT) \
    $(SANITIZER_LIB)
	$(MODERN_CROSS)ld $(addprefix -T ,$(TEST_LD) $(SYMBOL_SCRIPT)) -Map $(TEST_DIR)/tests.map \
	    $(LAYOUT_FLAGS) --wrap=mainproc -o $@ $(TEST_O_FILES) $(TEST_BOOT) $(MODERN_LIBS) \
	    $(SANITIZER_LIB)
	@$(PYTHON) tools/check_link.py $@ $(TEST_LD) $(SYMBOL_SCRIPT) -- $(O_FILES)

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
# directory, and any mismatch fails the link. The link compiles the
# bytecode to code, as the segments' links do, rather than writing the
# merged bytecode out again: GCC 16.2's lto1 faults on that, in the
# inliner's merge of the mod/ref summaries of the nested functions of
# particle_effects.c's SpawnShotDebris. Either way compares the
# declarations.
TYPECHECK_DIR := build/typecheck
# The objects compiled from C
C_O_FILES = $(filter %.c.o,$(O_FILES))

typecheck:
	@$(MAKE) MODERN=1 BUILD_DIR=$(TYPECHECK_DIR) OPTFLAGS='-Os -flto' \
		$(TYPECHECK_DIR)/linked.o

$(BUILD_DIR)/linked.o: $(C_O_FILES)
	$(CC) -r -nostdlib $(TARGETFLAGS) $(OPTFLAGS) -flinker-output=nolto-rel -flto=auto \
		-Werror=lto-type-mismatch -o $@ $^

tools: $(KMC_DIR)/gcc $(KMC_DIR)/as

$(KMC_DIR)/gcc:
	@mkdir -p $(KMC_DIR)
	wget -q -O - https://github.com/decompals/mips-gcc-2.7.2/releases/latest/download/gcc-2.7.2-linux.tar.gz | tar xz -C $(KMC_DIR)

$(KMC_DIR)/as:
	@mkdir -p $(KMC_DIR)
	wget -q -O - https://github.com/decompals/mips-binutils-2.6/releases/latest/download/binutils-2.6-linux.tar.gz | tar xz -C $(KMC_DIR)

clean:
	rm -rf $(BUILD_DIR)/src $(BUILD_DIR)/assets $(BUILD_DIR)/lib $(ROM) $(ELF) $(TEST_DIR) \
	    $(LTO_DIR)

distclean:
	rm -rf $(BUILD_DIR) assets

$(LAYOUT_MK): $(LAYOUT) tools/layout.py
	@mkdir -p $(dir $@)
	$(PYTHON) tools/layout.py objects $< > $@

$(MATCHING_LD): $(LAYOUT) $(LAYOUT_TOOLS)
	@mkdir -p $(dir $@)
	$(PYTHON) tools/layout.py matching $< $(BUILD_DIR) > $@

$(SHIFTABLE_LD): $(LAYOUT) $(LAYOUT_TOOLS)
	@mkdir -p $(dir $@)
	$(PYTHON) tools/layout.py shiftable $< $(BUILD_DIR) > $@

# The compiler's LTO takes its share of make's jobs
$(LTO_LD): $(O_FILES) $(LAYOUT) $(LAYOUT_TOOLS) tools/link_segments.py
	+$(PYTHON) tools/link_segments.py $(LAYOUT) $(BUILD_DIR) $(LTO_DIR) -- \
	    $(CC) $(TARGETFLAGS) $(OPTFLAGS) $(WARNINGS)

# IPL3 boots only a ROM whose header holds the checksum of the code after it
$(ROM) $(TEST_ROM): %.z64: %.elf
	$(OBJCOPY) -O binary --gap-fill=0xFF --pad-to=$(ROM_SIZE) $< $@
	$(PYTHON) -m ipl3checksum sum --update $@ > /dev/null

# tools/check_link.py checks the layout for what ld doesn't: scripts that
# override an object's symbol, overlays that the game can't load together,
# and sections with contents that the scripts discard
$(ELF): $(O_FILES) $(LD_SCRIPTS) $(SANITIZER_LIB)
	$(LD) $(LDFLAGS) -o $@ $(LIBS)
	@$(PYTHON) tools/check_link.py $@ $(LD_SCRIPTS) -- $(O_FILES)

ifneq ($(SANITIZE),0)
$(SANITIZER_LIB): $(SANITIZER_O)
	rm -f $@
	$(CROSS)ar rcs $@ $<
endif

$(BUILD_DIR)/%.s.o: %.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -o $@ $<

ifneq ($(MODERN),0)
# A modern ROM is a build of its own, so it carries a build date of its own,
# in the original's format, rather than the original's. The game shows it on
# a debug screen and stamps it into each save, which no build checks when it
# loads one. Set BUILD_DATE to choose it; by default it is the last commit's,
# in UTC, so that a commit always builds the same ROM. The source is
# rewritten only when the date changes, so that the ROM is relinked only then.
BUILD_DATE_FORMAT := %a %b %e %H:%M:%S %Y
BUILD_DATE ?= $(shell TZ=UTC git log -1 --date='format-local:$(BUILD_DATE_FORMAT)' \
  --format=%cd 2>/dev/null || TZ=UTC date '+$(BUILD_DATE_FORMAT)')
BUILD_DATE_S := $(BUILD_DIR)/src/system/build_date.s

$(BUILD_DIR)/src/system/build_date.s.o: $(BUILD_DATE_S)
	$(AS) $(ASFLAGS) -o $@ $<

$(BUILD_DATE_S): FORCE
	@mkdir -p $(dir $@)
	@printf '.section .data\n    .ascii "%s\\r\\n"\n    .balign 16\n' \
	    '$(BUILD_DATE)' > $@.new
	@if cmp -s $@.new $@; then rm $@.new; else mv $@.new $@; fi
endif

FORCE:

# The microcode that make extract cuts from the ROM
$(BUILD_DIR)/src/system/microcode.s.o: $(wildcard assets/microcode/*.bin)

$(BUILD_DIR)/%.bin.o: %.bin
	@mkdir -p $(dir $@)
	$(OBJCOPY) -I binary -O $(BIN_FORMAT) -B mips $< $@

# Custom characters' model files, which build_character.py writes from their
# packages over the original's models, and the header of their layouts,
# which character_model.c includes in place of the original's
CHARACTER_DIR    := $(BUILD_DIR)/custom_characters
CHARACTER_HEADER := $(CHARACTER_DIR)/layouts.h
# Changes to the list of packages, which their files' dates don't show
CHARACTER_LIST   := $(CHARACTER_DIR)/packages.txt
# Where the text files are encoded from: a custom build's copies have the
# characters' names changed
TEXT_ROOT        :=
# The assets that replace the archive's
ARCHIVE_REPLACEMENTS :=
ifneq ($(strip $(CHARACTERS)),)
TEXT_ROOT        := $(CHARACTER_DIR)/
ARCHIVE_REPLACEMENTS := $(CHARACTER_DIR)/asset_archive
# The list of the face sprites' files, which face_sprites.py writes last
FACE_SPRITES     := $(CHARACTER_DIR)/face_sprites.txt
DEFINES          += -DCUSTOM_CHARACTERS
CHARACTER_FILES  := $(shell $(PYTHON) tools/build_character.py --list-files $(CHARACTERS))
CHARACTER_BINS   := $(CHARACTER_FILES:%=$(CHARACTER_DIR)/%.bin)
CHARACTER_TOOLS  := $(addprefix tools/,build_character.py character_template.py \
                    character_model_format.py f3dex2.py lz_codec.py)

$(CHARACTER_LIST): FORCE
	@mkdir -p $(@D)
	@echo '$(abspath $(CHARACTERS))' | cmp -s - $@ || echo '$(abspath $(CHARACTERS))' > $@

$(CHARACTER_HEADER): $(CHARACTER_LIST) $(CHARACTER_TOOLS) $(shell find $(CHARACTERS) -type f) \
                     src/characters/character_model.c $(wildcard assets/character_models/*.bin) \
                     $(TEXT_FILES:%=%.txt)
	$(PYTHON) tools/build_character.py --output $(CHARACTER_DIR) --header $@ \
	    $(TEXT_FILES:%=--text %.txt) $(CHARACTERS)

# The header is written last, so it's newer than the files it was written with
$(CHARACTER_BINS) $(CHARACTER_DIR)/names.h $(TEXT_FILES:%=$(CHARACTER_DIR)/%.txt): \
    $(CHARACTER_HEADER) ;

$(FACE_SPRITES): $(CHARACTER_LIST) $(CHARACTER_TOOLS) $(shell find $(CHARACTERS) -type f) \
                  $(addprefix tools/,face_sprites.py face_sprites.json decode_asset.py)
	$(PYTHON) tools/face_sprites.py --assets $(ARCHIVE_DIR) --output $(CHARACTER_DIR) \
	    $(CHARACTERS)

# The files that include the layouts or, through characters/character_names.h,
# the names
$(addprefix $(BUILD_DIR)/src/,characters/character_model.c.o menus/menu.c.o \
                               menus/main_menu.c.o round/tournament.c.o): $(CHARACTER_HEADER)

# Every model's object, so that one no longer replaced is made again
$(filter $(BUILD_DIR)/assets/character_models/%,$(O_FILES)): $(CHARACTER_LIST)

$(CHARACTER_FILES:%=$(BUILD_DIR)/assets/character_models/%.bin.o): \
$(BUILD_DIR)/assets/character_models/%.bin.o: $(CHARACTER_DIR)/%.bin
	@mkdir -p $(dir $@)
	$(OBJCOPY) -I binary -O $(BIN_FORMAT) -B mips $< $@
endif

# The asset archive is packed from the assets that asset_archive.py splits
# it into, so that one can change without the table being worked out again.
# A custom build's characters replace some, such as their face sprites.
$(BUILD_DIR)/assets/asset_archive.bin: $(wildcard $(ARCHIVE_DIR)/*.bin) tools/asset_archive.py \
                                       $(FACE_SPRITES)
	$(PYTHON) tools/asset_archive.py pack $(ARCHIVE_DIR) $@ $(ARCHIVE_REPLACEMENTS)

# The text files are encoded from the text that messages.py decodes them to
$(TEXT_FILES:%=$(BUILD_DIR)/%.bin): $(BUILD_DIR)/%.bin: $(TEXT_ROOT)%.txt tools/messages.py tools/asset_archive.py
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

-include $(O_FILES:.o=.d) $(TEST_O_FILES:.o=.d) $(TEST_DIR)/target/boot.c.d
# A header that a .d file lists but that has since gone only forces the
# rebuild that writes a new list
%.h: ;
