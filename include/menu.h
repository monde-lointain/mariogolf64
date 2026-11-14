#ifndef MENU_H
#define MENU_H

#include <ultra64.h>

#include "asset.h"
#include "character.h"
#include "course.h"
#include "language.h"
#include "password.h"
#include "play_record.h"
#include "player.h"
#include "text_format.h"

enum MenuScreen {
    MENU_SCREEN_TITLE,
    MENU_SCREEN_CHARACTER_SELECT,
    MENU_SCREEN_MAIN,
    MENU_SCREEN_SAVED_GAMES,
    MENU_SCREEN_OPTIONS,
    MENU_SCREEN_MODE_SELECT,
    MENU_SCREEN_COURSE_SELECT,
    MENU_SCREEN_BEST_TIMES,
    MENU_SCREEN_CADDY_SELECT,
    MENU_SCREEN_CLUBHOUSE,
    MENU_SCREEN_COURSE_STATS = 11,
    MENU_SCREEN_BEST_SHOTS,
    MENU_SCREEN_RING_SHOT_RECORDS,
    MENU_SCREEN_BIRDIE_BADGES,
    MENU_SCREEN_CHARACTER_TABLE,
    MENU_SCREEN_OPPONENT_SELECT,
    MENU_SCREEN_HELP,
    MENU_SCREEN_BLANK,
    MENU_SCREEN_EXIT = 19,
    MENU_SCREEN_HANDICAPS,
    MENU_SCREEN_BET_SETTINGS = 22,
    MENU_SCREEN_RING_SHOT_EXIT,
    MENU_SCREEN_TOURNAMENT_CHARACTER_SELECT,
    MENU_SCREEN_TRAINING_EXIT = 26,
    MENU_SCREEN_CLUB_TOURNAMENT,
    MENU_SCREEN_MAIN_WITHOUT_TRANSMIT
};

typedef s32 MenuScreen;

#define MENU_SCREEN_COUNT 29
#define NO_MENU_SCREEN (-1)
extern s8 g_LoadedMenuScreen;
extern AssetId g_MenuScreenImageAssets[MENU_SCREEN_COUNT];
extern MenuScreen g_MenuScreen;

extern s32 g_MenuRepeatButtons;
extern s32 g_MenuPressedButtons;

extern AssetId g_CourseIconAssets[];
extern AssetId g_TournamentIconAssets[];
// How many courses the course choice offers; always COURSE_COUNT.
extern s32 g_MenuCourseCount;
extern u8 g_AreBetPagesBlocked;

#define TROPHY_FRAME_LIMIT 16
#define TROPHY_PALETTE_COUNT 3
extern AssetId g_TrophyFrameAssets[][TROPHY_FRAME_LIMIT];
#define TROPHY_FRAME_END (-1)
extern SignedAssetId g_TrophyPaletteFiles[][TROPHY_PALETTE_COUNT];
extern PaletteColor *g_TrophyPalettes[TROPHY_PALETTE_COUNT];
#define TROPHY_FRAME_BUFFER_COUNT 2
extern ImageAsset *g_TrophyFrameBuffers[TROPHY_FRAME_BUFFER_COUNT];
extern ImageAsset *g_MenuCourseSceneImage;

#define PLAYER_COUNT_ASSET_ROW_LENGTH 12
extern AssetId g_PlayerCountIconAssets[][PLAYER_COUNT_ASSET_ROW_LENGTH];

// A rectangle corner in screen pixels.
typedef struct ScreenCorner {
    s32 x;
    s32 y;
} ScreenCorner;

typedef struct TrophySize {
    s8 width;
    s8 height;
} TrophySize;

extern TrophySize g_TrophySizes[COURSE_COUNT];

#define ANIMATED_ICON_BUFFER_COUNT 2
#define MENU_SPARE_IMAGE_COUNT 2
#define MENU_ARROW_SIZED_IMAGE_COUNT 5
extern ImageAsset *g_AnimatedIconImages[ANIMATED_ICON_BUFFER_COUNT];
extern ImageAsset *g_SecondAnimatedIconImages[ANIMATED_ICON_BUFFER_COUNT];
extern ImageAsset *g_MenuSpareImages[MENU_SPARE_IMAGE_COUNT];
extern ImageAsset *g_MenuArrowSizedImages[MENU_ARROW_SIZED_IMAGE_COUNT];

