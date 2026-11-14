/*
 * The save data as the menus use it: records, best times, unlocks, Club
 * Tournaments and round statistics, and the menus' resources.
 */

#ifndef MENU_H
#define MENU_H

#include <ultra64.h>

#include "characters/character.h"
#include "course/course.h"
#include "menus/password.h"
#include "round/player.h"
#include "round/scoring.h"
#include "save/play_record.h"
#include "system/asset.h"
#include "system/c_language.h"
#include "text/text_format.h"

// The screens that the menu overlay shows, which g_MenuScreen picks
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
    // Get Character's choice of the second player's character
    MENU_SCREEN_OPPONENT_SELECT,
    MENU_SCREEN_HELP,
    // Shows nothing and waits for A to go to the title; the menu is left on it
    // when it exits for anything but Training
    MENU_SCREEN_BLANK,
    // Loads what the chosen game needs and leaves the menu
    MENU_SCREEN_EXIT = 19,
    MENU_SCREEN_HANDICAPS,
    MENU_SCREEN_BET_SETTINGS = 22,
    // Leaves the menu for Ring Shot with a wipe of the chosen course's shape
    MENU_SCREEN_RING_SHOT_EXIT,
    // The one player's character select for a Club Tournament
    MENU_SCREEN_TOURNAMENT_CHARACTER_SELECT,
    // Leaves the menu for Training, whose hole is already loading
    MENU_SCREEN_TRAINING_EXIT = 26,
    // A tournament entered with a code
    MENU_SCREEN_CLUB_TOURNAMENT,
    // The main menu when no Transfer Pak holds Mario Golf for Game Boy
    MENU_SCREEN_MAIN_WITHOUT_TRANSMIT
};

typedef s32 MenuScreen;
typedef s8 MenuScreen8;
typedef s16 MenuScreen16;

#define MENU_SCREEN_COUNT 29
// g_LoadedMenuScreen's value before any screen's assets are read
#define NO_MENU_SCREEN (-1)
// The screen whose assets UpdateMenus last read
extern MenuScreen8 g_LoadedMenuScreen;
// Each screen's title, which its banner shows
extern AssetId g_MenuScreenImageAssets[MENU_SCREEN_COUNT];
extern MenuScreen g_MenuScreen;

// The buttons of every controller: those repeating while held, and those just
// pressed
extern s32 g_MenuRepeatButtons;
extern s32 g_MenuPressedButtons;

// Each course's icon, then mini golf's
extern AssetId g_CourseIconAssets[];
// The courses' icons, then the putter tournaments' badges
extern AssetId g_TournamentIconAssets[];
// How many courses the course select offers
extern s32 g_MenuCourseCount;
// Set for a one-player Stroke Play or Mini-Golf round: the course select's
// Up-C then can't open the bet pages, and its Bet badge doesn't cycle its color
extern UBoolean8 g_AreBetPagesBlocked;

// Each course's frames of its spinning trophy, and the files of the trophy's
// place palettes
#define TROPHY_FRAME_LIMIT 16
#define TROPHY_PALETTE_COUNT 3
extern AssetId g_TrophyFrameAssets[][TROPHY_FRAME_LIMIT];
// Ends a course's trophy frames before TROPHY_FRAME_LIMIT
#define TROPHY_FRAME_END (-1)
extern SignedAssetId g_TrophyPaletteFiles[][TROPHY_PALETTE_COUNT];
// The chosen course's trophy palettes, by the place they show
extern PaletteColor *g_TrophyPalettes[TROPHY_PALETTE_COUNT];
// The trophy frames are double buffered: one buffer is shown while the next
// frame is read into the other
#define TROPHY_FRAME_BUFFER_COUNT 2
// The course select's buffers for the chosen course's trophy frame
extern ImageAsset *g_TrophyFrameBuffers[TROPHY_FRAME_BUFFER_COUNT];
// The menus' ASSET_COURSE_SCENE: the sky and the course select's backdrops
extern ImageAsset *g_MenuCourseSceneImage;

