# Mario Golf (North America) matching decompilation

TARGET      := mariogolf64
BASEROM     := baserom.us.z64
SPLAT_YAML  := $(TARGET).yaml
SPLAT_DIR   := build/splat
SPLAT_LD    := $(SPLAT_DIR)/$(TARGET).ld
KMC_DIR     := tools/gcc_kmc

# Set MODERN=1 to build with a modern GCC instead of KMC GCC 2.7.2. The
# ROM it makes doesn't match; it's for checking that the code is portable
# and, once every address is symbolic, for building a shiftable ROM.
MODERN      ?= 0
# Set NON_MATCHING=1 to build with decompiled-but-unmatched C instead of asm
NON_MATCHING ?= 0
# Set COMPARE=0 to skip the checksum comparison at the end of the build
COMPARE      ?= 1
# Set WARNINGS="-Wall -W" to see the compiler's warnings
WARNINGS    ?= -w

# The two toolchains, and what differs between their builds
ifeq ($(MODERN),0)
BUILD_DIR   := build
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
GAME_CFLAGS := -ffast-math $(KMC_ALIGN)
# Lets the sources keep the original's quirks that only KMC GCC accepts
GAME_DEFINES := -D__IS_KMC__=1
ULTRALIB_COMPILER := gcc
LD_SCRIPT   := $(SPLAT_LD)
else
BUILD_DIR   := build/modern
COMPARE     := 0
# libdragon's toolchain, built for the VR4300 with an o32 multilib, and its
# own binutils, whose linker takes its objects' flags
N64_INST    ?= /opt/libdragon
CROSS       := $(N64_INST)/bin/mips64-elf-
BIN_FORMAT  := elf32-bigmips
CC          := $(CROSS)gcc
LIB_CC      := $(CC)
# GNU C89 keeps the meaning of the sources' old-style declarations, and
# -fcommon the shared tentative definitions that the libraries rely on.
# -fno-toplevel-reorder keeps the statics that only included asm uses.
TARGETFLAGS := -G0 -march=vr4300 -mabi=32 -mfix4300 -mno-abicalls -fno-PIC -std=gnu89 -fcommon \
               -ffreestanding -fno-strict-aliasing -mdivide-breaks -fno-toplevel-reorder \
               -fno-math-errno
KMC_ALIGN   :=
GAME_CFLAGS :=
GAME_DEFINES :=
# ultralib's own settings for modern GCC
ULTRALIB_COMPILER := modern_gcc
ULTRALIB_MODERN := -DMODERN_CC
# splat's linker script names the objects under build/, so this build
# links a copy that names them under its own directory
LD_SCRIPT   := $(BUILD_DIR)/$(TARGET).ld
endif

ROM         := $(BUILD_DIR)/$(TARGET).z64
ELF         := $(BUILD_DIR)/$(TARGET).elf
MAP         := $(BUILD_DIR)/$(TARGET).map

AS          := $(CROSS)as
LD          := $(CROSS)ld
OBJCOPY     := $(CROSS)objcopy
OBJDUMP     := $(CROSS)objdump
CPP         := $(CROSS)cpp
PYTHON      := .venv/bin/python3

INCLUDES    := -Iinclude -Iinclude/libc -Ilib/ultralib/include -Ilib/ultralib/include/PR -Ilib/libmus/include -Isrc -I$(BUILD_DIR)
DEFINES     := -D_LANGUAGE_C -D_MIPS_SZLONG=32 -D_MIPS_SZINT=32 -DF3DEX_GBI_2 -DNDEBUG -D_FINALROM -DBUILD_VERSION=VERSION_J \
               $(GAME_DEFINES)
ifneq ($(NON_MATCHING),0)
DEFINES     += -DNON_MATCHING
endif

