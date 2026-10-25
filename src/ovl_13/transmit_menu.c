#include "common.h"
#include "asset.h"
#include "character.h"
#include "controller.h"
#include "debug_text.h"
#include "game_flags.h"
#include "heap.h"
#include "host_file.h"
#include "save.h"
#include "screen.h"
#include "screen_layers.h"
#include "sound.h"
#include "text.h"
#include "transfer_pak.h"

/* The options of each transfer mode */
#define TRANSFER_MODE_COUNT 2
#define MODE_OPTION_COUNT 3

/* sModeOptions entries, counted from the message naming each */
#define TRANSMIT_OPTION_MESSAGES 0x13F
#define TRANSMIT_OPTION_NONE -1
#define TRANSMIT_OPTION_STATUS 0
#define TRANSMIT_OPTION_EXPERIENCE 1
#define TRANSMIT_OPTION_CHARACTERS 2
#define TRANSMIT_OPTION_MAIN_MENU 3

/* sNoticeState: the notice cloud grows, shows its message, then shrinks */
#define NOTICE_HIDDEN 0
#define NOTICE_OPENING 1
#define NOTICE_CLOSING 2
#define NOTICE_SHOWN 3
/* The notices */
#define INSERT_TRANSFER_PAK_MESSAGE 0x149
#define SWAP_FOR_TRANSFER_PAK_MESSAGE 0x148
#define LOOSE_TRANSFER_PAK_MESSAGE 0x14A
#define NO_CARTRIDGE_MESSAGE 0x14C
#define WRONG_CARTRIDGE_MESSAGE 0x14D
#define CARTRIDGE_REMOVED_MESSAGE 0x14E
#define RETRYING_AFTER_REMOVAL_MESSAGE 0x14F
#define CONNECTOR_PROBLEM_MESSAGE 0x151
#define CORRUPTED_SAVE_MESSAGE 0x152
#define UNINITIALIZED_SAVE_MESSAGE 0x153
#define CARTRIDGE_REPLACED_MESSAGE 0x156
#define CHOOSE_EXPERIENCE_MESSAGE 0x158
#define EXPERIENCE_SENT_MESSAGE 0x159
#define CHOOSE_CHARACTER_MESSAGE 0x15C
#define CHARACTER_BROUGHT_MESSAGE 0x15D
#define CHOOSE_SWAPPED_CHARACTER_MESSAGE 0x15E
#define NO_CHARACTERS_MESSAGE 0x15F

/* The hints below the status screen */
#define CHOOSE_CARTRIDGE_MESSAGE 0x160
#define KEEP_POWER_ON_MESSAGE 0x161
#define SEND_STATUS_MESSAGE 0x162
#define HINT_LEFT 28
#define HINT_TOP 186
#define HINT_TEXT_SLOT 3

/* The faces of the characters brought over, along the bottom */
#define FACES_LEFT 24
#define FACES_TOP 136
#define FACE_SPACING 40
#define FACE_SIZE 32
#define FACE_CURSOR_MARGIN 9
#define CONTROLLER_ICONS_LEFT 60
#define CONTROLLER_ICONS_TOP 72
#define CONTROLLER_ICON_SPACING 56
#define CONTROLLER_ICON_SIZE 32
#define EXPERIENCE_LEFT 20 /* Below each face */
#define EXPERIENCE_TOP 174

/* The notice cloud grows by one step each time it is drawn, up to this size */
#define NOTICE_FULL_SIZE 4

/* The Accessing sign, drawn in strips */
#define ACCESSING_LEFT 52
#define ACCESSING_TOP 64
#define ACCESSING_WIDTH 216
#define ACCESSING_STRIP_HEIGHT 9
#define ACCESSING_STRIP_COUNT 10
#define ACCESSING_STRIP_SIZE (ACCESSING_WIDTH * ACCESSING_STRIP_HEIGHT)

/* pressingHandPosition values: the hand pressing the A button, while the menu waits for it */
#define PRESSING_HAND_HIDDEN 0
#define PRESSING_HAND_LOW 1
#define PRESSING_HAND_FOR_NOTICE 2
#define LOW_PRESSING_HAND_LEFT 272
#define LOW_PRESSING_HAND_TOP 192
#define NOTICE_PRESSING_HAND_LEFT 256
#define NOTICE_PRESSING_HAND_TOP 128
/* The hand's three images each show for three draws */
#define PRESSING_HAND_FRAMES_PER_IMAGE 3
#define PRESSING_HAND_FRAME_COUNT (3 * PRESSING_HAND_FRAMES_PER_IMAGE)
#define PRESSING_HAND_SIZE 24
#define PRESSING_HAND_IMAGE_SIZE (PRESSING_HAND_SIZE * PRESSING_HAND_SIZE)

/* Where the Game Boy cartridge's RAM appears through the Transfer Pak */
#define CARTRIDGE_RAM_ADDRESS 0xA000
#define CARTRIDGE_RAM_SIZE 0x2000

/* The largest value the status sends for each figure */
#define MAX_STATUS_VALUE 999

/* A Game Boy save in the cartridge RAM */
#define CARTRIDGE_SAVE_SIZE 0x200
#define SAVE_CHECKSUM_OFFSET 0x30
#define SAVE_UNKB_OFFSET 0xB
#define SAVE_CHARACTER_OFFSET 0x10
/* The save's 16-bit values are little-endian */
#define SAVE_HALFWORD_SIZE 2
#define SAVE_HAS_STATUS_OFFSET 0xA0
#define SAVE_STATUS_ADDRESS_OFFSET 0xA2
#define SAVE_STATUS_SIZE_OFFSET 0xA4
#define SAVE_STATUS_SUM_OFFSET 0xA6
#define STATUS_ADDRESS_BIAS 0x6000
#define STATUS_DATA_SIZE 0x800

#define SAVE_SIGNATURE_OFFSET 0x20

/* What CheckStatusCartridge found */
#define STATUS_CHECK_NO_STATUS 0
#define STATUS_CHECK_FAILED 1 /* With the cause in transferResult */
#define STATUS_CHECK_HAS_STATUS 2
#define STATUS_CHECK_READ_FAILED 3 /* With the error in transferResult */

/* What WriteStatusToCartridge did */
#define STATUS_WRITE_DONE 0
#define STATUS_WRITE_PAK_ERROR 1 /* With the error in transferResult */
#define STATUS_WRITE_FAILED 2 /* With the cause in transferResult */

#define WOOD_PANEL_ASSET 0x8FD
#define COURSE_SCENE_ASSET 0x51E
#define POINTER_ASSET 0x70F
#define ARROWS_ASSET 0x720
#define CONTROLLER_ICONS_ASSET 0x900
#define TITLE_ASSET 0x93F
#define CURSOR_BOX_ASSET 0x5F7
#define UNK8EE_ASSET 0x8EE
#define PRESSING_HAND_ASSET 0x8EB
#define STAT_BARS_ASSET 0x95D
#define SHOT_TYPES_ASSET 0x979
#define NOTICE_CLOUD_ASSET 0x70B
#define ACCESSING_ASSET 0x96B
#define STAT_LABELS_ASSET 0x978

