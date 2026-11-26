# Mario Golf (North America) matching decompilation
#
# A mod builds the game from its own folder, with a makefile that includes
# this one. Each file that the build reads is taken from the first folder
# that has it: the mod's, then each of MODS, then this repository's, the
# base's. A mod holds only the files that it changes, at the same paths as
# the base's, and its ROM is built under its own build/. MODS names further
# folders laid out the same way, such as a kit of tools that writes files
# for the build. Each of these folders' mod.mk, if it has one, is read
# before the rules below, to add its own.

# macOS's own make is GNU Make 3.81, from 2006, which this makefile isn't
# written or tested for
ifneq ($(filter 3.%,$(MAKE_VERSION)),)
$(error GNU Make $(MAKE_VERSION) is too old: use 4.0 or later, such as Homebrew's gmake)
endif

# This repository's folder, as the makefile that includes this one names it
BASE        := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))
MODS        ?=
MOD_ROOTS   := $(strip $(if $(filter .,$(BASE)),,.) $(MODS))
ROOTS       := $(MOD_ROOTS) $(BASE)
# PATH within each of ROOTS, in order, without a leading ./
roots        = $(patsubst ./%,%,$(addsuffix /$1,$(ROOTS)))
# The first of ROOTS that has the file PATH, as PATH within it
find         = $(firstword $(wildcard $(call roots,$1)))
# Each PATH that one of ROOTS has, in order, as include options
include_dirs = $(addprefix -I,$(wildcard $(foreach dir,$1,$(call roots,$(dir)))))
# Sources are searched for in the same order. Build outputs aren't, so that
# a mod's build never takes the base's.
vpath src/% $(ROOTS)
vpath lib/% $(ROOTS)
vpath tests/% $(ROOTS)
vpath assets/% $(ROOTS)

# make alone builds the ROM, whichever rule a mod.mk gives first
.DEFAULT_GOAL := all

# The ROM's name; a mod can give its own
TARGET      ?= mariogolf64
TOOLS       := $(BASE)/tools
# The tools' Python environment, which make creates from requirements.txt.
# Setting PYTHON to another interpreter leaves its packages to you.
# Python 3.14 warns when it runs from a path through .., as a mod's
# ../mariogolf64/.venv is, so a mod names the environment by its full path.
VENV        := $(if $(filter .,$(BASE)),.venv,$(abspath $(BASE)/.venv))
PYTHON      ?= $(VENV)/bin/python3
# The copy of requirements.txt that the environment was last made from,
# which make makes before anything that runs PYTHON, if PYTHON is its own
VENV_STAMP  := $(VENV)/requirements.txt
PYTHON_SETUP = $(if $(filter $(VENV)/bin/python3,$(PYTHON)),$(VENV_STAMP))
KMC_DIR     := $(BASE)/tools/gcc_kmc
# The original ROM, which make extract cuts the assets from
BASEROM     := $(BASE)/baserom.us.z64
# What the ROM holds, in order, from which tools/layout.py writes the
# linker scripts. A mod adds a file to the game by changing its own copy.
LAYOUT      := $(call find,layout.txt)
# What the linker scripts are written with
LAYOUT_TOOLS := $(TOOLS)/layout.py $(TOOLS)/heap_region.py $(BASE)/include/system/heap.h
# The symbols that the objects use and none defines
SYMBOL_SCRIPT := $(BASE)/undefined_syms.txt

# Set NON_MATCHING=1 to build a ROM that needn't match the original: it
# avoids the original's undefined behavior and is laid out to let code and
# data move (see the layouts below). A mod's ROM never matches.
NON_MATCHING ?= 0
ifneq ($(MOD_ROOTS),)
override NON_MATCHING := 1
endif
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

# The GNU binutils for MIPS that assemble, link and copy the matching
# build: the first of these whose ld is installed. Debian, Ubuntu and
# Homebrew package mips-linux-gnu-, Fedora mips64-linux-gnu-, the AUR
# mips64-elf-, and libdragon's toolchain is mips64-elf- in N64_INST. Set
# CROSS to choose others.
MATCHING_CROSS_CHOICES := mips-linux-gnu- mips64-linux-gnu- mips64-elf- $(MODERN_CROSS)

