/*
 * motor.c -- Rumble Pak (motor) driver.
 *
 * Drives the vibration motor in a Rumble Pak plugged into a controller port.
 * Turning the motor on or off is a Controller-Pak-style block write to the
 * rumble address; the pak echoes a fixed data CRC that confirms the command
 * took. Two implementations are selected by SDK version: the newer one
 * (VERSION_J) shares a single access routine, the older one keeps prebuilt
 * start/stop command buffers.
 */
#include "PRinternal/macros.h"
#include "PR/os_internal.h"
#include "PR/os_version.h"
#include "controller.h"
#include "siint.h"

#if BUILD_VERSION >= VERSION_J
extern OSPifRam __MotorDataBuf[MAXCONTROLLERS];

/* The rumble command shares the controller-RAM-read packet layout. */
#define READFORMAT(ptr) ((__OSContRamReadFormat*)(ptr))

/* Turn the motor on (flag != 0) or off (flag == 0) on the pak behind `pfs`.
 * Returns 5 if the pak was never initialized, PFS_ERR_CONTRFAIL if the pak's
 * echoed data CRC does not match the expected confirmation, else 0. */
s32 __osMotorAccess(OSPfs* pfs, s32 flag) {
  int i;
  s32 ret;
  u8* ptr = (u8*)&__MotorDataBuf[pfs->channel];

  if (!(pfs->status & PFS_MOTOR_INITIALIZED)) {
    return 5;
  }

  __osSiGetAccess();

  /* Fill the command block with the on/off flag, then run the write/read PIF
   * exchange; the second DMA reads back the pak's response. */
  __MotorDataBuf[pfs->channel].pifstatus = CONT_CMD_EXE;
  ptr += pfs->channel;
  for (i = 0; i < BLOCKSIZE; i++) {
    READFORMAT(ptr)->data[i] = flag;
  }
  __osContLastCmd = CONT_CMD_END;
  __osSiRawStartDma(OS_WRITE, &__MotorDataBuf[pfs->channel]);
  osRecvMesg(pfs->queue, NULL, OS_MESG_BLOCK);
  ret = __osSiRawStartDma(OS_READ, &__MotorDataBuf[pfs->channel]);
  osRecvMesg(pfs->queue, NULL, OS_MESG_BLOCK);

  /* The pak confirms the write by echoing a known data CRC: 0 for motor-off,
   * 0xEB for motor-on. Any other value means the command did not stick. */
  ret = READFORMAT(ptr)->rxsize & CHNL_ERR_MASK;
  if (!ret) {
    if (!flag) {
      if (READFORMAT(ptr)->datacrc != 0) {
        ret = PFS_ERR_CONTRFAIL;
      }
    } else {
      if (READFORMAT(ptr)->datacrc != 0xEB) {
        ret = PFS_ERR_CONTRFAIL;
      }
    }
  }

  __osSiRelAccess();
  return ret;
}

/* Prebuild the rumble write packet for `channel` into `mdata`: a write-pak
 * command aimed at the rumble block, preceded by skip bytes for the earlier
 * channels and capped with the end marker. */
static void __osMakeMotorData(int channel, OSPifRam* mdata) {
  u8* ptr = (u8*)mdata->ramarray;
  __OSContRamReadFormat ramreadformat;
  int i;

  ramreadformat.dummy = CONT_CMD_NOP;
  ramreadformat.txsize = CONT_CMD_WRITE_PAK_TX;
  ramreadformat.rxsize = CONT_CMD_WRITE_PAK_RX;
  ramreadformat.cmd = CONT_CMD_WRITE_PAK;
  ramreadformat.addrh = CONT_BLOCK_RUMBLE >> 3;
  ramreadformat.addrl =
      (u8)(__osContAddressCrc(CONT_BLOCK_RUMBLE) | (CONT_BLOCK_RUMBLE << 5));

  if (channel != 0) {
    for (i = 0; i < channel; i++) {
      *ptr++ = CONT_CMD_REQUEST_STATUS;
    }
  }
  *READFORMAT(ptr) = ramreadformat;
  ptr += sizeof(__OSContRamReadFormat);
  ptr[0] = CONT_CMD_END;
}

/* Probe and initialize the Rumble Pak on `channel`. Sets up the pfs handle,
 * confirms a rumble device is present by writing then reading the detect block
 * across two bank states, builds the cached command packet, and marks the pak
 * initialized. Returns 0 on success or a PFS error. */
