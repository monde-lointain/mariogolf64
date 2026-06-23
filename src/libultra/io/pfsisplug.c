/*
 * pfsisplug.c -- detect which controller ports currently have a Controller Pak
 * plugged in.
 *
 * Broadcasts a status request to every controller, retries a few times to ride
 * out transient CRC errors, and returns a bitmask of the ports that report a
 * present, error-free pak. The request build and reply decode are factored into
 * helpers that walk PIF RAM channel by channel.
 */
#include "PRinternal/macros.h"
#include "PR/os_internal.h"
#include "controller.h"
#include "siint.h"

/* Scan all controllers and set *pattern to a bitmask (bit N = channel N) of the
 * ports holding a usable Controller Pak. Returns the final DMA status. */
s32 osPfsIsPlug(OSMesgQueue* mq, u8* pattern) {
  s32 ret = 0;
  OSMesg msg;
  u8 bitpattern;
  OSContStatus contData[MAXCONTROLLERS];
  s32 channel;
  u8 bits = 0;
  s32 crcErrorCount = 3;

  __osSiGetAccess();

  /* Poll all channels; if any channel comes back clean, accept this pass.
   * Allow up to three passes for CRC errors to clear before giving up. */
  do {
    __osPfsRequestData(CONT_CMD_REQUEST_STATUS);
    ret = __osSiRawStartDma(OS_WRITE, &__osPfsPifRam);
    osRecvMesg(mq, &msg, OS_MESG_BLOCK);
    ret = __osSiRawStartDma(OS_READ, &__osPfsPifRam);
    osRecvMesg(mq, &msg, OS_MESG_BLOCK);
    __osPfsGetInitData(&bitpattern, &contData[0]);
    for (channel = 0; channel < __osMaxControllers; channel++) {
      if ((contData[channel].status & CONT_ADDR_CRC_ER) == 0) {
        crcErrorCount--;
        break;
      }
    }
    if (channel == __osMaxControllers) {
      crcErrorCount = 0;
    }
  } while (crcErrorCount > 0);

  /* A port counts as plugged when it had no channel error and reports a card.
   */
  for (channel = 0; channel < __osMaxControllers; channel++) {
    if ((contData[channel].errno == 0) &&
        ((contData[channel].status & CONT_CARD_ON) != 0)) {
      bits |= (1 << channel);
    }
  }

  __osSiRelAccess();
  *pattern = bits;
  return ret;
}

/* Fill PIF RAM with one status request per controller, back to back, ending
 * with the stop marker, so a single transaction polls every channel. */
void __osPfsRequestData(u8 cmd) {
  u8* ptr = (u8*)&__osPfsPifRam;
  __OSContRequesFormat requestformat;
  int i;

  __osContLastCmd = cmd;
  __osPfsPifRam.pifstatus = CONT_CMD_EXE;
  requestformat.dummy = CONT_CMD_NOP;
  requestformat.txsize = CONT_CMD_REQUEST_STATUS_TX;
  requestformat.rxsize = CONT_CMD_REQUEST_STATUS_RX;
  requestformat.cmd = cmd;
  requestformat.typeh = CONT_CMD_NOP;
  requestformat.typel = CONT_CMD_NOP;
  requestformat.status = CONT_CMD_NOP;
  requestformat.dummy1 = CONT_CMD_NOP;

  for (i = 0; i < __osMaxControllers; i++) {
    *((__OSContRequesFormat*)ptr) = requestformat;
    ptr += sizeof(__OSContRequesFormat);
  }
  *ptr = CONT_CMD_END;
}

/* Decode the per-channel replies in PIF RAM into the data[] array and build a
 * present-bitmask of channels that answered without error. */
void __osPfsGetInitData(u8* pattern, OSContStatus* data) {
  u8* ptr;
  __OSContRequesFormat requestformat;
  int i;
  u8 bits = 0;

  ptr = (u8*)&__osPfsPifRam;
  for (i = 0; i < __osMaxControllers;
       i++, ptr += sizeof(requestformat), data++) {
    requestformat = *((__OSContRequesFormat*)ptr);
    data->errno = CHNL_ERR(requestformat);
    if (data->errno != 0) {
      continue;
    }
    data->type = ((requestformat.typel << 8) | requestformat.typeh);
    data->status = requestformat.status;
    bits |= (1 << i);
  }
  *pattern = bits;
}
