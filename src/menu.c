#include "common.h"
#include <lib/libnusys/nusys.h>
#include "game_flags.h"
#include "asset.h"
#include "character.h"
#include "club.h"
#include "course.h"
#include "heap.h"
#include "host_file.h"
#include "libc.h"
#include "math_util.h"
#include "menu.h"
#include "password.h"
#include "round_stats.h"
#include "save.h"
#include "scene.h"
#include "screen.h"
#include "sound.h"
#include "sram.h"
#include "transfer_pak.h"
#include "variables.h"

/* Keeps the save data on the development host instead of in SRAM */
#define GAME_FLAG_SAVE_TO_HOST 0x53

/* The SRAM check buffer is read and written by DMA, so it starts on a cache line */
#define DMA_ALIGNMENT 0x10
#define ALIGN_FOR_DMA(address) (((u32)(address) + DMA_ALIGNMENT - 1) & ~(DMA_ALIGNMENT - 1))

/* The records that unlock two of the characters */
#define DONKEY_KONG_STAR_COUNT 30
#define MAPLE_BADGE_COUNT 50

/* Each Ring Shot course after the first opens after this many more stars */
#define STARS_PER_RING_SHOT_COURSE 4

/* Mirrors of the save data's options */
extern u8 g_OptionBits;
/* A copy of g_OptionBits, from which the music code reads OPTION_BIT_MONO */
extern u8 g_SoundOptionBits;

extern SaveCopyLocation g_SaveCopyLocations[];
/* The course points needed in total to open each course */
extern s32 g_CoursePointsNeeded[COURSE_COUNT];
extern char g_RomBuildDate[];

typedef struct Unk800C2B90 {
    /* 0x0 */ s16 unk0;
    /* 0x2 */ s16 column;
    /* 0x4 */ s16 unk4;
    /* 0x6 */ s16 row;
} Unk800C2B90; /* size = 0x8 */

extern Unk800C2B90 D_800C2B90[IMPORTED_CHARACTER_COUNT];
extern s8 D_800C2AA8[IMPORTED_CHARACTER_COUNT][IMPORTED_CHARACTER_NAME_LENGTH];
/* The imported characters that debug flag 0x54 adds */
extern s16 D_800C2B40[IMPORTED_CHARACTER_COUNT];
extern s16 D_800C2B48[IMPORTED_CHARACTER_COUNT];
extern s16 D_800C2B50[IMPORTED_CHARACTER_COUNT];
extern s16 D_800C2B58[IMPORTED_CHARACTER_COUNT];
extern u8 D_800C2B60[IMPORTED_CHARACTER_COUNT][IMPORTED_CHARACTER_NAME_LENGTH];

extern s8 D_800C1FF0;
extern s8 D_800C1FF4;
extern s8 D_800C1FF5;
extern s8 D_800C1FF6;
extern s8 D_800C1FF7;
extern s32 D_800C2BD0;
extern s8 D_800C2BD4;
extern s16 D_800FBD40;
extern s8 D_800FBDB9;
extern s8 D_800FBE08[4];
extern s16 D_800FBE2C;
extern s32 D_800FE334;
extern s16 D_800FE33C;
extern GameState D_801B55F0;
extern UnkB8 D_8012D0C8[MAX_PLAYER_COUNT];
extern Unk68 D_800FEDD8[MAX_PLAYER_COUNT];
extern s8 D_801B60B8;
extern s8 D_800FF4C0;
extern s8 D_801B5634;
extern s8 D_800C2BD5;
extern s8 D_801052E8;
extern s32 D_80105B68;
extern s8 D_80105DC1;
extern s8 D_80105DC2;
extern s32 D_80105DC4;
extern s32 D_80105DC8;
extern s32 D_80105DCC;
/* The code of the tournament whose password was entered */
extern u8 g_TournamentCode[TOURNAMENT_CODE_SIZE];
extern s32 D_8012F480;
extern s32 D_8012F484;
extern s32 D_8012F488;
extern s32 D_8012F48C;
extern u8 D_801323A0[0x49];
extern s8 D_801323D0;
extern s32 D_801B7070;
extern s8 D_801B70F0;
extern s8 D_801B70F1;
extern s8 D_801B70F2;
extern s8 D_801B70F3;
extern s16 D_801B71CC;
extern s8 D_801B9410;
extern u8 g_MapCamera[];
extern void *D_801F4A20;
extern void *D_8012D3AC;
extern void *D_8012F4B0[7];
extern void *D_801049A0[7];
extern void *D_801052C4[2];
extern void *D_800FF420[2];
extern void *D_801B60D0;
extern void *D_8012D420[2];
extern void *D_801B5CD0[2];
extern void *D_801052BC[2];
extern void *D_800FBDFC[2];
extern void *D_800FF4C4[2];
extern void *D_800FBE38[3];
extern void *D_801B93E0[5];
extern void *D_8010526C[2];
extern void *D_800FF180[2];
extern void *D_80106100;
extern void *D_800FC898;
extern void *D_8012D430[18];
extern void *D_80105B78[18];
extern void *D_800FBE30[2];
extern void *D_800FC6A0;
extern void *D_801B69EC;
extern void *D_8012D424; /* D_8012D420[1], named apart to match */
extern u16 D_800C2014[1];
extern u16 D_800C2016;
extern u16 D_800C2028[1];
extern u16 D_800C2064[7];
extern s16 D_800C20D4[2];
extern u16 D_800C20E0[][12];
extern u16 D_800C21B8[1];
extern s16 D_800C2278[3];
extern u16 D_800C28B0[1];
void SetFramebufferCount(s32 count);

/* D_800C29B8 lists slots in id order up to this count */
#define MARKABLE_CHARACTER_COUNT 10

typedef struct Unk800C2650 {
    /* 0x0 */ u16 assetId;
    /* 0x2 */ u8 unk2[0xE];
} Unk800C2650; /* size = 0x10 */

extern Unk800C2650 D_800C2650[];
extern s16 g_EveryCharacterGrid[CHARACTER_CHOICE_ROW_COUNT][CHARACTER_GRID_COLUMN_COUNT];
extern s16 D_800C293C[CHARACTER_CHOICE_ROW_COUNT][CHARACTER_GRID_COLUMN_COUNT];
extern s16 D_800C2968[13];
extern s16 g_StartingCharacterGrid[CHARACTER_GRID_ROW_COUNT][CHARACTER_GRID_COLUMN_COUNT];
extern void *D_80105BB0[4];
extern void *D_8012D468[4];

void func_8005CA48(void);
typedef struct Unk800C29B8 {
    /* 0x0 */ s16 column;
    /* 0x2 */ s16 row;
} Unk800C29B8; /* size = 0x4 */
extern Unk800C29B8 D_800C29B8[];
extern s16 D_800C2BB0[15];

void *func_8005AF74(void);
s32 GetClubTournamentCourse(void);
void func_8005D9A0(void);
s32 ReadSaveCopies(SaveData *save, u8 isInSram);
s32 WriteSaveCopies(SaveData *save, u8 isInSram);
void SyncSaveOptions(SaveData *save, s32 isWrite, s32 readResult);

/*
 * Allocates and reads the menu's assets, then reads the save. Overlay 3
 * calls this as it loads.
 */
