/*
 * Rumble Pak Manager.
 *
 * Owns the per-controller Rumble Pak state and drives the motors. The public
 * nuContRmb* calls post messages that set up or tear down a control entry; the
 * real work happens each retrace, where contRmbRetrace walks every controller
 * and steps its small state machine (RUN -> STOPPING -> STOPPED, plus FORCESTOP
 * and an AUTORUN auto-detect path). A failed motor I/O disables that pak.
 */
#include <nusys.h>

extern NUContRmbCtl nuContRmbCtl[NU_CONT_MAXCONTROLLERS];

/* Retrace interval, in frames, between AUTORUN pak-presence probes. */
u32 nuContRmbSearchTime = NU_CONT_RMB_AUTO_SEARCHTIME;

static s32 contRmbRetrace(NUSiCommonMesg* mesg);
static s32 contRmbCheckMesg(NUSiCommonMesg* mesg);
static s32 contRmbStartMesg(NUSiCommonMesg* mesg);
static s32 contRmbStopMesg(NUSiCommonMesg* mesg);
static s32 contRmbForceStopMesg(NUSiCommonMesg* mesg);
static s32 contRmbForceStopEndMesg(NUSiCommonMesg* mesg);

/* Dispatch table indexed by minor command number; trailing NULL terminates it.
 */
static s32 (*funcList[])(NUSiCommonMesg*) = {contRmbRetrace,
                                             contRmbCheckMesg,
                                             contRmbStartMesg,
                                             contRmbStopMesg,
                                             contRmbForceStopMesg,
                                             contRmbForceStopEndMesg,
                                             NULL};

NUCallBackList nuContRmbCallBack = {NULL, funcList, NU_SI_MAJOR_NO_RMB};

/*
 * Initialize the control table and register the manager with the SI Manager.
 *
 * Each entry starts stopped and disabled; counter is seeded with the port index
 * so the AUTORUN probes for different ports are staggered rather than all
 * firing on the same retrace.
 */
void nuContRmbMgrInit(void) {
  u32 cnt;

  for (cnt = 0; cnt < NU_CONT_MAXCONTROLLERS; cnt++) {
    nuContRmbCtl[cnt].state = NU_CONT_RMB_STATE_STOPPED;
    nuContRmbCtl[cnt].mode = NU_CONT_RMB_MODE_DISABLE;
    nuContRmbCtl[cnt].counter = cnt;
  }
  nuSiCallBackAdd(&nuContRmbCallBack);
}

/* Unregister the Rumble Pak Manager from the SI Manager. */
void nuContRmbMgrRemove(void) { nuSiCallBackRemove(&nuContRmbCallBack); }

/*
 * Advance one controller's motor state machine by a single retrace step.
 *
 * Returns the motor I/O result; a non-zero value signals the caller to give up
 * on this pak. The states are:
 *
 * STOPPED:   idle, nothing to do.
 *
 * STOPPING:  issue a few trailing stops (counter counts down from 2) to be sure
 * the motor is off, then settle to STOPPED.
 *
 * RUN:       while frames remain, pulse-width-modulate the motor. freq is added
 * into counter each frame; the carry out of the low 8 bits is a motor-on pulse
 * and the low byte keeps the fractional remainder. When frames run out, stop
 * and hand off to STOPPING.
 *
 * FORCESTOP: re-init the motor; on success stop it and enter STOPPING, on
 * failure mark it STOPPED (the pak is gone or unresponsive).
 */
static s32 contRmbControl(NUContRmbCtl* rmbCtl, u32 contNo) {
  s32 rtn = 0;
  u32 integer;

  switch (rmbCtl->state) {
    case NU_CONT_RMB_STATE_STOPPED:
      break;

    case NU_CONT_RMB_STATE_STOPPING:
      if (rmbCtl->counter > 0) {
        rtn = osMotorStop(&nuContPfs[contNo]);
      } else {
        rmbCtl->state = NU_CONT_RMB_STATE_STOPPED;
      }
      rmbCtl->counter--;
      break;

    case NU_CONT_RMB_STATE_RUN:
      if (rmbCtl->frame > 0) {
        // PWM step: carry out of the low byte = a motor-on pulse this frame.
        rmbCtl->counter += rmbCtl->freq;
        integer = rmbCtl->counter >> 8;
        rmbCtl->counter &= 0x00ff;
        if (integer > 0) {
          rtn = osMotorStart(&nuContPfs[contNo]);
        } else {
          rtn = osMotorStop(&nuContPfs[contNo]);
        }
      } else {
        rtn = osMotorStop(&nuContPfs[contNo]);
        rmbCtl->state = NU_CONT_RMB_STATE_STOPPING;
        rmbCtl->counter = 2;
      }
      rmbCtl->frame--;
      break;

    case NU_CONT_RMB_STATE_FORCESTOP:
      rtn = osMotorInit(&nuSiMesgQ, &nuContPfs[contNo], contNo);
      if (!rtn) {
        osMotorStop(&nuContPfs[contNo]);
        rmbCtl->state = NU_CONT_RMB_STATE_STOPPING;
        rmbCtl->counter = 2;
      } else {
        rmbCtl->state = NU_CONT_RMB_STATE_STOPPED;
      }
      break;

    default:
      break;
  }
  return rtn;
}

