/*
 * Tests of the save data's two copies in SRAM, through menu.c's own routines
 * and a fake SRAM: where each copy and its check go, what a read takes when
 * one copy or both are damaged or were never written, how a write keys its
 * checks, and how the options pass between the save and the menus. The rest
 * of menu.c is linked but never run, so its calls out end in stubs.
 */

#include <string.h>
#include <ultra64.h>

#include "audio/sound.h"
#include "camera/camera.h"
#include "characters/character.h"
#include "controllers/transfer_pak.h"
#include "course/hole_setup.h"
#include "debug/game_flags.h"
#include "framebuffer_count.h"
#include "math/math_util.h"
#include "menus/menu_memory.h"
#include "menus/password.h"
#include "round/round_stats.h"
#include "save/save.h"
#include "save/sram.h"
#include "screen/screen.h"
#include "shot/club.h"
#include "system/asset.h"
#include "system/crash_screen.h"
#include "system/heap.h"
#include "system/host_file.h"
#include "system/main.h"
#include "test_support.h"

// menu.c's own, which it declares for itself rather than in a header
SaveReadResult ReadSaveCopies(SaveData *save, UBoolean8 isInSram);

// As menu.c has them: each copy's slot, and the flag that sends the save to
// the development host's files instead
#define COPY_SLOT_SIZE 0x4000
#define GAME_FLAG_SAVE_TO_HOST 0x53

// The fake SRAM: twice a copy's slot, the most that the save reaches
#define SRAM_SIZE (2 * COPY_SLOT_SIZE)
#define CHECK_SIZE ((u32)sizeof(SaveCheck))
#define DATA_SIZE ((u32)sizeof(SaveData))
#define WRITE_LOG_MAX 8

typedef struct SramWrite {
    u32 offset;
    u32 size;
} SramWrite;

static u8 f_Sram[SRAM_SIZE];
static SramWrite f_Writes[WRITE_LOG_MAX];
static int f_WriteCount;
static int f_OutOfRangeCount;
static Boolean f_IsSavingToHost;
static int f_HostWriteCount;
static int f_StubCallCount;

// --- The fakes that the save's routines reach ---

void nuPiReadWriteSram(u32 address, void *buffer, u32 size, s32 direction) {
    u32 offset = address - PI_DOM2_ADDR2;

    if (offset > SRAM_SIZE || size > SRAM_SIZE - offset) {
        f_OutOfRangeCount++;
        return;
    }

    if (direction == OS_READ) {
        memcpy(buffer, &f_Sram[offset], size);
        return;
    }

    memcpy(&f_Sram[offset], buffer, size);
    if (f_WriteCount < WRITE_LOG_MAX) {
        f_Writes[f_WriteCount].offset = offset;
        f_Writes[f_WriteCount].size = size;
    }

    f_WriteCount++;
}

Boolean IsGameFlagSet(u32 flag) {
    return flag == GAME_FLAG_SAVE_TO_HOST && f_IsSavingToHost;
}

// The released game's host files: reads always fail, as on the N64
HostFileResult ReadHostFile(const char *fileName, u32 *size) {
    (void)fileName;
    (void)size;
    return HOST_FILE_NAME_TOO_SHORT;
}

HostFileResult WriteHostFile(const char *fileName, u32 size) {
    (void)fileName;
    (void)size;
    f_HostWriteCount++;
    return HOST_FILE_OK;
}

void InitSram(void) {
}

void osSyncPrintf(const char *format, ...) {
    (void)format;
}

char g_RomBuildDate[BUILD_DATE_LENGTH_MAX + 1] = "1999-05-17 12:34:56";

// --- Stubs for the rest of menu.c, which no test runs ---

Club g_Clubs[CLUB_COUNT];
TournamentRules g_ClubTournamentRules;
const char g_EmptyString[] = "";
GameBoyCharacter g_GameBoyCharacters[GAME_BOY_CHARACTER_COUNT];
HoleSetup g_HoleSetup;
ImportedCharacter g_ImportedCharacters[IMPORTED_CHARACTER_COUNT];
Camera g_MapCamera;
RoundStats g_RoundStats;

