#include "common.h"
#include <lib/libnusys/nusys.h>
#include "asset.h"
#include "character_slot.h"
#include "controller.h"
#include "hole_loader.h"
#include "sound.h"
#include "transfer_pak.h"

typedef struct Unk800FE450 {
    /* 0x000 */ u8 unk0[0x75C];
    /* 0x75C */ s32 unk75C[GAME_BOY_CHARACTER_COUNT];
} Unk800FE450;

extern Unk800FE450 D_800FE450;
extern s32 sHasGameBoyPak;
extern s32 D_800FE4AC;
extern s32 D_800FE4B0;
extern s32 D_800FE4B4;
extern s32 D_800FE4B8;
extern s32 D_800FEC00;
/* Set until the boot logos, finding a Game Boy Pak, have once gone to the Game Boy Pak transfer module */
extern s32 sIsBootTransmitPending;
extern s32 sHasRumblePak[MAXCONTROLLERS];

#define HELP_BOARD_IMAGE_HAND 1
#define HELP_BOARD_IMAGE_ARROWS 2
#define HELP_BOARD_IMAGE_CHALKBOARD 3
#define ASSET_HELP_BOARD_HAND 0x70F
#define ASSET_HELP_BOARD_ARROWS 0x720
#define ASSET_HELP_BOARD_CHALKBOARD 0x833

/* The help board lists its topics, the play modes and the record terms, this many at a time */
#define HELP_TOPIC_COUNT 17

#define HAND_SIZE 24
#define HAND_LEFT 70
/* How far the hand swings right and back */
#define HAND_SWING 8
#define ARROW_COUNT 2
/* A 16x16 arrow takes four bits a texel */
#define ARROW_IMAGE_SIZE (ARROW_SIZE * ARROW_SIZE / 2)
/* The arrow image holds each animation frame this many bytes apart */
#define ARROW_FRAME_SIZE 0x200
#define ARROW_UP 0
#define ARROW_DOWN 1
#define ARROW_SIZE 16
#define ARROW_LEFT 152
#define ARROW_UP_TOP 54
#define ARROW_DOWN_TOP 200
/* The arrows change frame every this many frames */
#define ARROW_FRAME_TIME 4
#define LIST_LEFT 160

#define DEGREES_TO_RADIANS(degrees) ((degrees) * (M_PI / 180))

/* A texture rectangle step of one texel per pixel, in 5.10 fixed point */
#define TEXEL_STEP_ONE (1 << 10)

/* Encoded images start with a header, then a CI palette, then texels */
#define IMAGE_HEADER_SIZE 8
/* Each palette colour is one RGBA16 value */
#define PALETTE_COLOR_SIZE 2
#define SMALL_PALETTE_COLOR_COUNT 16
#define LARGE_PALETTE_COLOR_COUNT 256
/* Where a four-bit image's texels start, after its header and palette */
#define PALETTED_TEXELS_OFFSET (IMAGE_HEADER_SIZE + SMALL_PALETTE_COLOR_COUNT * PALETTE_COLOR_SIZE)
#define CHALKBOARD_PALETTE_SIZE (LARGE_PALETTE_COLOR_COUNT * PALETTE_COLOR_SIZE)
#define CHALKBOARD_TEXELS_OFFSET (IMAGE_HEADER_SIZE + CHALKBOARD_PALETTE_SIZE)
#define CHALKBOARD_WIDTH 256
#define CHALKBOARD_STRIP_HEIGHT 8
#define CHALKBOARD_STRIP_COUNT 20
/* Each eight-row strip is drawn nine rows tall */
#define CHALKBOARD_DRAWN_STRIP_HEIGHT 9
#define CHALKBOARD_LEFT 36
#define CHALKBOARD_TOP 40

#define UNUSED_IMAGE_WIDTH 128
#define UNUSED_IMAGE_HEIGHT 64
#define UNUSED_IMAGE_LEFT 40
#define UNUSED_IMAGE_TOP 50
#define UNUSED_IMAGE_RIGHT 280
#define UNUSED_IMAGE_BOTTOM 230

