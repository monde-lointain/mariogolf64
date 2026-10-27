#include "common.h"
#include "asset.h"
#include "character.h"
#include "game_flags.h"
#include "hole_display.h"
#include "libc.h"
#include "menu.h"
#include "menu_memory.h"
#include "save.h"
#include "screen.h"
#include "screen_layers.h"
#include "sound.h"
#include "sram.h"
#include "text.h"
#include "variables.h"

/* The images of D_800C2064 that func_8021CF10 reads */
#define PLAYER_STATS_IMAGE_COUNT 6

/* The bytes of each of the four images that func_8021CFAC reads into one buffer */
#define PLAYER_STATS_IMAGE_SIZE 0x258
/* They are Japanese score labels (assets 0x82D, 0x82F, 0x82E and 0x830), such as birdie and albatross */
#define SCORE_LABEL_COUNT 4

/* func_8021F124 writes names beside the faces of these first characters only */
#define NAMED_CHARACTER_COUNT 14

/* The most bytes of each ranked record holder's palette file that func_8021EF64 keeps */
#define PLAYER_STATS_FILE_SIZE 0x200

/* The grid of stats that the cursor moves over, and the value of its empty cells */
#define STATS_GRID_COLUMN_COUNT 5
#define STATS_GRID_ROW_COUNT 4
#define STATS_CELL_EMPTY -1
/* The bit and the kind field in a ranked record's unk3A */
#define RANKED_RECORD_SAVED 0x80
#define RANKED_RECORD_KIND_MASK 0xF
/* func_8021E01C lists each kind's records in rows one longer than the grid */
#define STATS_KIND_LIST_SIZE (STATS_GRID_COLUMN_COUNT + 1)

/* The buffers that func_802178B0 fills, and the assets it reads into them */
#define PLAYER_STATS_BUFFER_ICONS 1
#define PLAYER_STATS_BUFFER_UNK2 2
#define PLAYER_STATS_BUFFER_UNK3 3
#define PLAYER_STATS_BUFFER_UNK4 4
#define PLAYER_STATS_BUFFER_UNK5 5
#define PLAYER_STATS_BUFFER_UNK6 6
#define PLAYER_STATS_BUFFER_UNK7 7
#define PLAYER_STATS_BUFFER_UNK8 8
#define PLAYER_STATS_BUFFER_GAME_LABELS 9
#define PLAYER_STATS_BUFFER_PLAYER_LABELS 10
#define PLAYER_STATS_BUFFER_UNK11 11
#define PLAYER_STATS_BUFFER_UNK12 12
#define PLAYER_STATS_BUFFER_UNK13 13
#define PLAYER_STATS_ICON_COUNT 8
#define PLAYER_STATS_ICON_SIZE 0x218
#define ASSET_PLAYER_STATS_UNK2 0x671
#define ASSET_PLAYER_STATS_UNK7 0x720
#define ASSET_PLAYER_STATS_UNK8 0x6C4
#define ASSET_PLAYER_STATS_GAME_LABELS 0x6E8
#define GAME_LABEL_WIDTH 128
#define GAME_LABEL_HEIGHT 20
#define GAME_LABEL_IMAGE_SIZE (GAME_LABEL_WIDTH * GAME_LABEL_HEIGHT / 2)
#define ASSET_PLAYER_STATS_UNK11 0x721
#define ASSET_PLAYER_STATS_UNK12 0x828
#define ASSET_PLAYER_STATS_UNK13 0x829
#define STATS_INDEX_COLUMN_COUNT 6
/* The stats icons are 24x24, and func_80219460 shows each of their two frames for 4 video frames */
#define STATS_ICON_SIZE 24
#define STATS_ICON_FRAME_COUNT 2
#define STATS_ICON_FRAME_DURATION 4

/* The text slots that screen 12 clears on leaving */
#define STATS_TEXT_SLOT_COUNT 4

/* The animated arrows that func_8021BE88 draws either side of the page */
#define PAGE_ARROW_COUNT 2
#define PAGE_ARROW_RIGHT 1
#define PAGE_ARROW_SIZE 32
#define PAGE_ARROW_MASK 5
#define PAGE_ARROW_IMAGE_SIZE (PAGE_ARROW_SIZE * PAGE_ARROW_SIZE / 2)
#define PAGE_ARROW_FRAME_COUNT 8
#define PAGE_ARROW_FRAME_DURATION 3
#define PAGE_ARROW_LEFT_X 76
#define PAGE_ARROW_RIGHT_X 212
#define PAGE_ARROW_TOP 122

/* The blinking pair of arrows that func_8021CBDC draws above and below a point */
#define STATS_ARROW_COUNT 2
#define STATS_ARROW_SIZE 16
#define STATS_ARROW_IMAGE_SIZE (STATS_ARROW_SIZE * STATS_ARROW_SIZE / 2)
#define STATS_ARROW_BLINK_STRIDE (4 * STATS_ARROW_IMAGE_SIZE)
#define STATS_ARROW_BLINK_FRAME_COUNT 4
#define STATS_ARROW_LEFT 152
#define STATS_UP_ARROW_OFFSET (-52)
#define STATS_DOWN_ARROW_OFFSET 36

/* The directions that func_8021E340 moves the cursor in, which index D_802309C4 */
#define STATS_DIRECTION_LEFT 0
#define STATS_DIRECTION_RIGHT 1
#define STATS_DIRECTION_UP 2
#define STATS_DIRECTION_DOWN 3

/* The presets of func_80217B88; two of them have a variant that four players take instead */
#define MENU_UNKE_PRESET_COUNT 7
#define MENU_UNKE_PRESET_FIRST 0
#define MENU_UNKE_PRESET_FIRST_FOUR_PLAYER 1
#define MENU_UNKE_PRESET_SECOND 5
#define MENU_UNKE_PRESET_SECOND_FOUR_PLAYER 6

/*
 * The rows of the stats panel slide across together: D_80230784 holds their
 * offsets, and D_802307A8 holds where each slide after the first sends them
 */
