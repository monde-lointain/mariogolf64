/*
 * GB Pak power control.
 *
 * Switches the Game Boy Game Pak cartridge inside the GB Pak on or off. The GB
 * Pak Manager performs a stabilization wait when ramping power up, so a freshly
 * powered cartridge is ready by the time the call returns.
 */
#include <nusys.h>

/*
 * Set cartridge power for handle's GB Pak.
 *
 * flag is OS_GBPAK_POWER_ON or OS_GBPAK_POWER_OFF. The request goes to the GB
 * Pak Manager over SI; the osGbpakPower result is stashed in handle->error and
 * returned.
 */
s32 nuContGBPakPower(NUContPakFile* handle, s32 flag) {
  NUContGBPakMesg gbpakMesg;

  gbpakMesg.handle = handle;
  gbpakMesg.data[0] = flag;
  handle->error = nuSiSendMesg(NU_CONT_GBPAK_POWER_MSG, (void*)&gbpakMesg);
  return handle->error;
}
