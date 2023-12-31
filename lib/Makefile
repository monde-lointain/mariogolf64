# TODO: Add nusys

MAKEFLAGS += --no-builtin-rules



# Set prefix to mips binutils binaries (mips-linux-gnu-ld => 'mips-linux-gnu-') - Change at your own risk!
# In nearly all cases, not having 'mips-linux-gnu-*' binaries on the PATH is indicative of missing dependencies
MIPS_BINUTILS_PREFIX ?= mips-linux-gnu-



### Output ###

BUILD_DIR := ../build/lib



#### Setup ####

MAKE = make

UNAME_S := $(shell uname -s)
ifeq ($(OS),Windows_NT)
	DETECTED_OS := windows
else ifeq ($(UNAME_S),Linux)
	DETECTED_OS := linux
else ifeq ($(UNAME_S),Darwin)
	DETECTED_OS := macos
	MAKE := gmake
endif



#### Tools ####
ifneq ($(shell type $(MIPS_BINUTILS_PREFIX)ld >/dev/null 2>/dev/null; echo $$?), 0)
	$(error Please install or build $(MIPS_BINUTILS_PREFIX))
endif


CC         := COMPILER_PATH=../tools/cc ../tools/cc/gcc

STRIP      := $(MIPS_BINUTILS_PREFIX)strip


IINC       := -Iultralib/include -Iultralib/include/gcc -Iultralib/include/PR -Iultralib/src -Ilibmus/src

# TODO: Change these if necessary
OPTFLAGS        := -O3
MIPS_VERSION    := -mips3
CFLAGS          := -nostdinc -G 0 -mgp32 -mfp32 -funsigned-char
ASFLAGS         := -x assembler-with-cpp
WARNINGS        := -w
COMMON_DEFINES  := -D_MIPS_SZLONG=32 -D__USE_ISOC99
GBI_DEFINES     := -DF3DEX_GBI
RELEASE_DEFINES := -DNDEBUG -D_FINALROM
AS_DEFINES      := -D_LANGUAGE_ASSEMBLY -DMIPSEB -D_ULTRA64 -D_MIPS_SIM=1
C_DEFINES       := -D_LANGUAGE_C



#### Files ####

SRC_DIRS := $(shell find ultralib/src -type d \( -name voice -o -name mgu \) -prune -o -print)

C_FILES       := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.c))
S_FILES       := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.s))
O_FILES       := $(foreach f,$(C_FILES:.c=.o),$(BUILD_DIR)/$f) \
                 $(foreach f,$(S_FILES:.s=.o),$(BUILD_DIR)/$f)

# Create build directories
$(shell mkdir -p $(foreach dir,$(SRC_DIRS),$(BUILD_DIR)/$(dir)))

export VR4300MUL := ON



#### Main Targets ###

all: $(O_FILES)

clean:
	$(RM) -r $(BUILD_DIR)/lib

distclean: clean

.PHONY: all clean distclean
.DEFAULT_GOAL := all
# Prevent removing intermediate files
.SECONDARY:

$(BUILD_DIR)/%.o: %.s
	$(CC) -c $(CFLAGS) $(IINC) -I $(dir $*) $(WARNINGS) $(MIPS_VERSION) $(COMMON_DEFINES) $(RELEASE_DEFINES) $(GBI_DEFINES) $(AS_DEFINES) $(ASFLAGS) -o $@ $<
	$(STRIP) $@ -N dummy-symbol-name

$(BUILD_DIR)/%.o: %.c
	$(CC) -c $(CFLAGS) $(IINC) -I $(dir $*) $(WARNINGS) $(MIPS_VERSION) $(COMMON_DEFINES) $(RELEASE_DEFINES) $(GBI_DEFINES) $(C_DEFINES) $(OPTFLAGS) -o $@ $<
	$(STRIP) $@ -N dummy-symbol-name
