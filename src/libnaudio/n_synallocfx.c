/*
 * n_synallocfx.c
 *
 * naudio synthesizer driver: allocate an effects (reverb) unit on the aux bus.
 * The naudio synth routes its aux bus through an array of effect slots; this
 * builds a fresh effect and binds it into the requested slot.
 */
#include "n_synthInternals.h"

/*
 * Build an effects unit from config c into aux-bus effect slot `bus`, drawing
 * its working storage from heap hp, and return a reference to it. Unlike the
 * stock synth the bus-to-main routing is wired statically, so no extra source
 * linking is done here.
 */
ALFxRef n_alSynAllocFX(s16 bus, ALSynConfig* c, ALHeap* hp) {
  n_alFxNew(&n_syn->auxBus->fx_array[bus], c, hp);
  return (n_syn->auxBus->fx_array[bus]);
}
