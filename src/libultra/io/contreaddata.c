/*
 * contreaddata.c -- controller input read: poll every controller for its
 * current button bitmap and analog stick position. osContStartReadData fires
 * the PIF read-button command, then osContGetReadData unpacks the reply into
 * per-port OSContPad records once the SI message lands.
 */
#include "PRinternal/macros.h"
#include "PR/os_internal.h"
#include "controller.h"
#include "siint.h"

static void __osPackReadData(void);

/* Issue the read-button command to all ports and start its SI DMA. */
s32 osContStartReadData(OSMesgQueue* mq) {
  s32 ret = 0;
  __osSiGetAccess();

  // Only rebuild and re-send the command block when the previous command
  // differed; repeated reads reuse the block already sitting in PIF RAM.
  if (__osContLastCmd != CONT_CMD_READ_BUTTON) {
    __osPackReadData();
    ret = __osSiRawStartDma(OS_WRITE, __osContPifRam.ramarray);
    osRecvMesg(mq, NULL, OS_MESG_BLOCK);
  }

  ret = __osSiRawStartDma(OS_READ, __osContPifRam.ramarray);
  __osContLastCmd = CONT_CMD_READ_BUTTON;
  __osSiRelAccess();
  return ret;
}

/* Walk the PIF reply, one fixed-size record per port, and copy each
 * controller's inputs into the caller's array. */
void osContGetReadData(OSContPad* data) {
  u8* ptr = (u8*)__osContPifRam.ramarray;
  __OSContReadFormat readformat;
  int i;
  for (i = 0; i < __osMaxControllers;
       i++, ptr += sizeof(__OSContReadFormat), data++) {
    readformat = *(__OSContReadFormat*)ptr;

    // A channel error means nothing is plugged in (or it did not answer); leave
    // the pad untouched so stale data is the caller's to interpret.
    data->errno = CHNL_ERR(readformat);
    if (data->errno != 0) {
      continue;
    }
    data->button = readformat.button;
    data->stick_x = readformat.stick_x;
    data->stick_y = readformat.stick_y;
  }
}

/* Lay out the read-button command block in PIF RAM: one request per active port
 * followed by the end marker. Reply fields are pre-seeded with sentinel values
 * the controller overwrites with real data. */
static void __osPackReadData(void) {
  u8* ptr = (u8*)__osContPifRam.ramarray;
  __OSContReadFormat readformat;
  int i;
  for (i = 0; i < ARRLEN(__osContPifRam.ramarray); i++) {
    __osContPifRam.ramarray[i] = 0;
  }
  __osContPifRam.pifstatus = CONT_CMD_EXE;

  // Command template: NOP dummy, the TX/RX sizes the PIF expects for a button
  // read, and sentinels (all-ones) in the reply slots.
  readformat.dummy = CONT_CMD_NOP;
  readformat.txsize = CONT_CMD_READ_BUTTON_TX;
  readformat.rxsize = CONT_CMD_READ_BUTTON_RX;
  readformat.cmd = CONT_CMD_READ_BUTTON;
  readformat.button = 0xFFFF;
  readformat.stick_x = -1;
  readformat.stick_y = -1;

  // Replicate the template for every active port, then cap with the end byte.
  for (i = 0; i < __osMaxControllers; i++) {
    *(__OSContReadFormat*)ptr = readformat;
    ptr += sizeof(__OSContReadFormat);
  }
  *ptr = CONT_CMD_END;
}
