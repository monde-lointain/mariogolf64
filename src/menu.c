/*
 * The save data and what is kept in it: reading and writing the save, the
 * records and best times, what is unlocked, the Club Tournaments and the round
 * statistics. It also loads the menus' resources.
 */
#include "common.h"

#include "menu.h"

#include <lib/libnusys/nusys.h>

#include "asset.h"
#include "character.h"
#include "club.h"
#include "course.h"
#include "game_flags.h"
#include "heap.h"
#include "host_file.h"
#include "libc.h"
#include "math_util.h"
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

/* The SRAM check buffer is read and written by DMA, so it starts on a cache
 * line */
#define DMA_ALIGNMENT 0x10

#define ALIGN_FOR_DMA(address) \
  (((u32)(address) + DMA_ALIGNMENT - 1) & ~(DMA_ALIGNMENT - 1))

/* The records that unlock two of the characters */
#define DONKEY_KONG_STAR_COUNT 30
#define MAPLE_BADGE_COUNT 50

/* Each Ring Shot course after the first opens after this many more stars */
#define STARS_PER_RING_SHOT_COURSE 4

/* Mirrors of the save data's options */
extern u8 g_OptionBits;

extern SaveCopyLocation f_SaveCopyLocations[];
/* The course points needed in total to open each course */
extern s32 f_CoursePointsNeeded[COURSE_COUNT];
extern char g_RomBuildDate[];

typedef struct Unk800C2B90 {
  /* 0x0 */ s16 unk0;
  /* 0x2 */ s16 column;
  /* 0x4 */ s16 unk4;
  /* 0x6 */ s16 row;
} Unk800C2B90; /* size = 0x8 */

extern Unk800C2B90 f_ImportedCharacterCells[IMPORTED_CHARACTER_COUNT];
extern s8 f_ImportedCharacterNames[IMPORTED_CHARACTER_COUNT]
                                  [IMPORTED_CHARACTER_NAME_LENGTH];
/* The imported characters that debug flag 0x54 adds */
extern s16 f_DebugImportedCharacters[IMPORTED_CHARACTER_COUNT];
extern s16 f_DebugImportedDistances[IMPORTED_CHARACTER_COUNT];
extern s16 f_DebugImportedSpins[IMPORTED_CHARACTER_COUNT];
extern s16 f_DebugImportedSideSpins[IMPORTED_CHARACTER_COUNT];
extern u8 f_DebugImportedNames[IMPORTED_CHARACTER_COUNT]
                              [IMPORTED_CHARACTER_NAME_LENGTH];

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
extern u8 D_800FF4C0;
extern s8 D_801B5634;
extern s8 D_801052E8;
extern s32 g_ChosenPlayerCount;
extern s32 D_80105DC4;
extern s32 D_80105DC8;
extern s32 D_80105DCC;
/* The code of the tournament whose password was entered */
extern u8 g_TournamentCode[TOURNAMENT_CODE_SIZE];
extern s32 D_8012F480;
extern u8 D_801323A0[0x49];
extern s8 D_801323D0;
extern s32 D_801B7070;
extern s8 D_801B70F0;
extern s16 D_801B71CC;
extern s8 D_801B9410;
extern u8 g_MapCamera[];
extern void* D_801F4A20;
extern void* D_8012D3AC;
extern void* D_8012F4B0[7];
extern void* D_801049A0[7];
extern void* D_801052C4[2];
extern void* D_800FF420[2];
extern void* D_801B60D0;
extern void* D_8012D420[2];
extern void* D_801B5CD0[2];
extern void* D_801052BC[2];
extern void* D_800FBDFC[2];
extern void* D_800FF4C4[2];
extern void* D_801B93E0[5];
extern void* D_8010526C[2];
extern void* D_800FF180[2];
extern void* D_80106100;
extern void* D_800FC898;
extern void* D_8012D430[18];
extern void* D_80105B78[18];
extern void* D_800FBE30[2];
extern void* D_800FC6A0;
extern void* D_801B69EC;
extern u16 f_MenuLabelAssets[];
extern u16 f_ToadImageAssets[1];

void SetFramebufferCount(s32 count);

/* f_SlotCells lists slots in id order up to this count */
#define MARKABLE_CHARACTER_COUNT 10

extern s16 f_EveryCharacterGrid[CHARACTER_CHOICE_ROW_COUNT]
                               [CHARACTER_GRID_COLUMN_COUNT];
extern s16 f_RecordGrid[CHARACTER_CHOICE_ROW_COUNT]
                       [CHARACTER_GRID_COLUMN_COUNT];
