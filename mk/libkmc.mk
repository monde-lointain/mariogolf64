# libkmc was built with `gcc -O` (not -O2) per its genn64.bat; -O is the only
# libkmc-specific override over the project default flags.
LIBKMC_CFLAGS := $(subst -O2,-O,$(CFLAGS))
$(BUILD_DIR)/$(SRC_DIR)/libkmc/%.o: C_PROFILE_CFLAGS = $(LIBKMC_CFLAGS)

# Vendored libkmc soft-float / 64-bit-math TUs: KMC register conventions (move ->
# addu encoding), assembled with KMC `as` directly. No objcopy alignment step
# (KMC `as` already pads to the 16-byte slot). -I src/libkmc resolves mcvtld.s's
# `.include "mips_as.h"`; it is harmless for the self-contained mmuldi3.s.
$(BUILD_DIR)/$(SRC_DIR)/libkmc/%.o: $(SRC_DIR)/libkmc/%.s
	@echo '  AS      $<'
	$(Q)$(KMC_AS) -EB -mips2 -I src/libkmc -o $@.tmp $<
	$(Q)cp $@.tmp $@
	$(Q)rm $@.tmp

# mcvtld.s includes mips_as.h; declare the dependency so header edits rebuild it.
$(BUILD_DIR)/$(SRC_DIR)/libkmc/mcvtld.o: $(SRC_DIR)/libkmc/mips_as.h