// Screen 5's images in place of g_CourseIconAssets's: a row of asset IDs for
// each g_ChosenPlayerCount from 1, of which it reads as many as
// g_CourseIconAssets has
#define PLAYER_COUNT_ASSET_ROW_LENGTH 12
extern AssetId g_PlayerCountIconAssets[][PLAYER_COUNT_ASSET_ROW_LENGTH];

// The ways SlideBrickWall slides the stats panel's rows
enum StatsSlide {
    STATS_SLIDE_CURRENT,
    STATS_SLIDE_OUT_RIGHT,
    STATS_SLIDE_OUT_LEFT,
    STATS_SLIDE_IN_FROM_RIGHT,
    STATS_SLIDE_IN_FROM_LEFT,
    STATS_SLIDE_COUNT
};

typedef s32 StatsSlide;
typedef u8 UStatsSlide8;

typedef struct ScreenCorner {
    s32 x;
    s32 y;
} ScreenCorner;

// The size of a course's tournament trophy images
typedef struct TrophySize {
    s8 width;
    s8 height;
} TrophySize;

extern TrophySize g_TrophySizes[COURSE_COUNT];

// An animated icon's frame buffers: one is shown while the next frame is read
// into the other
#define ANIMATED_ICON_BUFFER_COUNT 2
#define MENU_SPARE_IMAGE_COUNT 2
#define MENU_ARROW_SIZED_IMAGE_COUNT 5
// The animated icon's two frame buffers, the size of a mode icon, which some
// screens read other images into
extern ImageAsset *g_AnimatedIconImages[ANIMATED_ICON_BUFFER_COUNT];
// A second animated icon's two frame buffers, the size of a mode icon, which
// some screens read other images into
extern ImageAsset *g_SecondAnimatedIconImages[ANIMATED_ICON_BUFFER_COUNT];
// Two buffers, each twice the size of a Toad image, that menu screens read
// their own images into
extern ImageAsset *g_MenuSpareImages[MENU_SPARE_IMAGE_COUNT];
// Five buffers the size of the green arrows image, which menu screens read
// other images into
extern ImageAsset *g_MenuArrowSizedImages[MENU_ARROW_SIZED_IMAGE_COUNT];

// The mini-golf disc's images in each of g_MenuIconImages's buffers
#define MINI_GOLF_DISC_IMAGE_COUNT 2
#define MENU_IMAGE_BUFFER_COUNT 2
#define MENU_ICON_COUNT 7
// The disc's frames as it turns from one course to the next, each two images,
// read into the buffers of g_MenuIconImages not shown; the courses face the
// screen at the middle frame
#define MINI_GOLF_TURN_FRAME_COUNT 5
#define MINI_GOLF_FACING_FRAME 2
extern AssetId g_MiniGolfDiscFrameAssets[MINI_GOLF_COURSES_PER_RECORD * MINI_GOLF_DISC_IMAGE_COUNT]
                                        [MINI_GOLF_TURN_FRAME_COUNT];
// Each mini-golf course's disc image as it faces the screen
extern AssetId g_MiniGolfCourseIconAssets[];
// The file read with each mini-golf course's disc image
extern SignedAssetId g_MiniGolfDiscFiles[];

// Each character's face image, which the menus draw faces from
extern ImageAsset *g_CharacterFaceImages[CHARACTER_COUNT];
// The palette of each face in g_CharacterFaceImages
extern PaletteColor *g_CharacterFacePalettes[CHARACTER_COUNT];

// The "R: COM" icon above the "L: LEFTY" icon of the character select
extern ImageAsset *g_ComputerLeftyIcons;

// Loaded images: the menus load their plain labels, or other images in their
// place, into the first two by turns, and the club panel uses the last two as
// its buffers
#define IMAGE_SLOT_COUNT 14
extern ImageAsset *g_ImageSlots[IMAGE_SLOT_COUNT];

