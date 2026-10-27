#include "common.h"
#include "asset.h"
#include "character.h"
#include "game_flags.h"
#include "menu.h"
#include "menu_memory.h"
#include "save.h"

/* Slots marked with MARKED_CHARACTER_OFFSET hold values below this; empty ones hold -1 */
#define MARKED_CHARACTER_LIMIT (-10)

/* The bytes of the first of the two images that func_8020B438 reads into the menu's buffer */
#define CHARACTER_SELECT_FIRST_IMAGE_SIZE 0x628

/* The buffer that func_8020B2C4 keeps without reading an image into it */
#define CHARACTER_SELECT_UNREAD_KIND 10

/* The characters' faces, the first set of them shown with GAME_FLAG_UNK64 set */
extern u16 D_800C2650[][8];
extern u16 D_800C2780[][8];
extern void *D_80105B78[CHARACTER_COUNT];
extern void *D_8012D430[CHARACTER_COUNT];
extern s32 D_801B7070;
extern u8 D_8022FF01_ovl_01;
extern s32 D_8022FF2C_ovl_01[];
extern void *D_8023A568_ovl_01;
extern void *D_8023A570_ovl_01;
extern void *D_8023A574_ovl_01;
extern void *D_8023A578_ovl_01;
extern void *D_8023A57C_ovl_01;
extern void *D_8023A580_ovl_01;
extern void *D_8023A584_ovl_01;
extern s32 D_8022FF1C_ovl_01[4];
extern s32 D_8022FF50_ovl_01[];

void func_80209500_ovl_01(void) {
    s32 firstSlot = g_CharacterGrid[2][0];
    s32 secondSlot = g_CharacterGrid[2][1];
    s32 thirdSlot = g_CharacterGrid[2][2];
    s32 fourthSlot = g_CharacterGrid[2][3];

    g_CharacterGrid[2][0] = -1;
    g_CharacterGrid[2][1] = -1;
    g_CharacterGrid[2][2] = -1;
    g_CharacterGrid[2][3] = -1;
    D_8022FF1C_ovl_01[0] = firstSlot;
    D_8022FF1C_ovl_01[1] = secondSlot;
    D_8022FF1C_ovl_01[2] = thirdSlot;
    D_8022FF1C_ovl_01[3] = fourthSlot;
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/character_select", func_80209568_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/character_select", func_80209894_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/character_select", func_80209BE0_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/character_select", func_80209D50_ovl_01);

void func_8020B2C4_ovl_01(void *buffer, s16 bufferKind) {
    EncodedAsset asset;
    void *image = buffer;

    switch (bufferKind) {
        case 1:
            D_8023A570_ovl_01 = buffer;
            break;
        case 2:
            D_8023A568_ovl_01 = buffer;
            break;
        case 5:
            D_8023A574_ovl_01 = buffer;
            break;
        case 6:
            D_8023A57C_ovl_01 = buffer;
            break;
        case 7:
            D_8023A580_ovl_01 = buffer;
            break;
        case 8:
            D_8023A584_ovl_01 = buffer;
            break;
        case CHARACTER_SELECT_UNREAD_KIND:
            D_8023A578_ovl_01 = buffer;
            break;
        default:
            D_8023A570_ovl_01 = buffer;
            bufferKind = CHARACTER_SELECT_UNREAD_KIND;
            break;
    }
    if (bufferKind != CHARACTER_SELECT_UNREAD_KIND) {
        OpenEncodedAsset(D_8022FF2C_ovl_01[bufferKind], &asset);
        ReadEncodedAsset(image, &asset);
    }
    D_8022FF01_ovl_01 = FALSE;
}

void func_8020B3CC_ovl_01(void) {
    MenuMemory *memory = func_8005AF74();
    s32 row;
    s32 column;

    for (row = 0; row != CHARACTER_CHOICE_ROW_COUNT; row++) {
        for (column = 0; column != CHARACTER_GRID_COLUMN_COUNT; column++) {
            if (g_CharacterGrid[row][column] < MARKED_CHARACTER_LIMIT) {
                memory->characterColumn = column;
                memory->characterRow = row;
            }
        }
    }
}