# Each compile lists the headers it read in a .d file beside its object, so
# that changing a header rebuilds what includes it. Both compilers'
# preprocessors take the file and target from SUNPRO_DEPENDENCIES, KMC's
# having no -MF or -MT. Unlike DEPENDENCIES_OUTPUT, it also lists headers
# included with angle brackets, such as NuSystem's. KMC's appends to the
# file, so the old list goes first.
DEPS         = rm -f $(@:.o=.d); SUNPRO_DEPENDENCIES='$(@:.o=.d) $@'

OPTFLAGS    := -O2
CFLAGS       = $(TARGETFLAGS) $(GAME_CFLAGS) -nostdinc -funsigned-char $(WARNINGS) $(OPTFLAGS)
ASFLAGS     := -march=vr4300 -mabi=32 -G0 -EB -no-pad-sections -Iinclude
# ld lays out each object's common symbols in the order of its symbol hash
# table's buckets. The table grows with the symbol count unless it starts big
# enough, so fix its size to keep the commons where the names put them.
LD_HASH_SIZE := 8191
LDFLAGS      = -T $(LD_SCRIPT) -T undefined_syms.txt -T $(SPLAT_DIR)/undefined_syms_auto.txt \
               -T $(SPLAT_DIR)/undefined_funcs_auto.txt -Map $(MAP) --no-check-sections \
               --hash-size=$(LD_HASH_SIZE) -G 0

# Host-side syntax checking of C sources with clang, pretending to be 32-bit MIPS
CC_CHECK       := clang -fsyntax-only -fno-builtin -funsigned-char -std=gnu89 -m32 -nostdinc \
                  -D_MIPS_SZLONG=32 -D_MIPS_SZINT=32 -DCC_CHECK=1 -Wall -Wno-unknown-pragmas \
                  -Wno-unused-variable -Wno-unused-but-set-variable -Wno-missing-braces
RUN_CC_CHECK   ?= 0

# Every object the linker script asks for, under this build's directory
O_FILES := $(patsubst build/%,$(BUILD_DIR)/%,$(shell test -f $(SPLAT_LD) && \
  grep -oE 'build/[A-Za-z0-9_./-]+\.o' $(SPLAT_LD) | sort -u))

# libultra 2.0J, built the way ultralib builds libgultra_rom except that the
# game's copy was compiled with signed chars
ULTRALIB_DIR      := lib/ultralib
ULTRALIB_INCLUDES := -I$(ULTRALIB_DIR)/include -I$(ULTRALIB_DIR)/include/compiler/$(ULTRALIB_COMPILER) -I$(ULTRALIB_DIR)/include/PR
ULTRALIB_DEFINES  := $(ULTRALIB_MODERN) -D_MIPS_SZLONG=32 -D__USE_ISOC99 -DF3DEX_GBI -DBUILD_VERSION=VERSION_J \
                     -DBUILD_VERSION_STRING=\"2.0J\" -DNDEBUG -D_FINALROM
ULTRALIB_CFLAGS   := -w -nostdinc $(TARGETFLAGS) -D_LANGUAGE_C -O3
ULTRALIB_ASFLAGS  := -x assembler-with-cpp -w -nostdinc $(TARGETFLAGS) -P \
                     -DMIPSEB -D_LANGUAGE_ASSEMBLY -D_MIPS_SIM=1 -D_ULTRA64

# libmus, the n_audio build of the Software Creations music library, as its
# makefile built libn_gmus.
LIBMUS_DIR        := lib/libmus
LIBMUS_INCLUDES   := -I$(LIBMUS_DIR)/src -I$(LIBMUS_DIR)/include $(ULTRALIB_INCLUDES)
LIBMUS_DEFINES    := -D_MIPS_SZLONG=32 -D_MIPS_SZINT=32 -DF3DEX_GBI -DNDEBUG -DSUPPORT_NAUDIO
LIBMUS_CFLAGS     := -w -nostdinc $(TARGETFLAGS) -D_LANGUAGE_C -O3 $(KMC_ALIGN)