void LoadMenuResources(void) {
    EncodedAsset asset;
    AssetFile file;
    s32 unused[4]; /* The original frame has room for this */
    SaveData *save;
    s32 index;
    s32 size;

    save = GetSaveData();
    if (D_800C1FF0 != -1) {
        osSyncPrintf("menu const error");
        return;
    }
    D_801F4A20 = AllocMemory(OpenEncodedAsset(0x51E, &asset));
    ReadEncodedAsset(D_801F4A20, &asset);
    D_8012D3AC = AllocMemory(OpenEncodedAsset(0x721, &asset));
    ReadEncodedAsset(D_8012D3AC, &asset);
    D_800FF420[0] = AllocMemory(OpenEncodedAsset(0x520, &asset));
    ReadEncodedAsset(D_800FF420[0], &asset);
    D_800FF420[1] = AllocMemory(OpenEncodedAsset(0x520, &asset));
    ReadEncodedAsset(D_800FF420[1], &asset);
    D_801B60D0 = AllocMemory(OpenEncodedAsset(0x521, &asset));
    ReadEncodedAsset(D_801B60D0, &asset);
    D_8012D420[0] = AllocMemory(0x70A8);
    D_801B5CD0[0] = AllocMemory(0x70A8);
    D_8012D424 = AllocMemory(0x70A8);
    D_801B5CD0[1] = AllocMemory(0x70A8);
    OpenEncodedAsset(*D_800C2014, &asset);
    ReadEncodedAsset(D_8012D420[0], &asset);
    OpenEncodedAsset(*D_800C2014, &asset);
    ReadEncodedAsset(D_8012D424, &asset);
    if (g_MenuScreen == 5) {
        OpenEncodedAsset(0x528, &asset);
        ReadEncodedAsset(D_801B5CD0[0], &asset);
        OpenEncodedAsset(0x528, &asset);
        ReadEncodedAsset(D_801B5CD0[1], &asset);
    } else {
        OpenEncodedAsset(D_800C2016, &asset);
        ReadEncodedAsset(D_801B5CD0[0], &asset);
        OpenEncodedAsset(D_800C2016, &asset);
        ReadEncodedAsset(D_801B5CD0[1], &asset);
    }
    D_801052BC[0] = AllocMemory(OpenEncodedAsset(*D_800C2028, &asset));
    ReadEncodedAsset(D_801052BC[0], &asset);
    D_801052BC[1] = AllocMemory(OpenEncodedAsset(*D_800C2028, &asset));
    ReadEncodedAsset(D_801052BC[1], &asset);
    for (index = 0; index != ARRAY_COUNT(D_801049A0); index++) {
        if (g_MenuScreen == 5) {
            size = OpenEncodedAsset(D_800C20E0[D_80105B68][index], &asset);
        } else {
            size = OpenEncodedAsset(D_800C2064[index], &asset);
        }
        D_801049A0[index] = AllocMemory(size);
        ReadEncodedAsset(D_801049A0[index], &asset);
        if (g_MenuScreen == 5) {
            size = OpenEncodedAsset(D_800C20E0[D_80105B68][index], &asset);
        } else {
            size = OpenEncodedAsset(D_800C2064[index], &asset);
        }
        D_8012F4B0[index] = AllocMemory(size);
        ReadEncodedAsset(D_8012F4B0[index], &asset);
    }
    for (index = 0; index != ARRAY_COUNT(D_801052C4); index++) {
        OpenAssetFile(D_800C20D4[index], &file);
        size = GetAssetFileSize(&file);
        D_801052C4[index] = AllocMemory(size);
        ReadAssetFile(D_801052C4[index], size, &file);
    }
    OpenEncodedAsset(*D_800C21B8, &asset);
    D_800FBDFC[0] = AllocMemory(0xFA0);
    ReadEncodedAsset(D_800FBDFC[0], &asset);
    OpenEncodedAsset(*D_800C21B8, &asset);
    D_800FBDFC[1] = AllocMemory(0xFA0);
    ReadEncodedAsset(D_800FBDFC[1], &asset);
    for (index = 0; index != ARRAY_COUNT(D_800FF4C4); index++) {
        OpenEncodedAsset(*D_800C21B8, &asset);
        D_800FF4C4[index] = AllocMemory(0x125C);
        ReadEncodedAsset(D_800FF4C4[index], &asset);
    }
    for (index = 0; index != ARRAY_COUNT(D_801B93E0); index++) {
        D_801B93E0[index] = AllocMemory(OpenEncodedAsset(0x720, &asset));
        ReadEncodedAsset(D_801B93E0[index], &asset);
    }
    size = OpenEncodedAsset(0x623, &asset);
    D_8010526C[0] = AllocMemory(size);
    D_8010526C[1] = AllocMemory(size);
    D_800FF180[0] = AllocMemory(size);
    D_800FF180[1] = AllocMemory(size);
    ReadEncodedAsset(D_8010526C[0], &asset);
    D_80106100 = AllocMemory(OpenEncodedAsset(0x65D, &asset));
    ReadEncodedAsset(D_80106100, &asset);
    D_800FC898 = AllocMemory(OpenEncodedAsset(0x663, &asset));
    ReadEncodedAsset(D_800FC898, &asset);
    for (index = 0; index != ARRAY_COUNT(D_8012D430); index++) {
        D_8012D430[index] = AllocMemory(OpenEncodedAsset(D_800C2650[index].assetId, &asset));
        ReadEncodedAsset(D_8012D430[index], &asset);
    }
    for (index = 0; index != ARRAY_COUNT(D_80105B78); index++) {
        OpenAssetFile(D_800C2590[index].assetId, &file);
        size = GetAssetFileSize(&file);
        D_80105B78[index] = AllocMemory(size);
        ReadAssetFile(D_80105B78[index], size, &file);
    }
    for (index = 0; index != ARRAY_COUNT(D_800FBE38); index++) {
        OpenAssetFile(D_800C2278[index], &file);
        size = GetAssetFileSize(&file);
        D_800FBE38[index] = AllocMemory(size);
        ReadAssetFile(D_800FBE38[index], size, &file);
    }
    D_800FBE30[0] = AllocMemory(OpenEncodedAsset(*D_800C28B0, &asset) * 2);
    ReadEncodedAsset(D_800FBE30[0], &asset);
    D_800FBE30[1] = AllocMemory(OpenEncodedAsset(*D_800C28B0, &asset) * 2);
    ReadEncodedAsset(D_800FBE30[1], &asset);
    D_800FC6A0 = AllocMemory(OpenEncodedAsset(0x6F7, &asset) * 2);
    ReadEncodedAsset(D_800FC6A0, &asset);
    D_801B69EC = AllocMemory(OpenEncodedAsset(0x709, &asset));
    ReadEncodedAsset(D_801B69EC, &asset);
    PlaySong(0x27, 0);
    SetFramebufferCount(3);
    SetScreenEffect(SCREEN_EFFECT_NONE, 0);
    D_801323D0 = 6;
    InitSram();
    TransferSaveData(save, FALSE);
}

/* Frees what LoadMenuResources allocated, as overlay 3 unloads */
void FreeMenuResources(void) {
    s32 index;

    D_800C1FF0 = -1;
    FreeMemory(&D_801F4A20);
    FreeMemory(&D_8012D3AC);
    for (index = 0; index != ARRAY_COUNT(D_801049A0); index++) {
        FreeMemory(&D_801049A0[index]);
        FreeMemory(&D_8012F4B0[index]);
    }
    FreeMemory(&D_801052C4[0]);
    FreeMemory(&D_801052C4[1]);
    FreeMemory(&D_800FF420[0]);
    FreeMemory(&D_800FF420[1]);
    FreeMemory(&D_801B60D0);
    FreeMemory(&D_8012D420[0]);
    FreeMemory(&D_801B5CD0[0]);
    FreeMemory(&D_8012D420[1]);
    FreeMemory(&D_801B5CD0[1]);
    FreeMemory(&D_801052BC[0]);
    FreeMemory(&D_801052BC[1]);
    FreeMemory(&D_800FBDFC[0]);
    FreeMemory(&D_800FBDFC[1]);
    for (index = 0; index != ARRAY_COUNT(D_800FF4C4); index++) {
        FreeMemory(&D_800FF4C4[index]);
    }
    for (index = 0; index != ARRAY_COUNT(D_800FBE38); index++) {
        FreeMemory(&D_800FBE38[index]);
    }
    for (index = 0; index != ARRAY_COUNT(D_801B93E0); index++) {
        FreeMemory(&D_801B93E0[index]);
    }
    FreeMemory(&D_8010526C[0]);
    FreeMemory(&D_8010526C[1]);
    FreeMemory(&D_800FF180[0]);
    FreeMemory(&D_800FF180[1]);
    FreeMemory(&D_80106100);
    FreeMemory(&D_800FC898);
    for (index = 0; index != ARRAY_COUNT(D_8012D430); index++) {
        FreeMemory(&D_8012D430[index]);
    }
    for (index = 0; index != ARRAY_COUNT(D_80105B78); index++) {
        FreeMemory(&D_80105B78[index]);
    }
    FreeMemory(&D_800FBE30[0]);
    FreeMemory(&D_800FBE30[1]);
    FreeMemory(&D_800FC6A0);
    FreeMemory(&D_801B69EC);
}

SaveData *GetSaveData(void) {
    func_8005AF74();
    return &g_SaveData;
}

void *func_8005AF74(void) {
    return D_801323A0;
}

void ResetSaveData(void) {
    SaveData *save;
    s8 row;
    s8 column;

    save = GetSaveData();
    memset(save, 0, sizeof(SaveData));
    save->magic = SAVE_MAGIC;
    for (row = 0; row != ARRAY_COUNT(save->bestTimes); row++) {
        for (column = 0; column != ARRAY_COUNT(save->bestTimes[0]); column++) {
            save->bestTimes[row][column] = -1;
        }
    }
    TransferSaveData(save, TRUE);
}

void InitSaveData(void) {
    SaveData *save;

    save = GetSaveData();
    InitSram();
    TransferSaveData(save, FALSE);
}

