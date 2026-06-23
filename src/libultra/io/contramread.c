/*
 * contramread.c -- Controller Pak block read: fetch one BLOCKSIZE block from
 * the Pak plugged into a controller's joyport. The Pak speaks the joybus
 * read-pak command via PIF RAM; __osContRamRead builds that command, runs the
 * SI exchange, validates the returned data CRC, and retries on a CRC mismatch.
 */
#include "PRinternal/macros.h"
#include "PR/os_internal.h"
#include "PR/rcp.h"
#include "controller.h"
#include "siint.h"

// Reinterpret a byte cursor in PIF RAM as a typed read-pak command/response.
#define READFORMAT(ptr) ((__OSContRamReadFormat*)(ptr))

#if BUILD_VERSION >= VERSION_J

// Which controller port the last Pak command targeted; lets back-to-back reads
// on the same channel skip rebuilding the command block.
s32 __osPfsLastChannel = -1;

s32 __osContRamRead(OSMesgQueue* mq, int channel, u16 address, u8* buffer) {
  s32 ret = 0;
  s32 i;
  u8* ptr;
  s32 retry = 2;
  __osSiGetAccess();
  do {
    ptr = (u8*)&__osPfsPifRam;

    // Rebuild the command block only when the previous command or channel
    // differed. Earlier ports get a one-byte status request as padding so the
    // read-pak command lands on the target channel.
    if (__osContLastCmd != CONT_CMD_READ_PAK ||
        (u32)__osPfsLastChannel != channel) {
      __osContLastCmd = CONT_CMD_READ_PAK;
      __osPfsLastChannel = channel;
      for (i = 0; i < channel; i++) {
        *ptr++ = CONT_CMD_REQUEST_STATUS;
      }
      __osPfsPifRam.pifstatus = CONT_CMD_EXE;
      READFORMAT(ptr)->dummy = CONT_CMD_NOP;
      READFORMAT(ptr)->txsize = CONT_CMD_READ_PAK_TX;
      READFORMAT(ptr)->rxsize = CONT_CMD_READ_PAK_RX;
      READFORMAT(ptr)->cmd = CONT_CMD_READ_PAK;
      READFORMAT(ptr)->datacrc = 0xFF;
      ptr[sizeof(__OSContRamReadFormat)] = CONT_CMD_END;
    } else {
      ptr += channel;
    }

    // The 16-bit block address is sent as a high byte plus a low byte whose
    // bottom 5 bits carry the address CRC the Pak checks.
#if BUILD_VERSION >= VERSION_J
    READFORMAT(ptr)->addrh = address >> 3;
    READFORMAT(ptr)->addrl = (u8)((address << 5) | __osContAddressCrc(address));
#else
    READFORMAT(ptr)->address = (address << 0x5) | __osContAddressCrc(address);
#endif

    // Send the command, then read the Pak's reply back into the same buffer.
    ret = __osSiRawStartDma(OS_WRITE, &__osPfsPifRam);
    osRecvMesg(mq, NULL, OS_MESG_BLOCK);
    ret = __osSiRawStartDma(OS_READ, &__osPfsPifRam);
    osRecvMesg(mq, NULL, OS_MESG_BLOCK);

    // A channel error means no Pak responded. Otherwise verify the data CRC: a
    // mismatch may mean the Pak was swapped, so re-probe its status. A still-OK
    // status downgrades to a soft failure that the retry loop re-attempts.
    ret = CHNL_ERR(*READFORMAT(ptr));
    if (!ret) {
      if (__osContDataCrc(READFORMAT(ptr)->data) != READFORMAT(ptr)->datacrc) {
        ret = __osPfsGetStatus(mq, channel);
        if (ret) {
          break;
        } else {
          ret = PFS_ERR_CONTRFAIL;
        }
      } else {
        bcopy(READFORMAT(ptr)->data, buffer, BLOCKSIZE);
      }
    } else {
      ret = PFS_ERR_NOPACK;
    }
  } while ((ret == PFS_ERR_CONTRFAIL) && (retry-- >= 0));
  __osSiRelAccess();
  return ret;
}
#else

/* Pre-VERSION_J variant: builds the command via a dedicated packer and always
 * rewrites the block (no last-channel fast path), but the read/validate/retry
 * shape is the same. */
static void __osPackRamReadData(int channel, u16 address);
s32 __osContRamRead(OSMesgQueue* mq, int channel, u16 address, u8* buffer) {
  s32 ret = 0;
  int i;
  u8* ptr = (u8*)&__osPfsPifRam;
  __OSContRamReadFormat ramreadformat;
  int retry = 2;
  __osSiGetAccess();
  __osContLastCmd = CONT_CMD_READ_PAK;
  __osPackRamReadData(channel, address);
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
      u8 c = __osContDataCrc((u8*)&ramreadformat.data);
      if (c != ramreadformat.datacrc) {
        ret = __osPfsGetStatus(mq, channel);
        if (ret != 0) {
          __osSiRelAccess();
          return ret;
        }
        ret = PFS_ERR_CONTRFAIL;
      } else {
        for (i = 0; i < ARRLEN(ramreadformat.data); i++) {
          *buffer++ = ramreadformat.data[i];
        }
      }
    } else {
      ret = PFS_ERR_NOPACK;
    }
  } while ((ret == PFS_ERR_CONTRFAIL) && retry-- >= 0);
  __osSiRelAccess();
  return ret;
}

/* Build the legacy read-pak command block: stage it in a local, fill the
 * fixed fields plus the address-with-CRC, pad earlier channels, copy it into
 * PIF RAM, and append the end marker. */
static void __osPackRamReadData(int channel, u16 address) {
  u8* ptr;
  __OSContRamReadFormat ramreadformat;
  int i;
  ptr = (u8*)__osPfsPifRam.ramarray;
  __osPfsPifRam.pifstatus = CONT_CMD_EXE;
  ramreadformat.dummy = CONT_CMD_NOP;
  ramreadformat.txsize = CONT_CMD_READ_PAK_TX;
  ramreadformat.rxsize = CONT_CMD_READ_PAK_RX;
  ramreadformat.cmd = CONT_CMD_READ_PAK;
  ramreadformat.address = (address << 0x5) | __osContAddressCrc(address);
  ramreadformat.datacrc = CONT_CMD_NOP;
  for (i = 0; i < ARRLEN(ramreadformat.data); i++) {
    ramreadformat.data[i] = CONT_CMD_NOP;
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
