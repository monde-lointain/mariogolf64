/*
 * n_libaudio_sn_sc.h
 *
 * Source-compatibility shim that lets code written for the stock single-synth
 * AL audio library compile against the n_audio ("single command") driver. The
 * stock al* calls pass an explicit synth/globals handle as their first
 * argument; n_audio keeps one implicit global synth instead, so each macro
 * here forwards to the matching n_al* function while dropping that leading
 * handle argument `a`. Include it after n_libaudio_sc.h. (No include guard:
 * it defines only macros and is meant to be pulled in directly.)
 */

/* Map the stock AL handle/type names onto their n_audio equivalents. */
#define ALVoice N_ALVoice
#define ALSynth N_ALSynth
#define ALGlobals N_ALGlobals

/* Rewrite each al* call to its n_al* form, dropping the leading handle arg. */
#define alSynAddPlayer(a, b) n_alSynAddPlayer(b)
#define alSynAllocFX(a, b, c, d) n_alSynAllocFX(b, c, d)
#define alSynAllocVoice(a, b, c) n_alSynAllocVoice(b, c)
#define alSynDelete(a) n_alSynDelete()
#define alSynFreeVoice(a, b) n_alSynFreeVoice(b)
#define alSynGetFXRef(a, b, c) n_alSynGetFXRef(b, c)
#define alSynGetPriority(a, b) n_alSynGetPriority(b)
#define alSynRemovePlayer(a, b) n_alSynRemovePlayer(b)
#define alSynSetFXMix(a, b, c) n_alSynSetFXMix(b, c)
#define alSynSetFXParam(a, b, c, d) n_alSynSetFXParam(b, c, d)
#define alSynSetFXtype(a, b, c, d) n_alSynSetFXtype(b, c, d)
#define alSynSetPan(a, b, c) n_alSynSetPan(b, c)
#define alSynSetPitch(a, b, c) n_alSynSetPitch(b, c)
#define alSynSetPriority(a, b, c) n_alSynSetPriority(b, c)
#define alSynSetVol(a, b, c, d) n_alSynSetVol(b, c, d)
#define alSynStartVoice(a, b, c) n_alSynStartVoice(b, c)
#define alSynStartVoiceParams(a, b, c, d, e, f, g, h) \
  n_alSynStartVoiceParams(b, c, d, e, f, g, h)
#define alSynStopVoice(a, b) n_alSynStopVoice(b)
#define alSynNew(a, b) n_alSynNew(b)
#define alInit n_alInit
#define alClose(a) n_alClose()
#define alAudioFrame n_alAudioFrame