#define FIRST_GAME_BOY_GOLFER CHARACTER_KID
#define TRANSMIT_MENU_SONG 0x16

void LoadCursorBox(void *image);

/* Results of opening and accessing the Game Boy cartridges, after the notice each shows */
#define PAK_RESULT_CONNECTOR_PROBLEM 0
#define PAK_RESULT_OK 1
#define PAK_RESULT_NO_CHARACTERS 2
#define PAK_RESULT_NO_PAK 3
#define PAK_RESULT_NO_CARTRIDGE 4
#define PAK_RESULT_WRONG_CARTRIDGE 5
#define PAK_RESULT_OTHER_PAK 6
#define PAK_RESULT_LOOSE_PAK 7
#define PAK_RESULT_REMOVED_RETRYING 8
#define PAK_RESULT_CARTRIDGE_REMOVED 9
#define PAK_RESULT_CORRUPTED 10
#define PAK_RESULT_CARTRIDGE_REPLACED 11
#define PAK_RESULT_UNINITIALIZED 12
#define GAME_BOY_PAK_OPEN_ATTEMPTS 3

/*
 * g_GameBoyTransfer.pressingHandImage under a scalar name. Reading it as a scalar is there to match:
 * GCC keeps a struct field's constant address in a register when it is used twice.
 */
extern void *g_PressingHandImage;

void DrawCharacterStats(Gfx **displayListHead, GameBoyCharacter *character, GameBoyTransfer *transfer);
void DrawImportedFaces(Gfx **displayListHead, GameBoyTransfer *transfer);
void DrawCursorBox(Gfx **displayListHead, s32 left, s32 top);
s32 OpenGameBoyPak(s32 port, u8 *header, NUContPakFile *file);
s32 OpenGameBoyPakWithRetries(s32 port, u8 *header, NUContPakFile *file);

/* Debug code that writes the cartridge RAM of controller 1's Game Boy cartridge to the host */
void DumpCartridgeRam(void) {
    /* The transfer pointer is there to match */
    GameBoyTransfer *transfer = &g_GameBoyTransfer;
    u8 cartridgeRam[CARTRIDGE_RAM_SIZE];

    nuContGBPakMgrInit();
    PrintDebugText("          ", 10, 3);
    PrintDebugText("          ", 10, 4);
    PrintDebugText("          ", 10, 5);
    /* The original takes 0 from OpenGameBoyPakWithRetries as success, which is not PAK_RESULT_OK */
    if (OpenGameBoyPakWithRetries(0, transfer->cartridgeHeader, &transfer->pakFile) == 0) {
        PrintDebugText("GOLF OK", 10, 3);
        if (nuContGBPakFread(&transfer->pakFile, CARTRIDGE_RAM_ADDRESS, cartridgeRam,
                             CARTRIDGE_RAM_SIZE) == 0) {
            PrintDebugText("READ OK", 10, 4);
            if (WriteHostFile("x:/golf/capture/gbsram.bin", CARTRIDGE_RAM_SIZE) == 0) {
                PrintDebugText("WRITE OK", 10, 5);
            }
        }
    }
}

/*
 * Loads the menu's images, and the faces and palettes of the Game Boy golfers, then starts the
 * menu's song and stops any rumble.
 */
void LoadTransmitMenu(void) {
    AssetFile file;
    EncodedAsset asset;
    s32 index;
    s32 size;

    ReadEncodedAsset(g_GameBoyTransfer.woodPanelImage = AllocMemory(OpenEncodedAsset(WOOD_PANEL_ASSET, &asset)),
                     &asset);
    ReadEncodedAsset(g_GameBoyTransfer.courseSceneImage = AllocMemory(OpenEncodedAsset(COURSE_SCENE_ASSET, &asset)),
                     &asset);
    ReadEncodedAsset(g_GameBoyTransfer.pointerImage = AllocMemory(OpenEncodedAsset(POINTER_ASSET, &asset)), &asset);
    ReadEncodedAsset(g_GameBoyTransfer.arrowsImage = AllocMemory(OpenEncodedAsset(ARROWS_ASSET, &asset)), &asset);
    ReadEncodedAsset(g_GameBoyTransfer.controllerIconsImage =
                         AllocMemory(OpenEncodedAsset(CONTROLLER_ICONS_ASSET, &asset)),
                     &asset);
    ReadEncodedAsset(g_GameBoyTransfer.titleImage = AllocMemory(OpenEncodedAsset(TITLE_ASSET, &asset)), &asset);
    ReadEncodedAsset(g_GameBoyTransfer.cursorBoxImage = AllocMemory(OpenEncodedAsset(CURSOR_BOX_ASSET, &asset)),
                     &asset);
    ReadEncodedAsset(g_GameBoyTransfer.unk3C = AllocMemory(OpenEncodedAsset(UNK8EE_ASSET, &asset)), &asset);
    ReadEncodedAsset(g_GameBoyTransfer.pressingHandImage = AllocMemory(OpenEncodedAsset(PRESSING_HAND_ASSET, &asset)),
                     &asset);
    ReadEncodedAsset(g_GameBoyTransfer.statBarsImage = AllocMemory(OpenEncodedAsset(STAT_BARS_ASSET, &asset)),
                     &asset);
    ReadEncodedAsset(g_GameBoyTransfer.shotTypesImage = AllocMemory(OpenEncodedAsset(SHOT_TYPES_ASSET, &asset)),
                     &asset);
    ReadEncodedAsset(g_GameBoyTransfer.noticeCloudImage = AllocMemory(OpenEncodedAsset(NOTICE_CLOUD_ASSET, &asset)),
                     &asset);
    ReadEncodedAsset(g_GameBoyTransfer.accessingImage = AllocMemory(OpenEncodedAsset(ACCESSING_ASSET, &asset)),
                     &asset);
    ReadEncodedAsset(g_GameBoyTransfer.statLabelsImage = AllocMemory(OpenEncodedAsset(STAT_LABELS_ASSET, &asset)),
                     &asset);
    g_GameBoyTransfer.pressingHandPosition = PRESSING_HAND_HIDDEN;
    g_GameBoyTransfer.isAccessing = FALSE;
    for (index = 0; index != GAME_BOY_GOLFER_COUNT; index++) {
        ReadEncodedAsset(g_GameBoyTransfer.faceImages[index] = AllocMemory(OpenEncodedAsset(
                             g_CharacterImageAssets[FIRST_GAME_BOY_GOLFER + index], &asset)),
                         &asset);
        OpenAssetFile(g_CharacterPaletteAssets[FIRST_GAME_BOY_GOLFER + index], &file);
        size = GetAssetFileSize(&file);
        ReadAssetFile(g_GameBoyTransfer.facePalettes[index] = AllocMemory(size), size, &file);
    }
    PlaySong(TRANSMIT_MENU_SONG, 0);
    LoadCursorBox(g_GameBoyTransfer.cursorBoxImage);
    nuContRmbForceStop();
}