# The two toolchains, and what differs between their builds. Each build has
# its own directory, so that no build links another's objects.
ifeq ($(MODERN),0)
BUILD_DIR   := build$(if $(filter-out 0,$(NON_MATCHING)),/nonmatching)
ifeq ($(filter environment command line,$(origin CROSS)),)
CROSS       := $(firstword $(foreach prefix,$(MATCHING_CROSS_CHOICES),\
                 $(if $(shell command -v $(prefix)ld),$(prefix))) mips-linux-gnu-)
endif
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

# The two layouts (see tools/layout.py). The matching build places
# everything where the original has it. The shiftable layout lets the code
# and data grow, and leaves out the objects that only the matching links,
# such as main_bss.c, which pins main's common objects at the original's
# addresses.
LAYOUT_MK         := $(BUILD_DIR)/layout.mk
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
ELF         := $(BUILD_DIR)/$(TARGET).elf
MAP         := $(BUILD_DIR)/$(TARGET).map
# The cartridge's size, which 0xFF fills after the last asset
ROM_SIZE    := 0x2000000
# Where the asset archive's assets are split to, one file each
ARCHIVE_DIR := assets/asset_archive
# The folders of assets that replace the archive's, file by file: those of
# the mods. A mod.mk can add folders that it writes.
ARCHIVE_REPLACEMENTS := $(wildcard $(patsubst ./%,%,$(MOD_ROOTS:%=%/$(ARCHIVE_DIR))))
# The files of messages that are built from text, a message a line
TEXT_FILES  := assets/text/messages assets/text/staff_roll
# The folder of the text that they're built from, found in the mods before
# the base. A mod.mk that changes the text reads it from here and then sets
# this to the folder of its own copy, so that several mods' changes add up.
TEXT_SOURCE := assets/text

# Bare-metal binutils, whose names end in -elf-, and Linux ones name the
# o32 ABI's object format and linker emulation differently. A linker for
# mips64 links 64-bit code unless told otherwise.
ifneq ($(filter %-elf-,$(CROSS)),)
BIN_FORMAT  := elf32-bigmips
LD_EMULATION := elf32ebmip
else
BIN_FORMAT  := elf32-tradbigmips
LD_EMULATION := elf32btsmip
endif
AS          := $(CROSS)as
LD          := $(CROSS)ld -m $(LD_EMULATION)
OBJCOPY     := $(CROSS)objcopy
# The tools that read the objects and ELF files run the same binutils
# (see tools/binutils.py)
export CROSS

# Each library's headers are in its own include directory. NuSystem's is
# included through its converted copy, as the rule below explains. The
# game reaches libultra's PR/ headers through <ultra64.h>, so PR/ isn't
# searched, and none of its headers, such as PR/rcp.h, can stand in for a
# game header of the same name. A mod.mk can put folders of headers that it
# writes first.
INCLUDES    := $(call include_dirs,include lib/libkmc/include lib/ultralib/include lib/libmus/include) \
               -I$(BUILD_DIR)/lib/libnusys/include $(call include_dirs,lib/libnusys/include src) \
               -I$(BUILD_DIR)
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
# The assembler searches each of ROOTS for the files that .incbin names,
# such as the extracted microcode
ASFLAGS     := -march=vr4300 -mabi=32 -G0 -EB -no-pad-sections $(call include_dirs,include) \
               $(addprefix -I,$(filter-out .,$(ROOTS)))
# ld lays out each object's common symbols in the order of its symbol hash
# table's buckets. The table grows with the symbol count unless it starts big
# enough, so fix its size to keep the commons where the names put them.
LD_HASH_SIZE := 8191
LAYOUT_FLAGS   := --no-check-sections --hash-size=$(LD_HASH_SIZE) -G 0
LDFLAGS         = $(addprefix -T ,$(LD_SCRIPTS)) -Map $(MAP) $(LAYOUT_FLAGS) $(GC_FLAGS)

