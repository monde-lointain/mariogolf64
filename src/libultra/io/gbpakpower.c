/*
 * gbpakpower.c -- switch a Game Boy Transfer Pak's cartridge power on or off.
 */
#include "PR/os_internal.h"
#include "controller.h"
#include "controller_gbpak.h"

s32 osGbpakPower(OSPfs* pfs, s32 flag) {
  s32 i;
  s32 ret;
  u8 temp[BLOCKSIZE];

  /* Fill the whole status block with the requested power flag and write it. */
  for (i = 0; i < BLOCKSIZE; temp[i++] = (u8)flag) {
    ;
  }
  ret =
      __osContRamWrite(pfs->queue, pfs->channel, CONT_BLOCK_GB_STATUS, temp, 0);

  /* A "new pack" reply means the pak was swapped: re-init from scratch and, if
   * that succeeds, reissue the power write. */
  if (ret == PFS_ERR_NEW_PACK) {
    ret = osGbpakInit(pfs->queue, pfs, pfs->channel);
    if (ret == 0) {
      ret = __osContRamWrite(pfs->queue, pfs->channel, CONT_BLOCK_GB_STATUS,
                             temp, 0);
      if (ret == PFS_ERR_NEW_PACK) {
        ret = PFS_ERR_CONTRFAIL;
      }
    }
  }

  /* After powering on, give the cartridge time to come up before returning. */
  if (flag != OS_GBPAK_POWER_OFF) {
    osSetTimer(&__osGbpakTimer, OS_USEC_TO_CYCLES(120000), 0, &__osGbpakTimerQ,
               &__osGbpakTimerMsg);
    osRecvMesg(&__osGbpakTimerQ, NULL, OS_MESG_BLOCK);
  }
  return ret;
}