void *AllocMemory(u32 size) {
    (void)size;
    f_StubCallCount++;
    return NULL;
}

void FreeMemory(void *pointerToBlock) {
    (void)pointerToBlock;
    f_StubCallCount++;
}

void SetGameFlag(u32 flag) {
    (void)flag;
    f_StubCallCount++;
}

void ClearGameFlag(u32 flag) {
    (void)flag;
    f_StubCallCount++;
}

s32 GetAssetFileSize(AssetFile *file) {
    (void)file;
    f_StubCallCount++;
    return 0;
}

void OpenAssetFile(WideAssetId assetId, AssetFile *file) {
    (void)assetId;
    (void)file;
    f_StubCallCount++;
}

void ReadAssetFile(void *buffer, u32 size, AssetFile *file) {
    (void)buffer;
    (void)size;
    (void)file;
    f_StubCallCount++;
}

s32 OpenEncodedAsset(AssetId assetId, EncodedAsset *asset) {
    (void)assetId;
    (void)asset;
    f_StubCallCount++;
    return 0;
}

void ReadEncodedAsset(void *buffer, EncodedAsset *asset) {
    (void)buffer;
    (void)asset;
    f_StubCallCount++;
}

void PlaySong(SongId songId, SongFlags songFlags) {
    (void)songId;
    (void)songFlags;
    f_StubCallCount++;
}

void SetFramebufferCount(u32 count) {
    (void)count;
    f_StubCallCount++;
}

void SetScreenEffect(ScreenEffect effect, s32 frameCount) {
    (void)effect;
    (void)frameCount;
    f_StubCallCount++;
}

// --- Helpers ---

static u32 DataOffset(int copyIndex) {
    return (u32)copyIndex * COPY_SLOT_SIZE;
}

static u32 CheckOffset(int copyIndex) {
    return DataOffset(copyIndex) + DATA_SIZE;
}

static void EraseSram(void) {
    memset(f_Sram, 0, sizeof(f_Sram));
    memset(f_Writes, 0, sizeof(f_Writes));
    f_WriteCount = 0;
    f_OutOfRangeCount = 0;
    f_IsSavingToHost = FALSE;
    f_HostWriteCount = 0;
    g_MenuMemory.optionBits = 0;
}

static SaveCheck ReadCheck(int copyIndex) {
    SaveCheck check;

    memcpy(&check, &f_Sram[CheckOffset(copyIndex)], sizeof(check));
    return check;
}

static void WriteCheck(int copyIndex, SaveCheck check) {
    memcpy(&f_Sram[CheckOffset(copyIndex)], &check, sizeof(check));
}

// Fills the save with values of the test's own, and writes both copies
static void WriteKnownSave(s32 coursePoints) {
    SaveData *save = GetSaveData();

    memset(save, 0, sizeof(*save));
    save->magic = SAVE_MAGIC;
    save->coursePoints = coursePoints;
    TransferSaveData(save, TRUE);
}

static Boolean IsReset(const SaveData *save) {
    int course;
    int index;

    if (save->magic != SAVE_MAGIC || save->coursePoints != 0) {
        return FALSE;
    }

    for (course = 0; course < COURSE_COUNT; course++) {
        for (index = 0; index < BEST_TIME_COUNT; index++) {
            if (save->records.bestTimes[course][index] != NO_BEST_TIME) {
                return FALSE;
            }
        }
    }

    return TRUE;
}

// Damages one byte of a copy's data, in the middle of its course points
static void DamageData(int copyIndex) {
    f_Sram[DataOffset(copyIndex) + offsetof(SaveData, coursePoints) + 1] ^= 0x10;
}

static void ReadIntoSave(SaveReadResult *result) {
    SaveData *save = GetSaveData();

    memset(save, 0xAB, sizeof(*save));
    *result = ReadSaveCopies(save, TRUE);
}

// --- Where the copies go ---