# The recipes that turn a file into an object that holds it, and a text
# file into the messages that the game reads, for a mod.mk's rules too
BIN_TO_OBJECT = $(OBJCOPY) -I binary -O $(BIN_FORMAT) -B mips $< $@
ENCODE_TEXT   = $(PYTHON) $(TOOLS)/messages.py encode $< $@

# Every object that the build links, under its directory. When a change to
# the layout makes the list again, make reads it in afresh before building
# anything, so that a file added to the layout is built. The goals that
# build nothing skip it, as making it needs the Python environment.
SETUP_GOALS := help tools clean distclean
ifneq ($(filter-out $(SETUP_GOALS),$(or $(MAKECMDGOALS),all)),)
include $(LAYOUT_MK)
endif
O_FILES := $(LINKED_SOURCES:%=$(BUILD_DIR)/%.o)

# libultra 2.0J, built the way ultralib builds libgultra_rom except that the
# game's copy was compiled with signed chars
ULTRALIB_DIR      := lib/ultralib
ULTRALIB_INCLUDES := $(call include_dirs,$(addprefix $(ULTRALIB_DIR)/include,/ /compiler/$(ULTRALIB_COMPILER) /PR))
ULTRALIB_DEFINES  := $(LIB_MODERN) -D_MIPS_SZLONG=32 -D__USE_ISOC99 -DF3DEX_GBI -DBUILD_VERSION=VERSION_J \
                     -DBUILD_VERSION_STRING=\"2.0J\" -DNDEBUG -D_FINALROM
ULTRALIB_CFLAGS    = $(WARNINGS) -nostdinc $(TARGETFLAGS) -D_LANGUAGE_C $(OPTFLAGS)
ULTRALIB_ASFLAGS  := -x assembler-with-cpp -nostdinc $(TARGETFLAGS) -P \
                     -DMIPSEB -D_LANGUAGE_ASSEMBLY $(ULTRALIB_ASM_ABI) -D_ULTRA64

# libmus, the n_audio build of the Software Creations music library, as its
# makefile built libn_gmus.
LIBMUS_DIR        := lib/libmus
LIBMUS_INCLUDES   := $(call include_dirs,$(LIBMUS_DIR)/src $(LIBMUS_DIR)/include) $(ULTRALIB_INCLUDES)
LIBMUS_DEFINES    := $(LIB_MODERN) -D_MIPS_SZLONG=32 -D_MIPS_SZINT=32 -DF3DEX_GBI -DNDEBUG -DSUPPORT_NAUDIO
LIBMUS_CFLAGS      = $(WARNINGS) -nostdinc $(TARGETFLAGS) -D_LANGUAGE_C $(OPTFLAGS) $(KMC_ALIGN)

# n_audio, built from the SDK's n_audio_sc sources for the n_aspMain
# microcode as other games of the time built it
LIBNAUDIO_DIR      := lib/libnaudio
LIBNAUDIO_INCLUDES := $(call include_dirs,$(LIBNAUDIO_DIR)/src $(LIBNAUDIO_DIR)/include) $(ULTRALIB_INCLUDES)
LIBNAUDIO_DEFINES  := $(LIB_MODERN) -D_MIPS_SZLONG=32 -D_MIPS_SZINT=32 -DF3DEX_GBI -DNDEBUG -DN_MICRO
LIBNAUDIO_CFLAGS    = $(LIBMUS_CFLAGS)

# NuSystem, with the defines of the SDK makefile's ROM library. The
# scheduler is built with NU_DEBUG, which keeps its task performance
# measurements. The sources keep the SDK's CRLF line
# endings, and KMC's preprocessor doesn't join lines that end in a backslash
# and a carriage return, so they're compiled from copies without the CRs.
LIBNUSYS_DIR       := lib/libnusys
LIBNUSYS_INCLUDES  := -I$(BUILD_DIR)/$(LIBNUSYS_DIR)/include $(call include_dirs,$(LIBNUSYS_DIR)/include) \
                      $(ULTRALIB_INCLUDES)
