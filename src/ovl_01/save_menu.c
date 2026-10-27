#include "common.h"
#include "asset.h"
#include "character.h"
#include "game_flags.h"
#include "libc.h"
#include "menu_memory.h"
#include "round_hud.h"
#include "save.h"
#include "sram.h"

/* The image listing the game modes, in orange */
#define ASSET_MENU_GAME_MODES 0x528

extern s32 D_80230A90_ovl_01;
extern s32 D_80230A94_ovl_01;
extern s32 D_80230A98_ovl_01;
extern void *D_8023B670_ovl_01;
extern void *D_8023B674_ovl_01;
extern void *D_8023B678_ovl_01;
extern void *D_8023B67C_ovl_01;
/* The slots that the save menu steps through, which hold -1 when empty */
#define SAVE_MENU_SLOT_COUNT 5

/* The most bytes of each palette file that func_80220BB0 keeps */
#define SAVE_MENU_FILE_SIZE 0x200

/* The images of D_800C2064 that func_8021F6E0 reads, before one of its own */
#define SAVE_MENU_IMAGE_COUNT 6

/* The game modes that have an image of their own, in image order, then mini golf's */
#define SAVE_MENU_MODE_COUNT 10
#define SAVE_MENU_MODE_MINI_GOLF 10

void func_80221CC0_ovl_01(void);

/* The Ring Shot stars that each level of func_8022181C takes */
#define RING_SHOT_STARS_PER_LEVEL 4

/* What func_80221EBC reads: each saved game's mode image and a frame, or the chosen game's two */
#define SAVE_MENU_BUFFER_SLOTS 1
#define SAVE_MENU_BUFFER_CHOSEN 2
#define SAVE_MENU_CHOSEN_IMAGE_COUNT 2
#define ASSET_SAVE_MENU_FRAME 0x70D

/* The image of each game mode that a saved game can be in */
typedef struct {
    /* 0x0 */ s16 unk0;
    /* 0x2 */ u16 assetId;
} SaveMenuModeImage; /* size = 0x4 */

extern s8 D_800C1FF4[MAX_PLAYER_COUNT];
extern u16 D_800C2064[];
extern void *D_8012D430[CHARACTER_COUNT];
extern u16 D_802308A4_ovl_01[CHARACTER_COUNT];
extern s16 D_802308C8_ovl_01[CHARACTER_COUNT][CHARACTER_PALETTE_COUNT];
extern s32 D_8012F480[MAX_PLAYER_COUNT];
extern s8 D_801B70F0[MAXCONTROLLERS];
extern u16 D_80230B40_ovl_01[MAX_PLAYER_COUNT];
extern s16 D_80230B48_ovl_01;
extern s16 D_80230B4A_ovl_01;
extern u8 D_80230B4C_ovl_01;
extern s8 D_80230B4D_ovl_01;
extern s32 D_80230B50_ovl_01[SAVED_GAME_COUNT];
extern s32 D_80230B88_ovl_01[SAVE_MENU_MODE_COUNT];
extern SaveMenuModeImage D_80230B5C_ovl_01[];
extern void **D_8023B680_ovl_01;
extern s16 D_8023B684_ovl_01[SAVE_MENU_SLOT_COUNT];
extern void **D_8023BD70_ovl_01;
extern void **D_8023BD74_ovl_01;
extern void *D_8023BD78_ovl_01;
extern s16 D_8023BDA4_ovl_01[MAX_PLAYER_COUNT];
extern u8 D_8023BFF0_ovl_01[CHARACTER_COUNT][SAVE_MENU_FILE_SIZE];
extern u8 D_8023E3F0_ovl_01[SAVE_MENU_SLOT_COUNT][SAVE_MENU_FILE_SIZE];
extern void *D_8023BD7C_ovl_01;

void func_8021F5F0_ovl_01(void *buffer, s16 bufferKind) {
    EncodedAsset asset;

    switch (bufferKind) {
        case 1:
            D_8023B670_ovl_01 = buffer;
            break;
        case 2:
            D_8023B674_ovl_01 = buffer;
            OpenEncodedAsset(0x5F7, &asset);
            ReadEncodedAsset(D_8023B674_ovl_01, &asset);
            break;
        case 3:
            D_8023B678_ovl_01 = buffer;
            OpenEncodedAsset(0x70E, &asset);
            ReadEncodedAsset(D_8023B678_ovl_01, &asset);
            break;
        case 4:
            D_8023B67C_ovl_01 = buffer;
            OpenEncodedAsset(0x720, &asset);
            ReadEncodedAsset(D_8023B67C_ovl_01, &asset);
            D_80230A90_ovl_01 = 0;
            D_80230A94_ovl_01 = 0;
            D_80230A98_ovl_01 = 0;
            break;
    }
}