s32 CountOpenCourses(s32 coursePoints) {
    s32 courseIndex;

    for (courseIndex = COURSE_COUNT - 1; courseIndex != 0; courseIndex--) {
        if (coursePoints >= g_CoursePointsNeeded[courseIndex]) {
            break;
        }
    }
    return courseIndex + 1;
}

s32 GetCoursePointsNeeded(s32 courseIndex) {
    return g_CoursePointsNeeded[courseIndex];
}

s32 GetRecordPriority(Unk68 *record) {
    s32 value;
    s32 bits;
    s32 unk2C;
    s32 kind;
    s32 maxValue;

    bits = record->unk3A;
    kind = bits & 0xF;
    value = 0;
    unk2C = record->unk2C;
    maxValue = 0x64;
    if (bits & 0x80) {
        switch (kind) {
            case 3:
                value = 0x32;
                if (bits & 0x40) {
                    value = 0x3C;
                }
                break;
            case 2:
                value = 0x3C;
                if (bits & 0x40) {
                    value = 0x46;
                }
                if (unk2C < 5) {
                    value += 0xA;
                }
                break;
            case 0:
                value = 0x5A;
                if (unk2C < 4) {
                    break;
                }
                /* fallthrough */
            case 1:
                value = maxValue;
                break;
        }
    }
    return value;
}

/* Fills order with the indices of values from the largest value down, keeping ties in order */
void SortIndicesDescending(s32 *values, s32 *order, s32 count) {
    s32 sortedValues[count];
    s32 index;
    s32 searchIndex;
    s32 shiftIndex;
    s32 value;
    s32 valueIndex;

    for (index = 0; index < count; index++) {
        order[index] = index;
    }
    for (index = 0; index < count; index++) {
        sortedValues[index] = values[index];
    }
    for (index = 0; index < count; index++) {
        value = sortedValues[index];
        valueIndex = order[index];
        for (searchIndex = index - 1; searchIndex >= 0; searchIndex--) {
            if (sortedValues[searchIndex] >= value) {
                break;
            }
        }
        if (searchIndex < index - 1) {
            for (shiftIndex = index; searchIndex + 1 < shiftIndex; shiftIndex--) {
                sortedValues[shiftIndex] = sortedValues[shiftIndex - 1];
                order[shiftIndex] = order[shiftIndex - 1];
            }
            sortedValues[shiftIndex] = value;
            order[shiftIndex] = valueIndex;
        }
    }
}

void func_8005B28C(s32 recordIndex, Unk68 *record) {
    g_SaveData.unk1DF0[recordIndex] = *record;
    TransferSaveData(GetSaveData(), TRUE);
}

/* The save keeps the highest-priority records of each kind, this many each */
#define RANKED_GROUP_LENGTH 5

void InsertRankedRecord(Unk68 *record) {
    s32 priorities[RANKED_RECORD_COUNT];
    s32 order[RANKED_RECORD_COUNT];
    Unk68 copies[RANKED_RECORD_COUNT];
    s32 newPriority;
    Unk68 *records;
    s32 firstIndex;
    s32 lowestPriority;
    s32 lowestIndex;
    s32 index;
    s32 groupLength;

    osSyncPrintf("Checking SuperShot");
    if ((s8)record->unk3A & 0x80) {
        records = g_SaveData.rankedRecords;
        newPriority = GetRecordPriority(record);
        lowestPriority = 0x40000000; /* Above any priority */
        lowestIndex = 0;
        /* Every case sets the length, which the original needs to match */
        switch (record->unk3A & 0xF) {
            case 3:
                firstIndex = 0 * RANKED_GROUP_LENGTH;
                groupLength = RANKED_GROUP_LENGTH;
                break;
            case 2:
                firstIndex = 1 * RANKED_GROUP_LENGTH;
                groupLength = RANKED_GROUP_LENGTH;
                break;
            case 0:
                firstIndex = 2 * RANKED_GROUP_LENGTH;
                groupLength = RANKED_GROUP_LENGTH;
                break;
            case 1:
            default:
                firstIndex = 3 * RANKED_GROUP_LENGTH;
                groupLength = RANKED_GROUP_LENGTH;
                break;
        }
        osSyncPrintf("1:prior=%d:", newPriority);
        for (index = 0; index < groupLength; index++) {
            priorities[index] = GetRecordPriority(&records[firstIndex + index]);
        }
        for (index = 0; index < groupLength; index++) {
            if (lowestPriority >= priorities[index]) {
                lowestIndex = index;
                lowestPriority = priorities[index];
            }
        }
        if (newPriority >= lowestPriority) {
            /*
             * Rank the group with its lowest entry dropped into slots 1 on,
             * put the new record in slot 0, then rank all of them again
             */
            osSyncPrintf("2");
            priorities[lowestIndex] = 0;
            SortIndicesDescending(priorities, order, groupLength);
            for (index = 0; index < groupLength; index++) {
                copies[index] = records[firstIndex + index];
            }
            for (index = 0; index < groupLength - 1; index++) {
                records[firstIndex + index + 1] = copies[order[index]];
                priorities[index + 1] = GetRecordPriority(&records[firstIndex + index + 1]);
            }
            priorities[0] = newPriority;
            records[firstIndex] = *record;
            SortIndicesDescending(priorities, order, groupLength);
            for (index = 0; index < groupLength; index++) {
                copies[index] = records[firstIndex + index];
            }
            for (index = 0; index < groupLength; index++) {
                records[firstIndex + index] = copies[order[index]];
            }
            TransferSaveData(GetSaveData(), TRUE);
        }
    }
    osSyncPrintf("\n");
}

/* Player states that a saved game may hold; the first group restarts as 0 */
#define PLAYER_STATE_UNK0 0
#define PLAYER_STATE_UNK2 2
#define PLAYER_STATE_UNK3 3
#define PLAYER_STATE_UNK17 17
#define PLAYER_STATE_UNK20 20
#define PLAYER_STATE_UNK21 21

/*
 * Where each game mode keeps its game in the save's mode tables. Mode 4's
 * two records overlap mode 5's one.
 */
#define MODE4_GAME_INDEX 0
#define MODE4_PLAYER_INDEX 0
#define MODE4_RECORD_INDEX 0
#define MODE4_PLAYER_COUNT 2
#define MODE5_GAME_INDEX 1
#define MODE5_PLAYER_INDEX 2
#define MODE5_RECORD_INDEX 1
#define MODE5_PLAYER_COUNT 1
#define MODE0_GAME_INDEX 2
#define MODE0_PLAYER_INDEX 3
#define MODE0_RECORD_INDEX 2
#define MODE0_PLAYER_COUNT 4

s32 SaveOrContinueGameByMode(s32 isContinuing) {
    UnkB8 *players;
    Unk68 *records;
    SavedGame *game;
    s32 playerCount;
    s32 hasIllegalState;
    s32 index;

    playerCount = 0;
    switch (g_GameMode) {
        case 0:
        case 6:
        case 10:
            players = &g_SaveData.modePlayers[MODE0_PLAYER_INDEX];
            records = &g_SaveData.modeRecords[MODE0_RECORD_INDEX];
            game = &g_SaveData.modeGames[MODE0_GAME_INDEX];
            playerCount = MODE0_PLAYER_COUNT;
            break;
        case 5:
            players = &g_SaveData.modePlayers[MODE5_PLAYER_INDEX];
            records = &g_SaveData.modeRecords[MODE5_RECORD_INDEX];
            game = &g_SaveData.modeGames[MODE5_GAME_INDEX];
            osSyncPrintf("Record/Play -Tour-/\n");
            playerCount = MODE5_PLAYER_COUNT;
            break;
        case 4:
            players = &g_SaveData.modePlayers[MODE4_PLAYER_INDEX];
            records = &g_SaveData.modeRecords[MODE4_RECORD_INDEX];
            game = &g_SaveData.modeGames[MODE4_GAME_INDEX];
            playerCount = MODE4_PLAYER_COUNT;
            break;
    }
    if (!isContinuing) {
        hasIllegalState = FALSE;
        if (g_GameState.playerCount > playerCount) {
            osSyncPrintf("Too Many Players\n");
            return -1;
        }
        playerCount = g_GameState.playerCount;
        game->isValid = FALSE;
        for (index = 0; index < playerCount; index++) {
            players[index] = g_Players[index];
            switch (players[index].unkC) {
                case PLAYER_STATE_UNK0:
                case PLAYER_STATE_UNK2:
                case PLAYER_STATE_UNK3:
                case PLAYER_STATE_UNK21:
                    players[index].unkC = PLAYER_STATE_UNK0;
                    break;
                default:
                    osSyncPrintf("Illegal PlayerState\n");
                    hasIllegalState = TRUE;
                    break;
                case PLAYER_STATE_UNK17:
                case PLAYER_STATE_UNK20:
                    break;
            }
            records[index] = D_800FEDD8[index];
        }
        game->state = g_GameState;
        game->unk8 = D_800BA9D8;
        if (!hasIllegalState) {
            game->isValid = TRUE;
            return 0;
        }
        return -1;
    }
    if (game->isValid == TRUE) {
        if (game->state.playerCount > playerCount) {
            osSyncPrintf("Too Many Players\n");
            return -1;
        }
        playerCount = game->state.playerCount;
        for (index = 0; index < playerCount; index++) {
            g_Players[index] = players[index];
            D_800FEDD8[index] = records[index];
        }
        g_GameState = game->state;
        g_GameState.unk0 = 1;
        SetGameFlag(GAME_FLAG_UNK1E);
        D_800BA9D8 = game->unk8;
    }
    D_801B60B8 = 2;
    osSyncPrintf("Go Continue\n");
    return 0;
}