// The character select's grid of slots
#define CHARACTER_GRID_ROW_COUNT 3
#define CHARACTER_GRID_COLUMN_COUNT 7
// The rows of the grid that hold the characters to choose from
#define CHARACTER_CHOICE_ROW_COUNT 2
// The character select row of the Game Boy characters, whose column is the
// character's index
#define GAME_BOY_CHARACTER_ROW 2

// The first empty slot gets a negative mark: the id of the character that
// slot holds in f_EveryCharacterGrid, less this offset. In Get Character,
// DrawMenuCharacters shows that character as the rival.
#define MARKED_CHARACTER_OFFSET 50
// A slot of the grid that holds no character
#define EMPTY_GRID_SLOT (-1)
// Slots marked with MARKED_CHARACTER_OFFSET hold values below this
#define MARKED_CHARACTER_LIMIT (-10)

extern s16 g_CharacterGrid[CHARACTER_GRID_ROW_COUNT][CHARACTER_GRID_COLUMN_COUNT];

// What FindCharacterOnGrid returns
enum GridSearchResult {
    CHARACTER_OFF_GRID = -1,
    CHARACTER_ON_GRID
};

typedef s32 GridSearchResult;
GridSearchResult FindCharacterOnGrid(CharacterIndex characterIndex);

// g_PlayerControllerIndices holds this for a player with no controller of their
// own. The computer plays them, and they never clash with the others.
#define NO_CONTROLLER_INDEX 4
// A player's controller before the player chooses
#define UNCHOSEN_CONTROLLER (-1)
// g_ImportedCharacterColumns's value for a player without an imported character
#define NO_IMPORTED_COLUMN (-1)
extern s8 g_PlayerControllerIndices[PLAYER_COUNT_MAX];
extern s8 g_ChoosingControllerIndices[PLAYER_COUNT_MAX];
extern s16 g_ImportedCharacterColumns[PLAYER_COUNT_MAX];

// The 1P to 4P, 1C to 4C and Com labels, 16 by 8 each
#define PLAYER_LABEL_WIDTH 16
#define PLAYER_LABEL_HEIGHT 8
#define PLAYER_LABEL_IMAGE_SIZE (PLAYER_LABEL_WIDTH * PLAYER_LABEL_HEIGHT / 2)
#define PLAYER_LABEL_FIRST_COMPUTER 4
#define PLAYER_LABEL_COMPUTER 8
// Where a player's label goes below their 32 by 32 controller icon
#define PLAYER_LABEL_OFFSET_X 8
#define PLAYER_LABEL_OFFSET_Y 25
#define CONTROLLER_ICON_SIZE 32
#define CONTROLLER_ICON_IMAGE_SIZE (CONTROLLER_ICON_SIZE * CONTROLLER_ICON_SIZE)

// The menus' character models
// Baby Mario stands higher to look as tall as the others
#define BABY_MARIO_RAISE 24
#define MENU_ANIMATION_STANDING 16
#define MENU_ANIMATION_FIDGET 17
// The most frames a character stands before it fidgets again
#define FIDGET_DELAY_RANGE 128
// A fidget delay while the fidget plays
#define FIDGETING (-1)
// The orthographic projection that the menus view their characters with, and
// the eye of the LookAt that guLookAtReflectF builds for them
#define MENU_CHARACTERS_NEAR 1.0f
#define MENU_CHARACTERS_FAR 3000.0f
#define MENU_CHARACTERS_ORTHO_SCALE 10.0f
#define MENU_CHARACTERS_EYE_Y 70.0f
#define MENU_CHARACTERS_EYE_Z 60.0f

// Indices of the game-mode icon animations in g_ModeIconAnimations
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
// The table holds three rows past the last icon, copies of rows 2, 2 and 5
#define MODE_ICON_ANIMATION_COUNT 14
// Each icon's frames are asset ids, ended by END_OF_ICON_FRAMES
#define MODE_ICON_FRAME_COUNT 27
#define END_OF_ICON_FRAMES (-1)

extern s16 g_ModeIconAnimations[MODE_ICON_ANIMATION_COUNT][MODE_ICON_FRAME_COUNT];

