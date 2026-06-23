/*
 * Rumble Pak mode selection.
 *
 * Chooses how one controller's Rumble Pak is driven: DISABLE (motor never
 * runs), ENABLE (usable once nuContRmbCheck has confirmed a pak), or AUTORUN
 * (the manager keeps polling for a pak and enables it automatically). Every pak
 * starts DISABLED.
 */
#include <nusys.h>

/*
 * Set port contNo's Rumble Pak mode to mode.
 *
 * The PAUSE bit (set by a force-stop) is independent of the mode, so it is
 * preserved across the change instead of being clobbered. AUTORUN additionally
 * primes the auto-poll state: FIND if a pak is already known, otherwise SEARCH
 * so the retrace handler begins looking for one. (Note: an unrecognized mode
 * falls through and changes nothing.)
 */
void nuContRmbModeSet(u32 contNo, u8 mode) {
  u32 forceStop;

  forceStop = nuContRmbCtl[contNo].mode & NU_CONT_RMB_MODE_PAUSE;
  switch (mode) {
    case NU_CONT_RMB_MODE_DISABLE:
      nuContRmbCtl[contNo].mode = mode | forceStop;
      break;
    case NU_CONT_RMB_MODE_ENABLE:
      // Only honor ENABLE once a real Rumble Pak has been identified.
      if (nuContRmbCtl[contNo].type == NU_CONT_PAK_TYPE_RUMBLE) {
        nuContRmbCtl[contNo].mode = mode | forceStop;
      }
      break;
    case NU_CONT_RMB_MODE_AUTORUN:
      nuContRmbCtl[contNo].mode = mode | forceStop;
      if (nuContRmbCtl[contNo].type == NU_CONT_PAK_TYPE_RUMBLE) {
        nuContRmbCtl[contNo].autorun = NU_CONT_RMB_AUTO_FIND;
      } else {
        nuContRmbCtl[contNo].autorun = NU_CONT_RMB_AUTO_SEARCH;
      }
  }
}
