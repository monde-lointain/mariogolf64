#include "common.h"
#include "asset.h"
#include "hole_display.h"
#include "menu.h"
#include "menu_memory.h"
#include "sound.h"

/* The table of screen 15: its columns and rows, and how many of each fit on the screen */
#define TABLE_COLUMN_COUNT 14
#define TABLE_SHOWN_COLUMN_COUNT 7
#define TABLE_ROW_COUNT 18
#define TABLE_SHOWN_ROW_COUNT 5

extern void *D_8023AA44_ovl_01;

INCLUDE_ASM("asm/nonmatchings/ovl_01/menu_scene", func_80213390_ovl_01);

void func_8021342C_ovl_01(void *buffer) {
    EncodedAsset asset;

    D_8023AA44_ovl_01 = buffer;
    OpenEncodedAsset(0x8E9, &asset);
    ReadEncodedAsset(D_8023AA44_ovl_01, &asset);
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/menu_scene", func_80213464_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/menu_scene", func_80213558_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/menu_scene", func_80213748_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/menu_scene", func_802144E4_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/menu_scene", func_802149A8_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/menu_scene", func_8021508C_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/menu_scene", func_8021585C_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/menu_scene", func_802159C0_ovl_01);

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

INCLUDE_ASM("asm/nonmatchings/ovl_01/menu_scene", func_80215DC4_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/menu_scene", func_8021640C_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/menu_scene", func_802169CC_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/menu_scene", func_80216C14_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/menu_scene", func_8021730C_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/menu_scene", func_80217464_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/menu_scene", func_802177D0_ovl_01);
