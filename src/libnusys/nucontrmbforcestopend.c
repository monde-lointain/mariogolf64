/*
 * Rumble Pak forced-stop release (empty stub).
 *
 * The public counterpart to nuContRmbForceStop; stock nusys would post a
 * force-stop-end message that clears the PAUSE latch on every pak. In this
 * build the body is empty and does nothing. The manager still carries the
 * matching handler (contRmbForceStopEndMesg in nucontrmbmgr.c), but no caller
 * sends that message, so the latch is never lifted through here.
 */
#include "common.h"

void nuContRmbForceStopEnd(void) {}