# n_audio, built from the SDK's n_audio_sc sources for the n_aspMain
# microcode as other games of the time built it
LIBNAUDIO_DIR      := lib/libnaudio
LIBNAUDIO_INCLUDES := -I$(LIBNAUDIO_DIR)/src -I$(LIBNAUDIO_DIR)/include $(ULTRALIB_INCLUDES)
LIBNAUDIO_DEFINES  := -D_MIPS_SZLONG=32 -D_MIPS_SZINT=32 -DF3DEX_GBI -DNDEBUG -DN_MICRO
LIBNAUDIO_CFLAGS   := $(LIBMUS_CFLAGS)

# NuSystem, with the defines of the SDK makefile's ROM library but at -O2,
# the level the game's copy was built at. Like the SDK's nusys.o, the boot
# code is built without optimization. The scheduler is built with NU_DEBUG,
# which keeps its task performance measurements. The sources keep the SDK's CRLF line
# endings, and KMC's preprocessor doesn't join lines that end in a backslash
# and a carriage return, so they're compiled from copies without the CRs.
LIBNUSYS_DIR       := lib/libnusys
LIBNUSYS_INCLUDES  := -I$(BUILD_DIR)/$(LIBNUSYS_DIR) $(ULTRALIB_INCLUDES)
LIBNUSYS_DEFINES   := -D_MIPS_SZLONG=32 -D_MIPS_SZINT=32 -DUSE_EPI -DBUILD_VERSION=VERSION_J -D_FINALROM
LIBNUSYS_OPTFLAGS  := -O2
LIBNUSYS_CFLAGS     = -w -nostdinc $(TARGETFLAGS) -D_LANGUAGE_C $(LIBNUSYS_OPTFLAGS) $(KMC_ALIGN)

# Per-directory compiler flags
# libkmc was built without -ffast-math, which turns divisions by constants into multiplications
$(BUILD_DIR)/src/libkmc/%.o:   OPTFLAGS := -O1 -fno-fast-math
$(BUILD_DIR)/$(LIBNUSYS_DIR)/nuboot.c.o: LIBNUSYS_OPTFLAGS :=
$(BUILD_DIR)/$(LIBNUSYS_DIR)/nusched.c.o: LIBNUSYS_DEFINES += -DNU_DEBUG

.PHONY: all extract clean distclean setup tools format lint pycheck

all: $(ROM)
ifneq ($(COMPARE),0)
	@sha1sum -c $(TARGET).sha1
endif

extract:
	$(PYTHON) -m splat split $(SPLAT_YAML)
	$(PYTHON) tools/extract_textures.py

setup: tools extract

CLANG_FORMAT ?= clang-format-23
FORMAT_FILES := $(shell git ls-files 'src/*.c' 'src/*.h' 'include/*.h')

format:
	$(CLANG_FORMAT) -i $(FORMAT_FILES)
	.venv/bin/ruff format --quiet

# Lints and type-checks the Python tools, with the settings in pyproject.toml
pycheck:
	.venv/bin/ruff format --check --quiet
	.venv/bin/ruff check --quiet
	.venv/bin/basedpyright --level warning
	.venv/bin/vulture

CLANG_TIDY ?= run-clang-tidy-23
# src/libkmc is KMC's C library, which keeps the C library's names
LINT_FILES := $(filter-out src/libkmc/%,$(filter %.c,$(FORMAT_FILES)))

$(BUILD_DIR)/compile_commands.json: tools/compile_commands.py Makefile
	@mkdir -p $(BUILD_DIR)
	@python3 tools/compile_commands.py $@ $(INCLUDES) $(DEFINES) -- $(LINT_FILES)

LINT_JOBS ?= 4

# clang can't parse everything GCC 2.7 accepts, such as nested functions,
# so only the naming diagnostics fail the check. The rest stay in the log.
lint: $(BUILD_DIR)/compile_commands.json
	@$(CLANG_TIDY) -p $(BUILD_DIR) -j $(LINT_JOBS) -quiet $(LINT_FILES) \
		> $(BUILD_DIR)/lint.log 2>&1 || true
	@! grep -A2 '\[readability-identifier-naming' $(BUILD_DIR)/lint.log

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

