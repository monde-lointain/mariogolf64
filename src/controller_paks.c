/*
 * The controller paks: finding Transfer Paks with Game Boy cartridges, the Game
 * Boy characters on them, and starting and stopping the Rumble Paks.
 */
#include "common.h"

#include <lib/libnusys/nusys.h>

#include "controller.h"
#include "transfer_pak.h"

/* Set until the boot logos, finding a Game Boy Pak, have once gone to the Game
 * Boy Pak transfer module */
extern s32 f_IsBootTransmitPending;
extern s32 f_HasRumblePak[MAXCONTROLLERS];

/* Adds experience that a Game Boy character earned, up to what the Game Boy
 * Color version holds */
void AddGameBoyExperience(s32 characterIndex, s32 experience) {
  g_GameBoyCharacters[characterIndex].experience += experience;
  if (g_GameBoyCharacters[characterIndex].experience >
      MAX_GAME_BOY_EXPERIENCE) {
    g_GameBoyCharacters[characterIndex].experience = MAX_GAME_BOY_EXPERIENCE;
  }
}

s32 HasGameBoyPak(void) {
  return g_GameBoyTransfer.hasGameBoyPak;
}

s32 IsGameBoyCharacterPresent(s32 characterIndex) {
  return g_GameBoyCharacters[characterIndex].status ==
      GAME_BOY_CHARACTER_STATUS_PRESENT;
}

u8* GetGameBoyCharacterName(s32 characterIndex) {
  return g_GameBoyCharacters[characterIndex].name;
}

/* Records the transfer mode; mode 0 also empties every Game Boy character slot
 */
#ifdef NON_MATCHING
void ResetGameBoyTransfer(s32 transferMode) {
  s32 index;

  g_GameBoyTransfer.transferMode = transferMode;
  g_GameBoyTransfer.screen = TRANSMIT_SCREEN_OPTIONS;
  if (transferMode == TRANSFER_MODE_AT_BOOT) {
    for (index = 0; index != GAME_BOY_CHARACTER_COUNT; index++) {
      g_GameBoyCharacters[index].status = 0;
      g_GameBoyCharacters[index].unk34 = -1;
      g_GameBoyCharacters[index].unk38 = -1;
      g_GameBoyTransfer.sourceIndices[index] = -1;
    }
    g_GameBoyTransfer.optionIndex = 0;
    g_GameBoyTransfer.listedCharacterIndex = 0;
    g_GameBoyTransfer.characterCount = 0;
  } else {
    g_GameBoyTransfer.optionIndex = 0;
    g_GameBoyTransfer.listedCharacterIndex = 0;
  }
}

#else
INCLUDE_ASM("asm/nonmatchings/controller_paks", ResetGameBoyTransfer);
#endif

void ClearBootTransmitPending(void) {
  f_IsBootTransmitPending = FALSE;
}

s32 IsBootTransmitPending(void) {
  return f_IsBootTransmitPending;
}

/* Sets hasGameBoyPak if any controller holds a Transfer Pak with a Game Boy
 * cartridge in it */
void DetectGameBoyPaks(void) {
  NUContPakFile file;
  s32 index;

  nuContGBPakMgrInit();
  g_GameBoyTransfer.hasGameBoyPak = FALSE;
  for (index = 0; index < g_ControllerCount; index++) {
    switch (nuContGBPakOpen(&file, g_PlayerControllerPorts[index])) {
      case 0:
        osSyncPrintf("GB PACK OK %d\n", index);
        g_GameBoyTransfer.hasGameBoyPak = TRUE;
        break;
      case PFS_ERR_NOPACK:
        osSyncPrintf("GB NOPACK %d\n", index);
        break;
      case PFS_ERR_DEVICE:
        osSyncPrintf("GB DEVICE ER %d\n", index);
        break;
      default:
        osSyncPrintf("GB DEFAULT ERROR %d\n", index);
        break;
    }
  }
  nuContGBPakMgrRemove();
}

void InitRumblePaks(void) {
  s32 index;
  s32 result;

  for (index = 0; index != g_ControllerCount; index++) {
    nuContRmbModeSet(g_PlayerControllerPorts[index], NU_CONT_RMB_MODE_AUTORUN);
    result = nuContRmbCheck(g_PlayerControllerPorts[index]);
    if (result == PFS_ERR_NOPACK) {
      osSyncPrintf("PFS_ERR_NOPACK\n");
      f_HasRumblePak[index] = FALSE;
    } else if (result == PFS_ERR_DEVICE) {
      osSyncPrintf("PFS_ERR_DEVICE\n");
      f_HasRumblePak[index] = FALSE;
    } else if (result == PFS_ERR_CONTRFAIL) {
      osSyncPrintf("PFS_ERR_CONTRFAIL\n");
      f_HasRumblePak[index] = FALSE;
    } else {
      f_HasRumblePak[index] = TRUE;
    }
  }
  DetectGameBoyPaks();
}

void StopAllRumble(void) {
  nuContRmbForceStop();
}

/* StopAllRumble, logged */
void ForceStopRumble(void) {
  nuContRmbForceStop();
  osSyncPrintf("ReqForceStop\n");
}

/* Never called; references the Game Boy Pak functions, perhaps so that they are
 * linked */
void ReferenceGameBoyPakFunctions(void) {
  nuContGBPakMgrInit();
  nuContGBPakGetStatus(NULL, NULL);
  nuContGBPakPower(NULL, OS_GBPAK_POWER_ON);
  nuContGBPakReadWrite(NULL, OS_WRITE, 0, NULL, 0x20);
  nuContGBPakCheckConnector(NULL, NULL);
  nuContGBPakReadID(NULL, NULL, NULL);
  nuContGBPakReadWrite(NULL, OS_READ, 0, NULL, 0);
  nuContGBPakFread(NULL, 0, NULL, 0);
  nuContGBPakFwrite(NULL, 0, NULL, 0);
}