#define STATS_SLIDE_ROW_COUNT 9
#define STATS_SLIDE_CURRENT 0
#define STATS_SLIDE_OUT_RIGHT 1
#define STATS_SLIDE_OUT_LEFT 2
#define STATS_SLIDE_IN_FROM_RIGHT 3
#define STATS_SLIDE_IN_FROM_LEFT 4
#define STATS_SLIDE_COUNT 5

/* The phases of the screen 20 handler, which D_8022BE88 holds */
#define MENU_PHASE_START 0
#define MENU_PHASE_CHOOSING 5
#define MENU_PHASE_CANCELLED 6
#define MENU_PHASE_CONFIRMED 7
#define MENU_UNK89_UNK4 4

/* Each player's choice on screen 20, and where the screen's slide starts */
#define PLAYER_OPTION_COUNT 37
#define PLAYER_OPTION_SLIDE_START -512

extern u16 D_800C2064[];
extern void *D_8012D430[CHARACTER_COUNT];
extern u16 D_802308A4_ovl_01[CHARACTER_COUNT];
extern s16 D_802308C8_ovl_01[CHARACTER_COUNT][CHARACTER_PALETTE_COUNT];
extern s32 D_80105B68;
extern s8 D_801B60BA[MENU_UNKE_COUNT];
extern u8 D_802304BC_ovl_01[MENU_UNKE_PRESET_COUNT][MENU_UNKE_COUNT];
extern u8 D_802304F4_ovl_01[];
extern s8 D_802309C4_ovl_01[][2];
extern void **D_8023B630_ovl_01;
extern void *D_8023B634_ovl_01;
extern void *D_8023B638_ovl_01;
extern u8 *D_8023B63C_ovl_01;
extern s32 D_80230A48_ovl_01[CHARACTER_COUNT];
extern char D_8022BF70_ovl_01[][IMPORTED_CHARACTER_NAME_LENGTH];
extern const char D_802319E0_ovl_01[];
extern void *D_8023B5E0_ovl_01[STATS_GRID_ROW_COUNT][STATS_GRID_COLUMN_COUNT];
extern s16 D_8023B640_ovl_01[][STATS_GRID_COLUMN_COUNT];
extern s32 D_80230718_ovl_01[STATS_SLIDE_ROW_COUNT];
extern s32 D_8023073C_ovl_01[STATS_SLIDE_ROW_COUNT];
extern s32 D_80230784_ovl_01[STATS_SLIDE_ROW_COUNT];
extern s32 D_802307A8_ovl_01[STATS_SLIDE_COUNT - 1][STATS_SLIDE_ROW_COUNT];
extern u8 D_80230838_ovl_01;
extern u8 D_8022BE88_ovl_01;
extern u8 D_8022BE89_ovl_01;
extern u8 D_802303A1_ovl_01;
extern s16 D_802303A6_ovl_01;
extern s16 D_802303A8_ovl_01;
extern s32 D_8023BFD0_ovl_01;
extern void *D_8023B340_ovl_01;
extern void *D_8023B344_ovl_01;
extern void *D_8023B348_ovl_01;
extern void *D_8023B34C_ovl_01;
/* The two frames of the stats icon that func_80219460 animates */
extern void *D_8023B350_ovl_01[STATS_ICON_FRAME_COUNT];
extern void *D_8023B358_ovl_01;
extern void *D_8023B35C_ovl_01;
extern void *D_8023B360_ovl_01;
extern void *D_8023B364_ovl_01;
extern void *D_8023B368_ovl_01;
extern void *D_8023B36C_ovl_01;
extern u32 D_802303AC_ovl_01[PLAYER_STATS_ICON_COUNT];
extern u32 D_802303CC_ovl_01[][2];
extern s16 D_8023040C_ovl_01[][STATS_INDEX_COLUMN_COUNT];
extern u8 D_801B5634;
extern s32 D_8022F8C0_ovl_01;
extern u8 D_8022F8C4_ovl_01;
extern s8 D_8022F8C5_ovl_01;
extern u8 D_802303A0_ovl_01;
extern s16 D_802303A2_ovl_01;
extern s16 D_802303A4_ovl_01;
extern u8 D_802308A0_ovl_01;
extern s8 D_801052E8;
extern s32 D_800FE334;
extern s8 D_801B60B9;
extern u8 D_801B9410;
extern s16 D_8023BFDC_ovl_01[MAX_PLAYER_COUNT];

void func_8021B87C_ovl_01(Gfx **displayListHead, u8 slide);
extern u8 D_8023BFF0_ovl_01[STATS_GRID_ROW_COUNT * STATS_GRID_COLUMN_COUNT][PLAYER_STATS_FILE_SIZE];

