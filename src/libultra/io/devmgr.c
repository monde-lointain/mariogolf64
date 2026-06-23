/*
 * devmgr.c -- device-manager thread: the serialization point for all
 * peripheral I/O.
 *
 * __osDevMgrMain loops forever pulling I/O request messages off a command
 * queue and executing each one, holding the access semaphore for the duration
 * so only one transfer touches the bus at a time. Plain DMA/EDMA requests go
 * through the generic dispatch at the bottom; 64DD (LeoDisk) block transfers
 * take a dedicated path that pokes the disk's block-mode control register and
 * handles its retry/error semantics directly.
 */
#include "PR/os_internal.h"
#include "PR/rcp.h"
#include "piint.h"

void __osDevMgrMain(void* args) {
  OSIoMesg* mb;
  OSMesg em;
  OSMesg dummy;
  s32 ret;
  OSDevMgr* dm;
  s32 messageSend = 0;
  dm = (OSDevMgr*)args;
  mb = NULL;
  ret = 0;
  while (TRUE) {
    osRecvMesg(dm->cmdQueue, (OSMesg)&mb, OS_MESG_BLOCK);

    // 64DD block-transfer path: the disk drive is driven directly through its
    // block-mode control register rather than the generic DMA call.
    if (mb->piHandle != NULL && mb->piHandle->type == DEVICE_TYPE_64DD &&
        (mb->piHandle->transferInfo.cmdType == LEO_CMD_TYPE_0 ||
         mb->piHandle->transferInfo.cmdType == LEO_CMD_TYPE_1)) {
      __OSBlockInfo* blockInfo;
      __OSTranxInfo* info;
      info = &mb->piHandle->transferInfo;
      blockInfo = &info->block[info->blockNum];
      info->sectorNum = -1;

      // Outside sector mode the drive auto-advances, so back the DRAM pointer
      // up one sector to land the transfer where the caller expects it.
      if (info->transferMode != LEO_SECTOR_MODE) {
        blockInfo->dramAddr =
            (void*)((u32)blockInfo->dramAddr - blockInfo->sectorSize);
      }

      // A track-mode read of the first block expects a second event (the loop
      // below re-reads it); flag that so the second wait happens.
      if (info->transferMode == LEO_TRACK_MODE &&
          mb->piHandle->transferInfo.cmdType == LEO_CMD_TYPE_0) {
        messageSend = 1;
      } else {
        messageSend = 0;
      }

      // Take the bus, unmask PI, and arm the block-mode transfer (high bit of
      // the shadowed control value is the start/enable bit).
      osRecvMesg(dm->acsQueue, &dummy, OS_MESG_BLOCK);
      __osResetGlobalIntMask(OS_IM_PI);
      __osEPiRawWriteIo(mb->piHandle, LEO_BM_CTL,
                        (info->bmCtlShadow | 0x80000000));
    readblock1:
      // Wait for the drive's block-done interrupt event, then re-read the
      // current block descriptor (it may have advanced).
      osRecvMesg(dm->evtQueue, &em, OS_MESG_BLOCK);
      info = &mb->piHandle->transferInfo;
      blockInfo = &info->block[info->blockNum];

      // A "29" error means the drive needs a block-mode reset: pulse reset,
      // restore the control value, and if the mechanism raised its own
      // interrupt, clear that too. Demote the error to the recoverable code,
      // ack the PI interrupt, and re-enable PI so the caller can retry.
      if (blockInfo->errStatus == LEO_ERROR_29) {
        u32 stat;
        __osEPiRawWriteIo(mb->piHandle, LEO_BM_CTL,
                          info->bmCtlShadow | LEO_BM_CTL_RESET);
        __osEPiRawWriteIo(mb->piHandle, LEO_BM_CTL, info->bmCtlShadow);
        __osEPiRawReadIo(mb->piHandle, LEO_STATUS, &stat);
        if (stat & LEO_STATUS_MECHANIC_INTERRUPT) {
          __osEPiRawWriteIo(mb->piHandle, LEO_BM_CTL,
                            info->bmCtlShadow | LEO_BM_CTL_CLR_MECHANIC_INTR);
        }
        blockInfo->errStatus = LEO_ERROR_4;
        IO_WRITE(PI_STATUS_REG, PI_CLR_INTR);
        __osSetGlobalIntMask(OS_IM_PI | SR_IBIT4);
      }

      // Report this block to the caller. In the track-mode first-block case, if
      // block 0 came back clean, loop to read the second block before releasing
      // the bus.
      osSendMesg(mb->hdr.retQueue, mb, OS_MESG_NOBLOCK);
      if (messageSend == 1 &&
          mb->piHandle->transferInfo.block[0].errStatus == LEO_ERROR_GOOD) {
        messageSend = 0;
        goto readblock1;
      }
      osSendMesg(dm->acsQueue, NULL, OS_MESG_NOBLOCK);

      // After a two-block transfer, yield so a waiting thread can run.
      if (mb->piHandle->transferInfo.blockNum == 1) {
        osYieldThread();
      }
    } else {
      // Generic path: take the bus and run the request through the device's
      // plain DMA or expanded-PI (EDMA) callback by message type.
      switch (mb->hdr.type) {
        case OS_MESG_TYPE_DMAREAD:
          osRecvMesg(dm->acsQueue, &dummy, OS_MESG_BLOCK);
          ret = dm->dma(OS_READ, mb->devAddr, mb->dramAddr, mb->size);
          break;
        case OS_MESG_TYPE_DMAWRITE:
          osRecvMesg(dm->acsQueue, &dummy, OS_MESG_BLOCK);
          ret = dm->dma(OS_WRITE, mb->devAddr, mb->dramAddr, mb->size);
          break;
        case OS_MESG_TYPE_EDMAREAD:
          osRecvMesg(dm->acsQueue, &dummy, OS_MESG_BLOCK);
          ret = dm->edma(mb->piHandle, OS_READ, mb->devAddr, mb->dramAddr,
                         mb->size);
          break;
        case OS_MESG_TYPE_EDMAWRITE:
          osRecvMesg(dm->acsQueue, &dummy, OS_MESG_BLOCK);
          ret = dm->edma(mb->piHandle, OS_WRITE, mb->devAddr, mb->dramAddr,
                         mb->size);
          break;
        case OS_MESG_TYPE_LOOPBACK:
          // Loopback just echoes the message straight back; nothing to wait on.
          osSendMesg(mb->hdr.retQueue, mb, OS_MESG_NOBLOCK);
          ret = -1;
          break;
        default:
          ret = -1;
          break;
      }

      // A successful DMA kickoff (ret == 0) is asynchronous: wait for the
      // completion event, then reply and release the bus. Echoed/failed cases
      // already handled their own reply.
      if (ret == 0) {
        osRecvMesg(dm->evtQueue, &em, OS_MESG_BLOCK);
        osSendMesg(mb->hdr.retQueue, mb, OS_MESG_NOBLOCK);
        osSendMesg(dm->acsQueue, NULL, OS_MESG_NOBLOCK);
      }
    }
  }
}