s32 osMotorInit(OSMesgQueue* mq, OSPfs* pfs, int channel) {
  s32 ret;
  u8 temp[32];

  pfs->queue = mq;
  pfs->channel = channel;
  pfs->activebank = 0xFF;
  pfs->status = 0;

  /* Selecting bank 0xFE then 0x80 is the rumble-detect handshake; a fresh pak
   * answers the first select with NEW_PACK, so re-select before continuing. */
  ret = SELECT_BANK(pfs, 0xFE);
  if (ret == PFS_ERR_NEW_PACK) {
    ret = SELECT_BANK(pfs, 0x80);
  }
  if (ret != 0) {
    return ret;
  }
  ret = __osContRamRead(mq, channel, CONT_BLOCK_DETECT, temp);
  if (ret == PFS_ERR_NEW_PACK) {
    ret = PFS_ERR_CONTRFAIL;
  }
  if (ret != 0) {
    return ret;
  }
  if (temp[31] == 254) {
    return PFS_ERR_DEVICE;
  }

  /* Second half of the handshake: a real rumble pak now reads back 0x80. */
  ret = SELECT_BANK(pfs, 0x80);
  if (ret == PFS_ERR_NEW_PACK) {
    ret = PFS_ERR_CONTRFAIL;
  }
  if (ret != 0) {
    return ret;
  }
  ret = __osContRamRead(mq, channel, CONT_BLOCK_DETECT, temp);
  if (ret == PFS_ERR_NEW_PACK) {
    ret = PFS_ERR_CONTRFAIL;
  }
  if (ret != 0) {
    return ret;
  }
  if (temp[31] != 0x80) {
    return PFS_ERR_DEVICE;
  }

  if (!(pfs->status & PFS_MOTOR_INITIALIZED)) {
    __osMakeMotorData(channel, &__MotorDataBuf[channel]);
  }
  pfs->status = PFS_MOTOR_INITIALIZED;
  return 0;
}
#else
/* Older driver: separate start/stop entry points backed by command buffers that
 * are filled once at init. All buffers are 8-byte aligned for SI DMA. */
OSPifRam _MotorStopData[MAXCONTROLLERS] ALIGNED(0x8);
OSPifRam _MotorStartData[MAXCONTROLLERS] ALIGNED(0x8);
u8 _motorstopbuf[32] ALIGNED(0x8);
u8 _motorstartbuf[32] ALIGNED(0x8);

/* Per-channel "rumble pak initialized" flags (0 = not yet set up). */
u32 __osMotorinitialized[MAXCONTROLLERS] = {0, 0, 0, 0};

/* Stop the motor on `pfs`'s channel by sending the prebuilt stop packet and
 * verifying the pak's echoed data CRC. Returns PFS_ERR_INVALID if the pak was
 * not initialized, PFS_ERR_CONTRFAIL on a CRC mismatch, else 0. */
s32 osMotorStop(OSPfs* pfs) {
  int i;
  s32 ret;
  u8* ptr;
  __OSContRamReadFormat ramreadformat;

  ptr = (u8*)&__osPfsPifRam;
  if (!__osMotorinitialized[pfs->channel]) {
    return PFS_ERR_INVALID;
  }

  __osSiGetAccess();
  __osContLastCmd = CONT_CMD_WRITE_PAK;
  __osSiRawStartDma(OS_WRITE, &_MotorStopData[pfs->channel]);
  osRecvMesg(pfs->queue, NULL, OS_MESG_BLOCK);
  ret = __osSiRawStartDma(OS_READ, &__osPfsPifRam);
  osRecvMesg(pfs->queue, NULL, OS_MESG_BLOCK);

  /* Walk to this channel's reply slot and check it against the stop pattern. */
  ptr = (u8*)&__osPfsPifRam;
  if (pfs->channel != 0) {
    for (i = 0; i < pfs->channel; i++) {
      ptr++;
    }
  }
  ramreadformat = *(__OSContRamReadFormat*)ptr;
  ret = CHNL_ERR(ramreadformat);
  if (ret == 0 &&
      __osContDataCrc((u8*)&_motorstopbuf) != ramreadformat.datacrc) {
    ret = PFS_ERR_CONTRFAIL;
  }
  __osSiRelAccess();
  return ret;
}

/* Start the motor on `pfs`'s channel; mirror of osMotorStop using the start
 * packet and the start pattern. Same return codes. */