void func_802178B0_ovl_01(void *buffer, s16 bufferKind) {
    EncodedAsset asset;
    MenuMemory *memory = func_8005AF74();
    s16 iconIndex;
    u8 rowNumber; /* D_801B5634 counts the index tables' rows from 1 */
    u16 assetId;
    void **imageSlot;

    switch (bufferKind) {
        case PLAYER_STATS_BUFFER_ICONS:
            D_8023B340_ovl_01 = buffer;
            for (iconIndex = 0; iconIndex != PLAYER_STATS_ICON_COUNT; iconIndex++) {
                OpenEncodedAsset(D_802303AC_ovl_01[iconIndex], &asset);
                ReadEncodedAsset((u8 *)D_8023B340_ovl_01 + iconIndex * PLAYER_STATS_ICON_SIZE, &asset);
            }
            break;
        case PLAYER_STATS_BUFFER_UNK2:
            D_8023B348_ovl_01 = buffer;
            OpenEncodedAsset(ASSET_PLAYER_STATS_UNK2, &asset);
            ReadEncodedAsset(D_8023B348_ovl_01, &asset);
            break;
        case PLAYER_STATS_BUFFER_UNK3:
            D_8023B34C_ovl_01 = buffer;
            break;
        case PLAYER_STATS_BUFFER_UNK4:
            rowNumber = D_801B5634;
            imageSlot = &D_8023B350_ovl_01[0];
            *imageSlot = buffer;
            OpenEncodedAsset(D_802303CC_ovl_01[D_8023040C_ovl_01[rowNumber - 1][memory->unkD]][0], &asset);
            ReadEncodedAsset(*imageSlot, &asset);
            break;
        case PLAYER_STATS_BUFFER_UNK5:
            rowNumber = D_801B5634;
            imageSlot = &D_8023B350_ovl_01[1];
            *imageSlot = buffer;
            OpenEncodedAsset(D_802303CC_ovl_01[D_8023040C_ovl_01[rowNumber - 1][memory->unkD]][1], &asset);
            ReadEncodedAsset(*imageSlot, &asset);
            break;
        case PLAYER_STATS_BUFFER_UNK6:
            D_8023B344_ovl_01 = buffer;
            break;
        case PLAYER_STATS_BUFFER_UNK7:
            D_8023B358_ovl_01 = buffer;
            OpenEncodedAsset(ASSET_PLAYER_STATS_UNK7, &asset);
            ReadEncodedAsset(D_8023B358_ovl_01, &asset);
            break;
        case PLAYER_STATS_BUFFER_UNK8:
            D_8023B360_ovl_01 = buffer;
            OpenEncodedAsset(ASSET_PLAYER_STATS_UNK8, &asset);
            ReadEncodedAsset(D_8023B360_ovl_01, &asset);
            break;
        case PLAYER_STATS_BUFFER_GAME_LABELS:
            D_8023B364_ovl_01 = buffer;
            OpenEncodedAsset(ASSET_PLAYER_STATS_GAME_LABELS, &asset);
            ReadEncodedAsset(D_8023B364_ovl_01, &asset);
            break;
        case PLAYER_STATS_BUFFER_PLAYER_LABELS:
            D_8023B368_ovl_01 = buffer;
            OpenEncodedAsset(ASSET_PLAYER_LABELS, &asset);
            ReadEncodedAsset(D_8023B368_ovl_01, &asset);
            break;
        case PLAYER_STATS_BUFFER_UNK11:
            D_8023B36C_ovl_01 = buffer;
            OpenEncodedAsset(ASSET_PLAYER_STATS_UNK11, &asset);
            ReadEncodedAsset(D_8023B36C_ovl_01, &asset);
            break;
        case PLAYER_STATS_BUFFER_UNK12:
            assetId = ASSET_PLAYER_STATS_UNK12;
            D_8023B35C_ovl_01 = buffer;
            OpenEncodedAsset(assetId, &asset);
            ReadEncodedAsset(D_8023B35C_ovl_01, &asset);
            break;
        case PLAYER_STATS_BUFFER_UNK13:
            assetId = ASSET_PLAYER_STATS_UNK13;
            D_8023B35C_ovl_01 = buffer;
            OpenEncodedAsset(assetId, &asset);
            ReadEncodedAsset(D_8023B35C_ovl_01, &asset);
            break;
    }
    D_8022F8C5_ovl_01 = 0;
    D_8022F8C0_ovl_01 = 0;
    D_8022F8C4_ovl_01 = 0;
    D_802303A0_ovl_01 = 0;
    D_802303A1_ovl_01 = 0;
}

void func_80217B88_ovl_01(s32 requestedPreset) {
    s8 presetIndex = requestedPreset;
    MenuMemory *memory = func_8005AF74();
    s16 index;

    if ((u8)requestedPreset < MENU_UNKE_PRESET_COUNT) {
        if ((s8)requestedPreset == MENU_UNKE_PRESET_FIRST && D_80105B68 == MAX_PLAYER_COUNT) {
            presetIndex = MENU_UNKE_PRESET_FIRST_FOUR_PLAYER;
        }
        if (presetIndex == MENU_UNKE_PRESET_SECOND && D_80105B68 == MAX_PLAYER_COUNT) {
            presetIndex = MENU_UNKE_PRESET_SECOND_FOUR_PLAYER;
        }
        for (index = 0; index != MENU_UNKE_COUNT; index++) {
            memory->unkE[index] = D_802304BC_ovl_01[presetIndex][index];
            D_801B60BA[index] = D_802304F4_ovl_01[memory->unkE[index]];
        }
    } else {
        for (index = 0; index != MENU_UNKE_COUNT; index++) {
            memory->unkE[index] = -1;
            D_801B60BA[index] = -1;
        }
    }
    memory->unkD = 0;
    for (index = 0; index != MAX_PLAYER_COUNT; index++) {
        D_8023BFDC_ovl_01[index] = index < D_80105B68 ? 0 : -1;
    }
    memory->unk29 = 0;
    memory->unk2A = 0;
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_80217D20_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021826C_ovl_01);

void func_80219460_ovl_01(Gfx **displayListHead, s32 left, s32 top, s32 size, s32 textureStep) {
    Gfx *gfx = *displayListHead;
    MenuMemory *memory = func_8005AF74();
    EncodedAsset asset;
    void *image;
    u8 iconIndex;

    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPPipeSync(gfx++);
    image = D_8023B350_ovl_01[D_8022F8C4_ovl_01];
    gDPLoadTLUT_pal16(gfx++, 0, ((u32)image + IMAGE_HEADER_SIZE) & ~7);
    gDPLoadTextureTile_4b(gfx++, ((u32)image + PALETTED_TEXELS_OFFSET) & ~7, G_IM_FMT_CI, STATS_ICON_SIZE,
                          STATS_ICON_SIZE, 0, 0, STATS_ICON_SIZE - 1, STATS_ICON_SIZE - 1, 0,
                          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                          G_TX_NOLOD, G_TX_NOLOD);
    gSPScisTextureRectangle(gfx++, left << 2, top << 2, (left + size) << 2, (top + size) << 2, G_TX_RENDERTILE, 0,
                            0, textureStep, textureStep);
    D_8022F8C5_ovl_01++;
    gDPPipeSync(gfx++);
    if (D_8022F8C5_ovl_01 == STATS_ICON_FRAME_DURATION) {
        D_8022F8C5_ovl_01 = 0;
        D_8022F8C0_ovl_01++;
        if (D_8022F8C0_ovl_01 >= STATS_ICON_FRAME_COUNT) {
            D_8022F8C0_ovl_01 = 0;
        }
        D_8022F8C4_ovl_01 = !D_8022F8C4_ovl_01;
        iconIndex = D_8023040C_ovl_01[D_801B5634 - 1][memory->unkD];
        OpenEncodedAsset(D_802303CC_ovl_01[iconIndex][D_8022F8C0_ovl_01], &asset);
        ReadEncodedAsset(D_8023B350_ovl_01[D_8022F8C4_ovl_01], &asset);
    }
    *displayListHead = gfx;
}

