# Compile C with KMC GCC (-> .s), then KMC binutils 2.6 `as` (-> .o). KMC `as`
# gives the KMC `move dst,zero` -> `addu` encoding natively and pads .text to its
# 16-byte section alignment. One recipe serves all C profiles; C_PROFILE_CFLAGS
# (overridden per tree in mk/lib*.mk) selects the flags, and `%` spans slashes so
# this also builds the nested lib paths.
#
# strip -N dummy-symbol-name drops the inert placeholder symbol KMC GCC emits so
# it can't collide at link.
$(BUILD_DIR)/$(SRC_DIR)/%.o: $(SRC_DIR)/%.c
	@echo '  CC      $<'
	$(Q)$(CC) -S $(C_PROFILE_CFLAGS) -o $@.s $<
	$(Q)$(KMC_AS) -EB -mips2 -G 0 -I include -o $@.tmp $@.s
	$(Q)cp $@.tmp $@
	$(Q)$(STRIP) $@ -N dummy-symbol-name
	$(Q)rm $@.s $@.tmp

# The remaining hand-written asm under src/ is the entry stub: modern
# mips-linux-gnu `as`, same recipe as asm/. It uses .set gp=64 + .include
# "macro.inc", which KMC `as` rejects. The more-specific libultra/libkmc rules
# win for those trees, so this catches only the leftover src/*.s.
$(BUILD_DIR)/$(SRC_DIR)/%.o: $(SRC_DIR)/%.s
	$(assemble-modern)
