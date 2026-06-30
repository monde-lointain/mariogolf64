/*
 * aud_thread.h
 *
 * Interface to libmus's audio manager thread. The implementation
 * (aud_thread.c) stands up the libaudio synthesizer, allocates the per-frame
 * command and output buffers, and runs a dedicated thread that builds and kicks
 * off one RSP audio task per audio frame.
 */

#ifndef _LIBMUS_AUD_THREAD_H_
#define _LIBMUS_AUD_THREAD_H_

/* Audio library global synthesizer state. __MusIntAudManInit() hands it to
 * alInit(), which registers it as the active globals; the synthesizer then
 * works through it while building each frame's RSP command list. Shared across
 * the driver, so it lives at file scope. */
extern ALGlobals __libmus_alglobals;

/*
 * Bring the audio manager online: build an ALSynConfig from config and init the
 * synthesizer, derive the per-frame sample budget from vsyncs_per_second (the
 * 50/60 Hz video rate), allocate the RSP command list, output task buffers, and
 * thread stack, then create and start the audio thread. fx_type selects the
 * synthesizer effect (reverb) preset. Called once at driver startup.
 */
void __MusIntAudManInit(musConfig* config, int vsyncs_per_second, int fx_type);

#endif