#define MINI_GOLF_DISC_IMAGE_COUNT 2
#define MINI_GOLF_TURN_FRAME_COUNT 5
#define MINI_GOLF_FACING_FRAME 2
extern AssetId g_MiniGolfDiscFrameAssets[MINI_GOLF_COURSES_PER_RECORD * MINI_GOLF_DISC_IMAGE_COUNT]
                                        [MINI_GOLF_TURN_FRAME_COUNT];
extern AssetId g_MiniGolfCourseIconAssets[];
extern SignedAssetId g_MiniGolfDiscFiles[];

extern ImageAsset *g_CharacterFaceImages[CHARACTER_COUNT];
extern PaletteColor *g_CharacterFacePalettes[CHARACTER_COUNT];

extern ImageAsset *g_ComputerLeftyIcons;

#define IMAGE_SLOT_COUNT 14
extern ImageAsset *g_ImageSlots[IMAGE_SLOT_COUNT];

#define CHARACTER_GRID_ROW_COUNT 3
#define CHARACTER_GRID_COLUMN_COUNT 7
#define CHARACTER_CHOICE_ROW_COUNT 2
#define GAME_BOY_CHARACTER_ROW 2

#define MARKED_CHARACTER_OFFSET 50
#define EMPTY_GRID_SLOT (-1)
#define MARKED_CHARACTER_LIMIT (-10)

extern s16 g_CharacterGrid[CHARACTER_GRID_ROW_COUNT][CHARACTER_GRID_COLUMN_COUNT];

enum GridSearchResult {
    CHARACTER_OFF_GRID = -1,
    CHARACTER_ON_GRID
};

s32 FindCharacterOnGrid(CharacterIndex characterIndex);

#define NO_CONTROLLER_INDEX 4
#define UNCHOSEN_CONTROLLER (-1)
#define NO_IMPORTED_COLUMN (-1)
extern s8 g_PlayerControllerIndices[PLAYER_COUNT_MAX];
extern s8 g_ChoosingControllerIndices[PLAYER_COUNT_MAX];
extern s16 g_ImportedCharacterColumns[PLAYER_COUNT_MAX];

#define PLAYER_LABEL_WIDTH 16
#define PLAYER_LABEL_HEIGHT 8
#define PLAYER_LABEL_IMAGE_SIZE (PLAYER_LABEL_WIDTH * PLAYER_LABEL_HEIGHT / 2)
#define PLAYER_LABEL_FIRST_COMPUTER 4
#define PLAYER_LABEL_COMPUTER 8
#define PLAYER_LABEL_OFFSET_X 8
#define PLAYER_LABEL_OFFSET_Y 25
#define CONTROLLER_ICON_SIZE 32
#define CONTROLLER_ICON_IMAGE_SIZE (CONTROLLER_ICON_SIZE * CONTROLLER_ICON_SIZE)

#define BABY_MARIO_RAISE 24
#define MENU_ANIMATION_STANDING 16
#define MENU_ANIMATION_FIDGET 17
#define FIDGET_DELAY_RANGE 128
#define FIDGETING (-1)
#define MENU_CHARACTERS_NEAR 1.0f
#define MENU_CHARACTERS_FAR 3000.0f
#define MENU_CHARACTERS_ORTHO_SCALE 10.0f
#define MENU_CHARACTERS_EYE_Y 70.0f
#define MENU_CHARACTERS_EYE_Z 60.0f

