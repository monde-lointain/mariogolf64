/*
 * epidma.c -- queue an asynchronous EPI (Enhanced PI) DMA request.
 *
 * Validates the request, tags it with its direction, then hands it to the PI
 * manager thread, which performs the actual transfer and replies on the
 * caller's message queue. This is the non-blocking front end to PI/cartridge
 * DMA; the heavy lifting happens later in the manager.
 */
#include "piint.h"
#include "PR/ultraerror.h"

s32 osEPiStartDma(OSPiHandle* pihandle, OSIoMesg* mb, s32 direction) {
  register s32 ret;

  /* The PI manager must be running; without it there is nobody to service the
   * request. */
  if (!__osPiDevMgr.active) {
#ifdef _DEBUG
    __osError(ERR_OSPISTARTDMA_PIMGR, 0);
#endif
    return -1;
  }

  /* Reject malformed requests up front in debug builds: bad priority/direction,
   * an odd device address, a misaligned (non-8-byte) RDRAM address, an odd
   * size, or a size outside (0, 16MB]. */
#ifdef _DEBUG
  if ((mb->hdr.pri != OS_MESG_PRI_NORMAL) &&
      (mb->hdr.pri != OS_MESG_PRI_HIGH)) {
    __osError(ERR_OSPISTARTDMA_PRI, 1, mb->hdr.pri);
    return -1;
  }
  if ((direction != OS_READ) && (direction != OS_WRITE)) {
    __osError(ERR_OSPISTARTDMA_DIR, 1, direction);
    return -1;
  }
  if (mb->devAddr & 0x1) {
    __osError(ERR_OSPISTARTDMA_DEVADDR, 1, mb->devAddr);
    return -1;
  }
  if ((u32)mb->dramAddr & 0x7) {
    __osError(ERR_OSPISTARTDMA_ADDR, 1, mb->dramAddr);
    return -1;
  }
  if (mb->size & 0x1) {
    __osError(ERR_OSPISTARTDMA_SIZE, 1, mb->size);
    return -1;
  }
  if ((mb->size == 0) || (mb->size > (16 * 1024 * 1024))) {
    __osError(ERR_OSPISTARTDMA_RANGE, 1, mb->size);
    return -1;
  }
#endif

  /* Stamp the request with the handle and read/write type the manager will act
   * on. */
  mb->piHandle = pihandle;
  if (direction == OS_READ) {
    mb->hdr.type = OS_MESG_TYPE_EDMAREAD;
  } else {
    mb->hdr.type = OS_MESG_TYPE_EDMAWRITE;
  }

  /* Enqueue for the manager. A high-priority request jumps to the front of the
   * command queue (jam) rather than waiting its turn at the back (send). */
  if (mb->hdr.pri == OS_MESG_PRI_HIGH) {
    ret = osJamMesg(osPiGetCmdQueue(), (OSMesg)mb, OS_MESG_NOBLOCK);
  } else {
    ret = osSendMesg(osPiGetCmdQueue(), (OSMesg)mb, OS_MESG_NOBLOCK);
  }
  return ret;
}