void FreeTransmitMenu(void) {
    s32 index;

    FreeMemory(&g_GameBoyTransfer.woodPanelImage);
    FreeMemory(&g_GameBoyTransfer.courseSceneImage);
    FreeMemory(&g_GameBoyTransfer.pointerImage);
    FreeMemory(&g_GameBoyTransfer.arrowsImage);
    FreeMemory(&g_GameBoyTransfer.controllerIconsImage);
    FreeMemory(&g_GameBoyTransfer.titleImage);
    FreeMemory(&g_GameBoyTransfer.cursorBoxImage);
    FreeMemory(&g_GameBoyTransfer.unk3C);
    FreeMemory(&g_GameBoyTransfer.pressingHandImage);
    FreeMemory(&g_GameBoyTransfer.statBarsImage);
    FreeMemory(&g_GameBoyTransfer.shotTypesImage);
    FreeMemory(&g_GameBoyTransfer.statLabelsImage);
    FreeMemory(&g_GameBoyTransfer.noticeCloudImage);
    FreeMemory(&g_GameBoyTransfer.accessingImage);
    for (index = 0; index != GAME_BOY_CHARACTER_COUNT; index++) {
        FreeMemory(&g_GameBoyTransfer.faceImages[index]);
        FreeMemory(&g_GameBoyTransfer.facePalettes[index]);
    }
}

INCLUDE_ASM("asm/nonmatchings/ovl_13/transmit_menu", UpdateBringCharacters);

void ShowPakError(s32 pfsError) {
    switch (pfsError) {
        case PFS_ERR_NOPACK:
            SetNoticeState(NOTICE_OPENING, INSERT_TRANSFER_PAK_MESSAGE);
            break;
        case PFS_ERR_DEVICE:
            SetNoticeState(NOTICE_OPENING, SWAP_FOR_TRANSFER_PAK_MESSAGE);
            break;
        case PFS_ERR_CONTRFAIL:
            SetNoticeState(NOTICE_OPENING, CONNECTOR_PROBLEM_MESSAGE);
            break;
        case PFS_ERR_NO_GBCART:
            SetNoticeState(NOTICE_OPENING, NO_CARTRIDGE_MESSAGE);
            break;
        case PFS_ERR_NEW_GBCART:
            SetNoticeState(NOTICE_OPENING, RETRYING_AFTER_REMOVAL_MESSAGE);
            break;
    }
}

void ShowTransferError(u32 result) {
    g_GameBoyTransfer.pressingHandPosition = PRESSING_HAND_FOR_NOTICE;
    switch (result) {
        case PAK_RESULT_NO_CHARACTERS:
            SetNoticeState(NOTICE_OPENING, NO_CHARACTERS_MESSAGE);
            break;
        case PAK_RESULT_NO_PAK:
            SetNoticeState(NOTICE_OPENING, INSERT_TRANSFER_PAK_MESSAGE);
            break;
        case PAK_RESULT_NO_CARTRIDGE:
            SetNoticeState(NOTICE_OPENING, NO_CARTRIDGE_MESSAGE);
            break;
        case PAK_RESULT_WRONG_CARTRIDGE:
            SetNoticeState(NOTICE_OPENING, WRONG_CARTRIDGE_MESSAGE);
            break;
        case PAK_RESULT_OTHER_PAK:
            SetNoticeState(NOTICE_OPENING, SWAP_FOR_TRANSFER_PAK_MESSAGE);
            break;
        case PAK_RESULT_LOOSE_PAK:
            SetNoticeState(NOTICE_OPENING, LOOSE_TRANSFER_PAK_MESSAGE);
            break;
        case PAK_RESULT_REMOVED_RETRYING:
            SetNoticeState(NOTICE_OPENING, RETRYING_AFTER_REMOVAL_MESSAGE);
            break;
        case PAK_RESULT_CARTRIDGE_REMOVED:
            SetNoticeState(NOTICE_OPENING, CARTRIDGE_REMOVED_MESSAGE);
            break;
        case PAK_RESULT_CONNECTOR_PROBLEM:
            SetNoticeState(NOTICE_OPENING, CONNECTOR_PROBLEM_MESSAGE);
            break;
        case PAK_RESULT_CORRUPTED:
            SetNoticeState(NOTICE_OPENING, CORRUPTED_SAVE_MESSAGE);
            break;
        case PAK_RESULT_CARTRIDGE_REPLACED:
            SetNoticeState(NOTICE_OPENING, CARTRIDGE_REPLACED_MESSAGE);
            break;
        case PAK_RESULT_UNINITIALIZED:
            SetNoticeState(NOTICE_OPENING, UNINITIALIZED_SAVE_MESSAGE);
            break;
    }
}

INCLUDE_ASM("asm/nonmatchings/ovl_13/transmit_menu", UpdateSendStatus);

INCLUDE_ASM("asm/nonmatchings/ovl_13/transmit_menu", UpdateSendExperience);

/* Whether every playing controller holds a Rumble Pak */
s32 HaveAllRumblePaks(void) {
    s32 haveAll = TRUE;
    s32 index;

    for (index = 0; index != g_ControllerCount; index++) {
        if (nuContRmbCheck(g_PlayerControllerPorts[index]) != 0) {
            haveAll = FALSE;
        }
    }
    return haveAll;
}

INCLUDE_ASM("asm/nonmatchings/ovl_13/transmit_menu", UpdateTransmitMenu);

/* Fills the slot for a cartridge's save from the save's bytes */
void DecodeCartridgeCharacter(u8 *save, s32 cartridgeIndex, s32 saveIndex, s32 unk34) {
    GameBoyCharacter *character =
        &g_GameBoyTransfer.cartridgeCharacters[cartridgeIndex * CARTRIDGE_SAVE_COUNT + saveIndex];
    s32 index;

    character->status = GAME_BOY_CHARACTER_STATUS_PRESENT;
    for (index = 0; index != (SAVE_NAME_SIZE - 1); index++) {
        character->name[index] = save[index];
        character->saveName[index] = save[index];
        if (character->name[index] >= 'a' && character->name[index] <= 'z') {
            character->name[index] += LOWERCASE_CODE_OFFSET;
        }
    }
    character->name[(SAVE_NAME_SIZE - 1)] = '\0';
    character->saveName[(SAVE_NAME_SIZE - 1)] = '\0';
    for (index = 0; index != SAVE_UNKB_SIZE; index++) {
        character->saveUnkB[index] = save[SAVE_UNKB_OFFSET + index];
    }
    character->character = save[SAVE_CHARACTER_OFFSET];
    if (character->character >= GAME_BOY_GOLFER_COUNT) {
        character->character = GAME_BOY_GOLFER_COUNT - 1;
    }
    if (IsGameFlagSet(GAME_FLAG_UNK6F)) {
        character->unk1A = 1;
    } else {
        character->unk1A = save[0x11];
    }
    character->unk1B = save[0x12];
    character->unk1C = save[0x1C];
    character->unk1D = save[0x1D];
    character->unk1E = save[0x1E];
    character->unk1F = save[0x1F];
    if (IsGameFlagSet(GAME_FLAG_BALL_TYPE_1)) {
        character->ballType = 1;
    } else if (IsGameFlagSet(GAME_FLAG_BALL_TYPE_2)) {
        character->ballType = 2;
    } else if (IsGameFlagSet(GAME_FLAG_BALL_TYPE_3)) {
        character->ballType = 3;
    } else if (IsGameFlagSet(GAME_FLAG_BALL_TYPE_4)) {
        character->ballType = 4;
    } else if (IsGameFlagSet(GAME_FLAG_BALL_TYPE_5)) {
        character->ballType = 5;
    } else if (IsGameFlagSet(GAME_FLAG_BALL_TYPE_6)) {
        character->ballType = 6;
    } else if (IsGameFlagSet(GAME_FLAG_BALL_TYPE_7)) {
        character->ballType = 7;
    } else {
        character->ballType = save[0x3E];
    }
    osSyncPrintf("balltype %d\n", character->ballType);
    character->unk24 = (save[0x28] << 16) | (save[0x27] << 8) | save[0x26];
    character->experience = 0;
    character->unk2C = save[0x1A] | (save[0x1B] << 8);
    osSyncPrintf("%d, %d %d\n", character->experience, character->unk1C, character->unk1D);
    for (index = 0; index != GAME_BOY_CHARACTER_UNK3C_COUNT; index++) {
        character->unk3C[index].unk0 = save[0x100 + index * 6] | (save[0x101 + index * 6] << 8);
        character->unk3C[index].unk2 = save[0x102 + index * 6] | (save[0x103 + index * 6] << 8);
        character->unk3C[index].unk4 = save[0x104 + index * 6];
        character->unk3C[index].unk5 = save[0x105 + index * 6];
        osSyncPrintf("%d, %d %d\n", index, character->unk3C[index].unk0, character->unk3C[index].unk2);
    }
    character->unk34 = unk34;
    character->unk38 = saveIndex;
}