LIBNUSYS_DEFINES   := $(LIB_MODERN) $(LIB_AVOID_UB) -D_MIPS_SZLONG=32 -D_MIPS_SZINT=32 -DUSE_EPI -DBUILD_VERSION=VERSION_J -D_FINALROM
LIBNUSYS_CFLAGS     = $(WARNINGS) -nostdinc $(TARGETFLAGS) -D_LANGUAGE_C $(OPTFLAGS) $(KMC_ALIGN)
# Game code includes NuSystem's header through its converted copy, so the
# copy must exist before any game file compiles. A mod.mk can add the
# headers that it writes. The .d files make the files that include them
# rebuild when they change.
GENERATED_HEADERS  := $(BUILD_DIR)/$(LIBNUSYS_DIR)/include/nusys.h

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
# main_bss.c defines libmus's bss objects too, whose types only its sources
# declare. Those include libultra's headers from PR/ by their bare names.
$(BUILD_DIR)/src/system/main_bss.c.o: INCLUDES += $(call include_dirs,$(LIBMUS_DIR)/src $(ULTRALIB_DIR)/include/PR)

# The mods' own settings and rules. A mod.mk adds to MOD_SETTINGS each
# setting that chooses which files the build takes, such as the character
# kit's CHARACTERS, so that changing it rebuilds what it chose.
MOD_SETTINGS :=
include $(wildcard $(MOD_ROOTS:%=%/mod.mk))

.PHONY: all help setup tools extract objects clean distclean check-shiftable check-alignment equivalence \
        sanitize test target-tests format check-format lint typecheck pycheck tool-tests FORCE

# A recipe that fails after writing its target, such as a check of what it
# wrote, would otherwise leave a target that the next run takes as built
.DELETE_ON_ERROR:

# macOS has shasum but not sha1sum, which read the same list of hashes
all: $(ROM)
ifneq ($(COMPARE),0)
	@if command -v sha1sum > /dev/null; then sha1sum -c $(BASE)/$(TARGET).sha1; \
	else shasum -a 1 -c $(BASE)/$(TARGET).sha1; fi
endif

help:
	@echo 'Goals:'
	@echo '  all             build the ROM and, if it should match, check its SHA-1 (default)'
	@echo '  setup           install KMC GCC and the Python tools, and extract the assets'
	@echo '  objects         compile everything that needs no file from the original ROM'
	@echo '  test            run the unit tests on both builds in ares n64-harness'
	@echo '  check-shiftable check that the code links with its code and data moved'
	@echo '  format          format the C sources and the Python tools'
	@echo '  check-format    check the C sources format as make format leaves them'
	@echo '  pycheck         lint and type-check the Python tools'
	@echo '  tool-tests      test the Python tools'
	@echo '  lint            run clang-tidy on the game code'
	@echo '  clean           remove this build; distclean removes every build and the assets'
	@echo 'Settings, such as make MODERN=1:'
	@echo '  NON_MATCHING=1  build a ROM whose code and data can move'
	@echo '  MODERN=1        build with modern GCC from libdragon (N64_INST=$(N64_INST))'
	@echo '  CROSS=PREFIX    the MIPS binutils to use (found: $(CROSS))'
	@echo 'See README.md for the rest.'

# Everything that the matching build needs from outside the repository,
# besides the original ROM, which only you can supply
setup: tools $(PYTHON_SETUP) extract

# Cuts the files that the game reads from the original ROM into the base's
# assets/, once for every mod
extract:
	$(PYTHON) $(TOOLS)/extract_assets.py
	$(PYTHON) $(TOOLS)/extract_textures.py
	$(PYTHON) $(TOOLS)/asset_archive.py split $(BASE)/assets/asset_archive.bin $(BASE)/$(ARCHIVE_DIR)
	for text in $(TEXT_FILES); do \
		$(PYTHON) $(TOOLS)/messages.py decode $(BASE)/$$text.bin $(BASE)/$$text.txt || exit; \
	done

