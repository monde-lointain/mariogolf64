/*
 * GB Pak cartridge ID read.
 *
 * Reads the registration (header) area of the Game Boy Game Pak ROM and reports
 * the GB Pak status. The manager also powers the cartridge on as part of this
 * request, so it is the usual first step after opening a GB Pak.
 */
#include <nusys.h>

/*
 * Read handle's cartridge ID into id and its status into status.
 *
 * Both out-pointers are passed to the GB Pak Manager packed in the message data
 * words. The osGbpakReadId result is stored in handle->error and returned.
 */
s32 nuContGBPakReadID(NUContPakFile* handle, OSGbpakId* id, u8* status) {
  NUContGBPakMesg gbpakMesg;

  gbpakMesg.handle = handle;
  gbpakMesg.data[0] = (u32)id;
  gbpakMesg.data[1] = (u32)status;
  handle->error = nuSiSendMesg(NU_CONT_GBPAK_READID_MSG, (void*)&gbpakMesg);
  return handle->error;
}
