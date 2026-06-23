/*
 * pfsgetstatus.c -- Controller Pak presence/health probe for a single channel.
 *
 * Issues a status request to one controller port, runs the PIF transaction, and
 * decodes the reply into a PFS error code that says whether a usable pak is
 * present. The request/decode are split into small helpers so the version with
 * the extra command byte (VERSION_J) can be selected at compile time.
 */
#include "PR/os_internal.h"
#include "controller.h"
#include "siint.h"

#if BUILD_VERSION >= VERSION_J
void __osPfsRequestOneChannel(int channel, u8 cmd);
#else
void __osPfsRequestOneChannel(int channel);
#endif
void __osPfsGetOneChannelData(int channel, OSContStatus* data);

/* Probe `channel` for a Controller Pak. Returns PFS_ERR_NEW_PACK if a pak was
 * just inserted, PFS_ERR_NOPACK if none is present, PFS_ERR_CONTRFAIL on a
 * CRC/address fault, else the DMA status (0 = a healthy pak). */
s32 __osPfsGetStatus(OSMesgQueue* queue, int channel) {
  s32 ret = 0;
  OSMesg dummy;
  OSContStatus data;

  /* Build the status request, then write it to the PIF and read the reply back;
   * each DMA completes via a blocking receive on the SI queue. */
#if BUILD_VERSION >= VERSION_J
  __osPfsInodeCacheBank = 250;
  __osPfsRequestOneChannel(channel, CONT_CMD_REQUEST_STATUS);
#else
  __osPfsRequestOneChannel(channel);
#endif
  ret = __osSiRawStartDma(OS_WRITE, &__osPfsPifRam);
  osRecvMesg(queue, &dummy, OS_MESG_BLOCK);
  ret = __osSiRawStartDma(OS_READ, &__osPfsPifRam);
  osRecvMesg(queue, &dummy, OS_MESG_BLOCK);
  __osPfsGetOneChannelData(channel, &data);

  /* "Inserted" plus "removed" bits both set means a fresh insertion; otherwise
   * map an error or an absent card to the matching PFS code. */
  if (((data.status & CONT_CARD_ON) != 0) &&
      ((data.status & CONT_CARD_PULL) != 0)) {
    return PFS_ERR_NEW_PACK;
  }
  if ((data.errno != 0) || ((data.status & CONT_CARD_ON) == 0)) {
    return PFS_ERR_NOPACK;
  }
  if ((data.status & CONT_ADDR_CRC_ER) != 0) {
    return PFS_ERR_CONTRFAIL;
  }
  return ret;
}

/* Lay a single-channel status request into PIF RAM: pad the earlier channels
 * with skip bytes, drop the request packet at this channel's slot, and cap it
 * with the end marker. */
#if BUILD_VERSION >= VERSION_J
void __osPfsRequestOneChannel(int channel, u8 cmd) {
#else
void __osPfsRequestOneChannel(int channel) {
#endif
  u8* ptr;
  __OSContRequesFormatShort requestformat;
  int i;

#if BUILD_VERSION >= VERSION_J
  __osContLastCmd = CONT_CMD_END;
#else
  __osContLastCmd = CONT_CMD_REQUEST_STATUS;
#endif
  __osPfsPifRam.pifstatus = CONT_CMD_READ_BUTTON;
  ptr = (u8*)&__osPfsPifRam;
  requestformat.txsize = CONT_CMD_REQUEST_STATUS_TX;
  requestformat.rxsize = CONT_CMD_REQUEST_STATUS_RX;
#if BUILD_VERSION >= VERSION_J
  requestformat.cmd = cmd;
#else
  requestformat.cmd = CONT_CMD_REQUEST_STATUS;
#endif
  requestformat.typeh = CONT_CMD_NOP;
  requestformat.typel = CONT_CMD_NOP;
  requestformat.status = CONT_CMD_NOP;

  /* Skip the channels before the one of interest, then place the request. */
  for (i = 0; i < channel; i++) {
    *ptr++ = CONT_CMD_REQUEST_STATUS;
  }
  *(__OSContRequesFormatShort*)ptr = requestformat;
  ptr += sizeof(__OSContRequesFormatShort);
  *ptr = CONT_CMD_END;
}

/* Pull the reply for `channel` out of PIF RAM into `data`: the channel error,
 * and on success the device type and status bits. */
void __osPfsGetOneChannelData(int channel, OSContStatus* data) {
  u8* ptr = (u8*)&__osPfsPifRam;
  __OSContRequesFormatShort requestformat;
  int i;

  for (i = 0; i < channel; i++) {
    ptr++;
  }
  requestformat = *(__OSContRequesFormatShort*)ptr;
  data->errno = CHNL_ERR(requestformat);
  if (data->errno != 0) {
    return;
  }
  data->type = (requestformat.typel << 8) | (requestformat.typeh);
  data->status = requestformat.status;
}