# KMC GCC and its assembler, from the releases that tools/install_kmc.py
# pins. It needs only Python's standard library, so it runs before the
# environment exists.
tools: $(KMC_DIR)/gcc

$(KMC_DIR)/gcc:
	python3 $(TOOLS)/install_kmc.py $(KMC_DIR)

clean:
	rm -rf $(BUILD_DIR)/src $(BUILD_DIR)/assets $(BUILD_DIR)/lib $(ROM) $(ELF) $(TEST_DIR) \
	    $(LTO_DIR)

# Removes every build and, in the base, the files that make extract cut
distclean:
	rm -rf build $(if $(filter .,$(BASE)),assets)

# Links this build's objects in the shiftable layout, which fails on each
# reference that holds only while the code and data keep their original
# addresses. The shiftable builds link that way already.
check-shiftable: $(O_FILES) $(SHIFTABLE_LD) $(SYMBOL_SCRIPT) $(SANITIZER_LIB)
	$(LD) $(addprefix -T ,$(SHIFTABLE_LD) $(SYMBOL_SCRIPT)) -Map $(BUILD_DIR)/shiftable.map \
	    $(LAYOUT_FLAGS) -o $(BUILD_DIR)/shiftable.elf $(LIBS)
	@$(PYTHON) $(TOOLS)/check_link.py $(BUILD_DIR)/shiftable.elf $(SHIFTABLE_LD) $(SYMBOL_SCRIPT) -- $(O_FILES)

# Fails on each static buffer whose address reaches the RCP or a DMA but
# which doesn't ask for 8-byte alignment, so that it lands aligned in any
# layout (see tools/check_alignment.py). It reads the alignment that each
# buffer asks for from the modern build's objects, whose -fdata-sections
# gives each its own section.
CLANG_QUERY ?= $(call first_installed,clang-query-23 clang-query)
ifneq ($(MODERN),0)
check-alignment: $(O_FILES) $(BUILD_DIR)/compile_commands.json
	@$(PYTHON) $(TOOLS)/check_alignment.py $(BUILD_DIR) --clang-query $(CLANG_QUERY) \
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
	$(PYTHON) $(TOOLS)/equivalence.py --frames $(EQUIVALENCE_FRAMES)

# Builds the sanitized ROM (SANITIZE=1), runs it in n64-harness for
# SANITIZER_FRAMES frames, and reports the checks that failed (see
# tools/sanitize.py)
SANITIZER_FRAMES ?= 3000