static void TestFitsEachCopyAndItsCheckInItsSlot(void) {
    EXPECT(DATA_SIZE + CHECK_SIZE <= COPY_SLOT_SIZE);
}

static void TestWritesEachCopyThenItsCheck(void) {
    EraseSram();
    WriteKnownSave(1234);
    EXPECT(f_WriteCount == 4);
    EXPECT(f_Writes[0].offset == DataOffset(0) && f_Writes[0].size == DATA_SIZE);
    EXPECT(f_Writes[1].offset == CheckOffset(0) && f_Writes[1].size == CHECK_SIZE);
    EXPECT(f_Writes[2].offset == DataOffset(1) && f_Writes[2].size == DATA_SIZE);
    EXPECT(f_Writes[3].offset == CheckOffset(1) && f_Writes[3].size == CHECK_SIZE);
    EXPECT(f_OutOfRangeCount == 0);
}

static void TestWritesTheSameDataToBothCopies(void) {
    EraseSram();
    WriteKnownSave(1234);
    EXPECT(memcmp(&f_Sram[DataOffset(0)], &f_Sram[DataOffset(1)], DATA_SIZE) == 0);
}

static void TestSendsTheSaveToTheHostWhenItsFlagIsSet(void) {
    EraseSram();
    f_IsSavingToHost = TRUE;
    WriteKnownSave(1234);
    EXPECT(f_WriteCount == 0);
    EXPECT(f_HostWriteCount == 2 * SAVE_COPY_COUNT);
}

// --- What a write stamps ---

static void TestStampsTheBuildDateInverted(void) {
    SaveData *save = GetSaveData();
    int index;
    int wrongCount = 0;

    EraseSram();
    WriteKnownSave(1234);
    for (index = 0; index < (int)sizeof(save->invertedBuildDate); index++) {
        if (save->invertedBuildDate[index] != (u8) ~(u8)g_RomBuildDate[index]) {
            wrongCount++;
        }
    }

    EXPECT(wrongCount == 0);
}

static void TestKeysEachCheckWithTheDatasCrc(void) {
    u16 crc;
    SaveCheck first;
    SaveCheck second;

    EraseSram();
    WriteKnownSave(1234);
    crc = ComputeCrc16(&f_Sram[DataOffset(0)], DATA_SIZE);
    first = ReadCheck(0);
    second = ReadCheck(1);
    EXPECT((u16)(first.keyedCrc ^ first.key) == crc);
    EXPECT((u16)(second.keyedCrc ^ second.key) == crc);
}

static void TestDrawsANewKeyForEachCopy(void) {
    EraseSram();
    WriteKnownSave(1234);
    EXPECT(ReadCheck(0).key != ReadCheck(1).key);
}

// --- What a read takes ---

static void TestReadsBackTheFirstCopy(void) {
    SaveReadResult result;

    EraseSram();
    WriteKnownSave(1234);
    ReadIntoSave(&result);
    EXPECT(result == SAVE_READ_FIRST_COPY);
    EXPECT(GetSaveData()->coursePoints == 1234);
    EXPECT(memcmp(GetSaveData(), &f_Sram[DataOffset(0)], DATA_SIZE) == 0);
}

static void TestReadsWithoutWriting(void) {
    SaveReadResult result;

    EraseSram();
    WriteKnownSave(1234);
    f_WriteCount = 0;
    ReadIntoSave(&result);
    EXPECT(f_WriteCount == 0);
}

static void TestTakesTheSecondCopyWhenTheFirstsDataIsDamaged(void) {
    SaveReadResult result;

    EraseSram();
    WriteKnownSave(1234);
    DamageData(0);
    ReadIntoSave(&result);
    EXPECT(result == SAVE_READ_SECOND_COPY);
    EXPECT(GetSaveData()->coursePoints == 1234);
}

