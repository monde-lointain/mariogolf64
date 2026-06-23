/*
 * controller.c -- controller subsystem initialization and the shared
 * status-request packer.
 *
 * osContInit is the one-time entry point every other SI device routine depends
 * on: it waits for the PIF to finish its boot-time handshake, runs one
 * status-request exchange to discover what is plugged into each port, and sets
 * up the SI access queue and the EEPROM timer queue. The two helpers below
 * build and decode the status-request PIF command block shared with the query
 * path.
 */
#include "PRinternal/macros.h"
#include "PR/os_internal.h"
#include "controller.h"
#include "siint.h"

// Latched after the first successful init so repeat calls are no-ops.
s32 __osContinitialized = FALSE;

s32 osContInit(OSMesgQueue* mq, u8* bitpattern, OSContStatus* data) {
  OSMesg dummy;
  s32 ret = 0;
  OSTime t;
  OSTimer mytimer;
  OSMesgQueue timerMesgQueue;

  // Initialization is idempotent: only the first call does real work.
  if (__osContinitialized) {
    return 0;
  }
  __osContinitialized = TRUE;

  // The PIF spends roughly the first half-second after power-on talking to the
  // game pak and cannot service SI requests then. If we are still inside that
  // window, sleep on a one-shot timer until it closes.
  t = osGetTime();
  if (t < OS_USEC_TO_CYCLES(500000)) {
    osCreateMesgQueue(&timerMesgQueue, &dummy, 1);
    osSetTimer(&mytimer, OS_USEC_TO_CYCLES(500000) - t, 0, &timerMesgQueue,
               &dummy);
    osRecvMesg(&timerMesgQueue, &dummy, OS_MESG_BLOCK);
  }

  // Probe all four ports once: write the request, wait, read the reply, wait,
  // then decode the connection bitmap and per-port status for the caller.
  __osMaxControllers = 4;
  __osPackRequestData(CONT_CMD_REQUEST_STATUS);
  ret = __osSiRawStartDma(OS_WRITE, __osContPifRam.ramarray);
  osRecvMesg(mq, &dummy, OS_MESG_BLOCK);
  ret = __osSiRawStartDma(OS_READ, __osContPifRam.ramarray);
  osRecvMesg(mq, &dummy, OS_MESG_BLOCK);
  __osContGetInitData(bitpattern, data);
  __osContLastCmd = CONT_CMD_REQUEST_STATUS;

  // Stand up the access queue that serializes later SI users, plus the timer
  // queue the EEPROM driver waits on.
  __osSiCreateAccessQueue();
  osCreateMesgQueue(&__osEepromTimerQ, &__osEepromTimerMsg, 1);
  return ret;
}

/* Decode a status-request reply: fill in each port's type/status and build a
 * bitmap (bit i set => port i answered) in *pattern. */
void __osContGetInitData(u8* pattern, OSContStatus* data) {
  u8* ptr;
  __OSContRequesFormat requestHeader;
  int i;
  u8 bits = 0;
  ptr = (u8*)__osContPifRam.ramarray;
  for (i = 0; i < __osMaxControllers;
       i++, ptr += sizeof(requestHeader), data++) {
    requestHeader = *(__OSContRequesFormat*)ptr;

    // Skip ports that reported a channel error; they have no device.
    data->errno = CHNL_ERR(requestHeader);
    if (data->errno != 0) {
      continue;
    }

    // Device type arrives split across two bytes (high then low); reassemble
    // it.
    data->type = requestHeader.typel << 8 | requestHeader.typeh;
    data->status = requestHeader.status;
    bits |= 1 << i;
  }
  *pattern = bits;
}

/* Build a status-request command block in PIF RAM: clear the buffer, mark it
 * executable, then stamp one request template per port and close with the end
 * marker. The reply fields are seeded with NOPs the PIF overwrites. */
void __osPackRequestData(u8 cmd) {
  u8* ptr;
  __OSContRequesFormat requestHeader;
  s32 i;
  for (i = 0; i < ARRLEN(__osContPifRam.ramarray); i++) {
    __osContPifRam.ramarray[i] = 0;
  }
  __osContPifRam.pifstatus = CONT_CMD_EXE;
  ptr = (u8*)__osContPifRam.ramarray;
  requestHeader.dummy = CONT_CMD_NOP;
  requestHeader.txsize = CONT_CMD_RESET_TX;
  requestHeader.rxsize = CONT_CMD_RESET_RX;
  requestHeader.cmd = cmd;
  requestHeader.typeh = CONT_CMD_NOP;
  requestHeader.typel = CONT_CMD_NOP;
  requestHeader.status = CONT_CMD_NOP;
  requestHeader.dummy1 = CONT_CMD_NOP;
  for (i = 0; i < __osMaxControllers; i++) {
    *(__OSContRequesFormat*)ptr = requestHeader;
    ptr += sizeof(requestHeader);
  }
  *ptr = CONT_CMD_END;
}
