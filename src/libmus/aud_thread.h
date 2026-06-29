/*
 * aud_thread.h
 *
 * Interface to the libmus audio manager: the OS thread that drives audio
 * synthesis one frame at a time. __MusIntAudManInit builds the synthesis driver
 * and spawns that thread; __libmus_alglobals is the shared audio-library state
 * it initializes.
 */
#ifndef _LIBMUS_AUD_THREAD_H_
#define _LIBMUS_AUD_THREAD_H_

// Audio-library global state, initialized by __MusIntAudManInit via alInit.
extern ALGlobals __libmus_alglobals;

// Build the audio synthesis driver and start the audio manager thread.
void __MusIntAudManInit(musConfig* config, int vsyncs_per_second, int fx_type);

#endif
