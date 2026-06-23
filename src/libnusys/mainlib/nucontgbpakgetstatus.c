/*
 * GB Pak status query.
 *
 * Reports the current GB Pak status flags: whether a Game Boy cartridge is
 * present (OS_GBPAK_GBCART_ON), whether one was removed since the last check
 * (OS_GBPAK_GBCART_PULL), and whether cartridge power is on (OS_GBPAK_POWER).
 */
#include <nusys.h>

/*
 * Fetch handle's GB Pak status into the status byte.
 *
 * The out-pointer is forwarded to the GB Pak Manager in the message; the
 * osGbpakGetStatus result is stored in handle->error and returned.
 */
s32 nuContGBPakGetStatus(NUContPakFile* handle, u8* status) {
  NUContGBPakMesg gbpakMesg;

  gbpakMesg.handle = handle;
  gbpakMesg.data[0] = (u32)status;
  handle->error = nuSiSendMesg(NU_CONT_GBPAK_STATUS_MSG, (void*)&gbpakMesg);
  return handle->error;
}
