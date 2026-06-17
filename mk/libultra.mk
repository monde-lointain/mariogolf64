# libultra mirrors ultralib's VERSION_J gcc.mk profile: -O3, -mips3 (the project
# default), -DBUILD_VERSION=VERSION_J for version-conditional code.
#
# -fsigned-char deviates from ultralib's documented -funsigned-char: this ROM's
# libultra was built signed (string.c's strchr/strlen sign-extend chars with
# lb + sll/sra, not lbu/andi), and only -fsigned-char reproduces the ROM.
#
# -I include/libultra/compiler/gcc is PREPENDED so libultra's own standard C
# headers (e.g. stdlib.h with lldiv_t) win over include/libkmc/stdlib.h; it is
# libultra-only, so other trees keep resolving stdlib.h to the libkmc copy.
#
# -DF3DEX_GBI_2 selects OS_YIELD_DATA_SIZE=0xc00 in PR/sptask.h (the #else gives
# 0x900); MG64 runs the F3DEX2 microcode, so this is the accurate define.
LIBULTRA_CFLAGS := -I include/libultra/compiler/gcc $(subst -O2,-O3,$(CFLAGS)) -fsigned-char -DBUILD_VERSION=VERSION_J -DF3DEX_GBI_2 -I include/libultra/PR
$(BUILD_DIR)/$(SRC_DIR)/libultra/%.o: C_PROFILE_CFLAGS = $(LIBULTRA_CFLAGS)

# Vendored libultra hand-asm: assemble with the same KMC/N64 gcc profile ultralib
# uses. LOAD-BEARING: KMC pads each function's .text up to its 16-byte ROM slot,
# so a verbatim 0xC ultralib TU lands as the 0x10 the ROM expects. Modern
# mips-linux-gnu `as` emits the bare 0xC and shifts the subseg (SHA-1 break),
# which is why these must NOT fall through to the generic modern-GAS src/%.s rule.
LIBULTRA_ASFLAGS := -x assembler-with-cpp -w -nostdinc -c -G 0 -mips3 -mgp32 -mfp32 \
	-DMIPSEB -D_LANGUAGE_ASSEMBLY -D_MIPS_SIM=1 -D_ULTRA64 \
	-D_MIPS_SZLONG=32 -D__USE_ISOC99 -DBUILD_VERSION=VERSION_J -D_FINALROM \
	-I include -I include/libultra -I include/libultra/PR -I include/libultra/compiler/gcc

$(BUILD_DIR)/$(SRC_DIR)/libultra/%.o: $(SRC_DIR)/libultra/%.s
	@echo '  AS      $<'
	$(Q)$(CC) $(LIBULTRA_ASFLAGS) -o $@.tmp $<
	$(Q)$(OBJCOPY) $(OBJCOPY_ALIGN) $@.tmp $@
	$(Q)rm $@.tmp
