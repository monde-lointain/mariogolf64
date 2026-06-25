# libmus (the "Software Creations" Music Tools library, libn_mus / SUPPORT_NAUDIO
# variant): KMC GCC -O3 per the audio coddog pin (tools/coddog/audio_pins.tsv:
# libmus = sdk40v312_naudio, kmc, O3). -O3 is bumped from the base -O2 the same
# way libnaudio/libultra do it. The upstream SGI Makefile.sgi builds libn_mus with
# DEFINES='-DSUPPORT_NAUDIO' (the n_audio library/microcode variant MG64 links);
# audio_pin.py pins the empirical KMC -O3 over the SGI Makefile's -O2.
#
# -I src/libmus is PREPENDED so the libmus source-private headers (libmus_config.h,
# lib_memory.h, aud_*.h, ..., vendored under src/ per the header-placement
# convention) win. The public libmus.h (include/libmus) and the n_audio_sc alias
# headers (n_libaudio_sc.h / n_libaudio_sn_sc.h, include/libnaudio) and the bare
# <ultra64.h> / <libaudio.h> resolve via the base CFLAGS -I set.
#
# -DSUPPORT_NAUDIO selects the naudio path: lib_memory.c's `#ifndef SUPPORT_NAUDIO`
# picks <n_libaudio_sc.h> + <n_libaudio_sn_sc.h> (the alSyn*->n_alSyn* alias macros)
# over <libaudio.h>. (SUPPORT_FXCHANGE is set in libmus_config.h itself, not a -D.)
LIBMUS_CFLAGS := -I src/libmus -I include/libmus/PR $(subst -O2,-O3,$(CFLAGS)) -I include/libultra/PR -DSUPPORT_NAUDIO
$(BUILD_DIR)/$(SRC_DIR)/libmus/%.o: C_PROFILE_CFLAGS = $(LIBMUS_CFLAGS)