void SkipCartridgeSave(void) {
}

s32 IsCharacterFromSave(GameBoyCharacter *character, u8 *save) {
    s32 index;

    osSyncPrintf("PHASE a\n");
    for (index = 0; index != SAVE_NAME_SIZE; index++) {
        if (character->saveName[index] != save[index]) {
            if (FALSE) {
                osSyncPrintf("PHASE b\n");
            }
            return FALSE;
        }
    }
    osSyncPrintf("PHASE c\n");
    for (index = 0; index != SAVE_UNKB_SIZE; index++) {
        if (character->saveUnkB[index] != save[SAVE_UNKB_OFFSET + index]) {
            osSyncPrintf("PHASE d\n");
            return FALSE;
        }
    }
    osSyncPrintf("PHASE e\n");
    return TRUE;
}

/* The menu's options, each the offset of its name from TRANSMIT_OPTION_MESSAGES */
s32 sModeOptions[TRANSFER_MODE_COUNT][MODE_OPTION_COUNT] = {
    { TRANSMIT_OPTION_STATUS, TRANSMIT_OPTION_CHARACTERS, TRANSMIT_OPTION_MAIN_MENU },
    { TRANSMIT_OPTION_STATUS, TRANSMIT_OPTION_EXPERIENCE, TRANSMIT_OPTION_NONE },
};
char *sUnusedShotTypeNames[] = { "Straight", "Draw", "Fade" };
char sUnusedDriveName[8] = "Drive";
f32 sStatusPointerAngle = 0.0f;
s32 sCharacterArrowFrame = 0;
f32 sOptionPointerAngle = 0.0f;
/* The title in the Game Boy cartridge header, then its game code */
char sGameBoyCartridgeTitle[16] = "MARIO GOLF\0AWXE";
/* Begins every save in the Game Boy cartridge RAM */
char sCartridgeSaveSignature[16] = "CAMELOT GAMEBOY";
s32 sNoticeState = NOTICE_HIDDEN;
s32 sNoticeMessage = 0;
s32 sNoticeSize = 0;
s32 sPressingHandFrame = 0;
s32 sUnusedZeros[2] = { 0, 0 };
s32 sStatusHoleOrder[COURSE_HOLE_COUNT] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17 };

void BringOverCharacter(s32 listedIndex, s32 slot) {
    /* The transfer pointer is there to match */
    GameBoyTransfer *transfer = &g_GameBoyTransfer;

    g_GameBoyCharacters[slot] = transfer->cartridgeCharacters[transfer->listedCharacters[listedIndex]];
    g_GameBoyCharacters[slot].status = GAME_BOY_CHARACTER_STATUS_PRESENT;
    transfer->cartridgeCharacters[transfer->listedCharacters[listedIndex]].status =
        GAME_BOY_CHARACTER_STATUS_BROUGHT_OVER;
    transfer->sourceIndices[slot] = transfer->listedCharacters[listedIndex];
}

void ResetBringCharacters(void) {
    g_GameBoyTransfer.listedCharacterIndex = 0;
    g_GameBoyTransfer.characterIndex = 0;
    g_GameBoyTransfer.unk7B8 = 0;
    g_GameBoyTransfer.isCancelled = FALSE;
}

void ResetSendStatus(void) {
    g_GameBoyTransfer.chosenCartridgeIndex = 0;
    g_GameBoyTransfer.taggedCartridgeCount = 0;
    g_GameBoyTransfer.isCancelled = FALSE;
}

void ResetSendExperience(void) {
    g_GameBoyTransfer.chosenCartridgeIndex = 0;
    g_GameBoyTransfer.characterIndex = 0;
    g_GameBoyTransfer.taggedCartridgeCount = 0;
    g_GameBoyTransfer.isCancelled = FALSE;
}

s32 GetStoredChecksum(u8 *save) {
    return save[SAVE_CHECKSUM_OFFSET] | (save[SAVE_CHECKSUM_OFFSET + 1] << 8);
}

/* Sums the save's bytes after its checksum */
s32 ComputeSaveChecksum(u8 *save) {
    s32 sum = 0;
    s32 offset;

    for (offset = SAVE_CHECKSUM_OFFSET + 2; offset != CARTRIDGE_SAVE_SIZE; offset++) {
        sum += save[offset];
    }
    return sum;
}

void ListPresentCharacters(void) {
    /* The transfer pointer is there to match */
    GameBoyTransfer *transfer = &g_GameBoyTransfer;
    s32 listedCount = 0;
    s32 index;

    for (index = 0; index != CARTRIDGE_CHARACTER_COUNT; index++) {
        if (transfer->cartridgeCharacters[index].status == GAME_BOY_CHARACTER_STATUS_PRESENT) {
            transfer->listedCharacters[listedCount++] = index;
        }
    }
    for (; listedCount != CARTRIDGE_CHARACTER_COUNT; listedCount++) {
        transfer->listedCharacters[listedCount] = -1;
    }
}

void DrawSendStatusHint(Gfx **displayListHead, GameBoyTransfer *transfer) {
    Gfx *displayList = *displayListHead;
    s32 message = NO_MESSAGE;

    switch (transfer->screenStep) {
        case 1:
            message = CHOOSE_CARTRIDGE_MESSAGE;
            break;
        case 5:
            message = SEND_STATUS_MESSAGE;
            break;
        case 3:
            message = KEEP_POWER_ON_MESSAGE;
            break;
    }
    if (message != NO_MESSAGE) {
        SetTextSlot(HINT_LEFT, HINT_TOP, message, 1, HINT_TEXT_SLOT, TEXT_FLAG_UNK20, 255, 255, 255, 255);
        DrawLargeTextSlot(&displayList, HINT_TEXT_SLOT);
    }
    *displayListHead = displayList;
}

