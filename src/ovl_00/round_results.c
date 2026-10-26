#include "common.h"
#include "asset.h"
#include "billboard.h"
#include "character.h"
#include "course.h"
#include "effects.h"
#include "functions.h"
#include "game_flags.h"
#include "heap.h"
#include "round_hud.h"
#include "save.h"
#include "scene.h"
#include "scoring.h"
#include "screen.h"
#include "screen_layers.h"
#include "sound.h"
#include "text.h"
#include "tournament.h"
#include "variables.h"
/*
 * The round overlay. It begins with the results that slide in when a
 * round ends: the players' portraits and scores, the course points the
 * round earned and the notices of what those points unlocked.
 */

/* OpenRoundResults's request that frees the results */
#define CLOSE_ROUND_RESULTS -1

/* sResultsState values */
#define RESULTS_SLIDING_IN 0
#define RESULTS_SHOWN 1
#define RESULTS_CLOSING 2 /* Saves and frees them */

/* LoadUnlockNotice's unlocks */
#define UNLOCK_NOTICE_DONKEY_KONG 0
#define UNLOCK_NOTICE_PEACH 1
#define UNLOCK_NOTICE_METAL_MARIO 2
#define UNLOCK_NOTICE_COURSES 3 /* The first of the courses' unlocks */

/* sUnlockNoticeKind values */
#define UNLOCK_NOTICE_KIND_NONE 0
#define UNLOCK_NOTICE_KIND_COURSE 1
#define UNLOCK_NOTICE_KIND_CHARACTER 2

#define RESULTS_DIGIT_WIDTH 8
#define RESULTS_DIGIT_HEIGHT 12

#define SOUND_TALLY_TICK 0x20

/*
 * The course notice drops in on a half sine swing, NOTICE_SWING_STEP an
 * update, and is done NOTICE_DONE_FRAME frames after it starts
 */
#define NOTICE_SWING_HEIGHT 160.0f
#define NOTICE_SWING_STEP (M_PI / 9)
#define NOTICE_SWING_FRAMES 46
#define NOTICE_DONE_FRAME 51

/*
 * The character notice's portrait changes face every few frames after
 * a pause, then lifts away, faster and faster, until it is off screen
 */
#define NOTICE_FACE_DELAY 10
#define NOTICE_FACE_INTERVAL_MASK 3 /* Every fourth frame */
#define NOTICE_LAST_FACE 3
#define NOTICE_LIFT_DELAY 50
#define NOTICE_LIFT_START_SPEED 8
#define NOTICE_LIFT_EXIT -200

/* The places image's 32x32 icons, after its 256-color palette */
#define PLACE_ICON_SIZE 32

/* The labels image's 16-color strips. Each is stored wider than drawn. */
#define RESULTS_LABEL_WIDTH 72
#define RESULTS_LABEL_STORED_WIDTH 80
#define RESULTS_LABEL_HEIGHT 18

#define PORTRAIT_SIZE 32

#define RESULTS_SONG 3
#define TOURNAMENT_RESULTS_SONG 0x25

/* Course points are clamped there rather than overflow */
#define MAX_COURSE_POINTS 0x7FFFFFFF

/* The places of sTournamentPrizes's columns after the trophy winners' */
#define TROPHY_PLACE_COUNT 3
#define PRIZE_TOP_TEN 10
#define PRIZE_TOP_TWENTY 20

/* The results' images. g_IsUnk3B swaps in another set. */
#define RESULTS_BACKDROP_ASSET 0x6A0
#define RESULTS_UNK3B_BACKDROP_ASSET 0x947
#define RESULTS_UNK3B_UNK949_ASSET 0x949
#define PRESSING_HAND_ASSET 0x8EB
#define RESULTS_DIGITS_ASSET 0x722
#define RESULTS_UNK98E_ASSET 0x98E
#define RESULTS_UNK69F_ASSET 0x69F
#define LABELS_IMAGE_ASSET 0x68A
#define LARGE_DIGITS_IMAGE_ASSET 0x689
#define NOTICE_CLOUD_ASSET 0x70B
#define NOTICE_TITLE_ASSET 0x55F
#define SHINE_ASSET 0x6A4
#define RESULTS_UNK72C_IMAGE_ASSET 0x72C
#define RESULTS_UNK72D_PALETTE_ASSET 0x72D
#define PLACES_IMAGE_ASSET 0x688
#define MATCH_PLACES_IMAGE_ASSET 0x6A2
#define MATCH_UNK6A3_ASSET 0x6A3
#define NAMEPLATE_ASSET 0x832
#define MATCH_UNK8FE_ASSET 0x8FE
#define TOURNAMENT_UNK6A1_ASSET 0x6A1

extern s32 sAreResultsLoaded;
extern s32 sResultsState;
extern s32 sResultsSlide;
extern s32 sTallySpeed;
extern s32 sTallySoundCount;
extern f32 sCoursePointRates[];
extern s32 sOpenCourseCount;
extern s32 sPointsToNextCourse;
extern s32 sCourseNoticeAssets[];
extern const char sNumberFormat[];

extern s32 sUnlockNoticeKind;
extern s32 sNoticeStage;
extern s32 sNoticeFrameCount;
extern s32 sNoticeSwing;
extern s32 sNoticeUnk54;
extern f32 sNoticeAngle;
extern s32 sIsNoticeHeld;
extern s32 sNoticeTime;
extern s32 sNoticeFaceIndex;
extern s32 sNoticeLift;
extern s32 sNoticeUnk6C;

