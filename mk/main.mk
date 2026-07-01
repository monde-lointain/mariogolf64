# Main-segment game code (src/main/). MG64's RSP microcode is F3DEX2
# (gspF3DEX2.fifo 2.08), so the game's display-list code uses the F3DEX2 GBI
# opcodes (e.g. G_RDPHALF_1=0xE1, G_RDPHALF_2=0xF1 vs the F3DEX 0xB4/0xB3).
# -DF3DEX_GBI_2 selects them in PR/gbi.h. Otherwise the -O2 game profile.
# This overrides the generic src/%.o rule (mk/src.mk) for main/ targets.
MAIN_CFLAGS := $(CFLAGS) -DF3DEX_GBI_2
$(BUILD_DIR)/$(SRC_DIR)/main/%.o: C_PROFILE_CFLAGS = $(MAIN_CFLAGS)

# func_8006A000.c (integer vector-magnitude helpers) computes its magnitude with
# a double sqrt. KMC GCC only emits the bare `sqrt.d` opcode (no errno/NaN guard,
# matching the ROM) under -ffast-math; the default profile emits a guarded
# sqrt.d + library `jal sqrt` fallback. Per-file override (the sibling main/ files
# stay on the plain profile: mgu/mtxutil's float math matched without it).
$(BUILD_DIR)/$(SRC_DIR)/main/func_8006A000.o: C_PROFILE_CFLAGS := $(MAIN_CFLAGS) -ffast-math
