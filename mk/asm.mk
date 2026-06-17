# Splat-extracted assembly: preprocess with cpp, assemble with system as.
$(BUILD_DIR)/$(ASM_DIR)/%.o: $(ASM_DIR)/%.s
	$(assemble-modern)