extern u8 *sResultsCourseImage;
extern u8 *sPortraitImage;
extern u8 *sPortraitPalette;
extern u8 *sPlayerPortraitImages[MAX_PLAYER_COUNT];
extern u8 *sPlayerPortraitPalettes[MAX_PLAYER_COUNT];
extern u8 *sNameplateImage;
extern u8 *sNoticeTitleImage;
extern u8 *sResultsShineImage;
extern u8 *sDigitsImage;
extern u8 *sResultsBackdropImage;
extern u8 *sResultsHandImage;
extern u8 *sResultsLabelsImage;
extern u8 *sResultsLargeDigitsImage;
extern u8 *sResultsCloudImage;
extern u8 *sResultsPlacesImage;
extern u8 *D_802256C4_ovl_00;
extern u8 *D_80225708_ovl_00;
extern u8 *D_80225718_ovl_00;
extern u8 *D_8022571C_ovl_00;
extern u8 *sTrophyImage;
extern u8 *sTrophyPalette;
extern u8 *D_8022572C_ovl_00;
extern u8 *D_80225730_ovl_00;
extern u8 *D_80225738_ovl_00;
extern s32 sPortraitCharacterIndex;
extern s32 sPlayerResults[MAX_PLAYER_COUNT];
extern s32 sPlayerPoints[MAX_PLAYER_COUNT];
extern s32 sPlayerBetTotals[MAX_PLAYER_COUNT];
extern s32 sPlayerRanks[MAX_PLAYER_COUNT];
extern s32 sPointsEarned;
extern s32 sPointsShown;
extern s32 sPortraitAnimations[MAX_PLAYER_COUNT];
extern s32 sPortraitFrames[MAX_PLAYER_COUNT];
extern s32 sPortraitTimers[MAX_PLAYER_COUNT];
extern s32 sHasBets;
/*
 * Each course's tournament prizes, in course points: first to third
 * place, then up to PRIZE_TOP_TEN, PRIZE_TOP_TWENTY and the rest
 */
extern s32 sTournamentPrizes[][TROPHY_PLACE_COUNT + 3];
extern s32 sPrizePoints;
extern s32 sPrizeShown;
extern s32 sResultsUnk94;
extern s32 sResultsUnk98;
extern s32 sResultsUnk8C;
extern s32 sResultsUnk78;
extern s32 sResultsUnk7C;
extern s32 sResultsUnk9C;
extern s32 sResultsUnkA0;
extern s32 sTournamentOpenCourseCount;
extern u16 D_800C2074[];
extern s32 D_801B60B4;

/* The images of the trophy a course's tournament gives its top three */
typedef struct TrophyAssets {
    /* 0x0 */ s32 palette; /* For first place; second and third follow */
    /* 0x4 */ s32 image;
    /* 0x8 */ s32 unk8;
} TrophyAssets; /* size = 0xC */

extern TrophyAssets sTrophyAssets[];

/* sPlayerRanks of a match's players */
#define MATCH_RANK_WON 100
#define MATCH_RANK_LOST 101

s32 func_8005D23C(void);

void FreeRoundResults(void);
void LoadRoundResults(void);
void DrawCourseResultsNotice(Gfx **displayListHead);
void DrawCharacterResultsNotice(Gfx **displayListHead);
s32 func_801F9AD0_ovl_00(void);
s32 func_801FA098_ovl_00(void);
s32 func_801FA248_ovl_00(void);
s32 func_801FA684_ovl_00(void);

/* The course points that open the courses up to courseIndex, together */
s32 SumCoursePointsNeeded(s32 courseIndex) {
    s32 total = 0;
    s32 i;

    for (i = 1; i < courseIndex + 1; i++) {
        total += GetCoursePointsNeeded(i);
    }
    return total;
}

/* Loads the results and slides them in, or with CLOSE_ROUND_RESULTS frees them */
void OpenRoundResults(s32 request) {
    if (request == CLOSE_ROUND_RESULTS) {
        FreeRoundResults();
        return;
    }
    LoadRoundResults();
    sResultsSlide = SCREEN_WIDTH;
    sResultsState = RESULTS_SLIDING_IN;
}

#ifdef NON_MATCHING
/*
 * Adds to the save the average points of the players that have them,
 * scaled by the course's rate. A macro, since the original repeats it
 * for each mode.
 */
#define AddRoundPoints(save)                                                                         \
    {                                                                                                \
        s32 counted_;                                                                                \
        s32 total_;                                                                                  \
        s32 i_;                                                                                      \
                                                                                                     \
        sOpenCourseCount = CountOpenCourses((save)->coursePoints);                                   \
        if (sOpenCourseCount < COURSE_COUNT) {                                                       \
            counted_ = 0;                                                                            \
            sPointsToNextCourse = GetCoursePointsNeeded(sOpenCourseCount) - (save)->coursePoints;    \
            sPointsEarned = 0;                                                                       \
            for (i_ = 0; i_ != g_PlayerCount; i_++) {                                                \
                if (sPlayerPoints[i_] >= 0) {                                                        \
                    counted_++;                                                                      \
                    sPointsEarned += sPlayerPoints[i_];                                              \
                }                                                                                    \
            }                                                                                        \
            if (counted_ == 0) {                                                                     \
                counted_ = 1;                                                                        \
            }                                                                                        \
            sPointsEarned = sPointsEarned * sCoursePointRates[GetCourseRecordIndex()] / counted_;    \
            sPointsShown = sPointsEarned;                                                            \
            sPointsToNextCourse -= sPointsEarned;                                                    \
        }                                                                                            \
        total_ = (save)->coursePoints + sPointsEarned;                                               \
        if (total_ > MAX_COURSE_POINTS) {                                                                   \
            (save)->coursePoints = MAX_COURSE_POINTS;                                                       \
        } else {                                                                                     \
            (save)->coursePoints = total_;                                                           \
        }                                                                                            \
    }

