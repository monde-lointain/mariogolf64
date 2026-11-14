#include "main_menu.h"

#include <PR/libmus.h>
#include <stdlib.h>
#include <string.h>
#include <ultra64.h>

#define WIDENS_CHOSEN_OPTIONS
#define WIDENS_CHOSEN_BEST_SHOT

#include "address.h"
#include "asset.h"
#include "best_times.h"
#include "bets.h"
#include "camera.h"
#include "character.h"
#include "character_model.h"
#include "character_select.h"
#include "character_table.h"
#include "club.h"
#include "compiler.h"
#include "controller.h"
#include "course.h"
#include "course_menu.h"
#include "course_space.h"
#include "course_stats.h"
#include "cpu_projection.h"
#include "game.h"
#include "game_flags.h"
#include "game_state.h"
#include "gbi_helpers.h"
#include "geometry.h"
#include "help_board.h"
#include "hole_conditions.h"
#include "hole_loader.h"
#include "hole_setup.h"
#include "language.h"
#include "main.h"
#include "math_util.h"
#include "menu.h"
#include "menu_arrows.h"
#include "menu_memory.h"
#include "menu_number.h"
#include "menu_window.h"
#include "overlay_manager.h"
#include "password.h"
#include "play.h"
#include "player.h"
#include "player_stats.h"
#include "round_hud.h"
#include "round_stats.h"
#include "save.h"
#include "save_screens.h"
#include "scene.h"
#include "scoring.h"
#include "screen.h"
#include "screen_layers.h"
#include "sky_and_hud_animations.h"
#include "sound.h"
#include "sram.h"
#include "text.h"
#include "tournament_menu.h"
#include "transfer_pak.h"

#define MODE_CHOICE_LIMIT 12

#define MAIN_CHOICE_CLUBHOUSE 0
#define MAIN_CHOICE_HELP 1
#define MAIN_CHOICE_OPTIONS 2
#define MAIN_CHOICE_WITHOUT_TRANSMIT_COUNT 3
#define MAIN_CHOICE_CLUB_TOURNAMENT 2
#define MAIN_CHOICE_FULL_OPTIONS 3
#define MAIN_CHOICE_COUNT 4

#define CLUBHOUSE_CHOICE_COUNT 5

enum ClubhouseRoom {
    CLUBHOUSE_ROOM_COURSE_STATS,
    CLUBHOUSE_ROOM_BEST_SHOTS,
    CLUBHOUSE_ROOM_RING_SHOT,
    CLUBHOUSE_ROOM_BIRDIE_BADGES,
    CLUBHOUSE_ROOM_CHARACTER_TABLE,
    CLUBHOUSE_ROOM_BEST_TIMES
};

typedef s32 ClubhouseRoom;
typedef s16 ClubhouseRoom16;

#define OPTIONS_CHOICE_MUSIC 0
#define OPTIONS_CHOICE_MONO 1
#define OPTIONS_CHOICE_CAMERA 2
#define OPTIONS_CHOICE_ERASE_SAVE_DATA 3
#define OPTIONS_CHOICE_COUNT 4
#define OPTIONS_CHOICE_SONG 4
#define OPTIONS_CHOICE_SONG_REVERB 5
#define OPTIONS_CHOICE_SONG_FREQUENCY 6
#define OPTIONS_CHOICE_SONG_TEMPO 7
#define OPTIONS_CHOICE_SOUND 8
#define OPTIONS_CHOICE_SOUND_REVERB 9
#define OPTIONS_CHOICE_SOUND_FREQUENCY 10
#define OPTIONS_SOUND_TEST_CHOICE_COUNT 21

#define OPTIONS_ARROWS_OFFSET 0x3408
#define OPTIONS_SHADOW_OFFSET 0x3830
#define OPTION_VALUES_OFFSET 0x3228

#define COURSE_SELECT_WATER_OFFSET 0x1E88
#define COURSE_SELECT_LAYER_OFFSET 0x2E90

#define SOUND_TEST_SONG_NAME_LENGTH 12
#define SOUND_TEST_SOUND_NAME_LENGTH 24

#define CADDY_COUNT 2

#define MINI_GOLF_RECORD_COUNT 2

#define CHOSEN_LABEL_GROWTH_LIMIT 6
#define CHOSEN_LABEL_GROWTH_STEP 2

#define TITLE_LABEL_COUNT 7
#define TITLE_LABEL_WIDTH 128
#define TITLE_LABEL_HEIGHT 20
#define TITLE_LABEL_ROW_SIZE (TITLE_LABEL_WIDTH / 2)
#define TITLE_LABEL_IMAGE_SIZE (TITLE_LABEL_ROW_SIZE * TITLE_LABEL_HEIGHT)
#define STAR_COUNT_MAX 6
#define CHOSEN_STAR_GROWTH 8
#define MENU_CHARACTER_LAYOUT_COUNT 4
#define SOLO_CHARACTER_LAYOUT 0
#define MENU_CHARACTER_SLOT_COUNT 5
#define MENU_CHARACTER_SOLO_SCALE 5.0f
#define MENU_RIVAL_SCALE 4.0f
#define MENU_CHARACTER_PAIR_SCALE 4.0f
#define MENU_CHARACTER_FRONT_SCALE 3.5f
#define MENU_CHARACTER_BACK_SCALE 3.4f
#define MENU_CHARACTER_DEFAULT_SCALE 4.0f
#define WATER_VERTEX_COUNT 5
#define HOLES_BADGE_BET 0
#define HOLES_BADGE_COUNT 4
#define HOLES_BADGE_SIZE 24
#define HOLES_BADGE_IMAGE_SIZE ((HOLES_BADGE_SIZE * HOLES_BADGE_SIZE) / 2)
#define CLUBHOUSE_ROW_PLACE_COUNT 8

#define VIEWER_STARTING_ANIMATION 13
#define NO_VIEWER_CHARACTER (-1)
#define ALL_CLUB_SLOTS ((1 << CLUB_SLOT_COUNT) - 1)
#define MENU_IMAGE_BUFFER_SIZE 0x80A0

#define AmountPerMode(amount) \
    (f_ModeCounts[g_ChosenPlayerCount - 1] != 0 ? \
            (amount) / f_ModeCounts[g_ChosenPlayerCount - 1] : \
            (amount))

typedef struct ImageCorner {
    s16 x;
    s16 y;
} ImageCorner;

#define CHOSEN_HOLE_WIND_SPEED_MASK 0x3F
#define CHOSEN_HOLE_WIND_SPEED_STEP 6000
#define CHOSEN_HOLE_LIGHT_PITCH 0x1000
#define CHOSEN_HOLE_LIGHT_YAW 0x2000

#define TITLE_CHOICE_NEW_GAME 0
#define TITLE_CHOICE_SAVED_GAMES 1
#define TITLE_CHOICE_MENUS 2
#define TITLE_CHOICE_COUNT 3
#define TRANSMIT_BUTTONS (A_BUTTON | L_TRIG | R_TRIG)
#define TRANSMIT_OTHER_BUTTONS (A_BUTTON | Z_TRIG | R_TRIG)
#define TRANSMIT_SOUND_CHANNEL 14
#define TRANSMIT_WIPE_SHAPE 12

#define MENU_BOB_PHASE_STEP 0.13f
#define MENU_BOB_HEIGHT 5.0f

#define MENU_BANNER_TOP 18
#define UNUSED_BANNER_TOP 16

#define SMALL_BANNER_WIDTH 96

#define LARGE_BANNER_LEFT 26
#define LARGE_BANNER_TOP 174
#define LARGE_BANNER_WIDTH 270
#define LARGE_BANNER_STRIP_HEIGHT 20
#define LARGE_BANNER_STRIP_SPACING 16

#define NARROW_BOARD_WIDTH 160
#define NARROW_BOARD_STRIP_HEIGHT 8
#define NARROW_BOARD_STRIP_COUNT 20
#define NARROW_BOARD_STRIP_SPACING 7
#define NARROW_BOARD_ROW_SCALE 9
#define NARROW_BOARD_LEFT 15
#define NARROW_BOARD_TOP 36
#define NARROW_BOARD_SHOWN_WIDTH 170

#define COURSE_SCENE_SECOND_ROW_START TexelCoordinate(1)

#define NINE_HOLE_POWER_SHOT_COUNT 3
#define EIGHTEEN_HOLE_POWER_SHOT_COUNT 6

#define UNUSED_LIST_CHOICE_COUNT 21

#define MINI_GOLF_TURN_FRAME_DURATION 2
#define MINI_GOLF_TURN_NONE 0
#define MINI_GOLF_TURN_TO_PREVIOUS 1
#define MINI_GOLF_TURN_TO_NEXT 2
#define MINI_GOLF_RECORD_SLIDE 45
#define MINI_GOLF_RECORD_SLIDE_STEP 9

#define COURSE_SELECT_TURN 90
#define COURSE_SELECT_SPIN_SPEED 18
#define COURSE_SELECT_TEXT_SLOT_COUNT 4

void DrawModeSelect(Gfx **displayListHead);
void DrawAnimationViewer(Gfx **displayListHead);
void DrawCourseSelect(Gfx **displayListHead);
void DrawMainMenu(Gfx **displayListHead);
void DrawMainMenuWithoutTransmit(Gfx **displayListHead);
void DrawClubhouse(Gfx **displayListHead);
void DrawOptionsScreen(Gfx **displayListHead);
void DrawCaddySelect(Gfx **displayListHead);
void DrawBlankScreen(Gfx **displayListHead);
void DrawBirdieBadgeLabel(Gfx **displayListHead);
void OnMenuResourcesLoaded(void);
void SetUpChosenGame(void);
void UpdateMenuTitleScreen(void);
void UpdateAnimationViewer(void);
void UpdateModeSelect(void);
void UpdateCourseSelect(void);
void UpdateMiniGolfCourseSelect(void);
void UpdateMainMenu(void);
void UpdateMainMenuWithoutTransmit(void);
void UpdateClubhouse(void);
void UpdateOptionsScreen(void);
void UpdateCaddySelect(void);
void LeaveMenuWithWipe(void);
void LeaveMenuWithCourseWipe(void);
void UpdateBirdieBadges(void);
void UpdateBlankScreen(void);
void DrawNarrowMenuBoard(Gfx **displayListHead);
void DrawLargeMenuBanner(Gfx **displayListHead);
void DrawMenuTitleScreen(Gfx **displayListHead);

s32 g_MenuFidgetDelays[PLAYER_COUNT_MAX];
s32 g_CourseBannersSlide;

extern const char f_PeachsCastleName[];
extern const char f_LuigisGardenName[];

static Gfx f_UnusedEmptyDisplayList[] UNUSED = {gsSPEndDisplayList()};
Boolean g_IsHoleShownInMenuInset = FALSE;
static s32 f_MenuCharacterCount = CHARACTER_COUNT;
static u8 f_UnusedBytesAfterCharacterCount[8] UNUSED =
    {0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static s32 f_UnusedListChoice = 0;
static s32 f_StartingHoleSelection = 0;
static s32 f_UnusedWordAfterHoleSelection UNUSED = 0;
static AnimationId f_ViewerAnimation = VIEWER_STARTING_ANIMATION;
static Boolean f_IsViewerAdjusting = FALSE;
static Boolean f_IsChosenAnimationPlaying = FALSE;
static s32 f_ModeCounts[PLAYER_COUNT_MAX] = {7, 5, 4, 4};
static s32 f_UnusedModeCounts[4] UNUSED = {3, 5, 4, 4};
static u16 f_PlayerClubMasks[PLAYER_COUNT_MAX] =
    {ALL_CLUB_SLOTS, ALL_CLUB_SLOTS, ALL_CLUB_SLOTS, ALL_CLUB_SLOTS};
static s32 f_UnusedFlags[10] UNUSED = {0, 0, 0, 0, 0, 0, 0, 1, 1, 1};
static u8 f_ShownTrophyBufferIndex = 0;
static s8 f_ViewerModelKind = 0;
static s32 f_BetSlideOffset = 0;
UBetPagesPhase8 g_BetPagesPhase = MENU_PHASE_START;
u8 g_BetPagesWallSlide = STATS_SLIDE_CURRENT;
static s16 f_UnusedSpinningStarCounter = 0;
static s16 f_MenuTitleImageIndex = 0;
static s16 f_MenuLabelImageIndex = 0;
static s16 f_ImageSlotIndex = 0;
static s16 f_MenuBoardImageIndex = 0;
static UBoolean8 f_IsViewerModelKindChanged = FALSE;
static UBoolean8 f_IsEraseWindowUp = FALSE;
static f32 f_MenuBobOffset = 0.0f;
static f32 f_MenuBobPhase = 0.0f;
static SoundId f_SoundTestSound = 0;
static SongId f_SoundTestSong = 0;
static s32 f_SoundTestSoundReverb = 0;
static s32 f_SoundTestSongReverb = 0;
static f32 f_SoundTestSoundFrequency = 0.0f;
static f32 f_SoundTestSongFrequency = 0.0f;
static s32 f_SoundTestSongTempo = NORMAL_SONG_TEMPO;
static u8 f_ModeChoiceIcons[PLAYER_COUNT_MAX][MODE_CHOICE_LIMIT] = {
    {
        MODE_ICON_TOURNAMENT,
        MODE_ICON_GET_CHARACTER,
        MODE_ICON_RING_SHOT,
        MODE_ICON_SPEED_GOLF,
        MODE_ICON_STROKE,
        MODE_ICON_MINI_GOLF,
        MODE_ICON_TRAINING,
    },
    {
        MODE_ICON_STROKE,
        MODE_ICON_MATCH_GAME,
        MODE_ICON_SKINS_MATCH,
        MODE_ICON_CLUB_SLOTS,
        MODE_ICON_MINI_GOLF,
    },
    {MODE_ICON_STROKE, MODE_ICON_SKINS_MATCH, MODE_ICON_CLUB_SLOTS, MODE_ICON_MINI_GOLF},
    {MODE_ICON_STROKE, MODE_ICON_SKINS_MATCH, MODE_ICON_CLUB_SLOTS, MODE_ICON_MINI_GOLF},
};
static u8 f_UnusedModeChoiceIcons[4][12] UNUSED =
    {{2, 8, 3}, {2, 6, 7, 10, 5}, {2, 7, 10, 5}, {2, 7, 10, 5}};
static ClubhouseRoom16 f_ClubhouseRooms[CLUBHOUSE_CHOICE_COUNT] = {
    CLUBHOUSE_ROOM_COURSE_STATS,
    CLUBHOUSE_ROOM_CHARACTER_TABLE,
    CLUBHOUSE_ROOM_BEST_TIMES,
    CLUBHOUSE_ROOM_RING_SHOT,
    CLUBHOUSE_ROOM_BEST_SHOTS,
};
static ClubhouseRoom16 f_UnusedBirdieBadgesRoom UNUSED = CLUBHOUSE_ROOM_BIRDIE_BADGES;
static const char *f_PlainMenuNames[] =
    {"1P GAME", "2P GAME", "3P GAME", "4P GAME", "CONTINUE", "HOUISE"};
static const char *f_UnusedModeNames[] UNUSED = {
    "TOURNAMENT ",
    "VS CHARACTER",
    "STROKE PLAY",
    "MATCH PLAY",
    "SKINS",
    "TIME ATTACK",
    "TRAINING",
    "PUTTER GOLF",
    " HANDI CAP",
    " BET",
    "9HOLE MODE  ",
    "CLUB HANDI",
};
char g_MenuCharacterNames[CHARACTER_COUNT][IMPORTED_CHARACTER_NAME_LENGTH] = {
    "P\xEC\xF5\xED",
    "C\xE8\xE1\xF2\xEC\xE9\xE5",
    "P\xE5\xE1\xE3\xE8",
    "B\xE1\xE2\xF9 \xED\xE1\xF2\xE9\xEF",
    "L\xF5\xE9\xE7\xE9",
    "Y\xEF\xF3\xE8\xE9",
    "S\xEF\xEE\xEE\xF9",
    "W\xE1\xF2\xE9\xEF",
    "H\xE1\xF2\xF2\xF9",
    "M\xE1\xF2\xE9\xEF",
    "M\xE1\xF0\xEC\xE5",
    "DK",
    "B\xEF\xF7\xF3\xE5\xF2",
    "M\xE5\xF4\xE1\xEC \xED\xE1\xF2\xE9\xEF",
    "K\xE9\xE4",
    "S\xE8\xE5\xF2\xF2\xF9",
    "A\xFA\xE1\xEC\xE5\xE1",
    "J\xEF\xE5",
};
static const char *f_UnusedOptionNames[] UNUSED = {
    "TOURNAMENT",
    "MEMORIAL HOLES",
    "SPECIAL SHOT",
    "BIRDIE BADGE",
    "VS CHARACTER",
    "TIME ATTACK",
    "BGM",
    "SOUND",
    "DATA RESET",
    "BGM TEST",
    "SOUND TEST",
};
static const char *f_UnusedGameBoyNames[] UNUSED = {"GB 00 ", "GB 01 ", "GB 02 ", "GB 03 "};
static const char *f_UnusedSideSpinNames[] UNUSED = {"STRAIGHT", "DRAW", "FADE"};
static const char *f_UnusedBetNames[] UNUSED =
    {"DRIVE CON", "NEAR PIN", "STROKE", "SKINS", "NASSO", "LASVEGAS"};
static const char *f_TestCourseNames[] UNUSED = {
    "TEST COURSE1",
    "TEST COURSE2",
    "TEST COURSE3",
    "TEST COURSE4",
    "TEST COURSE5",
    "TEST COURSE6",
    "TEST COURSE7",
    "TEST COURSE8",
    "TEST COURSE9",
    "TEST COURSE10",
};
static const char *f_CaddyNames[CADDY_COUNT] = {"MARIO", "PEACH"};
static const char *f_UnusedClubNames[] UNUSED =
    {"1W", "3W", "4W", "2I", "3I", "4I", "5I", "6I", "7I", "8I", "9I", "PW", "SW", "PT"};
static char f_SoundTestSongNames[][SOUND_TEST_SONG_NAME_LENGTH] = {
    "jing1",
    "jing02",
    "jing03",
    "jing04",
    "jing05",
    "jing06",
    "shori",
    "make",
    "hikiwa",
    "mode-ok",
    "score",
    "coace3",
    "trad1",
    "trad2",
    "par",
    "bar",
    "dan1",
    "bgm04",
    "bgm05",
    "bgm06",
    "coace5",
    "training",
    "gbdata",
    "coace9",
    "coace8",
    "coace7",
    "coace6",
    "coace10",
    "coace4",
    "pop1",
    "pop2",
    "replay",
    "trad3",
    "trad4",
    "pop3",
    "pop4",
    "pop5",
    "pop6",
    "time",
    "64theme",
    "nangoku",
    "mariobgm",
    "mario20",
    "clav",
    "patar1",
    "patar2",
    "skins",
    "openning",
    "bgm20",
    "ending",
    "end-jingle",
};
static char f_SoundTestSoundNames[][SOUND_TEST_SOUND_NAME_LENGTH] = {
    "AE01 ",
    "AE05 ",
    "BL01 ",
    "BL02 ",
    "BL03 ",
    "BL04 ",
    "BL05 ",
    "BL06 ",
    "BL07 ",
    "BL08 ",
    "BL09 ",
    "BL10 ",
    "BL11 ",
    "BL14 ",
    "BL15 ",
    "BL16 ",
    "BL17 ",
    "BL18 ",
    "BL19 ",
    "BL20 ",
    "BL22 ",
    "BL23 ",
    "BR01MOZU ",
    "BR02KAKKO ",
    "BR03KANARIA ",
    "BR04MEJIRO ",
    "BR05SUZUME ",
    "BR06SUZUME02 ",
    "BR07HIBARI ",
    "BR07HIBARI1 ",
    "BR09KAWASEMI ",
    "DR01 ",
    "DR03 ",
    "DR04 ",
    "DR05 ",
    "DR06 ",
    "DR07 ",
    "DR10 ",
    "DR11 ",
    "DR12 ",
    "DR13 ",
    "DR14 ",
    "DR15 ",
    "DR40 ",
    "DR41 ",
    "DR42 ",
    "DR50 ",
    "DR53 ",
    "DR54 ",
    "DR55 ",
    "DR56 ",
    "DR57 ",
    "DR58 ",
    "DR59 ",
    "DR60 ",
    "DR61 ",
    "GP01 ",
    "GP02 ",
    "GP03 ",
    "GP04 ",
    "GP05 ",
    "GP06 ",
    "GP07 ",
    "GP08 ",
    "GP09 ",
    "GP10 ",
    "GP11 ",
    "SS01 ",
    "SS02 ",
    "SS03 ",
    "SS04 ",
    "SS05 ",
    "SS06 ",
    "SS07 ",
    "SS08 ",
    "SS09 ",
    "SS10 ",
    "SS12 ",
    "SS13 ",
    "SS14 ",
    "SW01 ",
    "SW02 ",
    "SW03 ",
    "SW04 ",
    "SW05 ",
    "SW06 ",
    "SW07 ",
    "SW08 ",
    "SW10 ",
    "SW20 ",
    "SW21 ",
    "SY01 ",
    "SY02 ",
    "SY03 ",
    "SY04 ",
    "SY05 ",
    "SY06 ",
    "SY07 ",
    "SY08 ",
    "SY09 ",
    "SY10 ",
    "SY11 ",
    "SY12 ",
    "SY13 ",
    "SY14 ",
    "SY15 ",
    "SY17 ",
    "SY18 ",
    "SY19 ",
    "SY20 ",
    "SY21 ",
    "SY22 ",
    "USA20",
    "USA21",
    "VO01 ",
    "VO02 ",
    "VO03 ",
    "VO04 ",
    "VO05 ",
    "VO10 ",
    "VO11 ",
    "VO12 ",
    "VO13 ",
    "NR01 ",
    "NR02 ",
    "NR03 ",
    "NR04 ",
    "NR05 ",
    "NR06 ",
    "NR07 ",
    "NR08 ",
    "NR09 ",
    "NR10 ",
    "NR11 ",
    "NR12 ",
    "NR13 ",
    "NR14 ",
    "NR15 ",
    "NR16 ",
    "NR17 ",
    "NR18 ",
    "NR20 ",
    "NR21 ",
    "NR26 ",
    "NR27 ",
    "NR30 ",
    "NR31 ",
    "NR32 ",
    "NR37 ",
    "NR38 ",
    "NR39 ",
    "NR40 ",
    "NR41 ",
    "NR42 ",
    "NR43 ",
    "NR44 ",
    "NR45 ",
    "NR46 ",
    "CAZ00 ",
    "CAZ01 ",
    "CAZ02 ",
    "CAZ03 ",
    "CAZ04 ",
    "CAZ05 ",
    "CAZ06 ",
    "CAZ07 ",
    "CAZ08 ",
    "CAZ09 ",
    "CAZ10 ",
    "CAZ12 ",
    "CAZ13 ",
    "CAZ14 ",
    "CAZ15 ",
    "CAZ16 ",
    "CAZ17 ",
    "CAZ18 ",
    "CAZ20 ",
    "CAZ21 ",
    "CAZ22 ",
    "CBB00 ",
    "CBB01 ",
    "CBB02 ",
    "CBB03 ",
    "CBB04 ",
    "CBB05 ",
    "CBB06 ",
    "CBB07 ",
    "CBB08 ",
    "CBB09 ",
    "CBB10 ",
    "CBB12 ",
    "CBB13 ",
    "CBB14 ",
    "CBB15 ",
    "CBB16 ",
    "CBB17 ",
    "CBB18 ",
    "CBB20 ",
    "CBB22 ",
    "CBB23 ",
    "CBB27 ",
    "CBB30 ",
    "CBB31 ",
    "CBB32 ",
    "CCH00 ",
    "CCH01 ",
    "CCH02 ",
    "CCH03 ",
    "CCH04 ",
    "CCH05 ",
    "CCH06 ",
    "CCH07 ",
    "CCH08 ",
    "CCH09 ",
    "CCH10 ",
    "CCH12 ",
    "CCH13 ",
    "CCH14 ",
    "CCH15 ",
    "CCH16 ",
    "CCH17 ",
    "CCH18 ",
    "CCH19 ",
    "CCH20 ",
    "CCH21 ",
    "CCH24 ",
    "CDK00 ",
    "CDK01 ",
    "CDK02 ",
    "CDK03 ",
    "CDK04 ",
    "CDK05 ",
    "CDK06 ",
    "CDK08 ",
    "CDK10 ",
    "CDK13 ",
    "CDK18 ",
    "CDK20 ",
    "CDK21 ",
    "CHR00 ",
    "CHR01 ",
    "CHR02 ",
    "CHR03 ",
    "CHR04 ",
    "CHR05 ",
    "CHR06 ",
    "CHR07 ",
    "CHR08 ",
    "CHR09 ",
    "CHR10 ",
    "CHR12 ",
    "CHR13 ",
    "CHR14 ",
    "CHR15 ",
    "CHR16 ",
    "CHR17 ",
    "CHR18 ",
    "CHR19 ",
    "CHR20 ",
    "CHR21 ",
    "CHR22 ",
    "CHR25 ",
    "CJO00 ",
    "CJO01 ",
    "CJO02 ",
    "CJO03 ",
    "CJO04 ",
    "CJO05 ",
    "CJO06 ",
    "CJO07 ",
    "CJO08 ",
    "CJO09 ",
    "CJO10 ",
    "CJO12 ",
    "CJO13 ",
    "CJO14 ",
    "CJO15 ",
    "CJO16 ",
    "CJO17 ",
    "CJO18 ",
    "CJO20 ",
    "CJO21 ",
    "CJO22 ",
    "CJO24 ",
    "CKD00 ",
    "CKD02 ",
    "CKD03 ",
    "CKD04 ",
    "CKD06 ",
    "CKD07 ",
    "CKD08 ",
    "CKD09 ",
    "CKD10 ",
    "CKD12 ",
    "CKD13 ",
    "CKD14 ",
    "CKD15 ",
    "CKD16 ",
    "CKD17 ",
    "CKD18 ",
    "CKD20 ",
    "CKD21 ",
    "CKD25 ",
    "CKP00 ",
    "CKP01 ",
    "CKP02 ",
    "CKP03 ",
    "CKP04 ",
    "CKP05 ",
    "CKP06 ",
    "CKP08 ",
    "CKP09 ",
    "CKP10 ",
    "CKP13 ",
    "CKP18 ",
    "CKP20 ",
    "CLG00 ",
    "CLG01 ",
    "CLG02 ",
    "CLG03 ",
    "CLG05 ",
    "CLG06 ",
    "CLG07 ",
    "CLG08 ",
    "CLG09 ",
    "CLG10 ",
    "CLG12 ",
    "CLG13 ",
    "CLG14 ",
    "CLG15 ",
    "CLG17 ",
    "CLG18 ",
    "CLG19 ",
    "CLG20 ",
    "CLG21 ",
    "CLG23 ",
    "CLG24 ",
    "CLG30 ",
    "CLG31 ",
    "CME00 ",
    "CME01 ",
    "CME02 ",
    "CME03 ",
    "CME04 ",
    "CME05 ",
    "CME06 ",
    "CME07 ",
    "CME08 ",
    "CME09 ",
    "CME10 ",
    "CME12 ",
    "CME13 ",
    "CME14 ",
    "CME15 ",
    "CME16 ",
    "CME17 ",
    "CME18 ",
    "CME19 ",
    "CME20 ",
    "CME21 ",
    "CME22 ",
    "CME25 ",
    "CMR00 ",
    "CMR01 ",
    "CMR02 ",
    "CMR03 ",
    "CMR04 ",
    "CMR05 ",
    "CMR06 ",
    "CMR07 ",
    "CMR08",
    "CMR09",
    "CMR10",
    "CMR11",
    "CMR12",
    "CMR13",
    "CMR14",
    "CMR15",
    "CMR16",
    "CMR17",
    "CMR18",
    "CMR19",
    "CMR20",
    "CMR21",
    "CMR22",
    "CMR23",
    "CMR26",
    "CMR30",
    "CMR31",
    "CMR32 ",
    "CMR33 ",
    "CMR34 ",
    "CMR35 ",
    "CMR36 ",
    "CMR37 ",
    "CPC00 ",
    "CPC01 ",
    "CPC02 ",
    "CPC03 ",
    "CPC04 ",
    "CPC05 ",
    "CPC06 ",
    "CPC07 ",
    "CPC08 ",
    "CPC09 ",
    "CPC10 ",
    "CPC12 ",
    "CPC13 ",
    "CPC14 ",
    "CPC15 ",
    "CPC16 ",
    "CPC17 ",
    "CPC18 ",
    "CPC19 ",
    "CPC20 ",
    "CPC21 ",
    "CPC24 ",
    "CPC30 ",
    "CPC31 ",
    "CPR00 ",
    "CPR01 ",
    "CPR02 ",
    "CPR03 ",
    "CPR04 ",
    "CPR05 ",
    "CPR06 ",
    "CPR07 ",
    "CPR08 ",
    "CPR09 ",
    "CPR10 ",
    "CPR12 ",
    "CPR13 ",
    "CPR14 ",
    "CPR15 ",
    "CPR16 ",
    "CPR17 ",
    "CPR18 ",
    "CPR19 ",
    "CPR20 ",
    "CPR21 ",
    "CPR24 ",
    "CPR25 ",
    "CPR30 ",
    "CSN00 ",
    "CSN01 ",
    "CSN02 ",
    "CSN03 ",
    "CSN04 ",
    "CSN05 ",
    "CSN06 ",
    "CSN07 ",
    "CSN08 ",
    "CSN09 ",
    "CSN10 ",
    "CSN12 ",
    "CSN13 ",
    "CSN14 ",
    "CSN15 ",
    "CSN16 ",
    "CSN17 ",
    "CSN18 ",
    "CSN19 ",
    "CSN20 ",
    "CSN21 ",
    "CSN24 ",
    "CSN25 ",
    "CSN26 ",
    "CSN27 ",
    "CSY00 ",
    "CSY01 ",
    "CSY02 ",
    "CSY03 ",
    "CSY04 ",
    "CSY05 ",
    "CSY06 ",
    "CSY07 ",
    "CSY08 ",
    "CSY09 ",
    "CSY10 ",
    "CSY12 ",
    "CSY13 ",
    "CSY14 ",
    "CSY15 ",
    "CSY16 ",
    "CSY17 ",
    "CSY18 ",
    "CSY19 ",
    "CSY20 ",
    "CSY21 ",
    "CSY26 ",
    "CWR00 ",
    "CWR01 ",
    "CWR02 ",
    "CWR03 ",
    "CWR05 ",
    "CWR06 ",
    "CWR07 ",
    "CWR08 ",
    "CWR09 ",
    "CWR10 ",
    "CWR11 ",
    "CWR12 ",
    "CWR13 ",
    "CWR14 ",
    "CWR15 ",
    "CWR16 ",
    "CWR17 ",
    "CWR18 ",
    "CWR19 ",
    "CWR20 ",
    "CWR23 ",
    "CWR24 ",
    "CYS00 ",
    "CYS01 ",
    "CYS02 ",
    "CYS03 ",
    "CYS04 ",
    "CYS05 ",
    "CYS06 ",
    "CYS07 ",
    "CYS08 ",
    "CYS13 ",
    "CYS14 ",
    "CYS17 ",
    "CYS18 ",
    "CYS19 ",
    "CYS20 ",
    "CYS21 ",
    "TITLE ",
    "USATITLE",
    "USATITLE01",
    "USATITLE02",
    "WARAI3",
    "NONE_SE",
    "NONE_SE",
    "NONE_SE",
    "NONE_SE",
    "NONE_SE",
    "NONE_SE",
    "NONE_SE",
    "NONE_SE",
    "NONE_SE",
    "NONE_SE",
    "NONE_SE",
    "NONE_SE",
    "NONE_SE",
    "NONE_SE",
    "NONE_SE",
    "NONE_SE",
    "NONE_SE",
    "NONE_SE",
    "NONE_SE",
    "NONE_SE",
    "NONE_SE",
    "NONE_SE",
    "NONE_SE",
    "NONE_SE",
    "NONE_SE",
};
static Vtx f_UnusedQuadVertices[8] UNUSED = {
    {{{-32, 32, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{-32, 0, 0}, 0, {0, 2048}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{32, 0, 0}, 0, {4096, 2048}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{32, 32, 0}, 0, {4096, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{-32, 0, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{-32, -32, 0}, 0, {0, 2048}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{32, -32, 0}, 0, {4096, 2048}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{32, 0, 0}, 0, {4096, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
};
static WideAssetId f_StarFrameAssets[STAR_FRAME_COUNT] = {
    0x05BE,
    0x05BF,
    0x05C0,
    0x05C1,
    0x05C2,
    0x05C3,
    0x05C4,
    0x05C5,
    0x05C6,
    0x05C7,
    0x05C8,
    0x05C9,
    0x05CA,
    0x05CB,
    0x05CC,
};
static WideAssetId f_MenuEmptyStarFrameAssets[STAR_FRAME_COUNT] = {
    0x05E5,
    0x05E6,
    0x05E7,
    0x05E8,
    0x05E9,
    0x05EA,
    0x05EB,
    0x05EC,
    0x05ED,
    0x05EE,
    0x05EF,
    0x05F0,
    0x05F1,
    0x05F2,
    0x05F3,
};
s32 g_AnimatedIconFrame = 0;
u8 g_AnimatedIconBufferIndex = 0;
s8 g_AnimatedIconFrameTime = 0;
static s32 f_SpinningIconFrame = 0;
static u8 f_SpinningStarBufferIndex = 0;
static s8 f_SpinningIconFrameTime = 0;
static u8 f_WavingToadImageIndex = 0;
static s32 f_UnusedWordAfterToadImage UNUSED = 0;
static s32 f_MiniGolfDiscFrame = MINI_GOLF_FACING_FRAME;
static s32 f_MiniGolfDiscFrameTime = 0;
static u8 f_MiniGolfDiscTurn = MINI_GOLF_TURN_NONE;
static u8 f_MiniGolfDiscBufferIndex = 0;
static s32 f_CameraOrbitUpdateCount = 0;
static u8 f_PlayerSoundPans[PLAYER_COUNT_MAX][PLAYER_COUNT_MAX] =
    {{127, 0, 0, 0}, {15, 240, 0, 0}, {15, 127, 240, 0}, {15, 85, 170, 240}};
static s32 f_CharacterChoices[CHARACTER_COUNT] = {
    CHARACTER_CHOICE_PLUM,
    CHARACTER_CHOICE_CHARLIE,
    CHARACTER_CHOICE_MAPLE,
    CHARACTER_CHOICE_BABY_MARIO,
    CHARACTER_CHOICE_LUIGI,
    CHARACTER_CHOICE_PEACH,
    CHARACTER_CHOICE_SONNY,
    CHARACTER_CHOICE_WARIO,
    CHARACTER_CHOICE_HARRY,
    CHARACTER_CHOICE_MARIO,
    CHARACTER_CHOICE_YOSHI,
    CHARACTER_CHOICE_DONKEY_KONG,
    CHARACTER_CHOICE_BOWSER,
    CHARACTER_CHOICE_METAL_MARIO,
    CHARACTER_CHOICE_KID,
    CHARACTER_CHOICE_SHERRY,
    CHARACTER_CHOICE_AZALEA,
    CHARACTER_CHOICE_JOE,
};
static s32 f_UnusedWordAfterCharacterChoices UNUSED = 0;
static ScreenCorner f_TitleLabelCorners[TITLE_LABEL_COUNT] =
    {{110, 86}, {40, 114}, {90, 114}, {147, 114}, {217, 114}, {96, 142}, {96, 170}};
static f32 f_ModeBoardBobPhase = 0.0f;
static s32 f_ModeSelectControllerIconPositions[PLAYER_COUNT_MAX][PLAYER_COUNT_MAX]
                                              [PLANE_AXIS_COUNT] = {
                                                  {{42, 140}, {76, 120}, {206, 120}, {248, 140}},
                                                  {{42, 140}, {248, 140}, {206, 120}, {248, 140}},
                                                  {{42, 140}, {76, 120}, {211, 120}, {248, 140}},
                                                  {{42, 140}, {76, 120}, {211, 120}, {248, 140}},
};
static s32 f_UnusedModeIconValues[11] UNUSED = {110, 109, 117, 97, 106, 105, 105, 97, 84, 124, 99};
static s32 f_ModeSelectModeLabelOffsets[MODE_ICON_COUNT] =
    {85, 85, 41, 60, 55, 60, 75, 66, 59, 0, 61};
static Degrees f_CharacterPitch = 0.0f;
static Degrees f_CharacterYaw = 0.0f;
static Degrees f_RivalSpinYaw = 0.0f;
static s32 f_UnusedWordsAfterRivalSpin[2] UNUSED = {0, 0};
static Vec3 f_MenuCharacterPositions[MENU_CHARACTER_LAYOUT_COUNT][MENU_CHARACTER_SLOT_COUNT] = {
    {
        {-84.0f, -43.0f, -1000.0f},
        {106.0f, -32.0f, -1000.0f},
        {64.0f, 0.0f, -1000.0f},
        {64.0f, 0.0f, -1000.0f},
        {0.0f, 0.0f, 0.0f},
    },
    {
        {-106.0f, -32.0f, -1000.0f},
        {106.0f, -32.0f, -1000.0f},
        {-106.0f, -52.0f, -1000.0f},
        {106.0f, -52.0f, -1000.0f},
        {0.0f, 0.0f, 0.0f},
    },
    {
        {-106.0f, -52.0f, -1000.0f},
        {-76.0f, -8.0f, -1700.0f},
        {76.0f, -8.0f, -1700.0f},
        {106.0f, -52.0f, -1000.0f},
        {0.0f, 0.0f, 0.0f},
    },
    {
        {-106.0f, -52.0f, -1000.0f},
        {-76.0f, -8.0f, -1700.0f},
        {76.0f, -8.0f, -1700.0f},
        {106.0f, -52.0f, -1000.0f},
        {0.0f, 0.0f, 0.0f},
    },
};
static s32 f_MarkedCharacters[CHARACTER_COUNT] =
    {0, 1, 5, 3, 4, 10, 6, 7, 8, 9, 2, 11, 12, 13, 14, 15, 16, 17};
static CharacterIndex f_ViewerLoadedCharacter = NO_VIEWER_CHARACTER;
static Boolean f_IsViewerAnimationChanged = FALSE;
static Degrees f_ViewerPitch = 0.0f;
static Degrees f_ViewerYaw = 0.0f;
static f32 f_ViewerScaleOffset = 0.0f;
static u32 f_ViewerCharacterSlot = 0;
static f32 f_ViewerOffsetX = 0.0f;
static f32 f_ViewerOffsetY = 0.0f;
static f32 f_FrontCourseBobPhase = 0.0f;
static s16 f_UnusedCourseOrder[6] UNUSED = {0, 3, 4, 1, 5, 2};
static char f_UnusedHolesChoiceNames[4][20] UNUSED =
    {"ﾍﾞｯﾄ", "ｱｳﾄ : 1-9ﾎｰﾙﾌﾟﾚｲ", "ｲﾝ : 10-18ﾎｰﾙﾌﾟﾚｲ", "1ﾗｳﾝﾄﾞ : 18ﾎｰﾙﾌﾟﾚｲ"};
static ImageCorner f_HolesBadgeCorners[HOLES_BADGE_COUNT] =
    {{252, 134}, {230, 150}, {274, 150}, {252, 162}};
static ImageCorner f_UnusedTrophyCorners[6] UNUSED =
    {{24, 173}, {23, 173}, {23, 173}, {29, 161}, {19, 181}, {29, 171}};
static ImageCorner f_TrophyCorners[COURSE_COUNT][TOURNAMENT_PLACE_COUNT][TOURNAMENT_PLACE_COUNT] = {
    {
        {{24, 169}, {24, 173}, {24, 173}},
        {{16, 169}, {32, 169}, {24, 173}},
        {{16, 169}, {32, 169}, {24, 173}},
    },
    {
        {{23, 169}, {23, 173}, {23, 173}},
        {{15, 169}, {31, 169}, {23, 173}},
        {{15, 169}, {31, 169}, {23, 173}},
    },
    {
        {{23, 169}, {23, 173}, {23, 173}},
        {{15, 169}, {31, 169}, {23, 173}},
        {{15, 169}, {31, 169}, {23, 173}},
    },
    {
        {{29, 157}, {29, 161}, {29, 161}},
        {{21, 157}, {37, 157}, {29, 161}},
        {{21, 157}, {37, 157}, {29, 161}},
    },
    {
        {{19, 177}, {19, 181}, {19, 181}},
        {{11, 177}, {27, 177}, {19, 181}},
        {{11, 177}, {27, 177}, {19, 181}},
    },
    {
        {{29, 167}, {29, 171}, {29, 171}},
        {{21, 167}, {37, 167}, {29, 171}},
        {{21, 167}, {37, 167}, {29, 171}},
    },
};
static char f_UnusedCourseGetText[16] UNUSED = "ｺｰｽｹﾞｯﾄ純ﾞ";
/* clang-format off */
static u8 f_UnusedRemainingText[16] UNUSED = {
    0x71, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
/* clang-format on */
static f32 f_BetBadgeColorPhase = 0.0f;
static f32 f_MiniGolfChoiceCenterY = 0.0f;
static Vtx f_WaterVertices[WATER_VERTEX_COUNT] = {
    {{{-800, 64, -1024}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{-800, -128, 0}, 0, {0, 25600}, {0x7F, 0x7F, 0x7F, 0xFF}}},
    {{{0, -128, 0}, 0, {16384, 25600}, {0x7F, 0x7F, 0x7F, 0xFF}}},
    {{{800, -128, 0}, 0, {32736, 25600}, {0x7F, 0x7F, 0x7F, 0xFF}}},
    {{{800, 64, -1024}, 0, {32736, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
};
static s32 f_WaterScroll = 0;
static char f_UnusedHolesChoiceNamesCopy[4][20] UNUSED =
    {"ﾍﾞｯﾄ", "ｱｳﾄ : 1-9ﾎｰﾙﾌﾟﾚｲ", "ｲﾝ : 10-18ﾎｰﾙﾌﾟﾚｲ", "1ﾗｳﾝﾄﾞ : 18ﾎｰﾙﾌﾟﾚｲ"};
static const char *f_MiniGolfRecordNames[MINI_GOLF_RECORD_COUNT] = {
    f_LuigisGardenName,
    f_PeachsCastleName,
};
static ScreenCorner f_MainChoiceLabelCorners[MAIN_CHOICE_COUNT] =
    {{96, 84}, {96, 114}, {96, 144}, {96, 174}};
static ScreenCorner f_ShortMainChoiceLabelCorners[MAIN_CHOICE_WITHOUT_TRANSMIT_COUNT] =
    {{96, 84}, {96, 124}, {96, 164}};
static ScreenCorner f_ClubhouseLabelCorners[CLUBHOUSE_ROW_PLACE_COUNT] =
    {{76, 80}, {76, 102}, {76, 124}, {76, 146}, {76, 168}, {76, 190}, {76, 212}, {76, 234}};
static s32 f_UnusedLabelPositions[10][2] UNUSED = {
    {48, 76},
    {132, 76},
    {180, 76},
    {48, 98},
    {72, 118},
    {168, 118},
    {48, 140},
    {60, 160},
    {176, 160},
    {90, 182},
};
/* clang-format off */
static u8 f_UnusedEraseAllDataPrompt[3][20] UNUSED = {
    {
        0xC3, 0xDE, 0xB0, 0xC0, 0x66, 0x7D, 0x8D, 0xDE, 0x83, 0x79,
        0x7C, 0x83, 0x72, 0x72, 0x83, 0xDE, 0x7D, 0x76, 0x3F, 0x00,
    },
    {
        0x8A, 0x72, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    },
    {
        0x72, 0x72, 0x74, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    },
};
/* clang-format on */
static s32 f_UnusedWordBeforeArrowTimer UNUSED = 0;
static s32 f_OptionsArrowTimer = 0;

static u8 f_UnusedBufferBeforeMatrices[0x1D0] UNUSED;
static Mat4 f_ModeSelectProjection;
static Mat4 f_ModeSelectView;
static LookAt f_ModeSelectLookAts[DRAW_BUFFER_COUNT];
static Mtx f_MenuCharacterTransforms[PLAYER_COUNT_MAX][DRAW_BUFFER_COUNT];
static Mtx f_MenuCharacterProjection;
static f32 f_MenuCharacterScales[PLAYER_COUNT_MAX];
static Mtx f_ViewerTransforms[DRAW_BUFFER_COUNT];
static Mtx f_ViewerProjection;
static u8 f_UnusedBufferBeforeWater[0x180] UNUSED;
static Mtx f_WaterTranslation;
static Mtx f_WaterProjection;
static Mat4 f_CourseSelectProjection;
static Mat4 f_CourseSelectView;
static s32 f_BetSettingsSlide;
static s32 f_HandicapsSlide;
static u8 f_MenuImageBuffer[MENU_IMAGE_BUFFER_SIZE];

void *GetMenuImageBuffer(void) {
    return f_MenuImageBuffer;
}

void StartLoadingChosenHole(void) {
    u32 slotIndex;

    GetSaveData();
    GetMenuMemory();
    for (slotIndex = 0; slotIndex != CHARACTER_SLOT_COUNT; slotIndex++) {
        if (IsCharacterSlotBusy(slotIndex)) {
            FreeCharacterSlot(slotIndex);
        }
    }

    g_IsChosenHoleLoadStarted = TRUE;
    if (g_MenuScreen == MENU_SCREEN_RING_SHOT_EXIT ||
        g_MenuScreen == MENU_SCREEN_RING_SHOT_RECORDS) {
        g_GameState.courseIndex = g_MenuMemory.courseIndex;
        g_GameState.holeIndex = g_MenuMemory.holeIndex;
    }
    else if (g_MenuScreen == MENU_SCREEN_CLUB_TOURNAMENT) {
        g_GameState.courseIndex = GetClubTournamentCourse();
        g_GameState.isMiniGolf = g_GameState.courseIndex >= MINI_GOLF_COURSE_INDEX;
        g_GameState.holeIndex = 0;
    }
    else if (g_GameState.gameMode == GAME_MODE_TRAINING) {
        g_GameState.courseIndex = 0;
        g_GameState.holeIndex = 0;
    }
    else {
        if (g_GameState.isMiniGolf) {
            g_GameState.courseIndex = g_MenuMemory.miniGolfCourseChoice + MINI_GOLF_COURSE_INDEX +
                (g_MenuMemory.miniGolfRecordChoice * MINI_GOLF_COURSES_PER_RECORD);
        }
        else {
            g_GameState.courseIndex = g_MenuMemory.courseChoice;
        }

        g_GameState.holeIndex = GetFirstHoleIndex();
    }

    g_GameState.unusedHoleValue = 0;
    g_HoleSetup.conditions.isRaining = FALSE;
    g_HoleSetup.conditions.windSpeed =
        (rand() & CHOSEN_HOLE_WIND_SPEED_MASK) * CHOSEN_HOLE_WIND_SPEED_STEP;
    g_HoleSetup.conditions.windDirection = rand();
    g_HoleSetup.conditions.sceneColorIndex = SCENE_COLOR_INDEX_DRIVING_RANGE;
    g_HoleSetup.conditions.lightPitch = CHOSEN_HOLE_LIGHT_PITCH;
    g_HoleSetup.conditions.lightYaw = CHOSEN_HOLE_LIGHT_YAW;
    SetGameFlag(GAME_FLAG_HOLE_LOADED);
    g_GameState.playerCount = 1;
    StopLoaderThread();
    StartLoaderThread();
}

s32 GetChosenCourseIndex(void) {
    if (g_GameState.isMiniGolf) {
        return g_MenuMemory.miniGolfCourseChoice + MINI_GOLF_COURSE_INDEX +
            (g_MenuMemory.miniGolfRecordChoice * MINI_GOLF_COURSES_PER_RECORD);
    }

    return g_MenuMemory.courseChoice;
}

#define ORBIT_ANGLE_PER_UPDATE 0x100
#define ORBIT_RADIUS 3000.0f
#define ORBIT_EYE_OFFSET_Y (-3000.0f)
#define MIDPOINT_TO_WORLD_SCALE (0.5f / WORLD_TO_COURSE_SCALE)

#define ASSET_TITLE_LABELS 0x52B
#define ASSET_TITLE_LABELS_DIM 0x52C
#define ASSET_MAIN_LABELS 0x523
#define ASSET_MAIN_LABELS_DIM 0x524
#define ASSET_CLUBHOUSE_LABELS 0x525
#define ASSET_CLUBHOUSE_LABELS_DIM 0x526
#define ASSET_OPTIONS_LABELS 0x529
#define ASSET_OPTIONS_LABELS_DIM 0x52A
#define ASSET_OPTION_VALUES 0x97C
#define ASSET_SQUARE_BOARD 0x591
#define ASSET_OPTIONS_MARIO 0x97D
#define ASSET_OPTIONS_SHADOW 0x97E
#define ASSET_SKY 0x51F
#define ASSET_WATER 0x4D8

void UpdateMenus(void) {
    AssetFile file;
    Vec3 center;
    Vec3 eye;
    EncodedAsset asset;
    SaveData *save;
    s32 index;
    u8 *imageBuffer;
    ImageAsset *tournamentImage;
    u8 *optionBits;
    u8 iconIndex;

    save = GetSaveData();
    GetMenuMemory();
    g_MenuRepeatButtons = 0;
    g_MenuPressedButtons = 0;
    for (index = 0; index != g_ControllerCount; index++) {
        g_MenuRepeatButtons |= g_MenuControllerData[index].button;
        g_MenuPressedButtons |= g_MenuControllerData[index].trigger;
    }

    if (g_MenuScreen == MENU_SCREEN_COURSE_SELECT) {
        if ((GetChosenCourseIndex() != g_GameState.courseIndex) || !g_IsChosenHoleLoadStarted) {
            StartLoadingChosenHole();
        }
    }
    else if (g_MenuScreen == MENU_SCREEN_RING_SHOT_EXIT) {
        if ((g_GameState.holeIndex != g_MenuMemory.holeIndex) ||
            (g_GameState.courseIndex != g_MenuMemory.courseIndex) ||
            !g_IsChosenHoleLoadStarted) {
            StartLoadingChosenHole();
        }
    }

    f_CameraOrbitUpdateCount++;
    center[AXIS_X] = (g_HoleSetup.cupPosition[AXIS_X] + g_HoleSetup.teePosition[AXIS_X]) *
        MIDPOINT_TO_WORLD_SCALE;
    center[AXIS_Y] = (g_HoleSetup.cupPosition[AXIS_Y] + g_HoleSetup.teePosition[AXIS_Y]) *
        MIDPOINT_TO_WORLD_SCALE;
    center[AXIS_Z] = (g_HoleSetup.cupPosition[AXIS_Z] + g_HoleSetup.teePosition[AXIS_Z]) *
        MIDPOINT_TO_WORLD_SCALE;
    eye[AXIS_X] = (cosf(
                       ((f_CameraOrbitUpdateCount * ORBIT_ANGLE_PER_UPDATE) & BINARY_ANGLE_MASK) *
                       BINARY_ANGLE_TO_RADIANS
                   ) * ORBIT_RADIUS) +
        center[AXIS_X];
    eye[AXIS_Y] = center[AXIS_Y] + ORBIT_EYE_OFFSET_Y;
    eye[AXIS_Z] = (sinf(
                       ((f_CameraOrbitUpdateCount * ORBIT_ANGLE_PER_UPDATE) & BINARY_ANGLE_MASK) *
                       BINARY_ANGLE_TO_RADIANS
                   ) * ORBIT_RADIUS) +
        center[AXIS_Z];
    g_Cameras[INSET_CAMERA].view.eye[AXIS_X] = eye[AXIS_X];
    g_Cameras[INSET_CAMERA].view.eye[AXIS_Y] = eye[AXIS_Y];
    g_Cameras[INSET_CAMERA].view.eye[AXIS_Z] = eye[AXIS_Z];
    g_Cameras[INSET_CAMERA].view.lookAt[AXIS_X] = center[AXIS_X];
    g_Cameras[INSET_CAMERA].view.lookAt[AXIS_Y] = center[AXIS_Y];
    g_Cameras[INSET_CAMERA].view.lookAt[AXIS_Z] = center[AXIS_Z];
    SetScreenEffect(SCREEN_EFFECT_NONE, 0);
    g_IsHoleShownInMenuInset = FALSE;
    g_Cameras[MAIN_CAMERA].view.drawFlags = CAMERA_DRAW_SCENE;
    g_Cameras[INSET_CAMERA].view.drawFlags = 0;
    g_InsetView.animation = INSET_VIEW_OPENING;
    switch (g_MenuScreen) {
        case MENU_SCREEN_TITLE:
            memset(&g_GameState, -1, sizeof(GameState));
            g_GameState.isClubTournament = FALSE;
            UpdateMenuTitleScreen();
            break;

        case MENU_SCREEN_MODE_SELECT:
            UpdateModeSelect();
            break;

        case MENU_SCREEN_CHARACTER_SELECT:
            UpdateCharacterSelect(0, g_ChosenPlayerCount, FALSE);
            break;

        case MENU_SCREEN_TOURNAMENT_CHARACTER_SELECT:
            UpdateCharacterSelect(0, 1, FALSE);
            break;

        case MENU_SCREEN_OPPONENT_SELECT:
            UpdateCharacterSelect(1, 2, TRUE);
            break;

        case MENU_SCREEN_SAVED_GAMES:
            UpdateSavedGames();
            break;

        case MENU_SCREEN_MAIN:
            UpdateMainMenu();
            break;

        case MENU_SCREEN_MAIN_WITHOUT_TRANSMIT:
            UpdateMainMenuWithoutTransmit();
            break;

        case MENU_SCREEN_CLUB_TOURNAMENT:
            UpdateClubTournament();
            break;

        case MENU_SCREEN_CLUBHOUSE:
            UpdateClubhouse();
            break;

        case MENU_SCREEN_OPTIONS:
            UpdateOptionsScreen();
            break;

        case MENU_SCREEN_HELP:
            if (IsGameFlagSet(GAME_FLAG_HELP_ANIMATION_VIEWER)) {
                UpdateAnimationViewer();
            }
            else {
                UpdateHelpBoard();
            }

            break;

        case MENU_SCREEN_COURSE_SELECT:
            if (!g_GameState.isMiniGolf) {
                UpdateCourseSelect();
            }
            else {
                UpdateMiniGolfCourseSelect();
            }

            break;

        case MENU_SCREEN_CADDY_SELECT:
            UpdateCaddySelect();
            break;

        case MENU_SCREEN_HANDICAPS:
            UpdateHandicaps(1, g_ChosenPlayerCount);
            break;

        case MENU_SCREEN_BET_SETTINGS:
            UpdateBetSettings();
            break;

        case MENU_SCREEN_BLANK:
            UpdateBlankScreen();
            break;

        case MENU_SCREEN_BEST_SHOTS:
            UpdateBestShots();
            break;

        case MENU_SCREEN_RING_SHOT_RECORDS:
            UpdateRingShotRecords();
            break;

        case MENU_SCREEN_COURSE_STATS:
            UpdateCourseStats();
            break;

        case MENU_SCREEN_BEST_TIMES:
            UpdateBestTimes();
            break;

        case MENU_SCREEN_BIRDIE_BADGES:
            UpdateBirdieBadges();
            break;

        case MENU_SCREEN_CHARACTER_TABLE:
            UpdateCharacterTable();
            break;

        case MENU_SCREEN_RING_SHOT_EXIT:
            LeaveMenuWithCourseWipe();
            break;

        case MENU_SCREEN_TRAINING_EXIT:
            LeaveMenuWithWipe();
            break;
    }

    if (g_LoadedMenuScreen != g_MenuScreen) {
        g_LoadedMenuScreen = g_MenuScreen;
        if (g_MenuScreen < MENU_SCREEN_COUNT) {
            f_MenuTitleImageIndex ^= 1;
            OpenEncodedAsset(g_MenuScreenImageAssets[g_MenuScreen], &asset);
            ReadEncodedAsset(g_MenuTitleImages[f_MenuTitleImageIndex], &asset);
        }

        switch (g_MenuScreen) {
            case MENU_SCREEN_OPTIONS:
                f_MenuLabelImageIndex ^= 1;
                f_ImageSlotIndex ^= 1;
                OpenEncodedAsset(ASSET_OPTIONS_LABELS, &asset);
                ReadEncodedAsset(g_MenuLabelImages[f_MenuLabelImageIndex], &asset);
                OpenEncodedAsset(ASSET_OPTIONS_LABELS_DIM, &asset);
                ReadEncodedAsset(g_ImageSlots[f_ImageSlotIndex], &asset);
                OpenEncodedAsset(ASSET_OPTION_VALUES, &asset);
                ReadEncodedAsset(
                    ImageAt(g_MenuLabelImages[f_MenuLabelImageIndex], OPTION_VALUES_OFFSET),
                    &asset
                );
                f_MenuBoardImageIndex ^= 1;
                OpenEncodedAsset(ASSET_SQUARE_BOARD, &asset);
                ReadEncodedAsset(g_MenuBoardImages[f_MenuBoardImageIndex], &asset);
                LoadTournamentBuffer(g_MenuWideSpareImage, TOURNAMENT_BUFFER_WIDE_CLOUD);
                LoadTournamentBuffer(g_MenuSpareImages[0], TOURNAMENT_BUFFER_YES_NO);
                imageBuffer = GetMenuImageBuffer();
                OpenEncodedAsset(ASSET_OPTIONS_MARIO, &asset);
                ReadEncodedAsset(imageBuffer, &asset);
                OpenEncodedAsset(ASSET_GREEN_ARROWS, &asset);
                ReadEncodedAsset(imageBuffer + OPTIONS_ARROWS_OFFSET, &asset);
                OpenEncodedAsset(ASSET_OPTIONS_SHADOW, &asset);
                ReadEncodedAsset(imageBuffer + OPTIONS_SHADOW_OFFSET, &asset);
                InitSram();
                TransferSaveData(save, FALSE);
                optionBits = &g_MenuMemory.optionBits;
                *optionBits = 0;
                if (IsMusicOffChosen()) {
                    *optionBits |= OPTION_BIT_MUSIC_OFF;
                }

                if (IsMonoChosen()) {
                    *optionBits |= OPTION_BIT_MONO;
                }

                if (IsSimpleCameraChosen()) {
                    *optionBits |= OPTION_BIT_SIMPLE_CAMERA;
                }

                break;

            case MENU_SCREEN_TITLE:
                f_MenuLabelImageIndex ^= 1;
                f_ImageSlotIndex ^= 1;
                OpenEncodedAsset(ASSET_TITLE_LABELS, &asset);
                ReadEncodedAsset(g_MenuLabelImages[f_MenuLabelImageIndex], &asset);
                OpenEncodedAsset(ASSET_TITLE_LABELS_DIM, &asset);
                ReadEncodedAsset(g_ImageSlots[f_ImageSlotIndex], &asset);
                OpenEncodedAsset(ASSET_COURSE_SCENE, &asset);
                ReadEncodedAsset(g_MenuCourseSceneImage, &asset);
                f_MenuBoardImageIndex ^= 1;
                OpenEncodedAsset(ASSET_WIDE_BOARD, &asset);
                ReadEncodedAsset(g_MenuBoardImages[f_MenuBoardImageIndex], &asset);
                g_ImportedCharacterColumns[0] = NO_IMPORTED_COLUMN;
                g_ImportedCharacterColumns[1] = NO_IMPORTED_COLUMN;
                g_ImportedCharacterColumns[2] = NO_IMPORTED_COLUMN;
                g_ImportedCharacterColumns[3] = NO_IMPORTED_COLUMN;
                ClearImportedCharacters();
                break;

            case MENU_SCREEN_BLANK:
                f_MenuLabelImageIndex ^= 1;
                f_ImageSlotIndex ^= 1;
                OpenEncodedAsset(ASSET_TITLE_LABELS, &asset);
                ReadEncodedAsset(g_MenuLabelImages[f_MenuLabelImageIndex], &asset);
                OpenEncodedAsset(ASSET_TITLE_LABELS_DIM, &asset);
                ReadEncodedAsset(g_ImageSlots[f_ImageSlotIndex], &asset);
                OpenEncodedAsset(ASSET_COURSE_SCENE, &asset);
                ReadEncodedAsset(g_MenuCourseSceneImage, &asset);
                f_MenuBoardImageIndex ^= 1;
                OpenEncodedAsset(ASSET_WIDE_BOARD, &asset);
                ReadEncodedAsset(g_MenuBoardImages[f_MenuBoardImageIndex], &asset);
                break;

            case MENU_SCREEN_MODE_SELECT:
                for (index = 0; index != ARRAY_COUNT(g_ModeIconImages); index++) {
                    OpenEncodedAsset(
                        g_PlayerCountIconAssets[g_ChosenPlayerCount - 1][index],
                        &asset
                    );
                    ReadEncodedAsset(g_ModeIconImages[index], &asset);
                }

                OpenEncodedAsset(ASSET_CONTROLLER_ICONS, &asset);
                ReadEncodedAsset(g_SecondAnimatedIconImages[0], &asset);
                OpenEncodedAsset(ASSET_PLAYER_LABELS, &asset);
                ReadEncodedAsset(g_SecondAnimatedIconImages[1], &asset);
                f_ImageSlotIndex ^= 1;
                OpenEncodedAsset(ASSET_MENU_GAME_MODES, &asset);
                ReadEncodedAsset(g_ImageSlots[f_ImageSlotIndex], &asset);
                OpenEncodedAsset(ASSET_COURSE_SCENE, &asset);
                ReadEncodedAsset(g_MenuCourseSceneImage, &asset);
                f_MenuBoardImageIndex ^= 1;
                OpenEncodedAsset(ASSET_SQUARE_BOARD, &asset);
                ReadEncodedAsset(g_MenuBoardImages[f_MenuBoardImageIndex], &asset);
                iconIndex = f_ModeChoiceIcons[g_ChosenPlayerCount - 1][g_MenuMemory.modeChoice];
                g_AnimatedIconFrame = 0;
                g_AnimatedIconBufferIndex = 0;
                g_AnimatedIconFrameTime = 0;
                OpenEncodedAsset(g_ModeIconAnimations[iconIndex][0], &asset);
                ReadEncodedAsset(g_AnimatedIconImages[g_AnimatedIconBufferIndex], &asset);
                LoadPlayerStatsBuffer(g_MenuSpareImages[0], PLAYER_STATS_BUFFER_DOUBLE_PAGE_ARROWS);
                g_MenuFidgetDelays[0] = guRandom() % FIDGET_DELAY_RANGE;
                g_MenuFidgetDelays[1] = guRandom() % FIDGET_DELAY_RANGE;
                g_MenuFidgetDelays[2] = guRandom() % FIDGET_DELAY_RANGE;
                g_MenuFidgetDelays[3] = guRandom() % FIDGET_DELAY_RANGE;
                for (index = 0; index != MAXCONTROLLERS; index++) {
                    g_IsMenuCharacterLoaded[index] = FALSE;
                }

                OpenEncodedAsset(ASSET_SPINNING_BADGE, &asset);
                ReadEncodedAsset(g_SpinningBadgeImage, &asset);
                LoadCourseMenuBuffer(g_MenuSpareImages[1], COURSE_MENU_BUFFER_LARGE_DIGITS);
                f_SpinningIconFrame = 0;
                f_SpinningStarBufferIndex = 0;
                f_SpinningIconFrameTime = 0;
                OpenEncodedAsset(f_StarFrameAssets[g_AnimatedIconFrame], &asset);
                ReadEncodedAsset(g_MenuArrowSizedImages[0], &asset);
                break;

            case MENU_SCREEN_CHARACTER_SELECT:
            case MENU_SCREEN_TOURNAMENT_CHARACTER_SELECT:
                BuildCharacterGrid(g_ChosenPlayerCount, FALSE);
                f_MenuBoardImageIndex ^= 1;
                LoadCharacterSelectBuffer(
                    g_MenuBoardImages[f_MenuBoardImageIndex],
                    CHARACTER_SELECT_BUFFER_BOARD
                );
                f_MenuLabelImageIndex ^= 1;
                f_ImageSlotIndex ^= 1;
                LoadCharacterSelectBuffer(
                    g_MenuLabelImages[f_MenuLabelImageIndex],
                    CHARACTER_SELECT_BUFFER_PLAYER_LABELS
                );
                LoadCharacterSelectBuffer(
                    g_ImageSlots[f_ImageSlotIndex],
                    CHARACTER_SELECT_BUFFER_CURSOR_BOX
                );
                LoadCharacterSelectBuffer(g_NamePlateImage, CHARACTER_SELECT_BUFFER_UNREAD);
                LoadCharacterSelectBuffer(
                    g_MenuSpareImages[0],
                    CHARACTER_SELECT_BUFFER_CURVE_CELLS
                );
                LoadCharacterFaces();
                break;

            case MENU_SCREEN_OPPONENT_SELECT:
                BuildCharacterGrid(1, FALSE);
                HideGameBoyCharacters();
                OpenEncodedAsset(ASSET_COURSE_SCENE, &asset);
                ReadEncodedAsset(g_MenuCourseSceneImage, &asset);
                f_MenuBoardImageIndex ^= 1;
                LoadCharacterSelectBuffer(
                    g_MenuBoardImages[f_MenuBoardImageIndex],
                    CHARACTER_SELECT_BUFFER_BOARD
                );
                f_MenuLabelImageIndex ^= 1;
                f_ImageSlotIndex ^= 1;
                LoadCharacterSelectBuffer(
                    g_MenuLabelImages[f_MenuLabelImageIndex],
                    CHARACTER_SELECT_BUFFER_PLAYER_LABELS
                );
                LoadCharacterSelectBuffer(
                    g_ImageSlots[f_ImageSlotIndex],
                    CHARACTER_SELECT_BUFFER_CURSOR_BOX
                );
                LoadCharacterSelectBuffer(g_NamePlateImage, CHARACTER_SELECT_BUFFER_UNREAD);
                LoadCharacterSelectBuffer(
                    g_MenuSpareImages[0],
                    CHARACTER_SELECT_BUFFER_CURVE_CELLS
                );
                LoadCharacterFaces();
                PlaceRivalCursor();
                break;

            case MENU_SCREEN_SAVED_GAMES:
                f_MenuLabelImageIndex ^= 1;
                f_ImageSlotIndex ^= 1;
                OpenEncodedAsset(ASSET_TITLE_LABELS_DIM, &asset);
                ReadEncodedAsset(g_ImageSlots[f_ImageSlotIndex], &asset);
                OpenEncodedAsset(ASSET_COURSE_SCENE, &asset);
                ReadEncodedAsset(g_MenuCourseSceneImage, &asset);
                f_MenuBoardImageIndex ^= 1;
                LoadPlayerStatsBuffer(
                    g_MenuBoardImages[f_MenuBoardImageIndex],
                    PLAYER_STATS_BUFFER_TRIPLE_PAGE_ARROWS
                );
                g_AnimatedIconFrame = 0;
                g_AnimatedIconBufferIndex = 0;
                g_AnimatedIconFrameTime = 0;
                ReadSavedGames();
                LoadSaveMenuBuffers(MENU_ICON_IMAGE_PAIRS[0], SAVE_MENU_BUFFER_SLOTS);
                LoadSaveMenuBuffers(g_AnimatedIconImages, SAVE_MENU_BUFFER_CHOSEN);
                LoadGameModeLabelBuffer(g_NamePlateImage, GAME_MODE_LABEL_BUFFER_NAME_PLATE);
                LoadGameModeLabelBuffer(
                    g_ImageSlots[f_ImageSlotIndex],
                    GAME_MODE_LABEL_BUFFER_LABELS
                );
                LoadSaveGameBuffer(
                    g_SecondAnimatedIconImages[0],
                    SAVE_GAME_BUFFER_CONTROLLER_ICONS
                );
                LoadSaveGameBuffer(g_SecondAnimatedIconImages[1], SAVE_GAME_BUFFER_PLAYER_LABELS);
                LoadSaveGameBuffer(g_MenuSpareImages[0], SAVE_GAME_BUFFER_FIRST_BOARD);
                LoadSaveGameBuffer(g_MenuSpareImages[1], SAVE_GAME_BUFFER_SECOND_BOARD);
                LoadSaveGameBuffer(g_MenuWideSpareImage, SAVE_GAME_BUFFER_THIRD_BOARD);
                LoadSaveGameBuffer(
                    g_MenuLabelImages[f_MenuLabelImageIndex],
                    SAVE_GAME_BUFFER_COURSE
                );
                LoadSaveGameBuffer(g_CourseTrophyImages[0], SAVE_GAME_BUFFER_CHOSEN_COURSE);
                g_MenuFidgetDelays[0] = guRandom() % FIDGET_DELAY_RANGE;
                g_MenuFidgetDelays[1] = guRandom() % FIDGET_DELAY_RANGE;
                g_MenuFidgetDelays[2] = guRandom() % FIDGET_DELAY_RANGE;
                g_MenuFidgetDelays[3] = guRandom() % FIDGET_DELAY_RANGE;
                for (index = 0; index != MAXCONTROLLERS; index++) {
                    g_IsMenuCharacterLoaded[index] = FALSE;
                }

                break;

            case MENU_SCREEN_MAIN:
                f_MenuLabelImageIndex ^= 1;
                f_ImageSlotIndex ^= 1;
                OpenEncodedAsset(ASSET_MAIN_LABELS, &asset);
                ReadEncodedAsset(g_MenuLabelImages[f_MenuLabelImageIndex], &asset);
                OpenEncodedAsset(ASSET_MAIN_LABELS_DIM, &asset);
                ReadEncodedAsset(g_ImageSlots[f_ImageSlotIndex], &asset);
                f_MenuBoardImageIndex ^= 1;
                OpenEncodedAsset(ASSET_WIDE_BOARD, &asset);
                ReadEncodedAsset(g_MenuBoardImages[f_MenuBoardImageIndex], &asset);
                break;

            case MENU_SCREEN_MAIN_WITHOUT_TRANSMIT:
                f_MenuLabelImageIndex ^= 1;
                f_ImageSlotIndex ^= 1;
                OpenEncodedAsset(ASSET_MAIN_LABELS, &asset);
                ReadEncodedAsset(g_MenuLabelImages[f_MenuLabelImageIndex], &asset);
                OpenEncodedAsset(ASSET_MAIN_LABELS_DIM, &asset);
                ReadEncodedAsset(g_ImageSlots[f_ImageSlotIndex], &asset);
                f_MenuBoardImageIndex ^= 1;
                OpenEncodedAsset(ASSET_WIDE_BOARD, &asset);
                ReadEncodedAsset(g_MenuBoardImages[f_MenuBoardImageIndex], &asset);
                break;

            case MENU_SCREEN_CLUB_TOURNAMENT:
                f_MenuBoardImageIndex ^= 1;
                f_MenuLabelImageIndex ^= 1;
                tournamentImage = GetMenuImageBuffer();
                OpenEncodedAsset(ASSET_PLACE_DIGITS, &asset);
                ReadEncodedAsset(tournamentImage, &asset);
                LoadCodeEntryBuffer(g_NamePlateImage, CODE_ENTRY_BUFFER_NAME_PLATE);
                LoadCodeEntryBuffer(
                    g_MenuBoardImages[f_MenuBoardImageIndex],
                    CODE_ENTRY_BUFFER_BAR
                );
                LoadCodeEntryBuffer(
                    g_MenuLabelImages[f_MenuLabelImageIndex],
                    CODE_ENTRY_BUFFER_GLYPHS
                );
                LoadCodeEntryBuffer(g_MenuSpareImages[0], CODE_ENTRY_BUFFER_POINTING_HAND);
                LoadCodeEntryBuffer(g_MenuSpareImages[1], CODE_ENTRY_BUFFER_CURSOR_BOX);
                LoadTournamentBuffer(g_MenuWideSpareImage, TOURNAMENT_BUFFER_WIDE_CLOUD);
                LoadTournamentBuffer(MENU_ICON_IMAGE_PAIRS[0][0], TOURNAMENT_BUFFER_YES_NO);
                LoadTournamentBuffer(MENU_ICON_IMAGE_PAIRS[0][1], TOURNAMENT_BUFFER_NINTENDO_BADGE);
                LoadTournamentBuffer(MENU_ICON_IMAGE_PAIRS[1][0], TOURNAMENT_BUFFER_ICON);
                LoadTournamentBuffer(MENU_ICON_IMAGE_PAIRS[1][1], TOURNAMENT_BUFFER_MENU_WINDOW);
                LoadTournamentBuffer(MENU_ICON_IMAGE_PAIRS[2][0], TOURNAMENT_BUFFER_ROW_FILE);
                break;

            case MENU_SCREEN_HELP:
                if (IsGameFlagSet(GAME_FLAG_HELP_ANIMATION_VIEWER)) {
                    f_MenuLabelImageIndex ^= 1;
                    f_ImageSlotIndex ^= 1;
                    OpenEncodedAsset(ASSET_TITLE_LABELS, &asset);
                    ReadEncodedAsset(g_MenuLabelImages[f_MenuLabelImageIndex], &asset);
                    OpenEncodedAsset(ASSET_TITLE_LABELS_DIM, &asset);
                    ReadEncodedAsset(g_ImageSlots[f_ImageSlotIndex], &asset);
                    OpenEncodedAsset(ASSET_COURSE_SCENE, &asset);
                    ReadEncodedAsset(g_MenuCourseSceneImage, &asset);
                    f_MenuBoardImageIndex ^= 1;
                    OpenEncodedAsset(ASSET_WIDE_BOARD, &asset);
                    ReadEncodedAsset(g_MenuBoardImages[f_MenuBoardImageIndex], &asset);
                    g_AnimatedIconFrame = 0;
                    g_AnimatedIconBufferIndex = 0;
                    g_AnimatedIconFrameTime = 0;
                    f_IsViewerModelKindChanged = FALSE;
                }
                else {
                    LoadHelpBoardImage(MENU_ICON_IMAGE_PAIRS[0][0], HELP_BOARD_IMAGE_HAND);
                    LoadHelpBoardImage(g_MenuArrowSizedImages[1], HELP_BOARD_IMAGE_ARROWS);
                    f_MenuBoardImageIndex ^= 1;
                    LoadHelpBoardImage(
                        g_MenuBoardImages[f_MenuBoardImageIndex],
                        HELP_BOARD_IMAGE_CHALKBOARD
                    );
                    f_WavingToadImageIndex = 0;
                }

                break;

            case MENU_SCREEN_CLUBHOUSE:
                f_MenuLabelImageIndex ^= 1;
                f_ImageSlotIndex ^= 1;
                OpenEncodedAsset(ASSET_CLUBHOUSE_LABELS, &asset);
                ReadEncodedAsset(g_MenuLabelImages[f_MenuLabelImageIndex], &asset);
                OpenEncodedAsset(ASSET_CLUBHOUSE_LABELS_DIM, &asset);
                ReadEncodedAsset(g_ImageSlots[f_ImageSlotIndex], &asset);
                f_MenuBoardImageIndex ^= 1;
                OpenEncodedAsset(ASSET_WIDE_BOARD, &asset);
                ReadEncodedAsset(g_MenuBoardImages[f_MenuBoardImageIndex], &asset);
                break;

            case MENU_SCREEN_COURSE_SELECT:
                if (g_GameState.isMiniGolf) {
                    for (index = 0; index != MINI_GOLF_DISC_IMAGE_COUNT; index++) {
                        OpenEncodedAsset(
                            g_MiniGolfDiscFrameAssets
                                [(g_MenuMemory.miniGolfCourseChoice * MINI_GOLF_DISC_IMAGE_COUNT) +
                                    index][MINI_GOLF_FACING_FRAME],
                            &asset
                        );
                        ReadEncodedAsset(MENU_ICON_IMAGE_PAIRS[0][index], &asset);
                        OpenEncodedAsset(
                            g_MiniGolfDiscFrameAssets
                                [(g_MenuMemory.miniGolfCourseChoice * MINI_GOLF_DISC_IMAGE_COUNT) +
                                    index][MINI_GOLF_FACING_FRAME],
                            &asset
                        );
                        ReadEncodedAsset(MENU_ICON_IMAGE_PAIRS[1][index], &asset);
                    }

                    for (index = 0; index != ARRAY_COUNT(g_MiniGolfDiscImages); index++) {
                        OpenAssetFile(
                            g_MiniGolfDiscFiles[(g_MenuMemory.miniGolfCourseChoice * 2) + index],
                            &file
                        );
                        ReadAssetFile(g_MiniGolfDiscImages[index], GetAssetFileSize(&file), &file);
                    }
                }
                else {
                    for (index = 0; index != COURSE_COUNT; index++) {
                        OpenEncodedAsset(g_CourseIconAssets[index], &asset);
                        ReadEncodedAsset(MENU_ICON_IMAGE_PAIRS[0][index], &asset);
                    }
                }

                OpenEncodedAsset(ASSET_SKY, &asset);
                ReadEncodedAsset(g_MenuCourseSceneImage, &asset);
                OpenEncodedAsset(ASSET_WATER, &asset);
                ReadEncodedAsset(
                    ImageAt(g_MenuCourseSceneImage, COURSE_SELECT_WATER_OFFSET),
                    &asset
                );
                OpenEncodedAsset(ASSET_BLENDED_TEXTURE_SCROLLING_LAYER, &asset);
                ReadEncodedAsset(
                    ImageAt(g_MenuCourseSceneImage, COURSE_SELECT_LAYER_OFFSET),
                    &asset
                );
                f_ShownTrophyBufferIndex = 0;
                OpenEncodedAsset(g_TrophyFrameAssets[g_MenuMemory.courseChoice][0], &asset);
                ReadEncodedAsset(g_TrophyFrameBuffers[0], &asset);
                OpenEncodedAsset(g_TrophyFrameAssets[g_MenuMemory.courseChoice][0], &asset);
                ReadEncodedAsset(g_TrophyFrameBuffers[1], &asset);
                for (index = 0; index != TROPHY_PALETTE_COUNT; index++) {
                    OpenAssetFile(g_TrophyPaletteFiles[g_MenuMemory.courseChoice][index], &file);
                    ReadAssetFile(g_TrophyPalettes[index], GetAssetFileSize(&file), &file);
                }

                OpenEncodedAsset(ASSET_HOLES_BADGES, &asset);
                ReadEncodedAsset(g_HoleBadgeImages, &asset);
                OpenEncodedAsset(ASSET_SPINNING_BADGE, &asset);
                ReadEncodedAsset(g_SpinningBadgeImage, &asset);
                f_ImageSlotIndex ^= 1;
                LoadCourseMenuBuffer(
                    g_ImageSlots[f_ImageSlotIndex],
                    COURSE_MENU_BUFFER_LARGE_DIGITS
                );
                f_SpinningIconFrame = 0;
                f_SpinningStarBufferIndex = 0;
                f_SpinningIconFrameTime = 0;
                f_UnusedSpinningStarCounter = 0;
                f_MenuBoardImageIndex ^= 1;
                LoadPlayerStatsBuffer(
                    g_MenuBoardImages[f_MenuBoardImageIndex],
                    PLAYER_STATS_BUFFER_BET_ICONS
                );
                LoadPlayerStatsBuffer(g_MenuArrowSizedImages[0], PLAYER_STATS_BUFFER_STAKE_MARK);
                LoadPlayerStatsBuffer(g_NamePlateImage, PLAYER_STATS_BUFFER_NAME_PLATE);
                LoadPlayerStatsBuffer(
                    g_AnimatedIconImages[0],
                    PLAYER_STATS_BUFFER_BET_ICON_FIRST_FRAME
                );
                LoadPlayerStatsBuffer(
                    g_AnimatedIconImages[1],
                    PLAYER_STATS_BUFFER_BET_ICON_SECOND_FRAME
                );
                LoadPlayerStatsBuffer(g_MenuArrowSizedImages[1], PLAYER_STATS_BUFFER_GREEN_ARROWS);
                LoadPlayerStatsBuffer(
                    g_SecondAnimatedIconImages[0],
                    PLAYER_STATS_BUFFER_PLAYER_LABELS
                );
                LoadPlayerStatsBuffer(
                    g_SecondAnimatedIconImages[1],
                    PLAYER_STATS_BUFFER_HANDICAP_PLAQUE
                );
                f_MenuLabelImageIndex ^= 1;
                LoadPlayerStatsBuffer(
                    g_MenuLabelImages[f_MenuLabelImageIndex],
                    PLAYER_STATS_BUFFER_BET_LABELS
                );
                LoadPlayerStatsBuffer(g_BrickWallImage, PLAYER_STATS_BUFFER_BRICK_WALL);
                LoadPlayerStatsBuffer(g_MenuSpareImages[0], PLAYER_STATS_BUFFER_TRIPLE_PAGE_ARROWS);
                ComputeRoundStatistics();
                f_MiniGolfDiscFrame = MINI_GOLF_FACING_FRAME;
                f_MiniGolfDiscFrameTime = 0;
                f_MiniGolfDiscTurn = MINI_GOLF_TURN_NONE;
                f_MiniGolfDiscBufferIndex = 0;
                break;

            case MENU_SCREEN_COURSE_STATS:
                f_SpinningIconFrameTime = 0;
                f_SpinningIconFrame = 0;
                if (g_IsDebugEnabled) {
                    ReadSavedGames();
                }

                LoadCourseStatsIcons(MENU_ICON_IMAGE_PAIRS[0]);
                LoadCourseTrophyImages(g_CourseTrophyImages, COURSE_BUFFER_FIRST);
                LoadCourseMenuBuffer(g_SpinningBadgeImage, COURSE_MENU_BUFFER_HOLE_IN_ONE_BADGE);
                LoadCourseMenuBuffer(g_NamePlateImage, COURSE_MENU_BUFFER_NAME_PLATE);
                LoadCourseMenuBuffer(g_AnimatedIconImages[0], COURSE_MENU_BUFFER_SPINNING_BADGE);
                LoadCourseMenuBuffer(
                    g_SecondAnimatedIconImages[0],
                    COURSE_MENU_BUFFER_DIGITS_AND_ARROWS
                );
                f_ImageSlotIndex ^= 1;
                LoadCourseMenuBuffer(
                    g_ImageSlots[f_ImageSlotIndex],
                    COURSE_MENU_BUFFER_LARGE_DIGITS
                );
                LoadCourseMenuBuffer(g_MenuSpareImages[0], COURSE_MENU_BUFFER_GREEN_ARROWS);
                ComputeRoundStatistics();
                g_MenuMemory.statsCourse = GetOpenCourseCount();
                break;

            case MENU_SCREEN_BEST_TIMES:
                ReadSavedGames();
                LoadBestTimesCourseIcons(MENU_ICON_IMAGE_PAIRS[0]);
                LoadBestTimesBuffer(NULL, BEST_TIMES_BUFFER_FACE_STRIP);
                f_ImageSlotIndex ^= 1;
                LoadCourseMenuBuffer(
                    g_ImageSlots[f_ImageSlotIndex],
                    COURSE_MENU_BUFFER_LARGE_DIGITS
                );
                LoadBestTimesBuffer(g_ImageSlots[f_ImageSlotIndex], BEST_TIMES_BUFFER_UNREAD);
                LoadCourseMenuBuffer(g_NamePlateImage, COURSE_MENU_BUFFER_NAME_PLATE);
                f_MenuBoardImageIndex ^= 1;
                LoadBestTimesBuffer(
                    g_MenuBoardImages[f_MenuBoardImageIndex],
                    BEST_TIMES_BUFFER_PLACES
                );
                LoadBestTimesBuffer(g_MenuSpareImages[0], BEST_TIMES_BUFFER_GREEN_ARROWS);
                LoadBestTimesFaces();
                break;

            case MENU_SCREEN_CHARACTER_TABLE:
                ReadSavedGames();
                LoadBestTimesBuffer(NULL, BEST_TIMES_BUFFER_FACE_STRIP);
                f_ImageSlotIndex ^= 1;
                LoadCourseMenuBuffer(
                    g_ImageSlots[f_ImageSlotIndex],
                    COURSE_MENU_BUFFER_LARGE_DIGITS
                );
                f_ImageSlotIndex ^= 1;
                LoadCharacterTableBuffer(g_ImageSlots[f_ImageSlotIndex], TABLE_BUFFER_CURSOR);
                LoadCharacterTableBuffer(g_NamePlateImage, TABLE_BUFFER_UNREAD);
                MarkShownTableCharacters();
                LoadCharacterTableBuffer(g_MenuSpareImages[0], TABLE_BUFFER_ARROWS);
                LoadCharacterTableBuffer(g_MenuSpareImages[1], TABLE_BUFFER_STAR);
                LoadTableFaces();
                break;

            case MENU_SCREEN_BEST_SHOTS:
                ReadSavedGames();
                LoadBestShotsCourseIcons(MENU_ICON_IMAGE_PAIRS[0]);
                f_MenuBoardImageIndex ^= 1;
                LoadCharacterFaceStrip(g_MenuBoardImages[f_MenuBoardImageIndex]);
                f_MenuLabelImageIndex ^= 1;
                f_ImageSlotIndex ^= 1;
                LoadCourseMenuBuffer(
                    g_ImageSlots[f_MenuLabelImageIndex],
                    COURSE_MENU_BUFFER_LARGE_DIGITS
                );
                LoadScoreLabelBuffer(
                    g_MenuLabelImages[f_ImageSlotIndex],
                    SCORE_LABEL_BUFFER_CURSOR_BOX
                );
                LoadScoreLabelBuffer(g_NamePlateImage, SCORE_LABEL_BUFFER_NAME_PLATE);
                LoadScoreLabelBuffer(g_MenuSpareImages[0], SCORE_LABEL_BUFFER_LABELS);
                ReadBestShots();
                LoadBestShotFaces();
                break;

            case MENU_SCREEN_RING_SHOT_RECORDS:
                ReadSavedGames();
                f_MenuBoardImageIndex ^= 1;
                LoadCharacterFaceStrip(g_MenuBoardImages[f_MenuBoardImageIndex]);
                f_MenuLabelImageIndex ^= 1;
                f_ImageSlotIndex ^= 1;
                LoadCourseMenuBuffer(
                    g_ImageSlots[f_ImageSlotIndex],
                    COURSE_MENU_BUFFER_LARGE_DIGITS
                );
                LoadRingShotRecordBuffer(g_NamePlateImage, RING_SHOT_RECORD_BUFFER_UNREAD);
                LoadRingShotRecordBuffer(
                    g_MenuLabelImages[f_MenuLabelImageIndex],
                    RING_SHOT_RECORD_BUFFER_RAINBOW_BOX
                );
                LoadRingShotRecordBuffer(
                    g_AnimatedIconImages[0],
                    RING_SHOT_RECORD_BUFFER_COURSE_NUMBERS
                );
                LoadRingShotCourseIcons(MENU_ICON_IMAGE_PAIRS[0]);
                LoadRingShotRecordBuffer(
                    g_MenuSpareImages[0],
                    RING_SHOT_RECORD_BUFFER_GREEN_ARROWS
                );
                ReadRingShotReplays();
                LoadRingShotRecordFaces();
                break;
        }
    }

    if (g_MenuScreen == MENU_SCREEN_EXIT) {
        TransferSaveData(save, TRUE);
        if (g_GameState.gameMode == GAME_MODE_TRAINING) {
            g_MenuScreen = MENU_SCREEN_COURSE_SELECT;
        }
        else {
            g_MenuScreen = MENU_SCREEN_BLANK;
        }

        if (g_GameState.isRoundContinued) {
            InitPlay(PLAY_KIND_WITHOUT_HOLE_DISPLAY);
            InitRoundHud();
            osSyncPrintf("ContPhase2\n");
        }
        else {
            g_GameState.saveCount = 0;
            InitPlay(PLAY_KIND_WITHOUT_HOLE_DISPLAY);
            SetUpChosenGame();
            InitRoundHud();
        }

        SetGameFlag(GAME_FLAG_HOLE_LOADED);
        if (IsGameFlagSet(GAME_FLAG_STAY_IN_MENUS)) {
            LoadMenuResources();
            OnMenuResourcesLoaded();
        }
        else {
            g_Scene = SCENE_SWAPPING_MODULES;
            g_NextScene = SCENE_PLAY_ROUND;
            g_SceneModuleToUnload = MODULE_MENU;
            g_SceneModuleToLoad = MODULE_PLAY;
            RequestSubModule();
        }
    }
}

void CheckControllersAndSetPans(void) {
    s32 playerIndex;
    s32 otherIndex;

    g_HasOwnControllers = TRUE;
    for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
        for (otherIndex = 0; otherIndex != g_GameState.playerCount; otherIndex++) {
            if (playerIndex != otherIndex &&
                g_PlayerControllerIndices[playerIndex] != NO_CONTROLLER_INDEX &&
                g_PlayerControllerIndices[otherIndex] != NO_CONTROLLER_INDEX &&
                g_PlayerControllerIndices[playerIndex] == g_PlayerControllerIndices[otherIndex]) {
                g_HasOwnControllers = FALSE;
            }
        }
    }

    for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
        (&g_Players[playerIndex])->soundPan =
            f_PlayerSoundPans[g_GameState.playerCount - 1][playerIndex];
    }
}

void OnMenuResourcesLoaded(void) {
}

#define IsHoleChoiceMode(mode) \
    ((mode) == GAME_MODE_STROKE || \
        (mode) == GAME_MODE_MATCH_GAME || \
        (mode) == GAME_MODE_RING_SHOT || \
        (mode) == GAME_MODE_SKINS_MATCH || \
        (mode) == GAME_MODE_CLUB_SLOTS)

void SetUpChosenGame(void) {
    SaveData *save = GetSaveData();
    MenuMemory *memory = GetMenuMemory();
    Player *player;
    s32 index;
    ClubSlot clubSlot;
    s32 *holeSelection;
    s32 row;
    s32 column;

    memset(&g_RoundHud, 0, sizeof(g_RoundHud));
    if (g_GameState.replayKind != REPLAY_KIND_NONE) {
        if (g_GameState.replayKind == REPLAY_KIND_RING_SHOT) {
            LoadRingShotRecord(&save->ringShotRecords[memory->ringShotReplaySlot]);
        }
        else {
            LoadBestShotReplay(&save->rankedRecords[GetChosenBestShot()]);
        }

        g_GameState.unusedHoleValue = 0;
        g_GameState.playerCount = 1;
        g_GameState.step = GAME_STATE_START;
        return;
    }

    StartBets();
    if (TakeClubTournamentEntry()) {
        g_GameState.holeSelection = HOLE_SELECTION_ALL;
        g_GameState.courseIndex = GetClubTournamentCourse();
        g_GameState.gameMode = GetClubTournamentGameMode();
        g_GameState.isMiniGolf = g_GameState.courseIndex >= MINI_GOLF_COURSE_INDEX;
        for (index = 0; index != BET_STAKE_SLOT_COUNT; index++) {
            g_GameState.betRates[index] = UNSET_BET_RATE;
        }

        for (index = 0; index != PLAYER_COUNT_MAX; index++) {
            g_ChosenHandicaps[index] = 0;
        }

        if (GetClubTournamentCharacter() != ANY_CHARACTER) {
            g_ChosenCharacters[0] = GetClubTournamentCharacter();
            g_ChosenLefties[0] = FALSE;
        }
    }
    else {
        if (g_GameState.gameMode == GAME_MODE_RING_SHOT) {
            g_GameState.courseIndex = g_MenuMemory.courseIndex;
        }
        else if (g_GameState.gameMode == GAME_MODE_TRAINING) {
            g_GameState.courseIndex = 0;
        }
        else {
            g_GameState.courseIndex = GetChosenCourseIndex();
        }

        if (IsHoleChoiceMode(g_GameState.gameMode)) {
            holeSelection = &g_GameState.holeSelection;
            *holeSelection = f_StartingHoleSelection;
            if (COURSE_SELECT_LAYOUT == COURSE_SELECT_LAYOUT_HOLES) {
                switch (g_MenuMemory.holesChoice) {
                    case HOLES_CHOICE_UNUSED:
                        *holeSelection = HOLE_SELECTION_ALL;
                        break;
                    case HOLES_CHOICE_FRONT_NINE:
                        *holeSelection = HOLE_SELECTION_FRONT_NINE;
                        break;
                    case HOLES_CHOICE_BACK_NINE:
                        *holeSelection = HOLE_SELECTION_BACK_NINE;
                        break;
                    case HOLES_CHOICE_ALL:
                        *holeSelection = HOLE_SELECTION_ALL;
                        break;
                }
            }
        }
        else {
            g_GameState.holeSelection = HOLE_SELECTION_ALL;
        }
    }

    g_GameState.unusedHoleValue = 0;
    g_GameState.playerCount = g_ChosenPlayerCount;
    if (g_GameState.gameMode == GAME_MODE_GET_CHARACTER) {
        g_GameState.playerCount = 2;
    }

    g_GameState.step = GAME_STATE_RELOAD_HOLE;
    CheckControllersAndSetPans();
    if (g_GameState.gameMode == GAME_MODE_TOURNAMENT) {
        StartRoundStats();
    }

    for (index = 0; index != g_GameState.playerCount; index++) {
        player = &g_Players[index];
        player->handicap = g_ChosenHandicaps[index];
        player->isLefty = g_ChosenLefties[index];
        player->characterIndex = g_ChosenCharacters[index];
        player->controllerIndex = g_PlayerControllerIndices[index];
        sprintf(
            player->name,
            "%s",
            g_MenuCharacterNames[f_CharacterChoices[g_ChosenCharacters[index]]]
        );
        if (g_ChosenCharacters[index] >= CHARACTER_KID) {
            row = g_MenuMemory.characterRows[index];
            column = g_MenuMemory.characterColumns[index];
            if (row == GAME_BOY_CHARACTER_ROW) {
                player->gameBoyCharacterIndex = column;
            }

            sprintf(player->name, "%s", g_GameBoyCharacters[player->gameBoyCharacterIndex].name);
        }
        else {
            player->gameBoyCharacterIndex = -1;
            sprintf(
                player->name,
                "%s",
                g_MenuCharacterNames[f_CharacterChoices[g_ChosenCharacters[index]]]
            );
        }

        if (player->controllerIndex == NO_CONTROLLER_INDEX) {
            g_Players[index].isComputer = TRUE;
        }
        else {
            g_Players[index].isComputer = FALSE;
        }

        for (clubSlot = 0; clubSlot != CLUB_SET_SIZE; clubSlot++) {
            if (!((f_PlayerClubMasks[index] >> clubSlot) & 1)) {
                player->clubs[clubSlot] = NO_CLUB;
            }
            else {
                if (g_ChosenCharacters[index] >= CHARACTER_KID) {
                    player->clubs[clubSlot] = clubSlot +
                        ((player->gameBoyCharacterIndex + CHARACTER_KID) * CLUB_SET_SIZE);
                }
                else {
                    player->clubs[clubSlot] =
                        clubSlot + (g_ChosenCharacters[index] * CLUB_SET_SIZE);
                }
            }
        }

        for (; clubSlot != CLUB_SLOT_COUNT; clubSlot++) {
            player->clubs[clubSlot] = NO_CLUB;
        }

        if (g_GameState.gameMode == GAME_MODE_TRAINING) {
            player->powerShotCount = POWER_SHOTS_UNLIMITED;
        }
        else if (g_GameState.holeSelection == HOLE_SELECTION_FRONT_NINE ||
            g_GameState.holeSelection == HOLE_SELECTION_BACK_NINE) {
            player->powerShotCount = NINE_HOLE_POWER_SHOT_COUNT;
        }
        else {
            player->powerShotCount = EIGHTEEN_HOLE_POWER_SHOT_COUNT;
        }

        LoadPlayerFace(index, player->characterIndex, player->paletteIndex);
    }

    if (g_GameState.gameMode == GAME_MODE_GET_CHARACTER) {
        g_Players[1].isComputer = TRUE;
        g_Players[1].controllerIndex = NO_CONTROLLER_INDEX;
    }
}

void UpdateMenuTitleScreen(void) {
    MenuMemory *memory = GetMenuMemory();
    s32 index;
    s8 titleChoice;

    if (IsHudAnimationDone() != TRUE) {
        return;
    }

    f_BetSlideOffset = 0;
    g_BetPagesPhase = MENU_PHASE_START;
    g_BetPagesWallSlide = STATS_SLIDE_CURRENT;
    g_CourseBannersSlide = 0;
    f_BetSettingsSlide = 0;
    f_HandicapsSlide = 0;
    g_GameState.isRoundContinued = FALSE;
    g_GameState.isSuddenDeath = FALSE;
    g_GameState.isMiniGolf = FALSE;
    g_GameState.replayKind = REPLAY_KIND_NONE;
    g_GameState.currentPlayerIndex = 0;
    if (GetWipeKind() != WIPE_NONE && !g_IsMenuWipeOpening) {
        if (GetWipeProgress() == WIPE_CLOSED_PROGRESS) {
            g_Scene = SCENE_SWAPPING_MODULES;
            g_SceneModuleToUnload = MODULE_MENU;
            g_NextScene = SCENE_TRANSMIT;
            g_SceneModuleToLoad = MODULE_TRANSMIT;
            ResetGameBoyTransfer(TRANSFER_MODE_FROM_MENU);
            StartWipe(WIPE_OPENING);
        }

        return;
    }

    if (GetWipeKind() != WIPE_NONE && g_IsMenuWipeOpening == TRUE) {
        return;
    }

    if (g_MenuPressedButtons & A_BUTTON) {
        PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
        titleChoice = g_MenuMemory.titleChoice;
        if (titleChoice == TITLE_CHOICE_NEW_GAME) {
            for (index = 0; index != PLAYER_COUNT_MAX; index++) {
                memory->characterColumns[index] = 0;
                memory->characterRows[index] = 0;
            }

            memory->modeChoice = 0;
            memory->modeSpin = 0;
            memory->betColumn = 0;
            for (index = 0; index != BET_STAKE_SLOT_COUNT; index++) {
                memory->betStakeLevels[index] = 0;
            }

            memory->courseChoice = 0;
            memory->courseSpin = 0;
            memory->holeIndex = 0;
            memory->courseIndex = 0;
            StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
            g_MenuScreen = MENU_SCREEN_CHARACTER_SELECT;
            g_ChosenPlayerCount = g_MenuMemory.playerCountChoice + 1;
            for (index = 0; index != PLAYER_COUNT_MAX; index++) {
                g_ChosenLefties[index] = FALSE;
            }

            g_ChoosingPlayerIndex = 0;
            for (index = 0; index != f_MenuCharacterCount; index++) {
                g_TakenCharacters.isCharacterTaken[index] = FALSE;
            }
        }
        else if (titleChoice == TITLE_CHOICE_SAVED_GAMES) {
            if (g_HasBroughtOverCharacter) {
                g_IsMenuWipeOpening = FALSE;
                LoadWipeShape(TRANSMIT_WIPE_SHAPE);
                StartWipe(WIPE_CLOSING);
                g_IsRingShotWipeOpening = FALSE;
            }
            else {
                g_ChosenPlayerCount = titleChoice;
                StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                g_MenuScreen = MENU_SCREEN_SAVED_GAMES;
            }
        }
        else if (titleChoice == TITLE_CHOICE_MENUS) {
            g_ChosenPlayerCount = 1;
            if ((g_MenuRepeatButtons == TRANSMIT_BUTTONS) |
                (g_MenuRepeatButtons == TRANSMIT_OTHER_BUTTONS)) {
                PlaySound(SOUND_CONTROLS_CLOSE, TRANSMIT_SOUND_CHANNEL, MENU_VOLUME_SOFT);
                g_MenuMemory.isTransmitShown = TRUE;
                g_MenuScreen = titleChoice;
            }
            else {
                g_MenuMemory.isTransmitShown = FALSE;
                g_MenuScreen = MENU_SCREEN_MAIN_WITHOUT_TRANSMIT;
            }
        }

        if (g_LastMenuScreen != g_MenuScreen ||
            g_LastMenuPlayerCountChoice != g_MenuMemory.playerCountChoice) {
            g_MenuMemory.modeChoice = 0;
            g_TakenCharacters.unusedMenuValue = 0;
        }

        g_IsRingShotWipeOpening = FALSE;
        g_IsMenuWipeOpening = FALSE;
        g_LastMenuScreen = g_MenuScreen;
        g_LastMenuPlayerCountChoice = g_MenuMemory.playerCountChoice;
        return;
    }

    if (g_MenuPressedButtons & B_BUTTON) {
        StartHudAnimation(HUD_ANIMATION_SPIRAL);
        g_Scene = SCENE_SWAPPING_MODULES;
        g_NextScene = SCENE_TITLE_SCREEN;
        g_SceneModuleToUnload = MODULE_MENU;
        g_SceneModuleToLoad = MODULE_TITLE_SCREEN;
        PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
        SetTextSlot(
            CLEARED_TEXT_X,
            CLEARED_TEXT_Y,
            NO_MESSAGE,
            TEXT_SLOT_LARGE_FONT,
            0,
            0,
            0,
            0,
            0,
            0
        );
        return;
    }

    if (g_MenuRepeatButtons & U_JPAD) {
        g_MenuMemory.titleChoice--;
        PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
    }
    else if (g_MenuRepeatButtons & D_JPAD) {
        g_MenuMemory.titleChoice++;
        PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
    }

    if (g_MenuMemory.titleChoice < 0) {
        g_MenuMemory.titleChoice = TITLE_CHOICE_COUNT - 1;
    }

    if (g_MenuMemory.titleChoice >= TITLE_CHOICE_COUNT) {
        g_MenuMemory.titleChoice = 0;
    }

    if (g_MenuMemory.titleChoice == TITLE_CHOICE_NEW_GAME) {
        if (g_MenuRepeatButtons & L_JPAD) {
            g_MenuMemory.playerCountChoice--;
            PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
        }
        else if (g_MenuRepeatButtons & R_JPAD) {
            g_MenuMemory.playerCountChoice++;
            PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
        }
    }

    if (g_MenuMemory.playerCountChoice < 0) {
        g_MenuMemory.playerCountChoice = PLAYER_COUNT_MAX - 1;
    }

    if (g_MenuMemory.playerCountChoice >= PLAYER_COUNT_MAX) {
        g_MenuMemory.playerCountChoice = 0;
    }
}

void ResetViewerCharacter(void) {
    g_MenuMemory.viewerCharacter = 0;
    while (g_TakenCharacters.isCharacterTaken[g_MenuMemory.viewerCharacter] == TRUE) {
        g_MenuMemory.viewerCharacter++;
        if (g_MenuMemory.viewerCharacter > f_MenuCharacterCount - 1) {
            g_MenuMemory.viewerCharacter = 0;
        }
    }
}

s32 UpdateViewerCharacter(s32 unusedSetting UNUSED, s32 finalPlayerIndex) {
    if (f_IsViewerAdjusting != FALSE) {
        while (g_TakenCharacters.isCharacterTaken[g_MenuMemory.viewerCharacter] == TRUE) {
            g_MenuMemory.viewerCharacter++;
            if (g_MenuMemory.viewerCharacter > f_MenuCharacterCount - 1) {
                g_MenuMemory.viewerCharacter = 0;
            }
        }

        if (g_ControllerStates[0].pressedButtons & U_JPAD) {
            g_MenuMemory.viewerCharacter--;
            PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
            while (g_TakenCharacters.isCharacterTaken[g_MenuMemory.viewerCharacter] == TRUE) {
                g_MenuMemory.viewerCharacter--;
                if (g_MenuMemory.viewerCharacter < 0) {
                    g_MenuMemory.viewerCharacter = f_MenuCharacterCount - 1;
                }
            }
        }
        else if (g_ControllerStates[0].pressedButtons & D_JPAD) {
            PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
            g_MenuMemory.viewerCharacter++;
            while (g_TakenCharacters.isCharacterTaken[g_MenuMemory.viewerCharacter] == TRUE) {
                g_MenuMemory.viewerCharacter++;
                if (g_MenuMemory.viewerCharacter > f_MenuCharacterCount - 1) {
                    g_MenuMemory.viewerCharacter = 0;
                }
            }
        }

        if (g_MenuMemory.viewerCharacter < 0) {
            g_MenuMemory.viewerCharacter = f_MenuCharacterCount - 1;
            while (g_TakenCharacters.isCharacterTaken[g_MenuMemory.viewerCharacter] == TRUE) {
                g_MenuMemory.viewerCharacter--;
            }
        }

        if (g_MenuMemory.viewerCharacter > f_MenuCharacterCount - 1) {
            g_MenuMemory.viewerCharacter = 0;
            while (g_TakenCharacters.isCharacterTaken[g_MenuMemory.viewerCharacter] == TRUE) {
                g_MenuMemory.viewerCharacter++;
            }
        }
    }
    else if (f_IsChosenAnimationPlaying == TRUE) {
        if (GetCurrentAnimationEndFrame(0) == GetAnimationFrame(0)) {
            f_IsChosenAnimationPlaying = FALSE;
            if (g_ChoosingPlayerIndex == finalPlayerIndex) {
                return 1;
            }
        }
    }
    else if (!(g_ControllerStates[0].buttons & Z_TRIG)) {
        if ((g_MenuPressedButtons & (A_BUTTON | R_TRIG)) &&
            !g_TakenCharacters.isCharacterTaken[g_MenuMemory.viewerCharacter]) {
            PlayCharacterSound(
                g_MenuMemory.viewerCharacter,
                0,
                CHARACTER_SOUND_CHOSEN,
                MENU_VOLUME_SOFT,
                SOUND_FULL_SCALE
            );
            f_IsChosenAnimationPlaying = TRUE;
            SetAnimationSpeed(0, 1.0f);
            return 0;
        }

        if (g_MenuPressedButtons & B_BUTTON) {
            PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
            return -1;
        }
        while (g_TakenCharacters.isCharacterTaken[g_MenuMemory.viewerCharacter] == TRUE) {
            g_MenuMemory.viewerCharacter++;
            if (g_MenuMemory.viewerCharacter > f_MenuCharacterCount - 1) {
                g_MenuMemory.viewerCharacter = 0;
            }
        }

        if (g_MenuRepeatButtons & U_JPAD) {
            g_MenuMemory.viewerCharacter--;
            PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            while (g_TakenCharacters.isCharacterTaken[g_MenuMemory.viewerCharacter] == TRUE) {
                g_MenuMemory.viewerCharacter--;
                if (g_MenuMemory.viewerCharacter < 0) {
                    g_MenuMemory.viewerCharacter = f_MenuCharacterCount - 1;
                }
            }
        }
        else if (g_MenuRepeatButtons & D_JPAD) {
            PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            g_MenuMemory.viewerCharacter++;
            while (g_TakenCharacters.isCharacterTaken[g_MenuMemory.viewerCharacter] == TRUE) {
                g_MenuMemory.viewerCharacter++;
                if (g_MenuMemory.viewerCharacter > f_MenuCharacterCount - 1) {
                    g_MenuMemory.viewerCharacter = 0;
                }
            }
        }

        if (g_MenuMemory.viewerCharacter < 0) {
            g_MenuMemory.viewerCharacter = f_MenuCharacterCount - 1;
            while (g_TakenCharacters.isCharacterTaken[g_MenuMemory.viewerCharacter] == TRUE) {
                g_MenuMemory.viewerCharacter--;
            }
        }

        if (g_MenuMemory.viewerCharacter > f_MenuCharacterCount - 1) {
            g_MenuMemory.viewerCharacter = 0;
            while (g_TakenCharacters.isCharacterTaken[g_MenuMemory.viewerCharacter] == TRUE) {
                g_MenuMemory.viewerCharacter++;
            }
        }
    }

    return 0;
}

void UpdateAnimationViewer(void) {
    Boolean isDone = IsHudAnimationDone();

    if (isDone == TRUE && UpdateViewerCharacter(1, 2) == -1) {
        g_ChoosingPlayerIndex = isDone;
        g_MenuScreen = MENU_SCREEN_TITLE;
        FreeAllCharacterSlots();
    }
}

#define MODE_SELECT_TURN 90
#define MODE_SELECT_SPIN_SPEED 18

#define MODE_TEXT_X 40
#define MODE_TEXT_Y 188
#define MODE_TEXT_SLOT 0
#define MESSAGE_FIRST_MODE 4
#define MESSAGE_SOLO_STROKE 0x39
#define MESSAGE_MODE_NEEDS_CONTROLLERS 0x4A

#define LoadChosenModeIcon() \
    { \
        g_AnimatedIconFrame = 0; \
        g_AnimatedIconFrameTime = 0; \
        OpenEncodedAsset( \
            g_ModeIconAnimations[f_ModeChoiceIcons[g_ChosenPlayerCount - 1] \
                                                  [g_MenuMemory.modeChoice]][0], \
            &asset \
        ); \
        ReadEncodedAsset(g_AnimatedIconImages[g_AnimatedIconBufferIndex ^ 1], &asset); \
        g_AnimatedIconBufferIndex ^= 1; \
    }

void UpdateModeSelect(void) {
    EncodedAsset asset;
    Boolean isDone = IsHudAnimationDone();
    s32 direction;
    s32 i;
    u32 step;
    s16 gridMark;
    s32 iconIndex;
    s32 chosenIcon;

    if (isDone != TRUE || GetWipeKind() != WIPE_NONE) {
        return;
    }

    g_AreBetPagesBlocked = FALSE;
    if (g_MenuPressedButtons & A_BUTTON) {
        if (g_MenuMemory.modeSpin != 0) {
            g_MenuMemory.modeSpin = 0;
            LoadChosenModeIcon();
            return;
        }

        PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
        SetTextSlot(
            0,
            0,
            NO_MESSAGE,
            TEXT_SLOT_LARGE_FONT,
            MODE_TEXT_SLOT,
            TEXT_FLAG_TYPED,
            0xFF,
            0xFF,
            0xFF,
            OPAQUE_ALPHA
        );
        f_BetSlideOffset = 0;
        g_BetPagesPhase = MENU_PHASE_START;
        g_BetPagesWallSlide = STATS_SLIDE_CURRENT;
        g_CourseBannersSlide = 0;
        f_BetSettingsSlide = 0;
        f_HandicapsSlide = 0;
        g_GameState.isMiniGolf = FALSE;
        iconIndex = f_ModeChoiceIcons[g_ChosenPlayerCount - 1][g_MenuMemory.modeChoice];
        g_GameState.roundSeed = rand();
        switch (iconIndex) {
            case MODE_ICON_TOURNAMENT:
                PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
                StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                g_GameState.gameMode = GAME_MODE_TOURNAMENT;
                g_MenuScreen = MENU_SCREEN_COURSE_SELECT;
                g_CourseSelectKind = COURSE_SELECT_KIND_TOURNAMENTS;
                COURSE_SELECT_LAYOUT = COURSE_SELECT_LAYOUT_AWARDS;
                g_BetSet = BET_SET_NONE;
                ApplyBetPreset(BET_PRESET_NONE);
                break;
            case MODE_ICON_GET_CHARACTER:
                PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
                StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                g_GameState.gameMode = GAME_MODE_GET_CHARACTER;
                g_GameState.playerCount = 2;
                g_MenuScreen = MENU_SCREEN_OPPONENT_SELECT;
                g_CourseSelectKind = COURSE_SELECT_KIND_COURSES;
                COURSE_SELECT_LAYOUT = COURSE_SELECT_LAYOUT_PLAIN;
                g_BetSet = BET_SET_NONE;
                ApplyBetPreset(BET_PRESET_NONE);
                for (i = 0; i != PLAYER_COUNT_MAX; i++) {
                    g_ChosenHandicaps[i] = 0;
                }

                for (i = 0; i != BET_STAKE_SLOT_COUNT - 1; i++) {
                    g_GameState.betRates[i] = UNSET_BET_RATE;
                }

                break;
            case MODE_ICON_STROKE:
                PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
                StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                g_MenuScreen = MENU_SCREEN_COURSE_SELECT;
                g_GameState.gameMode = GAME_MODE_STROKE;
                g_CourseSelectKind = COURSE_SELECT_KIND_COURSES;
                g_MenuMemory.holesChoice = HOLES_CHOICE_ALL;
                COURSE_SELECT_LAYOUT = COURSE_SELECT_LAYOUT_HOLES;
                if (g_ChosenPlayerCount == PLAYER_COUNT_MAX) {
                    g_BetSet = BET_SET_STROKE_FOUR_PLAYER;
                }
                else {
                    g_BetSet = BET_SET_STROKE;
                }

                ApplyBetPreset(BET_PRESET_STROKE);
                if (g_ChosenPlayerCount == 1) {
                    for (i = 0; i != PLAYER_COUNT_MAX; i++) {
                        g_ChosenHandicaps[i] = 0;
                    }

                    for (i = 0; i != BET_STAKE_SLOT_COUNT - 1; i++) {
                        g_GameState.betRates[i] = UNSET_BET_RATE;
                    }

                    g_AreBetPagesBlocked = TRUE;
                }

                break;
            case MODE_ICON_MATCH_GAME:
                PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
                StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                g_GameState.gameMode = GAME_MODE_MATCH_GAME;
                g_MenuScreen = MENU_SCREEN_COURSE_SELECT;
                g_CourseSelectKind = COURSE_SELECT_KIND_COURSES;
                COURSE_SELECT_LAYOUT = COURSE_SELECT_LAYOUT_HOLES;
                g_BetSet = BET_SET_MATCH_GAME;
                g_MenuMemory.holesChoice = HOLES_CHOICE_ALL;
                ApplyBetPreset(BET_PRESET_MATCH_GAME);
                break;
            case MODE_ICON_SKINS_MATCH:
                PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
                StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                g_GameState.gameMode = GAME_MODE_SKINS_MATCH;
                g_MenuScreen = MENU_SCREEN_COURSE_SELECT;
                g_CourseSelectKind = COURSE_SELECT_KIND_COURSES;
                COURSE_SELECT_LAYOUT = COURSE_SELECT_LAYOUT_HOLES;
                g_BetSet = BET_SET_SKINS_MATCH;
                g_MenuMemory.holesChoice = HOLES_CHOICE_ALL;
                ApplyBetPreset(BET_PRESET_SKINS_MATCH);
                break;
            case MODE_ICON_SPEED_GOLF:
                PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
                StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                g_GameState.gameMode = GAME_MODE_SPEED_GOLF;
                g_MenuScreen = MENU_SCREEN_COURSE_SELECT;
                g_CourseSelectKind = COURSE_SELECT_KIND_COURSES;
                COURSE_SELECT_LAYOUT = COURSE_SELECT_LAYOUT_PLAIN;
                g_BetSet = BET_SET_NONE;
                g_MenuMemory.holesChoice = HOLES_CHOICE_FRONT_NINE;
                ApplyBetPreset(BET_PRESET_NONE);
                break;
            case MODE_ICON_MINI_GOLF:
                for (i = 0; i != g_ChosenPlayerCount; i++) {
                    if (g_PlayerControllerIndices[i] == NO_CONTROLLER_INDEX) {
                        break;
                    }
                }

                if (i < g_ChosenPlayerCount) {
                    PlaySound(SOUND_MENU_BLOCKED, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
                    return;
                }

                PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
                StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                g_GameState.gameMode = GAME_MODE_STROKE;
                g_GameState.isMiniGolf = TRUE;
                g_MenuScreen = MENU_SCREEN_COURSE_SELECT;
                g_MenuMemory.holesChoice = HOLES_CHOICE_ALL;
                g_CourseSelectKind = COURSE_SELECT_KIND_COURSES;
                COURSE_SELECT_LAYOUT = COURSE_SELECT_LAYOUT_HOLES;
                if (g_ChosenPlayerCount == PLAYER_COUNT_MAX) {
                    g_BetSet = BET_SET_MINI_GOLF_FOUR_PLAYER;
                }
                else {
                    g_BetSet = BET_SET_MINI_GOLF;
                }

                ApplyBetPreset(BET_PRESET_MINI_GOLF);
                if (g_ChosenPlayerCount == 1) {
                    for (i = 0; i != PLAYER_COUNT_MAX; i++) {
                        g_ChosenHandicaps[i] = 0;
                    }

                    for (i = 0; i != BET_STAKE_SLOT_COUNT - 1; i++) {
                        g_GameState.betRates[i] = UNSET_BET_RATE;
                    }

                    g_AreBetPagesBlocked = TRUE;
                }

                break;
            case MODE_ICON_TRAINING:
                PlaySound(SOUND_MENU_START, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
                StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
                g_GameState.gameMode = GAME_MODE_TRAINING;
                g_GameState.holeSelection = HOLE_SELECTION_ALL;
                StartLoadingChosenHole();
                g_MenuScreen = MENU_SCREEN_TRAINING_EXIT;
                g_CourseSelectKind = COURSE_SELECT_KIND_COURSES;
                COURSE_SELECT_LAYOUT = COURSE_SELECT_LAYOUT_PLAIN;
                g_BetSet = BET_SET_NONE;
                g_MenuMemory.holesChoice = HOLES_CHOICE_FRONT_NINE;
                ApplyBetPreset(BET_PRESET_NONE);
                break;
            case MODE_ICON_UNLISTED_STROKE:
                PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
                StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                g_GameState.gameMode = GAME_MODE_UNLISTED_STROKE;
                g_MenuScreen = MENU_SCREEN_COURSE_SELECT;
                g_CourseSelectKind = COURSE_SELECT_KIND_COURSES;
                COURSE_SELECT_LAYOUT = COURSE_SELECT_LAYOUT_PLAIN;
                g_BetSet = BET_SET_NONE;
                g_MenuMemory.holesChoice = HOLES_CHOICE_FRONT_NINE;
                ApplyBetPreset(BET_PRESET_NONE);
                break;
            case MODE_ICON_RING_SHOT:
                PlaySound(SOUND_MENU_START, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
                StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
                g_GameState.gameMode = GAME_MODE_RING_SHOT;
                g_MenuScreen = MENU_SCREEN_RING_SHOT_EXIT;
                g_CourseSelectKind = COURSE_SELECT_KIND_COURSES;
                COURSE_SELECT_LAYOUT = COURSE_SELECT_LAYOUT_PLAIN;
                g_BetSet = BET_SET_NONE;
                ApplyBetPreset(BET_PRESET_NONE);
                break;
            case MODE_ICON_CLUB_SLOTS:
                for (i = 0; i != g_ChosenPlayerCount; i++) {
                    if (g_PlayerControllerIndices[i] == NO_CONTROLLER_INDEX) {
                        break;
                    }
                }

                if (i < g_ChosenPlayerCount) {
                    PlaySound(SOUND_MENU_BLOCKED, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
                    return;
                }

                PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
                StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                g_GameState.gameMode = GAME_MODE_CLUB_SLOTS;
                g_MenuScreen = MENU_SCREEN_COURSE_SELECT;
                g_CourseSelectKind = COURSE_SELECT_KIND_COURSES;
                COURSE_SELECT_LAYOUT = COURSE_SELECT_LAYOUT_HOLES;
                g_BetSet = BET_SET_CLUB_SLOTS;
                g_MenuMemory.holesChoice = HOLES_CHOICE_ALL;
                ApplyBetPreset(BET_PRESET_CLUB_SLOTS);
                break;
        }

        g_GameState.holeSelection = HOLE_SELECTION_ALL;

        if (g_MenuScreen != MENU_SCREEN_TRAINING_EXIT) {
            StopLoaderThread();
        }

        FreeAllCharacterSlots();
        g_IsChosenHoleLoadStarted = FALSE;
        return;
    }

    if (g_MenuPressedButtons & B_BUTTON) {
        CharacterIndex characterIndex;
        s32 row;
        s32 column;

        if (g_MenuMemory.modeSpin != 0) {
            g_MenuMemory.modeSpin = 0;
            LoadChosenModeIcon();
        }

        StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
        g_MenuMemory.modeSpin = 0;
        SetTextSlot(
            0,
            0,
            NO_MESSAGE,
            TEXT_SLOT_LARGE_FONT,
            MODE_TEXT_SLOT,
            TEXT_FLAG_TYPED,
            0xFF,
            0xFF,
            0xFF,
            OPAQUE_ALPHA
        );
        PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
        g_MenuScreen = MENU_SCREEN_CHARACTER_SELECT;
        g_ChoosingPlayerIndex--;
        characterIndex = g_ChosenCharacters[g_ChoosingPlayerIndex];
        g_ChosenLefties[g_ChoosingPlayerIndex] = FALSE;
        g_PlayerControllerIndices[g_ChoosingPlayerIndex] = 0;
        g_TakenCharacters.isCharacterTaken[characterIndex] = FALSE;
        column = g_MenuMemory.characterColumns[g_ChoosingPlayerIndex];
        row = g_MenuMemory.characterRows[g_ChoosingPlayerIndex];
        gridMark = g_CharacterGrid[row][column];
        if (gridMark >= FIRST_IMPORTED_CHARACTER) {
            g_ImportedCharacters[gridMark - FIRST_IMPORTED_CHARACTER].isChosen = FALSE;
            g_ImportedCharacterColumns[g_ChoosingPlayerIndex] = NO_IMPORTED_COLUMN;
        }

        StopLoaderThread();
        FreeAllCharacterSlots();
        return;
    }

    if (g_MenuMemory.modeSpin == 0) {
        if (g_MenuRepeatButtons & L_JPAD) {
            PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            if (--g_MenuMemory.modeChoice < 0) {
                g_MenuMemory.modeChoice = f_ModeCounts[g_ChosenPlayerCount - 1] - 1;
            }

            g_MenuMemory.modeSpin = AmountPerMode(MODE_SELECT_TURN);
            if (g_ChosenPlayerCount == 1) {
                g_AnimatedIconFrameTime = 0;
                g_AnimatedIconFrame = 0;
                f_SpinningIconFrameTime = 0;
                f_SpinningIconFrame = 0;
            }
        }
        else if (g_MenuRepeatButtons & R_JPAD) {
            PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            if (++g_MenuMemory.modeChoice > f_ModeCounts[g_ChosenPlayerCount - 1] - 1) {
                g_MenuMemory.modeChoice = 0;
            }

            g_MenuMemory.modeSpin = AmountPerMode(-MODE_SELECT_TURN);
            if (g_ChosenPlayerCount == 1) {
                g_AnimatedIconFrameTime = 0;
                g_AnimatedIconFrame = 0;
                f_SpinningIconFrameTime = 0;
                f_SpinningIconFrame = 0;
            }
        }
    }

    if (Abs(g_MenuMemory.modeSpin) != 0) {
        direction = g_MenuMemory.modeSpin < 0 ? -1 : 1;
        step = AmountPerMode(MODE_SELECT_SPIN_SPEED);
        if (step == 0) {
            step = 1;
        }

        g_MenuMemory.modeSpin -= direction * step;

        if ((g_MenuMemory.modeSpin < 0 ? -1 : 1) != direction) {
            g_MenuMemory.modeSpin = 0;
        }

        if (g_MenuMemory.modeSpin == 0) {
            LoadChosenModeIcon();
        }
    }

    chosenIcon = f_ModeChoiceIcons[g_ChosenPlayerCount - 1][g_MenuMemory.modeChoice];
    for (i = 0; i != g_ChosenPlayerCount; i++) {
        if (g_PlayerControllerIndices[i] == NO_CONTROLLER_INDEX) {
            break;
        }
    }

    if (((chosenIcon == MODE_ICON_CLUB_SLOTS) | (chosenIcon == MODE_ICON_MINI_GOLF)) &&
        i < g_ChosenPlayerCount) {
        SetTextSlot(
            MODE_TEXT_X,
            MODE_TEXT_Y,
            MESSAGE_MODE_NEEDS_CONTROLLERS,
            TEXT_SLOT_LARGE_FONT,
            MODE_TEXT_SLOT,
            TEXT_FLAG_TYPED,
            0xFF,
            0xFF,
            0xFF,
            OPAQUE_ALPHA
        );
    }
    else if (g_ChosenPlayerCount == 1 &&
        f_ModeChoiceIcons[0][g_MenuMemory.modeChoice] == MODE_ICON_STROKE) {
        SetTextSlot(
            MODE_TEXT_X,
            MODE_TEXT_Y,
            MESSAGE_SOLO_STROKE,
            TEXT_SLOT_LARGE_FONT,
            MODE_TEXT_SLOT,
            TEXT_FLAG_TYPED,
            0xFF,
            0xFF,
            0xFF,
            OPAQUE_ALPHA
        );
    }
    else {
        SetTextSlot(
            MODE_TEXT_X,
            MODE_TEXT_Y,
            chosenIcon + MESSAGE_FIRST_MODE,
            TEXT_SLOT_LARGE_FONT,
            MODE_TEXT_SLOT,
            TEXT_FLAG_TYPED,
            0xFF,
            0xFF,
            0xFF,
            OPAQUE_ALPHA
        );
    }
}

void UpdateCourseSelect(void) {
    EncodedAsset asset;
    AssetFile file;

    s32 courseCount = GetOpenCourseCount();
    s32 openCourseCount;
    Boolean isDone;
    s32 index;
    s32 direction;
    s32 step;

    courseCount = courseCount < COURSE_COUNT ? courseCount + 1 : COURSE_COUNT;
    if (IsHudAnimationDone() != TRUE) {
        return;
    }

    if (g_BetPagesPhase == BET_PHASE_CHOOSING) {
        PlaySong(SONG_CONTROLS, 0);
        g_MenuScreen = MENU_SCREEN_BET_SETTINGS;
        g_BetPagesPhase = BET_PHASE_OPENING;
        g_BetPagesWallSlide = STATS_SLIDE_CURRENT;
        return;
    }

    if (g_BetPagesWallSlide != STATS_SLIDE_CURRENT || g_BetPagesPhase != MENU_PHASE_START) {
        return;
    }

    if (GetWipeKind() != WIPE_NONE && !g_IsMenuWipeOpening) {
        if (GetWipeProgress() == WIPE_CLOSED_PROGRESS) {
            g_MenuScreen = MENU_SCREEN_EXIT;
        }

        return;
    }

    isDone = IsHudAnimationDone();
    if (isDone != TRUE) {
        return;
    }

    if (g_MenuPressedButtons & A_BUTTON) {
        if (g_MenuMemory.courseSpin != 0) {
            g_MenuMemory.courseSpin = 0;
            OpenEncodedAsset(g_TrophyFrameAssets[g_MenuMemory.courseChoice][0], &asset);
            ReadEncodedAsset(g_TrophyFrameBuffers[f_ShownTrophyBufferIndex ^ 1], &asset);
            for (index = 0; index != TROPHY_PALETTE_COUNT; index++) {
                OpenAssetFile(g_TrophyPaletteFiles[g_MenuMemory.courseChoice][index], &file);
                ReadAssetFile(g_TrophyPalettes[index], GetAssetFileSize(&file), &file);
            }

            f_ShownTrophyBufferIndex ^= 1;
            return;
        }

        if (g_MenuMemory.courseChoice < g_MenuCourseCount) {
            openCourseCount = GetOpenCourseCount();
            if (openCourseCount < COURSE_COUNT && openCourseCount == g_MenuMemory.courseChoice) {
                PlaySound(SOUND_MENU_BLOCKED, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
                return;
            }

            for (index = 0; index != COURSE_SELECT_TEXT_SLOT_COUNT; index++) {
                SetTextSlot(
                    0,
                    0,
                    NO_MESSAGE,
                    TEXT_SLOT_LARGE_FONT,
                    index,
                    0,
                    0xFF,
                    0xFF,
                    0xFF,
                    OPAQUE_ALPHA
                );
            }

            g_IsMenuWipeOpening = FALSE;
            FreeAllCharacterSlots();
            LoadWipeShape(g_MenuMemory.courseChoice);
            StartWipe(WIPE_CLOSING);
            g_IsRingShotWipeOpening = FALSE;
        }

        PlaySound(SOUND_MENU_START, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
        return;
    }

    if (g_MenuPressedButtons & B_BUTTON) {
        g_MenuMemory.courseSpin = 0;
        PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
        StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
        if (f_ModeChoiceIcons[g_ChosenPlayerCount - 1][g_MenuMemory.modeChoice] ==
            MODE_ICON_GET_CHARACTER) {
            g_MenuScreen = MENU_SCREEN_OPPONENT_SELECT;
            g_ChoosingPlayerIndex--;
            g_TakenCharacters.isCharacterTaken[g_ChosenCharacters[g_ChoosingPlayerIndex]] = 0;
        }
        else {
            g_MenuScreen = MENU_SCREEN_MODE_SELECT;
        }

        COURSE_SELECT_LAYOUT = COURSE_SELECT_LAYOUT_AWARDS;
        g_IsChosenHoleLoadStarted = FALSE;
        StopLoaderThread();
        FreeAllCharacterSlots();
        for (index = 0; index != COURSE_SELECT_TEXT_SLOT_COUNT; index++) {
            SetTextSlot(
                0,
                0,
                NO_MESSAGE,
                TEXT_SLOT_LARGE_FONT,
                index,
                0,
                0xFF,
                0xFF,
                0xFF,
                OPAQUE_ALPHA
            );
        }

        return;
    }

    if (g_MenuMemory.courseSpin == 0) {
        if (g_MenuRepeatButtons & L_JPAD) {
            PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            if (--g_MenuMemory.courseChoice < 0) {
                g_MenuMemory.courseChoice = courseCount - 1;
            }

            g_MenuMemory.courseSpin = COURSE_SELECT_TURN / courseCount;
            for (index = 0; index != COURSE_SELECT_TEXT_SLOT_COUNT; index++) {
                SetTextSlot(
                    0,
                    0,
                    NO_MESSAGE,
                    TEXT_SLOT_LARGE_FONT,
                    index,
                    0,
                    0xFF,
                    0xFF,
                    0xFF,
                    OPAQUE_ALPHA
                );
            }
        }
        else if (g_MenuRepeatButtons & R_JPAD) {
            PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            if (++g_MenuMemory.courseChoice > courseCount - 1) {
                g_MenuMemory.courseChoice = 0;
            }

            g_MenuMemory.courseSpin = -COURSE_SELECT_TURN / courseCount;
            for (index = 0; index != COURSE_SELECT_TEXT_SLOT_COUNT; index++) {
                SetTextSlot(
                    0,
                    0,
                    NO_MESSAGE,
                    TEXT_SLOT_LARGE_FONT,
                    index,
                    0,
                    0xFF,
                    0xFF,
                    0xFF,
                    OPAQUE_ALPHA
                );
            }
        }
        else if ((g_MenuRepeatButtons & U_CBUTTONS) &&
            COURSE_SELECT_LAYOUT == COURSE_SELECT_LAYOUT_HOLES) {
            if (g_AreBetPagesBlocked != TRUE) {
                PlaySound(SOUND_WALL_SLIDE_OUT, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
                g_BetPagesPhase = BET_PHASE_OPENING;
                g_BetPagesWallSlide = STATS_SLIDE_OUT_RIGHT;
            }
            else {
                PlaySound(SOUND_MENU_BLOCKED, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
            }
        }
        else if ((g_MenuRepeatButtons & D_CBUTTONS) &&
            COURSE_SELECT_LAYOUT == COURSE_SELECT_LAYOUT_HOLES) {
            PlaySound(SOUND_SLICE, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
            g_MenuMemory.holesChoice = HOLES_CHOICE_ALL;
            g_GameState.holeSelection = HOLE_SELECTION_ALL;
            StartLoadingChosenHole();
        }
        else if ((g_MenuRepeatButtons & L_CBUTTONS) &&
            COURSE_SELECT_LAYOUT == COURSE_SELECT_LAYOUT_HOLES) {
            PlaySound(SOUND_SLICE, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
            g_MenuMemory.holesChoice = HOLES_CHOICE_FRONT_NINE;
            g_GameState.holeSelection = HOLE_SELECTION_FRONT_NINE;
            StartLoadingChosenHole();
        }
        else if ((g_MenuRepeatButtons & R_CBUTTONS) &&
            COURSE_SELECT_LAYOUT == COURSE_SELECT_LAYOUT_HOLES) {
            PlaySound(SOUND_SLICE, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
            g_MenuMemory.holesChoice = HOLES_CHOICE_BACK_NINE;
            g_GameState.holeSelection = HOLE_SELECTION_BACK_NINE;
            StartLoadingChosenHole();
        }
    }

    if (Abs(g_MenuMemory.courseSpin) != 0) {
        step = COURSE_SELECT_SPIN_SPEED / courseCount;
        direction = g_MenuMemory.courseSpin < 0 ? -1 : 1;
        if (step == 0) {
            step = 1;
        }

        g_MenuMemory.courseSpin -= direction * step;

        if ((g_MenuMemory.courseSpin < 0 ? -1 : 1) != direction) {
            g_MenuMemory.courseSpin = 0;
        }

        if (g_MenuMemory.courseSpin == 0) {
            OpenEncodedAsset(g_TrophyFrameAssets[g_MenuMemory.courseChoice][0], &asset);
            ReadEncodedAsset(g_TrophyFrameBuffers[f_ShownTrophyBufferIndex ^ 1], &asset);
            for (index = 0; index != TROPHY_PALETTE_COUNT; index++) {
                OpenAssetFile(g_TrophyPaletteFiles[g_MenuMemory.courseChoice][index], &file);
                ReadAssetFile(g_TrophyPalettes[index], GetAssetFileSize(&file), &file);
            }

            f_ShownTrophyBufferIndex ^= 1;
        }
    }
}

void UpdateMiniGolfCourseSelect(void) {
    EncodedAsset asset;
    Boolean isDone;
    s32 course;
    s32 direction;
    s32 step;
    s32 positiveSign;
    void *(*buffers)[MINI_GOLF_DISC_IMAGE_COUNT];

    isDone = IsHudAnimationDone();
    if (isDone != TRUE) {
        return;
    }

    if (f_MiniGolfDiscTurn != MINI_GOLF_TURN_NONE) {
        switch (f_MiniGolfDiscTurn) {
            case MINI_GOLF_TURN_TO_PREVIOUS:
                if (++f_MiniGolfDiscFrameTime == MINI_GOLF_TURN_FRAME_DURATION) {
                    f_MiniGolfDiscFrameTime = 0;
                    if (++f_MiniGolfDiscFrame >= MINI_GOLF_TURN_FRAME_COUNT) {
                        f_MiniGolfDiscFrame = 0;
                    }

                    if (f_MiniGolfDiscFrame >= 0 && f_MiniGolfDiscFrame <= MINI_GOLF_FACING_FRAME) {
                        course = g_MenuMemory.miniGolfCourseChoice - 1;
                        if (course < 0) {
                            course = MINI_GOLF_COURSES_PER_RECORD - 1;
                        }

                        OpenEncodedAsset(
                            g_MiniGolfDiscFrameAssets[course * MINI_GOLF_DISC_IMAGE_COUNT]
                                                     [f_MiniGolfDiscFrame],
                            &asset
                        );
                        ReadEncodedAsset(
                            MENU_ICON_IMAGE_PAIRS[f_MiniGolfDiscBufferIndex ^ 1][0],
                            &asset
                        );
                        OpenEncodedAsset(
                            g_MiniGolfDiscFrameAssets[(course * MINI_GOLF_DISC_IMAGE_COUNT) + 1]
                                                     [f_MiniGolfDiscFrame],
                            &asset
                        );
                        ReadEncodedAsset(
                            MENU_ICON_IMAGE_PAIRS[f_MiniGolfDiscBufferIndex ^ 1][1],
                            &asset
                        );
                    }
                    else {
                        course = g_MenuMemory.miniGolfCourseChoice;
                        buffers = MENU_ICON_IMAGE_PAIRS;
                        OpenEncodedAsset(
                            g_MiniGolfDiscFrameAssets[course * MINI_GOLF_DISC_IMAGE_COUNT]
                                                     [f_MiniGolfDiscFrame],
                            &asset
                        );
                        ReadEncodedAsset(buffers[f_MiniGolfDiscBufferIndex ^ 1][0], &asset);
                        OpenEncodedAsset(
                            g_MiniGolfDiscFrameAssets[(course * MINI_GOLF_DISC_IMAGE_COUNT) + 1]
                                                     [f_MiniGolfDiscFrame],
                            &asset
                        );
                        ReadEncodedAsset(buffers[f_MiniGolfDiscBufferIndex ^ 1][1], &asset);
                    }

                    f_MiniGolfDiscBufferIndex ^= 1;
                    if (f_MiniGolfDiscFrame == MINI_GOLF_FACING_FRAME) {
                        if (--g_MenuMemory.miniGolfCourseChoice < 0) {
                            g_MenuMemory.miniGolfCourseChoice = MINI_GOLF_COURSES_PER_RECORD - 1;
                        }

                        f_MiniGolfDiscTurn = MINI_GOLF_TURN_NONE;
                    }
                }

                break;
            case MINI_GOLF_TURN_TO_NEXT:
                if (++f_MiniGolfDiscFrameTime == MINI_GOLF_TURN_FRAME_DURATION) {
                    f_MiniGolfDiscFrameTime = 0;
                    if (--f_MiniGolfDiscFrame < 0) {
                        f_MiniGolfDiscFrame = MINI_GOLF_TURN_FRAME_COUNT - 1;
                    }

                    if (f_MiniGolfDiscFrame >= MINI_GOLF_FACING_FRAME &&
                        f_MiniGolfDiscFrame < MINI_GOLF_TURN_FRAME_COUNT) {
                        course = g_MenuMemory.miniGolfCourseChoice + 1;
                        if (course >= MINI_GOLF_COURSES_PER_RECORD) {
                            course = 0;
                        }

                        OpenEncodedAsset(
                            g_MiniGolfDiscFrameAssets[course * MINI_GOLF_DISC_IMAGE_COUNT]
                                                     [f_MiniGolfDiscFrame],
                            &asset
                        );
                        ReadEncodedAsset(
                            MENU_ICON_IMAGE_PAIRS[f_MiniGolfDiscBufferIndex ^ 1][0],
                            &asset
                        );
                        OpenEncodedAsset(
                            g_MiniGolfDiscFrameAssets[(course * MINI_GOLF_DISC_IMAGE_COUNT) + 1]
                                                     [f_MiniGolfDiscFrame],
                            &asset
                        );
                        ReadEncodedAsset(
                            MENU_ICON_IMAGE_PAIRS[f_MiniGolfDiscBufferIndex ^ 1][1],
                            &asset
                        );
                    }
                    else {
                        course = g_MenuMemory.miniGolfCourseChoice;
                        buffers = MENU_ICON_IMAGE_PAIRS;
                        OpenEncodedAsset(
                            g_MiniGolfDiscFrameAssets[course * MINI_GOLF_DISC_IMAGE_COUNT]
                                                     [f_MiniGolfDiscFrame],
                            &asset
                        );
                        ReadEncodedAsset(buffers[f_MiniGolfDiscBufferIndex ^ 1][0], &asset);
                        OpenEncodedAsset(
                            g_MiniGolfDiscFrameAssets[(course * MINI_GOLF_DISC_IMAGE_COUNT) + 1]
                                                     [f_MiniGolfDiscFrame],
                            &asset
                        );
                        ReadEncodedAsset(buffers[f_MiniGolfDiscBufferIndex ^ 1][1], &asset);
                    }

                    f_MiniGolfDiscBufferIndex ^= 1;
                    if (f_MiniGolfDiscFrame == MINI_GOLF_FACING_FRAME) {
                        if (++g_MenuMemory.miniGolfCourseChoice >= MINI_GOLF_COURSES_PER_RECORD) {
                            g_MenuMemory.miniGolfCourseChoice = 0;
                        }

                        f_MiniGolfDiscTurn = MINI_GOLF_TURN_NONE;
                    }
                }

                break;
        }

        return;
    }

    if (g_BetPagesPhase == BET_PHASE_CHOOSING) {
        PlaySong(SONG_CONTROLS, 0);
        g_MenuScreen = MENU_SCREEN_BET_SETTINGS;
        g_BetPagesPhase = BET_PHASE_OPENING;
        g_BetPagesWallSlide = STATS_SLIDE_CURRENT;
        return;
    }

    if (g_BetPagesWallSlide != STATS_SLIDE_CURRENT || g_BetPagesPhase != MENU_PHASE_START) {
        return;
    }

    if (GetWipeKind() != WIPE_NONE && !g_IsMenuWipeOpening) {
        if (GetWipeProgress() == WIPE_CLOSED_PROGRESS) {
            g_MenuScreen = MENU_SCREEN_EXIT;
        }

        return;
    }

    isDone = IsHudAnimationDone();
    if (isDone != TRUE) {
        return;
    }

    if (g_MenuPressedButtons & A_BUTTON) {
        if (g_MenuMemory.miniGolfRecordSlide != 0) {
            g_MenuMemory.miniGolfRecordSlide = 0;
            return;
        }

        if (g_MenuMemory.miniGolfRecordChoice < g_MenuCourseCount) {
            g_IsMenuWipeOpening = FALSE;
            FreeAllCharacterSlots();
            LoadWipeShape(
                g_MenuMemory.miniGolfCourseChoice +
                (g_MenuMemory.miniGolfRecordChoice * MINI_GOLF_COURSES_PER_RECORD) +
                MINI_GOLF_COURSE_INDEX
            );
            StartWipe(WIPE_CLOSING);
            g_IsRingShotWipeOpening = FALSE;
        }

        PlaySound(SOUND_MENU_START, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
        return;
    }

    if (g_MenuPressedButtons & B_BUTTON) {
        StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
        g_MenuMemory.miniGolfRecordSlide = 0;
        PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
        g_GameState.isMiniGolf = FALSE;
        g_MenuScreen = MENU_SCREEN_MODE_SELECT;
        COURSE_SELECT_LAYOUT = COURSE_SELECT_LAYOUT_AWARDS;
        g_IsChosenHoleLoadStarted = FALSE;
        StopLoaderThread();
        FreeAllCharacterSlots();
        return;
    }

    if (g_MenuMemory.miniGolfRecordSlide == 0) {
        if (g_MenuRepeatButtons & L_JPAD) {
            PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            if (--g_MenuMemory.miniGolfRecordChoice < 0) {
                g_MenuMemory.miniGolfRecordChoice = MINI_GOLF_RECORD_COUNT - 1;
            }

            g_MenuMemory.miniGolfRecordSlide = MINI_GOLF_RECORD_SLIDE;
        }
        else if (g_MenuRepeatButtons & R_JPAD) {
            PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            if (++g_MenuMemory.miniGolfRecordChoice >= MINI_GOLF_RECORD_COUNT) {
                g_MenuMemory.miniGolfRecordChoice = 0;
            }

            g_MenuMemory.miniGolfRecordSlide = -MINI_GOLF_RECORD_SLIDE;
        }
        else if (g_MenuRepeatButtons & U_JPAD) {
            PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            f_MiniGolfDiscTurn = MINI_GOLF_TURN_TO_PREVIOUS;
        }
        else if (g_MenuRepeatButtons & D_JPAD) {
            PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            f_MiniGolfDiscTurn = MINI_GOLF_TURN_TO_NEXT;
        }
        else if ((g_MenuRepeatButtons & U_CBUTTONS) &&
            COURSE_SELECT_LAYOUT == COURSE_SELECT_LAYOUT_HOLES) {
            if (g_AreBetPagesBlocked != TRUE) {
                PlaySound(SOUND_WALL_SLIDE_OUT, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
                g_BetPagesPhase = BET_PHASE_OPENING;
                g_BetPagesWallSlide = STATS_SLIDE_OUT_RIGHT;
            }
            else {
                PlaySound(SOUND_MENU_BLOCKED, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
            }
        }
        else if ((g_MenuRepeatButtons & D_CBUTTONS) &&
            COURSE_SELECT_LAYOUT == COURSE_SELECT_LAYOUT_HOLES) {
            PlaySound(SOUND_SLICE, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
            g_MenuMemory.holesChoice = HOLES_CHOICE_ALL;
            g_GameState.holeSelection = HOLE_SELECTION_ALL;
            StartLoadingChosenHole();
        }
        else if ((g_MenuRepeatButtons & L_CBUTTONS) &&
            COURSE_SELECT_LAYOUT == COURSE_SELECT_LAYOUT_HOLES) {
            PlaySound(SOUND_SLICE, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
            g_MenuMemory.holesChoice = HOLES_CHOICE_FRONT_NINE;
            g_GameState.holeSelection = HOLE_SELECTION_FRONT_NINE;
            StartLoadingChosenHole();
        }
        else if ((g_MenuRepeatButtons & R_CBUTTONS) &&
            COURSE_SELECT_LAYOUT == COURSE_SELECT_LAYOUT_HOLES) {
            PlaySound(SOUND_SLICE, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
            g_MenuMemory.holesChoice = HOLES_CHOICE_BACK_NINE;
            g_GameState.holeSelection = HOLE_SELECTION_BACK_NINE;
            StartLoadingChosenHole();
        }
    }

    if (Abs(g_MenuMemory.miniGolfRecordSlide) != 0) {
        direction = g_MenuMemory.miniGolfRecordSlide >= 0 ? 1 : -1;
        step = MINI_GOLF_RECORD_SLIDE_STEP;
        positiveSign = 1;
        g_MenuMemory.miniGolfRecordSlide -= direction * step;

        if ((g_MenuMemory.miniGolfRecordSlide >= 0 ? positiveSign : -positiveSign) != direction) {
            g_MenuMemory.miniGolfRecordSlide = 0;
        }
    }
}

void UpdateUnusedStrokeStart(void) {
    if (IsHudAnimationDone() == TRUE) {
        if (g_MenuPressedButtons & A_BUTTON) {
            PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
            g_MenuScreen = MENU_SCREEN_EXIT;
            g_GameState.gameMode = GAME_MODE_STROKE;
            SaveOrContinueGameByMode(TRUE);
            return;
        }

        if (g_MenuPressedButtons & B_BUTTON) {
            PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
            g_MenuScreen = MENU_SCREEN_TITLE;
        }
    }
}

void UpdateMainMenu(void) {
    Boolean isDone = IsHudAnimationDone();

    if (isDone == TRUE) {
        if (g_MenuPressedButtons & A_BUTTON) {
            switch (g_MenuMemory.mainChoice) {
                case MAIN_CHOICE_CLUBHOUSE:
                    g_MenuScreen = MENU_SCREEN_CLUBHOUSE;
                    break;
                case MAIN_CHOICE_CLUB_TOURNAMENT:
                    StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                    ResetCodeEntry();
                    g_MenuScreen = MENU_SCREEN_CLUB_TOURNAMENT;
                    break;
                case MAIN_CHOICE_HELP:
                    StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                    g_MenuScreen = MENU_SCREEN_HELP;
                    if (IsGameFlagSet(GAME_FLAG_HELP_ANIMATION_VIEWER)) {
                        g_GameState.gameMode = GAME_MODE_GET_CHARACTER;
                        g_GameState.playerCount = 2;
                        g_CourseSelectKind = COURSE_SELECT_KIND_COURSES;
                        COURSE_SELECT_LAYOUT = COURSE_SELECT_LAYOUT_PLAIN;
                        g_BetSet = BET_SET_NONE;
                        ApplyBetPreset(BET_PRESET_NONE);
                    }

                    break;
                case MAIN_CHOICE_FULL_OPTIONS:
                    StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                    g_MenuScreen = MENU_SCREEN_OPTIONS;
                    break;
            }

            PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
            return;
        }

        if (g_MenuPressedButtons & B_BUTTON) {
            PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
            g_MenuScreen = MENU_SCREEN_TITLE;
            PlaySong(SONG_TITLE, 0);
            return;
        }

        if (g_MenuRepeatButtons & U_JPAD) {
            PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            g_MenuMemory.mainChoice--;
        }
        else if (g_MenuRepeatButtons & D_JPAD) {
            PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            g_MenuMemory.mainChoice++;
        }

        if (g_MenuMemory.mainChoice >= MAIN_CHOICE_COUNT) {
            g_MenuMemory.mainChoice = 0;
        }

        if (g_MenuMemory.mainChoice < 0) {
            g_MenuMemory.mainChoice = MAIN_CHOICE_COUNT - 1;
        }
    }
}

void UpdateMainMenuWithoutTransmit(void) {
    Boolean isDone = IsHudAnimationDone();

    if (isDone == TRUE) {
        if (g_MenuPressedButtons & A_BUTTON) {
            switch (g_MenuMemory.mainChoiceWithoutTransmit) {
                case MAIN_CHOICE_CLUBHOUSE:
                    g_MenuScreen = MENU_SCREEN_CLUBHOUSE;
                    break;
                case MAIN_CHOICE_HELP:
                    StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                    g_MenuScreen = MENU_SCREEN_HELP;
                    if (IsGameFlagSet(GAME_FLAG_HELP_ANIMATION_VIEWER)) {
                        g_GameState.gameMode = GAME_MODE_GET_CHARACTER;
                        g_GameState.playerCount = 2;
                        g_CourseSelectKind = COURSE_SELECT_KIND_COURSES;
                        COURSE_SELECT_LAYOUT = COURSE_SELECT_LAYOUT_PLAIN;
                        g_BetSet = BET_SET_NONE;
                        ApplyBetPreset(BET_PRESET_NONE);
                    }

                    break;
                case MAIN_CHOICE_OPTIONS:
                    StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                    g_MenuScreen = MENU_SCREEN_OPTIONS;
                    break;
            }

            PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
            return;
        }

        if (g_MenuPressedButtons & B_BUTTON) {
            PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
            g_MenuScreen = MENU_SCREEN_TITLE;
            PlaySong(SONG_TITLE, 0);
            return;
        }

        if (g_MenuRepeatButtons & U_JPAD) {
            PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            g_MenuMemory.mainChoiceWithoutTransmit--;
        }
        else if (g_MenuRepeatButtons & D_JPAD) {
            PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            g_MenuMemory.mainChoiceWithoutTransmit++;
        }

        if (g_MenuMemory.mainChoiceWithoutTransmit >= MAIN_CHOICE_WITHOUT_TRANSMIT_COUNT) {
            g_MenuMemory.mainChoiceWithoutTransmit = 0;
        }

        if (g_MenuMemory.mainChoiceWithoutTransmit < 0) {
            g_MenuMemory.mainChoiceWithoutTransmit = MAIN_CHOICE_WITHOUT_TRANSMIT_COUNT - 1;
        }
    }
}

void UpdateClubhouse(void) {
    Boolean isDone = IsHudAnimationDone();

    if (isDone == TRUE) {
        if (g_MenuPressedButtons & A_BUTTON) {
            switch (f_ClubhouseRooms[g_MenuMemory.clubhouseChoice]) {
                case CLUBHOUSE_ROOM_COURSE_STATS:
                    StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                    g_MenuScreen = MENU_SCREEN_COURSE_STATS;
                    break;
                case CLUBHOUSE_ROOM_BEST_SHOTS:
                    StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                    g_MenuScreen = MENU_SCREEN_BEST_SHOTS;
                    break;
                case CLUBHOUSE_ROOM_RING_SHOT:
                    StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                    g_MenuScreen = MENU_SCREEN_RING_SHOT_RECORDS;
                    break;
                case CLUBHOUSE_ROOM_BIRDIE_BADGES:
                    StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                    g_MenuScreen = MENU_SCREEN_BIRDIE_BADGES;
                    break;
                case CLUBHOUSE_ROOM_CHARACTER_TABLE:
                    StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                    g_MenuScreen = MENU_SCREEN_CHARACTER_TABLE;
                    break;
                case CLUBHOUSE_ROOM_BEST_TIMES:
                    StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                    g_MenuScreen = MENU_SCREEN_BEST_TIMES;
                    break;
            }

            PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
            return;
        }

        if (g_MenuPressedButtons & B_BUTTON) {
            PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
            if (g_MenuMemory.isTransmitShown == isDone) {
                g_MenuScreen = MENU_SCREEN_MAIN;
            }
            else {
                g_MenuScreen = MENU_SCREEN_MAIN_WITHOUT_TRANSMIT;
            }

            PlaySong(SONG_TITLE, 0);
            return;
        }

        if (g_MenuRepeatButtons & U_JPAD) {
            PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            g_MenuMemory.clubhouseChoice--;
        }
        else if (g_MenuRepeatButtons & D_JPAD) {
            PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            g_MenuMemory.clubhouseChoice++;
        }

        if (g_MenuMemory.clubhouseChoice >= CLUBHOUSE_CHOICE_COUNT) {
            g_MenuMemory.clubhouseChoice = 0;
        }

        if (g_MenuMemory.clubhouseChoice < 0) {
            g_MenuMemory.clubhouseChoice = CLUBHOUSE_CHOICE_COUNT - 1;
        }
    }
}

void UpdateUnusedBackToMain(void) {
    if (IsHudAnimationDone() == TRUE && (g_MenuPressedButtons & B_BUTTON)) {
        PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
        g_MenuScreen = MENU_SCREEN_MAIN;
    }
}

#define OPTIONS_TEXT_SLOT_COUNT 11
#define ERASE_DECLINED_SOUND_CHANNEL 13
#define ERASE_CHOICE_YES 0
#define ERASE_CHOICE_NO 1
#define ERASE_CHOICE_COUNT 2
#define SOUND_TEST_COARSE_STEP 10
#define SOUND_TEST_LAST_SOUND 0x214
#define SOUND_TEST_REVERB_MAX 0x7F
#define SOUND_TEST_TEMPO_MAX 0xFF
#define SOUND_TEST_FREQUENCY_FINE_STEP 0.1f
#define SOUND_TEST_FREQUENCY_COARSE_STEP 1.0f
#define SOUND_TEST_FREQUENCY_LIMIT 12.0f

void UpdateOptionsScreen(void) {
    SaveData *save = GetSaveData();
    MenuMemory *memory = GetMenuMemory();
    MenuWindowCheck closedCheck;
    s32 slot;
    u8 *optionBits;

    if (IsHudAnimationDone() != TRUE) {
        return;
    }

    if (f_IsEraseWindowUp) {
        closedCheck = IsMenuWindowClosed();
        for (slot = 0; slot != OPTIONS_TEXT_SLOT_COUNT; slot++) {
            SetTextSlot(
                0,
                0,
                NO_MESSAGE,
                TEXT_SLOT_LARGE_FONT,
                slot,
                0,
                0xFF,
                0xFF,
                0xFF,
                OPAQUE_ALPHA
            );
        }

        if (closedCheck == MENU_WINDOW_CHECK_MOVING) {
            return;
        }

        if (closedCheck == MENU_WINDOW_CHECK_YES) {
            f_IsEraseWindowUp = FALSE;
            for (slot = 0; slot != OPTIONS_TEXT_SLOT_COUNT; slot++) {
                SetTextSlot(
                    0,
                    0,
                    NO_MESSAGE,
                    TEXT_SLOT_LARGE_FONT,
                    slot,
                    0,
                    0xFF,
                    0xFF,
                    0xFF,
                    OPAQUE_ALPHA
                );
            }

            return;
        }

        if (g_MenuPressedButtons & A_BUTTON) {
            if (memory->eraseChoice == ERASE_CHOICE_YES) {
                PlaySound(SOUND_CAPTION_MEET, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
                ResetSaveData();
                memory->optionBits = 0;
                g_SoundOptionBits = 0;
                MusSetStereo(TRUE);
            }
            else {
                PlaySound(SOUND_READY, ERASE_DECLINED_SOUND_CHANNEL, MENU_VOLUME_SOFT);
            }

            CloseMenuWindow();
            for (slot = 0; slot != OPTIONS_TEXT_SLOT_COUNT; slot++) {
                SetTextSlot(
                    0,
                    0,
                    NO_MESSAGE,
                    TEXT_SLOT_LARGE_FONT,
                    slot,
                    0,
                    0xFF,
                    0xFF,
                    0xFF,
                    OPAQUE_ALPHA
                );
            }
        }
        else if (g_MenuPressedButtons & B_BUTTON) {
            PlaySound(SOUND_READY, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
            CloseMenuWindow();
            for (slot = 0; slot != OPTIONS_TEXT_SLOT_COUNT; slot++) {
                SetTextSlot(
                    0,
                    0,
                    NO_MESSAGE,
                    TEXT_SLOT_LARGE_FONT,
                    slot,
                    0,
                    0xFF,
                    0xFF,
                    0xFF,
                    OPAQUE_ALPHA
                );
            }
        }
        else if (g_MenuRepeatButtons & L_JPAD) {
            PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            memory->eraseChoice--;
            if (memory->eraseChoice < 0) {
                memory->eraseChoice = ERASE_CHOICE_COUNT - 1;
            }
        }
        else if (g_MenuRepeatButtons & R_JPAD) {
            PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            memory->eraseChoice++;
            if (memory->eraseChoice >= ERASE_CHOICE_COUNT) {
                memory->eraseChoice = 0;
            }
        }
    }
    else if (g_MenuPressedButtons & A_BUTTON) {
        switch (g_MenuMemory.optionsChoice) {
            case OPTIONS_CHOICE_MUSIC:
            case OPTIONS_CHOICE_MONO:
            case OPTIONS_CHOICE_CAMERA:
                break;
            case OPTIONS_CHOICE_ERASE_SAVE_DATA:
                PlaySound(SOUND_GO, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
                f_IsEraseWindowUp = TRUE;
                memory->eraseChoice = ERASE_CHOICE_NO;
                ResetMenuWindow();
                break;
            case OPTIONS_CHOICE_SONG:
            case OPTIONS_CHOICE_SONG_REVERB:
            case OPTIONS_CHOICE_SONG_FREQUENCY:
            case OPTIONS_CHOICE_SONG_TEMPO:
                if (f_SoundTestSong == NO_SONG) {
                    RequestSongStop();
                }
                else {
                    PlaySong(f_SoundTestSong, SONG_FLAG_RESTART);
                }

                break;
            case OPTIONS_CHOICE_SOUND:
            case OPTIONS_CHOICE_SOUND_REVERB:
            case OPTIONS_CHOICE_SOUND_FREQUENCY:
                RequestSongStop();
                PlaySound(f_SoundTestSound, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
                SetSoundReverb(MENU_SOUND_CHANNEL, f_SoundTestSoundReverb);
                SetSoundFrequencyOffset(MENU_SOUND_CHANNEL, f_SoundTestSoundFrequency);
                break;
        }
    }
    else if (g_MenuPressedButtons & B_BUTTON) {
        PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
        TransferSaveData(save, TRUE);
        if (g_MenuMemory.isTransmitShown) {
            g_MenuScreen = MENU_SCREEN_MAIN;
        }
        else {
            g_MenuScreen = MENU_SCREEN_MAIN_WITHOUT_TRANSMIT;
        }

        PlaySong(SONG_TITLE, 0);
    }
    else {
        if (g_MenuRepeatButtons & U_JPAD) {
            g_MenuMemory.optionsChoice--;
            PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
        }
        else if (g_MenuRepeatButtons & D_JPAD) {
            g_MenuMemory.optionsChoice++;
            PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
        }

        if (IsGameFlagSet(GAME_FLAG_OPTIONS_SOUND_TEST)) {
            if (g_MenuMemory.optionsChoice >= OPTIONS_SOUND_TEST_CHOICE_COUNT) {
                g_MenuMemory.optionsChoice = 0;
            }

            if (g_MenuMemory.optionsChoice < 0) {
                g_MenuMemory.optionsChoice = OPTIONS_SOUND_TEST_CHOICE_COUNT - 1;
            }
        }
        else {
            if (g_MenuMemory.optionsChoice >= OPTIONS_CHOICE_COUNT) {
                g_MenuMemory.optionsChoice = 0;
            }

            if (g_MenuMemory.optionsChoice < 0) {
                g_MenuMemory.optionsChoice = OPTIONS_CHOICE_COUNT - 1;
            }
        }

        switch (g_MenuMemory.optionsChoice) {
            case OPTIONS_CHOICE_MUSIC:
                if (g_MenuRepeatButtons & (L_JPAD | R_JPAD)) {
                    PlaySound(SOUND_OPTION_TOGGLED, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
                    optionBits = &g_MenuMemory.optionBits;
                    *optionBits ^= OPTION_BIT_MUSIC_OFF;
                }

                break;
            case OPTIONS_CHOICE_MONO:
                if (g_MenuRepeatButtons & (L_JPAD | R_JPAD)) {
                    PlaySound(SOUND_OPTION_TOGGLED, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
                    optionBits = &g_MenuMemory.optionBits;
                    *optionBits ^= OPTION_BIT_MONO;
                    g_SoundOptionBits = *optionBits;
                    if (*optionBits & OPTION_BIT_MONO) {
                        MusSetStereo(FALSE);
                    }
                    else {
                        MusSetStereo(TRUE);
                    }
                }

                break;
            case OPTIONS_CHOICE_CAMERA:
                if (g_MenuRepeatButtons & (L_JPAD | R_JPAD)) {
                    PlaySound(SOUND_OPTION_TOGGLED, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
                    optionBits = &g_MenuMemory.optionBits;
                    *optionBits ^= OPTION_BIT_SIMPLE_CAMERA;
                }

                break;
            case OPTIONS_CHOICE_SONG:
                if (g_MenuRepeatButtons & L_JPAD) {
                    f_SoundTestSong--;
                }

                if (g_MenuRepeatButtons & R_JPAD) {
                    f_SoundTestSong++;
                }

                if (g_MenuRepeatButtons & L_CBUTTONS) {
                    f_SoundTestSong -= SOUND_TEST_COARSE_STEP;
                }

                if (g_MenuRepeatButtons & R_CBUTTONS) {
                    f_SoundTestSong += SOUND_TEST_COARSE_STEP;
                }

                if (f_SoundTestSong < NO_SONG) {
                    f_SoundTestSong = NO_SONG;
                }

                break;
            case OPTIONS_CHOICE_SONG_REVERB:
                if (g_MenuRepeatButtons & L_JPAD) {
                    f_SoundTestSongReverb--;
                }

                if (g_MenuRepeatButtons & R_JPAD) {
                    f_SoundTestSongReverb++;
                }

                if (g_MenuRepeatButtons & L_CBUTTONS) {
                    f_SoundTestSongReverb -= SOUND_TEST_COARSE_STEP;
                }

                if (g_MenuRepeatButtons & R_CBUTTONS) {
                    f_SoundTestSongReverb += SOUND_TEST_COARSE_STEP;
                }

                if (f_SoundTestSongReverb < 0) {
                    f_SoundTestSongReverb = 0;
                }

                if (f_SoundTestSongReverb > SOUND_TEST_REVERB_MAX) {
                    f_SoundTestSongReverb = SOUND_TEST_REVERB_MAX;
                }

                break;
            case OPTIONS_CHOICE_SONG_FREQUENCY:
                if (g_MenuRepeatButtons & L_JPAD) {
                    f_SoundTestSongFrequency -= SOUND_TEST_FREQUENCY_FINE_STEP;
                }

                if (g_MenuRepeatButtons & R_JPAD) {
                    f_SoundTestSongFrequency += SOUND_TEST_FREQUENCY_FINE_STEP;
                }

                if (g_MenuRepeatButtons & L_CBUTTONS) {
                    f_SoundTestSongFrequency -= SOUND_TEST_FREQUENCY_COARSE_STEP;
                }

                if (g_MenuRepeatButtons & R_CBUTTONS) {
                    f_SoundTestSongFrequency += SOUND_TEST_FREQUENCY_COARSE_STEP;
                }

                if (f_SoundTestSongFrequency < -SOUND_TEST_FREQUENCY_LIMIT) {
                    f_SoundTestSongFrequency = -SOUND_TEST_FREQUENCY_LIMIT;
                }

                if (f_SoundTestSongFrequency > SOUND_TEST_FREQUENCY_LIMIT) {
                    f_SoundTestSongFrequency = SOUND_TEST_FREQUENCY_LIMIT;
                }

                break;
            case OPTIONS_CHOICE_SONG_TEMPO:
                if ((g_MenuRepeatButtons & L_JPAD) && f_SoundTestSongTempo > 0) {
                    f_SoundTestSongTempo--;
                }

                if ((g_MenuRepeatButtons & R_JPAD) &&
                    f_SoundTestSongTempo <= SOUND_TEST_TEMPO_MAX) {
                    f_SoundTestSongTempo++;
                }

                if ((g_MenuRepeatButtons & L_CBUTTONS) && f_SoundTestSongTempo > 0) {
                    f_SoundTestSongTempo -= SOUND_TEST_COARSE_STEP;
                }

                if ((g_MenuRepeatButtons & R_CBUTTONS) &&
                    f_SoundTestSongTempo <= SOUND_TEST_TEMPO_MAX) {
                    f_SoundTestSongTempo += SOUND_TEST_COARSE_STEP;
                }

                if (f_SoundTestSongTempo < 0) {
                    f_SoundTestSongTempo = 0;
                }

                if (f_SoundTestSongTempo > SOUND_TEST_TEMPO_MAX) {
                    f_SoundTestSongTempo = SOUND_TEST_TEMPO_MAX;
                }

                break;
            case OPTIONS_CHOICE_SOUND:
                if (g_MenuRepeatButtons & L_JPAD) {
                    f_SoundTestSound--;
                }

                if (g_MenuRepeatButtons & R_JPAD) {
                    f_SoundTestSound++;
                }

                if (g_MenuRepeatButtons & L_CBUTTONS) {
                    f_SoundTestSound -= SOUND_TEST_COARSE_STEP;
                }

                if (g_MenuRepeatButtons & R_CBUTTONS) {
                    f_SoundTestSound += SOUND_TEST_COARSE_STEP;
                }

                if (f_SoundTestSound < 0) {
                    f_SoundTestSound = 0;
                }

                if (f_SoundTestSound > SOUND_TEST_LAST_SOUND) {
                    f_SoundTestSound = SOUND_TEST_LAST_SOUND;
                }

                break;
            case OPTIONS_CHOICE_SOUND_REVERB:
                if (g_MenuRepeatButtons & L_JPAD) {
                    f_SoundTestSoundReverb--;
                }

                if (g_MenuRepeatButtons & R_JPAD) {
                    f_SoundTestSoundReverb++;
                }

                if (g_MenuRepeatButtons & L_CBUTTONS) {
                    f_SoundTestSoundReverb -= SOUND_TEST_COARSE_STEP;
                }

                if (g_MenuRepeatButtons & R_CBUTTONS) {
                    f_SoundTestSoundReverb += SOUND_TEST_COARSE_STEP;
                }

                if (f_SoundTestSoundReverb < 0) {
                    f_SoundTestSoundReverb = 0;
                }

                if (f_SoundTestSoundReverb > SOUND_TEST_REVERB_MAX) {
                    f_SoundTestSoundReverb = SOUND_TEST_REVERB_MAX;
                }

                break;
            case OPTIONS_CHOICE_SOUND_FREQUENCY:
                if (g_MenuRepeatButtons & L_JPAD) {
                    f_SoundTestSoundFrequency -= SOUND_TEST_FREQUENCY_FINE_STEP;
                }

                if (g_MenuRepeatButtons & R_JPAD) {
                    f_SoundTestSoundFrequency += SOUND_TEST_FREQUENCY_FINE_STEP;
                }

                if (g_MenuRepeatButtons & L_CBUTTONS) {
                    f_SoundTestSoundFrequency -= SOUND_TEST_FREQUENCY_COARSE_STEP;
                }

                if (g_MenuRepeatButtons & R_CBUTTONS) {
                    f_SoundTestSoundFrequency += SOUND_TEST_FREQUENCY_COARSE_STEP;
                }

                if (f_SoundTestSoundFrequency < -SOUND_TEST_FREQUENCY_LIMIT) {
                    f_SoundTestSoundFrequency = -SOUND_TEST_FREQUENCY_LIMIT;
                }

                if (f_SoundTestSoundFrequency > SOUND_TEST_FREQUENCY_LIMIT) {
                    f_SoundTestSoundFrequency = SOUND_TEST_FREQUENCY_LIMIT;
                }

                break;
        }
    }

    SetSongReverb(f_SoundTestSongReverb);
    SetSongTempo(f_SoundTestSongTempo);
    SetSongFrequencyOffset(f_SoundTestSongFrequency);
}

void UpdateCaddySelect(void) {
    if (IsHudAnimationDone() != TRUE) {
        return;
    }

    if (GetWipeKind() != WIPE_NONE) {
        if (GetWipeProgress() == WIPE_CLOSED_PROGRESS) {
            g_MenuScreen = MENU_SCREEN_EXIT;
        }

        return;
    }

    if (g_MenuPressedButtons & A_BUTTON) {
        PlaySound(SOUND_MENU_START, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
        StartWipe(WIPE_CLOSING);
        return;
    }

    if (g_MenuPressedButtons & B_BUTTON) {
        PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
        if (g_MenuMemory.modeChoice == MENU_SCREEN_MODE_SELECT) {
            g_MenuScreen = g_MenuMemory.modeChoice;
        }
        else {
            g_MenuScreen = MENU_SCREEN_COURSE_SELECT;
        }

        return;
    }

    if ((g_MenuRepeatButtons & U_JPAD) && g_MenuMemory.caddyChoice > 0) {
        PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
        g_MenuMemory.caddyChoice--;
    }
    else if ((g_MenuRepeatButtons & D_JPAD) && g_MenuMemory.caddyChoice < CADDY_COUNT - 1) {
        PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
        g_MenuMemory.caddyChoice++;
    }
}

void LeaveMenuWithWipe(void) {
    if (GetWipeKind() != WIPE_NONE && !g_IsMenuWipeOpening) {
        if (GetWipeProgress() == WIPE_CLOSED_PROGRESS) {
            g_MenuScreen = MENU_SCREEN_EXIT;
        }
    }
    else {
        LoadWipeShape(0);
        StartWipe(WIPE_CLOSING);
        g_IsRingShotWipeOpening = FALSE;
        g_IsMenuWipeOpening = FALSE;
    }
}

void LeaveMenuWithCourseWipe(void) {
    if (GetWipeKind() != WIPE_NONE && !g_IsMenuWipeOpening) {
        if (GetWipeProgress() == WIPE_CLOSED_PROGRESS) {
            g_MenuScreen = MENU_SCREEN_EXIT;
        }
    }
    else {
        LoadWipeShape(g_MenuMemory.courseIndex);
        StartWipe(WIPE_CLOSING);
        g_IsRingShotWipeOpening = FALSE;
        g_IsMenuWipeOpening = FALSE;
    }
}

void UpdateUnusedList(void) {
    GetSaveData();
    if (g_MenuPressedButtons & A_BUTTON) {
        PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
        g_MenuScreen = MENU_SCREEN_EXIT;
        g_IsChosenHoleLoadStarted = FALSE;
        g_GameState.replayKind = REPLAY_KIND_RING_SHOT;
        return;
    }

    if (g_MenuPressedButtons & B_BUTTON) {
        PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
        if (g_MenuMemory.isTransmitShown) {
            g_MenuScreen = MENU_SCREEN_MAIN;
        }
        else {
            g_MenuScreen = MENU_SCREEN_MAIN_WITHOUT_TRANSMIT;
        }

        return;
    }

    if (g_MenuRepeatButtons & U_JPAD) {
        f_UnusedListChoice--;
        PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
    }
    else if (g_MenuRepeatButtons & D_JPAD) {
        f_UnusedListChoice++;
        PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
    }

    if (f_UnusedListChoice < 0) {
        f_UnusedListChoice = UNUSED_LIST_CHOICE_COUNT - 1;
    }

    if (f_UnusedListChoice >= UNUSED_LIST_CHOICE_COUNT) {
        f_UnusedListChoice = 0;
    }
}

void UpdateBirdieBadges(void) {
    if (g_MenuPressedButtons & B_BUTTON) {
        PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
        g_MenuScreen = MENU_SCREEN_MAIN;
    }
}

void UpdateBlankScreen(void) {
    if (g_MenuPressedButtons & A_BUTTON) {
        PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
        g_MenuScreen = MENU_SCREEN_TITLE;
    }
}

void DrawMenuScreen(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;

    g_ModuleRdpRequestFrames[MODULE_MENU] = g_FrameCount;
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_NOOP, G_RM_NOOP2);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_FILL);
    gDPSetFillColor(gfx++, FillColor16(0, 0, 0, 1));
    gDPFillRectangle(
        gfx++,
        SCREEN_BORDER,
        SCREEN_BORDER,
        SCREEN_WIDTH - SCREEN_BORDER - 1,
        SCREEN_HEIGHT - SCREEN_BORDER - 1
    );
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    SetUpShadowedTextDrawing(&gfx);
    SetTextColor(&gfx, 255, 255, 255);
    f_MenuBobPhase += MENU_BOB_PHASE_STEP;
    f_MenuBobOffset = sinf(f_MenuBobPhase) * MENU_BOB_HEIGHT;
    switch (g_MenuScreen) {
        case MENU_SCREEN_TITLE:
            SetTextSlot(
                0,
                0,
                NO_MESSAGE,
                TEXT_SLOT_LARGE_FONT,
                0,
                TEXT_FLAG_TYPED,
                255,
                255,
                255,
                OPAQUE_ALPHA
            );
            DrawMenuTitleScreen(&gfx);
            break;
        case MENU_SCREEN_CHARACTER_SELECT:
        case MENU_SCREEN_TOURNAMENT_CHARACTER_SELECT:
            DrawCharacterSelect(&gfx, 0);
            break;
        case MENU_SCREEN_OPPONENT_SELECT:
            DrawCharacterSelect(&gfx, 1);
            break;
        case MENU_SCREEN_SAVED_GAMES:
            DrawSavedGames(&gfx);
            break;
        case MENU_SCREEN_MAIN:
            DrawMainMenu(&gfx);
            break;
        case MENU_SCREEN_MAIN_WITHOUT_TRANSMIT:
            DrawMainMenuWithoutTransmit(&gfx);
            break;
        case MENU_SCREEN_CLUB_TOURNAMENT:
            DrawClubTournament(&gfx);
            break;
        case MENU_SCREEN_CLUBHOUSE:
            DrawClubhouse(&gfx);
            break;
        case MENU_SCREEN_OPTIONS:
            DrawOptionsScreen(&gfx);
            break;
        case MENU_SCREEN_HELP:
            if (IsGameFlagSet(GAME_FLAG_HELP_ANIMATION_VIEWER)) {
                DrawAnimationViewer(&gfx);
            }
            else {
                DrawHelpBoard(&gfx);
            }

            break;
        case MENU_SCREEN_CADDY_SELECT:
            DrawCaddySelect(&gfx);
            break;
        case MENU_SCREEN_COURSE_SELECT:
        case MENU_SCREEN_HANDICAPS:
        case MENU_SCREEN_BET_SETTINGS:
            DrawCourseSelect(&gfx);
            break;
        case MENU_SCREEN_BLANK:
            DrawBlankScreen(&gfx);
            break;
        case MENU_SCREEN_BEST_SHOTS:
            DrawBestShots(&gfx);
            break;
        case MENU_SCREEN_RING_SHOT_RECORDS:
            DrawRingShotRecords(&gfx);
            break;
        case MENU_SCREEN_COURSE_STATS:
            DrawCourseStats(&gfx);
            break;
        case MENU_SCREEN_BEST_TIMES:
            DrawBestTimes(&gfx);
            break;
        case MENU_SCREEN_BIRDIE_BADGES:
            DrawBirdieBadgeLabel(&gfx);
            break;
        case MENU_SCREEN_CHARACTER_TABLE:
            DrawCharacterTable(&gfx);
            break;
        case MENU_SCREEN_MODE_SELECT:
        case MENU_SCREEN_RING_SHOT_EXIT:
        case MENU_SCREEN_TRAINING_EXIT:
            DrawModeSelect(&gfx);
            break;
    }

    SetUpShadowedTextDrawing(&gfx);
    SetTextColor(&gfx, 255, 255, 255);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

#define CLAMPED_COURSE_SCENE_STRIP_COUNT 4
#define CLAMPED_COURSE_SCENE_LAST_TOP \
    (COURSE_SCENE_TOP + \
        ((CLAMPED_COURSE_SCENE_STRIP_COUNT - 1) * COURSE_SCENE_STRIP_ROW_COUNT * \
            COURSE_SCENE_SCALE))
#define CLAMPED_COURSE_SCENE_BOTTOM 162

#define BOARD_WIDTH 256
#define BOARD_STRIP_HEIGHT 8
#define BOARD_STRIP_COUNT 20
#define BOARD_STRIP_SPACING 9
#define BOARD_LEFT 32
#define BOARD_TOP 44
#define TALL_NARROW_BOARD_LEFT 16

#define BANNER_BOB_LOWERING 13.0f
#define BANNER_BOB_RAISING 4.0f
#define BOBBING_BANNER_TOP 8
#define BOBBING_BANNER_DROP (MENU_BANNER_TOP - BOBBING_BANNER_TOP)
#define IsBannerBobScreen(screen) \
    ((screen) == MENU_SCREEN_TITLE || \
        (screen) == MENU_SCREEN_OPTIONS || \
        (screen) == MENU_SCREEN_MAIN || \
        (screen) == MENU_SCREEN_MAIN_WITHOUT_TRANSMIT || \
        (screen) == MENU_SCREEN_CLUBHOUSE)

void DrawMenuBackdropLayers(Gfx **displayListHead, s32 layers) {
    Gfx *gfx = *displayListHead;
    s32 stripIndex;
    s32 shownRowCount;
    s32 stripTop;
    s32 bobOffset;
    u8 *image;
    s32 loadedRowCount;

    GetMenuMemory();

    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal256(gfx++, TextureAddress(ImagePalette(g_MenuCourseSceneImage)));
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);

    if (layers & MENU_LAYER_COURSE_SCENE) {
        if (g_MenuScreen == MENU_SCREEN_COURSE_SELECT ||
            g_MenuScreen == MENU_SCREEN_BET_SETTINGS ||
            g_MenuScreen == MENU_SCREEN_HANDICAPS) {
            for (stripIndex = 0; stripIndex != CLAMPED_COURSE_SCENE_STRIP_COUNT; stripIndex++) {
                if (stripIndex == 0) {
                    gDPLoadTextureBlock(
                        gfx++,
                        TextureAddress(LargeImageTexels(g_MenuCourseSceneImage)),
                        G_IM_FMT_CI,
                        G_IM_SIZ_8b,
                        COURSE_SCENE_WIDTH,
                        COURSE_SCENE_STRIP_ROW_COUNT + COURSE_SCENE_SEAM_ROW_COUNT,
                        0,
                        G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMASK,
                        G_TX_NOMASK,
                        G_TX_NOLOD,
                        G_TX_NOLOD
                    );
                    gDPPipeSync(gfx++);

                    gSPScisTextureRectangle(
                        gfx++,
                        QuarterPixels(COURSE_SCENE_LEFT),
                        QuarterPixels(COURSE_SCENE_FIRST_TOP),
                        QuarterPixels(
                            COURSE_SCENE_LEFT + (COURSE_SCENE_WIDTH * COURSE_SCENE_SCALE)
                        ),
                        QuarterPixels(
                            COURSE_SCENE_FIRST_TOP +
                            ((COURSE_SCENE_STRIP_ROW_COUNT + 1) * COURSE_SCENE_SCALE)
                        ),
                        G_TX_RENDERTILE,
                        0,
                        0,
                        TEXEL_STEP_ONE / COURSE_SCENE_SCALE,
                        TEXEL_STEP_ONE / COURSE_SCENE_SCALE
                    );
                }
                else {
                    gDPPipeSync(gfx++);
                    loadedRowCount = COURSE_SCENE_STRIP_ROW_COUNT + COURSE_SCENE_SEAM_ROW_COUNT;
                    gDPLoadTextureBlock(
                        gfx++,
                        TextureAddress(LargeImageTexelsAt(
                            g_MenuCourseSceneImage,
                            stripIndex * COURSE_SCENE_WIDTH * COURSE_SCENE_STRIP_ROW_COUNT
                        )),
                        G_IM_FMT_CI,
                        G_IM_SIZ_8b,
                        COURSE_SCENE_WIDTH,
                        loadedRowCount,
                        0,
                        G_TX_NOMIRROR | G_TX_CLAMP,
                        G_TX_NOMIRROR | G_TX_CLAMP,
                        G_TX_NOMASK,
                        G_TX_NOMASK,
                        G_TX_NOLOD,
                        G_TX_NOLOD
                    );
                    gDPPipeSync(gfx++);

                    if (stripIndex == CLAMPED_COURSE_SCENE_STRIP_COUNT - 1) {
                        gSPScisTextureRectangle(
                            gfx++,
                            QuarterPixels(COURSE_SCENE_LEFT),
                            QuarterPixels(CLAMPED_COURSE_SCENE_LAST_TOP),
                            QuarterPixels(
                                COURSE_SCENE_LEFT + (COURSE_SCENE_WIDTH * COURSE_SCENE_SCALE)
                            ),
                            QuarterPixels(CLAMPED_COURSE_SCENE_BOTTOM),
                            G_TX_RENDERTILE,
                            0,
                            COURSE_SCENE_SECOND_ROW_START,
                            TEXEL_STEP_ONE / COURSE_SCENE_SCALE,
                            TEXEL_STEP_ONE / COURSE_SCENE_SCALE
                        );
                    }
                    else {
                        stripTop = COURSE_SCENE_TOP +
                            (stripIndex * COURSE_SCENE_STRIP_ROW_COUNT * COURSE_SCENE_SCALE);
                        gSPScisTextureRectangle(
                            gfx++,
                            QuarterPixels(COURSE_SCENE_LEFT),
                            QuarterPixels(stripTop),
                            QuarterPixels(
                                COURSE_SCENE_LEFT + (COURSE_SCENE_WIDTH * COURSE_SCENE_SCALE)
                            ),
                            QuarterPixels(
                                stripTop + (COURSE_SCENE_STRIP_ROW_COUNT * COURSE_SCENE_SCALE)
                            ),
                            G_TX_RENDERTILE,
                            0,
                            COURSE_SCENE_SECOND_ROW_START,
                            TEXEL_STEP_ONE / COURSE_SCENE_SCALE,
                            TEXEL_STEP_ONE / COURSE_SCENE_SCALE
                        );
                    }
                }
            }
        }
        else {
            for (stripIndex = 0; stripIndex != COURSE_SCENE_STRIP_COUNT; stripIndex++) {
                shownRowCount =
                    (stripIndex != COURSE_SCENE_STRIP_COUNT - 1) ? COURSE_SCENE_STRIP_ROW_COUNT : 1;

                if (stripIndex == 0) {
                    gDPLoadTextureBlock(
                        gfx++,
                        TextureAddress(LargeImageTexels(g_MenuCourseSceneImage)),
                        G_IM_FMT_CI,
                        G_IM_SIZ_8b,
                        COURSE_SCENE_WIDTH,
                        COURSE_SCENE_STRIP_ROW_COUNT + COURSE_SCENE_SEAM_ROW_COUNT,
                        0,
                        G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMASK,
                        G_TX_NOMASK,
                        G_TX_NOLOD,
                        G_TX_NOLOD
                    );
                    gDPPipeSync(gfx++);

                    gSPScisTextureRectangle(
                        gfx++,
                        QuarterPixels(COURSE_SCENE_LEFT),
                        QuarterPixels(COURSE_SCENE_FIRST_TOP),
                        QuarterPixels(
                            COURSE_SCENE_LEFT + (COURSE_SCENE_WIDTH * COURSE_SCENE_SCALE)
                        ),
                        QuarterPixels(
                            COURSE_SCENE_FIRST_TOP +
                            ((COURSE_SCENE_STRIP_ROW_COUNT + 1) * COURSE_SCENE_SCALE)
                        ),
                        G_TX_RENDERTILE,
                        0,
                        0,
                        TEXEL_STEP_ONE / COURSE_SCENE_SCALE,
                        TEXEL_STEP_ONE / COURSE_SCENE_SCALE
                    );
                }
                else {
                    gDPPipeSync(gfx++);
                    gDPLoadTextureBlock(
                        gfx++,
                        TextureAddress(
                            LargeImageTexels(g_MenuCourseSceneImage) +
                            (stripIndex * COURSE_SCENE_WIDTH * COURSE_SCENE_STRIP_ROW_COUNT)
                        ),
                        G_IM_FMT_CI,
                        G_IM_SIZ_8b,
                        COURSE_SCENE_WIDTH,
                        shownRowCount + COURSE_SCENE_SEAM_ROW_COUNT,
                        0,
                        G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMASK,
                        G_TX_NOMASK,
                        G_TX_NOLOD,
                        G_TX_NOLOD
                    );
                    gDPPipeSync(gfx++);

                    stripTop = COURSE_SCENE_TOP +
                        (stripIndex * COURSE_SCENE_STRIP_ROW_COUNT * COURSE_SCENE_SCALE);
                    gSPScisTextureRectangle(
                        gfx++,
                        QuarterPixels(COURSE_SCENE_LEFT),
                        QuarterPixels(stripTop),
                        QuarterPixels(
                            COURSE_SCENE_LEFT + (COURSE_SCENE_WIDTH * COURSE_SCENE_SCALE)
                        ),
                        QuarterPixels(
                            COURSE_SCENE_TOP +
                            (stripIndex * COURSE_SCENE_STRIP_ROW_COUNT * COURSE_SCENE_SCALE) +
                            (shownRowCount * COURSE_SCENE_SCALE)
                        ),
                        G_TX_RENDERTILE,
                        0,
                        COURSE_SCENE_SECOND_ROW_START,
                        TEXEL_STEP_ONE / COURSE_SCENE_SCALE,
                        TEXEL_STEP_ONE / COURSE_SCENE_SCALE
                    );
                }
            }
        }
    }

    if (layers & MENU_LAYER_BOARD) {
        gDPPipeSync(gfx++);
        gDPPipeSync(gfx++);
        gDPSetTextureFilter(gfx++, G_TF_POINT);
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_RGBA16);
        gDPLoadTLUT_pal256(
            gfx++,
            TextureAddress(ImagePalette(g_MenuBoardImages[f_MenuBoardImageIndex]))
        );

        for (stripIndex = 0; stripIndex != BOARD_STRIP_COUNT; stripIndex++) {
            gDPLoadTextureBlock(
                gfx++,
                TextureAddress(
                    LargeImageTexels(g_MenuBoardImages[f_MenuBoardImageIndex]) +
                    (stripIndex * BOARD_WIDTH * BOARD_STRIP_HEIGHT)
                ),
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                BOARD_WIDTH,
                BOARD_STRIP_HEIGHT,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gDPPipeSync(gfx++);

            stripTop = BOARD_TOP + (stripIndex * BOARD_STRIP_SPACING);
            gSPScisTextureRectangle(
                gfx++,
                QuarterPixels(BOARD_LEFT),
                QuarterPixels(stripTop),
                QuarterPixels(BOARD_LEFT + BOARD_WIDTH),
                QuarterPixels(stripTop + BOARD_STRIP_SPACING),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TexelRunStep(BOARD_STRIP_HEIGHT, BOARD_STRIP_SPACING)
            );
            gDPPipeSync(gfx++);
        }
    }

    if (layers & MENU_LAYER_TALL_NARROW_BOARD) {
        gDPPipeSync(gfx++);
        gDPPipeSync(gfx++);
        gDPSetTextureFilter(gfx++, G_TF_POINT);
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_RGBA16);
        gDPLoadTLUT_pal256(
            gfx++,
            TextureAddress(ImagePalette(g_MenuBoardImages[f_MenuBoardImageIndex]))
        );

        for (stripIndex = 0; stripIndex != NARROW_BOARD_STRIP_COUNT; stripIndex++) {
            gDPLoadTextureBlock(
                gfx++,
                TextureAddress(
                    LargeImageTexels(g_MenuBoardImages[f_MenuBoardImageIndex]) +
                    (stripIndex * NARROW_BOARD_WIDTH * NARROW_BOARD_STRIP_HEIGHT)
                ),
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                NARROW_BOARD_WIDTH,
                NARROW_BOARD_STRIP_HEIGHT,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gDPPipeSync(gfx++);

            stripTop = BOARD_TOP + (stripIndex * BOARD_STRIP_SPACING);
            gSPScisTextureRectangle(
                gfx++,
                QuarterPixels(TALL_NARROW_BOARD_LEFT),
                QuarterPixels(stripTop),
                QuarterPixels(TALL_NARROW_BOARD_LEFT + NARROW_BOARD_WIDTH),
                QuarterPixels(stripTop + BOARD_STRIP_SPACING),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TexelRunStep(NARROW_BOARD_STRIP_HEIGHT, BOARD_STRIP_SPACING)
            );
            gDPPipeSync(gfx++);
        }
    }

    if (IsBannerBobScreen(g_MenuScreen)) {
        bobOffset = (f_MenuBobOffset + BANNER_BOB_LOWERING) - BANNER_BOB_RAISING;
    }
    else {
        bobOffset = 0;
    }

    if (layers & MENU_LAYER_BANNER) {
        image = g_MenuTitleImages[f_MenuTitleImageIndex];
        gDPLoadTLUT_pal256(gfx++, TextureAddress(ImagePalette(image)));

        for (stripIndex = 0; stripIndex != NAME_PLATE_STRIP_COUNT; stripIndex++) {
            gDPLoadTextureBlock(
                gfx++,
                TextureAddress(
                    LargeImageTexels(image) +
                    (stripIndex * NAME_PLATE_WIDTH * NAME_PLATE_STRIP_HEIGHT)
                ),
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                NAME_PLATE_WIDTH,
                NAME_PLATE_STRIP_HEIGHT,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gDPPipeSync(gfx++);

            gSPScisTextureRectangle(
                gfx++,
                QuarterPixels(MENU_BANNER_LEFT),
                QuarterPixels(
                    BOBBING_BANNER_TOP + (stripIndex * NAME_PLATE_STRIP_HEIGHT) + bobOffset +
                    BOBBING_BANNER_DROP
                ),
                QuarterPixels(MENU_BANNER_LEFT + NAME_PLATE_WIDTH),
                QuarterPixels(
                    BOBBING_BANNER_TOP + NAME_PLATE_STRIP_HEIGHT +
                    (stripIndex * NAME_PLATE_STRIP_HEIGHT) + bobOffset + BOBBING_BANNER_DROP
                ),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
            gDPPipeSync(gfx++);
        }
    }

    if (layers & MENU_LAYER_PER_SCREEN) {
        switch (g_MenuScreen) {
            case MENU_SCREEN_COURSE_SELECT:
                gDPPipeSync(gfx++);
                *displayListHead = gfx;
                return;
        }
    }

    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void DrawLargeBannerBackdropLayers(Gfx **displayListHead, s32 layers) {
    Gfx *gfx = *displayListHead;

    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    if (layers & MENU_LAYER_COURSE_SCENE) {
        DrawMenuCourseScene(&gfx);
    }

    if (layers & MENU_LAYER_BOARD) {
        DrawNarrowMenuBoard(&gfx);
    }

    if (layers & MENU_LAYER_LARGE_BANNER) {
        DrawLargeMenuBanner(&gfx);
    }

    if (layers & MENU_LAYER_BANNER) {
        DrawMenuBanner(&gfx);
    }

    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void DrawMenuCourseScene(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 stripIndex;
    s32 shownRowCount;
    s32 stripTop;

    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal256(gfx++, TextureAddress(ImagePalette(g_MenuCourseSceneImage)));
    for (stripIndex = 0; stripIndex != COURSE_SCENE_STRIP_COUNT; stripIndex++) {
        shownRowCount =
            (stripIndex != COURSE_SCENE_STRIP_COUNT - 1) ? COURSE_SCENE_STRIP_ROW_COUNT : 1;
        if (stripIndex == 0) {
            gDPLoadTextureBlock(
                gfx++,
                TextureAddress(LargeImageTexels(g_MenuCourseSceneImage)),
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                COURSE_SCENE_WIDTH,
                COURSE_SCENE_STRIP_ROW_COUNT + COURSE_SCENE_SEAM_ROW_COUNT,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gDPPipeSync(gfx++);
            gSPScisTextureRectangle(
                gfx++,
                QuarterPixels(COURSE_SCENE_LEFT),
                QuarterPixels(COURSE_SCENE_FIRST_TOP),
                QuarterPixels(COURSE_SCENE_LEFT + (COURSE_SCENE_WIDTH * COURSE_SCENE_SCALE)),
                QuarterPixels(
                    COURSE_SCENE_FIRST_TOP +
                    ((COURSE_SCENE_STRIP_ROW_COUNT + 1) * COURSE_SCENE_SCALE)
                ),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE / COURSE_SCENE_SCALE,
                TEXEL_STEP_ONE / COURSE_SCENE_SCALE
            );
        }
        else {
            gDPPipeSync(gfx++);
            gDPLoadTextureBlock(
                gfx++,
                TextureAddress(
                    LargeImageTexels(g_MenuCourseSceneImage) +
                    (stripIndex * COURSE_SCENE_WIDTH * COURSE_SCENE_STRIP_ROW_COUNT)
                ),
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                COURSE_SCENE_WIDTH,
                shownRowCount + COURSE_SCENE_SEAM_ROW_COUNT,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gDPPipeSync(gfx++);
            stripTop =
                COURSE_SCENE_TOP + (stripIndex * COURSE_SCENE_STRIP_ROW_COUNT * COURSE_SCENE_SCALE);
            gSPScisTextureRectangle(
                gfx++,
                QuarterPixels(COURSE_SCENE_LEFT),
                QuarterPixels(stripTop),
                QuarterPixels(COURSE_SCENE_LEFT + (COURSE_SCENE_WIDTH * COURSE_SCENE_SCALE)),
                QuarterPixels(
                    COURSE_SCENE_TOP +
                    (stripIndex * COURSE_SCENE_STRIP_ROW_COUNT * COURSE_SCENE_SCALE) +
                    (shownRowCount * COURSE_SCENE_SCALE)
                ),
                G_TX_RENDERTILE,
                0,
                COURSE_SCENE_SECOND_ROW_START,
                TEXEL_STEP_ONE / COURSE_SCENE_SCALE,
                TEXEL_STEP_ONE / COURSE_SCENE_SCALE
            );
        }
    }

    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void DrawNarrowMenuBoard(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 stripIndex;
    s32 stripTop;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal256(
        gfx++,
        TextureAddress(ImagePalette(g_MenuBoardImages[f_MenuBoardImageIndex]))
    );
    for (stripIndex = 0; stripIndex != NARROW_BOARD_STRIP_COUNT; stripIndex++) {
        gDPLoadTextureBlock(
            gfx++,
            TextureAddress(
                LargeImageTexels(g_MenuBoardImages[f_MenuBoardImageIndex]) +
                (stripIndex * NARROW_BOARD_WIDTH * NARROW_BOARD_STRIP_HEIGHT)
            ),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            NARROW_BOARD_WIDTH,
            NARROW_BOARD_STRIP_HEIGHT,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPPipeSync(gfx++);
        stripTop = NARROW_BOARD_TOP + (stripIndex * NARROW_BOARD_STRIP_SPACING);
        gSPScisTextureRectangle(
            gfx++,
            QuarterPixels(NARROW_BOARD_LEFT),
            QuarterPixels(stripTop),
            QuarterPixels(NARROW_BOARD_LEFT + NARROW_BOARD_SHOWN_WIDTH),
            QuarterPixels(stripTop + NARROW_BOARD_STRIP_HEIGHT),
            G_TX_RENDERTILE,
            0,
            0,
            TexelRunStep(NARROW_BOARD_WIDTH, NARROW_BOARD_SHOWN_WIDTH),
            TexelRunStep(NARROW_BOARD_STRIP_HEIGHT, NARROW_BOARD_ROW_SCALE)
        );
        gDPPipeSync(gfx++);
    }

    *displayListHead = gfx;
}

void DrawNamePlateBanner(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 stripIndex;
    s32 stripTop;

    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal256(gfx++, TextureAddress(ImagePalette(g_NamePlateImage)));
    for (stripIndex = 0; stripIndex != NAME_PLATE_STRIP_COUNT; stripIndex++) {
        gDPLoadTextureBlock(
            gfx++,
            TextureAddress(
                LargeImageTexels(g_NamePlateImage) +
                (stripIndex * NAME_PLATE_WIDTH * NAME_PLATE_STRIP_HEIGHT)
            ),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            NAME_PLATE_WIDTH,
            NAME_PLATE_STRIP_HEIGHT,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPPipeSync(gfx++);
        stripTop = UNUSED_BANNER_TOP + (stripIndex * NAME_PLATE_STRIP_HEIGHT);
        gSPScisTextureRectangle(
            gfx++,
            QuarterPixels(MENU_BANNER_LEFT),
            QuarterPixels(stripTop),
            QuarterPixels(MENU_BANNER_LEFT + NAME_PLATE_WIDTH),
            QuarterPixels(stripTop + NAME_PLATE_STRIP_HEIGHT),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        gDPPipeSync(gfx++);
    }

    *displayListHead = gfx;
}

void DrawSmallNamePlate(Gfx **displayListHead, s32 left, s32 top) {
    Gfx *gfx = *displayListHead;
    s32 stripIndex;
    s32 stripTop;

    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal256(gfx++, TextureAddress(ImagePalette(g_NamePlateImage)));
    for (stripIndex = 0; stripIndex != NAME_PLATE_STRIP_COUNT; stripIndex++) {
        gDPLoadTextureBlock(
            gfx++,
            TextureAddress(
                LargeImageTexels(g_NamePlateImage) +
                stripIndex * NAME_PLATE_WIDTH * NAME_PLATE_STRIP_HEIGHT
            ),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            NAME_PLATE_WIDTH,
            NAME_PLATE_STRIP_HEIGHT,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPPipeSync(gfx++);
        stripTop = top + (stripIndex * NAME_PLATE_STRIP_HEIGHT);
        gSPScisTextureRectangle(
            gfx++,
            QuarterPixels(left),
            QuarterPixels(stripTop),
            QuarterPixels(left + SMALL_BANNER_WIDTH),
            QuarterPixels(stripTop + NAME_PLATE_STRIP_HEIGHT),
            G_TX_RENDERTILE,
            0,
            0,
            TexelRunStep(NAME_PLATE_WIDTH, SMALL_BANNER_WIDTH),
            TEXEL_STEP_ONE
        );
        gDPPipeSync(gfx++);
    }

    *displayListHead = gfx;
}

void DrawLargeMenuBanner(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 stripIndex;
    s32 stripTop;

    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal256(gfx++, TextureAddress(ImagePalette(g_NamePlateImage)));
    for (stripIndex = 0; stripIndex != NAME_PLATE_STRIP_COUNT; stripIndex++) {
        gDPLoadTextureBlock(
            gfx++,
            TextureAddress(
                LargeImageTexels(g_NamePlateImage) +
                (stripIndex * NAME_PLATE_WIDTH * NAME_PLATE_STRIP_HEIGHT)
            ),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            NAME_PLATE_WIDTH,
            NAME_PLATE_STRIP_HEIGHT,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPPipeSync(gfx++);
        stripTop = LARGE_BANNER_TOP + (stripIndex * LARGE_BANNER_STRIP_SPACING);
        gSPScisTextureRectangle(
            gfx++,
            QuarterPixels(LARGE_BANNER_LEFT),
            QuarterPixels(stripTop),
            QuarterPixels(LARGE_BANNER_LEFT + LARGE_BANNER_WIDTH),
            QuarterPixels(stripTop + LARGE_BANNER_STRIP_HEIGHT),
            G_TX_RENDERTILE,
            0,
            0,
            TexelRunStep(NAME_PLATE_WIDTH, LARGE_BANNER_WIDTH),
            TexelRunStep(NAME_PLATE_STRIP_HEIGHT, LARGE_BANNER_STRIP_HEIGHT)
        );
        gDPPipeSync(gfx++);
    }

    *displayListHead = gfx;
}

void DrawMenuBanner(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 stripIndex;
    u8 *image = g_MenuTitleImages[f_MenuTitleImageIndex];
    s32 stripTop;

    gDPLoadTLUT_pal256(gfx++, TextureAddress(ImagePalette(image)));
    for (stripIndex = 0; stripIndex != NAME_PLATE_STRIP_COUNT; stripIndex++) {
        gDPLoadTextureBlock(
            gfx++,
            TextureAddress(
                LargeImageTexels(image) + (stripIndex * NAME_PLATE_WIDTH * NAME_PLATE_STRIP_HEIGHT)
            ),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            NAME_PLATE_WIDTH,
            NAME_PLATE_STRIP_HEIGHT,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPPipeSync(gfx++);
        stripTop = MENU_BANNER_TOP + (stripIndex * NAME_PLATE_STRIP_HEIGHT);
        gSPScisTextureRectangle(
            gfx++,
            QuarterPixels(MENU_BANNER_LEFT),
            QuarterPixels(stripTop),
            QuarterPixels(MENU_BANNER_LEFT + NAME_PLATE_WIDTH),
            QuarterPixels(stripTop + NAME_PLATE_STRIP_HEIGHT),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        gDPPipeSync(gfx++);
    }

    *displayListHead = gfx;
}

#define TITLE_LABEL_NEW_GAME 0
#define TITLE_LABEL_FIRST_PLAYER_COUNT 1
#define TITLE_LABEL_FIRST_OTHER_CHOICE 4
#define TITLE_LABEL_SAVED_GAMES 5
#define TITLE_LABEL_TRANSMIT 7
#define PLAIN_MENU_HEADING_X 64
#define PLAIN_MENU_HEADING_Y 32
#define PLAIN_MENU_NAME_COUNT 3
#define PLAIN_MENU_NAME_X 112
#define PLAIN_MENU_NAME_TOP 128
#define PLAIN_MENU_NAME_SPACING 8
#define PLAIN_MENU_FIGURES_X 32
#define PLAIN_MENU_FIGURES_Y 100
#define BOARD_BACKDROP_LAYERS \
    (MENU_LAYER_COURSE_SCENE | MENU_LAYER_BOARD | MENU_LAYER_BANNER | MENU_LAYER_LARGE_BANNER | \
        MENU_LAYER_PER_SCREEN)

#define IsChosenNewGameLabel(labelIndex) \
    ((g_MenuMemory.titleChoice == TITLE_CHOICE_NEW_GAME && \
         (labelIndex) == TITLE_LABEL_NEW_GAME) || \
        (g_MenuMemory.titleChoice == TITLE_CHOICE_NEW_GAME && \
            (labelIndex) == g_MenuMemory.playerCountChoice + TITLE_LABEL_FIRST_PLAYER_COUNT))

void DrawMenuTitleScreen(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 labelIndex;
    s32 labelOffset;
    s16 growth;

    SetUpShadowedTextDrawing(&gfx);
    SetTextColor(&gfx, 255, 255, 255);
    DrawShadowedText(&gfx, PLAIN_MENU_HEADING_X, PLAIN_MENU_HEADING_Y, "MAIN MENU");
    for (labelIndex = 0; labelIndex != PLAIN_MENU_NAME_COUNT; labelIndex++) {
        DrawShadowedText(
            &gfx,
            PLAIN_MENU_NAME_X,
            PLAIN_MENU_NAME_TOP + (labelIndex * PLAIN_MENU_NAME_SPACING),
            (char *)f_PlainMenuNames[labelIndex]
        );
    }

    sprintf(
        g_PrintBuffer,
        "%8d:%8d:%8d",
        MemberSize(SaveData, rankedRecords),
        MemberSize(SaveData, ringShotRecords),
        (s32)sizeof(SaveData)
    );
    DrawShadowedText(&gfx, PLAIN_MENU_FIGURES_X, PLAIN_MENU_FIGURES_Y, g_PrintBuffer);
    DrawMenuBackdropLayers(&gfx, BOARD_BACKDROP_LAYERS);
    if (g_ControllerStates[0].buttons & B_BUTTON) {
        gDPPipeSync(gfx++);
        gDPSetTextureFilter(gfx++, G_TF_AVERAGE);
    }

    gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImagePalette(g_ImageSlots[f_ImageSlotIndex])));
    for (labelIndex = 0; labelIndex != TITLE_LABEL_COUNT; labelIndex++) {
        if (IsChosenNewGameLabel(labelIndex) ||
            (g_MenuMemory.titleChoice + TITLE_LABEL_FIRST_OTHER_CHOICE == labelIndex &&
                g_MenuMemory.titleChoice != TITLE_CHOICE_NEW_GAME)) {
            continue;
        }

        if (labelIndex == TITLE_LABEL_SAVED_GAMES && g_HasBroughtOverCharacter == TRUE) {
            labelOffset = TITLE_LABEL_TRANSMIT * TITLE_LABEL_IMAGE_SIZE;
        }
        else {
            s32 row = labelIndex * TITLE_LABEL_HEIGHT;

            labelOffset = row * TITLE_LABEL_ROW_SIZE;
        }

        gDPLoadTextureBlock_4b(
            gfx++,
            TextureAddress(ImageTexels(g_ImageSlots[f_ImageSlotIndex]) + labelOffset),
            G_IM_FMT_CI,
            TITLE_LABEL_WIDTH,
            TITLE_LABEL_HEIGHT,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(
            gfx++,
            QuarterPixels(f_TitleLabelCorners[labelIndex].x),
            QuarterPixels(f_TitleLabelCorners[labelIndex].y),
            QuarterPixels(f_TitleLabelCorners[labelIndex].x + TITLE_LABEL_WIDTH),
            QuarterPixels(f_TitleLabelCorners[labelIndex].y + TITLE_LABEL_HEIGHT),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        gDPPipeSync(gfx++);
    }

    gDPLoadTLUT_pal16(
        gfx++,
        0,
        TextureAddress(ImagePalette(g_MenuLabelImages[f_MenuLabelImageIndex]))
    );
    for (labelIndex = 0; labelIndex != TITLE_LABEL_COUNT; labelIndex++) {
        if (IsChosenNewGameLabel(labelIndex) ||
            (labelIndex == g_MenuMemory.titleChoice + TITLE_LABEL_FIRST_OTHER_CHOICE &&
                g_MenuMemory.titleChoice != TITLE_CHOICE_NEW_GAME)) {
            if (labelIndex == TITLE_LABEL_SAVED_GAMES && g_HasBroughtOverCharacter == TRUE) {
                labelOffset = TITLE_LABEL_TRANSMIT * TITLE_LABEL_IMAGE_SIZE;
            }
            else {
                s32 row = labelIndex * TITLE_LABEL_HEIGHT;

                labelOffset = row * TITLE_LABEL_ROW_SIZE;
            }

            gDPLoadTextureBlock_4b(
                gfx++,
                TextureAddress(ImageTexels(g_MenuLabelImages[f_MenuLabelImageIndex]) + labelOffset),
                G_IM_FMT_CI,
                TITLE_LABEL_WIDTH,
                TITLE_LABEL_HEIGHT,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gDPPipeSync(gfx++);

            growth = 0;
            if (labelIndex != TITLE_LABEL_NEW_GAME) {
                growth = g_ChosenLabelGrowth;
            }

            gSPScisTextureRectangle(
                gfx++,
                QuarterPixels(f_TitleLabelCorners[labelIndex].x - (growth / 2)),
                QuarterPixels(f_TitleLabelCorners[labelIndex].y - (growth / 2)),
                QuarterPixels(f_TitleLabelCorners[labelIndex].x + TITLE_LABEL_WIDTH + (growth / 2)),
                QuarterPixels(
                    f_TitleLabelCorners[labelIndex].y + (TITLE_LABEL_HEIGHT + (growth / 2))
                ),
                G_TX_RENDERTILE,
                0,
                0,
                TexelRunStep(TITLE_LABEL_WIDTH, growth + TITLE_LABEL_WIDTH),
                TexelRunStep(TITLE_LABEL_HEIGHT, growth + TITLE_LABEL_HEIGHT)
            );
            gDPPipeSync(gfx++);
        }
    }

    if (g_ChosenLabelGrowthDirection == LABEL_GROWING) {
        g_ChosenLabelGrowth += CHOSEN_LABEL_GROWTH_STEP;
    }

    if (g_ChosenLabelGrowthDirection == LABEL_SHRINKING) {
        g_ChosenLabelGrowth -= CHOSEN_LABEL_GROWTH_STEP;
    }

    if (g_ChosenLabelGrowth > CHOSEN_LABEL_GROWTH_LIMIT) {
        g_ChosenLabelGrowthDirection = LABEL_SHRINKING;
    }

    if (g_ChosenLabelGrowth < 0) {
        g_ChosenLabelGrowthDirection = LABEL_GROWING;
    }

    *displayListHead = gfx;
}

const char f_UnusedEmptyString[] = "";

#define MAIN_CHOICE_LABEL_WIDTH 128
#define MAIN_CHOICE_LABEL_HEIGHT 20
#define MAIN_CHOICE_LABEL_IMAGE_SIZE (MAIN_CHOICE_LABEL_WIDTH * MAIN_CHOICE_LABEL_HEIGHT / 2)

#define SPINNING_ICON_FRAME_DURATION 2
#define MODE_ICON_LEFT 168
#define MODE_ICON_TOP 96

#define NO_RIVAL (-1)
#define RIVAL_SLOT 1
#define PLAYER_AND_RIVAL_COUNT 2
#define IsRivalFound(rival) ((rival) >= 0)
#define IsRivalSlot(characterCount, rival, slot) \
    ((((characterCount) == PLAYER_AND_RIVAL_COUNT) & IsRivalFound(rival)) && ((slot) == RIVAL_SLOT))
#define RIVAL_SPIN_STEP 10.0f

#define TOAD_WIDTH 48
#define TOAD_STRIP_HEIGHT 30
#define TOAD_LAST_STRIP_HEIGHT 21
#define TOAD_STRIP_COUNT 2

#define CADDY_SELECT_TITLE_X 64
#define CADDY_SELECT_TITLE_Y 32
#define CADDY_SELECT_NAME_X 96
#define CADDY_SELECT_FIRST_Y 64
#define CADDY_SELECT_LINE_HEIGHT 8

#define MODE_SELECT_BACKDROP_LAYERS \
    (MENU_LAYER_COURSE_SCENE | MENU_LAYER_IGNORED | MENU_LAYER_BANNER | MENU_LAYER_LARGE_BANNER)
#define MODE_SELECT_LIGHT_YAW 0
#define MODE_SELECT_LIGHT_PITCH 0x3000
#define SINGLE_PLAYER_MODE_BOARD_RADIUS_X 64.0f
#define MODE_BANNER_TOP 152
#define MODE_BANNER_LEFT 80
#define MODE_BANNER_SHOWN_WIDTH 160
#define SINGLE_PLAYER_MODE_BANNER_SHOWN_WIDTH 180
#define MODE_ARROW_GAP 64
#define MODE_ARROW_Y 124
#define WON_COUNT_ICON_X 224
#define WON_COUNT_ICON_Y 64
#define WON_COUNT_X 250
#define WON_COUNT_Y 72
#define TWO_DIGIT_WON_COUNT_MIN 10
#define THREE_DIGIT_WON_COUNT_MIN 100

#define WATER_PLANE_DISTANCE 300.0f
#define WATER_FIELD_OF_VIEW 45.0f
#define WATER_PERSPECTIVE_SCALE 4.0f
#define WATER_FOG_START 980
#define WATER_FOG_END 1050
#define WATER_BLEND_ALPHA 127
#define WATER_TEXTURE_SIZE 32
#define WATER_TEXTURE_MASK 5
#define WATER_TEXTURE_SHIFT 2
#define WATER_LAYER_TMEM 0x100
#define WATER_LAYER_TILE 1
#define WATER_TILE_SPAN 64
#define COURSE_SELECT_EYE_HEIGHT 50.0f
#define COURSE_SELECT_EYE_DISTANCE 60.0f
#define FRONT_COURSE_BOB_PHASE_STEP 0.1f
#define FRONT_COURSE_BOB_HEIGHT 8.0f
#define CHOSEN_COURSE_DEPTH 0x7FFF
#define LOCKED_COURSE_SHADE 96
#define LOCKED_PANEL_LEFT 108
#define LOCKED_PANEL_TOP 100
#define LOCKED_PANEL_RIGHT 212
#define LOCKED_PANEL_BOTTOM 160
#define LOCKED_COURSE_TEXT_X 111
#define LOCKED_COURSE_TEXT_Y 116
#define MESSAGE_POINTS_TO_UNLOCK 0xC2
#define COURSE_BADGE_ICON_X 226
#define COURSE_BADGE_ICON_Y 200
#define COURSE_BADGE_COUNT_X 252
#define COURSE_BADGE_COUNT_Y 208
#define UNCHOSEN_HOLES_BADGE_SHADE 127
#define BET_BADGE_COLOR_STEP (M_PI / 18)
#define BET_BADGE_COLOR_MIDPOINT 127
#define BET_BADGE_COLOR_SWING 127.0f
#define BET_BADGE_ENV_DIVISOR 4
#define COURSE_SELECT_BACK_LAYERS MENU_LAYER_COURSE_SCENE
#define COURSE_SELECT_FRONT_LAYERS (MENU_LAYER_IGNORED | MENU_LAYER_BANNER)
#define BET_SLIDE_STEP 32
#define BET_SLIDE_DISTANCE 512

void DrawScaledModeIcon(Gfx **displayListHead, s32 left, s32 top, s32 halfSize, s32 scale);
void DrawMenuCharacters(Gfx **displayListHead);
void DrawCourseSelectBanners(Gfx **displayListHead);
void DrawSpinningStars(
    Gfx **displayListHead,
    s32 left,
    s32 top,
    u8 starCount,
    u8 enlargesChosenHole
);

void DrawModeSelect(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    u16 perspectiveNormalization;
    f32 boardX[WHEEL_SLOT_LIMIT];
    f32 boardY[WHEEL_SLOT_LIMIT];
    f32 boardScale[WHEEL_SLOT_LIMIT];
    s32 unusedSlots[WHEEL_SLOT_LIMIT] UNUSED;
    s32 drawOrder[WHEEL_SLOT_LIMIT];
    s32 position[PLANE_AXIS_COUNT];
    SaveData *save = GetSaveData();
    s16 badgeCount = 0;
    s16 starCount;
    s32 modeCount;
    s32 index;
    s32 key;
    f32 radiusX;
    s32 angleStep;
    Radians angle;
    f32 circleX;
    s32 drawnIndex;
    s32 halfSize;
    s32 texelStep;
    f32 bobOffset;
    s32 wheelShiftX = 0;
    UBoolean8 areCharactersDrawn;
    s32 labelIndex;
    s32 labelX;
    ImageAsset *image;
    s32 i;
    CourseIndex courseIndex;
    s32 holeIndex;

    g_HoleSetup.conditions.lightYaw = MODE_SELECT_LIGHT_YAW;
    g_HoleSetup.conditions.lightPitch = MODE_SELECT_LIGHT_PITCH;
    DrawLargeBannerBackdropLayers(&gfx, MODE_SELECT_BACKDROP_LAYERS);
    guPerspectiveF(
        f_ModeSelectProjection,
        &perspectiveNormalization,
        WHEEL_FIELD_OF_VIEW,
        SCREEN_ASPECT_RATIO,
        WHEEL_NEAR_PLANE,
        WHEEL_FAR_PLANE,
        1.0f
    );
    guLookAtReflectF(
        f_ModeSelectView,
        &f_ModeSelectLookAts[g_DrawBufferIndex],
        0.0f,
        CAROUSEL_EYE_HEIGHT,
        CAROUSEL_EYE_DISTANCE,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
        0.0f
    );
    gSPLookAt(gfx++, &f_ModeSelectLookAts[g_DrawBufferIndex]);
    SetCpuProjectionF(f_ModeSelectProjection, f_ModeSelectView, g_Viewports);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);

    modeCount = f_ModeCounts[g_ChosenPlayerCount - 1];
    for (index = 0; index != modeCount; index++) {
        ScreenPoint point;
        s32 turns;

        if (g_ChosenPlayerCount == 1) {
            radiusX = SINGLE_PLAYER_MODE_BOARD_RADIUS_X;
        }
        else {
            radiusX = CAROUSEL_RADIUS_X;
        }

        turns = (g_MenuMemory.modeChoice - index) % modeCount;
        if (modeCount != 0) {
            angleStep = DEGREES_PER_TURN / modeCount;
        }
        else {
            angleStep = DEGREES_PER_TURN;
        }

        angle = -DegreesToRadians(
            (turns * angleStep) + (g_MenuMemory.modeSpin * DEGREES_PER_SPIN_STEP)
        );
        circleX = sinf(angle) * radiusX;

        if (ProjectPoint(circleX, 0.0f, cosf(angle) * CAROUSEL_RADIUS_Z, &point) < 0) {
            boardX[index] = point.x;
            boardY[index] = point.y - WHEEL_RAISE;
            boardScale[index] = point.scaleX;
        }
    }

    for (i = 0; i != modeCount; i++) {
        drawOrder[i] = i;
    }

    for (i = 1; i != modeCount; i++) {
        key = drawOrder[i];
        for (index = i - 1; (index >= 0) && (boardY[drawOrder[index]] > boardY[key]); index--) {
            drawOrder[index + 1] = drawOrder[index];
        }

        drawOrder[index + 1] = key;
    }

    gDPPipeSync(gfx++);
    gDPSetDepthSource(gfx++, G_ZS_PRIM);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);

    areCharactersDrawn = FALSE;
    f_ModeBoardBobPhase += CAROUSEL_BOB_PHASE_STEP;
    for (index = 0; index != modeCount; index++) {
        drawnIndex = drawOrder[index];
        halfSize = boardScale[drawnIndex] * CAROUSEL_BOARD_HALF_SIZE;
        texelStep = (f32)(MODE_ICON_STRIP_HEIGHT << TEXEL_STEP_SHIFT) / halfSize;
        if (index == modeCount) {
            bobOffset = sinf(f_ModeBoardBobPhase) * CAROUSEL_BOB_HEIGHT;
        }
        else {
            bobOffset = 0.0f;
        }

        gDPLoadTLUT_pal256(gfx++, TextureAddress(ImagePalette(g_ModeIconImages[drawnIndex])));
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
        if ((g_MenuMemory.modeChoice == drawnIndex) && (g_MenuMemory.modeSpin == 0)) {
            if (!areCharactersDrawn) {
                DrawMenuCharacters(&gfx);
                areCharactersDrawn = TRUE;
                gDPPipeSync(gfx++);
                gDPPipeSync(gfx++);
                gDPSetCycleType(gfx++, G_CYC_1CYCLE);
                gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
                gDPPipeSync(gfx++);
                gDPSetTexturePersp(gfx++, G_TP_NONE);
                gDPPipeSync(gfx++);
                gDPSetTextureFilter(gfx++, G_TF_BILERP);
                gDPPipeSync(gfx++);
                gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
                gDPPipeSync(gfx++);
                gDPSetTextureLUT(gfx++, G_TT_RGBA16);
            }

            gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
            DrawScaledModeIcon(
                &gfx,
                wheelShiftX + (s32)(boardX[drawnIndex] - halfSize),
                boardY[drawnIndex] - halfSize + bobOffset,
                halfSize,
                texelStep
            );
        }
        else {
            if (g_MenuMemory.modeChoice == drawnIndex) {
                if (!areCharactersDrawn) {
                    DrawMenuCharacters(&gfx);
                    gDPPipeSync(gfx++);
                    gDPPipeSync(gfx++);
                    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
                    gSPTexture(
                        gfx++,
                        TEXTURE_SCALE_HALF,
                        TEXTURE_SCALE_HALF,
                        0,
                        G_TX_RENDERTILE,
                        G_ON
                    );
                    gDPPipeSync(gfx++);
                    gDPSetTexturePersp(gfx++, G_TP_NONE);
                    gDPPipeSync(gfx++);
                    gDPSetTextureFilter(gfx++, G_TF_BILERP);
                    gDPPipeSync(gfx++);
                    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
                    gDPPipeSync(gfx++);
                    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
                    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
                    gDPLoadTLUT_pal256(
                        gfx++,
                        TextureAddress(ImagePalette(g_ModeIconImages[drawnIndex]))
                    );
                    areCharactersDrawn = TRUE;
                }
            }
            else {
                gDPSetCombineMode(gfx++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
                gDPSetPrimColor(
                    gfx++,
                    0,
                    0,
                    UNCHOSEN_CAROUSEL_BOARD_SHADE,
                    UNCHOSEN_CAROUSEL_BOARD_SHADE,
                    UNCHOSEN_CAROUSEL_BOARD_SHADE,
                    OPAQUE_ALPHA
                );
            }

            for (i = 0; i != MODE_ICON_STRIP_COUNT; i++) {
                gDPLoadTextureBlock(
                    gfx++,
                    TextureAddress(
                        LargeImageTexels(g_ModeIconImages[drawnIndex]) +
                        (i * MODE_ICON_WIDTH * MODE_ICON_STRIP_HEIGHT)
                    ),
                    G_IM_FMT_CI,
                    G_IM_SIZ_8b,
                    MODE_ICON_WIDTH,
                    MODE_ICON_STRIP_HEIGHT,
                    0,
                    G_TX_NOMIRROR | G_TX_WRAP,
                    G_TX_NOMIRROR | G_TX_WRAP,
                    G_TX_NOMASK,
                    G_TX_NOMASK,
                    G_TX_NOLOD,
                    G_TX_NOLOD
                );
                gDPPipeSync(gfx++);
                gSPScisTextureRectangle(
                    gfx++,
                    QuarterPixels(wheelShiftX + (s32)(boardX[drawnIndex] - halfSize)),
                    QuarterPixels((s32)(boardY[drawnIndex] - halfSize + (i * halfSize) +
                        bobOffset)),
                    QuarterPixels(wheelShiftX + (s32)(boardX[drawnIndex] + halfSize)),
                    QuarterPixels((s32)(boardY[drawnIndex] + (i * halfSize) + bobOffset)),
                    G_TX_RENDERTILE,
                    0,
                    0,
                    texelStep,
                    texelStep
                );
                gDPPipeSync(gfx++);
            }
        }
    }

    gDPPipeSync(gfx++);
    gDPSetDepthSource(gfx++, G_ZS_PIXEL);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    BuildDynamicTextures(&gfx);

    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);

    gDPLoadTLUT_pal256(gfx++, TextureAddress(ImagePalette(g_NamePlateImage)));
    if (g_ChosenPlayerCount == 1) {
        for (i = 0; i != NAME_PLATE_STRIP_COUNT; i++) {
            gDPLoadTextureBlock(
                gfx++,
                TextureAddress(
                    LargeImageTexels(g_NamePlateImage) +
                    (i * NAME_PLATE_WIDTH * NAME_PLATE_STRIP_HEIGHT)
                ),
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                NAME_PLATE_WIDTH,
                NAME_PLATE_STRIP_HEIGHT,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gDPPipeSync(gfx++);
            gSPScisTextureRectangle(
                gfx++,
                QuarterPixels(MODE_BANNER_LEFT),
                QuarterPixels(MODE_BANNER_TOP + (i * NAME_PLATE_STRIP_HEIGHT)),
                QuarterPixels(MODE_BANNER_LEFT + SINGLE_PLAYER_MODE_BANNER_SHOWN_WIDTH),
                QuarterPixels(
                    MODE_BANNER_TOP + NAME_PLATE_STRIP_HEIGHT + (i * NAME_PLATE_STRIP_HEIGHT)
                ),
                G_TX_RENDERTILE,
                0,
                0,
                TexelRunStep(NAME_PLATE_WIDTH, SINGLE_PLAYER_MODE_BANNER_SHOWN_WIDTH),
                TEXEL_STEP_ONE
            );
            gDPPipeSync(gfx++);
        }
    }
    else {
        for (i = 0; i != NAME_PLATE_STRIP_COUNT; i++) {
            gDPLoadTextureBlock(
                gfx++,
                TextureAddress(
                    LargeImageTexels(g_NamePlateImage) +
                    (i * NAME_PLATE_WIDTH * NAME_PLATE_STRIP_HEIGHT)
                ),
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                NAME_PLATE_WIDTH,
                NAME_PLATE_STRIP_HEIGHT,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gDPPipeSync(gfx++);
            gSPScisTextureRectangle(
                gfx++,
                QuarterPixels(MODE_BANNER_LEFT),
                QuarterPixels(MODE_BANNER_TOP + (i * NAME_PLATE_STRIP_HEIGHT)),
                QuarterPixels(MODE_BANNER_LEFT + MODE_BANNER_SHOWN_WIDTH),
                QuarterPixels(
                    MODE_BANNER_TOP + NAME_PLATE_STRIP_HEIGHT + (i * NAME_PLATE_STRIP_HEIGHT)
                ),
                G_TX_RENDERTILE,
                0,
                0,
                TexelRunStep(NAME_PLATE_WIDTH, MODE_BANNER_SHOWN_WIDTH),
                TEXEL_STEP_ONE
            );
            gDPPipeSync(gfx++);
        }
    }

    i = g_MenuMemory.modeChoice;
    gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImagePalette(g_ImageSlots[f_ImageSlotIndex])));
    labelIndex = f_ModeChoiceIcons[g_ChosenPlayerCount - 1][i];
    gDPLoadTextureBlock_4b(
        gfx++,
        TextureAddress(
            ImageTexels(g_ImageSlots[f_ImageSlotIndex]) + (labelIndex * MODE_LABEL_IMAGE_SIZE)
        ),
        G_IM_FMT_CI,
        MODE_LABEL_WIDTH,
        MODE_LABEL_HEIGHT,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPPipeSync(gfx++);

    labelX = (g_ChosenPlayerCount == 1) ? SINGLE_PLAYER_MODE_LABEL_X : MODE_LABEL_X;
    gSPScisTextureRectangle(
        gfx++,
        QuarterPixels(labelX - f_ModeSelectModeLabelOffsets[labelIndex]),
        QuarterPixels(MODE_LABEL_TOP),
        QuarterPixels(labelX - f_ModeSelectModeLabelOffsets[labelIndex] + MODE_LABEL_WIDTH),
        QuarterPixels(MODE_LABEL_TOP + MODE_LABEL_HEIGHT),
        G_TX_RENDERTILE,
        0,
        0,
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );
    gDPPipeSync(gfx++);

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);

    image = g_SecondAnimatedIconImages[0];
    gDPLoadTLUT_pal256(gfx++, TextureAddress(ImagePalette(image)));
    for (i = 0; i != g_ChosenPlayerCount; i++) {
        if (g_PlayerControllerIndices[i] != NO_CONTROLLER_INDEX) {
            position[AXIS_X] =
                f_ModeSelectControllerIconPositions[g_ChosenPlayerCount - 1][i][AXIS_X];
            position[AXIS_Y] =
                f_ModeSelectControllerIconPositions[g_ChosenPlayerCount - 1][i][AXIS_Y];
            gDPLoadTextureBlock(
                gfx++,
                TextureAddress(
                    LargeImageTexels(image) +
                    (g_ChoosingControllerIndices[i] * CONTROLLER_ICON_IMAGE_SIZE)
                ),
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                CONTROLLER_ICON_SIZE,
                CONTROLLER_ICON_SIZE,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gDPPipeSync(gfx++);
            gSPScisTextureRectangle(
                gfx++,
                QuarterPixels(position[AXIS_X]),
                QuarterPixels(position[AXIS_Y]),
                QuarterPixels(position[AXIS_X] + CONTROLLER_ICON_SIZE),
                QuarterPixels(position[AXIS_Y] + CONTROLLER_ICON_SIZE),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
            gDPPipeSync(gfx++);
        }
    }

    image = g_SecondAnimatedIconImages[1];
    gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImagePalette(image)));
    for (i = 0; i != g_ChosenPlayerCount; i++) {
        position[AXIS_X] = f_ModeSelectControllerIconPositions[g_ChosenPlayerCount - 1][i][AXIS_X];
        position[AXIS_Y] = f_ModeSelectControllerIconPositions[g_ChosenPlayerCount - 1][i][AXIS_Y];
        if (g_PlayerControllerIndices[i] == NO_CONTROLLER_INDEX) {
            gDPLoadTextureBlock_4b(
                gfx++,
                TextureAddress(
                    ImageTexels(image) + (PLAYER_LABEL_FIRST_COMPUTER * PLAYER_LABEL_IMAGE_SIZE) +
                    (i * PLAYER_LABEL_IMAGE_SIZE)
                ),
                G_IM_FMT_CI,
                PLAYER_LABEL_WIDTH,
                PLAYER_LABEL_HEIGHT,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
        }
        else {
            gDPLoadTextureBlock_4b(
                gfx++,
                TextureAddress(ImageTexels(image) + (i * PLAYER_LABEL_IMAGE_SIZE)),
                G_IM_FMT_CI,
                PLAYER_LABEL_WIDTH,
                PLAYER_LABEL_HEIGHT,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
        }

        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(
            gfx++,
            QuarterPixels(position[AXIS_X] + PLAYER_LABEL_OFFSET_X),
            QuarterPixels(position[AXIS_Y] + PLAYER_LABEL_OFFSET_Y),
            QuarterPixels(position[AXIS_X] + PLAYER_LABEL_OFFSET_X + PLAYER_LABEL_WIDTH),
            QuarterPixels(position[AXIS_Y] + PLAYER_LABEL_OFFSET_Y + PLAYER_LABEL_HEIGHT),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        gDPPipeSync(gfx++);
    }

    DrawCenteredPageArrows(&gfx, MODE_ARROW_GAP, MODE_ARROW_Y);

    if ((g_ChosenPlayerCount == 1) &&
        (f_ModeChoiceIcons[0][g_MenuMemory.modeChoice] == MODE_ICON_TOURNAMENT)) {
        badgeCount = 0;
        for (courseIndex = 0; courseIndex != COURSE_COUNT; courseIndex++) {
            for (holeIndex = 0; holeIndex != COURSE_HOLE_COUNT; holeIndex++) {
                if (save->records.awards.birdieBadgeCounts[courseIndex][holeIndex] > 0) {
                    badgeCount++;
                }
            }
        }

        i = 0;
        if (badgeCount < TWO_DIGIT_WON_COUNT_MIN) {
            i = DIGIT_GLYPH_SIZE;
        }

        if ((badgeCount >= TWO_DIGIT_WON_COUNT_MIN) && (badgeCount < THREE_DIGIT_WON_COUNT_MIN)) {
            i = 0;
        }

        if (badgeCount >= THREE_DIGIT_WON_COUNT_MIN) {
            i = -DIGIT_GLYPH_SIZE;
        }

        DrawSpinningBadge(&gfx, g_SpinningBadgeImage, WON_COUNT_ICON_X + i, WON_COUNT_ICON_Y);
        gDPPipeSync(gfx++);
        DrawUnalignedMenuNumber(&gfx, WON_COUNT_X + i, WON_COUNT_Y, badgeCount, MENU_NUMBER_TIMES);
        gDPPipeSync(gfx++);
    }

    if ((g_ChosenPlayerCount == 1) &&
        (f_ModeChoiceIcons[0][g_MenuMemory.modeChoice] == MODE_ICON_RING_SHOT)) {
        for (courseIndex = 0, starCount = 0; courseIndex != RING_SHOT_COURSE_COUNT; courseIndex++) {
            for (holeIndex = 0; holeIndex != RING_SHOT_HOLE_COUNT; holeIndex++) {
                if (save->records.hasRingShotStar[courseIndex][holeIndex] > 0) {
                    starCount++;
                }
            }
        }

        i = 0;
        if (starCount < TWO_DIGIT_WON_COUNT_MIN) {
            i = DIGIT_GLYPH_SIZE;
        }

        if ((starCount >= TWO_DIGIT_WON_COUNT_MIN) && (badgeCount < THREE_DIGIT_WON_COUNT_MIN)) {
            i = 0;
        }

        if (starCount >= THREE_DIGIT_WON_COUNT_MIN) {
            i = -DIGIT_GLYPH_SIZE;
        }

        DrawSpinningStars(&gfx, WON_COUNT_ICON_X + i, WON_COUNT_ICON_Y, 1, FALSE);
        gDPPipeSync(gfx++);
        DrawUnalignedMenuNumber(&gfx, WON_COUNT_X + i, WON_COUNT_Y, starCount, MENU_NUMBER_TIMES);
        gDPPipeSync(gfx++);
    }

    *displayListHead = gfx;
}

void DrawModeIcon(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    EncodedAsset asset;
    s32 stripIndex;
    ImageAsset *image;
    u8 iconIndex;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    image = g_AnimatedIconImages[g_AnimatedIconBufferIndex];
    gDPLoadTLUT_pal256(gfx++, TextureAddress(ImagePalette(image)));
    for (stripIndex = 0; stripIndex != MODE_ICON_STRIP_COUNT; stripIndex++) {
        gDPLoadTextureBlock(
            gfx++,
            TextureAddress(
                LargeImageTexels(image) + (stripIndex * MODE_ICON_WIDTH * MODE_ICON_STRIP_HEIGHT)
            ),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            MODE_ICON_WIDTH,
            MODE_ICON_STRIP_HEIGHT,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(
            gfx++,
            QuarterPixels(MODE_ICON_LEFT),
            QuarterPixels(MODE_ICON_TOP + (stripIndex * MODE_ICON_STRIP_HEIGHT)),
            QuarterPixels(MODE_ICON_LEFT + MODE_ICON_WIDTH),
            QuarterPixels(
                MODE_ICON_TOP + MODE_ICON_STRIP_HEIGHT + (stripIndex * MODE_ICON_STRIP_HEIGHT)
            ),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        gDPPipeSync(gfx++);
    }

    g_AnimatedIconFrameTime++;
    if (g_AnimatedIconFrameTime == MODE_ICON_FRAME_DURATION) {
        iconIndex = f_ModeChoiceIcons[g_ChosenPlayerCount - 1][g_MenuMemory.modeChoice];
        g_AnimatedIconFrameTime = 0;
        g_AnimatedIconFrame++;
        if (g_AnimatedIconFrame >= MODE_ICON_FRAME_COUNT) {
            g_AnimatedIconFrame = 0;
        }

        if (g_ModeIconAnimations[iconIndex][g_AnimatedIconFrame] == END_OF_ICON_FRAMES) {
            g_AnimatedIconFrame = 0;
        }

        g_AnimatedIconBufferIndex = g_AnimatedIconBufferIndex == 0;
        OpenEncodedAsset(g_ModeIconAnimations[iconIndex][g_AnimatedIconFrame], &asset);
        ReadEncodedAsset(g_AnimatedIconImages[g_AnimatedIconBufferIndex], &asset);
    }

    *displayListHead = gfx;
}

void DrawScaledModeIcon(Gfx **displayListHead, s32 left, s32 top, s32 halfSize, s32 scale) {
    Gfx *gfx = *displayListHead;
    EncodedAsset asset;
    s32 stripIndex;
    ImageAsset *image;
    u8 iconIndex;

    gDPPipeSync(gfx++);
    image = g_AnimatedIconImages[g_AnimatedIconBufferIndex];
    gDPLoadTLUT_pal256(gfx++, TextureAddress(ImagePalette(image)));
    for (stripIndex = 0; stripIndex != MODE_ICON_STRIP_COUNT; stripIndex++) {
        gDPLoadTextureBlock(
            gfx++,
            TextureAddress(
                LargeImageTexels(image) + (stripIndex * MODE_ICON_WIDTH * MODE_ICON_STRIP_HEIGHT)
            ),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            MODE_ICON_WIDTH,
            MODE_ICON_STRIP_HEIGHT,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(
            gfx++,
            QuarterPixels(left),
            QuarterPixels(top + (stripIndex * halfSize)),
            QuarterPixels(left + halfSize + halfSize),
            QuarterPixels(top + halfSize + (stripIndex * halfSize)),
            G_TX_RENDERTILE,
            0,
            0,
            scale,
            scale
        );
        gDPPipeSync(gfx++);
    }

    g_AnimatedIconFrameTime++;
    if (g_AnimatedIconFrameTime == MODE_ICON_FRAME_DURATION) {
        iconIndex = f_ModeChoiceIcons[g_ChosenPlayerCount - 1][g_MenuMemory.modeChoice];
        g_AnimatedIconFrameTime = 0;
        g_AnimatedIconFrame++;
        if (g_AnimatedIconFrame >= MODE_ICON_FRAME_COUNT) {
            g_AnimatedIconFrame = 0;
        }

        if (g_ModeIconAnimations[iconIndex][g_AnimatedIconFrame] == END_OF_ICON_FRAMES) {
            g_AnimatedIconFrame = 0;
        }

        g_AnimatedIconBufferIndex = g_AnimatedIconBufferIndex == 0;
        OpenEncodedAsset(g_ModeIconAnimations[iconIndex][g_AnimatedIconFrame], &asset);
        ReadEncodedAsset(g_AnimatedIconImages[g_AnimatedIconBufferIndex], &asset);
    }

    *displayListHead = gfx;
}

void DrawMenuCharacters(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 rival = NO_RIVAL;
    MenuMemory *memory;
    s32 characterCount;
    s32 row;
    s32 column;
    s32 layout;
    CharacterIndex characterIndex;
    CButtonPalette paletteIndex;
    s32 raise;
    u16 slot;
    Vec3 translation;
    Vec3 rotation;
    Vec3 scale;
    s32 unused[12] UNUSED;
    Mat4 transform;

    memory = GetMenuMemory();
    characterCount = g_ChosenPlayerCount;
    if (f_ModeChoiceIcons[characterCount - 1][memory->modeChoice] == MODE_ICON_GET_CHARACTER) {
        if (characterCount == 1) {
            for (row = 0; row != CHARACTER_CHOICE_ROW_COUNT; row++) {
                for (column = 0; column != CHARACTER_GRID_COLUMN_COUNT; column++) {
                    if (g_CharacterGrid[row][column] < MARKED_CHARACTER_LIMIT) {
                        rival = f_MarkedCharacters
                            [g_CharacterGrid[row][column] + MARKED_CHARACTER_OFFSET];
                    }
                }
            }
        }
    }

    characterCount += IsRivalFound(rival);
    switch (characterCount) {
        case 1:
            f_MenuCharacterScales[0] = MENU_CHARACTER_SOLO_SCALE;
            break;
        case 2:
            f_MenuCharacterScales[0] =
                IsRivalFound(rival) ? MENU_CHARACTER_SOLO_SCALE : MENU_CHARACTER_PAIR_SCALE;
            f_MenuCharacterScales[1] =
                IsRivalFound(rival) ? MENU_RIVAL_SCALE : MENU_CHARACTER_PAIR_SCALE;
            break;
        case 3:
        case 4:
            f_MenuCharacterScales[0] = MENU_CHARACTER_FRONT_SCALE;
            f_MenuCharacterScales[1] = MENU_CHARACTER_BACK_SCALE;
            f_MenuCharacterScales[2] = MENU_CHARACTER_BACK_SCALE;
            f_MenuCharacterScales[3] = MENU_CHARACTER_FRONT_SCALE;
            break;
        default:
            f_MenuCharacterScales[0] = MENU_CHARACTER_DEFAULT_SCALE;
            f_MenuCharacterScales[1] = MENU_CHARACTER_DEFAULT_SCALE;
            f_MenuCharacterScales[2] = MENU_CHARACTER_DEFAULT_SCALE;
            f_MenuCharacterScales[3] = MENU_CHARACTER_DEFAULT_SCALE;
            break;
    }

    guOrtho(
        &f_MenuCharacterProjection,
        -(SCREEN_WIDTH / 2),
        SCREEN_WIDTH / 2,
        -(SCREEN_HEIGHT / 2),
        SCREEN_HEIGHT / 2,
        MENU_CHARACTERS_NEAR,
        MENU_CHARACTERS_FAR,
        MENU_CHARACTERS_ORTHO_SCALE
    );
    for (slot = 0; slot != characterCount; slot++) {
        if (g_MenuFidgetDelays[slot] > 0) {
            g_MenuFidgetDelays[slot]--;
        }

        if (g_MenuFidgetDelays[slot] == 0) {
            g_MenuFidgetDelays[slot] = FIDGETING;
            SetAnimationLooping(slot, FALSE);
            SetNextAnimation(slot, MENU_ANIMATION_FIDGET);
        }

        if (g_MenuFidgetDelays[slot] == FIDGETING) {
            if (GetAnimationEndFrame(slot, MENU_ANIMATION_FIDGET) == GetAnimationFrame(slot)) {
                SetCharacterAnimation(slot, MENU_ANIMATION_STANDING);
                SetAnimationLooping(slot, TRUE);
                g_MenuFidgetDelays[slot] = guRandom() % FIDGET_DELAY_RANGE;
            }
        }

        if (IsRivalSlot(characterCount, rival, slot)) {
            characterIndex = rival;
            paletteIndex = LEFT_C_PALETTE;
        }
        else {
            characterIndex = g_Players[slot].characterIndex;
            paletteIndex = g_Players[slot].paletteIndex;
        }

        if (!g_IsMenuCharacterLoaded[slot]) {
            if (IsCharacterSlotBusy(slot)) {
                FreeCharacterSlot(slot);
            }
            else {
                LoadCharacter(slot, characterIndex + CHARACTER_COUNT, paletteIndex);
                g_IsMenuCharacterLoaded[slot] = TRUE;
                SetCharacterAnimation(slot, MENU_ANIMATION_STANDING);
                SetAnimationSpeed(slot, 1.0f);
                if (IsRivalSlot(characterCount, rival, slot)) {
                    SetCharacterLighting(RIVAL_SLOT, CHARACTER_LIGHTING_BLACK);
                }
            }
        }

        if (IsRivalFound(rival)) {
            layout = SOLO_CHARACTER_LAYOUT;
        }
        else {
            layout = characterCount - 1;
        }

        if (IsRivalSlot(characterCount, rival, slot)) {
            f_RivalSpinYaw -= RIVAL_SPIN_STEP;
            f_CharacterYaw = f_RivalSpinYaw;
        }
        else {
            f_CharacterYaw = 0.0f;
        }

        rotation[AXIS_X] = DegreesToRadians(f_CharacterPitch);
        rotation[AXIS_Y] = DegreesToRadians(f_CharacterYaw);
        rotation[AXIS_Z] = 0.0f;
        translation[AXIS_X] = f_MenuCharacterPositions[layout][slot][AXIS_X];
        raise = (characterIndex == CHARACTER_BABY_MARIO) ? BABY_MARIO_RAISE : 0;
        translation[AXIS_Y] = f_MenuCharacterPositions[layout][slot][AXIS_Y] + raise;
        translation[AXIS_Z] = f_MenuCharacterPositions[layout][slot][AXIS_Z];
        scale[AXIS_X] = f_MenuCharacterScales[slot];
        scale[AXIS_Y] = f_MenuCharacterScales[slot];
        scale[AXIS_Z] = f_MenuCharacterScales[slot];
        BuildTransformMatrix(transform, rotation, translation, scale);
        ConvertMatrixToFixed(transform, &f_MenuCharacterTransforms[slot][g_DrawBufferIndex]);
        gDPPipeSync(gfx++);
        gDPSetTexturePersp(gfx++, G_TP_PERSP);
        gDPPipeSync(gfx++);
        gDPSetTextureFilter(gfx++, G_TF_BILERP);
        gDPPipeSync(gfx++);
        gSPMatrix(
            gfx++,
            PhysicalAddress(&f_MenuCharacterProjection),
            G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION
        );
        gSPMatrix(
            gfx++,
            PhysicalAddress(&f_MenuCharacterTransforms[slot][g_DrawBufferIndex]),
            G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW
        );
        g_DisplayListHead = gfx;
        DrawCharacter(slot);
        *displayListHead = g_DisplayListHead;
        gfx = *displayListHead;
        AnimateCharacter(slot);
    }
}

#define VIEWER_NAME_X 32
#define VIEWER_NAME_Y 192
#define VIEWER_PLAYER_NUMBER_X 144
#define VIEWER_PLAYER_NAME_X 176
#define VIEWER_PLAYER_TOP 64
#define VIEWER_PLAYER_SPACING 18
#define VIEWER_ANIMATION_X 64
#define VIEWER_ANIMATION_Y 56
#define VIEWER_CLUB_X 144
#define VIEWER_POWER_Y 184
#define VIEWER_SPIN_Y 192
#define VIEWER_SIDE_SPIN_Y 200
#define VIEWER_STICK_DEAD_ZONE 20
#define VIEWER_STICK_DIVISOR 4
#define VIEWER_SCALE_STEP 0.05f
#define VIEWER_STEP_SPEED 1.0f
#define VIEWER_FAST_SPEED 4.0f
#define VIEWER_LAST_ANIMATION 34
#define VIEWER_FIRST_SECOND_MODEL_ANIMATION 12
#define VIEWER_SECOND_MODEL_OFFSET 18
#define VIEWER_KIND_COUNT 5
#define VIEWER_HALF_WIDTH 160.0f
#define VIEWER_HALF_HEIGHT 120.0f
#define VIEWER_NEAR 1.0f
#define VIEWER_FAR 3000.0f
#define VIEWER_PERSPECTIVE_SCALE 10.0f
#define VIEWER_BASE_SCALE 5.0f
#define VIEWER_BASE_OFFSET (-64.0f)
#define VIEWER_DEPTH (-1000.0f)

void DrawAnimationViewer(Gfx **displayListHead) {
    Gfx *gfx;
    s32 index;
    s32 stickX;
    s32 stickY;
    CharacterIndex characterIndex;

    gfx = *displayListHead;

    if (g_ControllerStates[0].pressedButtons & START_BUTTON) {
        f_IsViewerAdjusting ^= TRUE;
    }

    if (f_IsViewerAdjusting == FALSE) {
        DrawMenuBackdropLayers(&gfx, PLAIN_BACKDROP_LAYERS);
    }

    SetUpTextDrawing(&gfx);
    gDPSetPrimColor(gfx++, 0, 0, 0x3F, 0x7F, 0xFF, OPAQUE_ALPHA);
    gDPSetEnvColor(gfx++, 0x00, 0x10, 0x40, OPAQUE_ALPHA);
    sprintf(
        g_PrintBuffer,
        "%d:%s",
        g_MenuMemory.viewerCharacter + 1,
        g_MenuCharacterNames[g_MenuMemory.viewerCharacter]
    );
    DrawLargeText(&gfx, VIEWER_NAME_X, VIEWER_NAME_Y, g_PrintBuffer);
    for (index = 0; index != g_ChosenPlayerCount; index++) {
        sprintf(g_PrintBuffer, "%d:", index + 1);
        DrawTextStub(
            &gfx,
            VIEWER_PLAYER_NUMBER_X,
            VIEWER_PLAYER_TOP + (index * VIEWER_PLAYER_SPACING),
            g_PrintBuffer
        );
    }

    for (index = 0; index != g_ChoosingPlayerIndex; index++) {
        sprintf(g_PrintBuffer, "%s", g_MenuCharacterNames[g_ChosenCharacters[index]]);
        DrawTextStub(
            &gfx,
            VIEWER_PLAYER_NAME_X,
            VIEWER_PLAYER_TOP + (index * VIEWER_PLAYER_SPACING),
            g_PrintBuffer
        );
    }

    SetUpShadowedTextDrawing(&gfx);
    SetTextColor(&gfx, 255, 255, 255);
    sprintf(g_PrintBuffer, "%d", f_ViewerAnimation);
    DrawShadowedText(&gfx, VIEWER_ANIMATION_X, VIEWER_ANIMATION_Y, g_PrintBuffer);
    sprintf(
        g_PrintBuffer,
        "POWER:%3d",
        g_Clubs[g_MenuMemory.viewerCharacter * CLUB_SET_SIZE].distance
    );
    DrawShadowedText(&gfx, VIEWER_CLUB_X, VIEWER_POWER_Y, g_PrintBuffer);
    sprintf(g_PrintBuffer, "SPIN :%3d", g_Clubs[g_MenuMemory.viewerCharacter * CLUB_SET_SIZE].spin);
    DrawShadowedText(&gfx, VIEWER_CLUB_X, VIEWER_SPIN_Y, g_PrintBuffer);
    if (g_Clubs[g_MenuMemory.viewerCharacter * CLUB_SET_SIZE].sideSpin > 0) {
        DrawShadowedText(&gfx, VIEWER_CLUB_X, VIEWER_SIDE_SPIN_Y, "DRAW");
    }
    else if (g_Clubs[g_MenuMemory.viewerCharacter * CLUB_SET_SIZE].sideSpin < 0) {
        DrawShadowedText(&gfx, VIEWER_CLUB_X, VIEWER_SIDE_SPIN_Y, "FADE");
    }
    else {
        DrawShadowedText(&gfx, VIEWER_CLUB_X, VIEWER_SIDE_SPIN_Y, "STRAIGHT");
    }

    if (f_IsViewerAdjusting == FALSE) {
        ClearGameFlag(GAME_FLAG_SHOW_ANIMATION_FRAMES);
    }
    else {
        stickX = g_ControllerStates[0].stickX;
        stickY = g_ControllerStates[0].stickY;
        SetGameFlag(GAME_FLAG_SHOW_ANIMATION_FRAMES);
        if (!(g_ControllerStates[0].buttons & Z_TRIG)) {
            if (stickX > 0) {
                stickX -= VIEWER_STICK_DEAD_ZONE;
                if (stickX < 0) {
                    stickX = 0;
                }

                stickX /= VIEWER_STICK_DIVISOR;
            }
            else if (stickX < 0) {
                stickX += VIEWER_STICK_DEAD_ZONE;
                if (stickX > 0) {
                    stickX = 0;
                }

                stickX /= VIEWER_STICK_DIVISOR;
            }

            if (stickY > 0) {
                stickY -= VIEWER_STICK_DEAD_ZONE;
                if (stickY < 0) {
                    stickY = 0;
                }

                stickY /= VIEWER_STICK_DIVISOR;
            }
            else if (stickY < 0) {
                stickY += VIEWER_STICK_DEAD_ZONE;
                if (stickY > 0) {
                    stickY = 0;
                }

                stickY /= VIEWER_STICK_DIVISOR;
            }

            f_ViewerPitch += stickY;
            f_ViewerYaw += stickX;
            if (g_ControllerStates[0].buttons & L_CBUTTONS) {
                f_ViewerOffsetX -= 1.0f;
            }

            if (g_ControllerStates[0].buttons & R_CBUTTONS) {
                f_ViewerOffsetX += 1.0f;
            }

            if (g_ControllerStates[0].buttons & U_CBUTTONS) {
                f_ViewerOffsetY += 1.0f;
            }

            if (g_ControllerStates[0].buttons & D_CBUTTONS) {
                f_ViewerOffsetY -= 1.0f;
            }

            if (g_ControllerStates[0].buttons & B_BUTTON) {
                f_ViewerScaleOffset -= VIEWER_SCALE_STEP;
            }

            if (g_ControllerStates[0].buttons & A_BUTTON) {
                f_ViewerScaleOffset += VIEWER_SCALE_STEP;
            }
        }

        if ((g_MenuPressedButtons & L_JPAD) && f_ViewerAnimation > 0) {
            f_ViewerAnimation--;
            f_IsViewerAnimationChanged = TRUE;
        }

        if ((g_MenuPressedButtons & R_JPAD) && f_ViewerAnimation < VIEWER_LAST_ANIMATION) {
            f_ViewerAnimation++;
            f_IsViewerAnimationChanged = TRUE;
        }

        if (g_ControllerStates[0].buttons & Z_TRIG) {
            SetAnimationSpeed(f_ViewerCharacterSlot, 0.0f);
            if (g_ControllerStates[0].repeatedButtons & L_CBUTTONS) {
                SetAnimationSpeed(f_ViewerCharacterSlot, -VIEWER_STEP_SPEED);
            }

            if (g_ControllerStates[0].repeatedButtons & R_CBUTTONS) {
                SetAnimationSpeed(f_ViewerCharacterSlot, VIEWER_STEP_SPEED);
            }

            if (g_ControllerStates[0].buttons & U_CBUTTONS) {
                SetAnimationSpeed(f_ViewerCharacterSlot, VIEWER_FAST_SPEED);
            }

            if (g_ControllerStates[0].buttons & D_CBUTTONS) {
                SetAnimationSpeed(f_ViewerCharacterSlot, -VIEWER_FAST_SPEED);
            }

            AnimateAllCharacters();
            SetAnimationSpeed(f_ViewerCharacterSlot, 0.0f);
        }

        if (g_ControllerStates[0].buttons & R_TRIG) {
            if (!(g_ControllerStates[0].buttons & Z_TRIG)) {
                SetCharacterAnimation(f_ViewerCharacterSlot, f_ViewerAnimation);
            }

            SetAnimationSpeed(f_ViewerCharacterSlot, 1.0f);
        }

        if ((g_MenuRepeatButtons & L_TRIG) && f_IsViewerModelKindChanged == FALSE) {
            f_ViewerModelKind++;
            if (f_ViewerModelKind >= VIEWER_KIND_COUNT) {
                f_ViewerModelKind = 0;
            }

            if (f_ViewerModelKind < 0) {
                f_ViewerModelKind = VIEWER_KIND_COUNT - 1;
            }

            f_IsViewerModelKindChanged = TRUE;
        }
    }

    if (f_ViewerLoadedCharacter != g_MenuMemory.viewerCharacter ||
        f_IsViewerAnimationChanged == TRUE ||
        f_IsViewerModelKindChanged == TRUE) {
        if (IsCharacterSlotBusy(f_ViewerCharacterSlot)) {
            FreeCharacterSlot(f_ViewerCharacterSlot);
        }
        else {
            characterIndex = g_MenuMemory.viewerCharacter;
            f_IsViewerModelKindChanged = FALSE;
            f_IsViewerAnimationChanged = FALSE;
            f_ViewerLoadedCharacter = characterIndex;
            if (f_ViewerAnimation >= VIEWER_FIRST_SECOND_MODEL_ANIMATION) {
                LoadCharacter(
                    f_ViewerCharacterSlot,
                    characterIndex + VIEWER_SECOND_MODEL_OFFSET,
                    f_ViewerModelKind
                );
            }
            else {
                LoadCharacter(f_ViewerCharacterSlot, characterIndex, f_ViewerModelKind);
            }

            SetCharacterAnimation(f_ViewerCharacterSlot, f_ViewerAnimation);
            SetAnimationSpeed(f_ViewerCharacterSlot, 0.0f);
        }
    }

    AnimateAllCharacters();
    guOrtho(
        &f_ViewerProjection,
        -VIEWER_HALF_WIDTH,
        VIEWER_HALF_WIDTH,
        -VIEWER_HALF_HEIGHT,
        VIEWER_HALF_HEIGHT,
        VIEWER_NEAR,
        VIEWER_FAR,
        VIEWER_PERSPECTIVE_SCALE
    );
    {
        Vec3 translation;
        Vec3 rotation;
        Vec3 scale;
        Mat4 matrix;

        rotation[AXIS_Z] = 0;
        rotation[AXIS_X] = DegreesToRadians(f_ViewerPitch);
        rotation[AXIS_Y] = DegreesToRadians(f_ViewerYaw);
        translation[AXIS_X] = f_ViewerOffsetX + VIEWER_BASE_OFFSET;
        translation[AXIS_Y] = f_ViewerOffsetY + VIEWER_BASE_OFFSET;
        translation[AXIS_Z] = VIEWER_DEPTH;
        scale[AXIS_X] = f_ViewerScaleOffset + VIEWER_BASE_SCALE;
        scale[AXIS_Y] = f_ViewerScaleOffset + VIEWER_BASE_SCALE;
        scale[AXIS_Z] = f_ViewerScaleOffset + VIEWER_BASE_SCALE;
        BuildTransformMatrix(matrix, rotation, translation, scale);
        ConvertMatrixToFixed(matrix, &f_ViewerTransforms[g_DrawBufferIndex]);
    }

    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_PERSP);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gSPMatrix(
        gfx++,
        PhysicalAddress(&f_ViewerProjection),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION
    );
    gSPMatrix(
        gfx++,
        PhysicalAddress(&f_ViewerTransforms[g_DrawBufferIndex]),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW
    );
    gDPPipeSync(gfx++);
    g_DisplayListHead = gfx;
    DrawCharacter(f_ViewerCharacterSlot);
    *displayListHead = g_DisplayListHead;
    gfx = *displayListHead;
}

void DrawCourseSelect(Gfx **displayListHead) {
    Gfx *gfx;
    u16 perspectiveNormalization;
    u16 wheelPerspNorm;
    u8 unusedStackBeforeIcons[0x10] UNUSED;
    f32 iconX[COURSE_COUNT];
    f32 iconY[COURSE_COUNT];
    f32 iconScale[COURSE_COUNT];
    u8 unusedStackAfterIcons[0x18] UNUSED;
    s32 drawOrder[COURSE_COUNT];
    s16 places[TOURNAMENT_PLACE_COUNT];
    SaveData *save;
    s32 courseCount;
    s32 choice;
    s32 chosenIndex;
    s32 spin;
    s32 texelsOffset;
    s32 angleStep;
    s32 index;
    s32 otherIndex;
    s32 key;
    s32 drawnIndex;
    s32 halfSize;
    s32 texelStep;
    s32 stripTop;
    s32 stripOffset;
    Radians angle;
    f32 wheelX;
    f32 bobOffset;
    PaletteColor *palette;
    u8 *texels;
    s16 badgeCount;
    s32 placeCount;
    s32 red;
    s32 green;
    s32 blue;
    s16 left;
    s16 top;
    u32 trophyPalette;
    u32 trophyImage;

    save = GetSaveData();
    gfx = *displayListHead;
    if (g_MenuScreen == MENU_SCREEN_COURSE_SELECT) {
        DrawMenuBackdropLayers(&gfx, COURSE_SELECT_BACK_LAYERS);
        guTranslate(&f_WaterTranslation, 0.0f, 0.0f, -WATER_PLANE_DISTANCE);
        guPerspective(
            &f_WaterProjection,
            &perspectiveNormalization,
            WATER_FIELD_OF_VIEW,
            SCREEN_ASPECT_RATIO,
            WHEEL_NEAR_PLANE,
            WHEEL_FAR_PLANE,
            WATER_PERSPECTIVE_SCALE
        );
        gSPPerspNormalize(gfx++, perspectiveNormalization);
        gDPPipeSync(gfx++);
        gDPPipeSync(gfx++);
        gDPSetCycleType(gfx++, G_CYC_2CYCLE);
        gDPPipeSync(gfx++);
        gSPFogPosition(gfx++, WATER_FOG_START, WATER_FOG_END);
        gDPSetFogColor(gfx++, 255, 255, 255, 255);
        gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
        gDPSetCombineMode(gfx++, G_CC_DECALRGB, G_CC_DECALRGB);
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
        gDPPipeSync(gfx++);
        gDPSetAlphaDither(gfx++, G_AD_PATTERN);
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_NONE);
        gDPPipeSync(gfx++);
        gDPSetTexturePersp(gfx++, G_TP_PERSP);
        gDPPipeSync(gfx++);
        gDPSetTextureFilter(gfx++, G_TF_BILERP);
        gSPLoadGeometryMode(gfx++, 0);
        gSPSetGeometryMode(gfx++, G_SHADE | G_SHADING_SMOOTH);
        gDPSetCombineLERP(
            gfx++,
            TEXEL0,
            TEXEL1,
            PRIMITIVE_ALPHA,
            TEXEL1,
            0,
            0,
            0,
            1,
            SHADE,
            0,
            COMBINED,
            0,
            0,
            0,
            0,
            COMBINED
        );
        gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, WATER_BLEND_ALPHA);
        gDPLoadTextureBlock(
            gfx++,
            TextureAddress(ImageBody(ImageAt(g_MenuCourseSceneImage, COURSE_SELECT_WATER_OFFSET))),
            G_IM_FMT_RGBA,
            G_IM_SIZ_16b,
            WATER_TEXTURE_SIZE,
            WATER_TEXTURE_SIZE,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            WATER_TEXTURE_MASK,
            WATER_TEXTURE_MASK,
            WATER_TEXTURE_SHIFT,
            WATER_TEXTURE_SHIFT
        );
        gDPLoadMultiBlock(
            gfx++,
            TextureAddress(ImageBody(ImageAt(g_MenuCourseSceneImage, COURSE_SELECT_LAYER_OFFSET))),
            WATER_LAYER_TMEM,
            WATER_LAYER_TILE,
            G_IM_FMT_RGBA,
            G_IM_SIZ_16b,
            WATER_TEXTURE_SIZE,
            WATER_TEXTURE_SIZE,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            WATER_TEXTURE_MASK,
            WATER_TEXTURE_MASK,
            WATER_TEXTURE_SHIFT,
            WATER_TEXTURE_SHIFT
        );
        gDPPipeSync(gfx++);
        f_WaterScroll++;
        gDPSetTileSize(
            gfx++,
            G_TX_RENDERTILE,
            f_WaterScroll,
            0,
            f_WaterScroll + WATER_TILE_SPAN,
            WATER_TILE_SPAN
        );
        gDPSetTileSize(
            gfx++,
            WATER_LAYER_TILE,
            -f_WaterScroll,
            f_WaterScroll,
            -f_WaterScroll + WATER_TILE_SPAN,
            f_WaterScroll + WATER_TILE_SPAN
        );
        gDPPipeSync(gfx++);
        gSPMatrix(
            gfx++,
            PhysicalAddress(&f_WaterProjection),
            G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION
        );
        gSPMatrix(
            gfx++,
            PhysicalAddress(&f_WaterTranslation),
            G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW
        );
        gDPPipeSync(gfx++);
        gSPVertex(gfx++, f_WaterVertices, WATER_VERTEX_COUNT, 0);
        gDPPipeSync(gfx++);
        gSP1Triangle(gfx++, 0, 1, 2, 0);
        gSP1Triangle(gfx++, 4, 2, 3, 0);
        gSP1Triangle(gfx++, 0, 2, 4, 0);
        gDPPipeSync(gfx++);
        guPerspectiveF(
            f_CourseSelectProjection,
            &wheelPerspNorm,
            WHEEL_FIELD_OF_VIEW,
            SCREEN_ASPECT_RATIO,
            WHEEL_NEAR_PLANE,
            WHEEL_FAR_PLANE,
            1.0f
        );
        guLookAtF(
            f_CourseSelectView,
            0.0f,
            COURSE_SELECT_EYE_HEIGHT,
            COURSE_SELECT_EYE_DISTANCE,
            0.0f,
            0.0f,
            0.0f,
            0.0f,
            1.0f,
            0.0f
        );
        SetCpuProjectionF(f_CourseSelectProjection, f_CourseSelectView, g_Viewports);
        gDPPipeSync(gfx++);
        gDPSetCycleType(gfx++, G_CYC_1CYCLE);
        gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
        gDPPipeSync(gfx++);
        gDPSetTextureFilter(gfx++, G_TF_BILERP);
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_RGBA16);
        gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
        gDPPipeSync(gfx++);
        gDPSetTexturePersp(gfx++, G_TP_NONE);
        if (g_GameState.isMiniGolf) {
            choice = g_MenuMemory.miniGolfRecordChoice;
            courseCount = MINI_GOLF_DISC_IMAGE_COUNT;
            angleStep = DEGREES_PER_TURN / MINI_GOLF_DISC_IMAGE_COUNT;
            texelsOffset = 0;
            spin = g_MenuMemory.miniGolfRecordSlide;
        }
        else {
            courseCount = GetOpenCourseCount();
            courseCount = (courseCount < COURSE_COUNT) ? (courseCount + 1) : COURSE_COUNT;

            choice = g_MenuMemory.courseChoice;
            angleStep = DEGREES_PER_TURN / courseCount;
            spin = g_MenuMemory.courseSpin;
            texelsOffset = LARGE_PALETTED_TEXELS_OFFSET;
        }

        for (index = 0; index != courseCount; index++) {
            ScreenPoint point;

            angle = -DegreesToRadians(
                (((choice - index) % courseCount) * angleStep) + (spin * DEGREES_PER_SPIN_STEP)
            );
            wheelX = sinf(angle) * WHEEL_RADIUS_X;
            if (ProjectPoint(wheelX, 0.0f, cosf(angle) * WHEEL_RADIUS_Z, &point) < 0) {
                iconX[index] = point.x;
                iconY[index] = point.y - WHEEL_RAISE;
                iconScale[index] = point.scaleX;
            }
        }

        for (otherIndex = 0; otherIndex != courseCount; otherIndex++) {
            drawOrder[otherIndex] = otherIndex;
        }

        for (otherIndex = 1; otherIndex != courseCount; otherIndex++) {
            key = drawOrder[otherIndex];
            for (index = otherIndex - 1; (index >= 0) && (iconY[drawOrder[index]] > iconY[key]);
                index--) {
                drawOrder[index + 1] = drawOrder[index];
            }

            drawOrder[index + 1] = key;
        }

        gDPPipeSync(gfx++);
        gDPSetDepthSource(gfx++, G_ZS_PRIM);
        gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
        f_FrontCourseBobPhase += FRONT_COURSE_BOB_PHASE_STEP;
        for (index = 0; index != courseCount; index++) {
            drawnIndex = drawOrder[index];
            halfSize = iconScale[drawnIndex] * COURSE_ICON_HALF_SIZE;
            texelStep = (f32)(COURSE_ICON_STRIP_HEIGHT << TEXEL_STEP_SHIFT) / halfSize;
            if (index == (courseCount - 1)) {
                bobOffset = sinf(f_FrontCourseBobPhase) * FRONT_COURSE_BOB_HEIGHT;
            }
            else {
                bobOffset = 0.0f;
            }

            chosenIndex = choice;
            if (g_GameState.isMiniGolf == TRUE) {
                palette = g_MiniGolfDiscImages[drawnIndex];
                texels =
                    MENU_ICON_IMAGE_PAIRS[0]
                                         [(f_MiniGolfDiscBufferIndex * MINI_GOLF_DISC_IMAGE_COUNT) +
                                             drawnIndex];
            }
            else {
                palette = (PaletteColor *)ImageBody(MENU_ICON_IMAGE_PAIRS[0][drawnIndex]);
                texels = MENU_ICON_IMAGE_PAIRS[0][drawnIndex];
            }

            gDPLoadTLUT_pal256(gfx++, TextureAddress(palette));
            if (chosenIndex == drawnIndex) {
                gDPPipeSync(gfx++);
                gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE | Z_UPD, G_RM_AA_TEX_EDGE2 | Z_UPD);
                gDPSetPrimDepth(gfx++, CHOSEN_COURSE_DEPTH, 0);
            }
            else {
                gDPPipeSync(gfx++);
                gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
            }

            if (!g_GameState.isMiniGolf) {
                if ((drawnIndex == (courseCount - 1)) && (GetOpenCourseCount() < COURSE_COUNT)) {
                    gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
                    gDPSetPrimColor(
                        gfx++,
                        0,
                        0,
                        LOCKED_COURSE_SHADE,
                        LOCKED_COURSE_SHADE,
                        LOCKED_COURSE_SHADE,
                        OPAQUE_ALPHA
                    );
                }
                else {
                    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
                }
            }

            for (otherIndex = 0; otherIndex != COURSE_ICON_STRIP_COUNT; otherIndex++) {
                stripOffset =
                    texelsOffset + (otherIndex * COURSE_ICON_WIDTH * COURSE_ICON_STRIP_HEIGHT);
                stripTop = otherIndex * halfSize;
                gDPLoadTextureBlock(
                    gfx++,
                    TextureAddress(texels + stripOffset),
                    G_IM_FMT_CI,
                    G_IM_SIZ_8b,
                    COURSE_ICON_WIDTH,
                    COURSE_ICON_STRIP_HEIGHT,
                    0,
                    G_TX_NOMIRROR | G_TX_WRAP,
                    G_TX_NOMIRROR | G_TX_WRAP,
                    G_TX_NOMASK,
                    G_TX_NOMASK,
                    G_TX_NOLOD,
                    G_TX_NOLOD
                );
                gDPPipeSync(gfx++);
                gSPScisTextureRectangle(
                    gfx++,
                    QuarterPixels((s32)(iconX[drawnIndex] - halfSize)),
                    QuarterPixels((s32)((iconY[drawnIndex] - halfSize) + stripTop + bobOffset)),
                    QuarterPixels((s32)(iconX[drawnIndex] + halfSize)),
                    QuarterPixels((s32)(iconY[drawnIndex] + stripTop + bobOffset)),
                    G_TX_RENDERTILE,
                    0,
                    0,
                    texelStep,
                    texelStep
                );
                gDPPipeSync(gfx++);
            }

            if ((g_GameState.isMiniGolf == TRUE) &&
                (g_MenuMemory.miniGolfRecordChoice == drawnIndex)) {
                if (spin == 0) {
                    f_MiniGolfChoiceCenterY = iconY[drawnIndex];
                }
                else if (f_MiniGolfChoiceCenterY == 0.0f) {
                    f_MiniGolfChoiceCenterY = iconY[drawnIndex];
                }

                DrawUpDownArrows(&gfx, f_MiniGolfChoiceCenterY + bobOffset);
            }
        }

        gDPPipeSync(gfx++);
        gDPSetDepthSource(gfx++, G_ZS_PIXEL);
        gDPPipeSync(gfx++);
        gDPSetTexturePersp(gfx++, G_TP_NONE);
        if (COURSE_SELECT_LAYOUT == COURSE_SELECT_LAYOUT_AWARDS) {
            badgeCount = 0;
            for (otherIndex = 0; otherIndex != COURSE_HOLE_COUNT; otherIndex++) {
                if (save->records.awards.birdieBadgeCounts[g_MenuMemory.courseChoice][otherIndex] !=
                    0) {
                    badgeCount++;
                }
            }

            if (badgeCount < TWO_DIGIT_WON_COUNT_MIN) {
                otherIndex = DIGIT_GLYPH_SIZE;
            }

            if ((badgeCount >= TWO_DIGIT_WON_COUNT_MIN) &&
                (badgeCount < THREE_DIGIT_WON_COUNT_MIN)) {
                otherIndex = 0;
            }

            if (badgeCount >= THREE_DIGIT_WON_COUNT_MIN) {
                otherIndex = -DIGIT_GLYPH_SIZE;
            }

            DrawSpinningBadge(
                &gfx,
                g_SpinningBadgeImage,
                otherIndex + COURSE_BADGE_ICON_X,
                COURSE_BADGE_ICON_Y
            );
        }

        if (COURSE_SELECT_LAYOUT == COURSE_SELECT_LAYOUT_HOLES) {
            gDPPipeSync(gfx++);
            gDPPipeSync(gfx++);
            gDPSetTexturePersp(gfx++, G_TP_NONE);
            gDPPipeSync(gfx++);
            gDPSetTextureFilter(gfx++, G_TF_BILERP);
            gDPPipeSync(gfx++);
            gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
            gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImagePalette(g_HoleBadgeImages)));
            for (otherIndex = 0; otherIndex != HOLES_BADGE_COUNT; otherIndex++) {
                if ((otherIndex == HOLES_BADGE_BET) && (g_AreBetPagesBlocked != TRUE)) {
                    gDPPipeSync(gfx++);
                    gDPLoadTextureTile_4b(
                        gfx++,
                        TextureAddress(
                            ImageTexels(g_HoleBadgeImages) + (otherIndex * HOLES_BADGE_IMAGE_SIZE)
                        ),
                        G_IM_FMT_CI,
                        HOLES_BADGE_SIZE,
                        HOLES_BADGE_SIZE,
                        0,
                        0,
                        HOLES_BADGE_SIZE - 1,
                        HOLES_BADGE_SIZE - 1,
                        0,
                        G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMASK,
                        G_TX_NOMASK,
                        G_TX_NOLOD,
                        G_TX_NOLOD
                    );
                    gDPPipeSync(gfx++);
                    gSPScisTextureRectangle(
                        gfx++,
                        QuarterPixels(f_HolesBadgeCorners[otherIndex].x),
                        QuarterPixels(f_HolesBadgeCorners[otherIndex].y),
                        QuarterPixels(f_HolesBadgeCorners[otherIndex].x + HOLES_BADGE_SIZE),
                        QuarterPixels(f_HolesBadgeCorners[otherIndex].y + HOLES_BADGE_SIZE),
                        G_TX_RENDERTILE,
                        0,
                        0,
                        TEXEL_STEP_ONE,
                        TEXEL_STEP_ONE
                    );
                    gDPPipeSync(gfx++);
                    f_BetBadgeColorPhase += BET_BADGE_COLOR_STEP;
                    red = (s32)(sinf(f_BetBadgeColorPhase) * BET_BADGE_COLOR_SWING) +
                        BET_BADGE_COLOR_MIDPOINT;
                    blue = (s32)(sinf(f_BetBadgeColorPhase + (M_PI / 2)) * BET_BADGE_COLOR_SWING) +
                        BET_BADGE_COLOR_MIDPOINT;
                    green = (s32)(sinf(f_BetBadgeColorPhase + M_PI) * BET_BADGE_COLOR_SWING) +
                        BET_BADGE_COLOR_MIDPOINT;
                    gDPSetPrimColor(gfx++, 0, 0, red, green, blue, 128);
                    gDPSetEnvColor(
                        gfx++,
                        red / BET_BADGE_ENV_DIVISOR,
                        green / BET_BADGE_ENV_DIVISOR,
                        blue / BET_BADGE_ENV_DIVISOR,
                        OPAQUE_ALPHA
                    );
                    gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
                    gDPPipeSync(gfx++);
                    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
                }
                else if (g_MenuMemory.holesChoice == otherIndex) {
                    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
                }
                else {
                    gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
                    gDPSetPrimColor(
                        gfx++,
                        0,
                        0,
                        UNCHOSEN_HOLES_BADGE_SHADE,
                        UNCHOSEN_HOLES_BADGE_SHADE,
                        UNCHOSEN_HOLES_BADGE_SHADE,
                        OPAQUE_ALPHA
                    );
                }

                gDPPipeSync(gfx++);
                gDPLoadTextureTile_4b(
                    gfx++,
                    TextureAddress(
                        ImageTexels(g_HoleBadgeImages) + (otherIndex * HOLES_BADGE_IMAGE_SIZE)
                    ),
                    G_IM_FMT_CI,
                    HOLES_BADGE_SIZE,
                    HOLES_BADGE_SIZE,
                    0,
                    0,
                    HOLES_BADGE_SIZE - 1,
                    HOLES_BADGE_SIZE - 1,
                    0,
                    G_TX_NOMIRROR | G_TX_WRAP,
                    G_TX_NOMIRROR | G_TX_WRAP,
                    G_TX_NOMASK,
                    G_TX_NOMASK,
                    G_TX_NOLOD,
                    G_TX_NOLOD
                );
                gDPPipeSync(gfx++);
                gSPScisTextureRectangle(
                    gfx++,
                    QuarterPixels(f_HolesBadgeCorners[otherIndex].x),
                    QuarterPixels(f_HolesBadgeCorners[otherIndex].y),
                    QuarterPixels(f_HolesBadgeCorners[otherIndex].x + HOLES_BADGE_SIZE),
                    QuarterPixels(f_HolesBadgeCorners[otherIndex].y + HOLES_BADGE_SIZE),
                    G_TX_RENDERTILE,
                    0,
                    0,
                    TEXEL_STEP_ONE,
                    TEXEL_STEP_ONE
                );
                gDPPipeSync(gfx++);
            }
        }

        gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
        if ((COURSE_SELECT_LAYOUT == COURSE_SELECT_LAYOUT_AWARDS) && !g_GameState.isMiniGolf) {
            for (otherIndex = 0, placeCount = 0; otherIndex != TOURNAMENT_PLACE_COUNT;
                otherIndex++) {
                if (g_CourseStatFigures[g_MenuMemory.courseChoice].hasPlace[otherIndex]) {
                    places[placeCount] = otherIndex;
                    placeCount++;
                }
            }

            if (g_MenuMemory.courseSpin == 0) {
                gDPPipeSync(gfx++);
                gDPPipeSync(gfx++);
                gDPSetCycleType(gfx++, G_CYC_1CYCLE);
                gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
                gDPPipeSync(gfx++);
                gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
                gDPPipeSync(gfx++);
                gDPSetTextureFilter(gfx++, G_TF_BILERP);
                gDPPipeSync(gfx++);
                gDPSetTextureLUT(gfx++, G_TT_RGBA16);
                for (otherIndex = 0; otherIndex != placeCount; otherIndex++) {
                    trophyImage = TextureAddress(g_TrophyFrameBuffers[f_ShownTrophyBufferIndex]);
                    trophyPalette = TextureAddress(g_TrophyPalettes[places[otherIndex]]);
                    gDPLoadTLUT_pal256(gfx++, trophyPalette);
                    gDPLoadTextureTile(
                        gfx++,
                        trophyImage,
                        G_IM_FMT_CI,
                        G_IM_SIZ_8b,
                        g_TrophySizes[g_MenuMemory.courseChoice].width,
                        g_TrophySizes[g_MenuMemory.courseChoice].height,
                        0,
                        0,
                        g_TrophySizes[g_MenuMemory.courseChoice].width - 1,
                        g_TrophySizes[g_MenuMemory.courseChoice].height - 1,
                        0,
                        G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMASK,
                        G_TX_NOMASK,
                        G_TX_NOLOD,
                        G_TX_NOLOD
                    );
                    gDPPipeSync(gfx++);
                    left = f_TrophyCorners[g_MenuMemory.courseChoice][placeCount - 1][otherIndex].x;
                    top = f_TrophyCorners[g_MenuMemory.courseChoice][placeCount - 1][otherIndex].y;
                    gSPScisTextureRectangle(
                        gfx++,
                        QuarterPixels(left),
                        QuarterPixels(top),
                        QuarterPixels(left + g_TrophySizes[g_MenuMemory.courseChoice].width),
                        QuarterPixels(top + g_TrophySizes[g_MenuMemory.courseChoice].height),
                        G_TX_RENDERTILE,
                        0,
                        0,
                        TEXEL_STEP_ONE,
                        TEXEL_STEP_ONE
                    );
                    gDPPipeSync(gfx++);
                }
            }

            badgeCount = 0;
            for (otherIndex = 0; otherIndex != COURSE_HOLE_COUNT; otherIndex++) {
                if (save->records.awards.birdieBadgeCounts[g_MenuMemory.courseChoice][otherIndex] !=
                    0) {
                    badgeCount++;
                }
            }

            if (badgeCount < TWO_DIGIT_WON_COUNT_MIN) {
                otherIndex = DIGIT_GLYPH_SIZE;
            }

            if ((badgeCount >= TWO_DIGIT_WON_COUNT_MIN) &&
                (badgeCount < THREE_DIGIT_WON_COUNT_MIN)) {
                otherIndex = 0;
            }

            if (badgeCount >= THREE_DIGIT_WON_COUNT_MIN) {
                otherIndex = -DIGIT_GLYPH_SIZE;
            }

            gDPPipeSync(gfx++);
            DrawUnalignedMenuNumber(
                &gfx,
                otherIndex + COURSE_BADGE_COUNT_X,
                COURSE_BADGE_COUNT_Y,
                badgeCount,
                MENU_NUMBER_TIMES
            );
            gDPPipeSync(gfx++);
        }

        DrawPageArrows(&gfx);
        DrawCourseSelectBanners(&gfx);
        if (!g_GameState.isMiniGolf &&
            (g_MenuMemory.courseChoice == (courseCount - 1)) &&
            (GetOpenCourseCount() < COURSE_COUNT)) {
            gDPSetPrimColor(gfx++, 0, 0, 64, 64, 64, 191);
            gDPPipeSync(gfx++);
            gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
            gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
            gDPPipeSync(gfx++);
            gSPTextureRectangle(
                gfx++,
                QuarterPixels(LOCKED_PANEL_LEFT),
                QuarterPixels(LOCKED_PANEL_TOP),
                QuarterPixels(LOCKED_PANEL_RIGHT),
                QuarterPixels(LOCKED_PANEL_BOTTOM),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
            gDPPipeSync(gfx++);
            SetUpGradientTextDrawing(&gfx, TRUE);
            gDPSetEnvColor(gfx++, 191, 191, 255, OPAQUE_ALPHA);
            gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, OPAQUE_ALPHA);
            SetTextSlotValue(
                GetCoursePointsNeeded(g_MenuMemory.courseChoice) - save->coursePoints,
                0,
                0
            );
            SetTextSlot(
                LOCKED_COURSE_TEXT_X,
                LOCKED_COURSE_TEXT_Y,
                MESSAGE_POINTS_TO_UNLOCK,
                TEXT_SLOT_LARGE_FONT,
                0,
                TEXT_FLAG_DRAWN_BY_OWNER,
                255,
                255,
                255,
                OPAQUE_ALPHA
            );
            DrawLargeTextSlot(&gfx, 0);
            gDPPipeSync(gfx++);
            gDPPipeSync(gfx++);
            gDPSetCycleType(gfx++, G_CYC_1CYCLE);
            gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
            gDPPipeSync(gfx++);
            gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
            gDPPipeSync(gfx++);
            gDPSetTextureFilter(gfx++, G_TF_BILERP);
            gDPPipeSync(gfx++);
            gDPSetTextureLUT(gfx++, G_TT_RGBA16);
        }

        switch (g_BetPagesWallSlide) {
            case STATS_SLIDE_OUT_RIGHT:
            case STATS_SLIDE_OUT_LEFT:
                if (SlideBrickWall(&gfx, STATS_SLIDE_OUT_RIGHT) == 0) {
                    g_BetPagesPhase = BET_PHASE_CHOOSING;
                }

                break;

            case STATS_SLIDE_IN_FROM_RIGHT:
                if (SlideBrickWall(&gfx, STATS_SLIDE_IN_FROM_RIGHT) == 0) {
                    g_BetPagesPhase = MENU_PHASE_START;
                    g_BetPagesWallSlide = STATS_SLIDE_CURRENT;
                    StartLoadingChosenHole();
                }

                break;

            case STATS_SLIDE_IN_FROM_LEFT:
                if (SlideBrickWall(&gfx, STATS_SLIDE_IN_FROM_LEFT) == 0) {
                    g_BetPagesPhase = MENU_PHASE_START;
                    g_BetPagesWallSlide = STATS_SLIDE_CURRENT;
                    StartLoadingChosenHole();
                }

                break;
        }

        DrawMenuBackdropLayers(&gfx, COURSE_SELECT_FRONT_LAYERS);
    }

    if ((g_MenuScreen == MENU_SCREEN_BET_SETTINGS) || (g_MenuScreen == MENU_SCREEN_HANDICAPS)) {
        switch (g_BetPagesPhase) {
            case BET_PHASE_OPENING:
                f_BetSlideOffset += BET_SLIDE_STEP;
                g_CourseBannersSlide += BET_SLIDE_STEP;
                f_BetSettingsSlide += BET_SLIDE_STEP;
                if (f_BetSlideOffset >= BET_SLIDE_DISTANCE) {
                    g_BetPagesPhase = BET_PHASE_CHOOSING;
                    f_BetSlideOffset = 0;
                    f_BetSettingsSlide = 0;
                }

                break;

            case BET_PHASE_CANCELED:
                f_BetSlideOffset += BET_SLIDE_STEP;
                f_BetSettingsSlide += BET_SLIDE_STEP;
                g_CourseBannersSlide -= BET_SLIDE_STEP;
                if (f_BetSlideOffset >= BET_SLIDE_DISTANCE) {
                    g_BetPagesPhase = MENU_PHASE_START;
                    f_BetSlideOffset = 0;
                    f_BetSettingsSlide = 0;
                    SetTextSlot(
                        0,
                        0,
                        NO_MESSAGE,
                        TEXT_SLOT_LARGE_FONT,
                        0,
                        TEXT_FLAG_TYPED,
                        255,
                        255,
                        255,
                        OPAQUE_ALPHA
                    );
                }

                break;

            case BET_PHASE_CONFIRMED:
                f_BetSlideOffset += BET_SLIDE_STEP;
                f_BetSettingsSlide -= BET_SLIDE_STEP;
                f_HandicapsSlide += BET_SLIDE_STEP;
                if (f_BetSlideOffset >= BET_SLIDE_DISTANCE) {
                    g_BetPagesPhase = MENU_PHASE_CHOOSING;
                    f_BetSlideOffset = 0;
                    f_BetSettingsSlide = 0;
                    f_HandicapsSlide = 0;
                }

                break;

            case MENU_PHASE_CANCELED:
                f_BetSlideOffset += BET_SLIDE_STEP;
                f_BetSettingsSlide += BET_SLIDE_STEP;
                f_HandicapsSlide += BET_SLIDE_STEP;
                if (f_BetSlideOffset >= BET_SLIDE_DISTANCE) {
                    g_BetPagesPhase = BET_PHASE_CHOOSING;
                    f_BetSlideOffset = 0;
                    f_BetSettingsSlide = 0;
                    f_HandicapsSlide = 0;
                }

                break;

            case MENU_PHASE_CONFIRMED:
                f_BetSlideOffset += BET_SLIDE_STEP;
                f_HandicapsSlide -= BET_SLIDE_STEP;
                g_CourseBannersSlide += BET_SLIDE_STEP;
                if (f_BetSlideOffset >= BET_SLIDE_DISTANCE) {
                    g_BetPagesPhase = MENU_PHASE_START;
                    f_BetSlideOffset = 0;
                    f_HandicapsSlide = 0;
                    g_CourseBannersSlide = 0;
                }

                break;
        }

        SlideBrickWall(&gfx, STATS_SLIDE_OUT_RIGHT);
        if ((g_BetPagesPhase >= BET_PHASE_CHOOSING) &&
            ((g_BetPagesPhase != BET_PHASE_CANCELED) &&
                (g_BetPagesPhase != MENU_PHASE_CONFIRMED))) {
            DrawCourseSelectBanners(&gfx);
        }

        DrawMenuBackdropLayers(&gfx, COURSE_SELECT_FRONT_LAYERS);
        switch (g_BetPagesPhase) {
            case BET_PHASE_OPENING:
                DrawBetSettings(&gfx, BET_SLIDE_DISTANCE - f_BetSettingsSlide, 0, 0);
                break;

            case BET_PHASE_CHOOSING:
                DrawBetSettings(&gfx, 0, 0, 1);
                break;

            case BET_PHASE_CANCELED:
                DrawBetSettings(&gfx, f_BetSettingsSlide, 0, 0);
                break;

            case BET_PHASE_CONFIRMED:
                DrawBetSettings(&gfx, f_BetSettingsSlide, 1, 0);
                DrawHandicaps(&gfx, BET_SLIDE_DISTANCE - f_HandicapsSlide, 0, 0);
                break;

            case MENU_PHASE_CHOOSING:
                DrawHandicaps(&gfx, 0, 0, 1);
                break;

            case MENU_PHASE_CANCELED:
                DrawBetSettings(&gfx, f_BetSettingsSlide - BET_SLIDE_DISTANCE, 1, 0);
                DrawHandicaps(&gfx, f_HandicapsSlide, 0, 0);
                break;

            case MENU_PHASE_CONFIRMED:
                DrawHandicaps(&gfx, f_HandicapsSlide, 1, 0);
                break;
        }
    }

    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

#define COURSE_NAME_BANNER_LEFT 67
#define COURSE_NAME_BANNER_TOP 168
#define COURSE_NAME_BANNER_WIDTH 190
#define COURSE_NAME_BANNER_SQUEEZE 48
#define HOLES_BANNER_LEFT 47
#define HOLES_BANNER_TOP 190
#define HOLES_BANNER_WIDTH 230
#define HOLES_BANNER_STRIP_HEIGHT 11
#define COURSE_NAME_Y 176
#define HOLES_CHOICE_Y 202
#define MESSAGE_HOLES_CHOICE_BASE 0xC3
#define HOLES_CHOICE_TEXT_SLOT 1

void DrawCourseSelectBanners(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 slide = g_CourseBannersSlide;

    s32 stripIndex;
    s32 stripTop;
    s32 squeeze;
    CourseSelectLayout layout;

    if (g_BetPagesPhase == MENU_PHASE_CONFIRMED) {
        slide = -slide;
    }

    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal256(gfx++, TextureAddress(ImagePalette(g_NamePlateImage)));
    if (COURSE_SELECT_LAYOUT == COURSE_SELECT_LAYOUT_HOLES) {
        for (stripIndex = 0; stripIndex != NAME_PLATE_STRIP_COUNT; stripIndex++) {
            gDPLoadTextureBlock(
                gfx++,
                TextureAddress(
                    LargeImageTexels(g_NamePlateImage) +
                    stripIndex * NAME_PLATE_WIDTH * NAME_PLATE_STRIP_HEIGHT
                ),
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                NAME_PLATE_WIDTH,
                NAME_PLATE_STRIP_HEIGHT,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gDPPipeSync(gfx++);
            stripTop = HOLES_BANNER_TOP + (stripIndex * HOLES_BANNER_STRIP_HEIGHT);
            gSPScisTextureRectangle(
                gfx++,
                QuarterPixels(HOLES_BANNER_LEFT),
                QuarterPixels(slide + stripTop),
                QuarterPixels(HOLES_BANNER_LEFT + HOLES_BANNER_WIDTH),
                QuarterPixels(slide + stripTop + HOLES_BANNER_STRIP_HEIGHT),
                G_TX_RENDERTILE,
                0,
                0,
                TexelRunStep(NAME_PLATE_WIDTH, HOLES_BANNER_WIDTH),
                TexelRunStep(NAME_PLATE_STRIP_HEIGHT, HOLES_BANNER_STRIP_HEIGHT)
            );
            gDPPipeSync(gfx++);
        }
    }

    squeeze = COURSE_SELECT_LAYOUT != COURSE_SELECT_LAYOUT_AWARDS ? COURSE_NAME_BANNER_SQUEEZE : 0;
    for (stripIndex = 0; stripIndex != NAME_PLATE_STRIP_COUNT; stripIndex++) {
        gDPLoadTextureBlock(
            gfx++,
            TextureAddress(
                LargeImageTexels(g_NamePlateImage) +
                stripIndex * NAME_PLATE_WIDTH * NAME_PLATE_STRIP_HEIGHT
            ),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            NAME_PLATE_WIDTH,
            NAME_PLATE_STRIP_HEIGHT,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPPipeSync(gfx++);
        stripTop = COURSE_NAME_BANNER_TOP + (stripIndex * NAME_PLATE_STRIP_HEIGHT);
        gSPScisTextureRectangle(
            gfx++,
            QuarterPixels(COURSE_NAME_BANNER_LEFT + (squeeze / 2)),
            QuarterPixels(slide + stripTop),
            QuarterPixels(COURSE_NAME_BANNER_LEFT + COURSE_NAME_BANNER_WIDTH - (squeeze / 2)),
            QuarterPixels(slide + stripTop + NAME_PLATE_STRIP_HEIGHT),
            G_TX_RENDERTILE,
            0,
            0,
            TexelRunStep(NAME_PLATE_WIDTH, COURSE_NAME_BANNER_WIDTH - squeeze),
            TEXEL_STEP_ONE
        );
        gDPPipeSync(gfx++);
    }

    SetUpTextDrawing(&gfx);
    SetTextColor(&gfx, 255, 255, 255);
    stripTop = !g_GameState.isMiniGolf;
    if (stripTop != 0) {
        if (g_CourseSelectKind == COURSE_SELECT_KIND_COURSES) {
            sprintf(
                g_PrintBuffer,
                "%d:%s",
                g_MenuMemory.courseChoice + 1,
                g_CourseRecords[g_MenuMemory.courseChoice].name
            );
        }
        else {
            sprintf(
                g_PrintBuffer,
                "%d:%s",
                g_MenuMemory.courseChoice + 1,
                g_TournamentNames[g_MenuMemory.courseChoice]
            );
        }
    }
    else {
        sprintf(
            g_PrintBuffer,
            "%d:%s",
            g_MenuMemory.miniGolfRecordChoice + 1,
            f_MiniGolfRecordNames[g_MenuMemory.miniGolfRecordChoice]
        );
    }

    SetUpGradientTextDrawing(&gfx, 0);
    if (g_GameState.isMiniGolf == TRUE) {
        stripIndex = g_MenuMemory.miniGolfCourseChoice + MINI_GOLF_COURSE_INDEX;
        DrawLargeTextWithShadow(
            &gfx,
            SCREEN_WIDTH / 2,
            slide + COURSE_NAME_Y,
            g_PrintBuffer,
            g_CourseNameEnvColors[stripIndex],
            g_CourseNamePrimColors[stripIndex],
            TEXT_FLAG_CENTERED
        );
    }
    else {
        stripIndex = g_MenuMemory.courseChoice;
        DrawLargeTextWithShadow(
            &gfx,
            SCREEN_WIDTH / 2,
            slide + COURSE_NAME_Y,
            g_PrintBuffer,
            g_CourseNameEnvColors[stripIndex],
            g_CourseNamePrimColors[stripIndex],
            TEXT_FLAG_CENTERED
        );
    }

    layout = COURSE_SELECT_LAYOUT;
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    if (layout == COURSE_SELECT_LAYOUT_HOLES) {
        gDPPipeSync(gfx++);
        SetUpTextDrawing(&gfx);
        SetTextColor(&gfx, 255, 255, 255);
        SetTextSlot(
            SCREEN_WIDTH / 2,
            slide + HOLES_CHOICE_Y,
            g_MenuMemory.holesChoice + MESSAGE_HOLES_CHOICE_BASE,
            TEXT_SLOT_LARGE_FONT,
            HOLES_CHOICE_TEXT_SLOT,
            TEXT_FLAG_CENTERED | TEXT_FLAG_DRAWN_BY_OWNER,
            255,
            255,
            255,
            OPAQUE_ALPHA
        );
        DrawLargeTextSlot(&gfx, HOLES_CHOICE_TEXT_SLOT);
    }

    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void DrawContinueLabel(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;

    DrawShadowedText(&gfx, 96, 64, "CONTINUE");
    *displayListHead = gfx;
}

void DrawMainMenu(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 choiceIndex;

    DrawMenuBackdropLayers(&gfx, BOARD_BACKDROP_LAYERS);
    gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImagePalette(g_ImageSlots[f_ImageSlotIndex])));
    for (choiceIndex = 0; choiceIndex != MAIN_CHOICE_COUNT; choiceIndex++) {
        if (g_MenuMemory.mainChoice != choiceIndex) {
            gDPLoadTextureBlock_4b(
                gfx++,
                TextureAddress(
                    ImageTexels(g_ImageSlots[f_ImageSlotIndex]) +
                    (choiceIndex * MAIN_CHOICE_LABEL_IMAGE_SIZE)
                ),
                G_IM_FMT_CI,
                MAIN_CHOICE_LABEL_WIDTH,
                MAIN_CHOICE_LABEL_HEIGHT,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gDPPipeSync(gfx++);
            gSPScisTextureRectangle(
                gfx++,
                QuarterPixels(f_MainChoiceLabelCorners[choiceIndex].x),
                QuarterPixels(f_MainChoiceLabelCorners[choiceIndex].y),
                QuarterPixels(f_MainChoiceLabelCorners[choiceIndex].x + MAIN_CHOICE_LABEL_WIDTH),
                QuarterPixels(f_MainChoiceLabelCorners[choiceIndex].y + MAIN_CHOICE_LABEL_HEIGHT),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
            gDPPipeSync(gfx++);
        }
    }

    gDPLoadTLUT_pal16(
        gfx++,
        0,
        TextureAddress(ImagePalette(g_MenuLabelImages[f_MenuLabelImageIndex]))
    );
    choiceIndex = g_MenuMemory.mainChoice;
    gDPLoadTextureBlock_4b(
        gfx++,
        TextureAddress(
            ImageTexels(g_MenuLabelImages[f_MenuLabelImageIndex]) +
            (choiceIndex * MAIN_CHOICE_LABEL_IMAGE_SIZE)
        ),
        G_IM_FMT_CI,
        MAIN_CHOICE_LABEL_WIDTH,
        MAIN_CHOICE_LABEL_HEIGHT,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(
        gfx++,
        QuarterPixels(f_MainChoiceLabelCorners[choiceIndex].x - (g_ChosenLabelGrowth / 2)),
        QuarterPixels(f_MainChoiceLabelCorners[choiceIndex].y - (g_ChosenLabelGrowth / 2)),
        QuarterPixels(
            f_MainChoiceLabelCorners[choiceIndex].x + MAIN_CHOICE_LABEL_WIDTH +
            (g_ChosenLabelGrowth / 2)
        ),
        QuarterPixels(
            f_MainChoiceLabelCorners[choiceIndex].y +
            (MAIN_CHOICE_LABEL_HEIGHT + (g_ChosenLabelGrowth / 2))
        ),
        G_TX_RENDERTILE,
        0,
        0,
        TexelRunStep(MAIN_CHOICE_LABEL_WIDTH, g_ChosenLabelGrowth + MAIN_CHOICE_LABEL_WIDTH),
        TexelRunStep(MAIN_CHOICE_LABEL_HEIGHT, g_ChosenLabelGrowth + MAIN_CHOICE_LABEL_HEIGHT)
    );
    gDPPipeSync(gfx++);
    if (g_ChosenLabelGrowthDirection == LABEL_GROWING) {
        g_ChosenLabelGrowth += CHOSEN_LABEL_GROWTH_STEP;
    }

    if (g_ChosenLabelGrowthDirection == LABEL_SHRINKING) {
        g_ChosenLabelGrowth -= CHOSEN_LABEL_GROWTH_STEP;
    }

    if (g_ChosenLabelGrowth > CHOSEN_LABEL_GROWTH_LIMIT) {
        g_ChosenLabelGrowthDirection = LABEL_SHRINKING;
    }

    if (g_ChosenLabelGrowth < 0) {
        g_ChosenLabelGrowthDirection = LABEL_GROWING;
    }

    *displayListHead = gfx;
}

void DrawMainMenuWithoutTransmit(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 choiceIndex;
    s32 labelOffset;

    DrawMenuBackdropLayers(&gfx, BOARD_BACKDROP_LAYERS);
    gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImagePalette(g_ImageSlots[f_ImageSlotIndex])));
    for (choiceIndex = 0; choiceIndex != MAIN_CHOICE_WITHOUT_TRANSMIT_COUNT; choiceIndex++) {
        if (g_MenuMemory.mainChoiceWithoutTransmit != choiceIndex) {
            labelOffset = MAIN_CHOICE_FULL_OPTIONS * MAIN_CHOICE_LABEL_IMAGE_SIZE;
            if (choiceIndex < MAIN_CHOICE_OPTIONS) {
                labelOffset = choiceIndex * MAIN_CHOICE_LABEL_IMAGE_SIZE;
            }

            gDPLoadTextureBlock_4b(
                gfx++,
                TextureAddress(ImageTexels(g_ImageSlots[f_ImageSlotIndex]) + labelOffset),
                G_IM_FMT_CI,
                MAIN_CHOICE_LABEL_WIDTH,
                MAIN_CHOICE_LABEL_HEIGHT,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gDPPipeSync(gfx++);
            gSPScisTextureRectangle(
                gfx++,
                QuarterPixels(f_ShortMainChoiceLabelCorners[choiceIndex].x),
                QuarterPixels(f_ShortMainChoiceLabelCorners[choiceIndex].y),
                QuarterPixels(
                    f_ShortMainChoiceLabelCorners[choiceIndex].x + MAIN_CHOICE_LABEL_WIDTH
                ),
                QuarterPixels(
                    f_ShortMainChoiceLabelCorners[choiceIndex].y + MAIN_CHOICE_LABEL_HEIGHT
                ),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
            gDPPipeSync(gfx++);
        }
    }

    gDPLoadTLUT_pal16(
        gfx++,
        0,
        TextureAddress(ImagePalette(g_MenuLabelImages[f_MenuLabelImageIndex]))
    );
    choiceIndex = g_MenuMemory.mainChoiceWithoutTransmit;
    if (choiceIndex == MAIN_CHOICE_OPTIONS) {
        labelOffset = MAIN_CHOICE_FULL_OPTIONS * MAIN_CHOICE_LABEL_IMAGE_SIZE;
    }
    else {
        labelOffset = choiceIndex * MAIN_CHOICE_LABEL_IMAGE_SIZE;
    }

    gDPLoadTextureBlock_4b(
        gfx++,
        TextureAddress(ImageTexels(g_MenuLabelImages[f_MenuLabelImageIndex]) + labelOffset),
        G_IM_FMT_CI,
        MAIN_CHOICE_LABEL_WIDTH,
        MAIN_CHOICE_LABEL_HEIGHT,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(
        gfx++,
        QuarterPixels(f_ShortMainChoiceLabelCorners[choiceIndex].x - (g_ChosenLabelGrowth / 2)),
        QuarterPixels(f_ShortMainChoiceLabelCorners[choiceIndex].y - (g_ChosenLabelGrowth / 2)),
        QuarterPixels(
            f_ShortMainChoiceLabelCorners[choiceIndex].x + MAIN_CHOICE_LABEL_WIDTH +
            (g_ChosenLabelGrowth / 2)
        ),
        QuarterPixels(
            f_ShortMainChoiceLabelCorners[choiceIndex].y +
            (MAIN_CHOICE_LABEL_HEIGHT + (g_ChosenLabelGrowth / 2))
        ),
        G_TX_RENDERTILE,
        0,
        0,
        TexelRunStep(MAIN_CHOICE_LABEL_WIDTH, g_ChosenLabelGrowth + MAIN_CHOICE_LABEL_WIDTH),
        TexelRunStep(MAIN_CHOICE_LABEL_HEIGHT, g_ChosenLabelGrowth + MAIN_CHOICE_LABEL_HEIGHT)
    );
    gDPPipeSync(gfx++);
    if (g_ChosenLabelGrowthDirection == LABEL_GROWING) {
        g_ChosenLabelGrowth += CHOSEN_LABEL_GROWTH_STEP;
    }

    if (g_ChosenLabelGrowthDirection == LABEL_SHRINKING) {
        g_ChosenLabelGrowth -= CHOSEN_LABEL_GROWTH_STEP;
    }

    if (g_ChosenLabelGrowth > CHOSEN_LABEL_GROWTH_LIMIT) {
        g_ChosenLabelGrowthDirection = LABEL_SHRINKING;
    }

    if (g_ChosenLabelGrowth < 0) {
        g_ChosenLabelGrowthDirection = LABEL_GROWING;
    }

    *displayListHead = gfx;
}

#define ROOM_LABEL_WIDTH 192
#define ROOM_LABEL_HEIGHT 20
#define ROOM_LABEL_IMAGE_SIZE (ROOM_LABEL_WIDTH * ROOM_LABEL_HEIGHT / 2)

void DrawClubhouse(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 choiceIndex;
    s32 labelOffset;

    DrawMenuBackdropLayers(&gfx, BOARD_BACKDROP_LAYERS);
    gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImagePalette(g_ImageSlots[f_ImageSlotIndex])));
    for (choiceIndex = 0; choiceIndex != CLUBHOUSE_CHOICE_COUNT; choiceIndex++) {
        if (choiceIndex != g_MenuMemory.clubhouseChoice) {
            labelOffset = f_ClubhouseRooms[choiceIndex] * ROOM_LABEL_IMAGE_SIZE;
            gDPLoadTextureBlock_4b(
                gfx++,
                TextureAddress(ImageTexels(g_ImageSlots[f_ImageSlotIndex]) + labelOffset),
                G_IM_FMT_CI,
                ROOM_LABEL_WIDTH,
                ROOM_LABEL_HEIGHT,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gDPPipeSync(gfx++);
            gSPScisTextureRectangle(
                gfx++,
                QuarterPixels(f_ClubhouseLabelCorners[choiceIndex].x),
                QuarterPixels(f_ClubhouseLabelCorners[choiceIndex].y),
                QuarterPixels(f_ClubhouseLabelCorners[choiceIndex].x + ROOM_LABEL_WIDTH),
                QuarterPixels(f_ClubhouseLabelCorners[choiceIndex].y + ROOM_LABEL_HEIGHT),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
            gDPPipeSync(gfx++);
        }
    }

    gDPLoadTLUT_pal16(
        gfx++,
        0,
        TextureAddress(ImagePalette(g_MenuLabelImages[f_MenuLabelImageIndex]))
    );
    choiceIndex = g_MenuMemory.clubhouseChoice;
    gDPLoadTextureBlock_4b(
        gfx++,
        TextureAddress(
            ImageTexels(g_MenuLabelImages[f_MenuLabelImageIndex]) +
            (f_ClubhouseRooms[choiceIndex] * ROOM_LABEL_IMAGE_SIZE)
        ),
        G_IM_FMT_CI,
        ROOM_LABEL_WIDTH,
        ROOM_LABEL_HEIGHT,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(
        gfx++,
        QuarterPixels(f_ClubhouseLabelCorners[choiceIndex].x - (g_ChosenLabelGrowth / 2)),
        QuarterPixels(f_ClubhouseLabelCorners[choiceIndex].y - (g_ChosenLabelGrowth / 2)),
        QuarterPixels(
            f_ClubhouseLabelCorners[choiceIndex].x + ROOM_LABEL_WIDTH + (g_ChosenLabelGrowth / 2)
        ),
        QuarterPixels(
            f_ClubhouseLabelCorners[choiceIndex].y + (ROOM_LABEL_HEIGHT + (g_ChosenLabelGrowth / 2))
        ),
        G_TX_RENDERTILE,
        0,
        0,
        TexelRunStep(ROOM_LABEL_WIDTH, g_ChosenLabelGrowth + ROOM_LABEL_WIDTH),
        TexelRunStep(ROOM_LABEL_HEIGHT, g_ChosenLabelGrowth + ROOM_LABEL_HEIGHT)
    );
    gDPPipeSync(gfx++);
    if (g_ChosenLabelGrowthDirection == LABEL_GROWING) {
        g_ChosenLabelGrowth += CHOSEN_LABEL_GROWTH_STEP;
    }

    if (g_ChosenLabelGrowthDirection == LABEL_SHRINKING) {
        g_ChosenLabelGrowth -= CHOSEN_LABEL_GROWTH_STEP;
    }

    if (g_ChosenLabelGrowth > CHOSEN_LABEL_GROWTH_LIMIT) {
        g_ChosenLabelGrowthDirection = LABEL_SHRINKING;
    }

    if (g_ChosenLabelGrowth < 0) {
        g_ChosenLabelGrowthDirection = LABEL_GROWING;
    }

    *displayListHead = gfx;
}

void DrawUnusedBoardScreen(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;

    DrawMenuBackdropLayers(&gfx, BOARD_BACKDROP_LAYERS);
    *displayListHead = gfx;
}

const char f_PeachsCastleName[] = "Peach's Castle";
const char f_LuigisGardenName[] = "Luigi's Garden";

#define OPTIONS_BACKDROP_LAYERS \
    (MENU_LAYER_COURSE_SCENE | MENU_LAYER_BANNER | MENU_LAYER_LARGE_BANNER | \
        MENU_LAYER_PER_SCREEN | MENU_LAYER_TALL_NARROW_BOARD)

#define MARIO_WIDTH 100
#define MARIO_HEIGHT 128
#define MARIO_STRIP_HEIGHT 16
#define MARIO_STRIP_COUNT 8
#define MARIO_LEFT 208
#define MARIO_TOP 88
#define MARIO_SHADOW_WIDTH 104
#define MARIO_SHADOW_HEIGHT 64
#define MARIO_SHADOW_LEFT 214
#define MARIO_SHADOW_TOP 168
#define MARIO_SHADOW_ALPHA 159

#define OPTION_ROW_SPACING 30

#define VALUE_BANNER_LEFT 136
#define VALUE_BANNER_WIDTH 120
#define VALUE_BANNER_ROW_TOP 62
#define VALUE_BANNER_OFFSET_Y 20
#define VALUE_BANNER_STRIP_HEIGHT 8
#define CHOSEN_BANNER_SHIFT 2
#define BANNER_SHADOW_DROP 3
#define BANNER_SHADOW_COLOR 31

#define ValueStripTop(toggleIndex, firstRowStripTop) \
    (((toggleIndex) * OPTION_ROW_SPACING) + (firstRowStripTop))
#define BANNER_SHADOW_ALPHA 159
#define DIMMED_COLOR 191

#define OPTION_LABEL_WIDTH 128
#define OPTION_LABEL_HEIGHT 20
#define OPTION_LABEL_IMAGE_SIZE (OPTION_LABEL_WIDTH * OPTION_LABEL_HEIGHT / 2)
#define OPTION_LABEL_SPACING (OPTION_LABEL_IMAGE_SIZE * 3)
#define OPTION_LABEL_LEFT 32
#define OPTION_LABEL_TOP 84
#define OPTION_VALUE_LEFT 142
#define OPTION_TOGGLE_COUNT OPTIONS_CHOICE_ERASE_SAVE_DATA

#define ARROW_FRAME_DURATION 5
#define ARROW_TIMER_PERIOD (GREEN_ARROW_FRAME_COUNT * ARROW_FRAME_DURATION)
#define LEFT_ARROW_LEFT 125
#define RIGHT_ARROW_LEFT 247
#define ARROW_TOP 86
#define ARROW_SHADOW_OFFSET_X 1
#define ARROW_SHADOW_OFFSET_Y 3
#define ARROW_PASS_COUNT 2

#define SONG_TEST_X 32
#define SOUND_TEST_X 168
#define TEST_NUMBER_Y 152
#define TEST_NAME_Y 162
#define SONG_NAME_X 94
#define SOUND_NAME_X 230
#define TEST_REVERB_Y 176
#define TEST_PITCH_Y 184
#define TEST_TEMPO_Y 192
#define CHOSEN_TEXT_BLUE 0

#define ERASE_WINDOW_WIDTH 240
#define ERASE_WINDOW_HEIGHT 160
#define ERASE_WINDOW_CENTER_X (SCREEN_WIDTH / 2)
#define ERASE_WINDOW_CENTER_Y (SCREEN_HEIGHT / 2)
#define MESSAGE_ERASE 0xC9
#define ERASE_MESSAGE_X (SCREEN_WIDTH / 2)
#define ERASE_MESSAGE_Y 106
#define ERASE_MESSAGE_SLOT 10
#define ERASE_MESSAGE_RED 95
#define ERASE_MESSAGE_GREEN 63
#define ERASE_MESSAGE_BLUE 255
#define ERASE_MESSAGE_PALETTE_RED 255
#define ERASE_MESSAGE_PALETTE_GREEN 127
#define ERASE_MESSAGE_PALETTE_BLUE 127

void DrawOptionsScreen(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    MenuMemory *memory = GetMenuMemory();
    ImageAsset *marioImage = GetMenuImageBuffer();
    u8 *arrows = AddressPast(GetMenuImageBuffer(), OPTIONS_ARROWS_OFFSET);
    u8 *shadow = GetMenuImageBuffer();
    s32 loopIndex;
    s32 toggleIndex;
    s32 firstRowStripTop;
    s32 valueOffset;
    s32 shiftX;
    s32 shiftY;
    s32 rowTop;
    s32 frameIndex;

    DrawMenuBackdropLayers(&gfx, OPTIONS_BACKDROP_LAYERS);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);

    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, MARIO_SHADOW_ALPHA);
    gDPSetCombineLERP(
        gfx++,
        0,
        0,
        0,
        PRIMITIVE,
        PRIMITIVE,
        0,
        TEXEL0,
        0,
        0,
        0,
        0,
        PRIMITIVE,
        PRIMITIVE,
        0,
        TEXEL0,
        0
    );
    gDPLoadTextureTile_4b(
        gfx++,
        TextureAddress(ImageBody(shadow + OPTIONS_SHADOW_OFFSET)),
        G_IM_FMT_I,
        MARIO_SHADOW_WIDTH,
        MARIO_SHADOW_HEIGHT,
        0,
        0,
        MARIO_SHADOW_WIDTH - 1,
        MARIO_SHADOW_HEIGHT - 1,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    DrawTextureRectangle(
        &gfx,
        QuarterPixels(MARIO_SHADOW_LEFT),
        QuarterPixels(MARIO_SHADOW_TOP),
        QuarterPixels(MARIO_SHADOW_LEFT + MARIO_SHADOW_WIDTH),
        QuarterPixels(MARIO_SHADOW_TOP + MARIO_SHADOW_HEIGHT),
        G_TX_RENDERTILE,
        0,
        0,
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );

    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    LoadLargePalette(&gfx, ImagePalette(marioImage));
    for (loopIndex = 0; loopIndex != MARIO_STRIP_COUNT; loopIndex++) {
        gDPLoadTextureTile(
            gfx++,
            TextureAddress(LargeImageTexels(marioImage)),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            MARIO_WIDTH,
            MARIO_HEIGHT,
            0,
            loopIndex * MARIO_STRIP_HEIGHT,
            MARIO_WIDTH - 1,
            (loopIndex * MARIO_STRIP_HEIGHT) + (MARIO_STRIP_HEIGHT - 1),
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(MARIO_LEFT),
            QuarterPixels(MARIO_TOP + (loopIndex * MARIO_STRIP_HEIGHT)),
            QuarterPixels(MARIO_LEFT + MARIO_WIDTH),
            QuarterPixels(MARIO_TOP + MARIO_STRIP_HEIGHT + (loopIndex * MARIO_STRIP_HEIGHT)),
            G_TX_RENDERTILE,
            0,
            TexelCoordinate(loopIndex * MARIO_STRIP_HEIGHT),
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
    }

    gDPLoadTLUT_pal256(gfx++, TextureAddress(ImagePalette(g_NamePlateImage)));
    gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, DIMMED_COLOR);
    for (toggleIndex = 0; toggleIndex != OPTION_TOGGLE_COUNT; toggleIndex++) {
        for (loopIndex = 0; loopIndex != NAME_PLATE_STRIP_COUNT; loopIndex++) {
            LoadTextureBlock(
                &gfx,
                LargeImageTexels(g_NamePlateImage) +
                    (loopIndex * NAME_PLATE_WIDTH * NAME_PLATE_STRIP_HEIGHT),
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                NAME_PLATE_WIDTH,
                NAME_PLATE_STRIP_HEIGHT,
                0,
                0,
                0,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            firstRowStripTop = (loopIndex * VALUE_BANNER_STRIP_HEIGHT) + VALUE_BANNER_ROW_TOP;
            if (g_MenuMemory.optionsChoice == toggleIndex) {
                gDPSetPrimColor(
                    gfx++,
                    0,
                    0,
                    BANNER_SHADOW_COLOR,
                    BANNER_SHADOW_COLOR,
                    BANNER_SHADOW_COLOR,
                    BANNER_SHADOW_ALPHA
                );
                gDPSetCombineLERP(
                    gfx++,
                    0,
                    0,
                    0,
                    PRIMITIVE,
                    PRIMITIVE,
                    0,
                    TEXEL0,
                    0,
                    0,
                    0,
                    0,
                    PRIMITIVE,
                    PRIMITIVE,
                    0,
                    TEXEL0,
                    0
                );
                DrawTextureRectangle(
                    &gfx,
                    QuarterPixels(VALUE_BANNER_LEFT),
                    QuarterPixels(
                        ValueStripTop(toggleIndex, firstRowStripTop) + VALUE_BANNER_OFFSET_Y +
                        BANNER_SHADOW_DROP
                    ),
                    QuarterPixels(VALUE_BANNER_LEFT + VALUE_BANNER_WIDTH),
                    QuarterPixels(
                        ValueStripTop(toggleIndex, firstRowStripTop) + VALUE_BANNER_OFFSET_Y +
                        BANNER_SHADOW_DROP + VALUE_BANNER_STRIP_HEIGHT
                    ),
                    G_TX_RENDERTILE,
                    0,
                    0,
                    TexelRunStep(NAME_PLATE_WIDTH, VALUE_BANNER_WIDTH),
                    TexelRunStep(NAME_PLATE_STRIP_HEIGHT, VALUE_BANNER_STRIP_HEIGHT)
                );
                gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
                DrawTextureRectangle(
                    &gfx,
                    QuarterPixels(VALUE_BANNER_LEFT - CHOSEN_BANNER_SHIFT),
                    QuarterPixels(
                        ValueStripTop(toggleIndex, firstRowStripTop) + VALUE_BANNER_OFFSET_Y
                    ),
                    QuarterPixels(VALUE_BANNER_LEFT + VALUE_BANNER_WIDTH - CHOSEN_BANNER_SHIFT),
                    QuarterPixels(
                        ValueStripTop(toggleIndex, firstRowStripTop) + VALUE_BANNER_OFFSET_Y +
                        VALUE_BANNER_STRIP_HEIGHT
                    ),
                    G_TX_RENDERTILE,
                    0,
                    0,
                    TexelRunStep(NAME_PLATE_WIDTH, VALUE_BANNER_WIDTH),
                    TexelRunStep(NAME_PLATE_STRIP_HEIGHT, VALUE_BANNER_STRIP_HEIGHT)
                );
            }
            else {
                gDPSetCombineLERP(
                    gfx++,
                    PRIMITIVE,
                    0,
                    TEXEL0,
                    0,
                    0,
                    0,
                    0,
                    TEXEL0,
                    PRIMITIVE,
                    0,
                    TEXEL0,
                    0,
                    0,
                    0,
                    0,
                    TEXEL0
                );
                gDPSetPrimColor(
                    gfx++,
                    0,
                    0,
                    DIMMED_COLOR,
                    DIMMED_COLOR,
                    DIMMED_COLOR,
                    OPAQUE_ALPHA
                );
                DrawTextureRectangle(
                    &gfx,
                    QuarterPixels(VALUE_BANNER_LEFT),
                    QuarterPixels(
                        ValueStripTop(toggleIndex, firstRowStripTop) + VALUE_BANNER_OFFSET_Y
                    ),
                    QuarterPixels(VALUE_BANNER_LEFT + VALUE_BANNER_WIDTH),
                    QuarterPixels(
                        ValueStripTop(toggleIndex, firstRowStripTop) + VALUE_BANNER_OFFSET_Y +
                        VALUE_BANNER_STRIP_HEIGHT
                    ),
                    G_TX_RENDERTILE,
                    0,
                    0,
                    TexelRunStep(NAME_PLATE_WIDTH, VALUE_BANNER_WIDTH),
                    TexelRunStep(NAME_PLATE_STRIP_HEIGHT, VALUE_BANNER_STRIP_HEIGHT)
                );
            }
        }
    }

    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImagePalette(g_ImageSlots[f_ImageSlotIndex])));
    for (loopIndex = 0; loopIndex != OPTIONS_CHOICE_COUNT; loopIndex++) {
        if (g_MenuMemory.optionsChoice != loopIndex) {
            LoadNibbleTextureBlock(
                &gfx,
                ImageTexels(g_ImageSlots[f_ImageSlotIndex]) + (loopIndex * OPTION_LABEL_SPACING),
                G_IM_FMT_CI,
                OPTION_LABEL_WIDTH,
                OPTION_LABEL_HEIGHT,
                0,
                0,
                0,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(OPTION_LABEL_LEFT),
                QuarterPixels(OPTION_LABEL_TOP + (loopIndex * OPTION_ROW_SPACING)),
                QuarterPixels(OPTION_LABEL_LEFT + OPTION_LABEL_WIDTH),
                QuarterPixels(
                    OPTION_LABEL_TOP + OPTION_LABEL_HEIGHT + (loopIndex * OPTION_ROW_SPACING)
                ),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
        }
    }

    LoadSmallPalette(&gfx, 0, ImagePalette(g_MenuLabelImages[f_MenuLabelImageIndex]));
    for (loopIndex = 0; loopIndex != OPTIONS_CHOICE_COUNT; loopIndex++) {
        if (g_MenuMemory.optionsChoice == loopIndex) {
            LoadNibbleTextureBlock(
                &gfx,
                ImageTexels(g_MenuLabelImages[f_MenuLabelImageIndex]) +
                    (loopIndex * OPTION_LABEL_SPACING),
                G_IM_FMT_CI,
                OPTION_LABEL_WIDTH,
                OPTION_LABEL_HEIGHT,
                0,
                0,
                0,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gDPPipeSync(gfx++);
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(OPTION_LABEL_LEFT),
                QuarterPixels(OPTION_LABEL_TOP + (loopIndex * OPTION_ROW_SPACING)),
                QuarterPixels(OPTION_LABEL_LEFT + OPTION_LABEL_WIDTH),
                QuarterPixels(
                    OPTION_LABEL_TOP + OPTION_LABEL_HEIGHT + (loopIndex * OPTION_ROW_SPACING)
                ),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
        }
    }

    gDPSetCombineLERP(
        gfx++,
        PRIMITIVE,
        0,
        TEXEL0,
        0,
        0,
        0,
        0,
        TEXEL0,
        PRIMITIVE,
        0,
        TEXEL0,
        0,
        0,
        0,
        0,
        TEXEL0
    );
    gDPLoadTLUT_pal16(
        gfx++,
        0,
        TextureAddress(
            ImagePalette(ImageAt(g_MenuLabelImages[f_MenuLabelImageIndex], OPTION_VALUES_OFFSET))
        )
    );
    for (loopIndex = 0; loopIndex != OPTION_TOGGLE_COUNT; loopIndex++) {
        valueOffset = loopIndex * 2 * OPTION_LABEL_IMAGE_SIZE;
        switch (loopIndex) {
            case OPTIONS_CHOICE_MUSIC:
                if (g_MenuMemory.optionBits & OPTION_BIT_MUSIC_OFF) {
                    valueOffset += OPTION_LABEL_IMAGE_SIZE;
                }

                break;
            case OPTIONS_CHOICE_MONO:
                if (g_MenuMemory.optionBits & OPTION_BIT_MONO) {
                    valueOffset += OPTION_LABEL_IMAGE_SIZE;
                }

                break;
            case OPTIONS_CHOICE_CAMERA:
                if (g_MenuMemory.optionBits & OPTION_BIT_SIMPLE_CAMERA) {
                    valueOffset += OPTION_LABEL_IMAGE_SIZE;
                }

                break;
        }

        LoadNibbleTextureBlock(
            &gfx,
            ImageTexels(ImageAt(g_MenuLabelImages[f_MenuLabelImageIndex], OPTION_VALUES_OFFSET)) +
                valueOffset,
            G_IM_FMT_CI,
            OPTION_LABEL_WIDTH,
            OPTION_LABEL_HEIGHT,
            0,
            0,
            0,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        if (g_MenuMemory.optionsChoice == loopIndex) {
            gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, OPAQUE_ALPHA);
        }
        else {
            gDPSetPrimColor(gfx++, 0, 0, DIMMED_COLOR, DIMMED_COLOR, DIMMED_COLOR, OPAQUE_ALPHA);
        }

        DrawTextureRectangle(
            &gfx,
            QuarterPixels(OPTION_VALUE_LEFT),
            QuarterPixels(OPTION_LABEL_TOP + (loopIndex * OPTION_ROW_SPACING)),
            QuarterPixels(OPTION_VALUE_LEFT + OPTION_LABEL_WIDTH),
            QuarterPixels(
                OPTION_LABEL_TOP + OPTION_LABEL_HEIGHT + (loopIndex * OPTION_ROW_SPACING)
            ),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
    }

    gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImagePalette(arrows)));
    if (g_MenuMemory.optionsChoice != OPTIONS_CHOICE_ERASE_SAVE_DATA) {
        f_OptionsArrowTimer = (f_OptionsArrowTimer + 1) % ARROW_TIMER_PERIOD;
        frameIndex = f_OptionsArrowTimer < ARROW_FRAME_DURATION;
        LoadNibbleTextureBlock(
            &gfx,
            ImageTexels(arrows) + (frameIndex * GREEN_ARROW_FRAME_SIZE),
            G_IM_FMT_CI,
            GREEN_ARROW_SIZE,
            GREEN_ARROW_FRAME_HEIGHT,
            0,
            0,
            0,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        for (loopIndex = 0; loopIndex != ARROW_PASS_COUNT; loopIndex++) {
            shiftX = 0;
            if (loopIndex == 0) {
                shiftX = ARROW_SHADOW_OFFSET_X;
                shiftY = ARROW_SHADOW_OFFSET_Y;
                gDPSetPrimColor(
                    gfx++,
                    0,
                    0,
                    BANNER_SHADOW_COLOR,
                    BANNER_SHADOW_COLOR,
                    BANNER_SHADOW_COLOR,
                    BANNER_SHADOW_ALPHA
                );
                gDPSetCombineLERP(
                    gfx++,
                    0,
                    0,
                    0,
                    PRIMITIVE,
                    PRIMITIVE,
                    0,
                    TEXEL0,
                    0,
                    0,
                    0,
                    0,
                    PRIMITIVE,
                    PRIMITIVE,
                    0,
                    TEXEL0,
                    0
                );
            }
            else {
                shiftY = 0;
                gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
            }

            rowTop = g_MenuMemory.optionsChoice * OPTION_ROW_SPACING;
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(LEFT_ARROW_LEFT + shiftX),
                QuarterPixels(rowTop + ARROW_TOP + shiftY),
                QuarterPixels(LEFT_ARROW_LEFT + GREEN_ARROW_SIZE + shiftX),
                QuarterPixels(rowTop + ARROW_TOP + GREEN_ARROW_SIZE + shiftY),
                G_TX_RENDERTILE,
                0,
                TexelCoordinate(GreenArrowRow(MENU_ARROW_LEFT)),
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
            rowTop = g_MenuMemory.optionsChoice * OPTION_ROW_SPACING;
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(RIGHT_ARROW_LEFT + shiftX),
                QuarterPixels(rowTop + ARROW_TOP + shiftY),
                QuarterPixels(RIGHT_ARROW_LEFT + GREEN_ARROW_SIZE + shiftX),
                QuarterPixels(rowTop + ARROW_TOP + GREEN_ARROW_SIZE + shiftY),
                G_TX_RENDERTILE,
                0,
                TexelCoordinate(GreenArrowRow(MENU_ARROW_RIGHT)),
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
        }
    }

    if (IsGameFlagSet(GAME_FLAG_OPTIONS_SOUND_TEST)) {
        SetUpShadowedTextDrawing(&gfx);
        SetTextColor(&gfx, 255, 255, 255);
        if (g_MenuMemory.optionsChoice == OPTIONS_CHOICE_SONG) {
            SetTextColor(&gfx, 255, 255, CHOSEN_TEXT_BLUE);
        }
        else {
            SetTextColor(&gfx, 255, 255, 255);
        }

        if (f_SoundTestSong == NO_SONG) {
            sprintf(g_PrintBuffer, "BGM TEST : STOP", f_SoundTestSong);
            DrawShadowedText(&gfx, SONG_TEST_X, TEST_NUMBER_Y, g_PrintBuffer);
        }
        else {
            sprintf(g_PrintBuffer, "BGM TEST : %2d", f_SoundTestSong);
            DrawShadowedText(&gfx, SONG_TEST_X, TEST_NUMBER_Y, g_PrintBuffer);
            sprintf(g_PrintBuffer, "%s", f_SoundTestSongNames[f_SoundTestSong]);
            DrawShadowedText(&gfx, SONG_NAME_X, TEST_NAME_Y, g_PrintBuffer);
        }

        if (g_MenuMemory.optionsChoice == OPTIONS_CHOICE_SONG_REVERB) {
            SetTextColor(&gfx, 255, 255, CHOSEN_TEXT_BLUE);
        }
        else {
            SetTextColor(&gfx, 255, 255, 255);
        }

        sprintf(g_PrintBuffer, " REVERB: %3d", f_SoundTestSongReverb);
        DrawShadowedText(&gfx, SONG_TEST_X, TEST_REVERB_Y, g_PrintBuffer);
        if (g_MenuMemory.optionsChoice == OPTIONS_CHOICE_SONG_FREQUENCY) {
            SetTextColor(&gfx, 255, 255, CHOSEN_TEXT_BLUE);
        }
        else {
            SetTextColor(&gfx, 255, 255, 255);
        }

        sprintf(g_PrintBuffer, " PICH  : %3.1f", f_SoundTestSongFrequency);
        DrawShadowedText(&gfx, SONG_TEST_X, TEST_PITCH_Y, g_PrintBuffer);
        if (g_MenuMemory.optionsChoice == OPTIONS_CHOICE_SONG_TEMPO) {
            SetTextColor(&gfx, 255, 255, CHOSEN_TEXT_BLUE);
        }
        else {
            SetTextColor(&gfx, 255, 255, 255);
        }

        sprintf(g_PrintBuffer, " TEMPO : %3d", f_SoundTestSongTempo);
        DrawShadowedText(&gfx, SONG_TEST_X, TEST_TEMPO_Y, g_PrintBuffer);
        if (g_MenuMemory.optionsChoice == OPTIONS_CHOICE_SOUND) {
            SetTextColor(&gfx, 255, 255, CHOSEN_TEXT_BLUE);
        }
        else {
            SetTextColor(&gfx, 255, 255, 255);
        }

        sprintf(g_PrintBuffer, "SE TEST : %3d", f_SoundTestSound);
        DrawShadowedText(&gfx, SOUND_TEST_X, TEST_NUMBER_Y, g_PrintBuffer);
        sprintf(g_PrintBuffer, "%s", f_SoundTestSoundNames[f_SoundTestSound]);
        DrawShadowedText(&gfx, SOUND_NAME_X, TEST_NAME_Y, g_PrintBuffer);
        if (g_MenuMemory.optionsChoice == OPTIONS_CHOICE_SOUND_REVERB) {
            SetTextColor(&gfx, 255, 255, CHOSEN_TEXT_BLUE);
        }
        else {
            SetTextColor(&gfx, 255, 255, 255);
        }

        sprintf(g_PrintBuffer, " REVERB: %3d", f_SoundTestSoundReverb);
        DrawShadowedText(&gfx, SOUND_TEST_X, TEST_REVERB_Y, g_PrintBuffer);
        if (g_MenuMemory.optionsChoice == OPTIONS_CHOICE_SOUND_FREQUENCY) {
            SetTextColor(&gfx, 255, 255, CHOSEN_TEXT_BLUE);
        }
        else {
            SetTextColor(&gfx, 255, 255, 255);
        }

        sprintf(g_PrintBuffer, " PICH  : %3.1f", f_SoundTestSoundFrequency);
        DrawShadowedText(&gfx, SOUND_TEST_X, TEST_PITCH_Y, g_PrintBuffer);
    }

    if (g_ChosenLabelGrowthDirection == LABEL_GROWING) {
        g_ChosenLabelGrowth += CHOSEN_LABEL_GROWTH_STEP;
    }

    if (g_ChosenLabelGrowthDirection == LABEL_SHRINKING) {
        g_ChosenLabelGrowth -= CHOSEN_LABEL_GROWTH_STEP;
    }

    if (g_ChosenLabelGrowth > CHOSEN_LABEL_GROWTH_LIMIT) {
        g_ChosenLabelGrowthDirection = LABEL_SHRINKING;
    }

    if (g_ChosenLabelGrowth < 0) {
        g_ChosenLabelGrowthDirection = LABEL_GROWING;
    }

    if (f_IsEraseWindowUp == TRUE) {
        DrawYesNoWindow(
            &gfx,
            ERASE_WINDOW_WIDTH,
            ERASE_WINDOW_HEIGHT,
            ERASE_WINDOW_CENTER_X,
            ERASE_WINDOW_CENTER_Y,
            NO_OK_TEXT,
            memory->eraseChoice
        );
        if (IsMenuWindowOpen()) {
            SetTextPaletteColor(
                ERASE_MESSAGE_PALETTE_RED,
                ERASE_MESSAGE_PALETTE_GREEN,
                ERASE_MESSAGE_PALETTE_BLUE,
                0
            );
            SetTextSlot(
                ERASE_MESSAGE_X,
                ERASE_MESSAGE_Y,
                MESSAGE_ERASE,
                TEXT_SLOT_LARGE_FONT,
                ERASE_MESSAGE_SLOT,
                TEXT_FLAG_CENTERED,
                ERASE_MESSAGE_RED,
                ERASE_MESSAGE_GREEN,
                ERASE_MESSAGE_BLUE,
                OPAQUE_ALPHA
            );
        }
        else {
            SetTextSlot(
                0,
                0,
                NO_MESSAGE,
                TEXT_SLOT_LARGE_FONT,
                ERASE_MESSAGE_SLOT,
                TEXT_FLAG_TYPED,
                255,
                255,
                255,
                OPAQUE_ALPHA
            );
        }
    }

    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void DrawCaddySelect(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 caddyIndex;

    DrawMenuBackdropLayers(&gfx, BOARD_BACKDROP_LAYERS);
    SetUpShadowedTextDrawing(&gfx);
    SetTextColor(&gfx, 255, 255, 255);
    DrawShadowedText(&gfx, CADDY_SELECT_TITLE_X, CADDY_SELECT_TITLE_Y, "CADDY SELECT");
    for (caddyIndex = 0; caddyIndex != CADDY_COUNT; caddyIndex++) {
        if (g_MenuMemory.caddyChoice == caddyIndex) {
            SetTextColor(&gfx, 255, 255, 0);
        }
        else {
            SetTextColor(&gfx, 255, 255, 255);
        }

        DrawShadowedText(
            &gfx,
            CADDY_SELECT_NAME_X,
            (caddyIndex * CADDY_SELECT_LINE_HEIGHT) + CADDY_SELECT_FIRST_Y,
            (char *)f_CaddyNames[caddyIndex]
        );
    }

    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void DrawBlankScreen(Gfx **displayListHead UNUSED) {
}

void UnusedMenuStub(void) {
}

void DrawSpinningBadge(Gfx **displayListHead, ImageAsset *image, s32 x, s32 y) {
    Gfx *gfx = *displayListHead;
    s32 unused[8] UNUSED;

    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImagePalette(image)));
    gDPPipeSync(gfx++);
    gDPLoadTextureTile_4b(
        gfx++,
        TextureAddress(ImageTexels(image) + (f_SpinningIconFrame * SPINNING_BADGE_FRAME_SIZE)),
        G_IM_FMT_CI,
        SPINNING_BADGE_SIZE,
        SPINNING_BADGE_SIZE,
        0,
        0,
        SPINNING_BADGE_SIZE - 1,
        SPINNING_BADGE_SIZE - 1,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(
        gfx++,
        QuarterPixels(x),
        QuarterPixels(y),
        QuarterPixels(x + SPINNING_BADGE_SIZE),
        QuarterPixels(y + SPINNING_BADGE_SIZE),
        G_TX_RENDERTILE,
        0,
        0,
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );
    gDPPipeSync(gfx++);
    f_SpinningIconFrameTime++;
    if (f_SpinningIconFrameTime == SPINNING_ICON_FRAME_DURATION) {
        f_SpinningIconFrameTime = 0;
        f_SpinningIconFrame++;
        if (f_SpinningIconFrame >= SPINNING_BADGE_FRAME_COUNT) {
            f_SpinningIconFrame = 0;
        }
    }

    *displayListHead = gfx;
}

void DrawSpinningStars(
    Gfx **displayListHead,
    s32 left,
    s32 top,
    u8 starCount,
    u8 enlargesChosenHole
) {
    Gfx *gfx = *displayListHead;
    EncodedAsset asset;
    ImageAsset *image = g_MenuArrowSizedImages[f_SpinningStarBufferIndex];
    u8 starIndex = 0;
    s32 growWidth = 0;
    s32 growHeight = 0;

    gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImagePalette(image)));
    gDPLoadTextureBlock_4b(
        gfx++,
        TextureAddress(ImageTexels(image)),
        G_IM_FMT_CI,
        STAR_WIDTH,
        STAR_HEIGHT,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPPipeSync(gfx++);
    if (starCount > STAR_COUNT_MAX) {
        starCount = STAR_COUNT_MAX;
    }

    if (starCount == 0) {
        starCount = 1;
    }

    for (; starIndex < starCount; starIndex++) {
        growWidth = 0;
        if (enlargesChosenHole == TRUE) {
            growHeight = 0;
            if (starIndex == g_MenuMemory.holeIndex) {
                growWidth = CHOSEN_STAR_GROWTH;
                growHeight = CHOSEN_STAR_GROWTH;
            }
        }
        else {
            growHeight = 0;
        }

        gSPScisTextureRectangle(
            gfx++,
            QuarterPixels(left + starIndex * STAR_WIDTH - (growWidth / 2)),
            QuarterPixels(top - (growHeight / 2)),
            QuarterPixels(left + starIndex * STAR_WIDTH + STAR_WIDTH + (growWidth / 2)),
            QuarterPixels(top + STAR_HEIGHT + (growHeight / 2)),
            G_TX_RENDERTILE,
            0,
            0,
            TexelRunStep(STAR_WIDTH, growWidth + STAR_WIDTH),
            TexelRunStep(STAR_HEIGHT, growHeight + STAR_HEIGHT)
        );
        gDPPipeSync(gfx++);
    }

    f_SpinningIconFrameTime++;
    if (f_SpinningIconFrameTime == SPINNING_ICON_FRAME_DURATION) {
        f_SpinningIconFrameTime = 0;
        f_SpinningIconFrame++;
        if (f_SpinningIconFrame >= STAR_FRAME_COUNT) {
            f_SpinningIconFrame = 0;
        }

        f_SpinningStarBufferIndex = f_SpinningStarBufferIndex == 0;
        OpenEncodedAsset(f_StarFrameAssets[f_SpinningIconFrame], &asset);
        ReadEncodedAsset(g_MenuArrowSizedImages[f_SpinningStarBufferIndex], &asset);
    }

    *displayListHead = gfx;
}

#define RING_SHOT_STAR_RADIUS 56
#define RING_SHOT_STAR_WIDTH 32
#define RING_SHOT_STAR_HEIGHT 24
#define RING_SHOT_STAR_GROWTH 16
#define RING_SHOT_STAR_FRAME_DURATION 2
#define GOLD_STAR_PALETTE 0
#define SILVER_STAR_PALETTE 1
#define SILVER_STAR_TMEM 128
#define SILVER_STAR_TILE 1

void DrawMenuRingShotStars(Gfx **displayListHead, s32 centerX, s32 centerY) {
    EncodedAsset asset;
    SaveData *save = GetSaveData();
    Gfx *gfx = *displayListHead;
    Vec2 starPositions[RING_SHOT_HOLE_COUNT] = {
        {0.0f, -1.0f},
        {0.89f, -0.45f},
        {0.89f, 0.45f},
        {0.0f, 1.0f},
        {-0.89f, 0.45f},
        {-0.89f, -0.45f},
    };
    ImageAsset *image;
    s32 tile;
    u8 holeIndex;
    s32 growWidth;
    s32 growHeight;
    s32 left;
    s32 top;

    image = g_AnimatedIconImages[g_AnimatedIconBufferIndex];
    gDPLoadTLUT_pal16(gfx++, GOLD_STAR_PALETTE, TextureAddress(ImagePalette(image)));
    gDPLoadTextureBlock_4b(
        gfx++,
        TextureAddress(ImageTexels(image)),
        G_IM_FMT_CI,
        RING_SHOT_STAR_WIDTH,
        RING_SHOT_STAR_HEIGHT,
        GOLD_STAR_PALETTE,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPPipeSync(gfx++);
    image = g_SecondAnimatedIconImages[g_AnimatedIconBufferIndex];
    gDPLoadTLUT_pal16(gfx++, SILVER_STAR_PALETTE, TextureAddress(ImagePalette(image)));
    gDPLoadMultiBlock_4b(
        gfx++,
        TextureAddress(ImageTexels(image)),
        SILVER_STAR_TMEM,
        SILVER_STAR_TILE,
        G_IM_FMT_CI,
        RING_SHOT_STAR_WIDTH,
        RING_SHOT_STAR_HEIGHT,
        SILVER_STAR_PALETTE,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPPipeSync(gfx++);
    for (holeIndex = 0; holeIndex < RING_SHOT_HOLE_COUNT; holeIndex++) {
        growWidth = 0;
        if (holeIndex == g_MenuMemory.holeIndex) {
            growWidth = RING_SHOT_STAR_GROWTH;
            growHeight = RING_SHOT_STAR_GROWTH;
        }
        else {
            growHeight = 0;
        }

        left = starPositions[holeIndex][AXIS_X] * RING_SHOT_STAR_RADIUS;
        top = starPositions[holeIndex][AXIS_Y] * RING_SHOT_STAR_RADIUS;
        left += centerX - (RING_SHOT_STAR_WIDTH / 2);
        top += centerY - (RING_SHOT_STAR_HEIGHT / 2);

        tile = save->records.hasRingShotStar[g_MenuMemory.courseIndex][holeIndex] == FALSE;
        gSPScisTextureRectangle(
            gfx++,
            QuarterPixels(left - (growWidth / 2)),
            QuarterPixels(top - (growHeight / 2)),
            QuarterPixels(left + RING_SHOT_STAR_WIDTH + (growWidth / 2)),
            QuarterPixels(top + RING_SHOT_STAR_HEIGHT + (growHeight / 2)),
            tile,
            0,
            0,
            TexelRunStep(RING_SHOT_STAR_WIDTH, growWidth + RING_SHOT_STAR_WIDTH),
            TexelRunStep(RING_SHOT_STAR_HEIGHT, growHeight + RING_SHOT_STAR_HEIGHT)
        );
        gDPPipeSync(gfx++);
    }

    g_AnimatedIconFrameTime++;
    if (g_AnimatedIconFrameTime == RING_SHOT_STAR_FRAME_DURATION) {
        g_AnimatedIconFrameTime = 0;
        g_AnimatedIconFrame++;
        if (g_AnimatedIconFrame >= STAR_FRAME_COUNT) {
            g_AnimatedIconFrame = 0;
        }

        g_AnimatedIconBufferIndex = g_AnimatedIconBufferIndex == 0;
        OpenEncodedAsset(f_StarFrameAssets[g_AnimatedIconFrame], &asset);
        ReadEncodedAsset(g_AnimatedIconImages[g_AnimatedIconBufferIndex], &asset);
        OpenEncodedAsset(f_MenuEmptyStarFrameAssets[g_AnimatedIconFrame], &asset);
        ReadEncodedAsset(g_SecondAnimatedIconImages[g_AnimatedIconBufferIndex], &asset);
    }

    *displayListHead = gfx;
}

void DrawWavingToad(Gfx **displayListHead, s32 left, s32 top) {
    Gfx *gfx = *displayListHead;
    s16 stripIndex;
    s32 rowCount;
    s32 unused[8] UNUSED;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPLoadTLUT_pal256(
        gfx++,
        TextureAddress(ImagePalette(g_MenuSpareImages[f_WavingToadImageIndex]))
    );
    for (stripIndex = 0; stripIndex != TOAD_STRIP_COUNT; stripIndex++) {
        rowCount = TOAD_LAST_STRIP_HEIGHT;
        if (stripIndex == 0) {
            rowCount = TOAD_STRIP_HEIGHT;
        }

        gDPLoadTextureBlock(
            gfx++,
            TextureAddress(
                LargeImageTexels(g_MenuSpareImages[f_WavingToadImageIndex]) +
                (stripIndex * TOAD_WIDTH * TOAD_STRIP_HEIGHT)
            ),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            TOAD_WIDTH,
            rowCount,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(
            gfx++,
            QuarterPixels(left),
            QuarterPixels(top + (stripIndex * TOAD_STRIP_HEIGHT)),
            QuarterPixels(left + TOAD_WIDTH),
            QuarterPixels(top + (stripIndex * TOAD_STRIP_HEIGHT) + rowCount),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        gDPPipeSync(gfx++);
    }

    *displayListHead = gfx;
}

void DrawBirdieBadgeLabel(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;

    DrawShadowedText(&gfx, 64, 32, "BIRDIE BADGE");
    *displayListHead = gfx;
}

s32 GetOpenCourseCount(void) {
    s32 coursePoints = GetSaveData()->coursePoints;

    if (IsGameFlagSet(GAME_FLAG_OPEN_SIX_COURSES)) {
        return 6;
    }

    if (IsGameFlagSet(GAME_FLAG_OPEN_FIVE_COURSES)) {
        return 5;
    }

    if (IsGameFlagSet(GAME_FLAG_OPEN_FOUR_COURSES)) {
        return 4;
    }

    if (IsGameFlagSet(GAME_FLAG_OPEN_THREE_COURSES)) {
        return 3;
    }

    if (IsGameFlagSet(GAME_FLAG_OPEN_TWO_COURSES)) {
        return 2;
    }

    return CountOpenCourses(coursePoints);
}
