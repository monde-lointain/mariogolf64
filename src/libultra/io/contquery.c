/*
 * contquery.c -- controller query: probe each port for what kind of SI device
 * (controller, mouse, etc.) is attached and whether a Pak is present. The
 * start/get pair works asynchronously: start kicks off the PIF exchange, get
 * unpacks it once the SI completion message has arrived.
 */
#include "PR/os_internal.h"
#include "controller.h"
#include "siint.h"

/* Issue a status-request command to the PIF and start the SI DMA that carries
 * it. The caller blocks on mq for completion before calling osContGetQuery. */
s32 osContStartQuery(OSMesgQueue* mq) {
  s32 ret = 0;
  __osSiGetAccess();

  // Rebuild the PIF command block only when the last command was something
  // else; back-to-back queries can reuse the block already in PIF RAM and skip
  // the write DMA entirely.
  if (__osContLastCmd != CONT_CMD_REQUEST_STATUS) {
    __osPackRequestData(CONT_CMD_REQUEST_STATUS);
    ret = __osSiRawStartDma(OS_WRITE, __osContPifRam.ramarray);
    osRecvMesg(mq, NULL, OS_MESG_BLOCK);
  }

  // Read the PIF response back into RAM; the actual data is decoded later.
  ret = __osSiRawStartDma(OS_READ, __osContPifRam.ramarray);
  __osContLastCmd = CONT_CMD_REQUEST_STATUS;
  __osSiRelAccess();
  return ret;
}

/* Decode the queued PIF response into per-port status. The connection bitmap
 * is irrelevant to a query (callers want the per-device status), so it is
 * discarded into a throwaway local. */
void osContGetQuery(OSContStatus* data) {
  u8 pattern;
  __osContGetInitData(&pattern, data);
}