/*
 * Doesn't match: the original indexes D_80105B78 afresh in each pass,
 * where this steps a pointer through it and needs one more saved register.
 */
#ifdef NON_MATCHING
void func_8020B438_ovl_01(void) {
    EncodedAsset asset;
    AssetFile file;
    s32 character = 0;
    u8 *images = GetMenuImageBuffer();
    s32 slotIndex;
    void *faceImage;
    void *characterFile;
    u16 faceAsset;

    OpenEncodedAsset(0x979, &asset);
    ReadEncodedAsset(images, &asset);
    OpenEncodedAsset(0x978, &asset);
    ReadEncodedAsset(images + CHARACTER_SELECT_FIRST_IMAGE_SIZE, &asset);
    for (slotIndex = 0; slotIndex != CHARACTER_COUNT; slotIndex++) {
        faceImage = D_8012D430[slotIndex];
        characterFile = D_80105B78[slotIndex];
        if (slotIndex >= FIRST_IMPORTED_CHARACTER) {
            if (g_ImportedCharacters[slotIndex - FIRST_IMPORTED_CHARACTER].character >= 0) {
                character = g_ImportedCharacters[slotIndex - FIRST_IMPORTED_CHARACTER].character;
            }
        } else {
            character = slotIndex;
        }
        if (IsGameFlagSet(GAME_FLAG_UNK64)) {
            faceAsset = D_800C2780[character][0];
        } else {
            faceAsset = D_800C2650[character][0];
        }
        OpenEncodedAsset(faceAsset, &asset);
        ReadEncodedAsset(faceImage, &asset);
        OpenAssetFile(D_800C2590[character].assetId, &file);
        ReadAssetFile(characterFile, GetAssetFileSize(&file), &file);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/character_select", func_8020B438_ovl_01);
#endif

INCLUDE_ASM("asm/nonmatchings/ovl_01/character_select", func_8020B590_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/character_select", func_8020BA08_ovl_01);

/* Doesn't match: the first loop's index and count take each other's registers */
#ifdef NON_MATCHING
s32 func_8020C2D8_ovl_01(s32 choiceIndex) {
    s32 freePalette = 0;
    s32 playerIndex = 0;
    s32 sameCount = 0;
    s32 character = D_8022FF50_ovl_01[choiceIndex];
    s32 unused[4];
    s32 samePlayers[MAX_PLAYER_COUNT];
    s32 palette;
    s32 sameIndex;

    for (; playerIndex != D_801B7070; playerIndex++) {
        if ((s8)g_Players[playerIndex].characterIndex == character) {
            samePlayers[sameCount++] = playerIndex;
        }
    }
    if (sameCount != 0) {
        for (palette = 0; palette != CHARACTER_PALETTE_COUNT; palette++) {
            for (sameIndex = 0; sameIndex != sameCount; sameIndex++) {
                if ((s8)g_Players[samePlayers[sameIndex]].paletteIndex == palette) {
                    break;
                }
            }
            if (sameIndex == sameCount) {
                freePalette = palette;
                break;
            }
        }
        return freePalette;
    }
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/character_select", func_8020C2D8_ovl_01);
#endif

INCLUDE_ASM("asm/nonmatchings/ovl_01/character_select", func_8020C3B8_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/character_select", func_8020C6D8_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/character_select", func_8020CD20_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/character_select", func_8020D338_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/character_select", func_8020D734_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/character_select", func_8020DB30_ovl_01);

const char D_80231810_ovl_01[] = "Sand Saves";
const char D_8023181C_ovl_01[] = "Par Ons";
const char D_80231824_ovl_01[] = "Par Saves";
const char D_80231830_ovl_01[] = "Avg. Putt";
const char D_8023183C_ovl_01[] = "On Fairway";
const char D_80231848_ovl_01[] = "Avg. Drive";
const char D_80231854_ovl_01[] = "Best Drive";
const char D_80231860_ovl_01[] = "Average";
const char D_80231868_ovl_01[] = "Best Score";
const char D_80231874_ovl_01[] = "Round";