INCLUDE_ASM("asm/nonmatchings/ovl_13/transmit_menu", DrawCharacterStats);

void DrawBringCharactersText(Gfx **displayListHead, GameBoyTransfer *transfer) {
    GameBoyCharacter *character = g_GameBoyCharacters;
    Gfx *displayList = *displayListHead;
    s32 isCharacterShown = FALSE;
    s32 message;

    SetUpTextDrawing(&displayList);
    gDPSetPrimColor(displayList++, 0, 0, 255, 255, 255, 255);
    sprintf(g_PrintBuffer, "%3d", transfer->screenStep);
    PrintDebugText(g_PrintBuffer, 20, 2);
    if (transfer->screenStep == 8) {
        isCharacterShown = TRUE;
        character = &g_GameBoyCharacters[transfer->characterIndex];
    }
    if ((transfer->screenStep == 2 || transfer->screenStep == 3) && g_GameBoyTransfer.unk79C > 0) {
        isCharacterShown = TRUE;
        character = &transfer->cartridgeCharacters[transfer->listedCharacters[transfer->listedCharacterIndex]];
    }
    if (isCharacterShown) {
        DrawCharacterStats(&displayList, character, transfer);
    }
    SetUpTextDrawing(&displayList);
    if (transfer->unk7A0 == g_ControllerCount * 4 - 1) {
        message = NO_MESSAGE;
        switch (transfer->screenStep) {
            case 2:
                message = CHOOSE_CHARACTER_MESSAGE;
                break;
            case 8:
                message = CHOOSE_SWAPPED_CHARACTER_MESSAGE;
                break;
            case 7:
                message = CHARACTER_BROUGHT_MESSAGE;
                break;
        }
        if (message != NO_MESSAGE) {
            SetTextSlot(HINT_LEFT, HINT_TOP, message, 1, HINT_TEXT_SLOT, TEXT_FLAG_UNK20, 255, 255, 255, 255);
            DrawLargeTextSlot(&displayList, HINT_TEXT_SLOT);
        }
    }
    *displayListHead = displayList;
}

void DrawSendExperienceText(Gfx **displayListHead, GameBoyTransfer *transfer) {
    Gfx *displayList = *displayListHead;
    s32 slot;
    s32 message;

    gDPSetPrimColor(displayList++, 0, 0, 255, 255, 255, 255);
    BeginShadowedText(&displayList);
    for (slot = 0; slot != g_GameBoyTransfer.characterCount; slot++) {
        GameBoyCharacter *character = &g_GameBoyCharacters[slot];

        sprintf(g_PrintBuffer, "%4d", character->experience);
        DrawSmallText(&displayList, EXPERIENCE_LEFT + slot * FACE_SPACING, EXPERIENCE_TOP, g_PrintBuffer);
    }
    DrawCharacterStats(&displayList, &g_GameBoyCharacters[transfer->characterIndex], transfer);
    SetUpTextDrawing(&displayList);
    message = NO_MESSAGE;
    switch (transfer->screenStep) {
        case 0:
            message = CHOOSE_EXPERIENCE_MESSAGE;
            break;
        case 3:
            message = EXPERIENCE_SENT_MESSAGE;
            break;
    }
    if (message != NO_MESSAGE) {
        SetTextSlot(HINT_LEFT, HINT_TOP, message, 1, HINT_TEXT_SLOT, TEXT_FLAG_UNK20, 255, 255, 255, 255);
        DrawLargeTextSlot(&displayList, HINT_TEXT_SLOT);
    }
    *displayListHead = displayList;
}

INCLUDE_ASM("asm/nonmatchings/ovl_13/transmit_menu", DrawSendStatusScreen);