static void TestTakesTheSecondCopyWhenTheFirstsCheckIsDamaged(void) {
    SaveReadResult result;
    SaveCheck check;

    EraseSram();
    WriteKnownSave(1234);
    check = ReadCheck(0);
    check.keyedCrc ^= 1;
    WriteCheck(0, check);
    ReadIntoSave(&result);
    EXPECT(result == SAVE_READ_SECOND_COPY);
}

// The key only hides the CRC, so a check whose key and keyed CRC both change
// by the same bits still holds
static void TestAcceptsACheckRekeyedWithTheSameCrc(void) {
    SaveReadResult result;
    SaveCheck check;

    EraseSram();
    WriteKnownSave(1234);
    check = ReadCheck(0);
    check.key ^= 0x5A5A;
    check.keyedCrc ^= 0x5A5A;
    WriteCheck(0, check);
    ReadIntoSave(&result);
    EXPECT(result == SAVE_READ_FIRST_COPY);
}

static void TestTakesTheSecondCopyWhenTheFirstIsUnmarked(void) {
    SaveReadResult result;

    EraseSram();
    WriteKnownSave(1234);
    memset(&f_Sram[DataOffset(0)], 0, DATA_SIZE);
    ReadIntoSave(&result);
    EXPECT(result == SAVE_READ_SECOND_COPY);
    EXPECT(GetSaveData()->coursePoints == 1234);
}

// A magic number one off is as good as none, even where the CRC holds
static void TestCountsAMagicNumberOneOffAsUnmarked(void) {
    SaveReadResult result;
    SaveData save;
    int copyIndex;

    EraseSram();
    WriteKnownSave(1234);
    for (copyIndex = 0; copyIndex < SAVE_COPY_COUNT; copyIndex++) {
        memcpy(&save, &f_Sram[DataOffset(copyIndex)], sizeof(save));
        save.magic = SAVE_MAGIC + 1;
        memcpy(&f_Sram[DataOffset(copyIndex)], &save, sizeof(save));
    }

    ReadIntoSave(&result);
    EXPECT(result == SAVE_READ_NEVER_WRITTEN);
}

// --- When neither copy will do ---

static void TestStartsOverWhenNeverWritten(void) {
    SaveReadResult result;

    EraseSram();
    ReadIntoSave(&result);
    EXPECT(result == SAVE_READ_NEVER_WRITTEN);
    EXPECT(IsReset(GetSaveData()));
}

static void TestStartsOverWhenBothCopiesAreDamaged(void) {
    SaveReadResult result;

    EraseSram();
    WriteKnownSave(1234);
    DamageData(0);
    DamageData(1);
    ReadIntoSave(&result);
    EXPECT(result == SAVE_READ_FAILED);
    EXPECT(IsReset(GetSaveData()));
}

// One copy marked is enough for the read to have failed, rather than never
// to have been written
static void TestFailsWhenOneCopyIsUnmarkedAndTheOtherDamaged(void) {
    SaveReadResult result;

    EraseSram();
    WriteKnownSave(1234);
    memset(&f_Sram[DataOffset(0)], 0, DATA_SIZE);
    DamageData(1);
    ReadIntoSave(&result);
    EXPECT(result == SAVE_READ_FAILED);
}

// Starting over writes the fresh save at once, over both copies
static void TestWritesTheFreshSaveOverBothCopies(void) {
    SaveReadResult result;

    EraseSram();
    WriteKnownSave(1234);
    DamageData(0);
    DamageData(1);
    f_WriteCount = 0;
    ReadIntoSave(&result);
    EXPECT(f_WriteCount == 4);
    ReadIntoSave(&result);
    EXPECT(result == SAVE_READ_FIRST_COPY);
    EXPECT(IsReset(GetSaveData()));
}

// The development host's files never read in the released game, and no
// copy counts as unmarked, so the read fails rather than finding none
static void TestFailsToReadFromTheHost(void) {
    SaveData *save = GetSaveData();

    EraseSram();
    memset(save, 0xAB, sizeof(*save));
    EXPECT(ReadSaveCopies(save, FALSE) == SAVE_READ_FAILED);
    EXPECT(IsReset(save));
}

// --- The options ---