#define MAX_SAVE_COUNT 99

s32 SaveOrContinueGame(s32 slotIndex, s32 isContinuing) {
    UnkB8 *players;
    SavedGame *game;
    s32 playerCount;
    s32 hasIllegalState;
    s32 index;

    playerCount = MAX_PLAYER_COUNT; /* The limit, until the count is known */
    players = g_SaveData.savedPlayers[slotIndex];
    game = &g_SaveData.savedGames[slotIndex];
    if (!isContinuing) {
        hasIllegalState = FALSE;
        if (g_GameState.playerCount > playerCount) {
            osSyncPrintf("Too Many Players\n");
            return -1;
        }
        playerCount = g_GameState.playerCount;
        game->isValid = FALSE;
        for (index = 0; index < playerCount; index++) {
            players[index] = D_8012D0C8[index];
            switch (players[index].unkC) {
                case PLAYER_STATE_UNK0:
                case PLAYER_STATE_UNK2:
                case PLAYER_STATE_UNK3:
                case PLAYER_STATE_UNK21:
                    players[index].unkC = PLAYER_STATE_UNK0;
                    break;
                default:
                    osSyncPrintf("Illegal PlayerState\n");
                    hasIllegalState = TRUE;
                    break;
                case PLAYER_STATE_UNK17:
                case PLAYER_STATE_UNK20:
                    break;
            }
        }
        game->state = D_801B55F0;
        if (game->state.saveCount < MAX_SAVE_COUNT) {
            game->state.saveCount++;
        }
        game->unk8 = D_800BA9D8;
        game->roundStats = g_RoundStats;
        if (hasIllegalState) {
            return -1;
        }
        game->isValid = TRUE;
    } else {
        if (game->isValid == TRUE) {
            if (game->state.playerCount > playerCount) {
                osSyncPrintf("Too Many Players\n");
                return -1;
            }
            playerCount = game->state.playerCount;
            for (index = 0; index < playerCount; index++) {
                g_Players[index] = players[index];
            }
            g_GameState = game->state;
            g_GameState.unk0 = 1;
            SetGameFlag(GAME_FLAG_UNK1E);
            D_800BA9D8 = game->unk8;
            g_RoundStats = game->roundStats;
        }
        D_801B60B8 = 2;
        osSyncPrintf("Go Continue\n");
    }
    if (!isContinuing) {
        TransferSaveData(GetSaveData(), TRUE);
    }
    return 0;
}

void RecordDisplayListLength(u32 commandCount) {
    if (g_SaveData.longestDisplayListLength < commandCount) {
        g_SaveData.longestDisplayListLength = commandCount;
    }
}

/*
 * The longest time accepted. The overlay at ADD30 adds 18000 to the time
 * that is passed in here (g_GameState.stopwatchFrameCount) and caps it at this.
 */
#define MAX_TIME 108000
#define UNKE50_MAX 3

void InsertBestTime(s32 courseIndex, s32 time, s32 arg2, s32 arg3, s32 arg4) {
    SaveData *save;
    s32 count;
    s32 index;
    s32 runIndex;
    s32 scanIndex;
    s32 otherIndex;
    s32 unused[4]; /* The original frame has room for this */

    save = GetSaveData();
    if (courseIndex < 0) {
        courseIndex = 0;
    }
    if (courseIndex >= COURSE_COUNT) {
        courseIndex = COURSE_COUNT - 1;
    }
    if (time >= 0) {
        if (time <= MAX_TIME) {
            if (arg4 < 0) {
                arg4 = 0;
            }
            if (arg4 > UNKE50_MAX) {
                arg4 = UNKE50_MAX;
            }
            count = 0;
            for (index = 0; index != BEST_TIME_COUNT; index++) {
                count += save->bestTimes[courseIndex][index] > 0;
            }
            /*
             * Both sorts copy the smaller entry over the larger one instead
             * of swapping them, so the larger one is lost.
             */
            for (index = 0; index != count; index++) {
                for (scanIndex = index + 1; scanIndex != count; scanIndex++) {
                    if (save->bestTimes[courseIndex][index] > save->bestTimes[courseIndex][scanIndex]) {
                        save->bestTimes[courseIndex][index] = save->bestTimes[courseIndex][scanIndex];
                        save->unkE08[courseIndex][index] = save->unkE08[courseIndex][scanIndex];
                        save->unkD78[courseIndex][index] = save->unkD78[courseIndex][scanIndex];
                        save->unkE50[courseIndex][index] = save->unkE50[courseIndex][scanIndex];
                    }
                }
            }
            for (index = 0; index != count; index++) {
                for (scanIndex = index + 1; scanIndex != count; scanIndex++) {
                    if (save->unkD78[courseIndex][index] != save->unkD78[courseIndex][scanIndex]) {
                        break;
                    }
                }
                if (index + 1 < scanIndex) {
                    for (runIndex = index; runIndex != scanIndex; runIndex++) {
                        for (otherIndex = runIndex + 1; otherIndex != scanIndex; otherIndex++) {
                            if (save->unkD78[courseIndex][runIndex] > save->unkD78[courseIndex][otherIndex]) {
                                save->bestTimes[courseIndex][runIndex] = save->bestTimes[courseIndex][otherIndex];
                                save->unkE08[courseIndex][runIndex] = save->unkE08[courseIndex][otherIndex];
                                save->unkD78[courseIndex][runIndex] = save->unkD78[courseIndex][otherIndex];
                                save->unkE50[courseIndex][runIndex] = save->unkE50[courseIndex][otherIndex];
                            }
                        }
                    }
                }
            }
            if (count > 0) {
                for (index = 0; index != count; index++) {
                    if (time < save->bestTimes[courseIndex][index]) {
                        break;
                    }
                    if (time == save->bestTimes[courseIndex][index] && arg2 <= save->unkD78[courseIndex][index]) {
                        break;
                    }
                }
                if (index < BEST_TIME_COUNT) {
                    for (scanIndex = BEST_TIME_COUNT - 1; index < scanIndex; scanIndex--) {
                        save->bestTimes[courseIndex][scanIndex] = save->bestTimes[courseIndex][scanIndex - 1];
                        save->unkE08[courseIndex][scanIndex] = save->unkE08[courseIndex][scanIndex - 1];
                        save->unkD78[courseIndex][scanIndex] = save->unkD78[courseIndex][scanIndex - 1];
                        save->unkE50[courseIndex][scanIndex] = save->unkE50[courseIndex][scanIndex - 1];
                    }
                    save->bestTimes[courseIndex][index] = time;
                    save->unkE08[courseIndex][index] = arg3;
                    save->unkD78[courseIndex][index] = arg2;
                    save->unkE50[courseIndex][index] = arg4;
                }
            } else {
                save->bestTimes[courseIndex][0] = time;
                save->unkE08[courseIndex][0] = arg3;
                save->unkD78[courseIndex][0] = arg2;
                save->unkE50[courseIndex][0] = arg4;
            }
            TransferSaveData(save, TRUE);
        }
    }
}


s32 HasEveryRingShotStar(void) {
    SaveData *save;
    s32 starCount;
    s32 courseIndex;
    s32 holeIndex;

    save = GetSaveData();
    starCount = 0;
    for (courseIndex = 0; courseIndex != RING_SHOT_COURSE_COUNT; courseIndex++) {
        for (holeIndex = 0; holeIndex != RING_SHOT_HOLE_COUNT; holeIndex++) {
            starCount += save->hasRingShotStar[courseIndex][holeIndex] != FALSE;
        }
    }
    return starCount == RING_SHOT_COURSE_COUNT * RING_SHOT_HOLE_COUNT;
}

s32 IsDonkeyKongUnlocked(void) {
    SaveData *save;
    s32 starCount;
    s32 courseIndex;
    s32 holeIndex;

    save = GetSaveData();
    starCount = 0;
    for (courseIndex = 0; courseIndex != RING_SHOT_COURSE_COUNT; courseIndex++) {
        for (holeIndex = 0; holeIndex != RING_SHOT_HOLE_COUNT; holeIndex++) {
            starCount += save->hasRingShotStar[courseIndex][holeIndex] != FALSE;
        }
    }
    return starCount >= DONKEY_KONG_STAR_COUNT;
}

