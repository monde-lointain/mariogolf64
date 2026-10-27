#include "common.h"
#include "asset.h"
#include "character.h"
#include "course.h"
#include "game_flags.h"
#include "libc.h"
#include "menu_memory.h"
#include "round_hud.h"
#include "save.h"
#include "sram.h"

/* The image listing the game modes, in orange */
/* The buffers of func_802218C0, and what it reads into them */
#define SAVE_GAME_BUFFER_UNK1 1
#define SAVE_GAME_BUFFER_PLAYER_LABELS 2
#define SAVE_GAME_BUFFER_UNK3 3
#define SAVE_GAME_BUFFER_UNK4 4
#define SAVE_GAME_BUFFER_UNK5 5
#define SAVE_GAME_BUFFER_COURSE 6
#define SAVE_GAME_BUFFER_CHOSEN_COURSE 7

/*
 * The saved game's 64 by 64 course image, shrunk in two strips; func_80226190
 * draws one with its own palette, func_8022644C one without
 */
#define COURSE_THUMBNAIL_WIDTH 64
#define COURSE_THUMBNAIL_STRIP_HEIGHT 32
#define COURSE_THUMBNAIL_STRIP_COUNT 2
#define COURSE_THUMBNAIL_LEFT 40
#define COURSE_THUMBNAIL_TOP 182
#define COURSE_THUMBNAIL_SHOWN_WIDTH 52
#define COURSE_THUMBNAIL_SHOWN_HEIGHT 18
#define ASSET_SAVE_GAME_UNK1 0x63D
#define ASSET_SAVE_GAME_UNK3 0x82A
#define ASSET_SAVE_GAME_UNK4 0x82B
#define ASSET_SAVE_GAME_UNK5 0x82C

