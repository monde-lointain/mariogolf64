/*
 * n_synaddplayer.c
 *
 * naudio synthesizer driver: register a player (sequence or sound-effect
 * client) with the synth. Each player exposes a handler the synth calls back
 * when its next scheduled parameter change comes due; this routine links the
 * player into the synth's client list.
 */
#include <os_internal.h>       // IWYU pragma: keep
#include "n_synthInternals.h"  // IWYU pragma: keep

/*
 * Add a client to the head of the synthesizer's player list. The list is walked
 * by the audio thread while building a frame, so the link is performed with
 * interrupts masked to keep it atomic against that thread. The client's next
 * callback time is seeded to the current sample clock so it fires on the coming
 * frame.
 */
void n_alSynAddPlayer(ALPlayer* client) {
  // Mask interrupts while splicing the shared client list.
  OSIntMask mask = osSetIntMask(OS_IM_NONE);

  client->samplesLeft = n_syn->curSamples;
  client->next = n_syn->head;
  n_syn->head = client;

  osSetIntMask(mask);
}
