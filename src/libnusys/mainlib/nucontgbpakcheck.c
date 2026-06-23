/*
 * GB Pak connector check.
 *
 * Detects a poorly seated Game Boy Game Pak by exercising special ROM and RAM
 * addresses and watching every address line. Use it to distinguish a genuine
 * read/write failure from a loose cartridge before trusting transferred data.
 */
#include <nusys.h>

/*
 * Run the connector check on handle, returning the GB Pak status in status.
 *
 * The out-pointer is forwarded to the GB Pak Manager in the message; the
 * osGbpakCheckConnector result is stored in handle->error and returned.
 */
s32 nuContGBPakCheckConnector(NUContPakFile* handle, u8* status) {
  NUContGBPakMesg gbpakMesg;

  gbpakMesg.handle = handle;
  gbpakMesg.data[0] = (u32)status;
  handle->error =
      nuSiSendMesg(NU_CONT_GBPAK_CHECKCONNECTOR_MSG, (void*)&gbpakMesg);
  return handle->error;
}