void LoadRoundResults(void) {
    EncodedAsset asset;
    AssetFile file;
    SaveData *save;
    s32 i;
    s32 j;
    s32 *rank;
    u32 size;

    save = GetSaveData();
    if (sAreResultsLoaded == TRUE) {
        return;
    }
    StopSound(RAIN_SOUND_CHANNEL);
    sAreResultsLoaded = TRUE;
    sUnlockNoticeKind = UNLOCK_NOTICE_KIND_NONE;
    sResultsUnk94 = 3;
    sResultsUnk98 = 0;
    sTallySpeed = 1;
    sResultsUnk8C = 0;
    if (IsGameFlagSet(GAME_FLAG_ADD_500_POINTS)) {
        g_Players[0].points += 500;
    }
    if (IsGameFlagSet(GAME_FLAG_ADD_9999_POINTS)) {
        g_Players[0].points += 9999;
    }

    sResultsBackdropImage = AllocMemory(OpenEncodedAsset(g_IsUnk3B ? RESULTS_UNK3B_BACKDROP_ASSET : RESULTS_BACKDROP_ASSET, &asset));
    ReadEncodedAsset(sResultsBackdropImage, &asset);
    sResultsHandImage = AllocMemory(OpenEncodedAsset(PRESSING_HAND_ASSET, &asset));
    ReadEncodedAsset(sResultsHandImage, &asset);
    sDigitsImage = AllocMemory(OpenEncodedAsset(RESULTS_DIGITS_ASSET, &asset));
    ReadEncodedAsset(sDigitsImage, &asset);
    D_80225738_ovl_00 = AllocMemory(OpenEncodedAsset(RESULTS_UNK98E_ASSET, &asset));
    ReadEncodedAsset(D_80225738_ovl_00, &asset);
    if (g_IsUnk3B) {
        D_80225730_ovl_00 = AllocMemory(OpenEncodedAsset(RESULTS_UNK3B_UNK949_ASSET, &asset));
        ReadEncodedAsset(D_80225730_ovl_00, &asset);
        sResultsCourseImage = AllocMemory(OpenEncodedAsset(D_800C2074[func_8005D23C()], &asset));
        ReadEncodedAsset(sResultsCourseImage, &asset);
    } else {
        D_802256C4_ovl_00 = AllocMemory(OpenEncodedAsset(RESULTS_UNK69F_ASSET, &asset));
        ReadEncodedAsset(D_802256C4_ovl_00, &asset);
        sResultsLabelsImage = AllocMemory(OpenEncodedAsset(LABELS_IMAGE_ASSET, &asset));
        ReadEncodedAsset(sResultsLabelsImage, &asset);
        sResultsLargeDigitsImage = AllocMemory(OpenEncodedAsset(LARGE_DIGITS_IMAGE_ASSET, &asset));
        ReadEncodedAsset(sResultsLargeDigitsImage, &asset);
        sResultsCloudImage = AllocMemory(OpenEncodedAsset(NOTICE_CLOUD_ASSET, &asset));
        ReadEncodedAsset(sResultsCloudImage, &asset);
        sResultsCourseImage = AllocMemory(OpenEncodedAsset(D_800C2074[0], &asset));
        ReadEncodedAsset(sResultsCourseImage, &asset);
        sNoticeTitleImage = AllocMemory(OpenEncodedAsset(NOTICE_TITLE_ASSET, &asset));
        ReadEncodedAsset(sNoticeTitleImage, &asset);
        sResultsShineImage = AllocMemory(OpenEncodedAsset(SHINE_ASSET, &asset));
        ReadEncodedAsset(sResultsShineImage, &asset);
        D_80225718_ovl_00 = AllocMemory(OpenEncodedAsset(RESULTS_UNK72C_IMAGE_ASSET, &asset));
        ReadEncodedAsset(D_80225718_ovl_00, &asset);
        OpenAssetFile(RESULTS_UNK72D_PALETTE_ASSET, &file);
        size = GetAssetFileSize(&file);
        D_8022571C_ovl_00 = AllocMemory(size);
        ReadAssetFile(D_8022571C_ovl_00, size, &file);
    }

    for (i = 0; i != g_PlayerCount; i++) {
        sPlayerPortraitImages[i] =
            AllocMemory(OpenEncodedAsset(g_CharacterImageAssets[(s8)g_Players[i].characterIndex], &asset));
        ReadEncodedAsset(sPlayerPortraitImages[i], &asset);
        OpenAssetFile(g_CharacterPaletteAssets[(s8)g_Players[i].characterIndex] + (s8)g_Players[i].paletteIndex,
                      &file);
        size = GetAssetFileSize(&file);
        sPlayerPortraitPalettes[i] = AllocMemory(size);
        ReadAssetFile(sPlayerPortraitPalettes[i], size, &file);
        sPlayerPoints[i] = (s8)g_Players[i].unk1E[0] == 0 ? g_Players[i].points : -1;
        sPlayerBetTotals[i] = 0;
        for (j = 0; j != BET_COUNT; j++) {
            sPlayerBetTotals[i] += g_Players[i].betTallies[j];
        }
    }

    if (!g_IsUnk3B) {
        if (g_GameMode == GAME_MODE_MATCH_GAME || g_GameMode == GAME_MODE_GET_CHARACTER) {
            sResultsPlacesImage = AllocMemory(OpenEncodedAsset(MATCH_PLACES_IMAGE_ASSET, &asset));
            ReadEncodedAsset(sResultsPlacesImage, &asset);
            D_80225708_ovl_00 = AllocMemory(OpenEncodedAsset(MATCH_UNK6A3_ASSET, &asset));
            ReadEncodedAsset(D_80225708_ovl_00, &asset);
            sNameplateImage = AllocMemory(OpenEncodedAsset(NAMEPLATE_ASSET, &asset));
            ReadEncodedAsset(sNameplateImage, &asset);
            D_8022572C_ovl_00 = AllocMemory(OpenEncodedAsset(MATCH_UNK8FE_ASSET, &asset));
            ReadEncodedAsset(D_8022572C_ovl_00, &asset);
            sPortraitCharacterIndex = (s8)g_Players[1].characterIndex;
        } else {
            sResultsPlacesImage = AllocMemory(OpenEncodedAsset(PLACES_IMAGE_ASSET, &asset));
            ReadEncodedAsset(sResultsPlacesImage, &asset);
        }
    }

    sHasBets = FALSE;
    for (i = 0; i != BET_COUNT; i++) {
        if (g_GameState.betRates[i] > 0) {
            sHasBets = TRUE;
        }
    }
    for (i = 0; i != g_PlayerCount; i++) {
        sPortraitAnimations[i] = 0;
        sPortraitFrames[i] = -1;
        sPortraitTimers[i] = 0;
    }

    if (g_IsUnk3B) {
        for (i = 0; i != g_PlayerCount; i++) {
            sPlayerResults[i] = GetNetScoreToPar(GetFirstHoleIndex() + GetRoundHoleCount(), i);
        }
    } else {
        switch (g_GameMode) {
            case GAME_MODE_STROKE:
            case GAME_MODE_SPEED_GOLF:
                for (i = 0; i != g_PlayerCount; i++) {
                    sPlayerResults[i] = GetNetScoreToPar(GetFirstHoleIndex() + GetRoundHoleCount(), i);
                }
                for (i = 0; i != g_PlayerCount; i++) {
                    rank = &sPlayerRanks[i];
                    *rank = 0;
                    for (j = 0; j != g_PlayerCount; j++) {
                        if (i != j && sPlayerResults[j] < sPlayerResults[i]) {
                            (*rank)++;
                        }
                    }
                }
                AddRoundPoints(save);
                break;
            case GAME_MODE_MATCH_GAME:
            case GAME_MODE_GET_CHARACTER:
                sPlayerResults[1] = 0;
                sPlayerResults[0] = 0;
                for (i = GetFirstHoleIndex(); i != g_HoleIndex; i++) {
                    if (g_Players[0].holes[i].strokes < g_Players[1].holes[i].strokes) {
                        sPlayerResults[0]++;
                        sPlayerResults[1]--;
                    } else if (g_Players[1].holes[i].strokes < g_Players[0].holes[i].strokes) {
                        sPlayerResults[0]--;
                        sPlayerResults[1]++;
                    }
                }
                if (sPlayerResults[1] < sPlayerResults[0]) {
                    sPlayerRanks[0] = MATCH_RANK_WON;
                    sPlayerRanks[1] = MATCH_RANK_LOST;
                } else {
                    sPlayerRanks[0] = MATCH_RANK_LOST;
                    sPlayerRanks[1] = MATCH_RANK_WON;
                }
                sPortraitImage =
                    AllocMemory(OpenEncodedAsset(g_CharacterImageAssets[(s8)g_Players[1].characterIndex], &asset));
                ReadEncodedAsset(sPortraitImage, &asset);
                OpenAssetFile(g_CharacterPaletteAssets[(s8)g_Players[1].characterIndex] +
                                  (s8)g_Players[1].paletteIndex,
                              &file);
                size = GetAssetFileSize(&file);
                sPortraitPalette = AllocMemory(size);
                ReadAssetFile(sPortraitPalette, size, &file);
                AddRoundPoints(save);
                break;
            case GAME_MODE_UNK1:
            case GAME_MODE_TRAINING:
            case GAME_MODE_UNK6:
            case GAME_MODE_UNK8:
            case GAME_MODE_RING_SHOT:
            case GAME_MODE_SKINS_MATCH:
            case GAME_MODE_CLUB_SLOTS:
                TallySkins(GetFirstHoleIndex() + GetRoundHoleCount());
                for (i = 0; i != g_PlayerCount; i++) {
                    sPlayerResults[i] = g_RoundHud.skinsTotals[i];
                }
                for (i = 0; i != g_PlayerCount; i++) {
                    rank = &sPlayerRanks[i];
                    *rank = 0;
                    for (j = 0; j != g_PlayerCount; j++) {
                        if (i != j && sPlayerResults[i] < sPlayerResults[j]) {
                            (*rank)++;
                        }
                    }
                }
                AddRoundPoints(save);
                break;
            case GAME_MODE_TOURNAMENT:
                D_80225708_ovl_00 = AllocMemory(OpenEncodedAsset(TOURNAMENT_UNK6A1_ASSET, &asset));
                ReadEncodedAsset(D_80225708_ovl_00, &asset);
                sPlayerResults[0] = GetNetScoreToPar(GetFirstHoleIndex() + GetRoundHoleCount(), 0);
                RankTournament(COURSE_HOLE_COUNT, 0, sPlayerResults[0], D_801B60B4);
                for (i = 0; i != TOURNAMENT_CONTESTANT_COUNT; i++) {
                    if (g_TournamentStandings[i].contestantId == -1) {
                        sPlayerRanks[0] = g_TournamentStandings[i].place & PLACE_MASK;
                        break;
                    }
                }
                if (sPlayerRanks[0] <= TROPHY_PLACE_COUNT) {
                    sPrizePoints = sTournamentPrizes[g_CourseIndex][sPlayerRanks[0] - 1];
                } else if (sPlayerRanks[0] <= PRIZE_TOP_TEN) {
                    sPrizePoints = sTournamentPrizes[g_CourseIndex][TROPHY_PLACE_COUNT];
                } else if (sPlayerRanks[0] <= PRIZE_TOP_TWENTY) {
                    sPrizePoints = sTournamentPrizes[g_CourseIndex][TROPHY_PLACE_COUNT + 1];
                } else {
                    sPrizePoints = sTournamentPrizes[g_CourseIndex][TROPHY_PLACE_COUNT + 2];
                }
                sPrizeShown = sPrizePoints;
                if (sPlayerRanks[0] <= TROPHY_PLACE_COUNT) {
                    sTrophyImage = AllocMemory(OpenEncodedAsset(sTrophyAssets[g_CourseIndex].image, &asset));
                    ReadEncodedAsset(sTrophyImage, &asset);
                    OpenAssetFile(sTrophyAssets[g_CourseIndex].palette + sPlayerRanks[0] - 1, &file);
                    size = GetAssetFileSize(&file);
                    sTrophyPalette = AllocMemory(size);
                    ReadAssetFile(sTrophyPalette, size, &file);
                }
                sPointsEarned = sPlayerPoints[0] * sCoursePointRates[g_CourseIndex];
                sPointsShown = sPointsEarned;
                sTournamentOpenCourseCount = sOpenCourseCount = CountOpenCourses(save->coursePoints);
                if (sOpenCourseCount < COURSE_COUNT) {
                    sPointsToNextCourse =
                        GetCoursePointsNeeded(sOpenCourseCount) - save->coursePoints - sPrizePoints - sPointsEarned;
                }
                if (save->coursePoints + sPrizePoints + sPointsEarned > MAX_COURSE_POINTS) {
                    save->coursePoints = MAX_COURSE_POINTS;
                } else {
                    save->coursePoints += sPrizePoints + sPointsEarned;
                }
                break;
        }
    }

    for (i = 0; i != g_PlayerCount; i++) {
        if ((s8)g_Players[i].characterIndex >= CHARACTER_KID && sPlayerPoints[i] > 0) {
            AddGameBoyExperience(g_Players[i].gameBoyCharacterIndex, sPlayerPoints[i]);
        }
    }
    switch (g_GameMode) {
        case GAME_MODE_STROKE:
            PlaySong(RESULTS_SONG, 0);
            break;
        case GAME_MODE_SPEED_GOLF:
            PlaySong(RESULTS_SONG, 0);
            break;
        case GAME_MODE_MATCH_GAME:
            PlaySong(RESULTS_SONG, 0);
            break;
        case GAME_MODE_GET_CHARACTER:
            PlaySong(RESULTS_SONG, 0);
            break;
        case GAME_MODE_SKINS_MATCH:
        case GAME_MODE_CLUB_SLOTS:
            PlaySong(RESULTS_SONG, 0);
            break;
        case GAME_MODE_TOURNAMENT:
            PlaySong(TOURNAMENT_RESULTS_SONG, 0);
            break;
    }
}