/* The Ring Shot course that opens at exactly the stars earned so far, or 0 for none */
s32 GetRingShotCourseOpenedAtStarCount(void) {
    SaveData *save;
    s32 starCount;
    s32 courseIndex;
    s32 holeIndex;

    save = GetSaveData();
    starCount = 0;
    for (courseIndex = 0; courseIndex != RING_SHOT_COURSE_COUNT; courseIndex++) {
        for (holeIndex = 0; holeIndex != RING_SHOT_HOLE_COUNT; holeIndex++) {
            starCount += save->hasRingShotStar[courseIndex][holeIndex] != FALSE;
        }
    }
    if (starCount == STARS_PER_RING_SHOT_COURSE * 1) {
        return 1;
    } else if (starCount == STARS_PER_RING_SHOT_COURSE * 2) {
        return 2;
    } else if (starCount == STARS_PER_RING_SHOT_COURSE * 3) {
        return 3;
    } else if (starCount == STARS_PER_RING_SHOT_COURSE * 4) {
        return 4;
    } else if (starCount == STARS_PER_RING_SHOT_COURSE * 5) {
        return 5;
    }
    return 0;
}

s32 IsMapleUnlocked(void) {
    SaveData *save;
    s32 badgeCount;
    s32 courseIndex;
    s32 holeIndex;

    save = GetSaveData();
    badgeCount = 0;
    for (courseIndex = 0; courseIndex != COURSE_COUNT; courseIndex++) {
        for (holeIndex = 0; holeIndex != COURSE_HOLE_COUNT; holeIndex++) {
            badgeCount += save->unk990.birdieBadgeCounts[courseIndex][holeIndex] != FALSE;
        }
    }
    return badgeCount >= MAPLE_BADGE_COUNT;
}

s32 IsMetalMarioUnlocked(void) {
    SaveData *save;
    s32 badgeCount;
    s32 courseIndex;
    s32 holeIndex;

    save = GetSaveData();
    badgeCount = 0;
    for (courseIndex = 0; courseIndex != COURSE_COUNT; courseIndex++) {
        for (holeIndex = 0; holeIndex != COURSE_HOLE_COUNT; holeIndex++) {
            badgeCount += save->unk990.birdieBadgeCounts[courseIndex][holeIndex] != FALSE;
        }
    }
    return badgeCount == COURSE_COUNT * COURSE_HOLE_COUNT;
}

/* arg0 is reused below as a record count and a character id, to match */
void func_8005C674(s32 arg0, s32 arg1) {
    EncodedAsset asset;
    AssetFile file;
    SaveData *save;
    s32 row;
    s32 column;
    s32 index;
    s32 recordIndex; /* Also holds a grid entry at the end, to match */

    save = GetSaveData();
    for (row = 0; row != CHARACTER_GRID_ROW_COUNT; row++) {
        for (column = 0; column != CHARACTER_GRID_COLUMN_COUNT; column++) {
            g_CharacterGrid[row][column] = g_StartingCharacterGrid[row][column];
        }
    }
    if (IsGameFlagSet(GAME_FLAG_ALL_CHARACTERS)) {
        for (row = 0; row != CHARACTER_CHOICE_ROW_COUNT; row++) {
            for (column = 0; column != CHARACTER_GRID_COLUMN_COUNT; column++) {
                g_CharacterGrid[row][column] = g_EveryCharacterGrid[row][column];
            }
        }
        func_8005CA48();
        return;
    }
    if (arg0 >= 2) {
        for (row = 0; row != CHARACTER_CHOICE_ROW_COUNT; row++) {
            for (column = 0; column != 5; column++) {
                g_CharacterGrid[row][column] = g_EveryCharacterGrid[row][column];
            }
        }
    }
    for (index = 0; index != ARRAY_COUNT(D_800C2968); index++) {
        recordIndex = 0;
        arg0 = 0;
        for (; recordIndex != SAVE_UNKAF0_ROW_COUNT; recordIndex++) {
            if (save->unk990.unkAF0[recordIndex][index].unk1 & 0x7F) {
                arg0++;
                break;
            }
        }
        if (arg0 > 0) {
            for (row = 0; row != CHARACTER_CHOICE_ROW_COUNT; row++) {
                for (column = 0; column != CHARACTER_GRID_COLUMN_COUNT; column++) {
                    if (D_800C293C[row][column] == index) {
                        g_CharacterGrid[row][column] = D_800C2968[index];
                    }
                }
            }
        }
    }
    if (IsMapleUnlocked()) {
        g_CharacterGrid[0][5] = CHARACTER_MAPLE;
    }
    if (IsDonkeyKongUnlocked()) {
        g_CharacterGrid[1][5] = CHARACTER_DONKEY_KONG;
    }
    if (IsMetalMarioUnlocked()) {
        g_CharacterGrid[1][6] = CHARACTER_METAL_MARIO;
    }
    func_8005CA48();
    for (index = 0; index != MARKABLE_CHARACTER_COUNT; index++) {
        row = D_800C29B8[index].row;
        column = D_800C29B8[index].column;
        if (g_CharacterGrid[row][column] < 0) {
            g_CharacterGrid[row][column] = index - MARKED_CHARACTER_OFFSET;
            break;
        }
    }
    if (index == MARKABLE_CHARACTER_COUNT) {
        if (g_CharacterGrid[0][6] != CHARACTER_BOWSER) {
            g_CharacterGrid[0][6] = CHARACTER_BOWSER - MARKED_CHARACTER_OFFSET;
        }
    }
    if ((u8)arg1 != 1) {
        for (index = 0; index != ARRAY_COUNT(g_ImportedCharacters); index++) {
            recordIndex = g_CharacterGrid[2][index];
            if (recordIndex >= 0) {
                arg0 = g_ImportedCharacters[index].character;
                OpenEncodedAsset(D_800C2650[arg0].assetId, &asset);
                ReadEncodedAsset(D_8012D468[index], &asset);
                OpenAssetFile(D_800C2590[arg0].assetId, &file);
                ReadAssetFile(D_80105BB0[index], GetAssetFileSize(&file), &file);
            }
        }
    }
}

void func_8005CA48(void) {
    s16 index;
    s16 byteIndex;
    s16 count; /* Also the byte index of the first loops, to match */
    u8 *name;
    u8 letter;

    for (index = 0; index != IMPORTED_CHARACTER_COUNT; index++) {
        g_ImportedCharacters[index].character = 0;
        g_ImportedCharacters[index].unk2 = 0;
        g_ImportedCharacters[index].unk4 = 0;
        g_ImportedCharacters[index].unk6 = 0;
        for (count = 0; count != IMPORTED_CHARACTER_NAME_LENGTH; count++) {
            g_ImportedCharacters[index].name[count] = 0;
        }
    }
    for (index = 0; index != IMPORTED_CHARACTER_COUNT; index++) {
        for (count = 0; count != IMPORTED_CHARACTER_NAME_LENGTH; count++) {
            D_800C2AA8[index][count] = 0;
        }
    }
    count = 0;
    for (index = 0; index != IMPORTED_CHARACTER_COUNT; index++) {
        if (g_GameBoyCharacters[index].status == GAME_BOY_CHARACTER_STATUS_PRESENT) {
            count++;
        }
    }
    if (IsGameFlagSet(GAME_FLAG_ALL_CHARACTERS)) {
        count = IMPORTED_CHARACTER_COUNT;
    }
    if (count > 0) {
        if (count > IMPORTED_CHARACTER_COUNT) {
            count = IMPORTED_CHARACTER_COUNT;
        }
        for (index = 0; index != count; index++) {
            if (IsGameFlagSet(GAME_FLAG_ALL_CHARACTERS)) {
                name = D_800C2B60[index];
            } else {
                name = g_GameBoyCharacters[index].name;
            }
            for (byteIndex = 0; byteIndex != IMPORTED_CHARACTER_NAME_LENGTH; byteIndex++) {
                letter = *name++;
                D_800C2AA8[index][byteIndex] = letter;
                g_ImportedCharacters[index].name[byteIndex] = letter;
            }
            g_CharacterGrid[D_800C2B90[index].row][D_800C2B90[index].column] = FIRST_IMPORTED_CHARACTER + index;
            if (IsGameFlagSet(GAME_FLAG_ALL_CHARACTERS)) {
                g_ImportedCharacters[index].character = D_800C2B40[index];
                g_ImportedCharacters[index].unk2 = D_800C2B48[index];
                g_ImportedCharacters[index].unk4 = D_800C2B50[index];
                g_ImportedCharacters[index].unk6 = D_800C2B58[index];
                g_Clubs[(FIRST_IMPORTED_CHARACTER + index) * CLUB_SET_SIZE].distance = D_800C2B48[index];
                g_Clubs[(FIRST_IMPORTED_CHARACTER + index) * CLUB_SET_SIZE].unk12 = D_800C2B50[index];
                g_Clubs[(FIRST_IMPORTED_CHARACTER + index) * CLUB_SET_SIZE].unk26 = D_800C2B58[index];
            } else {
                g_ImportedCharacters[index].character = g_GameBoyCharacters[index].character + FIRST_IMPORTED_CHARACTER;
                g_ImportedCharacters[index].unk2 = g_Clubs[(FIRST_IMPORTED_CHARACTER + index) * CLUB_SET_SIZE].distance;
                g_ImportedCharacters[index].unk4 = g_Clubs[(FIRST_IMPORTED_CHARACTER + index) * CLUB_SET_SIZE].unk12;
                g_ImportedCharacters[index].unk6 = g_Clubs[(FIRST_IMPORTED_CHARACTER + index) * CLUB_SET_SIZE].unk26;
            }
        }
    }
}

