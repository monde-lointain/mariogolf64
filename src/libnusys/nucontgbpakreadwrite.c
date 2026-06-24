/*
 * GB Pak block read/write.
 *
 * The low-level GB Pak transfer primitive: both address and size must be
 * multiples of 32 bytes. The unaligned-friendly nuContGBPakFread/Fwrite helpers
 * are built on top of this. Direction is selected by flag (OS_READ/OS_WRITE).
 */
#include <nusys.h>

/*
 * Transfer size bytes between buffer and the cartridge at address.
 *
 * address and size must be 32-byte aligned; a debug build prints a diagnostic
 * when they are not, but the request is forwarded either way. The
 * osGbpakReadWrite result is stored in handle->error and returned.
 */
s32 nuContGBPakReadWrite(NUContPakFile* handle, u16 flag, u16 address,
                         u8* buffer, u16 size) {
  NUContGBPakMesg gbpakMesg;

  // Both fields must be multiples of 32; flag the violation only in debug.
#ifdef NU_DEBUG
  if (address & 0x001f) {
    osSyncPrintf("nuContGBPakReadWrite: address(0x%04X) error!!\n", address);
  }
  if (size & 0x001f) {
    osSyncPrintf("nuContGBPakReadWrite: size(%d) error!!\n", size);
  }
#endif

  gbpakMesg.handle = handle;
  gbpakMesg.data[0] = flag;
  gbpakMesg.data[1] = address;
  gbpakMesg.data[2] = (u32)buffer;
  gbpakMesg.data[3] = size;
  handle->error = nuSiSendMesg(NU_CONT_GBPAK_READWRITE_MSG, (void*)&gbpakMesg);
  return handle->error;
}
