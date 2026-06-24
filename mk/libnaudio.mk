# libnaudio (the n_audio_sc / "Software Creations" variant): KMC GCC -O3 per the
# audio coddog pin (tools/coddog/audio_pins.tsv: libnaudio = n64sdkmod_sc, kmc, O3).
# -O3 is bumped from the base -O2 the same way libultra does it.
#
# -I src/libnaudio is PREPENDED so the n_audio_sc internal headers
# (n_synthInternals.h / synthInternals.h / n_abi.h, vendored under src/ per the
# header-placement convention) win over the libultra-internal copies of the same
# names (e.g. include/libultra/internal/synthInternals.h differs from the SC one).
# -I include/libultra/PR resolves the bare <os_internal.h> / <ultraerror.h> /
# <libaudio.h> includes (those headers already live under include/libultra/PR).
# The public n_libaudio_sc.h resolves via the global -I include/libnaudio.
#
# -DN_MICRO=1 mirrors the upstream n_audio_sc Makefile, which builds the WHOLE
# library with N_MICRO=1 (the "micro" command-stream variant). Functions that
# branch on `#ifdef N_MICRO` (n_save.c, n_resample.c, n_reverb.c, n_env.c,
# n_load.c, n_synthesizer.c) emit the micro path; without the pin n_save.c takes
# the longer non-micro path and SHA-misses (the #needs-define hazard, same class
# as the -DF3DEX_GBI_2 libultra pin). The setter mirrors (n_syn*.c) have no
# N_MICRO branch, so the pin is byte-neutral for them.
LIBNAUDIO_CFLAGS := -I src/libnaudio $(subst -O2,-O3,$(CFLAGS)) -I include/libultra/PR -DN_MICRO=1
$(BUILD_DIR)/$(SRC_DIR)/libnaudio/%.o: C_PROFILE_CFLAGS = $(LIBNAUDIO_CFLAGS)