static void TestReadsTheOptionsIntoTheMenus(void) {
    SaveData *save = GetSaveData();

    EraseSram();
    g_MenuMemory.optionBits = OPTION_BIT_MUSIC_OFF | OPTION_BIT_SIMPLE_CAMERA;
    TransferSaveData(save, TRUE);
    g_MenuMemory.optionBits = OPTION_BIT_MONO;
    TransferSaveData(save, FALSE);
    EXPECT(g_MenuMemory.optionBits == (OPTION_BIT_MUSIC_OFF | OPTION_BIT_SIMPLE_CAMERA));
}

static void TestWritesEachOptionAsABoolean(void) {
    SaveData *save = GetSaveData();

    EraseSram();
    save->options[SAVE_OPTION_MONO] = TRUE;
    g_MenuMemory.optionBits = OPTION_BIT_MUSIC_OFF;
    TransferSaveData(save, TRUE);
    EXPECT(save->options[SAVE_OPTION_MUSIC_OFF] == TRUE);
    EXPECT(save->options[SAVE_OPTION_MONO] == FALSE);
    EXPECT(save->options[SAVE_OPTION_SIMPLE_CAMERA] == FALSE);
}

// A save that starts over clears the menus' options, whatever they were,
// whether it was never written or both copies were damaged
static void TestClearsTheOptionsWhenTheReadStartsOver(void) {
    EraseSram();
    g_MenuMemory.optionBits = OPTION_BIT_MUSIC_OFF | OPTION_BIT_MONO;
    TransferSaveData(GetSaveData(), FALSE);
    EXPECT(g_MenuMemory.optionBits == 0);

    EraseSram();
    WriteKnownSave(1234);
    DamageData(0);
    DamageData(1);
    g_MenuMemory.optionBits = OPTION_BIT_MUSIC_OFF | OPTION_BIT_MONO;
    TransferSaveData(GetSaveData(), FALSE);
    EXPECT(g_MenuMemory.optionBits == 0);
}

// Starting over writes the fresh save with the menus' options as they were
// before the read cleared them, so the next read brings them back
static void TestKeepsTheOldOptionsInTheFreshSave(void) {
    EraseSram();
    g_MenuMemory.optionBits = OPTION_BIT_MUSIC_OFF;
    TransferSaveData(GetSaveData(), FALSE);
    TransferSaveData(GetSaveData(), FALSE);
    EXPECT(g_MenuMemory.optionBits == OPTION_BIT_MUSIC_OFF);
}

int main(void) {
    TestFitsEachCopyAndItsCheckInItsSlot();
    TestWritesEachCopyThenItsCheck();
    TestWritesTheSameDataToBothCopies();
    TestSendsTheSaveToTheHostWhenItsFlagIsSet();
    TestStampsTheBuildDateInverted();
    TestKeysEachCheckWithTheDatasCrc();
    TestDrawsANewKeyForEachCopy();
    TestReadsBackTheFirstCopy();
    TestReadsWithoutWriting();
    TestTakesTheSecondCopyWhenTheFirstsDataIsDamaged();
    TestTakesTheSecondCopyWhenTheFirstsCheckIsDamaged();
    TestAcceptsACheckRekeyedWithTheSameCrc();
    TestTakesTheSecondCopyWhenTheFirstIsUnmarked();
    TestCountsAMagicNumberOneOffAsUnmarked();
    TestStartsOverWhenNeverWritten();
    TestStartsOverWhenBothCopiesAreDamaged();
    TestFailsWhenOneCopyIsUnmarkedAndTheOtherDamaged();
    TestWritesTheFreshSaveOverBothCopies();
    TestFailsToReadFromTheHost();
    TestReadsTheOptionsIntoTheMenus();
    TestWritesEachOptionAsABoolean();
    TestClearsTheOptionsWhenTheReadStartsOver();
    TestKeepsTheOldOptionsInTheFreshSave();
    EXPECT(f_StubCallCount == 0);
    return ReportTestResults("save_copies_test");
}
