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

# Per-directory compiler flags
$(BUILD_DIR)/lib/ultralib/%.o: OPTFLAGS := -O3
$(BUILD_DIR)/src/libkmc/%.o:   OPTFLAGS := -O1

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

-include $(BUILD_DIR)/splat/$(TARGET).d