void DrawImportedFaces(Gfx **displayListHead, GameBoyTransfer *transfer) {
    Gfx *displayList = *displayListHead;
    s32 slot;
    s32 character;

    gDPPipeSync(displayList++);
    gDPSetTextureLUT(displayList++, G_TT_RGBA16);
    for (slot = 0; slot != GAME_BOY_CHARACTER_COUNT; slot++) {
        if (g_GameBoyCharacters[slot].status == GAME_BOY_CHARACTER_STATUS_PRESENT) {
            character = g_GameBoyCharacters[slot].character;
            LoadLargePalette(&displayList, transfer->facePalettes[character]);
            LoadTextureBlock(&displayList, transfer->faceImages[character], G_IM_FMT_CI,
                             G_IM_SIZ_8b, FACE_SIZE, FACE_SIZE, 0, 0, 0, 0, 0, 0, 0);
            DrawTextureRect(&displayList, (FACES_LEFT + slot * FACE_SPACING) << 2, FACES_TOP << 2,
                            (FACES_LEFT + slot * FACE_SPACING + FACE_SIZE) << 2, (FACES_TOP + FACE_SIZE) << 2,
                            G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
        }
    }
    *displayListHead = displayList;
}

void DrawOptionsScreen(Gfx **displayListHead, GameBoyTransfer *transfer) {
    Gfx *displayList = *displayListHead;

    DrawImportedFaces(&displayList, transfer);
    *displayListHead = displayList;
}

INCLUDE_ASM("asm/nonmatchings/ovl_13/transmit_menu", DrawBringCharactersScreen);

void DrawSendExperienceScreen(Gfx **displayListHead, GameBoyTransfer *transfer) {
    Gfx *displayList = *displayListHead;
    s32 index;
    /* A second counter alongside index is there to match */
    s32 slot;

    DrawImportedFaces(&displayList, transfer);
    if (transfer->screenStep == 0) {
        DrawCursorBox(&displayList, FACES_LEFT - FACE_CURSOR_MARGIN + transfer->characterIndex * FACE_SPACING,
                      FACES_TOP - FACE_CURSOR_MARGIN);
        gDPPipeSync(displayList++);
        gDPSetTexturePersp(displayList++, G_TP_NONE);
        gDPPipeSync(displayList++);
        gDPSetTextureLUT(displayList++, G_TT_RGBA16);
    }
    LoadSmallPalette(&displayList, 0, (u8 *)transfer->controllerIconsImage + IMAGE_HEADER_SIZE);
    LoadNibbleTextureBlock(&displayList, (u8 *)transfer->controllerIconsImage + PALETTED_TEXELS_OFFSET, G_IM_FMT_CI,
                           CONTROLLER_ICON_SIZE, CONTROLLER_ICON_SIZE * MAXCONTROLLERS, 0, 0, 0, 0, 0, 0, 0);
    slot = 0;
    gDPSetPrimColor(displayList++, 0, 0, 255, 255, 255, 255);
    for (index = 0; index != transfer->taggedCartridgeCount; index++) {
        if (transfer->screenStep == 3 && slot != transfer->chosenCartridgeIndex) {
            gDPSetPrimColor(displayList++, 0, 0, 127, 127, 127, 255);
        } else {
            gDPSetPrimColor(displayList++, 0, 0, 255, 255, 255, 255);
        }
        DrawTextureRect(&displayList, (CONTROLLER_ICONS_LEFT + slot * CONTROLLER_ICON_SPACING) << 2,
                        CONTROLLER_ICONS_TOP << 2,
                        (CONTROLLER_ICONS_LEFT + slot * CONTROLLER_ICON_SPACING + CONTROLLER_ICON_SIZE) << 2,
                        (CONTROLLER_ICONS_TOP + CONTROLLER_ICON_SIZE) << 2, G_TX_RENDERTILE, 0,
                        (transfer->taggedControllers[index] * CONTROLLER_ICON_SIZE) << 5, TEXEL_STEP_ONE,
                        TEXEL_STEP_ONE);
        slot++;
    }
    *displayListHead = displayList;
}

INCLUDE_ASM("asm/nonmatchings/ovl_13/transmit_menu", DrawTransmitMenu);

void ClearCartridgeCharacters(void) {
    /* The transfer pointer is there to match */
    GameBoyTransfer *transfer = &g_GameBoyTransfer;
    s32 controller;
    s32 save;

    if (transfer->screen == TRANSMIT_SCREEN_BRING_CHARACTERS) {
        transfer->unk7A0 = 0;
        ResetBringCharacters();
        for (controller = 0; controller != MAXCONTROLLERS; controller++) {
            for (save = 0; save != CARTRIDGE_SAVE_COUNT; save++) {
                transfer->unk76C[controller * CARTRIDGE_SAVE_COUNT + save] = 0;
                transfer->cartridgeCharacters[controller * CARTRIDGE_SAVE_COUNT + save].status = 0;
            }
        }
        transfer->unk79C = 0;
    }
}

INCLUDE_ASM("asm/nonmatchings/ovl_13/transmit_menu", WriteExperienceToCartridge);
/* WriteExperienceToCartridge and WriteStatusToCartridge share one copy of this */
extern const char sWriteSumFormat[];

INCLUDE_RODATA("asm/nonmatchings/ovl_13/transmit_menu", sWriteSumFormat);
INCLUDE_RODATA("asm/nonmatchings/ovl_13/transmit_menu", D_801FBF18_ovl_13);
INCLUDE_RODATA("asm/nonmatchings/ovl_13/transmit_menu", D_801FBF24_ovl_13);
INCLUDE_RODATA("asm/nonmatchings/ovl_13/transmit_menu", D_801FBF30_ovl_13);

/*
 * Checks that the chosen cartridge is the one tagged and that its save is intact, and reports
 * whether the save already holds a status, which WriteStatusToCartridge marks.
 */
s32 CheckStatusCartridge(s32 cartridgeIndex) {
    /* The transfer pointer is there to match */
    GameBoyTransfer *transfer = &g_GameBoyTransfer;
    u8 save[CARTRIDGE_SAVE_SIZE];
    s32 isSignatureWrong;
    s32 index;
    s32 result;
    s32 readResult;
    s32 computedSum;
    s32 storedSum;

    result = OpenGameBoyPakWithRetries(g_PlayerControllerPorts[g_GameBoyTransfer.taggedControllers[cartridgeIndex]],
                                       transfer->cartridgeHeader, &transfer->pakFile);
    g_GameBoyTransfer.transferResult = result;
    if (result != PAK_RESULT_OK) {
        /* Going to the shared power-off is there to match */
        goto failed;
    }
    isSignatureWrong = FALSE;
    readResult = nuContGBPakFread(&transfer->pakFile, CARTRIDGE_RAM_ADDRESS, save, CARTRIDGE_SAVE_SIZE);
    if (readResult != 0) {
        osSyncPrintf("head error  2\n");
        g_GameBoyTransfer.transferResult = readResult;
        nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
        return STATUS_CHECK_READ_FAILED;
    }
    for (index = 0; index != CARTRIDGE_TAG_SIZE; index++) {
        if (save[SAVE_SIGNATURE_OFFSET + index] != sCartridgeSaveSignature[index]) {
            isSignatureWrong = TRUE;
        }
    }
    for (index = 0; index != CARTRIDGE_TAG_SIZE; index++) {
        if (save[index] != transfer->cartridgeTags[transfer->taggedControllers[cartridgeIndex]][index]) {
            osSyncPrintf("\x83\x89\x83" "C\x83g\x83}\x81[\x83N\x88\xE1\x94\xBD\n"); /* "Write mark violation" */
            transfer->transferResult = PAK_RESULT_REMOVED_RETRYING;
            nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
            return STATUS_CHECK_FAILED;
        }
    }
    computedSum = ComputeSaveChecksum(save);
    storedSum = GetStoredChecksum(save);
    osSyncPrintf("sum1 %04x\n", computedSum);
    osSyncPrintf("sum2 %04x\n", storedSum);
    if (computedSum != storedSum || isSignatureWrong) {
        osSyncPrintf("sum error  2\n");
        transfer->transferResult = PAK_RESULT_CORRUPTED;
        nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
        return STATUS_CHECK_FAILED;
    }
    if (save[SAVE_HAS_STATUS_OFFSET] == 0) {
        nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
        return STATUS_CHECK_NO_STATUS;
    }
    nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
    return STATUS_CHECK_HAS_STATUS;
failed:
    nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
    return STATUS_CHECK_FAILED;
}

/*
 * Marks the chosen cartridge's save as holding a status and writes the status data where the save
 * says, then the sums of both. It first checks that the cartridge is the one tagged and that its
 * save is intact.
 */
s32 WriteStatusToCartridge(s32 cartridgeIndex) {
    /* The transfer pointer is there to match */
    GameBoyTransfer *transfer = &g_GameBoyTransfer;
    u8 save[CARTRIDGE_SAVE_SIZE];
    u8 status[STATUS_DATA_SIZE];
    s32 result;
    s32 index;
    s32 statusAddress;
    s32 statusSize;
    s32 sum;

    g_GameBoyTransfer.transferResult =
        OpenGameBoyPakWithRetries(g_PlayerControllerPorts[transfer->taggedControllers[cartridgeIndex]],
                                  transfer->cartridgeHeader, &transfer->pakFile);
    if (g_GameBoyTransfer.transferResult != PAK_RESULT_OK) {
        /* Going to the shared power-off is there to match */
        goto failed;
    }
    result = nuContGBPakFread(&transfer->pakFile, CARTRIDGE_RAM_ADDRESS, save, CARTRIDGE_SAVE_SIZE);
    if (result != 0) {
        nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
        g_GameBoyTransfer.transferResult = result;
        return STATUS_WRITE_PAK_ERROR;
    }
    for (index = 0; index != CARTRIDGE_TAG_SIZE; index++) {
        if (save[index] != transfer->cartridgeTags[transfer->taggedControllers[cartridgeIndex]][index]) {
            /* "Write mark violation"; the original passes sum before setting it */
            osSyncPrintf("\x83\x89\x83" "C\x83g\x83}\x81[\x83N\x88\xE1\x94\xBD\n", sum);
            nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
            transfer->transferResult = PAK_RESULT_CARTRIDGE_REPLACED;
            return STATUS_WRITE_FAILED;
        }
    }
    sum = ComputeSaveChecksum(save);
    if (sum != GetStoredChecksum(save)) {
        transfer->transferResult = PAK_RESULT_LOOSE_PAK;
        nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
        return STATUS_WRITE_FAILED;
    }
    statusAddress = save[SAVE_STATUS_ADDRESS_OFFSET] + (save[SAVE_STATUS_ADDRESS_OFFSET + 1] << 8);
    save[SAVE_HAS_STATUS_OFFSET] = TRUE;
    statusSize = save[SAVE_STATUS_SIZE_OFFSET] + (save[SAVE_STATUS_SIZE_OFFSET + 1] << 8);
    result = nuContGBPakFwrite(&transfer->pakFile, CARTRIDGE_RAM_ADDRESS + SAVE_HAS_STATUS_OFFSET,
                               &save[SAVE_HAS_STATUS_OFFSET], SAVE_HALFWORD_SIZE);
    if (result != 0) {
        nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
        transfer->transferResult = result;
        return STATUS_WRITE_PAK_ERROR;
    }
    BuildStatusData(status);
    result = nuContGBPakFwrite(&transfer->pakFile, statusAddress - STATUS_ADDRESS_BIAS, status, statusSize);
    if (result != 0) {
        nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
        transfer->transferResult = result;
        return STATUS_WRITE_PAK_ERROR;
    }
    sum = 0;
    for (index = 0; index != statusSize; index++) {
        sum += status[index];
    }
    sum &= 0xFFFF;
    osSyncPrintf(sWriteSumFormat, sum);
    save[SAVE_STATUS_SUM_OFFSET] = sum;
    save[SAVE_STATUS_SUM_OFFSET + 1] = sum >> 8;
    result = nuContGBPakFwrite(&transfer->pakFile, CARTRIDGE_RAM_ADDRESS + SAVE_STATUS_SUM_OFFSET,
                               &save[SAVE_STATUS_SUM_OFFSET], SAVE_HALFWORD_SIZE);
    if (result != 0) {
        nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
        transfer->transferResult = result;
        return STATUS_WRITE_PAK_ERROR;
    }
    sum = ComputeSaveChecksum(save) & 0xFFFF;
    save[SAVE_CHECKSUM_OFFSET] = sum;
    save[SAVE_CHECKSUM_OFFSET + 1] = sum >> 8;
    osSyncPrintf("write sum2 %04x\n", sum);
    result = nuContGBPakFwrite(&transfer->pakFile, CARTRIDGE_RAM_ADDRESS + SAVE_CHECKSUM_OFFSET,
                               &save[SAVE_CHECKSUM_OFFSET], SAVE_HALFWORD_SIZE);
    if (result != 0) {
        nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
        transfer->transferResult = result;
        return STATUS_WRITE_PAK_ERROR;
    }
    nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
    transfer->transferResult = PAK_RESULT_OK;
    return STATUS_WRITE_DONE;
failed:
    nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
    return STATUS_WRITE_FAILED;
}

INCLUDE_ASM("asm/nonmatchings/ovl_13/transmit_menu", ReadCartridgeCharacters);
INCLUDE_RODATA("asm/nonmatchings/ovl_13/transmit_menu", D_801FBFD4_ovl_13);
INCLUDE_RODATA("asm/nonmatchings/ovl_13/transmit_menu", D_801FBFF8_ovl_13);
INCLUDE_RODATA("asm/nonmatchings/ovl_13/transmit_menu", D_801FC010_ovl_13);
INCLUDE_RODATA("asm/nonmatchings/ovl_13/transmit_menu", D_801FC028_ovl_13);
INCLUDE_RODATA("asm/nonmatchings/ovl_13/transmit_menu", D_801FC044_ovl_13);
INCLUDE_RODATA("asm/nonmatchings/ovl_13/transmit_menu", D_801FC060_ovl_13);
INCLUDE_RODATA("asm/nonmatchings/ovl_13/transmit_menu", jtbl_801FC080_ovl_13);

s32 OpenGameBoyPakWithRetries(s32 port, u8 *header, NUContPakFile *file) {
    s32 failureCount;
    s32 result;

    for (failureCount = 0; failureCount != GAME_BOY_PAK_OPEN_ATTEMPTS; failureCount++) {
        result = OpenGameBoyPak(port, header, file);
        if (result == PAK_RESULT_OK) {
            return PAK_RESULT_OK;
        }
        nuContGBPakMgrInit();
    }
    return result;
}

INCLUDE_ASM("asm/nonmatchings/ovl_13/transmit_menu", OpenGameBoyPak);
INCLUDE_RODATA("asm/nonmatchings/ovl_13/transmit_menu", D_801FC114_ovl_13);
INCLUDE_RODATA("asm/nonmatchings/ovl_13/transmit_menu", D_801FC124_ovl_13);
INCLUDE_RODATA("asm/nonmatchings/ovl_13/transmit_menu", D_801FC15C_ovl_13);
INCLUDE_RODATA("asm/nonmatchings/ovl_13/transmit_menu", D_801FC188_ovl_13);
INCLUDE_RODATA("asm/nonmatchings/ovl_13/transmit_menu", D_801FC1B4_ovl_13);
INCLUDE_RODATA("asm/nonmatchings/ovl_13/transmit_menu", D_801FC1E0_ovl_13);
INCLUDE_RODATA("asm/nonmatchings/ovl_13/transmit_menu", D_801FC204_ovl_13);
INCLUDE_RODATA("asm/nonmatchings/ovl_13/transmit_menu", D_801FC208_ovl_13);
INCLUDE_RODATA("asm/nonmatchings/ovl_13/transmit_menu", D_801FC210_ovl_13);
INCLUDE_RODATA("asm/nonmatchings/ovl_13/transmit_menu", D_801FC244_ovl_13);
INCLUDE_RODATA("asm/nonmatchings/ovl_13/transmit_menu", D_801FC284_ovl_13);
INCLUDE_RODATA("asm/nonmatchings/ovl_13/transmit_menu", D_801FC2B0_ovl_13);
INCLUDE_RODATA("asm/nonmatchings/ovl_13/transmit_menu", D_801FC2DC_ovl_13);
INCLUDE_RODATA("asm/nonmatchings/ovl_13/transmit_menu", D_801FC328_ovl_13);
INCLUDE_RODATA("asm/nonmatchings/ovl_13/transmit_menu", D_801FC360_ovl_13);
INCLUDE_RODATA("asm/nonmatchings/ovl_13/transmit_menu", D_801FC394_ovl_13);
INCLUDE_RODATA("asm/nonmatchings/ovl_13/transmit_menu", D_801FC3D4_ovl_13);
INCLUDE_RODATA("asm/nonmatchings/ovl_13/transmit_menu", D_801FC400_ovl_13);
INCLUDE_RODATA("asm/nonmatchings/ovl_13/transmit_menu", jtbl_801FC440_ovl_13);
INCLUDE_RODATA("asm/nonmatchings/ovl_13/transmit_menu", jtbl_801FC478_ovl_13);

/*
 * Writes a random tag to the cartridge RAM of each Game Boy cartridge and lists the controllers
 * whose cartridges took it, so that later accesses can tell whether a cartridge was swapped.
 */
s32 TagCartridges(void) {
    /* The transfer pointer is there to match */
    GameBoyTransfer *transfer = &g_GameBoyTransfer;
    u8 header[CARTRIDGE_HEADER_SIZE];
    NUContPakFile file;
    u8 tag[CARTRIDGE_TAG_SIZE];
    u8 status;
    s32 isAnyTagged = FALSE;
    s32 wasNoCartridge = FALSE;
    s32 wasNoPak = FALSE;
    s32 wasOtherPak = FALSE;
    s32 wasWrongCartridge = FALSE;
    s32 wasCartridgePulled = FALSE;
    s32 controller;
    s32 index;
    s32 result;

    g_GameBoyTransfer.taggedCartridgeCount = 0;
    for (controller = 0; controller != MAXCONTROLLERS; controller++) {
        transfer->taggedControllers[controller] = 0;
    }
    nuContGBPakMgrInit();
    for (controller = 0; controller < g_ControllerCount; controller++) {
        result = OpenGameBoyPakWithRetries(g_PlayerControllerPorts[controller], header, &file);
        if (result == PAK_RESULT_NO_PAK) {
            wasNoPak = TRUE;
        }
        if (result == PAK_RESULT_OTHER_PAK) {
            wasOtherPak = TRUE;
        }
        if (result == PAK_RESULT_NO_CARTRIDGE) {
            wasNoCartridge = TRUE;
        }
        if (result == PAK_RESULT_WRONG_CARTRIDGE) {
            wasWrongCartridge = TRUE;
        }
        if (result == PAK_RESULT_OK) {
            for (index = 0; index != CARTRIDGE_TAG_SIZE; index++) {
                transfer->cartridgeTags[controller][index] = tag[index] = guRandom() % 256;
            }
            if (nuContGBPakFwrite(&file, CARTRIDGE_RAM_ADDRESS, tag, CARTRIDGE_TAG_SIZE) == 0
                && nuContGBPakGetStatus(&file, &status) == 0) {
                if (status & OS_GBPAK_GBCART_PULL) {
                    wasCartridgePulled = TRUE;
                } else {
                    transfer->taggedControllers[transfer->taggedCartridgeCount] = controller;
                    isAnyTagged = TRUE;
                    transfer->taggedCartridgeCount++;
                }
            }
        }
        nuContGBPakPower(&file, NU_CONT_GBPAK_POWER_OFF);
    }
    if (isAnyTagged == TRUE) {
        return PAK_RESULT_OK;
    }
    if (wasCartridgePulled == TRUE) {
        return PAK_RESULT_REMOVED_RETRYING;
    }
    if (wasWrongCartridge == TRUE) {
        return PAK_RESULT_WRONG_CARTRIDGE;
    }
    if (wasNoCartridge == TRUE) {
        return PAK_RESULT_NO_CARTRIDGE;
    }
    if (wasOtherPak == TRUE) {
        return PAK_RESULT_OTHER_PAK;
    }
    if (wasNoPak == TRUE) {
        return PAK_RESULT_NO_PAK;
    }
    return PAK_RESULT_LOOSE_PAK;
}

void SetNoticeState(s32 state, s32 message) {
    sNoticeState = state;
    if (state == NOTICE_OPENING) {
        sNoticeSize = 0;
        sNoticeMessage = message;
    }
    if (state == NOTICE_CLOSING) {
        sNoticeSize = NOTICE_FULL_SIZE;
    }
}

s32 GetNoticeState(void) {
    return sNoticeState;
}

INCLUDE_ASM("asm/nonmatchings/ovl_13/transmit_menu", DrawNotice);

void DrawAccessingSign(Gfx **displayListHead) {
    Gfx *displayList = *displayListHead;
    s32 strip;

    LoadLargePalette(&displayList, (u8 *)g_GameBoyTransfer.accessingImage + IMAGE_HEADER_SIZE);
    for (strip = 0; strip != ACCESSING_STRIP_COUNT; strip++) {
        LoadTextureBlock(&displayList,
                         (u8 *)g_GameBoyTransfer.accessingImage + LARGE_PALETTED_TEXELS_OFFSET +
                             strip * ACCESSING_STRIP_SIZE,
                         G_IM_FMT_CI, G_IM_SIZ_8b, ACCESSING_WIDTH, ACCESSING_STRIP_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
        DrawTextureRect(&displayList, ACCESSING_LEFT << 2, (ACCESSING_TOP + strip * ACCESSING_STRIP_HEIGHT) << 2,
                        (ACCESSING_LEFT + ACCESSING_WIDTH) << 2,
                        (ACCESSING_TOP + strip * ACCESSING_STRIP_HEIGHT + ACCESSING_STRIP_HEIGHT) << 2,
                        G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    }
    *displayListHead = displayList;
}

void DrawPressingHand(Gfx **displayListHead) {
    Gfx *displayList = *displayListHead;

    sPressingHandFrame = (sPressingHandFrame + 1) % PRESSING_HAND_FRAME_COUNT;
    LoadLargePalette(&displayList, (u8 *)g_PressingHandImage + IMAGE_HEADER_SIZE);
    LoadTextureBlock(&displayList,
                     (u8 *)g_PressingHandImage + LARGE_PALETTED_TEXELS_OFFSET +
                         sPressingHandFrame / PRESSING_HAND_FRAMES_PER_IMAGE * PRESSING_HAND_IMAGE_SIZE,
                     G_IM_FMT_CI, G_IM_SIZ_8b, PRESSING_HAND_SIZE, PRESSING_HAND_SIZE, 0, 0, 0, 0, 0, 0, 0);
    if (g_GameBoyTransfer.pressingHandPosition == PRESSING_HAND_LOW) {
        DrawTextureRect(&displayList, LOW_PRESSING_HAND_LEFT << 2, LOW_PRESSING_HAND_TOP << 2,
                        (LOW_PRESSING_HAND_LEFT + PRESSING_HAND_SIZE) << 2,
                        (LOW_PRESSING_HAND_TOP + PRESSING_HAND_SIZE) << 2, G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE,
                        TEXEL_STEP_ONE);
    } else {
        DrawTextureRect(&displayList, NOTICE_PRESSING_HAND_LEFT << 2, NOTICE_PRESSING_HAND_TOP << 2,
                        (NOTICE_PRESSING_HAND_LEFT + PRESSING_HAND_SIZE) << 2,
                        (NOTICE_PRESSING_HAND_TOP + PRESSING_HAND_SIZE) << 2, G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE,
                        TEXEL_STEP_ONE);
    }
    *displayListHead = displayList;
}

INCLUDE_ASM("asm/nonmatchings/ovl_13/transmit_menu", BuildStatusData);

/* The whole part of value once rounded to tenths, up to MAX_STATUS_VALUE */
s32 RoundedWholePart(f32 value) {
    s32 wholePart = value;
    /* The fraction in hundredths, then in tenths; reusing one variable is there to match */
    s32 fraction = (value - wholePart) * 100.0f;

    if (fraction % 10 >= 5) {
        fraction += 10;
    }
    fraction /= 10;
    if (fraction >= 10) {
        wholePart++;
        if (wholePart >= MAX_STATUS_VALUE + 1) {
            wholePart = MAX_STATUS_VALUE;
        }
    }
    return wholePart;
}