#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/round_results", LoadRoundResults);
#endif

/*
 * Loads the notice of what the round unlocked: one of the characters for
 * UNLOCK_NOTICE_* below UNLOCK_NOTICE_COURSES, or else the course from
 * UNLOCK_NOTICE_COURSES on.
 */
void LoadUnlockNotice(s32 unlock) {
    EncodedAsset asset;
    AssetFile file;
    s32 courseIndex;
    u32 size;

    GetSaveData();
    sIsNoticeHeld = FALSE;
    sNoticeTime = 0;
    sNoticeFaceIndex = 0;
    sNoticeLift = 0;
    sNoticeUnk6C = 0;
    sNoticeFrameCount = 0;
    sNoticeStage = 0;
    sNoticeSwing = 0;
    sNoticeUnk54 = 0;
    sNoticeAngle = 0;
    courseIndex = 0;
    if (unlock == UNLOCK_NOTICE_DONKEY_KONG) {
        sPortraitCharacterIndex = CHARACTER_DONKEY_KONG;
        sUnlockNoticeKind = UNLOCK_NOTICE_KIND_CHARACTER;
    } else if (unlock == UNLOCK_NOTICE_PEACH) {
        sPortraitCharacterIndex = CHARACTER_PEACH;
        sUnlockNoticeKind = UNLOCK_NOTICE_KIND_CHARACTER;
    } else if (unlock == UNLOCK_NOTICE_METAL_MARIO) {
        sPortraitCharacterIndex = CHARACTER_METAL_MARIO;
        sUnlockNoticeKind = UNLOCK_NOTICE_KIND_CHARACTER;
    } else {
        courseIndex = unlock - UNLOCK_NOTICE_COURSES;
        sUnlockNoticeKind = UNLOCK_NOTICE_KIND_COURSE;
    }
    sResultsShineImage = AllocMemory(OpenEncodedAsset(SHINE_ASSET, &asset));
    ReadEncodedAsset(sResultsShineImage, &asset);
    if (sUnlockNoticeKind == UNLOCK_NOTICE_KIND_COURSE) {
        sNoticeTitleImage = AllocMemory(OpenEncodedAsset(NOTICE_TITLE_ASSET, &asset));
        ReadEncodedAsset(sNoticeTitleImage, &asset);
        sResultsCourseImage = AllocMemory(OpenEncodedAsset(sCourseNoticeAssets[courseIndex], &asset));
        ReadEncodedAsset(sResultsCourseImage, &asset);
    }
    if (sUnlockNoticeKind == UNLOCK_NOTICE_KIND_CHARACTER) {
        sNoticeTitleImage = AllocMemory(OpenEncodedAsset(0x560, &asset));
        ReadEncodedAsset(sNoticeTitleImage, &asset);
        sNameplateImage = AllocMemory(OpenEncodedAsset(NAMEPLATE_ASSET, &asset));
        ReadEncodedAsset(sNameplateImage, &asset);
        sPortraitImage = AllocMemory(OpenEncodedAsset(g_CharacterImageAssets[sPortraitCharacterIndex], &asset));
        ReadEncodedAsset(sPortraitImage, &asset);
        OpenAssetFile(g_CharacterPaletteAssets[sPortraitCharacterIndex], &file);
        size = GetAssetFileSize(&file);
        sPortraitPalette = AllocMemory(size);
        ReadAssetFile(sPortraitPalette, size, &file);
    }
}