// The name plate image, drawn in strips
#define NAME_PLATE_WIDTH 200
#define NAME_PLATE_HEIGHT 30
#define NAME_PLATE_STRIP_HEIGHT 10
#define NAME_PLATE_STRIP_COUNT 3

// The course scene, which DrawMenuScenery draws from g_MenuCourseSceneryAsset
// at double size as the backdrop. Each strip loads a row either side of the
// rows it shows, so that the filtering blends across the seams.
#define COURSE_SCENE_WIDTH 152
#define COURSE_SCENE_STRIP_ROW_COUNT 11
#define COURSE_SCENE_STRIP_COUNT 11
#define COURSE_SCENE_SCALE 2
#define COURSE_SCENE_SEAM_ROW_COUNT 2
// Where the course scene is drawn
#define COURSE_SCENE_LEFT 8
#define COURSE_SCENE_FIRST_TOP 8
#define COURSE_SCENE_TOP (COURSE_SCENE_FIRST_TOP + COURSE_SCENE_SCALE)
// DrawSpinningStars draws a row of spinning stars, one per hole, from g_MenuArrowSizedImages, and
// enlarges the chosen hole's
#define STAR_WIDTH 32
#define STAR_HEIGHT 24
// The stars spin through this many images
#define STAR_FRAME_COUNT 15
// DrawScaledModeIcon draws the chosen mode's icon, 64x64 CI8, in two strips
#define MODE_ICON_WIDTH 64
#define MODE_ICON_STRIP_HEIGHT 32
#define MODE_ICON_STRIP_COUNT 2
#define MODE_ICON_FRAME_DURATION 4
// The rows of the modes' label image, 192 by 20 CI4 each
#define MODE_LABEL_WIDTH 192
#define MODE_LABEL_HEIGHT 20
#define MODE_LABEL_IMAGE_SIZE ((MODE_LABEL_WIDTH * MODE_LABEL_HEIGHT) / 2)
#define MODE_LABEL_X 160
#define SINGLE_PLAYER_MODE_LABEL_X 170
#define MODE_LABEL_TOP 156
// The course image, two strips of 64 by 32 eight-bit texels
#define COURSE_IMAGE_WIDTH 64
#define COURSE_IMAGE_STRIP_HEIGHT 32
#define COURSE_IMAGE_STRIP_COUNT 2
// The course numbers image holds the digits 1 to 6, then the word COURSE
#define COURSE_NUMBERS_WIDTH 136
#define COURSE_NUMBERS_HEIGHT 16
#define COURSE_DIGIT_WIDTH 16
#define COURSE_WORD_WIDTH 40
// DrawClubWindow adds to that window the Nintendo badge of f_NintendoBadge near its top, the course
// image of f_ClubCourseIcon at its lower left and the picture of f_MenuWindowImage, colored by the
// palette in f_MenuWindowPalette, at its lower right
#define NINTENDO_BADGE_WIDTH 144
#define NINTENDO_BADGE_STRIP_HEIGHT 10
#define NINTENDO_BADGE_STRIP_COUNT 2
#define FACE_DARKENED_SHADE 10
#define FACE_NAME_OFFSET_X 24
#define FACE_NAME_OFFSET_Y 12

// The wheel that DrawCourseStats and DrawBestTimes turn the open courses' 64 by
// 64 images on, seen from above, with the blinking arrows either side
#define WHEEL_FIELD_OF_VIEW 120.0f
#define WHEEL_NEAR_PLANE 10.0f
#define WHEEL_FAR_PLANE 30000.0f
#define WHEEL_EYE_HEIGHT 80.0f
#define WHEEL_EYE_DISTANCE 120.0f
#define WHEEL_RADIUS_X 104.0f
#define WHEEL_RADIUS_Z 40.0f
#define WHEEL_SLOT_LIMIT 8
// The images sit above the wheel's projected points, and each strip 12 pixels
// higher again
#define WHEEL_RAISE 32
#define WHEEL_ICON_RAISE 12
// The spins count steps of four degrees
#define DEGREES_PER_SPIN_STEP 4
#define COURSE_ICON_WIDTH 64
#define COURSE_ICON_STRIP_HEIGHT 32
#define COURSE_ICON_STRIP_COUNT 2
#define COURSE_ICON_HALF_SIZE 32.0f
#define WHEEL_ARROW_WIDTH 32
#define WHEEL_ARROW_X 160
#define WHEEL_ARROW_Y 94