/* Returns nothing, but is declared to return a value, which leaves v0 live at its end */
s32 func_80219948_ovl_01(void) {
    MenuMemory *memory = func_8005AF74();

    if (D_8022BE88_ovl_01 == MENU_PHASE_START) {
        PlaySong(SONG_TITLE, 0);
        g_MenuScreen = MENU_SCREEN_UNK6;
        D_8022BE89_ovl_01 = MENU_UNK89_UNK4;
        FreeAllCharacterSlots();
    }
    if (D_8022BE88_ovl_01 == MENU_PHASE_CHOOSING) {
        if (g_MenuPressedButtons & A_BUTTON) {
            PlaySound(SOUND_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_FULL);
            SetTextSlot(0, 0, NO_MESSAGE, 1, 0, 1, 0xFF, 0xFF, 0xFF, 0xFF);
            D_8023BFD0_ovl_01 = PLAYER_OPTION_SLIDE_START;
            D_802303A1_ovl_01 = 0;
            D_8022BE88_ovl_01 = MENU_PHASE_CONFIRMED;
            return;
        }
        if (g_MenuPressedButtons & B_BUTTON) {
            PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, MENU_VOLUME_FULL);
            g_MenuScreen = MENU_SCREEN_UNK22;
            SetTextSlot(0, 0, NO_MESSAGE, 1, 0, 1, 0xFF, 0xFF, 0xFF, 0xFF);
            D_802303A1_ovl_01 = 0;
            D_8022BE88_ovl_01 = MENU_PHASE_CANCELLED;
            return;
        }
        if (g_MenuRepeatButtons & L_JPAD) {
            PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            memory->unk29--;
            if (memory->unk29 < 0) {
                memory->unk29 = D_80105B68 - 1;
            }
            D_802303A1_ovl_01 = 0;
            return;
        }
        if (g_MenuRepeatButtons & R_JPAD) {
            PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            memory->unk29++;
            if (memory->unk29 > D_80105B68 - 1) {
                memory->unk29 = 0;
            }
            D_802303A1_ovl_01 = 0;
            return;
        }
        if (g_MenuRepeatButtons & U_JPAD) {
            PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            D_8023BFDC_ovl_01[memory->unk29]++;
            if (D_8023BFDC_ovl_01[memory->unk29] >= PLAYER_OPTION_COUNT) {
                D_8023BFDC_ovl_01[memory->unk29] = 0;
            }
            return;
        }
        if (g_MenuRepeatButtons & D_JPAD) {
            PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            D_8023BFDC_ovl_01[memory->unk29]--;
            if (D_8023BFDC_ovl_01[memory->unk29] < 0) {
                D_8023BFDC_ovl_01[memory->unk29] = PLAYER_OPTION_COUNT - 1;
            }
        }
    }
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_80219BD4_ovl_01);