void FreeUnlockNotice(void) {
    FreeMemory(&sNoticeTitleImage);
    FreeMemory(&sResultsShineImage);
    if (sUnlockNoticeKind == UNLOCK_NOTICE_KIND_COURSE) {
        FreeMemory(&sResultsCourseImage);
    }
    if (sUnlockNoticeKind == UNLOCK_NOTICE_KIND_CHARACTER) {
        FreeMemory(&sNameplateImage);
        FreeMemory(&sPortraitImage);
        FreeMemory(&sPortraitPalette);
    }
    sUnlockNoticeKind = UNLOCK_NOTICE_KIND_NONE;
}

/* Swings the course in, or raises the character's face; returns whether the notice is done */
#ifdef NON_MATCHING
s32 UpdateUnlockNotice(void) {
    EncodedAsset asset;

    if (sUnlockNoticeKind == UNLOCK_NOTICE_KIND_COURSE) {
        sNoticeFrameCount++;
        switch (sNoticeStage) {
            case 0:
                sNoticeSwing = -sinf(sNoticeAngle) * NOTICE_SWING_HEIGHT;
                sNoticeUnk54 = 0;
                sNoticeAngle += NOTICE_SWING_STEP;
                if (sNoticeAngle > M_PI) {
                    sNoticeAngle = M_PI;
                }
                if (sNoticeFrameCount >= NOTICE_SWING_FRAMES) {
                    sNoticeStage = 1;
                }
                break;
            case 1:
                sNoticeSwing = -sinf(sNoticeAngle) * NOTICE_SWING_HEIGHT;
                sNoticeAngle += NOTICE_SWING_STEP;
                if (sNoticeFrameCount < NOTICE_DONE_FRAME) {
                    break;
                }
                return TRUE;
        }
        return FALSE;
    }
    sNoticeTime++;
    if (sIsNoticeHeld) {
        return FALSE;
    }
    if (sNoticeTime > NOTICE_FACE_DELAY && !(sNoticeTime & NOTICE_FACE_INTERVAL_MASK) &&
        sNoticeFaceIndex < NOTICE_LAST_FACE) {
        sNoticeFaceIndex++;
        OpenEncodedAsset(g_CharacterImageAssets[sPortraitCharacterIndex] + sNoticeFaceIndex, &asset);
        ReadEncodedAsset(sPortraitImage, &asset);
    }
    if (sNoticeTime > NOTICE_LIFT_DELAY) {
        if (sNoticeLift > -2) {
            sNoticeLift -= NOTICE_LIFT_START_SPEED;
        } else {
            sNoticeLift += sNoticeLift * 2 / 3;
        }
        if (sNoticeLift < NOTICE_LIFT_EXIT) {
            return TRUE;
        }
    }
    return FALSE;
}