s32 func_8005CEE0(s32 index) {
    s32 slot;
    s32 row;
    s32 column;

    if (index < 0) {
        return -1;
    }
    if (index >= ARRAY_COUNT(D_800C2BB0)) {
        return -1;
    }
    func_8005C674(1, 1);
    slot = D_800C2BB0[index];
    row = D_800C29B8[slot].row;
    column = D_800C29B8[slot].column;
    return g_CharacterGrid[row][column] < 0 ? -1 : 0;
}

/* Marks saved games whose settings or players are out of range as no longer valid */
void InvalidateBrokenSavedGames(void) {
    SaveData *save;
    s32 isInvalid[SAVED_GAME_COUNT];
    s32 index;
    s32 entryIndex;
    SavedGame *unk;
    GameState *sub;
    UnkB8 *player;

    save = GetSaveData();
    for (index = 0; index != SAVED_GAME_COUNT; index++) {
        isInvalid[index] = FALSE;
    }
    for (index = 0; index != SAVED_GAME_COUNT; index++) {
        if (!save->savedGames[index].isValid) {
            continue;
        }
        unk = &save->savedGames[index];
        sub = &unk->state;
        if (sub->isMiniGolf == FALSE) {
            switch (sub->gameMode) {
                case GAME_MODE_STROKE:
                case GAME_MODE_MATCH_GAME:
                case GAME_MODE_GET_CHARACTER:
                case GAME_MODE_TOURNAMENT:
                case GAME_MODE_SKINS_MATCH:
                case GAME_MODE_CLUB_SLOTS:
                    break;
                default:
                    isInvalid[index] = TRUE;
                    break;
            }
        }
        if (sub->playerCount < 1 || sub->playerCount > MAX_PLAYER_COUNT) {
            isInvalid[index] = TRUE;
        }
        if (sub->courseIndex < 0 || sub->courseIndex >= DRIVING_RANGE_COURSE_INDEX) {
            isInvalid[index] = TRUE;
        }
        if (sub->holeIndex < 0 || sub->holeIndex >= COURSE_HOLE_COUNT) {
            isInvalid[index] = TRUE;
        }
        for (entryIndex = 0; entryIndex != sub->playerCount; entryIndex++) {
            player = &save->savedPlayers[index][entryIndex];
            if (player->characterIndex >= CHARACTER_COUNT) {
                isInvalid[index] = TRUE;
            }
            if (player->paletteIndex >= CHARACTER_PALETTE_COUNT) {
                isInvalid[index] = TRUE;
            }
        }
    }
    for (index = 0; index != SAVED_GAME_COUNT; index++) {
        if (isInvalid[index] == TRUE) {
            save->savedGames[index].isValid = FALSE;
        }
    }
}