void func_8021F6E0_ovl_01(void **buffers) {
    EncodedAsset asset;
    s16 imageIndex;

    D_8023B680_ovl_01 = buffers;
    for (imageIndex = 0; imageIndex != SAVE_MENU_IMAGE_COUNT; imageIndex++) {
        OpenEncodedAsset(D_800C2064[imageIndex], &asset);
        ReadEncodedAsset(D_8023B680_ovl_01[imageIndex], &asset);
    }
    OpenEncodedAsset(0x944, &asset);
    ReadEncodedAsset(D_8023B680_ovl_01[SAVE_MENU_IMAGE_COUNT], &asset);
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/save_menu", func_8021F79C_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/save_menu", func_8021FC5C_ovl_01);

void func_80220A30_ovl_01(void) {
    SaveData *save = GetSaveData();
    MenuMemory *memory = func_8005AF74();
    s8 slot;

    InitSram();
    TransferSaveData(save, FALSE);
    for (slot = 0; slot != SAVE_MENU_SLOT_COUNT; slot++) {
        D_8023B684_ovl_01[slot] = -1;
    }
    for (slot = 0; slot != SAVE_MENU_SLOT_COUNT; slot++) {
        if ((s16)func_80220E28_ovl_01(&save->unk1DF0[slot])) {
            D_8023B684_ovl_01[slot] = slot;
        } else {
            D_8023B684_ovl_01[slot] = -1;
        }
    }
    if (D_8023B684_ovl_01[memory->unk42] == -1) {
        for (slot = 0; slot != SAVE_MENU_SLOT_COUNT; slot++) {
            if (D_8023B684_ovl_01[slot] != -1) {
                memory->unk42 = slot;
            }
        }
    }
}

void func_80220BB0_ovl_01(void) {
    EncodedAsset asset;
    AssetFile file;
    SaveData *save = GetSaveData();
    s32 index;
    s32 character;
    s32 palette;

    for (index = 0; index != CHARACTER_COUNT; index++) {
        OpenEncodedAsset(D_802308A4_ovl_01[index], &asset);
        ReadEncodedAsset(D_8012D430[index], &asset);
        OpenAssetFile(D_802308C8_ovl_01[index][0], &file);
        ReadAssetFile(D_8023BFF0_ovl_01[index], GetAssetFileSize(&file), &file);
    }
    for (index = 0; index != SAVE_MENU_SLOT_COUNT; index++) {
        if (D_8023B684_ovl_01[index] != -1) {
            character = (s8)save->unk1DF0[D_8023B684_ovl_01[index]].unk37;
            palette = (s8)save->unk1DF0[D_8023B684_ovl_01[index]].unk39;
            if (character < 0) {
                character = 0;
            }
            if (character >= CHARACTER_COUNT) {
                character = CHARACTER_COUNT - 1;
            }
            if (palette < 0) {
                palette = 0;
            }
            if (palette >= CHARACTER_PALETTE_COUNT) {
                palette = CHARACTER_PALETTE_COUNT - 1;
            }
        } else {
            palette = 0;
            character = 0;
        }
        OpenAssetFile(D_802308C8_ovl_01[character][palette], &file);
        ReadAssetFile(D_8023E3F0_ovl_01[index], GetAssetFileSize(&file), &file);
    }
}

s16 func_80220D68_ovl_01(s8 startSlot, u8 isForward) {
    s32 slot = startSlot;
    s32 step;
    s32 tryCount;

    if (slot < 0) {
        return -1;
    }
    if (slot >= SAVE_MENU_SLOT_COUNT) {
        return -1;
    }
    step = isForward ? 1 : -1;
    slot += step;
    for (tryCount = 0; tryCount != SAVE_MENU_SLOT_COUNT - 1; tryCount++) {
        if (slot >= SAVE_MENU_SLOT_COUNT) {
            slot = 0;
        }
        if (slot < 0) {
            slot = SAVE_MENU_SLOT_COUNT - 1;
        }
        if (D_8023B684_ovl_01[slot] != -1) {
            break;
        }
        slot += step;
    }
    if (tryCount == SAVE_MENU_SLOT_COUNT - 1) {
        return -1;
    }
    return slot;
}

s16 func_80220DF8_ovl_01(void) {
    return D_8023B684_ovl_01[func_8005AF74()->unk42];
}

s32 func_80220E28_ovl_01(Unk68 *record) {
    return (u8)record->unk36 >> 7;
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/save_menu", func_80220E34_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/save_menu", func_802213C8_ovl_01);