#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/round_results", UpdateUnlockNotice);
#endif

void FreeRoundResults(void) {
    s32 i;

    if (sAreResultsLoaded) {
        sAreResultsLoaded = FALSE;
        FreeMemory(&sResultsBackdropImage);
        FreeMemory(&sResultsHandImage);
        FreeMemory(&sDigitsImage);
        FreeMemory(&D_80225738_ovl_00);
        if (g_IsUnk3B) {
            FreeMemory(&D_80225730_ovl_00);
            FreeMemory(&sResultsCourseImage);
        } else {
            FreeMemory(&D_802256C4_ovl_00);
            FreeMemory(&sResultsLabelsImage);
            FreeMemory(&sResultsLargeDigitsImage);
            FreeMemory(&sResultsCloudImage);
            FreeMemory(&sResultsCourseImage);
            FreeMemory(&sNoticeTitleImage);
            FreeMemory(&sResultsShineImage);
            FreeMemory(&D_80225718_ovl_00);
            FreeMemory(&D_8022571C_ovl_00);
        }
        for (i = 0; i != g_PlayerCount; i++) {
            FreeMemory(&sPlayerPortraitImages[i]);
            FreeMemory(&sPlayerPortraitPalettes[i]);
        }
        if (!g_IsUnk3B) {
            if (g_GameMode == GAME_MODE_MATCH_GAME || g_GameMode == GAME_MODE_GET_CHARACTER) {
                FreeMemory(&sResultsPlacesImage);
                FreeMemory(&D_80225708_ovl_00);
                FreeMemory(&sNameplateImage);
                FreeMemory(&D_8022572C_ovl_00);
            } else {
                FreeMemory(&sResultsPlacesImage);
            }
            switch (g_GameMode) {
                case GAME_MODE_STROKE:
                case GAME_MODE_UNK1:
                case GAME_MODE_TRAINING:
                case GAME_MODE_UNK6:
                case GAME_MODE_SPEED_GOLF:
                case GAME_MODE_UNK8:
                case GAME_MODE_RING_SHOT:
                case GAME_MODE_SKINS_MATCH:
                case GAME_MODE_CLUB_SLOTS:
                    break;
                case GAME_MODE_MATCH_GAME:
                case GAME_MODE_GET_CHARACTER:
                    FreeMemory(&sPortraitImage);
                    FreeMemory(&sPortraitPalette);
                    break;
                case GAME_MODE_TOURNAMENT:
                    FreeMemory(&D_80225708_ovl_00);
                    if (sPlayerRanks[0] <= TROPHY_PLACE_COUNT) {
                        FreeMemory(&sTrophyImage);
                        FreeMemory(&sTrophyPalette);
                    }
                    break;
            }
        }
    }
}