s32 osMotorStart(OSPfs* pfs) {
  int i;
  s32 ret;
  u8* ptr;
  __OSContRamReadFormat ramreadformat;

  ptr = (u8*)&__osPfsPifRam;
  if (!__osMotorinitialized[pfs->channel]) {
    return PFS_ERR_INVALID;
  }

  __osSiGetAccess();
  __osContLastCmd = CONT_CMD_WRITE_PAK;
  __osSiRawStartDma(OS_WRITE, &_MotorStartData[pfs->channel]);
  osRecvMesg(pfs->queue, NULL, OS_MESG_BLOCK);
  ret = __osSiRawStartDma(OS_READ, &__osPfsPifRam);
  osRecvMesg(pfs->queue, NULL, OS_MESG_BLOCK);

  ptr = (u8*)&__osPfsPifRam;
  if (pfs->channel != 0) {
    for (i = 0; i < pfs->channel; i++) {
      ptr++;
    }
  }
  ramreadformat = *(__OSContRamReadFormat*)ptr;
  ret = CHNL_ERR(ramreadformat);
  if (ret == 0 &&
      __osContDataCrc((u8*)&_motorstartbuf) != ramreadformat.datacrc) {
    ret = PFS_ERR_CONTRFAIL;
  }
  __osSiRelAccess();
  return ret;
}

/* Build a rumble write packet from `buffer` into `mdata` for `channel`,
 * targeting `address`. Zeroes the command array, sets the write-pak header with
 * the address CRC, copies the data payload, and pads the earlier channels. */
static void _MakeMotorData(int channel, u16 address, u8* buffer,
                           OSPifRam* mdata) {
  u8* ptr = (u8*)mdata->ramarray;
  __OSContRamReadFormat ramreadformat;
  int i;

  for (i = 0; i < ARRLEN(mdata->ramarray); i++) {
    mdata->ramarray[i] = 0;
  }
  mdata->pifstatus = CONT_CMD_EXE;
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
      *ptr++ = 0;
    }
  }
  *(__OSContRamReadFormat*)ptr = ramreadformat;
  ptr += sizeof(__OSContRamReadFormat);
  ptr[0] = CONT_CMD_END;
}

/* Probe and initialize the Rumble Pak on `channel` (older driver). Confirms a
 * rumble device with the 0xFE/0x80 detect-block handshake, then builds the
 * cached start and stop packets and marks the channel initialized. Returns 0 on
 * success or a PFS error. */
s32 osMotorInit(OSMesgQueue* mq, OSPfs* pfs, int channel) {
  int i;
  s32 ret;
  u8 temp[32];

  pfs->queue = mq;
  pfs->channel = channel;
  pfs->status = 0;
  pfs->activebank = 128;

  /* Write 0xFE to every byte of the detect block; retry once on a fresh pak. */
  for (i = 0; i < ARRLEN(temp); i++) {
    temp[i] = 254;
  }
  ret = __osContRamWrite(mq, channel, CONT_BLOCK_DETECT, temp, FALSE);
  if (ret == PFS_ERR_NEW_PACK) {
    ret = __osContRamWrite(mq, channel, CONT_BLOCK_DETECT, temp, FALSE);
  }
  if (ret != 0) {
    return ret;
  }
  ret = __osContRamRead(mq, channel, CONT_BLOCK_DETECT, temp);
  if (ret == PFS_ERR_NEW_PACK) {
    ret = PFS_ERR_CONTRFAIL;
  }
  if (ret != 0) {
    return ret;
  }

  /* A device that reads 0xFE back is not a rumble pak. */
  if (temp[31] == 254) {
    return PFS_ERR_DEVICE;
  }

  /* Second handshake byte: a rumble pak must read back 0x80. */
  for (i = 0; i < ARRLEN(temp); i++) {
    temp[i] = 128;
  }
  ret = __osContRamWrite(mq, channel, CONT_BLOCK_DETECT, temp, FALSE);
  if (ret == PFS_ERR_NEW_PACK) {
    ret = __osContRamWrite(mq, channel, CONT_BLOCK_DETECT, temp, FALSE);
  }
  if (ret != 0) {
    return ret;
  }
  ret = __osContRamRead(mq, channel, CONT_BLOCK_DETECT, temp);
  if (ret == PFS_ERR_NEW_PACK) {
    ret = PFS_ERR_CONTRFAIL;
  }
  if (ret != 0) {
    return ret;
  }
  if (temp[31] != 0x80) {
    return PFS_ERR_DEVICE;
  }

  /* First time on this channel, prebuild the on (all 1s) and off (all 0s)
   * command packets so start/stop are just a DMA away. */
  if (!__osMotorinitialized[channel]) {
    for (i = 0; i < ARRLEN(_motorstartbuf); i++) {
      _motorstartbuf[i] = 1;
      _motorstopbuf[i] = 0;
    }
    _MakeMotorData(channel, CONT_BLOCK_RUMBLE, _motorstartbuf,
                   &_MotorStartData[channel]);
    _MakeMotorData(channel, CONT_BLOCK_RUMBLE, _motorstopbuf,
                   &_MotorStopData[channel]);
    __osMotorinitialized[channel] = 1;
  }
  return 0;
}
#endif