#define MODE_ICON_TOURNAMENT 0
#define MODE_ICON_GET_CHARACTER 1
#define MODE_ICON_STROKE 2
#define MODE_ICON_SPEED_GOLF 3
#define MODE_ICON_TRAINING 4
#define MODE_ICON_MINI_GOLF 5
#define MODE_ICON_MATCH_GAME 6
#define MODE_ICON_SKINS_MATCH 7
#define MODE_ICON_RING_SHOT 8
#define MODE_ICON_UNLISTED_STROKE 9
#define MODE_ICON_CLUB_SLOTS 10
#define NO_MODE_ICON (-1)
#define MODE_ICON_COUNT 11
#define MODE_ICON_ANIMATION_COUNT 14
#define MODE_ICON_FRAME_COUNT 27
#define END_OF_ICON_FRAMES (-1)

extern s16 g_ModeIconAnimations[MODE_ICON_ANIMATION_COUNT][MODE_ICON_FRAME_COUNT];

// Images that more than one menu draws.
#define NAME_PLATE_WIDTH 200
#define NAME_PLATE_HEIGHT 30
#define NAME_PLATE_STRIP_HEIGHT 10
#define NAME_PLATE_STRIP_COUNT 3
#define COURSE_SCENE_WIDTH 152
#define COURSE_SCENE_STRIP_ROW_COUNT 11
#define COURSE_SCENE_STRIP_COUNT 11
#define COURSE_SCENE_SCALE 2
// Each strip loads extra rows below it so filtering blends across the seam.
#define COURSE_SCENE_SEAM_ROW_COUNT 2
#define COURSE_SCENE_LEFT 8
#define COURSE_SCENE_FIRST_TOP 8
// The first strip shows one extra row, so later strips are laid out from here.
#define COURSE_SCENE_TOP (COURSE_SCENE_FIRST_TOP + COURSE_SCENE_SCALE)
#define STAR_WIDTH 32
#define STAR_HEIGHT 24
#define STAR_FRAME_COUNT 15
#define MODE_ICON_WIDTH 64
#define MODE_ICON_STRIP_HEIGHT 32
#define MODE_ICON_STRIP_COUNT 2
#define MODE_ICON_FRAME_DURATION 4
#define MODE_LABEL_WIDTH 192
#define MODE_LABEL_HEIGHT 20
#define MODE_LABEL_IMAGE_SIZE ((MODE_LABEL_WIDTH * MODE_LABEL_HEIGHT) / 2)
#define MODE_LABEL_X 160
#define SINGLE_PLAYER_MODE_LABEL_X 170
#define MODE_LABEL_TOP 156
#define COURSE_IMAGE_WIDTH 64
#define COURSE_IMAGE_STRIP_HEIGHT 32
#define COURSE_IMAGE_STRIP_COUNT 2
#define COURSE_NUMBERS_WIDTH 136
#define COURSE_NUMBERS_HEIGHT 16
#define COURSE_DIGIT_WIDTH 16
#define COURSE_WORD_WIDTH 40
#define NINTENDO_BADGE_WIDTH 144
#define NINTENDO_BADGE_STRIP_HEIGHT 10
#define NINTENDO_BADGE_STRIP_COUNT 2

#define FACE_WITH_NAME 0
#define FACE_PLAIN 1
#define FACE_DARKENED 10
#define FACE_DARKENED_SHADE 10
#define FACE_NAME_OFFSET_X 24
#define FACE_NAME_OFFSET_Y 12

#define WHEEL_FIELD_OF_VIEW 120.0f
#define WHEEL_NEAR_PLANE 10.0f
#define WHEEL_FAR_PLANE 30000.0f
#define WHEEL_EYE_HEIGHT 80.0f
#define WHEEL_EYE_DISTANCE 120.0f
#define WHEEL_RADIUS_X 104.0f
#define WHEEL_RADIUS_Z 40.0f
#define WHEEL_SLOT_LIMIT 8
#define WHEEL_RAISE 32
#define WHEEL_ICON_RAISE 12
#define DEGREES_PER_SPIN_STEP 4
#define COURSE_ICON_WIDTH 64
#define COURSE_ICON_STRIP_HEIGHT 32
#define COURSE_ICON_STRIP_COUNT 2
#define COURSE_ICON_HALF_SIZE 32.0f
#define WHEEL_ARROW_WIDTH 32
#define WHEEL_ARROW_X 160
#define WHEEL_ARROW_Y 94