void StartPortraitAnimation(s32 animation, s32 playerIndex) {
    sPortraitAnimations[playerIndex] = animation;
    sPortraitFrames[playerIndex] = 0;
    sPortraitTimers[playerIndex] = 0;
}

/* Ticks as the points count up, on every fourth call while sTallySpeed is 1 */
void PlayTallySound(void) {
    sTallySoundCount++;
    if (sTallySpeed != 1 || !(sTallySoundCount & 3)) {
        PlaySound(SOUND_TALLY_TICK, g_NextSoundChannel, 0x7F);
    }
}

/* Draws value right-aligned in four of the 8 by 12 digits of sDigitsImage */
void DrawResultsNumber(Gfx **displayListHead, s32 x, s32 y, s32 value) {
    Gfx *gfx = *displayListHead;
    u8 *c;
    u8 ch;
    s32 offset;

    sprintf(g_PrintBuffer, sNumberFormat, value);
    LoadSmallPalette(&gfx, 0, sDigitsImage + IMAGE_HEADER_SIZE);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTextureBlock_4b(gfx++, ((u32)sDigitsImage + PALETTED_TEXELS_OFFSET) & ~7, G_IM_FMT_CI, 128, RESULTS_DIGIT_HEIGHT, 0,
                           G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                           G_TX_NOLOD);
    c = (u8 *)g_PrintBuffer;
    offset = 0;
    while (*c != '\0') {
        ch = *c;
        if (ch != ' ') {
            gSPScisTextureRectangle(gfx++, (x + offset) << 2, y << 2, (x + offset + RESULTS_DIGIT_WIDTH) << 2,
                                    (y + RESULTS_DIGIT_HEIGHT) << 2, G_TX_RENDERTILE,
                                    (ch - '0') * RESULTS_DIGIT_WIDTH << 5, 0, 1 << 10, 1 << 10);
            /* The empty do-while is there to match */
            do {
            } while (0);
        }
        c++;
        offset += RESULTS_DIGIT_WIDTH;
    }
    *displayListHead = gfx;
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_results", func_801F6718_ovl_00);

void DrawPlaceIcon(Gfx **displayListHead, s32 x, s32 y, s32 place) {
    Gfx *gfx = *displayListHead;

    LoadLargePalette(&gfx, sResultsPlacesImage + IMAGE_HEADER_SIZE);
    LoadTextureBlock(&gfx, sResultsPlacesImage + ((place * PLACE_ICON_SIZE * PLACE_ICON_SIZE) + LARGE_PALETTED_TEXELS_OFFSET), G_IM_FMT_CI,
                     G_IM_SIZ_8b, PLACE_ICON_SIZE, PLACE_ICON_SIZE, 0, 0, 0, 0, 0, 0, 0);
    DrawTextureRect(&gfx, x * 4, y * 4, (x + PLACE_ICON_SIZE) * 4, (y + PLACE_ICON_SIZE) * 4, G_TX_RENDERTILE, 0, 0,
                    TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    *displayListHead = gfx;
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_results", func_801F6DFC_ovl_00);

void DrawResultsLabel(Gfx **displayListHead, s32 x, s32 y, s32 label) {
    Gfx *gfx = *displayListHead;

    LoadSmallPalette(&gfx, 0, sResultsLabelsImage + IMAGE_HEADER_SIZE);
    gDPLoadTextureTile_4b(gfx++,
                          (u32)(sResultsLabelsImage +
                                (label * (RESULTS_LABEL_STORED_WIDTH * RESULTS_LABEL_HEIGHT / 2) + PALETTED_TEXELS_OFFSET)) &
                              ~7,
                          G_IM_FMT_CI, RESULTS_LABEL_STORED_WIDTH, 0, 0, 0, RESULTS_LABEL_WIDTH - 1,
                          RESULTS_LABEL_HEIGHT - 1, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP,
                          G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    DrawTextureRect(&gfx, x * 4, y * 4, (x + RESULTS_LABEL_WIDTH) * 4, (y + RESULTS_LABEL_HEIGHT) * 4,
                    G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    *displayListHead = gfx;
}

/*
 * isMirrored starts the texture a width in (S10.5 texels), where its
 * mirrored wrap flips it
 */
void DrawPlayerPortrait(Gfx **displayListHead, s32 x, s32 y, s32 playerIndex, s32 isMirrored) {
    Gfx *gfx = *displayListHead;

    LoadLargePalette(&gfx, sPlayerPortraitPalettes[playerIndex]);
    LoadTextureBlock(&gfx, sPlayerPortraitImages[playerIndex], G_IM_FMT_CI, G_IM_SIZ_8b, PORTRAIT_SIZE, PORTRAIT_SIZE,
                     0, G_TX_MIRROR, G_TX_NOMIRROR, 5, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    DrawTextureRect(&gfx, x * 4, y * 4, (x + PORTRAIT_SIZE) * 4, (y + PORTRAIT_SIZE) * 4, G_TX_RENDERTILE,
                    isMirrored * (PORTRAIT_SIZE << 5), 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    *displayListHead = gfx;
}

void StartResultsNotice(s32 kind) {
    sUnlockNoticeKind = kind;
    switch (kind) {
        case UNLOCK_NOTICE_KIND_NONE:
            break;
        case UNLOCK_NOTICE_KIND_COURSE:
            sNoticeStage = 0;
            sNoticeFrameCount = 0;
            sNoticeSwing = SCREEN_WIDTH;
            sNoticeUnk54 = 0;
            sNoticeAngle = -1.57f;
            break;
        case UNLOCK_NOTICE_KIND_CHARACTER:
            sIsNoticeHeld = FALSE;
            sNoticeTime = 0;
            sNoticeFaceIndex = 0;
            sNoticeLift = 0;
            sNoticeUnk6C = 0;
            break;
    }
}

INCLUDE_RODATA("asm/nonmatchings/ovl_00/round_results", sNumberFormat);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/round_results", D_80224CBC_ovl_00);

/* Swings the notice in, then counts the course it opened */
void UpdateCourseResultsNotice(void) {
    SaveData *save = GetSaveData();

    sNoticeFrameCount++;
    switch (sNoticeStage) {
        case 0:
            sNoticeSwing = -sinf(sNoticeAngle) * NOTICE_SWING_HEIGHT;
            sNoticeUnk54 = 0;
            sNoticeAngle += NOTICE_SWING_STEP;
            if (sNoticeAngle > M_PI) {
                sNoticeAngle = M_PI;
            }
            if (sNoticeFrameCount >= NOTICE_SWING_FRAMES) {
                sNoticeStage = 1;
            }
            break;
        case 1:
            sNoticeSwing = -sinf(sNoticeAngle) * NOTICE_SWING_HEIGHT;
            sNoticeAngle += NOTICE_SWING_STEP;
            if (sNoticeFrameCount >= NOTICE_DONE_FRAME) {
                StartResultsNotice(UNLOCK_NOTICE_KIND_NONE);
                sOpenCourseCount++;
                if (sOpenCourseCount < COURSE_COUNT) {
                    sPointsToNextCourse = GetCoursePointsNeeded(sOpenCourseCount) - save->coursePoints;
                }
            }
            break;
    }
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_results", DrawCourseResultsNotice);

/* Changes the beaten character's face, then lifts the notice away */
void UpdateCharacterResultsNotice(void) {
    EncodedAsset asset;

    GetSaveData();
    sNoticeTime++;
    if (sIsNoticeHeld) {
        return;
    }
    if (sNoticeTime > NOTICE_FACE_DELAY && !(sNoticeTime & NOTICE_FACE_INTERVAL_MASK) &&
        sNoticeFaceIndex < NOTICE_LAST_FACE) {
        sNoticeFaceIndex++;
        OpenEncodedAsset(g_CharacterImageAssets[(s8)g_Players[1].characterIndex] + sNoticeFaceIndex, &asset);
        ReadEncodedAsset(sPortraitImage, &asset);
    }
    if (sNoticeTime > NOTICE_LIFT_DELAY) {
        if (sNoticeLift > -2) {
            sNoticeLift -= NOTICE_LIFT_START_SPEED;
        } else {
            sNoticeLift += sNoticeLift * 2 / 3;
        }
        if (sNoticeLift < NOTICE_LIFT_EXIT) {
            StartResultsNotice(UNLOCK_NOTICE_KIND_NONE);
        }
    }
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_results", DrawCharacterResultsNotice);

/* Returns TRUE once the results have been saved and freed */
s32 UpdateRoundResults(void) {
    s32 isDone;

    switch (sResultsState) {
        case RESULTS_SLIDING_IN:
            if (sResultsSlide == 1) {
                sResultsState = RESULTS_SHOWN;
                sResultsSlide = 0;
                sResultsUnk78 = 0;
                sResultsUnk7C = 0;
                break;
            }
            sResultsSlide += -sResultsSlide / 2;
            break;
        case RESULTS_SHOWN:
            if (g_IsUnk3B) {
                isDone = func_801FA098_ovl_00();
            } else {
                switch (g_GameMode) {
                    case GAME_MODE_STROKE:
                    case GAME_MODE_UNK6:
                    case GAME_MODE_SPEED_GOLF:
                    case GAME_MODE_SKINS_MATCH:
                    case GAME_MODE_CLUB_SLOTS:
                        isDone = func_801FA248_ovl_00();
                        break;
                    case GAME_MODE_MATCH_GAME:
                    case GAME_MODE_GET_CHARACTER:
                        isDone = func_801FA684_ovl_00();
                        break;
                    case GAME_MODE_TOURNAMENT:
                        isDone = func_801F9AD0_ovl_00();
                        break;
                    default:
                        return FALSE;
                }
            }
            if (isDone) {
                sResultsState = RESULTS_CLOSING;
            }
            break;
        case RESULTS_CLOSING:
            WriteSaveData();
            OpenRoundResults(CLOSE_ROUND_RESULTS);
            return TRUE;
    }
    return FALSE;
}

s32 GetResultsUnk94(void) {
    return sResultsUnk94;
}

void SetResultsUnk94(s32 value, s32 unk98) {
    sResultsUnk94 = value;
    sResultsUnk98 = unk98;
    if (value == 0) {
        sResultsUnk9C = SCREEN_WIDTH;
        sResultsUnkA0 = 0;
    }
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_results", func_801F8BB8_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_results", func_801F9AD0_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_results", func_801FA098_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_results", func_801FA248_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_results", func_801FA684_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_results", func_801FAD2C_ovl_00);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/round_results", D_80224DD4_ovl_00);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/round_results", D_80224DD8_ovl_00);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/round_results", D_80224DE0_ovl_00);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/round_results", D_80224DE4_ovl_00);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/round_results", D_80224DF4_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_results", func_801FB02C_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_results", func_801FB500_ovl_00);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/round_results", D_80224E08_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_results", func_801FBBBC_ovl_00);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/round_results", D_80224E10_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_results", func_801FC3D4_ovl_00);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/round_results", D_80224E3C_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_results", func_801FCC48_ovl_00);

void DrawResultsNotice(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;

    switch (sUnlockNoticeKind) {
        case UNLOCK_NOTICE_KIND_NONE:
            break;
        case UNLOCK_NOTICE_KIND_COURSE:
            DrawCourseResultsNotice(&gfx);
            break;
        case UNLOCK_NOTICE_KIND_CHARACTER:
            DrawCharacterResultsNotice(&gfx);
            break;
    }
    *displayListHead = gfx;
}
