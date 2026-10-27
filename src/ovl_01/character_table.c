#include "common.h"
#include "asset.h"
#include "character.h"
#include "hole_display.h"
#include "menu.h"
#include "menu_memory.h"
#include "save.h"
#include "sound.h"

void func_8005C674(s32 arg0, s32 arg1);

/* The table of screen 15: its columns and rows, and how many of each fit on the screen */
#define TABLE_COLUMN_COUNT 14
#define TABLE_SHOWN_COLUMN_COUNT 7
#define TABLE_ROW_COUNT 18
#define TABLE_SHOWN_ROW_COUNT 5

/* The buffers that func_802159C0 keeps, by the kind its caller gives */
#define TABLE_BUFFER_CURSOR 1
#define TABLE_BUFFER_UNREAD 2
#define TABLE_BUFFER_ARROWS 3
#define TABLE_BUFFER_STAR 4
#define ASSET_TABLE_CURSOR 0x6EF
#define ASSET_TABLE_ARROWS 0x94A
#define ASSET_TABLE_STAR 0x5BE

/* func_80217464 squeezes the star's image to fit a table cell */
#define TABLE_STAR_IMAGE_WIDTH 32
#define TABLE_STAR_IMAGE_HEIGHT 24
#define TABLE_STAR_WIDTH 24
#define TABLE_STAR_HEIGHT 20

/* Where the table's popup goes, and the cursor places that move it */
#define TABLE_POPUP_TOP_LEFT 0
#define TABLE_POPUP_TOP_RIGHT 1
#define TABLE_POPUP_BOTTOM_LEFT 2
#define TABLE_POPUP_BOTTOM_RIGHT 3
#define TABLE_POPUP_LEFT_X 68
#define TABLE_POPUP_RIGHT_X 164
#define TABLE_POPUP_TOP_Y 84
#define TABLE_POPUP_BOTTOM_Y 170
#define TABLE_POPUP_LEFT_COLUMN_LIMIT 4
#define TABLE_POPUP_RIGHT_COLUMN 3
#define TABLE_POPUP_TOP_ROW_LIMIT 2
#define TABLE_POPUP_BOTTOM_ROW 3

/* The characters that func_8021730C shows on the table, the first ones being those of the character grid */
#define TABLE_CHARACTER_HIDDEN (-1)
#define TABLE_CHARACTER_SHOWN 1
#define GRID_CHARACTER_COUNT 13
#define TABLE_CELL_COUNT 17

/* A cell of the character grid */
typedef struct TableCell {
    /* 0x0 */ s32 column;
    /* 0x4 */ s32 row;
} TableCell; /* size = 0x8 */

/* The lines between the table's cells, each from one corner to the next */
#define TABLE_LINE_COUNT 12

typedef struct TableCorner {
    /* 0x0 */ s32 x;
    /* 0x4 */ s32 y;
} TableCorner; /* size = 0x8 */

/* The most bytes of each character's file that func_802177D0 keeps */
#define MENU_SCENE_FILE_SIZE 0x200

extern void *D_8012D430[CHARACTER_COUNT];
extern s32 D_80230190_ovl_01;
extern s32 D_80230194_ovl_01;
extern s32 D_80230198_ovl_01;
extern s32 D_8023019C_ovl_01;
extern s32 D_802301A0_ovl_01;
extern s32 D_802301A4_ovl_01;
extern TableCorner D_80230248_ovl_01[TABLE_LINE_COUNT * 2];
extern u16 D_802308A4_ovl_01[CHARACTER_COUNT];
extern s16 D_802308C8_ovl_01[CHARACTER_COUNT][CHARACTER_PALETTE_COUNT];
extern void *D_8023AC64_ovl_01;
extern void *D_8023AC68_ovl_01;
extern void *D_8023AC6C_ovl_01;
extern void *D_8023AC70_ovl_01;
extern s32 D_8023AC78_ovl_01[CHARACTER_COUNT];
extern u8 D_8023BFF0_ovl_01[CHARACTER_COUNT][MENU_SCENE_FILE_SIZE];