s32 SaveOrContinueGame(s32 slotIndex, Boolean isContinuing);
MessageId GetClubTournamentCourseTitleMessage(void);
GameMode GetClubTournamentGameMode(void);
MessageId GetClubTournamentModeMessage(void);
MessageId GetClubTournamentNameMessage(void);
s32 GetClubTournamentNumber(void);
s32 GetRingShotCourseOpenedAtStarCount(void);
Boolean HasEveryRingShotStar(void);
void InsertBestTime(
    CourseIndex courseIndex,
    s32 time,
    s32 scoreToPar,
    CharacterIndex characterIndex,
    s32 paletteIndex
);
void InsertRankedRecord(PlayRecord *record);
Boolean IsDonkeyKongUnlocked(void);
Boolean IsMapleUnlocked(void);
Boolean IsMetalMarioUnlocked(void);
Boolean IsNintendoClubTournament(void);
void LoadMenuResources(void);
void FreeMenuResources(void);

#define MENU_CURSOR_SIZE 48
#define MENU_CURSOR_VERTEX_COUNT 4
#define MENU_CURSOR_PHASE_STEP (M_PI / 12)
#define MENU_CURSOR_COLOR_SWING 127.0f
#define MENU_CURSOR_COLOR_MIDDLE 127
#define MENU_CURSOR_NEAR 10.0f
#define MENU_CURSOR_FAR 2000.0f
#define MENU_CURSOR_Z (-100.0f)
#define MENU_CURSOR_SCALE_Y 0.87f

enum BetSet {
    BET_SET_NONE,
    BET_SET_STROKE,
    BET_SET_STROKE_FOUR_PLAYER,
    BET_SET_MATCH_GAME,
    BET_SET_SKINS_MATCH,
    BET_SET_CLUB_SLOTS,
    BET_SET_MINI_GOLF,
    BET_SET_MINI_GOLF_FOUR_PLAYER
};

typedef u8 BetSet;

extern BetSet g_BetSet;
extern s16 g_LastMenuPlayerCountChoice;
extern Boolean g_IsNintendoClubTournament;
extern s32 g_ClubTournamentNameIndex;
extern s32 g_ClubTournamentNumber;
extern u8 g_TournamentCode[TOURNAMENT_CODE_SIZE];
extern s16 g_LastMenuScreen;
extern ImageAsset *g_MenuIconImages[7];
#define MENU_ICON_IMAGE_PAIRS ((void *(*)[MINI_GOLF_DISC_IMAGE_COUNT])g_MenuIconImages)
extern ImageAsset *g_MenuBoardImages[2];
extern ImageAsset *g_MenuTitleImages[2];
extern ImageAsset *g_MenuWideSpareImage;
extern ImageAsset *g_SpinningBadgeImage;
extern ImageAsset *g_CourseTrophyImages[2];
extern ImageAsset *g_MiniGolfDiscImages[2];
extern ImageAsset *g_HoleBadgeImages;
extern ImageAsset *g_BrickWallImage;
extern ImageAsset *g_ModeIconImages[7];

extern u8 g_UnreadMenuBuffer[0x1218];

void BuildCharacterGrid(s32 playerCount, Boolean skipsImportedFaces);
void ClearImportedCharacters(void);
void EnterMenuAtBestShots(void);
void EnterMenuAtBlankScreen(void);
void EnterMenuAtCourseSelect(void);
void EnterMenuAtModeSelect(void);
void EnterMenuAtRingShotRecords(void);
void EnterMenuAtTitle(void);
s32 GetClubTournamentCourse(void);
void InvalidateBrokenRingShotRecords(void);
void InvalidateBrokenSavedGames(void);
void RememberPlayedHole(void);
void ResetMenuChoices(void);
s32 SaveOrContinueGameByMode(Boolean isContinuing);
void SaveRingShotRecord(s32 recordIndex, RingShotRecord *record);
void SortIndicesDescending(s32 *values, s32 *order, s32 count);

#endif
