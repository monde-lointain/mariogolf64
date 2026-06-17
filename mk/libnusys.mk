# libnusys: -DUSE_EPI enables the nuPi* function bodies (upstream sets USE_EPI=1).
LIBNUSYS_CFLAGS := $(CFLAGS) -DUSE_EPI
$(BUILD_DIR)/$(SRC_DIR)/libnusys/%.o: C_PROFILE_CFLAGS = $(LIBNUSYS_CFLAGS)
