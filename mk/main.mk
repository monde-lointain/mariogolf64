# Main-segment game code (src/main/). MG64's RSP microcode is F3DEX2
# (gspF3DEX2.fifo 2.08), so the game's display-list code uses the F3DEX2 GBI
# opcodes (e.g. G_RDPHALF_1=0xE1, G_RDPHALF_2=0xF1 vs the F3DEX 0xB4/0xB3).
# -DF3DEX_GBI_2 selects them in PR/gbi.h. Otherwise the -O2 game profile.
# This overrides the generic src/%.o rule (mk/src.mk) for main/ targets.
MAIN_CFLAGS := $(CFLAGS) -DF3DEX_GBI_2
$(BUILD_DIR)/$(SRC_DIR)/main/%.o: C_PROFILE_CFLAGS = $(MAIN_CFLAGS)

# func_8006A000.c is the whole main-segment math one-tu (0x8006A000-0x8006A2C0).
# Its double-precision magnitudes (calculate_*_safe) emit a bare `sqrt.d`, and its
# single-precision helpers (hypotf_2d, calc_vec3_magnitude) a bare `sqrt.s`; both
# are the BUILT_IN_FSQRT builtin (identical expr.c path, mode DF->sqrtdf2 vs
# SF->sqrtsf2). Without -ffast-math KMC GCC guards each with a c.eq/bc1t NaN check
# + library `jal sqrt` fallback the ROM lacks; -ffast-math drops the guard to the
# bare opcode. Per-file override (sibling main/ files stay on the plain -O2 profile;
# mgu/mtxutil's float math matched without it).
$(BUILD_DIR)/$(SRC_DIR)/main/func_8006A000.o: C_PROFILE_CFLAGS := $(MAIN_CFLAGS) -ffast-math

# func_80078910.c (effects/particle TU). func_8007E30C computes a 2D magnitude via
# sqrtf() -> bare `sqrt.s` in the ROM, so the whole TU was compiled -ffast-math.
# Same BUILT_IN_FSQRT guard-drop as func_8006A000; per-file override. The already
# banked FP siblings match the (-ffast-math) ROM, so they are fast-math-invariant
# and stay matched under the flag.
$(BUILD_DIR)/$(SRC_DIR)/main/func_80078910.o: C_PROFILE_CFLAGS := $(MAIN_CFLAGS) -ffast-math