void func_802159C0_ovl_01(void *buffer, s16 bufferKind) {
    EncodedAsset asset;
    MenuMemory *memory = func_8005AF74();
    s32 x;
    s32 y;

    switch (bufferKind) {
        case TABLE_BUFFER_CURSOR:
            D_8023AC64_ovl_01 = buffer;
            OpenEncodedAsset(ASSET_TABLE_CURSOR, &asset);
            ReadEncodedAsset(D_8023AC64_ovl_01, &asset);
            break;
        case TABLE_BUFFER_UNREAD:
            D_8023AC68_ovl_01 = buffer;
            break;
        case TABLE_BUFFER_ARROWS:
            D_8023AC6C_ovl_01 = buffer;
            OpenEncodedAsset(ASSET_TABLE_ARROWS, &asset);
            ReadEncodedAsset(D_8023AC6C_ovl_01, &asset);
            D_8023019C_ovl_01 = 0;
            D_802301A0_ovl_01 = 0;
            D_802301A4_ovl_01 = 0;
            break;
        case TABLE_BUFFER_STAR:
            D_8023AC70_ovl_01 = buffer;
            OpenEncodedAsset(ASSET_TABLE_STAR, &asset);
            ReadEncodedAsset(D_8023AC70_ovl_01, &asset);
            break;
    }
    /* The popup goes to the quarter of the screen away from the cursor */
    if (memory->tableColumn < TABLE_POPUP_LEFT_COLUMN_LIMIT && memory->tableRow < TABLE_POPUP_TOP_ROW_LIMIT) {
        x = TABLE_POPUP_RIGHT_X;
        y = TABLE_POPUP_BOTTOM_Y;
        D_80230198_ovl_01 = TABLE_POPUP_BOTTOM_RIGHT;
    } else if (memory->tableColumn >= TABLE_POPUP_RIGHT_COLUMN && memory->tableRow < TABLE_POPUP_TOP_ROW_LIMIT) {
        x = TABLE_POPUP_LEFT_X;
        y = TABLE_POPUP_BOTTOM_Y;
        D_80230198_ovl_01 = TABLE_POPUP_BOTTOM_LEFT;
    } else if (memory->tableColumn < TABLE_POPUP_LEFT_COLUMN_LIMIT && memory->tableRow >= TABLE_POPUP_BOTTOM_ROW) {
        x = TABLE_POPUP_RIGHT_X;
        y = TABLE_POPUP_TOP_Y;
        D_80230198_ovl_01 = TABLE_POPUP_TOP_RIGHT;
    } else {
        x = TABLE_POPUP_LEFT_X;
        y = TABLE_POPUP_TOP_Y;
        D_80230198_ovl_01 = TABLE_POPUP_TOP_LEFT;
    }
    D_80230190_ovl_01 = x;
    D_80230194_ovl_01 = y;
}

void func_80215B84_ovl_01(void) {
    MenuMemory *memory = func_8005AF74();
    s8 firstColumn;
    s8 firstRow;
    s8 column;
    s8 row;

    if (IsHudAnimationDone() == TRUE) {
        if (g_MenuPressedButtons & B_BUTTON) {
            StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
            PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, MENU_VOLUME_FULL);
            StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
            g_MenuScreen = MENU_SCREEN_CLUBHOUSE;
            return;
        }
        firstColumn = memory->tableFirstColumn;
        firstRow = memory->tableFirstRow;
        column = memory->tableColumn;
        row = memory->tableRow;
        if (g_MenuRepeatButtons & R_JPAD) {
            PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            if (column == TABLE_SHOWN_COLUMN_COUNT - 1 && firstColumn < TABLE_COLUMN_COUNT - TABLE_SHOWN_COLUMN_COUNT) {
                memory->tableFirstColumn++;
            }
            memory->tableColumn++;
            if (memory->tableColumn >= TABLE_SHOWN_COLUMN_COUNT) {
                memory->tableColumn = TABLE_SHOWN_COLUMN_COUNT - 1;
            }
        } else if (g_MenuRepeatButtons & L_JPAD) {
            PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            if (column == 0 && firstColumn > 0) {
                memory->tableFirstColumn--;
            }
            memory->tableColumn--;
            if (memory->tableColumn < 0) {
                memory->tableColumn = 0;
            }
        } else if (g_MenuRepeatButtons & D_JPAD) {
            PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            if (row == TABLE_SHOWN_ROW_COUNT - 1 && firstRow < TABLE_ROW_COUNT - TABLE_SHOWN_ROW_COUNT) {
                memory->tableFirstRow++;
            }
            memory->tableRow++;
            if (memory->tableRow >= TABLE_SHOWN_ROW_COUNT) {
                memory->tableRow = TABLE_SHOWN_ROW_COUNT - 1;
            }
        } else if (g_MenuRepeatButtons & U_JPAD) {
            PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            if (row == 0 && firstRow > 0) {
                memory->tableFirstRow--;
            }
            memory->tableRow--;
            if (memory->tableRow < 0) {
                memory->tableRow = 0;
            }
        }
    }
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/character_table", func_80215DC4_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/character_table", func_8021640C_ovl_01);

