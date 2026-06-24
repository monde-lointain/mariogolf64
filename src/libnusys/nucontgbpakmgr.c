/*
 * GB Pak Manager.
 *
 * Registers a handler with the SI Manager that services GB Pak requests, and
 * holds the worker callbacks behind it. Each public nuContGBPak* wrapper posts
 * an SI message tagged with a minor command number; the SI Manager dispatches
 * through funcList by that number, and the matching callback below unpacks the
 * message and invokes the corresponding os-level GB Pak primitive.
 */
#include <nusys.h>

static s32 contGBPakOpen(NUSiCommonMesg* mesg);
static s32 contGBPakGetState(NUSiCommonMesg* mesg);
static s32 contGBPakPower(NUSiCommonMesg* mesg);
static s32 contGBPakReadID(NUSiCommonMesg* mesg);
static s32 contGBPakReadWrite(NUSiCommonMesg* mesg);
static s32 contGBPakCheckConnector(NUSiCommonMesg* mesg);

/*
 * Dispatch table indexed by the message's minor command number. Slot 0 is
 * unused (no command zero) and the trailing NULL terminates the list for the SI
 * Manager.
 */
static s32 (*funcList[])(NUSiCommonMesg*) = {NULL,
                                             contGBPakOpen,
                                             contGBPakGetState,
                                             contGBPakPower,
                                             contGBPakReadID,
                                             contGBPakReadWrite,
                                             contGBPakCheckConnector,
                                             NULL};

NUCallBackList nuContGBPakCallBack = {NULL, funcList, NU_SI_MAJOR_NO_GBPAK};

/* Register the GB Pak handler with the SI Manager; not done by nuContInit. */
void nuContGBPakMgrInit(void) { nuSiCallBackAdd(&nuContGBPakCallBack); }

/* Unregister the GB Pak handler from the SI Manager. */
void nuContGBPakMgrRemove(void) { nuSiCallBackRemove(&nuContGBPakCallBack); }

/* Initialize GB Pak hardware on the handle's port (NU_CONT_GBPAK_OPEN_MSG). */
static s32 contGBPakOpen(NUSiCommonMesg* mesg) {
  NUContPakFile* handle;
  NUContGBPakMesg* gbpakMesg;

  gbpakMesg = (NUContGBPakMesg*)mesg->dataPtr;
  handle = gbpakMesg->handle;
  return osGbpakInit(&nuSiMesgQ, handle->pfs, handle->pfs->channel);
}

/* Read GB Pak status flags into data[0] (NU_CONT_GBPAK_STATUS_MSG). */
static s32 contGBPakGetState(NUSiCommonMesg* mesg) {
  NUContPakFile* handle;
  NUContGBPakMesg* gbpakMesg;

  gbpakMesg = (NUContGBPakMesg*)mesg->dataPtr;
  handle = gbpakMesg->handle;
  return osGbpakGetStatus(handle->pfs, (u8*)gbpakMesg->data[0]);
}

/* Set cartridge power from data[0] (NU_CONT_GBPAK_POWER_MSG). */
static s32 contGBPakPower(NUSiCommonMesg* mesg) {
  NUContPakFile* handle;
  NUContGBPakMesg* gbpakMesg;

  gbpakMesg = (NUContGBPakMesg*)mesg->dataPtr;
  handle = gbpakMesg->handle;
  return osGbpakPower(handle->pfs, gbpakMesg->data[0]);
}

/* Read cartridge ID into data[0] and status into data[1]
 * (NU_CONT_GBPAK_READID_MSG). */
static s32 contGBPakReadID(NUSiCommonMesg* mesg) {
  NUContPakFile* handle;
  NUContGBPakMesg* gbpakMesg;

  gbpakMesg = (NUContGBPakMesg*)mesg->dataPtr;
  handle = gbpakMesg->handle;
  return osGbpakReadId(handle->pfs, (OSGbpakId*)gbpakMesg->data[0],
                       (u8*)gbpakMesg->data[1]);
}

/*
 * Aligned block transfer (NU_CONT_GBPAK_READWRITE_MSG): data[0]=flag,
 * data[1]=address, data[2]=buffer, data[3]=size.
 */
static s32 contGBPakReadWrite(NUSiCommonMesg* mesg) {
  NUContPakFile* handle;
  NUContGBPakMesg* gbpakMesg;

  gbpakMesg = (NUContGBPakMesg*)mesg->dataPtr;
  handle = gbpakMesg->handle;
  return osGbpakReadWrite(handle->pfs, (u16)gbpakMesg->data[0],
                          (u16)gbpakMesg->data[1], (u8*)gbpakMesg->data[2],
                          (u16)gbpakMesg->data[3]);
}

/* Probe the connector, status into data[0] (NU_CONT_GBPAK_CHECKCONNECTOR_MSG).
 */
static s32 contGBPakCheckConnector(NUSiCommonMesg* mesg) {
  NUContPakFile* handle;
  NUContGBPakMesg* gbpakMesg;

  gbpakMesg = (NUContGBPakMesg*)mesg->dataPtr;
  handle = gbpakMesg->handle;
  return osGbpakCheckConnector(handle->pfs, (u8*)gbpakMesg->data[0]);
}
