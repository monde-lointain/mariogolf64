/*
 * contramwrite.c -- Controller Pak block write: store one BLOCKSIZE block to
 * the Pak. Mirrors the read path (build the joybus write-pak command in PIF
 * RAM, run the SI exchange, retry on failure), but the CRC check runs the other
 * direction: __osContRamWrite computes the CRC of what it sent and compares it
 * to the CRC the Pak echoes back, confirming the write took.
 */
#include "PRinternal/macros.h"
#include "PR/os_internal.h"
#include "PR/rcp.h"
#include "controller.h"
#include "siint.h"

// Shared with contramread.c: the port the last Pak command used.
extern s32 __osPfsLastChannel;

// Reinterpret a byte cursor in PIF RAM as a typed write-pak command/response.
#define READFORMAT(ptr) ((__OSContRamReadFormat*)(ptr))

#if BUILD_VERSION >= VERSION_J

s32 __osContRamWrite(OSMesgQueue* mq, int channel, u16 address, u8* buffer,
                     int force) {
  s32 ret = 0;
  s32 i;
  u8* ptr;
  s32 retry = 2;
  u8 crc;

  // Block 0 holds the label/ID; refuse to overwrite the protected label area
  // unless the caller explicitly forces it.
  if ((force != TRUE) && (address < PFS_LABEL_AREA) && (address != 0)) {
    return 0;
  }
  __osSiGetAccess();
  do {
    ptr = (u8*)__osPfsPifRam.ramarray;

    // Rebuild the command block only on a command/channel change; pad earlier
    // ports with status requests so the write lands on the target channel.
    if (__osContLastCmd != CONT_CMD_WRITE_PAK ||
        (u32)__osPfsLastChannel != channel) {
      __osContLastCmd = CONT_CMD_WRITE_PAK;
      __osPfsLastChannel = channel;
      for (i = 0; i < channel; i++) {
        *ptr++ = CONT_CMD_REQUEST_STATUS;
      }
      __osPfsPifRam.pifstatus = CONT_CMD_EXE;
      READFORMAT(ptr)->dummy = CONT_CMD_NOP;
      READFORMAT(ptr)->txsize = CONT_CMD_WRITE_PAK_TX;
      READFORMAT(ptr)->rxsize = CONT_CMD_WRITE_PAK_RX;
      READFORMAT(ptr)->cmd = CONT_CMD_WRITE_PAK;
      READFORMAT(ptr)->datacrc = 0xFF;
      ptr[sizeof(__OSContRamReadFormat)] = CONT_CMD_END;
    } else {
      ptr += channel;
    }

    // Address high byte plus low byte carrying the address CRC, as in the read.
#if BUILD_VERSION >= VERSION_J
    READFORMAT(ptr)->addrh = address >> 3;
    READFORMAT(ptr)->addrl = ((address << 5) | __osContAddressCrc(address));
#else
    READFORMAT(ptr)->address = (address << 0x5) | __osContAddressCrc(address);
#endif

    // Stage the payload, fire the write, and compute the CRC of what we sent so
    // we can match it against the Pak's echo.
    bcopy(buffer, READFORMAT(ptr)->data, BLOCKSIZE);
    ret = __osSiRawStartDma(OS_WRITE, &__osPfsPifRam);
    crc = __osContDataCrc(buffer);
    osRecvMesg(mq, NULL, OS_MESG_BLOCK);
    ret = __osSiRawStartDma(OS_READ, &__osPfsPifRam);
    osRecvMesg(mq, NULL, OS_MESG_BLOCK);

    // No channel error means the Pak answered. If its returned CRC disagrees
    // with ours the write may have hit a swapped Pak, so re-probe status; an
    // otherwise-OK status becomes a soft failure the retry loop re-attempts.
    ret = CHNL_ERR(*READFORMAT(ptr));
    if (!ret) {
      if (crc != READFORMAT(ptr)->datacrc) {
        if ((ret = __osPfsGetStatus(mq, channel))) {
          break;
        } else {
          ret = PFS_ERR_CONTRFAIL;
        }
      }
    } else {
      ret = PFS_ERR_NOPACK;
    }
  } while ((ret == PFS_ERR_CONTRFAIL) && (retry-- >= 0));
  __osSiRelAccess();
  return ret;
}
#else

/* Pre-VERSION_J variant: packs the write command through a dedicated helper and
 * always rewrites the block, but applies the same label-area guard and retry.
 */
static void __osPackRamWriteData(int channel, u16 address, u8* buffer);
s32 __osContRamWrite(OSMesgQueue* mq, int channel, u16 address, u8* buffer,
                     int force) {
  s32 ret = 0;
  s32 i;
  u8* ptr = (u8*)&__osPfsPifRam;
  __OSContRamReadFormat ramreadformat;
  s32 retry = 2;
  u8 crc;
  if ((force != TRUE) && (address < PFS_LABEL_AREA) && (address != 0)) {
    return 0;
  }
  __osSiGetAccess();
  __osContLastCmd = CONT_CMD_WRITE_PAK;
  __osPackRamWriteData(channel, address, buffer);
  ret = __osSiRawStartDma(OS_WRITE, &__osPfsPifRam);
  osRecvMesg(mq, NULL, OS_MESG_BLOCK);
  do {
    ret = __osSiRawStartDma(OS_READ, &__osPfsPifRam);
    osRecvMesg(mq, NULL, OS_MESG_BLOCK);
    ptr = (u8*)&__osPfsPifRam;
    if (channel != 0) {
      for (i = 0; i < channel; i++) {
        ptr++;
      }
    }
    ramreadformat = *READFORMAT(ptr);
    ret = CHNL_ERR(ramreadformat);
    if (ret == 0) {
      if (__osContDataCrc(buffer) != ramreadformat.datacrc) {
        ret = __osPfsGetStatus(mq, channel);
        if (ret != 0) {
          __osSiRelAccess();
          return ret;
        }
        ret = PFS_ERR_CONTRFAIL;
      }
    } else {
      ret = PFS_ERR_NOPACK;
    }
  } while ((ret == PFS_ERR_CONTRFAIL) && retry-- >= 0);
  __osSiRelAccess();
  return ret;
}

/* Build the legacy write-pak command block: stage it in a local, fill the
 * fixed fields, the address-with-CRC, and the BLOCKSIZE payload, pad earlier
 * channels, copy it into PIF RAM, and append the end marker. */
static void __osPackRamWriteData(int channel, u16 address, u8* buffer) {
  u8* ptr;
  __OSContRamReadFormat ramreadformat;
  int i;
  ptr = (u8*)__osPfsPifRam.ramarray;
  __osPfsPifRam.pifstatus = CONT_CMD_EXE;
  ramreadformat.dummy = CONT_CMD_NOP;
  ramreadformat.txsize = CONT_CMD_WRITE_PAK_TX;
  ramreadformat.rxsize = CONT_CMD_WRITE_PAK_RX;
  ramreadformat.cmd = CONT_CMD_WRITE_PAK;
  ramreadformat.address = (address << 0x5) | __osContAddressCrc(address);
  ramreadformat.datacrc = CONT_CMD_NOP;
  for (i = 0; i < ARRLEN(ramreadformat.data); i++) {
    ramreadformat.data[i] = *buffer++;
  }
  if (channel != 0) {
    for (i = 0; i < channel; i++) {
      *ptr++ = CONT_CMD_REQUEST_STATUS;
    }
  }
  *(__OSContRamReadFormat*)ptr = ramreadformat;
  ptr += sizeof(__OSContRamReadFormat);
  ptr[0] = CONT_CMD_END;
}
#endif
