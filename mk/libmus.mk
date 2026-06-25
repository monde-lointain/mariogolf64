# libmus (the "Software Creations" Music Tools library, libn_mus / SUPPORT_NAUDIO
# variant): KMC GCC -O3 per the audio coddog pin (tools/coddog/audio_pins.tsv:
# libmus = n64sdkmod_naudio, kmc, O3 -- the game rev is libmus 3.14 = n64sdkmod, NOT
# the DiskLS 3.11 sdk40v312 rows (S143: identical leaf bodies but aud_thread
# EXTRA_SAMPLES_N 20 vs 15; see tools/audio_ref_versions.tsv). -O3 is bumped from the base -O2 the same
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
# -U_FINALROM: MG64's libmus library was built NON-FINALROM (the base CFLAGS -D_FINALROM is the
# game/libultra setting; the 3rd-party Software Creations libmus object was compiled debug/non-final).
# The tell is OSScTask (sched.h): its `#ifndef _FINALROM` trailing startTime+totalTime (2x OSTime =
# 0x10) is PRESENT in the ROM's __OsSchedDoTask (aud_sched.c), giving a 0xA0 stack frame vs the 0x90
# FINALROM build (S145). No other libmus file has _FINALROM-sensitive codegen (the other conditionals
# are OS_NUM_EVENTS macros + an alParseAbiCL proto, unused here), so the 3 banked siblings are unaffected.
LIBMUS_CFLAGS := -I src/libmus -I include/libmus/PR $(subst -O2,-O3,$(CFLAGS)) -I include/libultra/PR -DSUPPORT_NAUDIO -U_FINALROM
$(BUILD_DIR)/$(SRC_DIR)/libmus/%.o: C_PROFILE_CFLAGS = $(LIBMUS_CFLAGS)