extern s16 f_RecordCharacters[13];
extern s16 f_StartingCharacterGrid[CHARACTER_GRID_ROW_COUNT]
                                  [CHARACTER_GRID_COLUMN_COUNT];

void func_8005CA48(void);

typedef struct Unk800C29B8 {
  /* 0x0 */ s16 column;
  /* 0x2 */ s16 row;
} Unk800C29B8; /* size = 0x4 */

extern Unk800C29B8 f_SlotCells[];
extern s16 f_CharacterSlots[15];

void* func_8005AF74(void);
s32 GetClubTournamentCourse(void);
void func_8005D9A0(void);
s32 ReadSaveCopies(SaveData* save, u8 isInSram);
s32 WriteSaveCopies(SaveData* save, u8 isInSram);
void SyncSaveOptions(SaveData* save, s32 isWrite, s32 readResult);

/* Not static: the original left these to the linker's common block */
s8 D_80105DC1;
s8 D_80105DC2;
s32 D_8012F484;
s32 D_8012F488;
s32 D_8012F48C;
s8 D_801B70F1;
s8 D_801B70F2;
s8 D_801B70F3;
void* D_8012D424; /* D_8012D420[1], named apart to match */
void* D_80105BB0[4];
void* D_8012D468[4];

/*
 * Allocates and reads the menu's assets, then reads the save. Overlay 3
 * calls this as it loads.
 */
