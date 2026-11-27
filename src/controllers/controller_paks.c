/*
 * The controller paks: finding Transfer Paks with Game Boy cartridges, the Game
 * Boy characters on them, and starting and stopping the Rumble Paks.
 */

#include <nusys.h>
#include <ultra64.h>

#include "controllers/controller.h"
#include "controllers/transfer_pak.h"
#include "system/c_language.h"
#include "system/compiler.h"

Boolean g_HasBroughtOverCharacter = FALSE;
// Set until the boot logos, finding a Game Boy Pak, have once gone to the Game
// Boy Pak transfer module
static Boolean f_IsBootTransmitPending = TRUE;
static s32 f_UnusedWordsAfterBootTransmitPending[2] UNUSED = {0};

GameBoyCharacter g_GameBoyCharacters[GAME_BOY_CHARACTER_COUNT];
GameBoyTransfer g_GameBoyTransfer;

FILE_STATIC Boolean f_HasRumblePak[MAXCONTROLLERS];

// Adds experience that a Game Boy character earned, up to what the Game Boy
// Color version holds
void AddGameBoyExperience(s32 gameBoyCharacterIndex, s32 experience) {
    g_GameBoyCharacters[gameBoyCharacterIndex].experience += experience;
    if (g_GameBoyCharacters[gameBoyCharacterIndex].experience > GAME_BOY_EXPERIENCE_MAX) {
        g_GameBoyCharacters[gameBoyCharacterIndex].experience = GAME_BOY_EXPERIENCE_MAX;
    }
}

Boolean HasGameBoyPak(void) {
    return g_GameBoyTransfer.hasGameBoyPak;
}

Boolean IsGameBoyCharacterPresent(s32 gameBoyCharacterIndex) {
    return g_GameBoyCharacters[gameBoyCharacterIndex].status == GAME_BOY_CHARACTER_STATUS_PRESENT;
}

char *GetGameBoyCharacterName(s32 gameBoyCharacterIndex) {
    return g_GameBoyCharacters[gameBoyCharacterIndex].name;
}

// Records the transfer mode; mode 0 also empties every Game Boy character slot
void ResetGameBoyTransfer(TransferMode transferMode) {
    GameBoyTransfer *transfer = &g_GameBoyTransfer;
    s32 index;

    transfer->transferMode = transferMode;
    g_GameBoyTransfer.screen = TRANSMIT_SCREEN_OPTIONS;
    if (transferMode == TRANSFER_MODE_AT_BOOT) {
        for (index = 0; index != GAME_BOY_CHARACTER_COUNT; index++) {
            g_GameBoyCharacters[index].status = GAME_BOY_CHARACTER_STATUS_EMPTY;
            g_GameBoyCharacters[index].companyCode = -1;
            g_GameBoyCharacters[index].saveIndex = -1;
            g_GameBoyTransfer.sourceIndices[index] = -1;
        }

        g_GameBoyTransfer.optionIndex = 0;
        g_GameBoyTransfer.listedCharacterIndex = 0;
        g_GameBoyTransfer.characterCount = 0;
    }
    else {
        g_GameBoyTransfer.optionIndex = 0;
        g_GameBoyTransfer.listedCharacterIndex = 0;
    }
}

void ClearBootTransmitPending(void) {
    f_IsBootTransmitPending = FALSE;
}

Boolean IsBootTransmitPending(void) {
    return f_IsBootTransmitPending;
}

// Sets hasGameBoyPak if any controller holds a Transfer Pak with a Game Boy
// cartridge in it
void DetectGameBoyPaks(void) {
    NUContPakFile file;
    s32 index;

    nuContGBPakMgrInit();
    g_GameBoyTransfer.hasGameBoyPak = FALSE;
    for (index = 0; index < g_ControllerCount; index++) {
        switch (nuContGBPakOpen(&file, g_PlayerControllerPorts[index])) {
            case PAK_ACCESS_OK:
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
        }
        else if (result == PFS_ERR_DEVICE) {
            osSyncPrintf("PFS_ERR_DEVICE\n");
            f_HasRumblePak[index] = FALSE;
        }
        else if (result == PFS_ERR_CONTRFAIL) {
            osSyncPrintf("PFS_ERR_CONTRFAIL\n");
            f_HasRumblePak[index] = FALSE;
        }
        else {
            f_HasRumblePak[index] = TRUE;
        }
    }

    DetectGameBoyPaks();
}

void StopAllRumble(void) {
    nuContRmbForceStop();
}

// StopAllRumble, logged
void ForceStopRumble(void) {
    nuContRmbForceStop();
    osSyncPrintf("ReqForceStop\n");
}

// Never called; references the Game Boy Pak functions, perhaps so that they are
// linked
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
