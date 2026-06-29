/*
 * n_sl.c
 *
 * naudio audio-library lifecycle. Brings the naudio synthesis driver up and
 * down: n_alInit claims the global driver slot and constructs the synthesizer,
 * n_alClose destroys it and releases the slot. The naudio counterpart of the
 * base library's alInit/alClose. n_alGlobals and n_syn are the library-wide
 * driver state, defined elsewhere; both are non-null only between init and
 * close.
 */
#include "n_synthInternals.h"

/*
 * Bring up the naudio library: adopt g as the globals block and build the
 * synthesis driver from config c. The nested guards make initialization a
 * singleton -- a second call while already brought up is a no-op.
 */
void n_alInit(N_ALGlobals* g, ALSynConfig* c) {
  if (!n_alGlobals) {
    n_alGlobals = g;
    if (!n_syn) {
      n_syn = &n_alGlobals->drvr;
      n_alSynNew(c);
    }
  }
}

/*
 * Tear the driver down and clear the global slots, leaving the library ready
 * for a future n_alInit.
 */
void n_alClose(void) {
  if (n_alGlobals) {
    n_alSynDelete();
    n_alGlobals = 0;
    n_syn = 0;
  }
}
