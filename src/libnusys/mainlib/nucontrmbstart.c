/*
 * Rumble Pak vibration start.
 *
 * Kicks off a timed vibration on one controller's Rumble Pak. The motor is
 * pulse-width modulated: freq controls duty (1 = on once per 256 frames, 256 =
 * always on, so larger means stronger), frame is how many frames the effect
 * lasts. nuContRmbCheck must have confirmed a pak first.
 */
#include <nusys.h>

/*
 * Start vibration on port contNo at strength freq for frame frames.
 *
 * Honors a force-stop: if the pak is paused the request is dropped silently.
 * Otherwise the profile is bundled into a RUN-state control record and handed
 * to the Rumble Pak Manager, which drives the motor from the retrace handler.
 */
void nuContRmbStart(u32 contNo, u16 freq, u16 frame) {
  NUContRmbCtl rmbCtl;
  NUContRmbMesg startMesg;

  if (nuContRmbCtl[contNo].mode & NU_CONT_RMB_MODE_PAUSE) {
    return;
  }
  rmbCtl.freq = freq;
  rmbCtl.frame = frame;
  rmbCtl.state = NU_CONT_RMB_STATE_RUN;
  startMesg.contNo = contNo;
  startMesg.data = &rmbCtl;
  nuSiSendMesg(NU_CONT_RMB_START_MSG, (void*)&startMesg);
}
