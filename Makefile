MAKEFLAGS += --no-builtin-rules

# Fail if no baserom
ifeq (,$(wildcard baserom.z64))
$(error baserom.z64 not found. Place your ROM in the project root.)
endif

BASENAME := mariogolf64
TARGET := $(BASENAME).z64
LD_SCRIPT := $(BASENAME).ld

# Directories
BUILD_DIR := build
ASM_DIR := asm
ASSETS_DIR := assets

# Tools
CROSS := mips-linux-gnu-
AS := $(CROSS)as
LD := $(CROSS)ld
OBJCOPY := $(CROSS)objcopy

# Flags
ASFLAGS := -march=vr4300 -mabi=32 -G 0 -I include

# Find all source files
ASM_FILES := $(wildcard $(ASM_DIR)/*.s) $(wildcard $(ASM_DIR)/data/*.s)
BIN_FILES := $(wildcard $(ASSETS_DIR)/*.bin)

# Object files
ASM_O_FILES := $(patsubst $(ASM_DIR)/%.s,$(BUILD_DIR)/$(ASM_DIR)/%.o,$(ASM_FILES))
BIN_O_FILES := $(patsubst $(ASSETS_DIR)/%.bin,$(BUILD_DIR)/$(ASSETS_DIR)/%.o,$(BIN_FILES))

O_FILES := $(ASM_O_FILES) $(BIN_O_FILES)

# Default target
all: $(BUILD_DIR)/$(TARGET)
	@md5sum $< baserom.z64

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
# Fix databin/rodatabin: use objcopy'd binaries instead of assembled .incbin (avoids MIPS 16-byte alignment)
	sed -i 's|build/asm/data/main_data.o(.data)|build/assets/main_data.databin.o(.data)|' $(LD_SCRIPT)
	sed -i 's|build/asm/data/main_rodata.o(.rodata)|build/assets/main_rodata.rodatabin.o(.data)|' $(LD_SCRIPT)

.PHONY: all clean distclean setup extract

# Create build directories
$(shell mkdir -p $(BUILD_DIR)/$(ASM_DIR)/data $(BUILD_DIR)/$(ASSETS_DIR))

# Link
$(BUILD_DIR)/$(TARGET): $(BUILD_DIR)/$(BASENAME).elf
	$(OBJCOPY) -O binary $< $@

$(BUILD_DIR)/$(BASENAME).elf: $(O_FILES) $(LD_SCRIPT) undefined_funcs_auto.txt undefined_syms_auto.txt hardware_regs.txt extern_syms.txt
	$(LD) --no-check-sections --omagic -T hardware_regs.txt -T undefined_funcs_auto.txt -T undefined_syms_auto.txt -T extern_syms.txt -T $(LD_SCRIPT) -Map $(BUILD_DIR)/$(BASENAME).map -o $@

# Assemble
$(BUILD_DIR)/$(ASM_DIR)/%.o: $(ASM_DIR)/%.s
	$(AS) $(ASFLAGS) -o $@ $<

# Binary to object
$(BUILD_DIR)/$(ASSETS_DIR)/%.o: $(ASSETS_DIR)/%.bin
	$(OBJCOPY) -I binary -O elf32-tradbigmips $< $@