void func_802169CC_ovl_01(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 index;

    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPPipeSync(gfx++);
    for (index = 0; index != TABLE_LINE_COUNT * 2; index += 2) {
        gSPScisTextureRectangle(gfx++, D_80230248_ovl_01[index].x << 2, D_80230248_ovl_01[index].y << 2,
                                D_80230248_ovl_01[index + 1].x << 2, D_80230248_ovl_01[index + 1].y << 2,
                                G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
        gDPPipeSync(gfx++);
    }
    *displayListHead = gfx;
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/character_table", func_80216C14_ovl_01);

void func_8021730C_ovl_01(void) {
    SaveData *save = GetSaveData();
    TableCell cells[TABLE_CELL_COUNT] = {
        { 0, 0 }, { 0, 1 }, { 1, 0 }, { 1, 1 }, { 2, 0 }, { 2, 1 }, { 3, 0 }, { 3, 1 }, { 4, 0 },
        { 4, 1 }, { 5, 0 }, { 5, 1 }, { 6, 0 }, { 0, 2 }, { 1, 2 }, { 2, 2 }, { 3, 2 },
    };
    s32 index;
    s32 recordCount;
    s32 row;
    s32 column;

    func_8005C674(1, 0);
    for (index = 0; index != CHARACTER_COUNT; index++) {
        D_8023AC78_ovl_01[index] = TABLE_CHARACTER_HIDDEN;
    }
    for (index = 0; index != GRID_CHARACTER_COUNT; index++) {
        row = cells[index].row;
        column = cells[index].column;
        if (g_CharacterGrid[row][column] >= 0) {
            D_8023AC78_ovl_01[index] = TABLE_CHARACTER_SHOWN;
        }
    }
    for (row = GRID_CHARACTER_COUNT; row != CHARACTER_COUNT; row++) {
        column = 0;
        recordCount = 0;
        for (; column != SAVE_UNKAF0_COLUMN_COUNT; column++) {
            recordCount += (save->unk990.unkAF0[row][column].unk1 & 0x7F) != 0;
        }
        /* The original stores these one entry early, over the grid's last character */
        if (recordCount > 0) {
            D_8023AC78_ovl_01[row - 1] = TABLE_CHARACTER_SHOWN;
        }
    }
}

void func_80217464_ovl_01(Gfx **displayListHead, s32 left, s32 top) {
    Gfx *gfx = *displayListHead;

    gDPPipeSync(gfx++);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPLoadTLUT_pal16(gfx++, 0, ((u32)D_8023AC70_ovl_01 + IMAGE_HEADER_SIZE) & ~7);
    gDPLoadTextureBlock_4b(gfx++, ((u32)D_8023AC70_ovl_01 + PALETTED_TEXELS_OFFSET) & ~7, G_IM_FMT_CI,
                           TABLE_STAR_IMAGE_WIDTH, TABLE_STAR_IMAGE_HEIGHT, 0, G_TX_NOMIRROR | G_TX_WRAP,
                           G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gSPScisTextureRectangle(gfx++, left << 2, top << 2, (left + TABLE_STAR_WIDTH) << 2, (top + TABLE_STAR_HEIGHT) << 2,
                            G_TX_RENDERTILE, 0, 0, (TABLE_STAR_IMAGE_WIDTH << 10) / TABLE_STAR_WIDTH,
                            (TABLE_STAR_IMAGE_HEIGHT << 10) / TABLE_STAR_HEIGHT);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void func_802177D0_ovl_01(void) {
    EncodedAsset asset;
    AssetFile file;
    s32 characterIndex;

    GetSaveData();
    for (characterIndex = 0; characterIndex != CHARACTER_COUNT; characterIndex++) {
        OpenEncodedAsset(D_802308A4_ovl_01[characterIndex], &asset);
        ReadEncodedAsset(D_8012D430[characterIndex], &asset);
        OpenAssetFile(D_802308C8_ovl_01[characterIndex][0], &file);
        ReadAssetFile(D_8023BFF0_ovl_01[characterIndex], GetAssetFileSize(&file), &file);
    }
}
