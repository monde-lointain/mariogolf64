/*
 * player_fx.h
 *
 * Custom effects (reverb) unit for the libmus music driver. libmus supplies its
 * own multi-section reverb in place of the stock audio-library effect so that a
 * song can carry and switch effect presets at runtime. This header declares the
 * custom synth/effect constructors, which deliberately alias the standard
 * audio-library entry points (alInit, etc.) so existing callers transparently
 * pick up libmus's effect, plus the REVERB_MEM working state. Declarations come
 * in parallel non-naudio and naudio (SUPPORT_NAUDIO) forms.
 */
#ifndef _PLAYER_FX_H_
#define _PLAYER_FX_H_

#include "synthInternals.h"

// Under the naudio synth, pull in the naudio-side audio headers and internals
// so the Custom* hooks below match the naudio driver's signatures.
#ifdef SUPPORT_NAUDIO
#include <n_libaudio_sc.h>
#include <n_libaudio_sn_sc.h>
#include "n_synthInternals.h"
#endif

// Select a different custom effect/reverb preset while playing (the
// SUPPORT_FXCHANGE runtime hook); the argument is the effect index.
int ChangeCustomEffect(s32);

// Initialise the synth driver with libmus's custom effect installed. Aliased to
// the library's standard init entry (see the alInit/n_alInit define below) so
// every caller of alInit() gets the reverb-aware driver.
void CustomInit(ALGlobals*, ALSynConfig*);
// Redirect the audio library's standard synth-init entry to CustomInit, so any
// client that calls alInit() (or n_alInit() under naudio) builds the driver
// with libmus's effect rather than the stock one.
//
// CustomSynNew builds the synth driver; CustomAllocFX allocates the custom
// effect on a synth bus (replacing alSynAllocFX). The naudio variants drop the
// ALSynth* argument because naudio keeps the driver in a global.
#ifndef SUPPORT_NAUDIO
#define alInit CustomInit
void CustomSynNew(ALSynth*, ALSynConfig*);
ALFxRef* CustomAllocFX(ALSynth*, s16, ALSynConfig*, ALHeap*);
#else
#define n_alInit CustomInit
void CustomSynNew(ALSynConfig*);
ALFxRef CustomAllocFX(s16, ALSynConfig*, ALHeap*);
#endif

// Construct the custom effect node in the supplied heap (naudio takes an
// out-pointer because it returns the node by reference).
#ifndef SUPPORT_NAUDIO
void CustomFxNew(ALFx*, ALSynConfig*, ALHeap*);
#else
void CustomFxNew(ALFx**, ALSynConfig*, ALHeap*);
#endif

// Load the reverb's parameters from a preset, passed as a flat s32 array.
void CustomFxSet(s32*);
// Working memory for the custom multi-section reverb: the owning driver, the
// per-section delay lines, and the resampler/low-pass DSP blocks (with their
// RSP working states) that form the feedback path.
typedef struct {
  ALSynth* synth;           // owning synth driver
  int sections;             // number of reverb delay sections (taps)
  u32 length;               // delay-line length, in samples
  s32* pParams;             // active preset parameters driving the effect
  ALDelay* pDelay;          // per-section delay-line descriptors
  s16* pBase;               // base of the delay-line sample buffer
  ALResampler* pResampler;  // resampler for pitch-modulated delay
  RESAMPLE_STATE* pResampleState;  // its RSP working state
  ALLowPass* pLowPass;             // low-pass filter on the feedback path
  POLEF_STATE* pLpfState;          // pole-filter working state for the low-pass
} REVERB_MEM;

#endif
