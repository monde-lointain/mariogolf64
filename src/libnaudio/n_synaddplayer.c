/*
 * Synth player registration.
 *
 * Registers an audio client (player) with the naudio synthesizer driver by
 * prepending it to the driver's singly-linked list of active players, seeding
 * its sample clock so it is scheduled relative to the synth's current time.
 */
#include <os_internal.h>
#include "n_synthInternals.h"

/* Add player client to the synth's active-player list. */
void n_alSynAddPlayer(ALPlayer* client) {
  /* Mask interrupts across the whole insertion so the audio interrupt/thread
   * never observes a half-linked list; the original mask is restored below. */
  OSIntMask mask = osSetIntMask(OS_IM_NONE);

  /* Seed the player from the synth's current sample clock so it is scheduled
   * relative to "now", then prepend it to the list head. */
  client->samplesLeft = n_syn->curSamples;
  client->next = n_syn->head;
  n_syn->head = client;

  osSetIntMask(mask);
}