#define HELP_TEXT_SLOT_COUNT 7
#define HELP_TEXT_SLOT_TITLE 0
#define HELP_TEXT_SLOT_PAGE 1
/* Given this as its text, func_800719A0 clears the slot */
#define NO_TEXT (-1)
#define TOPIC_TITLE_X 48
#define TOPIC_TITLE_Y 72
#define TOPIC_PAGE_X 52
#define TOPIC_PAGE_Y 105
#define LISTED_TOPIC_COUNT 5

typedef struct MenuMemory {
    /* 0x00 */ u8 unk0[0x20];
    /* 0x20 */ s8 helpTopicIndex;
    /* 0x21 */ u8 unk21[0x46 - 0x21];
    /* 0x46 */ u8 unk46;
} MenuMemory;

MenuMemory *func_8005AF74(void);
s32 func_800959F8(void);
void func_8009226C(s32);
void func_800718C4(void);
void func_800718F4(s32 red, s32 green, s32 blue, s32 colorIndex);
void func_800719A0(s32 x, s32 y, s32 image, s32, s32 slot, s32, s32, s32, s32, s32);
void func_80071C74(s32 slot);
s32 func_80071C9C(s32 slot);

extern s32 D_800C2000;
extern s32 D_800FE334;
extern s32 D_801EFFA0;
extern void *sHandImages;
extern void *sArrowImages;
extern void *sUnusedImage;
extern void *sChalkboardImage;
/* The open topic, counted from 1, or 0 while the list shows */
extern s8 sOpenTopic;
extern s8 sTopicPage;
extern s32 sHandOffset;
extern s32 sIsHandReturning;
extern s32 sArrowFrame;
extern s32 sArrowFrameTime;
extern s32 D_800C42F4;
extern s32 D_800C42F8;
extern s32 sFirstListedTopic;
extern f32 sListColorPhase;

typedef struct TextListLayout {
    /* 0x0 */ s32 top;
    /* 0x4 */ s32 rowSpacing;
} TextListLayout;

extern TextListLayout sHelpListLayout;

void func_801FB54C(Gfx **displayListHead, s32);
void func_80074968(Gfx **displayListHead);
void func_80074CA8(Gfx **displayListHead, s32 red, s32 green, s32 blue);
void DrawChalkboard(Gfx **displayListHead);
/* Indexed by topic; each topic's pages are the messages that follow its title */
extern s8 sTopicPageCounts[];
extern s16 sTopicTitleMessages[];

/* Adds experience that a Game Boy character earned, up to what the Game Boy Color version holds */
void AddGameBoyExperience(s32 characterIndex, s32 experience) {
    g_GameBoyCharacters[characterIndex].experience += experience;
    if (g_GameBoyCharacters[characterIndex].experience > MAX_GAME_BOY_EXPERIENCE) {
        g_GameBoyCharacters[characterIndex].experience = MAX_GAME_BOY_EXPERIENCE;
    }
}

s32 HasGameBoyPak(void) {
    return sHasGameBoyPak;
}

s32 IsGameBoyCharacterPresent(s32 characterIndex) {
    return g_GameBoyCharacters[characterIndex].status == GAME_BOY_CHARACTER_STATUS_PRESENT;
}

u8 *GetGameBoyCharacterName(s32 characterIndex) {
    return g_GameBoyCharacters[characterIndex].name;
}

/* Records the transfer mode; mode 0 also empties every Game Boy character slot */
#ifdef NON_MATCHING
void ResetGameBoyTransfer(s32 transferMode) {
    s32 index;

    D_800FE4B0 = transferMode;
    D_800FE4AC = 0;
    if (transferMode == 0) {
        for (index = 0; index != GAME_BOY_CHARACTER_COUNT; index++) {
            g_GameBoyCharacters[index].status = 0;
            g_GameBoyCharacters[index].unk34 = -1;
            g_GameBoyCharacters[index].unk38 = -1;
            ((s32 *)&D_800FE450)[index + 0x75C / 4] = -1;
        }
        D_800FE4B4 = 0;
        D_800FEC00 = 0;
        D_800FE4B8 = 0;
    } else {
        D_800FE4B4 = 0;
        D_800FEC00 = 0;
    }
}

#else
INCLUDE_ASM("asm/nonmatchings/rumble", ResetGameBoyTransfer);
#endif

void ClearBootTransmitPending(void) {
    sIsBootTransmitPending = FALSE;
}

