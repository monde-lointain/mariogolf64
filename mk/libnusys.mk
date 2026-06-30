# libnusys: -DUSE_EPI enables the nuPi* function bodies (upstream sets USE_EPI=1).
LIBNUSYS_CFLAGS := $(CFLAGS) -DUSE_EPI
$(BUILD_DIR)/$(SRC_DIR)/libnusys/%.o: C_PROFILE_CFLAGS = $(LIBNUSYS_CFLAGS)

# nuboot.c (the game-embedded nusys boot: main + idle) shipped compiled at -O0
# (frame pointer kept, no CSE / no instruction scheduling), unlike the rest of
# libnusys (-O2). File-SPECIFIC override; it beats the libnusys/%.o pattern above.
# Verified by isolated compile: idle is byte-exact at -O0 (53 instrs).
NUBOOT_O0_CFLAGS := $(subst -O2,-O0,$(CFLAGS))
$(BUILD_DIR)/$(SRC_DIR)/libnusys/nuboot.o: C_PROFILE_CFLAGS := $(NUBOOT_O0_CFLAGS)