s32 func_8022181C_ovl_01(void) {
    SaveData *save = GetSaveData();
    s32 starCount = 0;
    s32 courseIndex;
    s32 holeIndex;

    for (courseIndex = 0; courseIndex != RING_SHOT_COURSE_COUNT; courseIndex++) {
        for (holeIndex = 0; holeIndex != RING_SHOT_HOLE_COUNT; holeIndex++) {
            starCount += save->hasRingShotStar[courseIndex][holeIndex] > 0;
        }
    }
    if (starCount >= RING_SHOT_STARS_PER_LEVEL * 5) {
        return 6;
    }
    if (starCount >= RING_SHOT_STARS_PER_LEVEL * 4) {
        return 5;
    }
    if (starCount >= RING_SHOT_STARS_PER_LEVEL * 3) {
        return 4;
    }
    if (starCount >= RING_SHOT_STARS_PER_LEVEL * 2) {
        return 3;
    }
    if (starCount >= RING_SHOT_STARS_PER_LEVEL) {
        return 2;
    }
    return 1;
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/save_menu", func_802218C0_ovl_01);

void func_80221B20_ovl_01(void) {
    SaveData *save = GetSaveData();
    MenuMemory *memory = func_8005AF74();
    s8 gameIndex;
    s8 modeIndex;

    for (gameIndex = 0; gameIndex != SAVED_GAME_COUNT; gameIndex++) {
        D_80230B50_ovl_01[gameIndex] = -1;
    }
    D_80230B4D_ovl_01 = 0;
    InitSram();
    TransferSaveData(save, FALSE);
    for (gameIndex = 0; gameIndex != SAVED_GAME_COUNT; gameIndex++) {
        if (save->savedGames[gameIndex].isValid) {
            if (save->savedGames[gameIndex].state.isMiniGolf) {
                D_80230B50_ovl_01[gameIndex] = SAVE_MENU_MODE_MINI_GOLF;
            } else {
                for (modeIndex = 0; modeIndex != SAVE_MENU_MODE_COUNT; modeIndex++) {
                    if (save->savedGames[gameIndex].state.gameMode == D_80230B88_ovl_01[modeIndex]) {
                        D_80230B50_ovl_01[gameIndex] = modeIndex;
                        break;
                    }
                }
            }
        }
    }
    for (gameIndex = 0; gameIndex != SAVED_GAME_COUNT; gameIndex++) {
        if (D_80230B50_ovl_01[gameIndex] != -1) {
            break;
        }
    }
    if (gameIndex == SAVED_GAME_COUNT) {
        memory->savedGameIndex = 0;
    } else {
        memory->savedGameIndex = gameIndex;
    }
    func_80221CC0_ovl_01();
}

/* Doesn't match: the original sets the first loop's counter later and keeps the player count in other registers */
#ifdef NON_MATCHING
void func_80221CC0_ovl_01(void) {
    SaveData *save = GetSaveData();
    MenuMemory *memory = func_8005AF74();
    s8 playerIndex;
    s8 playerCount;
    s8 humanCount;
    s8 gameIndex;
    UnkB8 *savedPlayers;

    for (playerIndex = 0; playerIndex != MAX_PLAYER_COUNT; playerIndex++) {
        D_80230B40_ovl_01[playerIndex] = -1;
        D_8023BDA4_ovl_01[playerIndex] = -1;
    }
    D_80230B48_ovl_01 = 0;
    gameIndex = memory->savedGameIndex;
    playerCount = save->savedGames[gameIndex].state.playerCount;
    savedPlayers = save->savedPlayers[gameIndex];
    D_80230B4A_ovl_01 = save->savedGames[gameIndex].state.playerCount;
    for (playerIndex = 0; playerIndex != playerCount; playerIndex++) {
        if ((s8)savedPlayers[playerIndex].unk1E[0]) {
            D_80230B40_ovl_01[playerIndex] = MAXCONTROLLERS;
            D_80230B4A_ovl_01--;
        } else {
            D_80230B40_ovl_01[playerIndex] = -1;
        }
    }
    humanCount = 0;
    for (playerIndex = 0; playerIndex != playerCount; playerIndex++) {
        if (D_80230B40_ovl_01[playerIndex] != MAXCONTROLLERS) {
            D_8023BDA4_ovl_01[humanCount] = playerIndex;
            humanCount++;
        }
    }
    D_80230B48_ovl_01 = 0;
    D_80230B4C_ovl_01 = 0;
    for (playerIndex = 0; playerIndex != MAXCONTROLLERS; playerIndex++) {
        D_801B70F0[playerIndex] = 0;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/save_menu", func_80221CC0_ovl_01);
#endif

/*
 * Doesn't match: the menu memory pointer and the mode image table take
 * each other's saved registers.
 */
#ifdef NON_MATCHING
void func_80221EBC_ovl_01(void **buffers, u8 bufferKind) {
    EncodedAsset asset;
    MenuMemory *memory = func_8005AF74();
    s32 imageIndex;
    s32 mode;
    SaveMenuModeImage *modeImages;

    switch (bufferKind) {
        case SAVE_MENU_BUFFER_SLOTS:
            D_8023BD70_ovl_01 = buffers;
            modeImages = D_80230B5C_ovl_01;
            for (imageIndex = 0; imageIndex != SAVED_GAME_COUNT; imageIndex++) {
                mode = D_80230B50_ovl_01[imageIndex];
                if (mode == -1) {
                    mode = 0;
                }
                OpenEncodedAsset(modeImages[mode].assetId, &asset);
                ReadEncodedAsset(D_8023BD70_ovl_01[imageIndex], &asset);
            }
            OpenEncodedAsset(ASSET_SAVE_MENU_FRAME, &asset);
            ReadEncodedAsset(D_8023BD70_ovl_01[SAVED_GAME_COUNT], &asset);
            break;
        case SAVE_MENU_BUFFER_CHOSEN:
            D_8023BD74_ovl_01 = buffers;
            modeImages = D_80230B5C_ovl_01;
            for (imageIndex = 0; imageIndex != SAVE_MENU_CHOSEN_IMAGE_COUNT; imageIndex++) {
                mode = D_80230B50_ovl_01[memory->savedGameIndex];
                if (mode == -1) {
                    mode = 0;
                }
                OpenEncodedAsset(modeImages[mode].assetId, &asset);
                ReadEncodedAsset(D_8023BD74_ovl_01[imageIndex], &asset);
            }
            break;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/save_menu", func_80221EBC_ovl_01);
#endif

void func_80222038_ovl_01(void *buffer, u8 bufferIndex) {
    EncodedAsset asset;

    switch (bufferIndex) {
        case 1:
            D_8023BD78_ovl_01 = buffer;
            break;
        case 2:
            D_8023BD7C_ovl_01 = buffer;
            OpenEncodedAsset(ASSET_MENU_GAME_MODES, &asset);
            ReadEncodedAsset(D_8023BD7C_ovl_01, &asset);
            break;
    }
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/save_menu", func_8022209C_ovl_01);

s32 func_80222C94_ovl_01(void) {
    SaveData *save = GetSaveData();
    MenuMemory *memory = func_8005AF74();
    SavedGame *game = &save->savedGames[memory->savedGameIndex];

    return -(game->isValid == FALSE);
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/save_menu", func_80222CDC_ovl_01);

extern const char D_80231A3C_ovl_01[];

s32 func_80222F2C_ovl_01(void) {
    SaveData *save = GetSaveData();
    MenuMemory *memory = func_8005AF74();
    UnkB8 *savedPlayer;
    s32 playerIndex;
    s32 character;
    s32 controller;

    if (D_80230B50_ovl_01[memory->savedGameIndex] == -1) {
        return -1;
    }
    if (SaveOrContinueGame(memory->savedGameIndex, TRUE) == -1) {
        return -1;
    }
    savedPlayer = save->savedPlayers[memory->savedGameIndex];
    for (playerIndex = 0; playerIndex != MAX_PLAYER_COUNT; playerIndex++) {
        controller = D_80230B40_ovl_01[playerIndex];
        D_800C1FF4[playerIndex] = controller;
        g_Players[playerIndex].controllerIndex = controller;
        sprintf(g_Players[playerIndex].name, D_80231A3C_ovl_01, savedPlayer->name);
        character = (s8)savedPlayer->characterIndex;
        D_8012F480[playerIndex] = character;
        g_Players[playerIndex].characterIndex = character;
        LoadPlayerFace(playerIndex, character, (s8)g_Players[playerIndex].paletteIndex);
        savedPlayer++;
    }
    if (!IsGameFlagSet(GAME_FLAG_KEEP_SAVED_GAMES)) {
        memset(&save->savedGames[memory->savedGameIndex], 0, sizeof(SavedGame));
    }
    return 0;
}

const char D_80231A3C_ovl_01[] = "%s";

INCLUDE_ASM("asm/nonmatchings/ovl_01/save_menu", func_802230C0_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/save_menu", func_80224AFC_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/save_menu", func_80225358_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/save_menu", func_80225960_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/save_menu", func_80226190_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/save_menu", func_8022644C_ovl_01);