/* The games that func_80222CDC continues from the save's mode tables, by the save menu's slot */
#define SAVE_MENU_GAME_TOURNAMENT 0
#define SAVE_MENU_GAME_GET_CHARACTER 1
#define SAVE_MENU_GAME_STROKE 2

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
/* The 64x64 CI8 icons of D_8023B680, which func_802213C8 shrinks to 32x24 in two strips */
#define SAVE_ICON_WIDTH 64
#define SAVE_ICON_STRIP_HEIGHT 32
#define SAVE_ICON_STRIP_COUNT 2
#define SAVE_ICON_SHOWN_WIDTH 32
#define SAVE_ICON_SHOWN_STRIP_HEIGHT 12

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
extern u16 D_800C208C[];
extern s16 D_800C20D4[];
extern void *D_801052C4[2];
extern void *D_8023BD80_ovl_01;
extern void *D_8023BD84_ovl_01;
extern void *D_8023BD8C_ovl_01;
extern void *D_8023BD90_ovl_01;
extern void *D_8023BD98_ovl_01;
extern void *D_8023BD9C_ovl_01;
extern void *D_8023BDA0_ovl_01;
extern u8 D_80230B4E_ovl_01;
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
extern Unk68 D_800FEDD8[MAX_PLAYER_COUNT];
extern s8 D_801B60B8;
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
    if (D_8023B684_ovl_01[memory->ringShotReplaySlot] == -1) {
        for (slot = 0; slot != SAVE_MENU_SLOT_COUNT; slot++) {
            if (D_8023B684_ovl_01[slot] != -1) {
                memory->ringShotReplaySlot = slot;
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
    return D_8023B684_ovl_01[func_8005AF74()->ringShotReplaySlot];
}

s32 func_80220E28_ovl_01(Unk68 *record) {
    return (u8)record->unk36 >> 7;
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/save_menu", func_80220E34_ovl_01);

void func_802213C8_ovl_01(Gfx **displayListHead, s32 imageIndex, s32 left, s32 top) {
    Gfx *gfx = *displayListHead;
    s32 stripIndex;

    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal256(gfx++, ((u32)D_8023B680_ovl_01[imageIndex] + IMAGE_HEADER_SIZE) & ~7);
    for (stripIndex = 0; stripIndex != SAVE_ICON_STRIP_COUNT; stripIndex++) {
        gDPLoadTextureBlock(gfx++,
                            ((u32)D_8023B680_ovl_01[imageIndex] + LARGE_PALETTED_TEXELS_OFFSET +
                             stripIndex * SAVE_ICON_WIDTH * SAVE_ICON_STRIP_HEIGHT) & ~7,
                            G_IM_FMT_CI, G_IM_SIZ_8b, SAVE_ICON_WIDTH, SAVE_ICON_STRIP_HEIGHT, 0,
                            G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                            G_TX_NOLOD, G_TX_NOLOD);
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(gfx++, left << 2, (top + stripIndex * SAVE_ICON_SHOWN_STRIP_HEIGHT) << 2,
                                (left + SAVE_ICON_SHOWN_WIDTH) << 2,
                                (top + stripIndex * SAVE_ICON_SHOWN_STRIP_HEIGHT + SAVE_ICON_SHOWN_STRIP_HEIGHT) << 2,
                                G_TX_RENDERTILE, 0, 0, (SAVE_ICON_WIDTH << 10) / SAVE_ICON_SHOWN_WIDTH,
                                (SAVE_ICON_STRIP_HEIGHT << 10) / SAVE_ICON_SHOWN_STRIP_HEIGHT);
        gDPPipeSync(gfx++);
    }
    *displayListHead = gfx;
}

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

/* Doesn't match: the original keeps the saved game's place in a saved register */
#ifdef NON_MATCHING
void func_802218C0_ovl_01(void *buffer, u8 bufferKind) {
    EncodedAsset asset;
    AssetFile file;
    SaveData *save = GetSaveData();
    MenuMemory *memory = func_8005AF74();
    s32 gameIndex;
    void **bufferSlot;
    void **imageSlot;
    s32 miniGolfIndex;

    switch (bufferKind) {
        case SAVE_GAME_BUFFER_UNK1:
            D_8023BD80_ovl_01 = buffer;
            OpenEncodedAsset(ASSET_SAVE_GAME_UNK1, &asset);
            ReadEncodedAsset(D_8023BD80_ovl_01, &asset);
            break;
        case SAVE_GAME_BUFFER_PLAYER_LABELS:
            D_8023BD84_ovl_01 = buffer;
            OpenEncodedAsset(ASSET_PLAYER_LABELS, &asset);
            ReadEncodedAsset(D_8023BD84_ovl_01, &asset);
            break;
        case SAVE_GAME_BUFFER_UNK3:
            imageSlot = &D_8023BD98_ovl_01;
            *imageSlot = buffer;
            OpenEncodedAsset(ASSET_SAVE_GAME_UNK3, &asset);
            ReadEncodedAsset(*imageSlot, &asset);
            break;
        case SAVE_GAME_BUFFER_UNK4:
            imageSlot = &D_8023BD9C_ovl_01;
            *imageSlot = buffer;
            OpenEncodedAsset(ASSET_SAVE_GAME_UNK4, &asset);
            ReadEncodedAsset(*imageSlot, &asset);
            break;
        case SAVE_GAME_BUFFER_UNK5:
            imageSlot = &D_8023BDA0_ovl_01;
            *imageSlot = buffer;
            OpenEncodedAsset(ASSET_SAVE_GAME_UNK5, &asset);
            ReadEncodedAsset(*imageSlot, &asset);
            break;
        case SAVE_GAME_BUFFER_COURSE:
            bufferSlot = &D_8023BD8C_ovl_01;
            *bufferSlot = buffer;
            gameIndex = memory->savedGameIndex;
            if (gameIndex < 0) {
                gameIndex = 0;
            }
            if (gameIndex >= SAVED_GAME_COUNT) {
                gameIndex = SAVED_GAME_COUNT - 1;
            }
            if (!save->savedGames[gameIndex].isValid) {
                break;
            }
            if (!save->savedGames[gameIndex].state.isMiniGolf) {
                OpenEncodedAsset(D_800C2064[save->savedGames[gameIndex].state.courseIndex], &asset);
                ReadEncodedAsset(*bufferSlot, &asset);
            } else {
                miniGolfIndex = save->savedGames[gameIndex].state.courseIndex - MINI_GOLF_COURSE_INDEX;
                OpenEncodedAsset(D_800C208C[miniGolfIndex], &asset);
                ReadEncodedAsset(*bufferSlot, &asset);
                OpenAssetFile(D_800C20D4[miniGolfIndex], &file);
                ReadAssetFile(D_801052C4[0], GetAssetFileSize(&file), &file);
            }
            break;
        case SAVE_GAME_BUFFER_CHOSEN_COURSE:
            bufferSlot = &D_8023BD90_ovl_01;
            *bufferSlot = buffer;
            gameIndex = memory->savedGameIndex;
            if (gameIndex < 0) {
                gameIndex = 0;
            }
            if (gameIndex >= SAVED_GAME_COUNT) {
                gameIndex = SAVED_GAME_COUNT - 1;
            }
            if (!save->savedGames[gameIndex].isValid) {
                break;
            }
            if (!save->savedGames[gameIndex].state.isMiniGolf) {
                OpenEncodedAsset(D_800C2064[save->savedGames[gameIndex].state.courseIndex], &asset);
                ReadEncodedAsset(*bufferSlot, &asset);
            } else {
                miniGolfIndex = save->savedGames[gameIndex].state.courseIndex - MINI_GOLF_COURSE_INDEX;
                OpenEncodedAsset(D_800C208C[miniGolfIndex], &asset);
                ReadEncodedAsset(*bufferSlot, &asset);
                OpenAssetFile(D_800C20D4[miniGolfIndex], &file);
                ReadAssetFile(D_801052C4[0], GetAssetFileSize(&file), &file);
            }
            break;
    }
    D_80230B4E_ovl_01 = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/save_menu", func_802218C0_ovl_01);
#endif

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

s32 func_80222CDC_ovl_01(void) {
    SaveData *save = GetSaveData();
    MenuMemory *memory = func_8005AF74();
    SavedGame *game;
    UnkB8 *players;
    Unk68 *records;
    s32 playerCount = 0;
    s32 index;

    switch (memory->savedGameIndex) {
        case SAVE_MENU_GAME_TOURNAMENT:
            game = &save->modeGames[MODE5_GAME_INDEX];
            records = &save->modeRecords[MODE5_RECORD_INDEX];
            players = &save->modePlayers[MODE5_PLAYER_INDEX];
            playerCount = MODE5_PLAYER_COUNT;
            break;
        case SAVE_MENU_GAME_GET_CHARACTER:
            game = &save->modeGames[MODE4_GAME_INDEX];
            records = &save->modeRecords[MODE4_RECORD_INDEX];
            players = &save->modePlayers[MODE4_PLAYER_INDEX];
            playerCount = MODE4_PLAYER_COUNT;
            break;
        case SAVE_MENU_GAME_STROKE:
            game = &save->modeGames[MODE0_GAME_INDEX];
            records = &save->modeRecords[MODE0_RECORD_INDEX];
            players = &save->modePlayers[MODE0_PLAYER_INDEX];
            playerCount = MODE0_PLAYER_COUNT;
            break;
    }
    if (!game->isValid) {
        return -1;
    }
    if (game->state.playerCount > playerCount) {
        return -1;
    }
    playerCount = game->state.playerCount;
    for (index = 0; index != playerCount; index++) {
        g_Players[index] = players[index];
        D_800FEDD8[index] = records[index];
    }
    g_GameState = game->state;
    g_GameState.unk0 = 1;
    SetGameFlag(GAME_FLAG_UNK1E);
    D_800BA9D8 = game->unk8;
    D_801B60B8 = 2;
    return 0;
}

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

void func_80226190_ovl_01(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 stripIndex;
    s32 stripTop;

    GetSaveData();
    func_8005AF74();
    gDPLoadTLUT_pal256(gfx++, ((u32)(&D_8023BD8C_ovl_01)[D_80230B4E_ovl_01] + IMAGE_HEADER_SIZE) & ~7);
    for (stripIndex = 0; stripIndex != COURSE_THUMBNAIL_STRIP_COUNT; stripIndex++) {
        gDPLoadTextureBlock(gfx++,
                            ((u32)(&D_8023BD8C_ovl_01)[D_80230B4E_ovl_01] + LARGE_PALETTED_TEXELS_OFFSET +
                             stripIndex * COURSE_THUMBNAIL_WIDTH * COURSE_THUMBNAIL_STRIP_HEIGHT) & ~7,
                            G_IM_FMT_CI, G_IM_SIZ_8b, COURSE_THUMBNAIL_WIDTH, COURSE_THUMBNAIL_STRIP_HEIGHT, 0,
                            G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                            G_TX_NOLOD, G_TX_NOLOD);
        gDPPipeSync(gfx++);
        stripTop = COURSE_THUMBNAIL_TOP + stripIndex * COURSE_THUMBNAIL_SHOWN_HEIGHT;
        gSPScisTextureRectangle(gfx++, COURSE_THUMBNAIL_LEFT << 2, stripTop << 2,
                                (COURSE_THUMBNAIL_LEFT + COURSE_THUMBNAIL_SHOWN_WIDTH) << 2,
                                (stripTop + COURSE_THUMBNAIL_SHOWN_HEIGHT) << 2, G_TX_RENDERTILE, 0, 0,
                                (COURSE_THUMBNAIL_WIDTH << 10) / COURSE_THUMBNAIL_SHOWN_WIDTH,
                                (COURSE_THUMBNAIL_STRIP_HEIGHT << 10) / COURSE_THUMBNAIL_SHOWN_HEIGHT);
        gDPPipeSync(gfx++);
    }
    *displayListHead = gfx;
}

void func_8022644C_ovl_01(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 stripIndex;
    s32 stripTop;
    void *palette;

    GetSaveData();
    func_8005AF74();
    palette = D_801052C4[0];
    gDPLoadTLUT_pal256(gfx++, (u32)palette & ~7);
    for (stripIndex = 0; stripIndex != COURSE_THUMBNAIL_STRIP_COUNT; stripIndex++) {
        /* The course and chosen course buffers are adjacent; D_80230B4E picks one */
        gDPLoadTextureBlock(gfx++,
                            ((u32)(&D_8023BD8C_ovl_01)[D_80230B4E_ovl_01] +
                             stripIndex * COURSE_THUMBNAIL_WIDTH * COURSE_THUMBNAIL_STRIP_HEIGHT) & ~7,
                            G_IM_FMT_CI, G_IM_SIZ_8b, COURSE_THUMBNAIL_WIDTH, COURSE_THUMBNAIL_STRIP_HEIGHT, 0,
                            G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                            G_TX_NOLOD, G_TX_NOLOD);
        gDPPipeSync(gfx++);
        stripTop = COURSE_THUMBNAIL_TOP + stripIndex * COURSE_THUMBNAIL_SHOWN_HEIGHT;
        gSPScisTextureRectangle(gfx++, COURSE_THUMBNAIL_LEFT << 2, stripTop << 2,
                                (COURSE_THUMBNAIL_LEFT + COURSE_THUMBNAIL_SHOWN_WIDTH) << 2,
                                (stripTop + COURSE_THUMBNAIL_SHOWN_HEIGHT) << 2, G_TX_RENDERTILE, 0, 0,
                                (COURSE_THUMBNAIL_WIDTH << 10) / COURSE_THUMBNAIL_SHOWN_WIDTH,
                                (COURSE_THUMBNAIL_STRIP_HEIGHT << 10) / COURSE_THUMBNAIL_SHOWN_HEIGHT);
        gDPPipeSync(gfx++);
    }
    *displayListHead = gfx;
}
