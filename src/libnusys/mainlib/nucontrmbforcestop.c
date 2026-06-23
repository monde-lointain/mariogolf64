/*
 * Rumble Pak forced stop.
 *
 * Tells the Rumble Pak Manager to halt every controller's motor and latch them
 * off: once forced to stop, nuContRmbStart has no effect while the PAUSE latch
 * is set. Used to guarantee motors are quiet across events like a pause or a
 * save-to-pak operation. (The stock release counterpart, nuContRmbForceStopEnd,
 * is an empty stub in this build.)
 */
#include <nusys.h>

/* Post the force-stop request; it carries no payload, so the data pointer is
 * NULL. */
void nuContRmbForceStop(void) {
  nuSiSendMesg(NU_CONT_RMB_FORCESTOP_MSG, (void*)NULL);
}