Outcome SaveOrContinueGame(s32 slotIndex, Boolean isContinuing);
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
    ScoreToPar scoreToPar,
    CharacterIndex characterIndex,
    CButtonPalette paletteIndex
);
void InsertRankedRecord(PlayRecord *record);
Boolean IsDonkeyKongUnlocked(void);
Boolean IsMapleUnlocked(void);
Boolean IsMetalMarioUnlocked(void);
Boolean IsNintendoClubTournament(void);
void LoadMenuResources(void);
void FreeMenuResources(void);

// The cursor of the character table and the player stats: a quad whose corners
// cycle through the colors, masked by the 48x48 IA4 image of asset 0x6EF
#define MENU_CURSOR_SIZE 48
#define MENU_CURSOR_VERTEX_COUNT 4
#define MENU_CURSOR_PHASE_STEP (M_PI / 12)
#define MENU_CURSOR_COLOR_SWING 127.0f
#define MENU_CURSOR_COLOR_MIDDLE 127
#define MENU_CURSOR_NEAR 10.0f
#define MENU_CURSOR_FAR 2000.0f
#define MENU_CURSOR_Z (-100.0f)
#define MENU_CURSOR_SCALE_Y 0.87f

// The sets of bets that a game's mode and player count offer, as g_BetSet
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

// The bets that the chosen game offers
extern BetSet g_BetSet;
// The player count and screen the main menu last left by; a change of either
// resets the mode choice
extern s16 g_LastMenuPlayerCountChoice;
// The entered club tournament's code fields: whether Nintendo runs it, which
// club tournament name it has, and its number from 1
extern Boolean g_IsNintendoClubTournament;
extern s32 g_ClubTournamentNameIndex;
extern s32 g_ClubTournamentNumber;
// The code of the tournament whose password was entered
extern u8 g_TournamentCode[TOURNAMENT_CODE_SIZE];
extern MenuScreen16 g_LastMenuScreen;
// The menu screen's course or mode icons, whose buffers other screens reuse
// for their own images
extern ImageAsset *g_MenuIconImages[MENU_ICON_COUNT];
// g_MenuIconImages as the main menu uses them, in pairs of buffers
#define MENU_ICON_IMAGE_PAIRS ((void *(*)[MINI_GOLF_DISC_IMAGE_COUNT])g_MenuIconImages)
extern ImageAsset *g_MenuBoardImages[MENU_IMAGE_BUFFER_COUNT];
extern ImageAsset *g_MenuTitleImages[MENU_IMAGE_BUFFER_COUNT];
// More of the menus' image buffers, named for the image LoadMenuResources first reads into each.
// The screens that UpdateMenus switches to reuse them: g_MenuWideSpareImage holds a wide cloud or
// the save menu's third board, and g_CourseTrophyImages a chosen course's images.
extern ImageAsset *g_MenuWideSpareImage;
extern ImageAsset *g_SpinningBadgeImage;
extern ImageAsset *g_CourseTrophyImages[MENU_IMAGE_BUFFER_COUNT];
extern ImageAsset *g_MiniGolfDiscImages[MINI_GOLF_DISC_IMAGE_COUNT];
extern ImageAsset *g_HoleBadgeImages;
extern ImageAsset *g_BrickWallImage;

// Read by no routine
extern ImageAsset *g_ModeIconImages[MENU_ICON_COUNT];

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
Outcome SaveOrContinueGameByMode(Boolean isContinuing);
void SaveRingShotRecord(s32 recordIndex, RingShotRecord *record);
void SortIndicesDescending(s32 *values, s32 *order, s32 count);

#endif
