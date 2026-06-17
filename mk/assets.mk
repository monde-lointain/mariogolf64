# Binary assets embedded directly as big-endian MIPS objects.
$(BUILD_DIR)/$(ASSETS_DIR)/%.o: $(ASSETS_DIR)/%.bin
	@echo '  BIN     $<'
	$(Q)$(OBJCOPY) -I binary -O elf32-tradbigmips $< $@
