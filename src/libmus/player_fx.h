/*
 * player_fx.h
 *
 * libmus player: custom audio-effect (reverb) support.
 *
 * Declares the "Custom" overrides for the audio library's synth-driver and
 * effect bring-up path (alInit / alSynNew / alSynAllocFX / alFxNew), plus
 * REVERB_MEM -- the block that records every audio-heap allocation behind the
 * player's single, reusable effect. Audio-heap memory is never freed, so one
 * effect is allocated once at its worst-case size and then re-parameterized in
 * place rather than reallocated whenever the active effect changes.
 */
#ifndef _PLAYER_FX_H_
#define _PLAYER_FX_H_

#include "synthInternals.h"

/*
 * n_audio variant: when the game links the "n_audio" synthesizer instead of
 * the classic audio library, pull in its parallel type and entry-point
 * headers. The Custom* signatures below switch to match.
 */
#ifdef SUPPORT_NAUDIO
#include <n_libaudio_sc.h>
#include <n_libaudio_sn_sc.h>
#include "n_synthInternals.h"
#endif

/* Public API: switch the active effect to the preset selected by index. */
int ChangeCustomEffect(s32);

/* Replacement for alInit(): one-time synth-driver bring-up. */
void CustomInit(ALGlobals*, ALSynConfig*);

/*
 * Redirect the library's standard init symbol to CustomInit so existing call
 * sites reach the custom path unchanged. CustomSynNew / CustomAllocFX mirror
 * alSynNew() / alSynAllocFX(); the n_audio synth keeps its driver and bus
 * state in a global, so its variants drop the leading ALSynth* and return the
 * FX reference by value rather than by pointer.
 */
#ifndef SUPPORT_NAUDIO
#define alInit CustomInit
void CustomSynNew(ALSynth*, ALSynConfig*);
ALFxRef* CustomAllocFX(ALSynth*, s16, ALSynConfig*, ALHeap*);
#else
#define n_alInit CustomInit
void CustomSynNew(ALSynConfig*);
ALFxRef CustomAllocFX(s16, ALSynConfig*, ALHeap*);
#endif

/*
 * Allocate the effect's heap blocks at worst-case size, then hand off to
 * CustomFxSet(). The n_audio variant also allocates the ALFx object itself,
 * so it takes the slot by reference (ALFx**) rather than a caller-owned ALFx*.
 */
#ifndef SUPPORT_NAUDIO
void CustomFxNew(ALFx*, ALSynConfig*, ALHeap*);
#else
void CustomFxNew(ALFx**, ALSynConfig*, ALHeap*);
#endif

/* (Re)program the effect filter from an s32 param table; no allocation. */
void CustomFxSet(s32*);

/*
 * Records the audio-heap allocations behind the single shared effect, so a
 * later effect change can reuse the same memory instead of allocating again.
 */
typedef struct {
  ALSynth* synth; /* synth driver this effect is attached to */

  /* Active effect description. */
  int sections; /* number of delay sections */
  u32 length;   /* delay-line length, in samples */
  s32* pParams; /* parameter table driving the active effect */

  /* ALFx delay line wired onto the aux bus. */
  ALDelay* pDelay; /* per-section delay descriptors (worst-case count) */
  s16* pBase;      /* delay-line sample buffer (worst-case length) */

  /* Per-tap resampler (chorus / flange) and low-pass (damping) filter. */
  ALResampler* pResampler;
  RESAMPLE_STATE* pResampleState;
  ALLowPass* pLowPass;
  POLEF_STATE* pLpfState;
} REVERB_MEM;

#endif