$(BUILD_DIR)/$(TARGET).ld: $(SPLAT_LD)
	@mkdir -p $(dir $@)
	sed 's|build/|$(BUILD_DIR)/|g' $< > $@

$(ROM): $(ELF)
	$(OBJCOPY) -O binary --gap-fill=0xFF $< $@

$(ELF): $(O_FILES) $(LD_SCRIPT) undefined_syms.txt
	$(LD) $(LDFLAGS) -o $@

$(BUILD_DIR)/%.s.o: %.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -o $@ $<

$(BUILD_DIR)/%.bin.o: %.bin
	@mkdir -p $(dir $@)
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

# A C file is rebuilt whenever the assembly it includes with INCLUDE_ASM changes
.SECONDEXPANSION:
$(BUILD_DIR)/%.c.o: %.c $$(wildcard asm/nonmatchings/$$(subst src/,,$$*)/*.s)
	@mkdir -p $(dir $@)
ifneq ($(RUN_CC_CHECK),0)
	$(CC_CHECK) $(INCLUDES) $(DEFINES) $<
endif
	{ echo '# 1 "$<"'; $(ICONV) $<; } > $(BUILD_DIR)/$*.sjis.c
	$(DEPS) $(CC) -c $(CFLAGS) -I$(dir $<) $(INCLUDES) $(DEFINES) -o $@ $(BUILD_DIR)/$*.sjis.c

# main_bss.c defines libmus's bss objects too, whose types only its sources
# declare
$(BUILD_DIR)/src/main_bss.c.o: INCLUDES += -I$(LIBMUS_DIR)/src

$(BUILD_DIR)/$(ULTRALIB_DIR)/%.c.o: $(ULTRALIB_DIR)/%.c
	@mkdir -p $(dir $@)
	$(DEPS) $(LIB_CC) -c $(ULTRALIB_CFLAGS) $(ULTRALIB_INCLUDES) $(ULTRALIB_DEFINES) -o $@ $<

# The game's own variants of libultra objects, built the same way
$(BUILD_DIR)/src/libultra/%.c.o: src/libultra/%.c
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

$(BUILD_DIR)/$(LIBNUSYS_DIR)/%.c.o: $(LIBNUSYS_DIR)/%.c $(BUILD_DIR)/$(LIBNUSYS_DIR)/nusys.h
	@mkdir -p $(dir $@)
	tr -d '\r' < $< > $(@:.o=)
	$(DEPS) $(LIB_CC) -c $(LIBNUSYS_CFLAGS) $(LIBNUSYS_INCLUDES) $(LIBNUSYS_DEFINES) -o $@ $(@:.o=)

# Game code includes NuSystem's header through its converted copy, so the
# copy must exist before any game file compiles. The .d files make the
# files that include it rebuild when it changes.
$(filter $(BUILD_DIR)/src/%,$(O_FILES)): | $(BUILD_DIR)/$(LIBNUSYS_DIR)/nusys.h

$(BUILD_DIR)/$(LIBNUSYS_DIR)/nusys.h: $(LIBNUSYS_DIR)/nusys.h
	@mkdir -p $(dir $@)
	tr -d '\r' < $< > $@

# splat names data-only objects as if they were C, so assembly files that
# only hold data (such as gu/libm_vals.s) are found under a .c.o name
$(BUILD_DIR)/$(ULTRALIB_DIR)/%.c.o: $(ULTRALIB_DIR)/%.s
	@mkdir -p $(dir $@)
	$(DEPS) $(CC) -c $(ULTRALIB_ASFLAGS) $(ULTRALIB_INCLUDES) $(ULTRALIB_DEFINES) -o $@ $<

-include $(BUILD_DIR)/splat/$(TARGET).d
-include $(O_FILES:.o=.d)
# A header that a .d file lists but that has since gone only forces the
# rebuild that writes a new list
%.h: ;
