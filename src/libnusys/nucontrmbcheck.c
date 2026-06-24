/*
 * Rumble Pak presence check.
 *
 * Probes one controller port for a Rumble Pak and records the outcome in the
 * Rumble Pak control table so later start/stop calls know whether a motor is
 * actually there. Unlike nuContPakOpen, it does not look for a Controller Pak.
 */
#include <nusys.h>

/*
 * Check port contNo for a Rumble Pak and cache the result.
 *
 * The probe runs through the Rumble Pak Manager (ultimately osMotorInit); a
 * zero return means a Rumble Pak responded, so the control entry's type is
 * marked RUMBLE, otherwise NONE. The osMotorInit result is returned unchanged.
 */
s32 nuContRmbCheck(u32 contNo) {
  NUContRmbMesg checkMesg;
  s32 rtn;

  checkMesg.contNo = contNo;
  rtn = nuSiSendMesg(NU_CONT_RMB_CHECK_MSG, (void*)&checkMesg);
  if (!rtn) {
    nuContRmbCtl[contNo].type = NU_CONT_PAK_TYPE_RUMBLE;
  } else {
    nuContRmbCtl[contNo].type = NU_CONT_PAK_TYPE_NONE;
  }
  return rtn;
}