sanitize:
	$(MAKE) SANITIZE=1
	$(PYTHON) $(TOOLS)/sanitize.py --frames $(SANITIZER_FRAMES)

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
TEST_SEGMENT  := $(BASE)/tests/target/segment.ld
TEST_SOURCES  := $(patsubst $(BASE)/%,%,$(wildcard $(BASE)/tests/*.c)) tests/target/runner.c \
                 tests/target/support.c tests/unity/unity.c tests/unity/unity_fixture.c
TEST_O_FILES  := $(TEST_SOURCES:%=$(BUILD_DIR)/%.o)
# The boot stub goes into main's code, which takes it from an archive
TEST_BOOT     := $(TEST_DIR)/boot.a
# The tests see the declarations of the build they test, but not KMC GCC's
# forms, as modern GCC compiles them. Unity needs the compiler's own
# headers, such as stdarg.h, which the game's code doesn't include.
TEST_CFLAGS    = $(MODERN_TARGETFLAGS) -Os -nostdinc -funsigned-char -Wall -Wextra \
                 -isystem $(shell $(MODERN_CC) -print-file-name=include)
TEST_INCLUDES := $(call include_dirs,tests/target tests/unity) $(INCLUDES)
TEST_DEFINES  := $(filter-out $(GAME_DEFINES),$(DEFINES)) -DUNITY_INCLUDE_CONFIG_H

# Runs the tests against both toolchains' code, once the matching ROM has
# matched the original
test: all target-tests
	$(MAKE) MODERN=1 check-alignment target-tests

# Runs this build's test ROM in ares's n64-harness, which N64_HARNESS names
target-tests: $(TEST_ROM)
	$(PYTHON) $(TOOLS)/run_target_tests.py $(TEST_ELF) $<

$(BUILD_DIR)/tests/%.c.o: tests/%.c | $(GENERATED_HEADERS)
	@mkdir -p $(dir $@)
	$(DEPS) $(MODERN_CC) -c $(TEST_CFLAGS) $(TEST_INCLUDES) $(TEST_DEFINES) -o $@ $<

$(TEST_BOOT): $(TEST_DIR)/target/boot.c.o
	rm -f $@
	$(MODERN_CROSS)ar rcs $@ $<

# The shiftable layout with the tests' segment
$(TEST_LD): $(LAYOUT) $(TEST_SEGMENT) $(LAYOUT_TOOLS)
	@mkdir -p $(dir $@)
	$(PYTHON) $(TOOLS)/layout.py shiftable $(LAYOUT) $(BUILD_DIR) $(TEST_SEGMENT) > $@

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
	@$(PYTHON) $(TOOLS)/check_link.py $@ $(TEST_LD) $(SYMBOL_SCRIPT) -- $(O_FILES)

# The Python environment, made again whenever requirements.txt changes.
# The list of objects needs it, and make reads that list in on every run,
# so the environment is always made before anything else. uv makes it if
# it's installed, and needs no venv module; otherwise Python's own venv
# and pip do.
$(VENV_STAMP): $(BASE)/requirements.txt
	if command -v uv > /dev/null; then \
	    uv venv --quiet --allow-existing $(VENV) && \
	    VIRTUAL_ENV=$(VENV) uv pip install --quiet --requirement $<; \
	else \
	    python3 -m venv $(VENV) && \
	    $(VENV)/bin/python3 -m pip install --quiet --disable-pip-version-check --requirement $<; \
	fi
	cp $< $@

# The linker scripts, and the list of objects, from the layout
$(LAYOUT_MK): $(LAYOUT) $(TOOLS)/layout.py | $(PYTHON_SETUP)
	@mkdir -p $(dir $@)
	$(PYTHON) $(TOOLS)/layout.py objects $< > $@

$(MATCHING_LD): $(LAYOUT) $(LAYOUT_TOOLS)
	@mkdir -p $(dir $@)
	$(PYTHON) $(TOOLS)/layout.py matching $< $(BUILD_DIR) > $@

$(SHIFTABLE_LD): $(LAYOUT) $(LAYOUT_TOOLS)
	@mkdir -p $(dir $@)
	$(PYTHON) $(TOOLS)/layout.py shiftable $< $(BUILD_DIR) > $@

# The compiler's LTO takes its share of make's jobs
$(LTO_LD): $(O_FILES) $(LAYOUT) $(LAYOUT_TOOLS) $(TOOLS)/link_segments.py
	+$(PYTHON) $(TOOLS)/link_segments.py $(LAYOUT) $(BUILD_DIR) $(LTO_DIR) -- \
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
	@$(PYTHON) $(TOOLS)/check_link.py $@ $(LD_SCRIPTS) -- $(O_FILES)

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
$(BUILD_DIR)/src/system/microcode.s.o: $(wildcard $(call roots,assets/microcode/*.bin))

$(BUILD_DIR)/%.bin.o: %.bin
	@mkdir -p $(dir $@)
	$(BIN_TO_OBJECT)

# The asset archive is packed from the assets that asset_archive.py splits
# it into, so that one can change without the table being worked out again,
# and from the folders of assets that replace some
$(BUILD_DIR)/assets/asset_archive.bin: $(wildcard $(BASE)/$(ARCHIVE_DIR)/*.bin) \
                                       $(wildcard $(ARCHIVE_REPLACEMENTS:%=%/*.bin)) $(TOOLS)/asset_archive.py
	@mkdir -p $(dir $@)
	$(PYTHON) $(TOOLS)/asset_archive.py pack $(BASE)/$(ARCHIVE_DIR) $@ $(ARCHIVE_REPLACEMENTS)

# The text files are encoded from the text that messages.py decodes them to
$(BUILD_DIR)/assets/text/%.bin: $(TEXT_SOURCE)/%.txt $(TOOLS)/messages.py $(TOOLS)/asset_archive.py
	@mkdir -p $(dir $@)
	$(ENCODE_TEXT)

$(BUILD_DIR)/assets/asset_archive.bin.o $(TEXT_FILES:%=$(BUILD_DIR)/%.bin.o): %.o: %
	$(BIN_TO_OBJECT)

# Textures in C data come from PNGs that extract_textures.py writes to
# assets/SOURCE/, where SOURCE is the path of the C file under src/ that
# includes them
TEXTURE_PNGS := $(shell sed 's/\#.*//' $(BASE)/textures.txt | awk 'NF { print $$5 }')

$(BUILD_DIR)/assets/%.inc.c: assets/%.png $(TOOLS)/png_to_c.py
	@mkdir -p $(dir $@)
	$(PYTHON) $(TOOLS)/png_to_c.py $< $@

$(foreach png,$(TEXTURE_PNGS),$(eval \
  $(patsubst assets/%/,$(BUILD_DIR)/src/%.c.o,$(dir $(png))): \
      $(BUILD_DIR)/$(png:.png=.inc.c)))

# Compiles and assembles every object that needs nothing cut from the
# original ROM: all but the files of assets, the microcode and the C
# files that include textures. It checks the code where there is no ROM,
# such as in CI.
ASSET_O_FILES := $(filter %.bin.o,$(O_FILES)) $(BUILD_DIR)/src/system/microcode.s.o \
                 $(patsubst assets/%/,$(BUILD_DIR)/src/%.c.o,$(dir $(TEXTURE_PNGS)))
objects: $(filter-out $(ASSET_O_FILES),$(O_FILES))

# The sources are UTF-8, but the game's strings are Shift-JIS. KMC GCC
# compiles a Shift-JIS copy, and the line marker keeps its messages
# pointing at the source. -I adds back the source's own directory, which
# GCC searches first for quoted includes, in each of ROOTS. CP932,
# Microsoft's Shift-JIS, gives the same bytes as glibc's SHIFT_JIS for
# every character in the sources, but macOS's iconv reads SHIFT_JIS's 0x5C
# and 0x7E as yen and overline, and wouldn't pass C's \ and ~ through.
ICONV := iconv -f UTF-8 -t CP932

$(BUILD_DIR)/%.c.o: %.c
	@mkdir -p $(dir $@)
	{ echo '# 1 "$<"'; $(ICONV) $<; } > $(BUILD_DIR)/$*.sjis.c
	$(DEPS) $(CC) -c $(CFLAGS) $(call include_dirs,$(patsubst %/,%,$(dir $*))) $(INCLUDES) $(DEFINES) -o $@ $(BUILD_DIR)/$*.sjis.c

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

$(filter $(BUILD_DIR)/src/%,$(O_FILES)): | $(GENERATED_HEADERS)

$(BUILD_DIR)/$(LIBNUSYS_DIR)/include/nusys.h: $(LIBNUSYS_DIR)/include/nusys.h
	@mkdir -p $(dir $@)
	tr -d '\r' < $< > $@

# Which files a mod's build takes, which no file's date shows: adding a
# file to a mod, or removing one, changes the file that the build takes for
# its path, and so does a change to the layout taken or to MOD_SETTINGS. A
# change to any of them rebuilds everything built from the files taken.
ifneq ($(MOD_ROOTS),)
MOD_INPUTS := $(BUILD_DIR)/mod_inputs.txt
MOD_INPUT_LIST := $(LAYOUT) $(MOD_SETTINGS) $(shell find $(wildcard $(foreach root,$(MOD_ROOTS),\
                   $(addprefix $(root)/,src include lib tests assets))) -type f | sort)

$(MOD_INPUTS): FORCE
	@mkdir -p $(@D)
	@echo '$(MOD_INPUT_LIST)' | cmp -s - $@ || echo '$(MOD_INPUT_LIST)' > $@

$(O_FILES) $(TEST_O_FILES) $(TEST_DIR)/target/boot.c.o $(GENERATED_HEADERS) \
$(BUILD_DIR)/assets/asset_archive.bin $(TEXT_FILES:%=$(BUILD_DIR)/%.bin) \
$(addprefix $(BUILD_DIR)/,$(TEXTURE_PNGS:.png=.inc.c)) \
$(LAYOUT_MK) $(MATCHING_LD) $(SHIFTABLE_LD) $(TEST_LD): $(MOD_INPUTS)
endif

-include $(O_FILES:.o=.d) $(TEST_O_FILES:.o=.d) $(TEST_DIR)/target/boot.c.d
# A header that a .d file lists but that has since gone only forces the
# rebuild that writes a new list
%.h: ;

# What follows checks the base's own sources and tools, from its folder.

# The first of the named programs that's installed, or else the first named
first_installed = $(firstword $(foreach program,$1,$(if $(shell command -v $(program)),$(program))) $1)

# The clang-format that requirements.txt pins
CLANG_FORMAT ?= $(VENV)/bin/clang-format
FORMAT_FILES  = $(shell git ls-files 'src/*.c' 'src/*.h' 'include/*.h' '$(LIBKMC_DIR)/*.[ch]')
# The tests are formatted too, but not Unity, which is kept as upstream has
# it, and they aren't linted, as the lint's compile commands are the game's
TEST_FORMAT_FILES = $(wildcard tests/*.c tests/target/*.c tests/target/*.h)

format:
	$(CLANG_FORMAT) -i $(FORMAT_FILES) $(TEST_FORMAT_FILES)
	$(VENV)/bin/ruff format --quiet

# Fails on each C file that make format would change; ruff's own check is
# part of pycheck
check-format:
	@$(CLANG_FORMAT) --dry-run --Werror $(FORMAT_FILES) $(TEST_FORMAT_FILES)

# Lints and type-checks the Python tools, with the settings in pyproject.toml
pycheck:
	$(VENV)/bin/ruff format --check --quiet
	$(VENV)/bin/ruff check --quiet
	$(VENV)/bin/basedpyright --level warning
	$(VENV)/bin/vulture

# Tests the Python tools; the tests that need the game's files skip until
# they're extracted
tool-tests:
	PYTHONPATH=tools $(PYTHON) -m unittest discover --start-directory tools/tests

CLANG_TIDY ?= $(call first_installed,run-clang-tidy-23 run-clang-tidy)
# libkmc is KMC's C library, which keeps the C library's names
LINT_FILES = $(filter-out $(LIBKMC_DIR)/%,$(filter %.c,$(FORMAT_FILES)))

$(BUILD_DIR)/compile_commands.json: $(TOOLS)/compile_commands.py $(BASE)/Makefile
	@mkdir -p $(BUILD_DIR)
	@python3 $(TOOLS)/compile_commands.py $@ $(INCLUDES) $(DEFINES) -- $(LINT_FILES)

LINT_JOBS ?= 4

# clang can't parse everything GCC 2.7 accepts, such as nested functions,
# so only the naming and macro diagnostics fail the check, each printed
# once though a header's repeats for every file that includes it. The
# rest stay in the log. As the parse errors make run-clang-tidy fail
# anyway, the check fails on its own errors, such as finding no
# clang-tidy, by their messages, so that a lint that never ran can't pass.
LINT_CHECKS := readability-identifier-naming|bugprone-macro-parentheses
LINT_RUN_ERRORS := ^error: |^Unable to run clang-tidy

lint: $(BUILD_DIR)/compile_commands.json
	@command -v $(CLANG_TIDY) > /dev/null || \
	    { echo 'make lint: no run-clang-tidy found; set CLANG_TIDY to one' >&2; exit 1; }
	@$(CLANG_TIDY) -p $(BUILD_DIR) -j $(LINT_JOBS) -quiet $(LINT_FILES) \
		> $(BUILD_DIR)/lint.log 2>&1 || true
	@! grep -E '$(LINT_RUN_ERRORS)' $(BUILD_DIR)/lint.log
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