/*
 * Per-retrace handler: service every controller according to its mode.
 *
 * DISABLE does nothing. ENABLE just steps the state machine, disabling the pak
 * if motor I/O fails. AUTORUN either periodically probes for a pak (SEARCH) or,
 * once found (FIND), runs the state machine and reverts to SEARCH on failure.
 * The two PAUSE variants (force-stopped) still step the machine so an in-flight
 * stop completes, but only if a real pak is present.
 */
static s32 contRmbRetrace(NUSiCommonMesg* mesg) {
  u32 cnt;
  s32 rtn;
  u32 counter;
  NUContRmbCtl* rmbCtl;

  counter = nuScRetraceCounter % nuContRmbSearchTime;
  for (cnt = 0; cnt < NU_CONT_MAXCONTROLLERS; cnt++) {
    rmbCtl = &nuContRmbCtl[cnt];
    switch (rmbCtl->mode) {
      case NU_CONT_RMB_MODE_DISABLE:
        break;

      case NU_CONT_RMB_MODE_ENABLE:
        rtn = contRmbControl(rmbCtl, cnt);
        if (rtn) {
          rmbCtl->mode = NU_CONT_RMB_MODE_DISABLE;
        }
        break;

      case NU_CONT_RMB_MODE_AUTORUN:
        if (rmbCtl->autorun == NU_CONT_RMB_AUTO_SEARCH) {
          // Probe for a pak only on the controller's scheduled search frame.
          counter = rmbCtl->counter % nuContRmbSearchTime;
          if (!counter) {
            rtn = osMotorInit(&nuSiMesgQ, &nuContPfs[cnt], cnt);
            if (!rtn) {
              rmbCtl->autorun = NU_CONT_RMB_AUTO_FIND;
              rmbCtl->type = NU_CONT_PAK_TYPE_RUMBLE;
            }
          }
          rmbCtl->counter++;
        } else {
          // Pak found: drive it; on I/O failure fall back to searching again.
          rtn = contRmbControl(rmbCtl, cnt);
          if (rtn) {
            rmbCtl->counter = cnt;
            rmbCtl->autorun = NU_CONT_RMB_AUTO_SEARCH;
            rmbCtl->type = NU_CONT_PAK_TYPE_NONE;
          }
        }
        break;

      case (NU_CONT_RMB_MODE_ENABLE | NU_CONT_RMB_MODE_PAUSE):
      case (NU_CONT_RMB_MODE_AUTORUN | NU_CONT_RMB_MODE_PAUSE):
        if (rmbCtl->type == NU_CONT_PAK_TYPE_RUMBLE) {
          rtn = contRmbControl(rmbCtl, cnt);
        }
        break;

      default:
        break;
    }
  }
  return NU_SI_CALLBACK_CONTINUE;
}

/* Check handler: probe for a pak on the requested port (NU_CONT_RMB_CHECK_MSG).
 */
static s32 contRmbCheckMesg(NUSiCommonMesg* mesg) {
  NUContRmbMesg* rmbMesg;

  rmbMesg = (NUContRmbMesg*)mesg->dataPtr;
  return osMotorInit(&nuSiMesgQ, &nuContPfs[rmbMesg->contNo], rmbMesg->contNo);
}

/*
 * Force-stop handler: mark every pak FORCESTOP and set the PAUSE bit so the
 * retrace handler stops the motors and nuContRmbStart stays inert until
 * release.
 */
static s32 contRmbForceStopMesg(NUSiCommonMesg* mesg) {
  u32 cnt;

  for (cnt = 0; cnt < NU_CONT_MAXCONTROLLERS; cnt++) {
    nuContRmbCtl[cnt].state = NU_CONT_RMB_STATE_FORCESTOP;
    nuContRmbCtl[cnt].mode |= NU_CONT_RMB_MODE_PAUSE;
  }
  return 0;
}

/* Force-stop-end handler: clear the PAUSE bit on every pak to allow motion
 * again. */
static s32 contRmbForceStopEndMesg(NUSiCommonMesg* mesg) {
  u32 cnt;

  for (cnt = 0; cnt < NU_CONT_MAXCONTROLLERS; cnt++) {
    nuContRmbCtl[cnt].mode &= 0x7f;
  }
  return 0;
}

/*
 * Start handler: load a controller's RUN profile (state/frame/freq) from the
 * message and reset its PWM accumulator (NU_CONT_RMB_START_MSG).
 */
static s32 contRmbStartMesg(NUSiCommonMesg* mesg) {
  NUContRmbMesg* rmbMesg;
  NUContRmbCtl* rmbCtlData;

  rmbMesg = (NUContRmbMesg*)mesg->dataPtr;
  rmbCtlData = (NUContRmbCtl*)rmbMesg->data;
  nuContRmbCtl[rmbMesg->contNo].state = rmbCtlData->state;
  nuContRmbCtl[rmbMesg->contNo].frame = rmbCtlData->frame;
  nuContRmbCtl[rmbMesg->contNo].freq = rmbCtlData->freq;
  nuContRmbCtl[rmbMesg->contNo].counter = 0;
  return 0;
}

/* Stop handler: zero the remaining frames so the state machine winds the pak
 * down. */
static s32 contRmbStopMesg(NUSiCommonMesg* mesg) {
  NUContRmbMesg* rmbMesg;

  rmbMesg = (NUContRmbMesg*)mesg->dataPtr;
  nuContRmbCtl[rmbMesg->contNo].frame = 0;
  return 0;
}
