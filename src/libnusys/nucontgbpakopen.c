/*
 * GB Pak open.
 *
 * Binds a GB Pak handle to a controller port and asks the GB Pak Manager to
 * initialize the hardware. The handle reuses the Controller Pak file structure;
 * this routine wires its pfs sub-handle to the chosen port before dispatch.
 */
#include <nusys.h>

/*
 * Open the GB Pak in port contNo through handle.
 *
 * The request is forwarded to the GB Pak Manager over the SI message path; the
 * underlying osGbpakInit result is stored back in handle->error and returned
 * (PFS_ERR_NOPACK / PFS_ERR_DEVICE / PFS_ERR_CONTRFAIL on trouble).
 */
s32 nuContGBPakOpen(NUContPakFile* handle, s32 contNo) {
  NUContGBPakMesg gbpakMesg;

  handle->pfs = &nuContPfs[contNo];
  handle->pfs->channel = contNo;
  gbpakMesg.handle = handle;
  handle->error = nuSiSendMesg(NU_CONT_GBPAK_OPEN_MSG, (void*)&gbpakMesg);
  return handle->error;
}
