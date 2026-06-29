/*
 * n_syndelete.c
 *
 * naudio synthesizer driver: tear-down. Detaches every client from the global
 * synthesizer instance so the audio frame builder stops servicing them.
 */
#include "n_synthInternals.h"  // IWYU pragma: keep

/*
 * Shut the synthesizer down by emptying its client list: clearing the head of
 * the player chain leaves nothing for the audio frame to walk. The voice and
 * heap storage is owned by the caller's heap and is not freed here.
 */
void n_alSynDelete(void) { n_syn->head = 0; }