void func_8021B1E0_ovl_01(Gfx **displayListHead, s32 centerX, s32 top, u8 labelIndex) {
    Gfx *gfx = *displayListHead;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal16(gfx++, 0, ((u32)D_8023B364_ovl_01 + IMAGE_HEADER_SIZE) & ~7);
    gDPLoadTextureBlock_4b(gfx++,
                           ((u32)D_8023B364_ovl_01 + PALETTED_TEXELS_OFFSET + labelIndex * GAME_LABEL_IMAGE_SIZE) & ~7,
                           G_IM_FMT_CI, GAME_LABEL_WIDTH, GAME_LABEL_HEIGHT, 0, G_TX_NOMIRROR | G_TX_WRAP,
                           G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, (centerX - GAME_LABEL_WIDTH / 2) << 2, top << 2,
                            (centerX + GAME_LABEL_WIDTH / 2) << 2, (top + GAME_LABEL_HEIGHT) << 2, G_TX_RENDERTILE, 0,
                            0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

s32 func_8021B618_ovl_01(Gfx **displayListHead, u8 slide) {
    Gfx *gfx = *displayListHead;
    s32 index;
    s32 rowIndex;
    s32 offset; /* The slide's first target, then the first row's offset */
    s32 first;

    if ((slide != STATS_SLIDE_CURRENT && slide < STATS_SLIDE_COUNT) == FALSE) {
        return -1;
    }
    offset = D_802307A8_ovl_01[slide - 1][0];
    first = D_80230784_ovl_01[0];
    for (index = 0; index != STATS_SLIDE_ROW_COUNT; index++) {
        if (D_80230784_ovl_01[index] != offset && first != D_80230784_ovl_01[index]) {
            break;
        }
    }
    if (index == STATS_SLIDE_ROW_COUNT && D_80230838_ovl_01 != slide) {
        D_80230838_ovl_01 = slide;
        switch (slide) {
            case STATS_SLIDE_OUT_RIGHT:
                for (rowIndex = 0; rowIndex != STATS_SLIDE_ROW_COUNT; rowIndex++) {
                    D_80230784_ovl_01[rowIndex] = D_802307A8_ovl_01[STATS_SLIDE_OUT_RIGHT - 1][rowIndex];
                    D_8023073C_ovl_01[rowIndex] = D_80230718_ovl_01[rowIndex];
                }
                break;
            case STATS_SLIDE_OUT_LEFT:
                for (rowIndex = 0; rowIndex != STATS_SLIDE_ROW_COUNT; rowIndex++) {
                    D_80230784_ovl_01[rowIndex] = D_802307A8_ovl_01[STATS_SLIDE_OUT_LEFT - 1][rowIndex];
                    D_8023073C_ovl_01[rowIndex] = D_80230718_ovl_01[rowIndex];
                }
                break;
            case STATS_SLIDE_IN_FROM_RIGHT:
            case STATS_SLIDE_IN_FROM_LEFT:
                for (rowIndex = 0; rowIndex != STATS_SLIDE_ROW_COUNT; rowIndex++) {
                    D_80230784_ovl_01[rowIndex] = D_802307A8_ovl_01[D_80230838_ovl_01 - 1][rowIndex];
                    D_8023073C_ovl_01[rowIndex] = D_80230718_ovl_01[rowIndex];
                }
                break;
        }
    }
    func_8021B87C_ovl_01(&gfx, D_80230838_ovl_01);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
    for (index = 0; index != STATS_SLIDE_ROW_COUNT; index++) {
        if ((offset = D_80230784_ovl_01[0]) != D_80230784_ovl_01[index]) {
            break;
        }
    }
    return index != STATS_SLIDE_ROW_COUNT;
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021B87C_ovl_01);

/*
 * Doesn't match: the original computes the mirrored arrow's start without a
 * branch, and keeps the display list head in its argument slot.
 */
#ifdef NON_MATCHING
void func_8021BE88_ovl_01(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 arrowIndex;
    s32 left;
    s32 unused[4];

    D_802303A8_ovl_01++;
    if (D_802303A8_ovl_01 >= PAGE_ARROW_FRAME_DURATION) {
        D_802303A6_ovl_01++;
        if (D_802303A6_ovl_01 >= PAGE_ARROW_FRAME_COUNT) {
            D_802303A6_ovl_01 = 0;
        }
        D_802303A8_ovl_01 = 0;
    }
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPLoadTLUT_pal16(gfx++, 0, ((u32)D_8023B35C_ovl_01 + IMAGE_HEADER_SIZE) & ~7);
    gDPPipeSync(gfx++);
    for (arrowIndex = 0; arrowIndex != PAGE_ARROW_COUNT; arrowIndex++) {
        if (arrowIndex == PAGE_ARROW_RIGHT) {
            /* The right arrow is the left one's image, mirrored */
            gDPLoadTextureTile_4b(gfx++,
                                  ((u32)D_8023B35C_ovl_01 + PALETTED_TEXELS_OFFSET +
                                   D_802303A6_ovl_01 * PAGE_ARROW_IMAGE_SIZE) &
                                      ~7,
                                  G_IM_FMT_CI, PAGE_ARROW_SIZE, PAGE_ARROW_SIZE, 0, 0, PAGE_ARROW_SIZE - 1,
                                  PAGE_ARROW_SIZE - 1, 0, G_TX_MIRROR | G_TX_WRAP, G_TX_MIRROR | G_TX_WRAP,
                                  PAGE_ARROW_MASK, PAGE_ARROW_MASK, G_TX_NOLOD, G_TX_NOLOD);
            left = PAGE_ARROW_RIGHT_X;
        } else {
            gDPLoadTextureTile_4b(gfx++,
                                  ((u32)D_8023B35C_ovl_01 + PALETTED_TEXELS_OFFSET +
                                   D_802303A6_ovl_01 * PAGE_ARROW_IMAGE_SIZE) &
                                      ~7,
                                  G_IM_FMT_CI, PAGE_ARROW_SIZE, PAGE_ARROW_SIZE, 0, 0, PAGE_ARROW_SIZE - 1,
                                  PAGE_ARROW_SIZE - 1, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP,
                                  G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
            left = PAGE_ARROW_LEFT_X;
        }
        gDPPipeSync(gfx++);
        gSPTextureRectangle(gfx++, left << 2, PAGE_ARROW_TOP << 2, (left + PAGE_ARROW_SIZE) << 2,
                            (PAGE_ARROW_TOP + PAGE_ARROW_SIZE) << 2, G_TX_RENDERTILE,
                            (arrowIndex == PAGE_ARROW_RIGHT) << 10, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
        gDPPipeSync(gfx++);
    }
    *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021BE88_ovl_01);
#endif

/*
 * Doesn't match: the original keeps the mirrored arrow's texture start in
 * the register it computes it in, where this moves it to another.
 */
#ifdef NON_MATCHING
void func_8021C200_ovl_01(Gfx **displayListHead, s32 gap, s32 centerY) {
    Gfx *gfx = *displayListHead;
    s32 arrowIndex;
    s32 left;

    D_802303A8_ovl_01++;
    if (D_802303A8_ovl_01 >= PAGE_ARROW_FRAME_DURATION) {
        D_802303A6_ovl_01++;
        if (D_802303A6_ovl_01 >= PAGE_ARROW_FRAME_COUNT) {
            D_802303A6_ovl_01 = 0;
        }
        D_802303A8_ovl_01 = 0;
    }
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPLoadTLUT_pal16(gfx++, 0, ((u32)D_8023B35C_ovl_01 + IMAGE_HEADER_SIZE) & ~7);
    gDPPipeSync(gfx++);
    for (arrowIndex = 0; arrowIndex != PAGE_ARROW_COUNT; arrowIndex++) {
        if (arrowIndex == PAGE_ARROW_RIGHT) {
            /* The right arrow is the left one's image, mirrored */
            gDPLoadTextureTile_4b(gfx++,
                                  ((u32)D_8023B35C_ovl_01 + PALETTED_TEXELS_OFFSET +
                                   D_802303A6_ovl_01 * PAGE_ARROW_IMAGE_SIZE) &
                                      ~7,
                                  G_IM_FMT_CI, PAGE_ARROW_SIZE, PAGE_ARROW_SIZE, 0, 0, PAGE_ARROW_SIZE - 1,
                                  PAGE_ARROW_SIZE - 1, 0, G_TX_MIRROR | G_TX_WRAP, G_TX_MIRROR | G_TX_WRAP,
                                  PAGE_ARROW_MASK, PAGE_ARROW_MASK, G_TX_NOLOD, G_TX_NOLOD);
            left = (SCREEN_WIDTH / 2) + gap / 2;
        } else {
            gDPLoadTextureTile_4b(gfx++,
                                  ((u32)D_8023B35C_ovl_01 + PALETTED_TEXELS_OFFSET +
                                   D_802303A6_ovl_01 * PAGE_ARROW_IMAGE_SIZE) &
                                      ~7,
                                  G_IM_FMT_CI, PAGE_ARROW_SIZE, PAGE_ARROW_SIZE, 0, 0, PAGE_ARROW_SIZE - 1,
                                  PAGE_ARROW_SIZE - 1, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP,
                                  G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
            left = (SCREEN_WIDTH / 2) - PAGE_ARROW_SIZE - gap / 2;
        }
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(gfx++, left << 2, (centerY - PAGE_ARROW_SIZE / 2) << 2,
                                (left + PAGE_ARROW_SIZE) << 2, (centerY + PAGE_ARROW_SIZE / 2) << 2, G_TX_RENDERTILE,
                                (arrowIndex == PAGE_ARROW_RIGHT) << 10, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
        gDPPipeSync(gfx++);
    }
    *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021C200_ovl_01);
#endif

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021C67C_ovl_01);

/*
 * Doesn't match: the original adds the texel offset to the blink frame's
 * offset before the arrow's, and adds the arrow size to the centre first.
 */
#ifdef NON_MATCHING
void func_8021CBDC_ovl_01(Gfx **displayListHead, s32 centerY) {
    Gfx *gfx = *displayListHead;
    s32 left = 0;
    s32 offsetY = 0;
    s32 arrowIndex;

    D_802303A4_ovl_01++;
    if (D_802303A4_ovl_01 == STATS_ARROW_BLINK_FRAME_COUNT) {
        D_802303A4_ovl_01 = 0;
        D_802303A2_ovl_01 ^= 1;
    }
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPLoadTLUT_pal16(gfx++, 0, ((u32)D_8023B358_ovl_01 + IMAGE_HEADER_SIZE) & ~7);
    for (arrowIndex = 0; arrowIndex != STATS_ARROW_COUNT; arrowIndex++) {
        gDPLoadTextureTile_4b(gfx++,
                              ((u32)D_8023B358_ovl_01 + PALETTED_TEXELS_OFFSET +
                               (D_802303A2_ovl_01 * STATS_ARROW_BLINK_STRIDE + arrowIndex * STATS_ARROW_IMAGE_SIZE)) &
                                  ~7,
                              G_IM_FMT_CI, STATS_ARROW_SIZE, STATS_ARROW_SIZE, 0, 0, STATS_ARROW_SIZE - 1,
                              STATS_ARROW_SIZE - 1, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP,
                              G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        gDPPipeSync(gfx++);
        if (arrowIndex == 0) {
            left = STATS_ARROW_LEFT;
            offsetY = STATS_UP_ARROW_OFFSET;
        } else if (arrowIndex == 1) {
            left = STATS_ARROW_LEFT;
            offsetY = STATS_DOWN_ARROW_OFFSET;
        }
        gSPScisTextureRectangle(gfx++, left << 2, (offsetY + centerY) << 2, (left + STATS_ARROW_SIZE) << 2,
                                (offsetY + (centerY + STATS_ARROW_SIZE)) << 2, G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE,
                                TEXEL_STEP_ONE);
        gDPPipeSync(gfx++);
    }
    *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021CBDC_ovl_01);
#endif

void func_8021CF10_ovl_01(void **buffers) {
    EncodedAsset asset;
    s16 imageIndex;

    D_8023B630_ovl_01 = buffers;
    for (imageIndex = 0; imageIndex != PLAYER_STATS_IMAGE_COUNT; imageIndex++) {
        OpenEncodedAsset(D_800C2064[imageIndex], &asset);
        ReadEncodedAsset(D_8023B630_ovl_01[imageIndex], &asset);
    }
}

void func_8021CFAC_ovl_01(void *buffer, s16 bufferKind) {
    EncodedAsset asset;

    switch (bufferKind) {
        case 1:
            D_8023B634_ovl_01 = buffer;
            break;
        case 2:
            D_8023B638_ovl_01 = buffer;
            OpenEncodedAsset(0x6EF, &asset);
            ReadEncodedAsset(D_8023B638_ovl_01, &asset);
            break;
        case 3:
            D_8023B63C_ovl_01 = buffer;
            OpenEncodedAsset(0x82D, &asset);
            ReadEncodedAsset(D_8023B63C_ovl_01, &asset);
            OpenEncodedAsset(0x82F, &asset);
            ReadEncodedAsset(D_8023B63C_ovl_01 + PLAYER_STATS_IMAGE_SIZE, &asset);
            OpenEncodedAsset(0x82E, &asset);
            ReadEncodedAsset(D_8023B63C_ovl_01 + PLAYER_STATS_IMAGE_SIZE * 2, &asset);
            OpenEncodedAsset(0x830, &asset);
            ReadEncodedAsset(D_8023B63C_ovl_01 + PLAYER_STATS_IMAGE_SIZE * 3, &asset);
            break;
    }
}

s16 func_8021E468_ovl_01(void);

/* func_8021E340 is called without a prototype; the original extends its result from 16 bits, not 8 */
void func_8021D0B8_ovl_01(void) {
    SaveData *save = GetSaveData();
    MenuMemory *memory = func_8005AF74();
    s32 slot;
    s32 moved;
    s32 column;

    if (IsHudAnimationDone() == TRUE) {
        if (GetWipeKind() != WIPE_NONE && D_801B9410 == 0) {
            if (GetWipeProgress() == 0) {
                g_MenuScreen = MENU_SCREEN_EXIT;
                D_800FE334 = 0;
                D_801B60B9 = 1;
                SetGameFlag(GAME_FLAG_UNK1E);
                for (slot = 0; slot != STATS_TEXT_SLOT_COUNT; slot++) {
                    SetTextSlot(0, 0, NO_MESSAGE, 1, slot, 0, 0xFF, 0xFF, 0xFF, 0xFF);
                }
            }
        } else if (GetWipeKind() == WIPE_NONE || D_801B9410 != 1) {
            if (g_MenuPressedButtons & A_BUTTON) {
                if (func_8021E468_ovl_01() != STATS_CELL_EMPTY) {
                    PlaySound(SOUND_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
                    D_801B9410 = 0;
                    column = memory->unk39;
                    LoadWipeShape((s8)save->rankedRecords[D_8023B640_ovl_01[memory->unk3A][column]].unk38);
                    StartWipe(WIPE_CLOSING);
                    D_801052E8 = 0;
                }
            } else if (g_MenuPressedButtons & B_BUTTON) {
                StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
                PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, MENU_VOLUME_FULL);
                StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
                g_MenuScreen = MENU_SCREEN_CLUBHOUSE;
                for (slot = 0; slot != STATS_TEXT_SLOT_COUNT; slot++) {
                    SetTextSlot(0, 0, NO_MESSAGE, 1, slot, 0, 0xFF, 0xFF, 0xFF, 0xFF);
                }
            } else if (g_MenuRepeatButtons & U_JPAD) {
                moved = func_8021E340_ovl_01(memory->unk39, memory->unk3A, STATS_DIRECTION_UP);
                if ((s16)moved != STATS_CELL_EMPTY) {
                    PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
                    memory->unk3A = moved;
                }
            } else if (g_MenuRepeatButtons & D_JPAD) {
                moved = func_8021E340_ovl_01(memory->unk39, memory->unk3A, STATS_DIRECTION_DOWN);
                if ((s16)moved != STATS_CELL_EMPTY) {
                    PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
                    memory->unk3A = moved;
                }
            } else if (g_MenuRepeatButtons & L_JPAD) {
                moved = func_8021E340_ovl_01(memory->unk39, memory->unk3A, STATS_DIRECTION_LEFT);
                if ((s16)moved != STATS_CELL_EMPTY) {
                    PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
                    memory->unk39 = moved;
                }
            } else if (g_MenuRepeatButtons & R_JPAD) {
                moved = func_8021E340_ovl_01(memory->unk39, memory->unk3A, STATS_DIRECTION_RIGHT);
                if ((s16)moved != STATS_CELL_EMPTY) {
                    PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
                    memory->unk39 = moved;
                }
            }
        }
    }
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021D3AC_ovl_01);

/*
 * Doesn't match: the original copies the chosen row's zero into the column
 * rather than loading zero, and so reads the last row into a1 instead of v1.
 */
#ifdef NON_MATCHING
void func_8021E01C_ovl_01(void) {
    SaveData *save = GetSaveData();
    s32 kindRecords[STATS_GRID_ROW_COUNT][STATS_KIND_LIST_SIZE];
    s32 index;
    s32 recordIndex;
    s32 kindThreeCount;
    s32 kindTwoCount;
    s32 kindOneCount;
    s32 kindZeroCount;
    s32 flags;
    u8 row;
    s32 column;
    MenuMemory *memory;

    InitSram();
    TransferSaveData(save, FALSE);
    for (recordIndex = 0; recordIndex != STATS_GRID_COLUMN_COUNT; recordIndex++) {
        kindRecords[0][recordIndex] = STATS_CELL_EMPTY;
        kindRecords[1][recordIndex] = STATS_CELL_EMPTY;
        kindRecords[2][recordIndex] = STATS_CELL_EMPTY;
        kindRecords[3][recordIndex] = STATS_CELL_EMPTY;
    }
    kindThreeCount = 0;
    kindTwoCount = 0;
    kindZeroCount = 0;
    kindOneCount = 0;
    for (recordIndex = 0; recordIndex != RANKED_RECORD_COUNT; recordIndex++) {
        flags = (s8)save->rankedRecords[recordIndex].unk3A;
        if (flags & RANKED_RECORD_SAVED) {
            switch (flags & RANKED_RECORD_KIND_MASK) {
                case 3:
                    if (kindThreeCount < STATS_GRID_COLUMN_COUNT) {
                        kindRecords[0][kindThreeCount++] = recordIndex;
                    }
                    break;
                case 2:
                    if (kindTwoCount < STATS_GRID_COLUMN_COUNT) {
                        kindRecords[1][kindTwoCount++] = recordIndex;
                    }
                    break;
                case 1:
                    if (kindOneCount < STATS_GRID_COLUMN_COUNT) {
                        kindRecords[3][kindOneCount++] = recordIndex;
                    }
                    break;
                case 0:
                    if (kindZeroCount < STATS_GRID_COLUMN_COUNT) {
                        kindRecords[2][kindZeroCount++] = recordIndex;
                    }
                    break;
            }
        }
    }
    row = 0;
    memory = func_8005AF74();
    column = row;
    if (kindThreeCount == 0) {
        if (kindTwoCount != 0) {
            row = 1;
        } else if (kindOneCount != 0) {
            row = 2;
        } else if (kindZeroCount != 0) {
            row = 3;
        }
    }
    switch (memory->unk3A) {
        case 0:
            if (kindThreeCount != 0) {
                row = 0;
                if (memory->unk39 < kindThreeCount && memory->unk39 >= 0) {
                    column = memory->unk39;
                }
            }
            break;
        case 1:
            if (kindTwoCount != 0) {
                row = 1;
                if (memory->unk39 < kindTwoCount && memory->unk39 >= 0) {
                    column = memory->unk39;
                }
            }
            break;
        case 2:
            if (kindOneCount != 0) {
                row = 2;
                if (memory->unk39 < kindOneCount && memory->unk39 >= 0) {
                    column = memory->unk39;
                }
            }
            break;
        case 3:
            if (kindZeroCount != 0) {
                row = 3;
                if (memory->unk39 < kindZeroCount && memory->unk39 >= 0) {
                    column = memory->unk39;
                }
            }
            break;
    }
    memory->unk39 = column;
    memory->unk3A = row;
    for (recordIndex = 0; recordIndex != STATS_GRID_COLUMN_COUNT; recordIndex++) {
        D_8023B640_ovl_01[0][recordIndex] = kindRecords[0][recordIndex];
        D_8023B640_ovl_01[1][recordIndex] = kindRecords[1][recordIndex];
        D_8023B640_ovl_01[2][recordIndex] = kindRecords[3][recordIndex];
        D_8023B640_ovl_01[3][recordIndex] = kindRecords[2][recordIndex];
    }
    recordIndex = 0;
    D_802308A0_ovl_01 = 0;
    for (; recordIndex != STATS_GRID_ROW_COUNT; recordIndex++) {
        for (index = 0; index != STATS_GRID_COLUMN_COUNT; index++) {
            if (D_8023B640_ovl_01[recordIndex][index] != STATS_CELL_EMPTY) {
                D_802308A0_ovl_01++;
                break;
            }
        }
        if (D_802308A0_ovl_01 != 0) {
            break;
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021E01C_ovl_01);
#endif

s8 func_8021E340_ovl_01(s8 column, s8 row, u8 direction) {
    s32 stepCount;
    s32 stepIndex;

    if (direction == STATS_DIRECTION_LEFT || direction == STATS_DIRECTION_RIGHT) {
        stepCount = STATS_GRID_COLUMN_COUNT;
    } else if (direction == STATS_DIRECTION_UP || direction == STATS_DIRECTION_DOWN) {
        stepCount = STATS_GRID_ROW_COUNT;
    } else {
        stepCount = 0;
    }
    for (stepIndex = 0; stepIndex != stepCount; stepIndex++) {
        column += D_802309C4_ovl_01[direction][0];
        row += D_802309C4_ovl_01[direction][1];
        if (column < 0) {
            column = STATS_GRID_COLUMN_COUNT - 1;
        }
        if (column >= STATS_GRID_COLUMN_COUNT) {
            column = 0;
        }
        if (row < 0) {
            row = STATS_GRID_ROW_COUNT - 1;
        }
        if (row >= STATS_GRID_ROW_COUNT) {
            row = 0;
        }
        if (D_8023B640_ovl_01[row][column] != STATS_CELL_EMPTY) {
            break;
        }
    }
    if (stepIndex == stepCount) {
        return -1;
    }
    /* No return follows for other directions, which step no cells and so returned above */
    if (direction == STATS_DIRECTION_LEFT || direction == STATS_DIRECTION_RIGHT) {
        return column;
    } else if (direction == STATS_DIRECTION_UP || direction == STATS_DIRECTION_DOWN) {
        return row;
    }
}

s16 func_8021E468_ovl_01(void) {
    MenuMemory *memory = func_8005AF74();

    return D_8023B640_ovl_01[memory->unk3A][memory->unk39];
}

void func_8021E4B0_ovl_01(Gfx **displayListHead, s32 left, s32 top, s32 labelIndex) {
    Gfx *gfx = *displayListHead;
    u8 *image;
    s32 width;
    s32 height;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    if (labelIndex < 0) {
        labelIndex = 0;
    }
    if (labelIndex >= SCORE_LABEL_COUNT) {
        labelIndex = SCORE_LABEL_COUNT - 1;
    }
    image = D_8023B63C_ovl_01 + labelIndex * PLAYER_STATS_IMAGE_SIZE;
    width = image[IMAGE_WIDTH_LOW_BYTE];
    height = image[IMAGE_HEIGHT_LOW_BYTE];
    /* The original passes the palette's first byte, not its address, so the colors come from low memory */
    gDPLoadTLUT_pal16(gfx++, 0, image[IMAGE_HEADER_SIZE] & ~7);
    gDPPipeSync(gfx++);
    gDPLoadTextureBlock_4b(gfx++, ((u32)image + PALETTED_TEXELS_OFFSET) & ~7, G_IM_FMT_CI, width, height, 0,
                           G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                           G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, left << 2, top << 2, (left + width) << 2, (top + height) << 2, G_TX_RENDERTILE, 0,
                            0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021E97C_ovl_01);

/* Doesn't match: the original keeps the row and column counters in other saved registers */
#ifdef NON_MATCHING
void func_8021EF64_ovl_01(void) {
    EncodedAsset asset;
    AssetFile file;
    SaveData *save = GetSaveData();
    s32 index;
    s32 row;
    s32 column;
    s32 character;
    s32 palette;
    s32 cell;

    for (index = 0; index != CHARACTER_COUNT; index++) {
        OpenEncodedAsset(D_802308A4_ovl_01[index], &asset);
        ReadEncodedAsset(D_8012D430[index], &asset);
    }
    for (row = 0; row != STATS_GRID_ROW_COUNT; row++) {
        cell = row * STATS_GRID_COLUMN_COUNT;
        for (column = 0; column != STATS_GRID_COLUMN_COUNT; column++, cell++) {
            if (D_8023B640_ovl_01[row][column] != STATS_CELL_EMPTY) {
                character = (s8)save->rankedRecords[D_8023B640_ovl_01[row][column]].unk3B;
                palette = (s8)save->rankedRecords[D_8023B640_ovl_01[row][column]].unk3D;
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
                character = 0;
                palette = 0;
            }
            D_8023B5E0_ovl_01[row][column] = D_8023BFF0_ovl_01[cell];
            OpenAssetFile(D_802308C8_ovl_01[character][palette], &file);
            ReadAssetFile(D_8023B5E0_ovl_01[row][column], GetAssetFileSize(&file), &file);
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/player_stats", func_8021EF64_ovl_01);
#endif

void func_8021F124_ovl_01(Gfx **displayListHead, void *texels, void *palette, s32 left, s32 top, s16 character,
                          u8 drawMode) {
    Gfx *gfx = *displayListHead;

    if (character < 0) {
        character = 0;
    }
    if (character >= CHARACTER_COUNT) {
        character = CHARACTER_COUNT - 1;
    }
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    if (drawMode == FACE_DARKENED) {
        gDPSetCombineMode(gfx++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
        gDPSetPrimColor(gfx++, 0, 0, FACE_DARKENED_SHADE, FACE_DARKENED_SHADE, FACE_DARKENED_SHADE, 255);
    } else {
        gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    }
    gDPLoadTLUT_pal256(gfx++, (u32)palette & ~7);
    gDPLoadTextureBlock(gfx++, (u32)texels & ~7, G_IM_FMT_CI, G_IM_SIZ_8b, FACE_WIDTH, FACE_HEIGHT, 0,
                        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                        G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, left << 2, top << 2, (left + FACE_WIDTH) << 2, (top + FACE_HEIGHT) << 2,
                            G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    gDPPipeSync(gfx++);
    if (drawMode == FACE_WITH_NAME) {
        if (character < NAMED_CHARACTER_COUNT) {
            SetUpTextDrawing(&gfx);
            SetTextColor(&gfx, 255, 255, 255);
            sprintf(g_PrintBuffer, D_802319E0_ovl_01, D_8022BF70_ovl_01[D_80230A48_ovl_01[character]]);
            DrawLargeText(&gfx, left + FACE_NAME_OFFSET_X, top + FACE_NAME_OFFSET_Y, g_PrintBuffer);
            gDPPipeSync(gfx++);
        }
    }
    *displayListHead = gfx;
}