void LoadMenuResources(void) {
  EncodedAsset asset;
  AssetFile file;
  s32 unused[4]; /* The original frame has room for this */
  SaveData* save;
  s32 index;
  s32 size;

  save = GetSaveData();
  if (g_LoadedMenuScreen != -1) {
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
  OpenEncodedAsset(*f_MenuLabelAssets, &asset);
  ReadEncodedAsset(D_8012D420[0], &asset);
  OpenEncodedAsset(*f_MenuLabelAssets, &asset);
  ReadEncodedAsset(D_8012D424, &asset);
  if (g_MenuScreen == 5) {
    OpenEncodedAsset(0x528, &asset);
    ReadEncodedAsset(D_801B5CD0[0], &asset);
    OpenEncodedAsset(0x528, &asset);
    ReadEncodedAsset(D_801B5CD0[1], &asset);
  } else {
    OpenEncodedAsset(f_MenuLabelAssets[1], &asset);
    ReadEncodedAsset(D_801B5CD0[0], &asset);
    OpenEncodedAsset(f_MenuLabelAssets[1], &asset);
    ReadEncodedAsset(D_801B5CD0[1], &asset);
  }

  D_801052BC[0] =
      AllocMemory(OpenEncodedAsset(*g_MenuScreenImageAssets, &asset));
  ReadEncodedAsset(D_801052BC[0], &asset);
  D_801052BC[1] =
      AllocMemory(OpenEncodedAsset(*g_MenuScreenImageAssets, &asset));
  ReadEncodedAsset(D_801052BC[1], &asset);
  for (index = 0; index != ARRAY_COUNT(D_801049A0); index++) {
    if (g_MenuScreen == 5) {
      size = OpenEncodedAsset(
          g_PlayerCountIconAssets[g_ChosenPlayerCount - 1][index], &asset);
    } else {
      size = OpenEncodedAsset(g_CourseIconAssets[index], &asset);
    }

    D_801049A0[index] = AllocMemory(size);
    ReadEncodedAsset(D_801049A0[index], &asset);
    if (g_MenuScreen == 5) {
      size = OpenEncodedAsset(
          g_PlayerCountIconAssets[g_ChosenPlayerCount - 1][index], &asset);
    } else {
      size = OpenEncodedAsset(g_CourseIconAssets[index], &asset);
    }

    D_8012F4B0[index] = AllocMemory(size);
    ReadEncodedAsset(D_8012F4B0[index], &asset);
  }

  for (index = 0; index != ARRAY_COUNT(D_801052C4); index++) {
    OpenAssetFile(g_MiniGolfDiscFiles[index], &file);
    size = GetAssetFileSize(&file);
    D_801052C4[index] = AllocMemory(size);
    ReadAssetFile(D_801052C4[index], size, &file);
  }

  OpenEncodedAsset(g_CourseWheelImageAssets[0][0], &asset);
  D_800FBDFC[0] = AllocMemory(0xFA0);
  ReadEncodedAsset(D_800FBDFC[0], &asset);
  OpenEncodedAsset(g_CourseWheelImageAssets[0][0], &asset);
  D_800FBDFC[1] = AllocMemory(0xFA0);
  ReadEncodedAsset(D_800FBDFC[1], &asset);
  for (index = 0; index != ARRAY_COUNT(D_800FF4C4); index++) {
    OpenEncodedAsset(g_CourseWheelImageAssets[0][0], &asset);
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
    D_8012D430[index] =
        AllocMemory(OpenEncodedAsset(g_FaceAnimationAssets[index][0], &asset));
    ReadEncodedAsset(D_8012D430[index], &asset);
  }

  for (index = 0; index != ARRAY_COUNT(D_80105B78); index++) {
    OpenAssetFile(g_CharacterFacePaletteFiles[index][0], &file);
    size = GetAssetFileSize(&file);
    D_80105B78[index] = AllocMemory(size);
    ReadAssetFile(D_80105B78[index], size, &file);
  }

  for (index = 0; index != ARRAY_COUNT(D_800FBE38); index++) {
    OpenAssetFile(g_CourseTrophyFiles[0][index], &file);
    size = GetAssetFileSize(&file);
    D_800FBE38[index] = AllocMemory(size);
    ReadAssetFile(D_800FBE38[index], size, &file);
  }

  D_800FBE30[0] = AllocMemory(OpenEncodedAsset(*f_ToadImageAssets, &asset) * 2);
  ReadEncodedAsset(D_800FBE30[0], &asset);
  D_800FBE30[1] = AllocMemory(OpenEncodedAsset(*f_ToadImageAssets, &asset) * 2);
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

  g_LoadedMenuScreen = NO_MENU_SCREEN;
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

SaveData* GetSaveData(void) {
  func_8005AF74();
  return &g_SaveData;
}

void* func_8005AF74(void) {
  return D_801323A0;
}

void ResetSaveData(void) {
  SaveData* save;
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
  SaveData* save;

  save = GetSaveData();
  InitSram();
  TransferSaveData(save, FALSE);
}

s32 CountOpenCourses(s32 coursePoints) {
  s32 courseIndex;

  for (courseIndex = COURSE_COUNT - 1; courseIndex != 0; courseIndex--) {
    if (coursePoints >= f_CoursePointsNeeded[courseIndex]) {
      break;
    }
  }

  return courseIndex + 1;
}

s32 GetCoursePointsNeeded(s32 courseIndex) {
  return f_CoursePointsNeeded[courseIndex];
}

s32 GetRecordPriority(Unk68* record) {
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

/* Fills order with the indices of values from the largest value down, keeping
 * ties in order */
void SortIndicesDescending(s32* values, s32* order, s32 count) {
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

void func_8005B28C(s32 recordIndex, Unk68* record) {
  g_SaveData.unk1DF0[recordIndex] = *record;
  TransferSaveData(GetSaveData(), TRUE);
}

/* The save keeps the highest-priority records of each kind, this many each */
#define RANKED_GROUP_LENGTH 5

void InsertRankedRecord(Unk68* record) {
  s32 priorities[RANKED_RECORD_COUNT];
  s32 order[RANKED_RECORD_COUNT];
  Unk68 copies[RANKED_RECORD_COUNT];
  s32 newPriority;
  Unk68* records;
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
        priorities[index + 1] =
            GetRecordPriority(&records[firstIndex + index + 1]);
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

s32 SaveOrContinueGameByMode(s32 isContinuing) {
  UnkB8* players;
  Unk68* records;
  SavedGame* game;
  s32 playerCount;
  s32 hasIllegalState;
  s32 index;

  playerCount = 0;
  switch (g_GameState.gameMode) {
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
    game->hole = g_HoleSetup;
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
    g_HoleSetup = game->hole;
  }

  g_GameState.unk30 = 2;
  osSyncPrintf("Go Continue\n");
  return 0;
}

#define MAX_SAVE_COUNT 99

s32 SaveOrContinueGame(s32 slotIndex, s32 isContinuing) {
  UnkB8* players;
  SavedGame* game;
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

    game->hole = g_HoleSetup;
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
      g_HoleSetup = game->hole;
      g_RoundStats = game->roundStats;
    }

    g_GameState.unk30 = 2;
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
  SaveData* save;
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
          if (save->bestTimes[courseIndex][index] >
              save->bestTimes[courseIndex][scanIndex]) {
            save->bestTimes[courseIndex][index] =
                save->bestTimes[courseIndex][scanIndex];
            save->unkE08[courseIndex][index] =
                save->unkE08[courseIndex][scanIndex];
            save->unkD78[courseIndex][index] =
                save->unkD78[courseIndex][scanIndex];
            save->unkE50[courseIndex][index] =
                save->unkE50[courseIndex][scanIndex];
          }
        }
      }

      for (index = 0; index != count; index++) {
        for (scanIndex = index + 1; scanIndex != count; scanIndex++) {
          if (save->unkD78[courseIndex][index] !=
              save->unkD78[courseIndex][scanIndex]) {
            break;
          }
        }

        if (index + 1 < scanIndex) {
          for (runIndex = index; runIndex != scanIndex; runIndex++) {
            for (otherIndex = runIndex + 1; otherIndex != scanIndex;
                otherIndex++) {
              if (save->unkD78[courseIndex][runIndex] >
                  save->unkD78[courseIndex][otherIndex]) {
                save->bestTimes[courseIndex][runIndex] =
                    save->bestTimes[courseIndex][otherIndex];
                save->unkE08[courseIndex][runIndex] =
                    save->unkE08[courseIndex][otherIndex];
                save->unkD78[courseIndex][runIndex] =
                    save->unkD78[courseIndex][otherIndex];
                save->unkE50[courseIndex][runIndex] =
                    save->unkE50[courseIndex][otherIndex];
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

          if (time == save->bestTimes[courseIndex][index] &&
              arg2 <= save->unkD78[courseIndex][index]) {
            break;
          }
        }

        if (index < BEST_TIME_COUNT) {
          for (scanIndex = BEST_TIME_COUNT - 1; index < scanIndex;
              scanIndex--) {
            save->bestTimes[courseIndex][scanIndex] =
                save->bestTimes[courseIndex][scanIndex - 1];
            save->unkE08[courseIndex][scanIndex] =
                save->unkE08[courseIndex][scanIndex - 1];
            save->unkD78[courseIndex][scanIndex] =
                save->unkD78[courseIndex][scanIndex - 1];
            save->unkE50[courseIndex][scanIndex] =
                save->unkE50[courseIndex][scanIndex - 1];
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
  SaveData* save;
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
  SaveData* save;
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

/* The Ring Shot course that opens at exactly the stars earned so far, or 0 for
 * none */
s32 GetRingShotCourseOpenedAtStarCount(void) {
  SaveData* save;
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
  SaveData* save;
  s32 badgeCount;
  s32 courseIndex;
  s32 holeIndex;

  save = GetSaveData();
  badgeCount = 0;
  for (courseIndex = 0; courseIndex != COURSE_COUNT; courseIndex++) {
    for (holeIndex = 0; holeIndex != COURSE_HOLE_COUNT; holeIndex++) {
      badgeCount +=
          save->unk990.birdieBadgeCounts[courseIndex][holeIndex] != FALSE;
    }
  }

  return badgeCount >= MAPLE_BADGE_COUNT;
}

s32 IsMetalMarioUnlocked(void) {
  SaveData* save;
  s32 badgeCount;
  s32 courseIndex;
  s32 holeIndex;

  save = GetSaveData();
  badgeCount = 0;
  for (courseIndex = 0; courseIndex != COURSE_COUNT; courseIndex++) {
    for (holeIndex = 0; holeIndex != COURSE_HOLE_COUNT; holeIndex++) {
      badgeCount +=
          save->unk990.birdieBadgeCounts[courseIndex][holeIndex] != FALSE;
    }
  }

  return badgeCount == COURSE_COUNT * COURSE_HOLE_COUNT;
}

/* arg0 is reused below as a record count and a character id, to match */
void func_8005C674(s32 arg0, s32 arg1) {
  EncodedAsset asset;
  AssetFile file;
  SaveData* save;
  s32 row;
  s32 column;
  s32 index;
  s32 recordIndex; /* Also holds a grid entry at the end, to match */

  save = GetSaveData();
  for (row = 0; row != CHARACTER_GRID_ROW_COUNT; row++) {
    for (column = 0; column != CHARACTER_GRID_COLUMN_COUNT; column++) {
      g_CharacterGrid[row][column] = f_StartingCharacterGrid[row][column];
    }
  }

  if (IsGameFlagSet(GAME_FLAG_ALL_CHARACTERS)) {
    for (row = 0; row != CHARACTER_CHOICE_ROW_COUNT; row++) {
      for (column = 0; column != CHARACTER_GRID_COLUMN_COUNT; column++) {
        g_CharacterGrid[row][column] = f_EveryCharacterGrid[row][column];
      }
    }

    func_8005CA48();
    return;
  }

  if (arg0 >= 2) {
    for (row = 0; row != CHARACTER_CHOICE_ROW_COUNT; row++) {
      for (column = 0; column != 5; column++) {
        g_CharacterGrid[row][column] = f_EveryCharacterGrid[row][column];
      }
    }
  }

  for (index = 0; index != ARRAY_COUNT(f_RecordCharacters); index++) {
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
          if (f_RecordGrid[row][column] == index) {
            g_CharacterGrid[row][column] = f_RecordCharacters[index];
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
    row = f_SlotCells[index].row;
    column = f_SlotCells[index].column;
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
        OpenEncodedAsset(g_FaceAnimationAssets[arg0][0], &asset);
        ReadEncodedAsset(D_8012D468[index], &asset);
        OpenAssetFile(g_CharacterFacePaletteFiles[arg0][0], &file);
        ReadAssetFile(D_80105BB0[index], GetAssetFileSize(&file), &file);
      }
    }
  }
}

void func_8005CA48(void) {
  s16 index;
  s16 byteIndex;
  s16 count; /* Also the byte index of the first loops, to match */
  u8* name;
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
      f_ImportedCharacterNames[index][count] = 0;
    }
  }

  count = 0;
  for (index = 0; index != IMPORTED_CHARACTER_COUNT; index++) {
    if (g_GameBoyCharacters[index].status ==
        GAME_BOY_CHARACTER_STATUS_PRESENT) {
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
        name = f_DebugImportedNames[index];
      } else {
        name = g_GameBoyCharacters[index].name;
      }

      for (byteIndex = 0; byteIndex != IMPORTED_CHARACTER_NAME_LENGTH;
          byteIndex++) {
        letter = *name++;
        f_ImportedCharacterNames[index][byteIndex] = letter;
        g_ImportedCharacters[index].name[byteIndex] = letter;
      }

      g_CharacterGrid[f_ImportedCharacterCells[index].row]
                     [f_ImportedCharacterCells[index].column] =
                         FIRST_IMPORTED_CHARACTER + index;
      if (IsGameFlagSet(GAME_FLAG_ALL_CHARACTERS)) {
        g_ImportedCharacters[index].character =
            f_DebugImportedCharacters[index];
        g_ImportedCharacters[index].unk2 = f_DebugImportedDistances[index];
        g_ImportedCharacters[index].unk4 = f_DebugImportedSpins[index];
        g_ImportedCharacters[index].unk6 = f_DebugImportedSideSpins[index];
        g_Clubs[(FIRST_IMPORTED_CHARACTER + index) * CLUB_SET_SIZE].distance =
            f_DebugImportedDistances[index];
        g_Clubs[(FIRST_IMPORTED_CHARACTER + index) * CLUB_SET_SIZE].spin =
            f_DebugImportedSpins[index];
        g_Clubs[(FIRST_IMPORTED_CHARACTER + index) * CLUB_SET_SIZE].sideSpin =
            f_DebugImportedSideSpins[index];
      } else {
        g_ImportedCharacters[index].character =
            g_GameBoyCharacters[index].character + FIRST_IMPORTED_CHARACTER;
        g_ImportedCharacters[index].unk2 =
            g_Clubs[(FIRST_IMPORTED_CHARACTER + index) * CLUB_SET_SIZE]
                .distance;
        g_ImportedCharacters[index].unk4 =
            g_Clubs[(FIRST_IMPORTED_CHARACTER + index) * CLUB_SET_SIZE].spin;
        g_ImportedCharacters[index].unk6 =
            g_Clubs[(FIRST_IMPORTED_CHARACTER + index) * CLUB_SET_SIZE]
                .sideSpin;
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

  if (index >= ARRAY_COUNT(f_CharacterSlots)) {
    return -1;
  }

  func_8005C674(1, 1);
  slot = f_CharacterSlots[index];
  row = f_SlotCells[slot].row;
  column = f_SlotCells[slot].column;
  return g_CharacterGrid[row][column] < 0 ? -1 : 0;
}

/* Marks saved games whose settings or players are out of range as no longer
 * valid */
void InvalidateBrokenSavedGames(void) {
  SaveData* save;
  s32 isInvalid[SAVED_GAME_COUNT];
  s32 index;
  s32 entryIndex;
  SavedGame* unk;
  GameState* sub;
  UnkB8* player;

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

    if (sub->courseIndex < 0 ||
        sub->courseIndex >= DRIVING_RANGE_COURSE_INDEX) {
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
  SaveData* save;
  s32 isInvalid[SAVE_RECORD_COUNT];
  s32 recordIndex;
  Unk68* record;
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

  /* Toggling rather than clearing sets the flag on records that fail with it
   * clear */
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
  u8* unk;

  unk = func_8005AF74();
  unk[0x2E] = g_GameState.holeIndex;
  unk[0x2F] = g_GameState.courseIndex;
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

void CopyTournamentCode(u8* destination) {
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
    for (byteIndex = 0; byteIndex != IMPORTED_CHARACTER_NAME_LENGTH;
        byteIndex++) {
      g_ImportedCharacters[index].name[byteIndex] = 0;
    }

    g_ImportedCharacters[index].isChosen = 0;
  }
}

void ComputeRoundStatistics(void) {
  UnkE98* saved;
  UnkE98* entry;
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
      D_80105C38[courseIndex].bestScore = 0.0f;
      D_80105C38[courseIndex].averageScore = 0.0f;
      D_80105C38[courseIndex].bestDrive = 0.0f;
      D_80105C38[courseIndex].averageDrive = 0.0f;
      D_80105C38[courseIndex].unk14 = 0.0f;
      D_80105C38[courseIndex].fairwayPercent = 0.0f;
      D_80105C38[courseIndex].averagePutts = 0.0f;
      D_80105C38[courseIndex].parOnPercent = 0.0f;
      D_80105C38[courseIndex].parSavePercent = 0.0f;
      D_80105C38[courseIndex].hasPlace[THIRD_PLACE_INDEX] = FALSE;
      D_80105C38[courseIndex].hasPlace[SECOND_PLACE_INDEX] = FALSE;
      D_80105C38[courseIndex].hasPlace[FIRST_PLACE_INDEX] = FALSE;
    } else {
      /* unk14 is left as it was */
      reciprocal = 1.0f / entry->roundCount;
      D_80105C38[courseIndex].bestScore = entry->bestScore;
      D_80105C38[courseIndex].averageScore = entry->strokeTotal * reciprocal;
      D_80105C38[courseIndex].bestDrive = entry->longestDrive * UNK_SCALE;
      D_80105C38[courseIndex].averageDrive = entry->averageDrive * UNK_SCALE;
      D_80105C38[courseIndex].fairwayPercent =
          entry->measuredDriveTotal * reciprocal * (100.0f / HOLES_PER_ROUND);
      D_80105C38[courseIndex].averagePutts =
          entry->puttTotal * reciprocal * (1.0f / HOLES_PER_ROUND);
      D_80105C38[courseIndex].parSavePercent =
          entry->parOrBetterCount * reciprocal * (100.0f / HOLES_PER_ROUND);
      D_80105C38[courseIndex].parOnPercent =
          entry->parOnCount * reciprocal * (100.0f / HOLES_PER_ROUND);
      sum = entry->unk28 + entry->unk2C;
      if (sum <= 0) {
        D_80105C38[courseIndex].sandSavePercent = 0.0f;
      } else {
        D_80105C38[courseIndex].sandSavePercent =
            ((f32)entry->unk28 / (f32)sum) * 100.0f;
      }

      D_80105C38[courseIndex].hasPlace[THIRD_PLACE_INDEX] =
          entry->hasPlace[THIRD_PLACE_INDEX];
      D_80105C38[courseIndex].hasPlace[SECOND_PLACE_INDEX] =
          entry->hasPlace[SECOND_PLACE_INDEX];
      D_80105C38[courseIndex].hasPlace[FIRST_PLACE_INDEX] =
          entry->hasPlace[FIRST_PLACE_INDEX];
    }
  }

  courseIndex = 0; /* Zeroed this early to match */
  hasPlayedCourse = FALSE;
  D_80105C38[COURSE_COUNT].roundCount = 0.0f;
  D_80105C38[COURSE_COUNT].bestScore = 0.0f;
  D_80105C38[COURSE_COUNT].averageScore = 0.0f;
  D_80105C38[COURSE_COUNT].bestDrive = 0.0f;
  D_80105C38[COURSE_COUNT].averageDrive = 0.0f;
  D_80105C38[COURSE_COUNT].fairwayPercent = 0.0f;
  D_80105C38[COURSE_COUNT].averagePutts = 0.0f;
  D_80105C38[COURSE_COUNT].parSavePercent = 0.0f;
  D_80105C38[COURSE_COUNT].parOnPercent = 0.0f;
  D_80105C38[COURSE_COUNT].sandSavePercent = 0.0f;
  D_80105C38[COURSE_COUNT].hasPlace[THIRD_PLACE_INDEX] = FALSE;
  D_80105C38[COURSE_COUNT].hasPlace[SECOND_PLACE_INDEX] = FALSE;
  D_80105C38[COURSE_COUNT].hasPlace[FIRST_PLACE_INDEX] = FALSE;
  D_80105C38[COURSE_COUNT].bestScore = 0.0f;
  D_80105C38[COURSE_COUNT].bestDrive = 0.0f;
  D_80105C38[COURSE_COUNT].bestScore = D_80105C38[0].bestScore;
  D_80105C38[COURSE_COUNT].bestDrive = D_80105C38[0].bestDrive;
  zero = D_80105C38[COURSE_COUNT].roundCount; /* Read back after being zeroed */
  for (; courseIndex != COURSE_COUNT; courseIndex++) {
    if (zero < D_80105C38[courseIndex].roundCount) {
      if (!hasPlayedCourse) {
        D_80105C38[COURSE_COUNT].bestScore = D_80105C38[courseIndex].bestScore;
        hasPlayedCourse = TRUE;
        D_80105C38[COURSE_COUNT].bestDrive = D_80105C38[courseIndex].bestDrive;
      }

      if (D_80105C38[COURSE_COUNT].bestScore >
          D_80105C38[courseIndex].bestScore) {
        D_80105C38[COURSE_COUNT].bestScore = D_80105C38[courseIndex].bestScore;
      }

      if (D_80105C38[COURSE_COUNT].bestDrive <
          D_80105C38[courseIndex].bestDrive) {
        D_80105C38[COURSE_COUNT].bestDrive = D_80105C38[courseIndex].bestDrive;
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
      D_80105C38[COURSE_COUNT].sandSavePercent = 0.0f;
    } else {
      D_80105C38[COURSE_COUNT].sandSavePercent = (sum28 / sum28And2C) * 100.0f;
    }

    holeCount = D_80105C38[COURSE_COUNT].roundCount * HOLES_PER_ROUND;
    D_80105C38[COURSE_COUNT].averagePutts = sum1C / holeCount;
    D_80105C38[COURSE_COUNT].fairwayPercent = (sum18 / holeCount) * 100.0f;
    D_80105C38[COURSE_COUNT].parSavePercent = (sum20 / holeCount) * 100.0f;
    D_80105C38[COURSE_COUNT].parOnPercent = (sum24 / holeCount) * 100.0f;
    D_80105C38[COURSE_COUNT].averageScore =
        sum8 / D_80105C38[COURSE_COUNT].roundCount;
    if (sum10 > 0.0f && sum14 > 0) {
      D_80105C38[COURSE_COUNT].averageDrive = sum10 / sum14;
    } else {
      D_80105C38[COURSE_COUNT].averageDrive = 0.0f;
    }
  }
}

void func_8005D9A0(void) {
  s8 index;

  g_MenuCourseCount = 6;
  D_800FE334 = 0;
  g_MenuScreen = 0;
  g_ChosenPlayerCount = 0;
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
  switch (g_GameState.gameMode) {
    case 5:
      D_800FF4C0 = 2;
      g_MapCamera[0x4C] = 0;
      D_801B5634 = 0;
      break;
    case 0:
      D_800FF4C0 = 1;
      g_MapCamera[0x4C] = 1;
      if (g_ChosenPlayerCount == 4) {
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
  g_ImportedCharacters[0].isChosen = 0;
  g_ImportedCharacters[1].isChosen = 0;
  g_ImportedCharacters[2].isChosen = 0;
  g_ImportedCharacters[3].isChosen = 0;
  g_PlayerControllerIndices[0] = UNCHOSEN_CONTROLLER;
  g_PlayerControllerIndices[1] = UNCHOSEN_CONTROLLER;
  g_PlayerControllerIndices[2] = UNCHOSEN_CONTROLLER;
  g_PlayerControllerIndices[3] = UNCHOSEN_CONTROLLER;
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
  return GetSaveData()->options[SAVE_OPTION_MUSIC_OFF];
}

s8 func_8005DE20(void) {
  return GetSaveData()->options[SAVE_OPTION_MONO];
}

s8 func_8005DE40(void) {
  return GetSaveData()->options[SAVE_OPTION_SIMPLE_CAMERA];
}

void WriteSaveData(void) {
  TransferSaveData(GetSaveData(), TRUE);
}

/* Copies the options between the save data and g_OptionBits, in the direction
 * of the transfer */
void SyncSaveOptions(SaveData* save, s32 isWrite, s32 readResult) {
  u8* bits;
  s8* options;

  options = save->options;
  if (!isWrite) {
    bits = &g_OptionBits;
    *bits = 0;
    if (readResult > 0) {
      if (options[SAVE_OPTION_MUSIC_OFF]) {
        *bits = OPTION_BIT_MUSIC_OFF;
      }

      if (options[SAVE_OPTION_MONO]) {
        *bits |= OPTION_BIT_MONO;
        g_SoundOptionBits = *bits;
      }

      if (options[SAVE_OPTION_SIMPLE_CAMERA]) {
        *bits |= OPTION_BIT_SIMPLE_CAMERA;
      }
    }
  } else {
    if (g_OptionBits & OPTION_BIT_MUSIC_OFF) {
      options[SAVE_OPTION_MUSIC_OFF] = TRUE;
    } else {
      options[SAVE_OPTION_MUSIC_OFF] = FALSE;
    }

    if (g_OptionBits & OPTION_BIT_MONO) {
      options[SAVE_OPTION_MONO] = TRUE;
    } else {
      options[SAVE_OPTION_MONO] = FALSE;
    }

    if (g_OptionBits & OPTION_BIT_SIMPLE_CAMERA) {
      options[SAVE_OPTION_SIMPLE_CAMERA] = TRUE;
    } else {
      options[SAVE_OPTION_SIMPLE_CAMERA] = FALSE;
    }
  }
}

void TransferSaveData(SaveData* save, u8 isWrite) {
  u8 isSramSelected;
  u8 isInSram;
  s32 readResult;

  isSramSelected = !IsGameFlagSet(GAME_FLAG_SAVE_TO_HOST);
  isInSram = isSramSelected;
  if (!isWrite) {
    readResult = ReadSaveCopies(save, isInSram);
    SyncSaveOptions(save, FALSE, readResult);
  } else {
    /* readResult is never set on this path, but SyncSaveOptions ignores it when
     * writing */
    SyncSaveOptions(save, TRUE, readResult);
    WriteSaveCopies(save, isInSram);
  }
}

/* Reads the first intact copy; if neither is, the save data starts over */
s32 ReadSaveCopies(SaveData* save, u8 isInSram) {
  u8 unusedBuffer[0x90]; /* Only takes up stack */
  u8 checkBuffer[0x40];
  SaveCheck* check;
  u32 size;
  s32 unmarkedCount;
  s32 copyIndex;
  u16 crc;

  check = (SaveCheck*)ALIGN_FOR_DMA(checkBuffer);
  unmarkedCount = 0;
  for (copyIndex = 0; copyIndex < SAVE_COPY_COUNT; copyIndex++) {
    if (!isInSram) {
      if (ReadHostFile(f_SaveCopyLocations[copyIndex].dataFileName, &size) !=
              HOST_FILE_OK ||
          size != sizeof(SaveData)) {
        continue;
      }

      if (ReadHostFile(f_SaveCopyLocations[copyIndex].checkFileName, &size) !=
              HOST_FILE_OK ||
          size != sizeof(SaveCheck)) {
        continue;
      }
    } else {
      nuPiReadSram(f_SaveCopyLocations[copyIndex].dataSramAddress, save,
          sizeof(SaveData));
      nuPiReadSram(f_SaveCopyLocations[copyIndex].checkSramAddress, check,
          sizeof(SaveCheck));
    }

    if (save->magic != SAVE_MAGIC) {
      unmarkedCount++;
      continue;
    }

    crc = check->keyedCrc ^ check->key;
    if (crc == ComputeCrc16((u8*)save, sizeof(SaveData))) {
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
  return (unmarkedCount == SAVE_COPY_COUNT) ? SAVE_READ_NEVER_WRITTEN
                                            : SAVE_READ_FAILED;
}

s32 WriteSaveCopies(SaveData* save, u8 isInSram) {
  u8 checkBuffer[0xC0];
  SaveCheck* check;
  u16 crc;
  s32 dateIndex;
  s32 copyIndex;

  check = (SaveCheck*)ALIGN_FOR_DMA(checkBuffer);
  for (dateIndex = 0; dateIndex < ARRAY_COUNT(save->invertedBuildDate);
      dateIndex++) {
    save->invertedBuildDate[dateIndex] = ~g_RomBuildDate[dateIndex];
  }

  crc = ComputeCrc16((u8*)save, sizeof(SaveData));
  for (copyIndex = 0; copyIndex < SAVE_COPY_COUNT; copyIndex++) {
    check->key = rand();
    check->keyedCrc = check->key ^ crc;
    if (!isInSram) {
      if (WriteHostFile(f_SaveCopyLocations[copyIndex].dataFileName,
              sizeof(SaveData)) == HOST_FILE_OK) {
        WriteHostFile(
            f_SaveCopyLocations[copyIndex].checkFileName, sizeof(SaveCheck));
      }
    } else {
      nuPiWriteSram(f_SaveCopyLocations[copyIndex].dataSramAddress, save,
          sizeof(SaveData));
      nuPiWriteSram(f_SaveCopyLocations[copyIndex].checkSramAddress, check,
          sizeof(SaveCheck));
    }
  }

  return TRUE;
}