#ifdef NON_MATCHING
void func_8005D0D8(void) {
    SaveData *save;
    s32 isInvalid[SAVE_RECORD_COUNT];
    s32 recordIndex;
    Unk68 *record;
    s32 kind;
    s32 unused[2]; /* The original frame has room for this */

    save = GetSaveData();
    for (recordIndex = 0; recordIndex != SAVE_RECORD_COUNT; recordIndex++) {
        isInvalid[recordIndex] = FALSE;
    }
    for (recordIndex = 0; recordIndex != SAVE_RECORD_COUNT; recordIndex++) {
        record = &save->unk1DF0[recordIndex];
        kind = record->unk36;
        if (record->unk36 & 0x80) {
            kind &= 0xF;
            if (kind >= 5 || kind < 0) {
                isInvalid[recordIndex] = TRUE;
            }
        }
        if (record->unk37 >= COURSE_HOLE_COUNT) {
            isInvalid[recordIndex] = TRUE;
        }
        if (record->unk39 >= 4) {
            isInvalid[recordIndex] = TRUE;
        }
        if (record->unk34 >= 6) {
            isInvalid[recordIndex] = TRUE;
        }
        if (record->unk35 >= 6) {
            isInvalid[recordIndex] = TRUE;
        }
    }
    /* Toggling rather than clearing sets the flag on records that fail with it clear */
    for (recordIndex = 0; recordIndex != SAVE_RECORD_COUNT; recordIndex++) {
        record = &save->unk1DF0[recordIndex];
        if (isInvalid[recordIndex] == TRUE) {
            record->unk36 ^= 0x80;
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/menu", func_8005D0D8);
#endif


void func_8005D1E4(void) {
    u8 *unk;

    unk = func_8005AF74();
    unk[0x2E] = g_HoleIndex;
    unk[0x2F] = g_CourseIndex;
}

s32 GetClubTournamentGameMode(void) {
    s32 value;

    switch (D_80105DC1) {
        case 0:
        default:
            value = 0;
            break;
        case 1:
            value = 7;
            break;
    }
    return value;
}


s32 GetClubTournamentCourse(void) {
    return D_80105DC2;
}

s32 IsNintendoClubTournament(void) {
    return D_80105DC4 != 0;
}

s32 GetClubTournamentNameMessage(void) {
    s32 value;

    value = D_80105DC8;
    if (value < 0) {
        value = 0;
    }
    return value + 0x16F;
}

s32 GetClubTournamentCourseMessage(void) {
    s32 value;

    value = GetClubTournamentCourse();
    if (value < 0) {
        value = 0;
    }
    return value + 0x1EB;
}

s32 GetClubTournamentNumber(void) {
    s32 value;

    value = D_80105DCC;
    if (value <= 0) {
        value = 1;
    }
    return value;
}

void CopyTournamentCode(u8 *destination) {
    s32 index;

    for (index = 0; index != ARRAY_COUNT(g_TournamentCode); index++) {
        *destination++ = g_TournamentCode[index];
    }
}

s32 GetClubTournamentModeMessage(void) {
    s32 value;

    switch (D_80105DC1) {
        case 0:
        default:
            value = 0x9E;
            break;
        case 1:
            value = 0x9D;
            break;
    }
    return value;
}


s32 GetClubTournamentCourseTitleMessage(void) {
    s32 value;

    if (D_80105DC2 >= 6) {
        value = 0xC7;
        if (D_80105DC2 >= 9) {
            value = 0xC8;
        }
    } else {
        value = D_80105DC2 + 0x1EB;
    }
    return value;
}


void func_8005D334(void) {
    s32 index;
    s32 byteIndex;

    for (index = 0; index != ARRAY_COUNT(g_ImportedCharacters); index++) {
        g_ImportedCharacters[index].character = 0;
        g_ImportedCharacters[index].unk2 = 0;
        g_ImportedCharacters[index].unk4 = 0;
        g_ImportedCharacters[index].unk6 = 0;
        for (byteIndex = 0; byteIndex != IMPORTED_CHARACTER_NAME_LENGTH; byteIndex++) {
            g_ImportedCharacters[index].name[byteIndex] = 0;
        }
        g_ImportedCharacters[index].unk14 = 0;
    }
}

void ComputeRoundStatistics(void) {
    UnkE98 *saved;
    UnkE98 *entry;
    f32 reciprocal;
    s32 sum;
    s32 courseIndex;
    s32 hasPlayedCourse;
    f32 sum8;
    f32 sum18;
    f32 sum1C;
    f32 sum20;
    f32 sum24;
    f32 sum28;
    f32 sum28And2C;
    f32 sum10;
    s32 sum14;
    f32 zero;
    f32 emptyCount; /* Only ever zero */
    f32 holeCount;

    saved = GetSaveData()->unkE98;
    for (courseIndex = 0; courseIndex != COURSE_COUNT; courseIndex++) {
        entry = &saved[courseIndex];
        D_80105C38[courseIndex].roundCount = entry->roundCount;
        if (D_80105C38[courseIndex].roundCount == 0.0f) {
            D_80105C38[courseIndex].unk4 = 0.0f;
            D_80105C38[courseIndex].unk8 = 0.0f;
            D_80105C38[courseIndex].unkC = 0.0f;
            D_80105C38[courseIndex].unk10 = 0.0f;
            D_80105C38[courseIndex].unk14 = 0.0f;
            D_80105C38[courseIndex].unk18 = 0.0f;
            D_80105C38[courseIndex].unk1C = 0.0f;
            D_80105C38[courseIndex].unk24 = 0.0f;
            D_80105C38[courseIndex].unk20 = 0.0f;
            D_80105C38[courseIndex].hasThirdPlace = FALSE;
            D_80105C38[courseIndex].hasSecondPlace = FALSE;
            D_80105C38[courseIndex].hasFirstPlace = FALSE;
        } else {
            /* unk14 is left as it was */
            reciprocal = 1.0f / entry->roundCount;
            D_80105C38[courseIndex].unk4 = entry->bestScore;
            D_80105C38[courseIndex].unk8 = entry->strokeTotal * reciprocal;
            D_80105C38[courseIndex].unkC = entry->longestDrive * UNK_SCALE;
            D_80105C38[courseIndex].unk10 = entry->averageDrive * UNK_SCALE;
            D_80105C38[courseIndex].unk18 = entry->measuredDriveTotal * reciprocal * (100.0f / HOLES_PER_ROUND);
            D_80105C38[courseIndex].unk1C = entry->puttTotal * reciprocal * (1.0f / HOLES_PER_ROUND);
            D_80105C38[courseIndex].unk20 = entry->parOrBetterCount * reciprocal * (100.0f / HOLES_PER_ROUND);
            D_80105C38[courseIndex].unk24 = entry->parOnCount * reciprocal * (100.0f / HOLES_PER_ROUND);
            sum = entry->unk28 + entry->unk2C;
            if (sum <= 0) {
                D_80105C38[courseIndex].unk28 = 0.0f;
            } else {
                D_80105C38[courseIndex].unk28 = ((f32)entry->unk28 / (f32)sum) * 100.0f;
            }
            D_80105C38[courseIndex].hasThirdPlace = entry->hasPlace[THIRD_PLACE_INDEX];
            D_80105C38[courseIndex].hasSecondPlace = entry->hasPlace[SECOND_PLACE_INDEX];
            D_80105C38[courseIndex].hasFirstPlace = entry->hasPlace[FIRST_PLACE_INDEX];
        }
    }

    courseIndex = 0; /* Zeroed this early to match */
    hasPlayedCourse = FALSE;
    D_80105C38[COURSE_COUNT].roundCount = 0.0f;
    D_80105C38[COURSE_COUNT].unk4 = 0.0f;
    D_80105C38[COURSE_COUNT].unk8 = 0.0f;
    D_80105C38[COURSE_COUNT].unkC = 0.0f;
    D_80105C38[COURSE_COUNT].unk10 = 0.0f;
    D_80105C38[COURSE_COUNT].unk18 = 0.0f;
    D_80105C38[COURSE_COUNT].unk1C = 0.0f;
    D_80105C38[COURSE_COUNT].unk20 = 0.0f;
    D_80105C38[COURSE_COUNT].unk24 = 0.0f;
    D_80105C38[COURSE_COUNT].unk28 = 0.0f;
    D_80105C38[COURSE_COUNT].hasThirdPlace = FALSE;
    D_80105C38[COURSE_COUNT].hasSecondPlace = FALSE;
    D_80105C38[COURSE_COUNT].hasFirstPlace = FALSE;
    D_80105C38[COURSE_COUNT].unk4 = 0.0f;
    D_80105C38[COURSE_COUNT].unkC = 0.0f;
    D_80105C38[COURSE_COUNT].unk4 = D_80105C38[0].unk4;
    D_80105C38[COURSE_COUNT].unkC = D_80105C38[0].unkC;
    zero = D_80105C38[COURSE_COUNT].roundCount; /* Read back after being zeroed */
    for (; courseIndex != COURSE_COUNT; courseIndex++) {
        if (zero < D_80105C38[courseIndex].roundCount) {
            if (!hasPlayedCourse) {
                D_80105C38[COURSE_COUNT].unk4 = D_80105C38[courseIndex].unk4;
                hasPlayedCourse = TRUE;
                D_80105C38[COURSE_COUNT].unkC = D_80105C38[courseIndex].unkC;
            }
            if (D_80105C38[COURSE_COUNT].unk4 > D_80105C38[courseIndex].unk4) {
                D_80105C38[COURSE_COUNT].unk4 = D_80105C38[courseIndex].unk4;
            }
            if (D_80105C38[COURSE_COUNT].unkC < D_80105C38[courseIndex].unkC) {
                D_80105C38[COURSE_COUNT].unkC = D_80105C38[courseIndex].unkC;
            }
            D_80105C38[COURSE_COUNT].roundCount += D_80105C38[courseIndex].roundCount;
        }
    }

    sum14 = 0;
    sum28 = 0.0f;
    sum28And2C = 0.0f;
    sum20 = 0.0f;
    sum18 = 0.0f;
    sum24 = 0.0f;
    sum1C = 0.0f;
    sum10 = 0.0f;
    sum8 = 0.0f;
    emptyCount = 0.0f;
    for (courseIndex = 0; courseIndex != COURSE_COUNT; courseIndex++) {
        entry = &saved[courseIndex];
        if (emptyCount < D_80105C38[courseIndex].roundCount) {
            sum18 += entry->measuredDriveTotal;
            sum1C += entry->puttTotal;
            sum20 += entry->parOrBetterCount;
            sum24 += entry->parOnCount;
            sum8 += entry->strokeTotal;
            sum28 += entry->unk28;
            sum28And2C += entry->unk28 + entry->unk2C;
            sum14 += entry->averagedDriveCount;
            sum10 += entry->averageDrive * UNK_SCALE * entry->averagedDriveCount;
        }
    }
    if (D_80105C38[COURSE_COUNT].roundCount > 0.0f) {
        if (sum28And2C <= 0.0f) {
            D_80105C38[COURSE_COUNT].unk28 = 0.0f;
        } else {
            D_80105C38[COURSE_COUNT].unk28 = (sum28 / sum28And2C) * 100.0f;
        }
        holeCount = D_80105C38[COURSE_COUNT].roundCount * HOLES_PER_ROUND;
        D_80105C38[COURSE_COUNT].unk1C = sum1C / holeCount;
        D_80105C38[COURSE_COUNT].unk18 = (sum18 / holeCount) * 100.0f;
        D_80105C38[COURSE_COUNT].unk20 = (sum20 / holeCount) * 100.0f;
        D_80105C38[COURSE_COUNT].unk24 = (sum24 / holeCount) * 100.0f;
        D_80105C38[COURSE_COUNT].unk8 = sum8 / D_80105C38[COURSE_COUNT].roundCount;
        if (sum10 > 0.0f && sum14 > 0) {
            D_80105C38[COURSE_COUNT].unk10 = sum10 / sum14;
        } else {
            D_80105C38[COURSE_COUNT].unk10 = 0.0f;
        }
    }
}

void func_8005D9A0(void) {
    s8 index;

    D_800C2BD0 = 6;
    D_800FE334 = 0;
    g_MenuScreen = 0;
    D_80105B68 = 0;
    D_801B7070 = 0;
    D_8012F480 = -1;
    D_8012F484 = -1;
    D_8012F488 = -1;
    D_8012F48C = -1;
    D_801B70F0 = 0;
    D_801B70F1 = 0;
    D_801B70F2 = 0;
    D_801B70F3 = 0;
    memset(D_801323A0, 0, sizeof(D_801323A0));
    D_800FBD40 = 0;
    D_801B71CC = 0;
    D_800FBE2C = 0;
    D_800FBDB9 = 0;
    D_800FE33C = 0;
    D_800FF4C0 = 0;
    g_MapCamera[0x4C] = 0;
    for (index = 0; index != ARRAY_COUNT(D_800FBE08); index++) {
        D_800FBE08[index] = 0;
    }
    g_MenuChoices.unk0 = 3;
    g_GameState.holeSelection = HOLE_SELECTION_ALL;
    D_801323D0 = 6;
    D_801052E8 = 0;
    D_800C2BD4 = 0;
}

void func_8005DAD4(void) {
    D_800FE334 = 0;
    D_801052E8 = 1;
    D_801B9410 = 0;
    D_800C2BD4 = 0;
}

void func_8005DAFC(void) {
    switch (g_GameMode) {
        case 5:
            D_800FF4C0 = 2;
            g_MapCamera[0x4C] = 0;
            D_801B5634 = 0;
            break;
        case 0:
            D_800FF4C0 = 1;
            g_MapCamera[0x4C] = 1;
            if (D_80105B68 == 4) {
                D_801B5634 = 2;
            } else {
                D_801B5634 = 1;
            }
            D_800C2BD5 = 1;
            break;
        case 2:
            D_800FF4C0 = 1;
            g_MapCamera[0x4C] = 1;
            D_801B5634 = 3;
            break;
        case 10:
            D_800FF4C0 = 1;
            g_MapCamera[0x4C] = 1;
            D_801B5634 = 4;
            break;
        case 3:
        case 4:
        case 6:
        case 7:
        case 9:
            D_800FF4C0 = 1;
            g_MapCamera[0x4C] = 2;
            D_801B5634 = 0;
            break;
        case 11:
            D_800FF4C0 = 1;
            g_MapCamera[0x4C] = 1;
            D_801B5634 = 5;
            break;
    }
    D_801052E8 = 1;
    D_801B9410 = 1;
    D_800FE334 = 0;
    D_800C2BD4 = 0;
    g_MenuScreen = 6;
}

void func_8005DC50(void) {
    func_8005D9A0();
    D_801052E8 = 1;
    D_801B9410 = 1;
    D_800FE334 = 0;
    D_800C2BD4 = 0;
    g_ImportedCharacters[0].unk14 = 0;
    g_ImportedCharacters[1].unk14 = 0;
    g_ImportedCharacters[2].unk14 = 0;
    g_ImportedCharacters[3].unk14 = 0;
    D_800C1FF4 = -1;
    D_800C1FF5 = -1;
    D_800C1FF6 = -1;
    D_800C1FF7 = -1;
    g_MenuScreen = 0;
}

void func_8005DCDC(void) {
    D_801052E8 = 1;
    D_801B9410 = 1;
    D_800FE334 = 0;
    D_800C2BD4 = 0;
    g_MenuScreen = 0x17;
}

void func_8005DD10(void) {
    D_801052E8 = 1;
    D_801B9410 = 1;
    D_800FE334 = 0;
    D_800C2BD4 = 0;
    g_MenuScreen = 5;
}

void func_8005DD44(void) {
    D_801052E8 = 1;
    D_801B9410 = 1;
    D_800FE334 = 0;
    D_800C2BD4 = 0;
    g_MenuScreen = 0x12;
}

void func_8005DD78(void) {
    D_801052E8 = 1;
    D_801B9410 = 1;
    D_800FE334 = 0;
    D_800C2BD4 = 0;
    g_MenuScreen = 0xD;
}

void func_8005DDAC(void) {
    D_800FE334 = 0;
    D_801052E8 = 1;
    D_800C2BD4 = 0;
    D_801B9410 = 1;
    g_GameState.unk31 = 0;
    ClearGameFlag(GAME_FLAG_UNK1E);
    g_MenuScreen = 0xC;
}

s8 func_8005DE00(void) {
    return GetSaveData()->options[SAVE_OPTION_UNK0];
}

s8 func_8005DE20(void) {
    return GetSaveData()->options[SAVE_OPTION_MONO];
}

s8 func_8005DE40(void) {
    return GetSaveData()->options[SAVE_OPTION_UNK3];
}

void WriteSaveData(void) {
    TransferSaveData(GetSaveData(), TRUE);
}

/* Copies the options between the save data and g_OptionBits, in the direction of the transfer */
void SyncSaveOptions(SaveData *save, s32 isWrite, s32 readResult) {
    u8 *bits;
    s8 *options;

    options = save->options;
    if (!isWrite) {
        bits = &g_OptionBits;
        *bits = 0;
        if (readResult > 0) {
            if (options[SAVE_OPTION_UNK0]) {
                *bits = OPTION_BIT_UNK0;
            }
            if (options[SAVE_OPTION_MONO]) {
                *bits |= OPTION_BIT_MONO;
                g_SoundOptionBits = *bits;
            }
            if (options[SAVE_OPTION_UNK3]) {
                *bits |= OPTION_BIT_UNK3;
            }
        }
    } else {
        if (g_OptionBits & OPTION_BIT_UNK0) {
            options[SAVE_OPTION_UNK0] = TRUE;
        } else {
            options[SAVE_OPTION_UNK0] = FALSE;
        }
        if (g_OptionBits & OPTION_BIT_MONO) {
            options[SAVE_OPTION_MONO] = TRUE;
        } else {
            options[SAVE_OPTION_MONO] = FALSE;
        }
        if (g_OptionBits & OPTION_BIT_UNK3) {
            options[SAVE_OPTION_UNK3] = TRUE;
        } else {
            options[SAVE_OPTION_UNK3] = FALSE;
        }
    }
}

void TransferSaveData(SaveData *save, u8 isWrite) {
    u8 isSramSelected;
    u8 isInSram;
    s32 readResult;

    isSramSelected = !IsGameFlagSet(GAME_FLAG_SAVE_TO_HOST);
    isInSram = isSramSelected;
    if (!isWrite) {
        readResult = ReadSaveCopies(save, isInSram);
        SyncSaveOptions(save, FALSE, readResult);
    } else {
        /* readResult is never set on this path, but SyncSaveOptions ignores it when writing */
        SyncSaveOptions(save, TRUE, readResult);
        WriteSaveCopies(save, isInSram);
    }
}


/* Reads the first intact copy; if neither is, the save data starts over */
s32 ReadSaveCopies(SaveData *save, u8 isInSram) {
    u8 unusedBuffer[0x90]; /* Only takes up stack */
    u8 checkBuffer[0x40];
    SaveCheck *check;
    u32 size;
    s32 unmarkedCount;
    s32 copyIndex;
    u16 crc;

    check = (SaveCheck *)ALIGN_FOR_DMA(checkBuffer);
    unmarkedCount = 0;
    for (copyIndex = 0; copyIndex < SAVE_COPY_COUNT; copyIndex++) {
        if (!isInSram) {
            if (ReadHostFile(g_SaveCopyLocations[copyIndex].dataFileName, &size) != HOST_FILE_OK ||
                size != sizeof(SaveData)) {
                continue;
            }
            if (ReadHostFile(g_SaveCopyLocations[copyIndex].checkFileName, &size) != HOST_FILE_OK ||
                size != sizeof(SaveCheck)) {
                continue;
            }
        } else {
            nuPiReadSram(g_SaveCopyLocations[copyIndex].dataSramAddress, save, sizeof(SaveData));
            nuPiReadSram(g_SaveCopyLocations[copyIndex].checkSramAddress, check, sizeof(SaveCheck));
        }
        if (save->magic != SAVE_MAGIC) {
            unmarkedCount++;
            continue;
        }
        crc = check->keyedCrc ^ check->key;
        if (crc == ComputeCrc16((u8 *)save, sizeof(SaveData))) {
            break;
        }
    }
    if (copyIndex == 0) {
        return SAVE_READ_FIRST_COPY;
    }
    if (copyIndex == 1) {
        return SAVE_READ_SECOND_COPY;
    }
    ResetSaveData();
    return (unmarkedCount == SAVE_COPY_COUNT) ? SAVE_READ_NEVER_WRITTEN : SAVE_READ_FAILED;
}

s32 WriteSaveCopies(SaveData *save, u8 isInSram) {
    u8 checkBuffer[0xC0];
    SaveCheck *check;
    u16 crc;
    s32 dateIndex;
    s32 copyIndex;

    check = (SaveCheck *)ALIGN_FOR_DMA(checkBuffer);
    for (dateIndex = 0; dateIndex < ARRAY_COUNT(save->invertedBuildDate); dateIndex++) {
        save->invertedBuildDate[dateIndex] = ~g_RomBuildDate[dateIndex];
    }
    crc = ComputeCrc16((u8 *)save, sizeof(SaveData));
    for (copyIndex = 0; copyIndex < SAVE_COPY_COUNT; copyIndex++) {
        check->key = rand();
        check->keyedCrc = check->key ^ crc;
        if (!isInSram) {
            if (WriteHostFile(g_SaveCopyLocations[copyIndex].dataFileName, sizeof(SaveData)) == HOST_FILE_OK) {
                WriteHostFile(g_SaveCopyLocations[copyIndex].checkFileName, sizeof(SaveCheck));
            }
        } else {
            nuPiWriteSram(g_SaveCopyLocations[copyIndex].dataSramAddress, save, sizeof(SaveData));
            nuPiWriteSram(g_SaveCopyLocations[copyIndex].checkSramAddress, check, sizeof(SaveCheck));
        }
    }
    return TRUE;
}