s32 IsBootTransmitPending(void) {
    return sIsBootTransmitPending;
}

/* Sets sHasGameBoyPak if any controller holds a Transfer Pak with a Game Boy cartridge in it */
void DetectGameBoyPaks(void) {
    NUContPakFile file;
    s32 index;

    nuContGBPakMgrInit();
    sHasGameBoyPak = FALSE;
    for (index = 0; index < g_ControllerCount; index++) {
        switch (nuContGBPakOpen(&file, g_PlayerControllerPorts[index])) {
            case 0:
                osSyncPrintf("GB PACK OK %d\n", index);
                sHasGameBoyPak = TRUE;
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
            sHasRumblePak[index] = FALSE;
        } else if (result == PFS_ERR_DEVICE) {
            osSyncPrintf("PFS_ERR_DEVICE\n");
            sHasRumblePak[index] = FALSE;
        } else if (result == PFS_ERR_CONTRFAIL) {
            osSyncPrintf("PFS_ERR_CONTRFAIL\n");
            sHasRumblePak[index] = FALSE;
        } else {
            sHasRumblePak[index] = TRUE;
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

/* Never called; references the Game Boy Pak functions, perhaps so that they are linked */
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

/* Reads one of the help board's images into buffer */
void LoadHelpBoardImage(void *buffer, s32 image) {
    EncodedAsset asset;
    MenuMemory *memory = func_8005AF74();

    switch (image) {
        case HELP_BOARD_IMAGE_HAND:
            sHandImages = buffer;
            OpenEncodedAsset(ASSET_HELP_BOARD_HAND, &asset);
            ReadEncodedAsset(sHandImages, &asset);
            break;
        case HELP_BOARD_IMAGE_ARROWS:
            sArrowImages = buffer;
            OpenEncodedAsset(ASSET_HELP_BOARD_ARROWS, &asset);
            ReadEncodedAsset(sArrowImages, &asset);
            break;
        case HELP_BOARD_IMAGE_CHALKBOARD:
            sChalkboardImage = buffer;
            OpenEncodedAsset(ASSET_HELP_BOARD_CHALKBOARD, &asset);
            ReadEncodedAsset(sChalkboardImage, &asset);
            break;
    }
    sOpenTopic = 0;
    sTopicPage = 0;
    sHandOffset = 0;
    sIsHandReturning = FALSE;
    sArrowFrame = 0;
    sArrowFrameTime = 0;
    D_800C42F4 = 0;
    D_800C42F8 = 0;
    if (memory->helpTopicIndex < LISTED_TOPIC_COUNT) {
        sFirstListedTopic = 0;
    } else if (memory->helpTopicIndex + LISTED_TOPIC_COUNT - 1 < HELP_TOPIC_COUNT - 1) {
        sFirstListedTopic = memory->helpTopicIndex;
    } else {
        sFirstListedTopic = HELP_TOPIC_COUNT - LISTED_TOPIC_COUNT;
    }
    func_800718C4();
}

/* Clears every text slot of the help board */
#define CLEAR_HELP_TEXT(index)                                                                       \
    for (index = 0; index != HELP_TEXT_SLOT_COUNT; index++) {                                      \
        func_800719A0(0, 0, NO_TEXT, 1, index, 1, 0xFF, 0xFF, 0xFF, 0xFF);                         \
    }

#define SET_HELP_TEXT_COLORS()                                                                       \
    func_800718F4(0xFF, 0xFF, 0, 0);                                                                 \
    func_800718F4(0xFF, 0xFF, 0, 1);                                                                 \
    func_800718F4(0xFF, 0x7F, 0x7F, 2);                                                              \
    func_800718F4(0x3F, 0xFF, 0x3F, 3)

/* Shows the open topic's title and its current page */
#define SHOW_TOPIC_PAGE()                                                                            \
    func_800719A0(0, 0, NO_TEXT, 1, HELP_TEXT_SLOT_TITLE, 1, 0xFF, 0xFF, 0xFF, 0xFF);               \
    func_800719A0(0, 0, NO_TEXT, 1, HELP_TEXT_SLOT_PAGE, 1, 0xFF, 0xFF, 0xFF, 0xFF);                \
    SET_HELP_TEXT_COLORS();                                                                          \
    func_800719A0(TOPIC_TITLE_X, TOPIC_TITLE_Y, sTopicTitleMessages[sOpenTopic], 1, HELP_TEXT_SLOT_TITLE, 2, \
                  0xFF, 0xFF, 0xFF, 0xFF);                                                           \
    SET_HELP_TEXT_COLORS();                                                                          \
    func_800719A0(TOPIC_PAGE_X, TOPIC_PAGE_Y, sTopicTitleMessages[sOpenTopic] + sTopicPage, 1,       \
                  HELP_TEXT_SLOT_PAGE, 3, 0xFF, 0xFF, 0xFF, 0xFF)

/* Opens and pages through topics with A, closes them or leaves with B, and moves through the list */
void UpdateHelpBoard(void) {
    s32 index;
    MenuMemory *memory = func_8005AF74();

    if (func_800959F8() != 1) {
        return;
    }
    if (D_800C2000 & A_BUTTON) {
        if (sOpenTopic == 0) {
            sOpenTopic = memory->helpTopicIndex + 1;
            CLEAR_HELP_TEXT(index);
            PlaySound(0x5D, 0xF, 0x50);
            sTopicPage++;
            SHOW_TOPIC_PAGE();
        } else if (sTopicPage < sTopicPageCounts[sOpenTopic]) {
            if (func_80071C9C(HELP_TEXT_SLOT_PAGE)) {
                PlaySound(0x5D, 0xF, 0x50);
                sTopicPage++;
                SHOW_TOPIC_PAGE();
            } else {
                func_80071C74(HELP_TEXT_SLOT_TITLE);
                func_80071C74(HELP_TEXT_SLOT_PAGE);
            }
        } else if (func_80071C9C(HELP_TEXT_SLOT_PAGE)) {
            CLEAR_HELP_TEXT(index);
            sOpenTopic = 0;
            sTopicPage = 0;
        } else {
            func_80071C74(HELP_TEXT_SLOT_TITLE);
            func_80071C74(HELP_TEXT_SLOT_PAGE);
        }
    } else if (D_800C2000 & B_BUTTON) {
        PlaySound(0x5F, 0xF, 0x7F);
        if (sOpenTopic <= 0) {
            func_8009226C(9);
            if (memory->unk46 == 0) {
                D_801EFFA0 = 0x1C;
            } else {
                D_801EFFA0 = 2;
            }
            StopLoaderThread();
            FreeAllCharacterSlots();
            D_800FE334 = 0;
            sOpenTopic = 0;
            CLEAR_HELP_TEXT(index);
        } else if (func_80071C9C(HELP_TEXT_SLOT_TITLE) && func_80071C9C(HELP_TEXT_SLOT_PAGE)) {
            sOpenTopic = 0;
            sTopicPage = 0;
            CLEAR_HELP_TEXT(index);
        } else {
            func_80071C74(HELP_TEXT_SLOT_TITLE);
            func_80071C74(HELP_TEXT_SLOT_PAGE);
        }
    } else if (D_800C2000 & U_JPAD) {
        if (sOpenTopic == 0) {
            PlaySound(0x5C, 0xF, 0x64);
            if (sFirstListedTopic == memory->helpTopicIndex) {
                if (sFirstListedTopic != 0) {
                    sFirstListedTopic--;
                    memory->helpTopicIndex--;
                }
            } else {
                memory->helpTopicIndex--;
            }
            CLEAR_HELP_TEXT(index);
        }
    } else if (D_800C2000 & D_JPAD) {
        if (sOpenTopic == 0) {
            PlaySound(0x5C, 0xF, 0x64);
            if (sFirstListedTopic + LISTED_TOPIC_COUNT - 1 == memory->helpTopicIndex) {
                if (sFirstListedTopic + LISTED_TOPIC_COUNT - 1 < HELP_TOPIC_COUNT - 1) {
                    sFirstListedTopic++;
                    memory->helpTopicIndex++;
                }
            } else {
                memory->helpTopicIndex++;
            }
            CLEAR_HELP_TEXT(index);
        }
    }
}

/* Draws the chalkboard, the topic list with its hand and arrows, and the text */
#ifdef NON_MATCHING
void DrawHelpBoard(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    MenuMemory *memory = func_8005AF74();
    s32 index;
    s32 handTop;
    s32 handLeft;
    s32 arrowTop;
    s32 arrowLeft;
    s32 red;
    s32 green;
    s32 blue;
    s32 topic;

    func_801FB54C(&gfx, 0xB);
    DrawChalkboard(&gfx);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    if (sOpenTopic == 0) {
        gDPPipeSync(gfx++);
        gDPSetTextureFilter(gfx++, G_TF_BILERP);
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_RGBA16);
        gDPLoadTLUT_pal16(gfx++, 0, ((u32)sHandImages + IMAGE_HEADER_SIZE) & ~7);
        gDPLoadTextureTile_4b(gfx++, ((u32)sHandImages + PALETTED_TEXELS_OFFSET) & ~7, G_IM_FMT_CI, HAND_SIZE, 0, 0,
                              0, HAND_SIZE - 1, HAND_SIZE - 1, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP,
                              G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        gDPPipeSync(gfx++);
        if (sIsHandReturning == FALSE) {
            sHandOffset++;
            if (sHandOffset == HAND_SWING) {
                sIsHandReturning = TRUE;
            }
        } else if (sIsHandReturning == TRUE) {
            sHandOffset--;
            if (sHandOffset == 0) {
                sIsHandReturning = FALSE;
            }
        } else {
            sHandOffset = 0;
            sIsHandReturning = FALSE;
        }
        handLeft = sHandOffset + HAND_LEFT;
        handTop = sHelpListLayout.top + (memory->helpTopicIndex - sFirstListedTopic) * sHelpListLayout.rowSpacing;
        gSPScisTextureRectangle(gfx++, handLeft << 2, handTop << 2, (handLeft + HAND_SIZE) << 2,
                                (handTop + HAND_SIZE) << 2,
                                G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
        gDPPipeSync(gfx++);
        sArrowFrameTime++;
        if (sArrowFrameTime == ARROW_FRAME_TIME) {
            sArrowFrameTime = 0;
            sArrowFrame ^= 1;
        }
        if (D_800C42F4 == 0) {
            D_800C42F8 += 2;
            if (D_800C42F8 == 8) {
                D_800C42F4 = 1;
            }
        } else if (D_800C42F4 == 1) {
            D_800C42F8 -= 2;
            if (D_800C42F8 == 0) {
                D_800C42F4 = 0;
            }
        } else {
            D_800C42F8 = 0;
            D_800C42F4 = 0;
        }
        gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
        gDPLoadTLUT_pal16(gfx++, 0, ((u32)sArrowImages + IMAGE_HEADER_SIZE) & ~7);
        for (index = 0; index != ARROW_COUNT; index++) {
            if ((index == ARROW_UP && sFirstListedTopic > 0) ||
                (index == ARROW_DOWN && sFirstListedTopic < HELP_TOPIC_COUNT - LISTED_TOPIC_COUNT)) {
                gDPLoadTextureTile_4b(gfx++,
                                      (u32)((u8 *)sArrowImages + (index * ARROW_IMAGE_SIZE +
                                                                  sArrowFrame * ARROW_FRAME_SIZE +
                                                                  PALETTED_TEXELS_OFFSET)) & ~7,
                                      G_IM_FMT_CI, ARROW_SIZE, 0, 0, 0, ARROW_SIZE - 1, ARROW_SIZE - 1, 0,
                                      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
                                      G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
                gDPPipeSync(gfx++);
                arrowLeft = ARROW_LEFT;
                arrowTop = ARROW_DOWN_TOP;
                if (index == ARROW_UP) {
                    arrowTop = ARROW_UP_TOP;
                }
                gSPScisTextureRectangle(gfx++, arrowLeft << 2, arrowTop << 2, (arrowLeft + ARROW_SIZE) << 2,
                                        (arrowTop + ARROW_SIZE) << 2, G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE,
                                        TEXEL_STEP_ONE);
                gDPPipeSync(gfx++);
            }
        }
    }
    gDPPipeSync(gfx++);
    func_80074968(&gfx);
    func_80074CA8(&gfx, 0xFF, 0xFF, 0xFF);
    if (sOpenTopic == 0) {
        for (index = 0, topic = sFirstListedTopic; index != LISTED_TOPIC_COUNT; index++, topic++) {
            sListColorPhase += DEGREES_TO_RADIANS(5);
            red = (s32)(sinf(sListColorPhase) * 127.0f) + 127;
            blue = (s32)(sinf(sListColorPhase + M_PI / 2) * 127.0f) + 127;
            green = (s32)(sinf(sListColorPhase + M_PI) * 127.0f) + 127;
            if (topic == memory->helpTopicIndex) {
                func_800719A0(LIST_LEFT, sHelpListLayout.top + index * sHelpListLayout.rowSpacing,
                              sTopicTitleMessages[0] + topic, 1, index, 0x10, red, green, blue, 0xFF);
            } else {
                func_800719A0(LIST_LEFT, sHelpListLayout.top + index * sHelpListLayout.rowSpacing,
                              sTopicTitleMessages[sOpenTopic] + topic, 1, index, 0x10, 0xFF, 0xFF, 0xFF, 0xFF);
            }
        }
    }
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

#else
INCLUDE_ASM("asm/nonmatchings/rumble", DrawHelpBoard);
#endif

/* Draws a tinted intensity image across the board; nothing calls this or loads its image */
void DrawUnusedHelpBoardImage(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPSetPrimColor(gfx++, 0, 0, 0, 220, 230, 210);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
    gDPSetCombineLERP(gfx++, TEXEL0, 0, PRIMITIVE, 0, PRIMITIVE, 0, TEXEL0, 0, TEXEL0, 0, PRIMITIVE, 0, PRIMITIVE, 0,
                      TEXEL0, 0);
    gDPLoadTextureBlock_4b(gfx++, ((u32)sUnusedImage + IMAGE_HEADER_SIZE) & ~7, G_IM_FMT_I, UNUSED_IMAGE_WIDTH,
                           UNUSED_IMAGE_HEIGHT, 0, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK,
                           G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gSPTextureRectangle(gfx++, UNUSED_IMAGE_LEFT << 2, UNUSED_IMAGE_TOP << 2, UNUSED_IMAGE_RIGHT << 2,
                        UNUSED_IMAGE_BOTTOM << 2, G_TX_RENDERTILE, 0, 0,
                        (UNUSED_IMAGE_WIDTH << 10) / (UNUSED_IMAGE_RIGHT - UNUSED_IMAGE_LEFT),
                        (UNUSED_IMAGE_HEIGHT << 10) / (UNUSED_IMAGE_BOTTOM - UNUSED_IMAGE_TOP));
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

/* Draws the chalkboard in strips, stretching each strip a little taller */
void DrawChalkboard(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 stripIndex;
    s32 stripTop;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_OPA_SURF | AA_EN | CVG_X_ALPHA | ALPHA_CVG_SEL, G_RM_OPA_SURF2);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    gDPPipeSync(gfx++);
    gDPLoadTLUT_pal256(gfx++, ((u32)sChalkboardImage + IMAGE_HEADER_SIZE) & ~7);
    for (stripIndex = 0; stripIndex != CHALKBOARD_STRIP_COUNT; stripIndex++) {
        gDPLoadTextureBlock(gfx++,
                            ((u32)sChalkboardImage + CHALKBOARD_TEXELS_OFFSET +
                             stripIndex * CHALKBOARD_WIDTH * CHALKBOARD_STRIP_HEIGHT) & ~7,
                            G_IM_FMT_CI, G_IM_SIZ_8b, CHALKBOARD_WIDTH, CHALKBOARD_STRIP_HEIGHT, 0,
                            G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                            G_TX_NOLOD, G_TX_NOLOD);
        gDPPipeSync(gfx++);
        stripTop = CHALKBOARD_TOP + stripIndex * CHALKBOARD_DRAWN_STRIP_HEIGHT;
        gSPScisTextureRectangle(gfx++, CHALKBOARD_LEFT << 2,
                                stripTop << 2,
                                (CHALKBOARD_LEFT + CHALKBOARD_WIDTH) << 2,
                                (stripTop + CHALKBOARD_DRAWN_STRIP_HEIGHT) << 2,
                                G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE,
                                (CHALKBOARD_STRIP_HEIGHT << 10) / CHALKBOARD_DRAWN_STRIP_HEIGHT);
        gDPPipeSync(gfx++);
    }
    *displayListHead = gfx;
}

