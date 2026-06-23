/*
 * One-call controller subsystem bring-up.
 *
 * nuContInit is the convenience entry point most titles use: it starts the SI
 * Manager and the Controller and Rumble Pak managers in the right order so the
 * controllers are immediately usable. (The GB Pak manager is deliberately left
 * out and must be registered separately when needed.)
 */
#include <nusys.h>

/*
 * Initialize and start the controller input subsystem.
 *
 * Starting the SI Manager probes the ports, so its return value is the bit
 * pattern of connected controllers (bit 0 = port 1, ...); that pattern is
 * captured before the dependent managers run and handed back to the caller.
 */
u8 nuContInit(void) {
  u8 pattern;

  pattern = nuSiMgrInit();
  nuContMgrInit();
  nuContRmbMgrInit();
  return pattern;
}
