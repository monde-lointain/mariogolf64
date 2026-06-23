/*
 * sl.c
 *
 * Top-level audio-library lifecycle and the intrusive doubly-linked-list
 * primitive (ALLink) used throughout the synthesizer to thread players,
 * voices, and events.
 */
#include <libaudio.h>

/* The one synthesis driver instance; non-null only between init and close. */
ALGlobals* alGlobals = 0;

/*
 * Bring up the audio library: claim the globals slot and build the synthesis
 * driver from config c. A second call while already initialized is a no-op,
 * so the library is effectively a singleton.
 */
void alInit(ALGlobals* g, ALSynConfig* c) {
  if (!alGlobals) {
    alGlobals = g;
    alSynNew(&alGlobals->drvr, c);
  }
}

/* Tear the driver down and release the globals slot for a future alInit. */
void alClose(ALGlobals* glob) {
  if (alGlobals) {
    alSynDelete(&glob->drvr);
    alGlobals = 0;
  }
}

/* Splice ln into the list immediately after node to. */
void alLink(ALLink* ln, ALLink* to) {
  ln->next = to->next;
  ln->prev = to;
  if (to->next) to->next->prev = ln;
  to->next = ln;
}

/* Unsplice ln, repairing its neighbors' links. */
void alUnlink(ALLink* ln) {
  if (ln->next) ln->next->prev = ln->prev;
  if (ln->prev) ln->prev->next = ln->next;
}
