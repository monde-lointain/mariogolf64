/*
 * The menus' update, which loads each screen and runs it, and their drawing,
 * which hands each screen to its draw routine. It holds the title scene of the
 * menus, the main menu, the clubhouse, the options, the course select and the
 * mini-golf course select, and the wipes that leave the menus.
 *
 * It also draws most of the menu screens: the main menu, the clubhouse, the
 * options, the help and birdie badge screens, and screens 5, 6, 8, 18, 20, 22,
 * 23 and 26, as well as the menus' labels, the chosen mode's icon, the
 * players' characters and the waving Toad, a debug character viewer and a
 * debug caddy select.
 */
#include "common.h"

#include "main_menu.h"

#include <PR/libmus.h>

#include "asset.h"
#include "billboard.h"
#include "camera.h"
#include "character.h"
#include "character_slot.h"
#include "club.h"
#include "controller.h"
#include "course.h"
#include "cpu_projection.h"
#include "game.h"
#include "game_flags.h"
#include "help_board.h"
#include "hole_display.h"
#include "hole_loader.h"
#include "libc.h"
#include "main.h"
#include "math_util.h"
#include "menu.h"
#include "menu_memory.h"
#include "overlay_manager.h"
#include "round_hud.h"
#include "round_stats.h"
#include "save.h"
#include "scene.h"
#include "scoring.h"
#include "screen.h"
#include "screen_layers.h"
#include "sound.h"
#include "sram.h"
#include "text.h"
#include "transfer_pak.h"
#include "variables.h"

/* The most modes the mode select offers for a player count */
#define MODE_CHOICE_LIMIT 12

/* The choices of both main menus; the one without Transmit stops at its options
 */
#define MAIN_CHOICE_CLUBHOUSE 0
#define MAIN_CHOICE_HELP 1
#define MAIN_CHOICE_OPTIONS 2
#define MAIN_CHOICE_WITHOUT_TRANSMIT_COUNT 3
#define MAIN_CHOICE_CLUB_TOURNAMENT 2
#define MAIN_CHOICE_FULL_OPTIONS 3
#define MAIN_CHOICE_COUNT 4

/* The clubhouse's rows, and the rooms that D_8022BF1C gives each of them */
#define CLUBHOUSE_CHOICE_COUNT 5
#define CLUBHOUSE_ROOM_COURSE_STATS 0
#define CLUBHOUSE_ROOM_UNK12 1
#define CLUBHOUSE_ROOM_UNK13 2
#define CLUBHOUSE_ROOM_BIRDIE_BADGES 3
#define CLUBHOUSE_ROOM_CHARACTER_TABLE 4
#define CLUBHOUSE_ROOM_BEST_TIMES 5

/* The options screen's choices; game flag 0x55 adds the sound test's */
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

/*
 * The menu image buffer holds the options screen's Mario at its start, then
 * two frames of four 16 by 16 arrows, then Mario's shadow
 */
#define OPTIONS_ARROWS_OFFSET 0x3408
#define OPTIONS_SHADOW_OFFSET 0x3830
/* Where the options' lit values follow their lit labels in D_8012D420 */
#define OPTION_VALUES_OFFSET 0x3228

/* Where the course select's images follow the sky in g_MenuCourseSceneryAsset
 */
#define COURSE_SELECT_WATER_OFFSET 0x1E88
#define COURSE_SELECT_LAYER_OFFSET 0x2E90

/* The sound test's song and sound file names, which it prints */
#define SOUND_TEST_SONG_NAME_LENGTH 12
#define SOUND_TEST_SOUND_NAME_LENGTH 24

/* The debug caddy select's two caddies */
#define CADDY_COUNT 2

/* The mini-golf course select's records, of three courses each */
#define MINI_GOLF_RECORD_COUNT 2

/* The chosen label grows two pixels a frame until it passes the limit, then
 * shrinks until it passes 0 */
#define CHOSEN_LABEL_GROWTH_LIMIT 6
#define CHOSEN_LABEL_GROWTH_STEP 2
#define LABEL_GROWING 0
#define LABEL_SHRINKING 1

/* The title screen's labels, 128 by 20 each, plain in D_801B5CD0 and lit in
 * D_8012D420 */
#define TITLE_LABEL_COUNT 7
#define TITLE_LABEL_WIDTH 128
#define TITLE_LABEL_HEIGHT 20
#define TITLE_LABEL_IMAGE_SIZE (TITLE_LABEL_WIDTH * TITLE_LABEL_HEIGHT / 2)
/* func_802081E8 draws a row of spinning stars, one per hole, from D_801B93E0,
 * and enlarges the chosen hole's */
#define STAR_WIDTH 32
#define STAR_HEIGHT 24
#define STAR_FRAME_COUNT 15
#define MAX_STAR_COUNT 6
#define CHOSEN_STAR_GROWTH 8
/* DrawMenuCharacters places the characters by a layout, chosen by their count,
 * of five slots */
#define MENU_CHARACTER_LAYOUT_COUNT 4
#define MENU_CHARACTER_SLOT_COUNT 5
/*
 * Their models' scales: a player alone stands largest, beside the rival or not,
 * and of three or four the two at the back stand slightly smaller
 */
#define MENU_CHARACTER_SOLO_SCALE 5.0f
#define MENU_RIVAL_SCALE 4.0f
#define MENU_CHARACTER_PAIR_SCALE 4.0f
#define MENU_CHARACTER_FRONT_SCALE 3.5f
#define MENU_CHARACTER_BACK_SCALE 3.4f
#define MENU_CHARACTER_DEFAULT_SCALE 4.0f
/* The course select's water plane's vertices */
#define WATER_VERTEX_COUNT 5
/* The holes badges of D_80106100, 24 by 24 CI4 each */
#define HOLES_BADGE_BET 0
#define HOLES_BADGE_COUNT 4
#define HOLES_BADGE_SIZE 24
#define HOLES_BADGE_IMAGE_SIZE ((HOLES_BADGE_SIZE * HOLES_BADGE_SIZE) / 2)
/* Places for the clubhouse's rows, three more than it shows */
#define CLUBHOUSE_ROW_PLACE_COUNT 8

/* The viewer's animation until the player picks another */
#define VIEWER_STARTING_ANIMATION 13
/* What D_8022FBA0 holds before the viewer shows a character, so that no
 * character matches it */
#define NO_VIEWER_CHARACTER -1
/* A club mask with a bit set for every slot */
#define ALL_CLUB_SLOTS ((1 << CLUB_SLOT_COUNT) - 1)
/* The size of the image buffer that GetMenuImageBuffer returns */
#define MENU_IMAGE_BUFFER_SIZE 0x80A0

/* Divides a turn among the player count's modes, keeping it whole should a
 * count be zero */
#define AmountPerMode(amount) \
  (D_8022BE30_ovl_01[g_ChosenPlayerCount - 1] != 0 \
          ? (amount) / D_8022BE30_ovl_01[g_ChosenPlayerCount - 1] \
          : (amount))

typedef struct ImageCorner {
  s16 x;
  s16 y;
} ImageCorner;

/* The wind and light of the hole that loads behind the menus */
#define CHOSEN_HOLE_WIND_SPEED_MASK 0x3F
#define CHOSEN_HOLE_WIND_SPEED_STEP 6000
#define CHOSEN_HOLE_LIGHT_PITCH 0x1000
#define CHOSEN_HOLE_LIGHT_YAW 0x2000

/* InitPlay's kind that loads no game state and skips PrepareHoleDisplay, as
 * the menus have set them up already */
#define PLAY_KIND_WITHOUT_HOLE_DISPLAY 3

/* The title screen's rows, which func_801F70F0 moves between */
#define TITLE_CHOICE_NEW_GAME 0
#define TITLE_CHOICE_SAVED_GAMES 1
#define TITLE_CHOICE_MENUS 2
#define TITLE_CHOICE_COUNT 3
/* Holding these as A is pressed on the menus row shows the Transmit choice in
 * the main menu */
#define TRANSMIT_BUTTONS (A_BUTTON | L_TRIG | R_TRIG)
#define TRANSMIT_OTHER_BUTTONS (A_BUTTON | Z_TRIG | R_TRIG)
#define TRANSMIT_SOUND_CHANNEL 14
/* The wipe that leaves for the Transmit scene from the saved games row */
#define TRANSMIT_WIPE_SHAPE 12
/* Where the title scene's text goes; pressing B clears it before going back
 * there */
#define TITLE_SCREEN_TEXT_Y 114

/* How the menus' bobbing items move: the phase step per frame, and the height
 * in pixels */
#define MENU_BOB_PHASE_STEP 0.13f
#define MENU_BOB_HEIGHT 5.0f

/* The bobbing of the menus' items: its phase, and the offset that it gives */
/* g_GameState.gameMode under a symbol of its own, which func_801F6B28_ovl_01
 * needs to match */

/* Where func_801FD7B0 and func_801FCEF8 draw the banners of D_801052BC and
 * D_801B60D0 */
#define MENU_BANNER_TOP 18
#define UNUSED_BANNER_TOP 16

/* func_801FD1B4 squeezes D_801B60D0 to this width at a given position; nothing
 * calls it */
#define SMALL_BANNER_WIDTH 96

/* func_801FD4F0 stretches D_801B60D0; its strips overlap, as they are taller
 * than their spacing */
#define LARGE_BANNER_LEFT 26
#define LARGE_BANNER_TOP 174
#define LARGE_BANNER_WIDTH 270
#define LARGE_BANNER_STRIP_HEIGHT 20
#define LARGE_BANNER_STRIP_SPACING 16

/*
 * func_801FCB74 reads D_800FF420's buffer as a 160 texel wide image, though
 * menu.c loads the 256 texel wide board into it; its strips overlap, as each
 * samples its rows at 8/9 of a texel
 */
#define NARROW_BOARD_WIDTH 160
#define NARROW_BOARD_STRIP_HEIGHT 8
#define NARROW_BOARD_STRIP_COUNT 20
#define NARROW_BOARD_STRIP_SPACING 7
#define NARROW_BOARD_ROW_SCALE 9
#define NARROW_BOARD_LEFT 15
#define NARROW_BOARD_TOP 36
#define NARROW_BOARD_SHOWN_WIDTH 170

/*
 * func_801FC754 draws the course backdrop of g_MenuCourseSceneryAsset at double
 * size. Each strip loads a row either side of the rows it shows, so that the
 * filtering blends across the seams.
 */
#define BACKDROP_WIDTH 152
#define BACKDROP_SCALE 2
#define BACKDROP_SEAM_ROW_COUNT 2
#define BACKDROP_SECOND_ROW_START TexelCoordinate(1)
#define BACKDROP_STRIP_ROW_COUNT 11
#define BACKDROP_STRIP_COUNT 11
#define BACKDROP_LEFT 8
#define BACKDROP_FIRST_TOP 8
#define BACKDROP_TOP 10

extern void* D_800FF420[2];
extern s32 D_801051F0;
extern void* D_801052BC[2];
extern const char D_80231514_ovl_01[];

#define PLAYER_UNK28_TRAINING (-1)
#define PLAYER_UNK28_NINE_HOLES 3
#define PLAYER_UNK28_EIGHTEEN_HOLES 6

/* The choices of the list that func_801FAFEC runs, which no menu screen
 * dispatches to */
#define UNUSED_LIST_CHOICE_COUNT 21

extern s8 D_801052E8;
extern s32 D_800FE334;
extern void* D_8010526C[2];
extern s8 D_800FBE08[MAX_PLAYER_COUNT];
extern s16 D_801B71CC;
extern s16 D_800FBE2C;
extern u8 D_801B9410;
extern s8 D_801B5634;

/*
 * The mini-golf course select. A record's three courses lie on a disc that
 * turns from one to the next through MINI_GOLF_TURN_FRAME_COUNT frames.
 * Changing record slides the screen across instead.
 */
#define MINI_GOLF_TURN_FRAME_DURATION 2
/* D_8022F8DC values */
#define MINI_GOLF_TURN_NONE 0
#define MINI_GOLF_TURN_TO_PREVIOUS 1
#define MINI_GOLF_TURN_TO_NEXT 2
#define MINI_GOLF_RECORD_SLIDE 45
#define MINI_GOLF_RECORD_SLIDE_STEP 9
/* D_8022BE88's value once the popup opened with C-up has finished */
#define MENU_PHASE_UNK2 2

extern void* D_801049A0[][MINI_GOLF_DISC_IMAGE_COUNT];

/*
 * The course select. Its wheel shows the open courses and the next one
 * still shut, and turns from one to the next; the chosen course's image
 * is read into the one of D_800FBDFC's buffers not shown, and its trophy
 * files into D_800FBE38.
 */
#define COURSE_SELECT_TURN 90
#define COURSE_SELECT_SPIN_SPEED 18
#define COURSE_SELECT_TEXT_SLOT_COUNT 4

extern void* D_800FBDFC[2];

/* What func_801F4C78 reads as the menu screen changes */
extern void* D_800FBE30[2];
extern void* D_800FC6A0;
extern void* D_800FC898;
extern void* D_800FF180[2];
extern void* D_800FF4C4[2];
extern void* D_801052C4[2];
extern void* D_80106100;
extern void* D_8012D3AC;
extern void* D_8012F4B0[7];
extern void* D_801B93E0[5];

void InitPlay(s32 kind);
void func_801FE4D0_ovl_01(Gfx** displayListHead);
void func_80201574_ovl_01(Gfx** displayListHead);
void func_80202000_ovl_01(Gfx** displayListHead);
void func_80205038_ovl_01(Gfx** displayListHead);
void func_802058B8_ovl_01(Gfx** displayListHead);
void func_80206180_ovl_01(Gfx** displayListHead);
void func_80206A68_ovl_01(Gfx** displayListHead);
void func_80207D08_ovl_01(Gfx** displayListHead);
void func_80207E18_ovl_01(Gfx** displayListHead);
void DrawBirdieBadgeLabel(Gfx** displayListHead);
/* Defined as returning s8, but their callers here use the whole register */
s32 IsMusicOffChosen(void);
s32 IsMonoChosen(void);
s32 IsSimpleCameraChosen(void);
void func_8005C674(s32 arg0, s32 arg1);
void func_8005D334(void);
void func_801F6B20_ovl_01(void);
void func_801F6B28_ovl_01(void);
void func_801F70F0_ovl_01(void);
void func_801F7CD0_ovl_01(void);
void func_801F7D28_ovl_01(void);
void func_801F8BAC_ovl_01(void);
void func_801F93D8_ovl_01(void);
void func_801F9C58_ovl_01(void);
void func_801F9E38_ovl_01(void);
void func_801F9FF8_ovl_01(void);
void func_801FA20C_ovl_01(void);
void func_801FADBC_ovl_01(void);
void LeaveMenuWithWipe(void);
void LeaveMenuWithCourseWipe(void);
void func_801FB11C_ovl_01(void);
void func_801FB15C_ovl_01(void);
void func_80209500_ovl_01(void);
void func_8020B2C4_ovl_01(void* buffer, s16 bufferKind);
void func_8020B3CC_ovl_01(void);
void func_8020B438_ovl_01(void);
void func_8020E010_ovl_01(void** buffers);
void func_8020E0AC_ovl_01(void** buffers, s32 requestedKind);
void func_8020E26C_ovl_01(void* buffer, s16 bufferKind);
void func_8020E390_ovl_01(void);
void func_80213390_ovl_01(void** buffers);
void func_8021342C_ovl_01(void* buffer);
void func_80213464_ovl_01(void* buffer, s16 bufferKind);
void func_80213558_ovl_01(void);
void func_8021585C_ovl_01(void);
void func_802159C0_ovl_01(void* buffer, s16 bufferKind);
void func_80215B84_ovl_01(void);
void func_8021730C_ovl_01(void);
void func_802177D0_ovl_01(void);
void func_802178B0_ovl_01(void* buffer, s16 bufferKind);
void func_80217D20_ovl_01(void);
/* Defined without parameters, though func_801F4C78 passes it two */
s32 func_80219948_ovl_01();
void func_8021CF10_ovl_01(void** buffers);
void func_8021CFAC_ovl_01(void* buffer, s16 bufferKind);
void func_8021D0B8_ovl_01(void);
void func_8021E01C_ovl_01(void);
void func_8021EF64_ovl_01(void);
void func_8021F5F0_ovl_01(void* buffer, s16 bufferKind);
void func_8021F6E0_ovl_01(void** buffers);
void func_8021F79C_ovl_01(void);
void func_80220A30_ovl_01(void);
void func_80220BB0_ovl_01(void);
void func_80221B20_ovl_01(void);
void func_802218C0_ovl_01(void* buffer, u8 bufferKind);
void func_80221EBC_ovl_01(void** buffers, u8 bufferKind);
void func_80222038_ovl_01(void* buffer, u8 bufferIndex);
void func_8022209C_ovl_01(void);
void func_802267B0_ovl_01(void* buffer, s16 bufferKind);
void func_80228A80_ovl_01(void* buffer, s32 bufferKind);
void func_80228C00_ovl_01(void);
s32 GetClubTournamentCourse(void);
s32 GetClubTournamentGameMode(void);
s32 func_8022BDC4_ovl_01(void);
s32 func_8022BDD0_ovl_01(void);
s32 func_8021E468_ovl_01(void);
void LoadRingShotRecord(RingShotRecord* record);
void LoadBestShotReplay(PlayRecord* record);
s32 SaveOrContinueGameByMode(Boolean isContinuing);
s32 func_801F7660_ovl_01(s32 arg0, s32 playerNumber);
void func_80217B88_ovl_01(s32 requestedPreset);
void func_80228B60_ovl_01(void);
void func_801FC754_ovl_01(Gfx** displayListHead);
void func_801FCB74_ovl_01(Gfx** displayListHead);
void func_801FD4F0_ovl_01(Gfx** displayListHead);
void func_801FD7B0_ovl_01(Gfx** displayListHead);
void func_801FDA40_ovl_01(Gfx** displayListHead);
void func_8020B590_ovl_01(Gfx** displayListHead, u8 isSinglePlayer);
void func_8020E898_ovl_01(Gfx** displayListHead);
void func_80213748_ovl_01(Gfx** displayListHead);
void func_80215DC4_ovl_01(Gfx** displayListHead);
void func_8021D3AC_ovl_01(Gfx** displayListHead);
void func_8021FC5C_ovl_01(Gfx** displayListHead);
void func_802230C0_ovl_01(Gfx** displayListHead);
void func_80229CB0_ovl_01(Gfx** displayListHead);

/* Not static: the original left these to the linker's common block */
s32 D_8023BFC0_ovl_01[MAX_PLAYER_COUNT];
s32 D_8023BFD0_ovl_01;

/* Defined after the routines whose strings come before theirs */
extern const char D_802316EC_ovl_01[];
extern const char D_802316FC_ovl_01[];

/* Read by no routine */
static Gfx D_8022BE00_ovl_01[] = {gsSPEndDisplayList()};
static s32 D_8022BE08_ovl_01 = 0;
static s32 D_8022BE0C_ovl_01 = CHARACTER_COUNT;
/* Read by no routine */
static u8 D_8022BE10_ovl_01[8] = {
    0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static s32 D_8022BE18_ovl_01 = 0;
static s32 D_8022BE1C_ovl_01 = 0;
/* Read by no routine */
static s32 D_8022BE20_ovl_01 = 0;
static s32 D_8022BE24_ovl_01 = VIEWER_STARTING_ANIMATION;
static s32 D_8022BE28_ovl_01 = 0;
static s32 D_8022BE2C_ovl_01 = 0;
/* Each player count's mode count */
static s32 D_8022BE30_ovl_01[MAX_PLAYER_COUNT] = {7, 5, 4, 4};
/* Read by no routine; the mode counts of D_8022BEEC */
static s32 D_8022BE40_ovl_01[4] = {3, 5, 4, 4};
/* Each player's clubs, a bit per slot */
static u16 D_8022BE50_ovl_01[MAX_PLAYER_COUNT] = {
    ALL_CLUB_SLOTS, ALL_CLUB_SLOTS, ALL_CLUB_SLOTS, ALL_CLUB_SLOTS};
/* Read by no routine */
static s32 D_8022BE58_ovl_01[10] = {0, 0, 0, 0, 0, 0, 0, 1, 1, 1};
/* Which of D_800FBDFC's buffers shows the course */
static u8 D_8022BE80_ovl_01 = 0;
static s8 D_8022BE81_ovl_01 = 0;
static s32 D_8022BE84_ovl_01 = 0;
u8 D_8022BE88_ovl_01 = 0;
u8 D_8022BE89_ovl_01 = 0;
static s16 D_8022BE8A_ovl_01 = 0;
static s16 D_8022BE8C_ovl_01 = 0;
static s16 D_8022BE8E_ovl_01 = 0;
static s16 D_8022BE90_ovl_01 = 0;
static s16 D_8022BE92_ovl_01 = 0;
static u8 D_8022BE94_ovl_01 = 0;
/* Whether the options screen's erase window is up */
static u8 f_IsEraseWindowUp = FALSE;
static f32 f_MenuBobOffset = 0.0f;
static f32 f_MenuBobPhase = 0.0f;
/* The sound test's sound, song and settings */
static s32 f_SoundTestSound = 0;
static s32 f_SoundTestSong = 0;
static s32 f_SoundTestSoundReverb = 0;
static s32 f_SoundTestSongReverb = 0;
static f32 f_SoundTestSoundFrequency = 0.0f;
static f32 f_SoundTestSongFrequency = 0.0f;
static s32 f_SoundTestSongTempo = NORMAL_SONG_TEMPO;
static u8 D_8022BEBC_ovl_01[MAX_PLAYER_COUNT][MODE_CHOICE_LIMIT] = {
    {MODE_ICON_TOURNAMENT, MODE_ICON_GET_CHARACTER, MODE_ICON_RING_SHOT,
        MODE_ICON_SPEED_GOLF, MODE_ICON_STROKE, MODE_ICON_MINI_GOLF,
        MODE_ICON_TRAINING},
    {MODE_ICON_STROKE, MODE_ICON_MATCH_GAME, MODE_ICON_SKINS_MATCH,
        MODE_ICON_CLUB_SLOTS, MODE_ICON_MINI_GOLF},
    {MODE_ICON_STROKE, MODE_ICON_SKINS_MATCH, MODE_ICON_CLUB_SLOTS,
        MODE_ICON_MINI_GOLF},
    {MODE_ICON_STROKE, MODE_ICON_SKINS_MATCH, MODE_ICON_CLUB_SLOTS,
        MODE_ICON_MINI_GOLF}};
/* Read by no routine; modes like D_8022BEBC's, as many as D_8022BE40 counts */
static u8 D_8022BEEC_ovl_01[4][12] = {
    {2, 8, 3}, {2, 6, 7, 10, 5}, {2, 7, 10, 5}, {2, 7, 10, 5}};
static s16 D_8022BF1C_ovl_01[CLUBHOUSE_CHOICE_COUNT] = {
    CLUBHOUSE_ROOM_COURSE_STATS, CLUBHOUSE_ROOM_CHARACTER_TABLE,
    CLUBHOUSE_ROOM_BEST_TIMES, CLUBHOUSE_ROOM_UNK13, CLUBHOUSE_ROOM_UNK12};
/* Read by no routine; the one room that D_8022BF1C leaves out */
static s16 D_8022BF26_ovl_01 = CLUBHOUSE_ROOM_BIRDIE_BADGES;
/* The plain-text menu's names; func_801FDA40 prints the first three */
static const char* D_8022BF28_ovl_01[] = {
    "1P GAME", "2P GAME", "3P GAME", "4P GAME", "CONTINUE", "HOUISE"};
/* Read by no routine */
static const char* D_8022BF40_ovl_01[] = {"TOURNAMENT ", "VS CHARACTER",
    "STROKE PLAY", "MATCH PLAY", "SKINS", "TIME ATTACK", "TRAINING",
    "PUTTER GOLF", " HANDI CAP", " BET", "9HOLE MODE  ", "CLUB HANDI"};
/* Each character's name, with LOWERCASE_CODE_OFFSET added to its lowercase
 * letters */
char D_8022BF70_ovl_01[CHARACTER_COUNT][IMPORTED_CHARACTER_NAME_LENGTH] = {
    "P\xEC\xF5\xED", /* Plum */
    "C\xE8\xE1\xF2\xEC\xE9\xE5", /* Charlie */
    "P\xE5\xE1\xE3\xE8", /* Peach */
    "B\xE1\xE2\xF9 \xED\xE1\xF2\xE9\xEF", /* Baby mario */
    "L\xF5\xE9\xE7\xE9", /* Luigi */
    "Y\xEF\xF3\xE8\xE9", /* Yoshi */
    "S\xEF\xEE\xEE\xF9", /* Sonny */
    "W\xE1\xF2\xE9\xEF", /* Wario */
    "H\xE1\xF2\xF2\xF9", /* Harry */
    "M\xE1\xF2\xE9\xEF", /* Mario */
    "M\xE1\xF0\xEC\xE5", /* Maple */
    "DK", /* DK */
    "B\xEF\xF7\xF3\xE5\xF2", /* Bowser */
    "M\xE5\xF4\xE1\xEC \xED\xE1\xF2\xE9\xEF", /* Metal mario */
    "K\xE9\xE4", /* Kid */
    "S\xE8\xE5\xF2\xF2\xF9", /* Sherry */
    "A\xFA\xE1\xEC\xE5\xE1", /* Azalea */
    "J\xEF\xE5" /* Joe */
};
/* Read by no routine */
static const char* D_8022C048_ovl_01[] = {"TOURNAMENT", "MEMORIAL HOLES",
    "SPECIAL SHOT", "BIRDIE BADGE", "VS CHARACTER", "TIME ATTACK", "BGM",
    "SOUND", "DATA RESET", "BGM TEST", "SOUND TEST"};
/* Read by no routine */
static const char* D_8022C074_ovl_01[] = {
    "GB 00 ", "GB 01 ", "GB 02 ", "GB 03 "};
/* Read by no routine */
static const char* D_8022C084_ovl_01[] = {"STRAIGHT", "DRAW", "FADE"};
/* Read by no routine */
static const char* D_8022C090_ovl_01[] = {
    "DRIVE CON", "NEAR PIN", "STROKE", "SKINS", "NASSO", "LASVEGAS"};
/* Read by no routine */
static const char* D_8022C0A8_ovl_01[] = {"TEST COURSE1", "TEST COURSE2",
    "TEST COURSE3", "TEST COURSE4", "TEST COURSE5", "TEST COURSE6",
    "TEST COURSE7", "TEST COURSE8", "TEST COURSE9", "TEST COURSE10"};
/* The caddies' names */
static const char* D_8022C0D0_ovl_01[CADDY_COUNT] = {"MARIO", "PEACH"};
/* Read by no routine */
static const char* D_8022C0D8_ovl_01[] = {"1W", "3W", "4W", "2I", "3I", "4I",
    "5I", "6I", "7I", "8I", "9I", "PW", "SW", "PT"};
static char f_SoundTestSongNames[][SOUND_TEST_SONG_NAME_LENGTH] = {"jing1",
    "jing02", "jing03", "jing04", "jing05", "jing06", "shori", "make", "hikiwa",
    "mode-ok", "score", "coace3", "trad1", "trad2", "par", "bar", "dan1",
    "bgm04", "bgm05", "bgm06", "coace5", "training", "gbdata", "coace9",
    "coace8", "coace7", "coace6", "coace10", "coace4", "pop1", "pop2", "replay",
    "trad3", "trad4", "pop3", "pop4", "pop5", "pop6", "time", "64theme",
    "nangoku", "mariobgm", "mario20", "clav", "patar1", "patar2", "skins",
    "openning", "bgm20", "ending", "end-jingle"};
static char f_SoundTestSoundNames[][SOUND_TEST_SOUND_NAME_LENGTH] = {"AE01 ",
    "AE05 ", "BL01 ", "BL02 ", "BL03 ", "BL04 ", "BL05 ", "BL06 ", "BL07 ",
    "BL08 ", "BL09 ", "BL10 ", "BL11 ", "BL14 ", "BL15 ", "BL16 ", "BL17 ",
    "BL18 ", "BL19 ", "BL20 ", "BL22 ", "BL23 ", "BR01MOZU ", "BR02KAKKO ",
    "BR03KANARIA ", "BR04MEJIRO ", "BR05SUZUME ", "BR06SUZUME02 ",
    "BR07HIBARI ", "BR07HIBARI1 ", "BR09KAWASEMI ", "DR01 ", "DR03 ", "DR04 ",
    "DR05 ", "DR06 ", "DR07 ", "DR10 ", "DR11 ", "DR12 ", "DR13 ", "DR14 ",
    "DR15 ", "DR40 ", "DR41 ", "DR42 ", "DR50 ", "DR53 ", "DR54 ", "DR55 ",
    "DR56 ", "DR57 ", "DR58 ", "DR59 ", "DR60 ", "DR61 ", "GP01 ", "GP02 ",
    "GP03 ", "GP04 ", "GP05 ", "GP06 ", "GP07 ", "GP08 ", "GP09 ", "GP10 ",
    "GP11 ", "SS01 ", "SS02 ", "SS03 ", "SS04 ", "SS05 ", "SS06 ", "SS07 ",
    "SS08 ", "SS09 ", "SS10 ", "SS12 ", "SS13 ", "SS14 ", "SW01 ", "SW02 ",
    "SW03 ", "SW04 ", "SW05 ", "SW06 ", "SW07 ", "SW08 ", "SW10 ", "SW20 ",
    "SW21 ", "SY01 ", "SY02 ", "SY03 ", "SY04 ", "SY05 ", "SY06 ", "SY07 ",
    "SY08 ", "SY09 ", "SY10 ", "SY11 ", "SY12 ", "SY13 ", "SY14 ", "SY15 ",
    "SY17 ", "SY18 ", "SY19 ", "SY20 ", "SY21 ", "SY22 ", "USA20", "USA21",
    "VO01 ", "VO02 ", "VO03 ", "VO04 ", "VO05 ", "VO10 ", "VO11 ", "VO12 ",
    "VO13 ", "NR01 ", "NR02 ", "NR03 ", "NR04 ", "NR05 ", "NR06 ", "NR07 ",
    "NR08 ", "NR09 ", "NR10 ", "NR11 ", "NR12 ", "NR13 ", "NR14 ", "NR15 ",
    "NR16 ", "NR17 ", "NR18 ", "NR20 ", "NR21 ", "NR26 ", "NR27 ", "NR30 ",
    "NR31 ", "NR32 ", "NR37 ", "NR38 ", "NR39 ", "NR40 ", "NR41 ", "NR42 ",
    "NR43 ", "NR44 ", "NR45 ", "NR46 ", "CAZ00 ", "CAZ01 ", "CAZ02 ", "CAZ03 ",
    "CAZ04 ", "CAZ05 ", "CAZ06 ", "CAZ07 ", "CAZ08 ", "CAZ09 ", "CAZ10 ",
    "CAZ12 ", "CAZ13 ", "CAZ14 ", "CAZ15 ", "CAZ16 ", "CAZ17 ", "CAZ18 ",
    "CAZ20 ", "CAZ21 ", "CAZ22 ", "CBB00 ", "CBB01 ", "CBB02 ", "CBB03 ",
    "CBB04 ", "CBB05 ", "CBB06 ", "CBB07 ", "CBB08 ", "CBB09 ", "CBB10 ",
    "CBB12 ", "CBB13 ", "CBB14 ", "CBB15 ", "CBB16 ", "CBB17 ", "CBB18 ",
    "CBB20 ", "CBB22 ", "CBB23 ", "CBB27 ", "CBB30 ", "CBB31 ", "CBB32 ",
    "CCH00 ", "CCH01 ", "CCH02 ", "CCH03 ", "CCH04 ", "CCH05 ", "CCH06 ",
    "CCH07 ", "CCH08 ", "CCH09 ", "CCH10 ", "CCH12 ", "CCH13 ", "CCH14 ",
    "CCH15 ", "CCH16 ", "CCH17 ", "CCH18 ", "CCH19 ", "CCH20 ", "CCH21 ",
    "CCH24 ", "CDK00 ", "CDK01 ", "CDK02 ", "CDK03 ", "CDK04 ", "CDK05 ",
    "CDK06 ", "CDK08 ", "CDK10 ", "CDK13 ", "CDK18 ", "CDK20 ", "CDK21 ",
    "CHR00 ", "CHR01 ", "CHR02 ", "CHR03 ", "CHR04 ", "CHR05 ", "CHR06 ",
    "CHR07 ", "CHR08 ", "CHR09 ", "CHR10 ", "CHR12 ", "CHR13 ", "CHR14 ",
    "CHR15 ", "CHR16 ", "CHR17 ", "CHR18 ", "CHR19 ", "CHR20 ", "CHR21 ",
    "CHR22 ", "CHR25 ", "CJO00 ", "CJO01 ", "CJO02 ", "CJO03 ", "CJO04 ",
    "CJO05 ", "CJO06 ", "CJO07 ", "CJO08 ", "CJO09 ", "CJO10 ", "CJO12 ",
    "CJO13 ", "CJO14 ", "CJO15 ", "CJO16 ", "CJO17 ", "CJO18 ", "CJO20 ",
    "CJO21 ", "CJO22 ", "CJO24 ", "CKD00 ", "CKD02 ", "CKD03 ", "CKD04 ",
    "CKD06 ", "CKD07 ", "CKD08 ", "CKD09 ", "CKD10 ", "CKD12 ", "CKD13 ",
    "CKD14 ", "CKD15 ", "CKD16 ", "CKD17 ", "CKD18 ", "CKD20 ", "CKD21 ",
    "CKD25 ", "CKP00 ", "CKP01 ", "CKP02 ", "CKP03 ", "CKP04 ", "CKP05 ",
    "CKP06 ", "CKP08 ", "CKP09 ", "CKP10 ", "CKP13 ", "CKP18 ", "CKP20 ",
    "CLG00 ", "CLG01 ", "CLG02 ", "CLG03 ", "CLG05 ", "CLG06 ", "CLG07 ",
    "CLG08 ", "CLG09 ", "CLG10 ", "CLG12 ", "CLG13 ", "CLG14 ", "CLG15 ",
    "CLG17 ", "CLG18 ", "CLG19 ", "CLG20 ", "CLG21 ", "CLG23 ", "CLG24 ",
    "CLG30 ", "CLG31 ", "CME00 ", "CME01 ", "CME02 ", "CME03 ", "CME04 ",
    "CME05 ", "CME06 ", "CME07 ", "CME08 ", "CME09 ", "CME10 ", "CME12 ",
    "CME13 ", "CME14 ", "CME15 ", "CME16 ", "CME17 ", "CME18 ", "CME19 ",
    "CME20 ", "CME21 ", "CME22 ", "CME25 ", "CMR00 ", "CMR01 ", "CMR02 ",
    "CMR03 ", "CMR04 ", "CMR05 ", "CMR06 ", "CMR07 ", "CMR08", "CMR09", "CMR10",
    "CMR11", "CMR12", "CMR13", "CMR14", "CMR15", "CMR16", "CMR17", "CMR18",
    "CMR19", "CMR20", "CMR21", "CMR22", "CMR23", "CMR26", "CMR30", "CMR31",
    "CMR32 ", "CMR33 ", "CMR34 ", "CMR35 ", "CMR36 ", "CMR37 ", "CPC00 ",
    "CPC01 ", "CPC02 ", "CPC03 ", "CPC04 ", "CPC05 ", "CPC06 ", "CPC07 ",
    "CPC08 ", "CPC09 ", "CPC10 ", "CPC12 ", "CPC13 ", "CPC14 ", "CPC15 ",
    "CPC16 ", "CPC17 ", "CPC18 ", "CPC19 ", "CPC20 ", "CPC21 ", "CPC24 ",
    "CPC30 ", "CPC31 ", "CPR00 ", "CPR01 ", "CPR02 ", "CPR03 ", "CPR04 ",
    "CPR05 ", "CPR06 ", "CPR07 ", "CPR08 ", "CPR09 ", "CPR10 ", "CPR12 ",
    "CPR13 ", "CPR14 ", "CPR15 ", "CPR16 ", "CPR17 ", "CPR18 ", "CPR19 ",
    "CPR20 ", "CPR21 ", "CPR24 ", "CPR25 ", "CPR30 ", "CSN00 ", "CSN01 ",
    "CSN02 ", "CSN03 ", "CSN04 ", "CSN05 ", "CSN06 ", "CSN07 ", "CSN08 ",
    "CSN09 ", "CSN10 ", "CSN12 ", "CSN13 ", "CSN14 ", "CSN15 ", "CSN16 ",
    "CSN17 ", "CSN18 ", "CSN19 ", "CSN20 ", "CSN21 ", "CSN24 ", "CSN25 ",
    "CSN26 ", "CSN27 ", "CSY00 ", "CSY01 ", "CSY02 ", "CSY03 ", "CSY04 ",
    "CSY05 ", "CSY06 ", "CSY07 ", "CSY08 ", "CSY09 ", "CSY10 ", "CSY12 ",
    "CSY13 ", "CSY14 ", "CSY15 ", "CSY16 ", "CSY17 ", "CSY18 ", "CSY19 ",
    "CSY20 ", "CSY21 ", "CSY26 ", "CWR00 ", "CWR01 ", "CWR02 ", "CWR03 ",
    "CWR05 ", "CWR06 ", "CWR07 ", "CWR08 ", "CWR09 ", "CWR10 ", "CWR11 ",
    "CWR12 ", "CWR13 ", "CWR14 ", "CWR15 ", "CWR16 ", "CWR17 ", "CWR18 ",
    "CWR19 ", "CWR20 ", "CWR23 ", "CWR24 ", "CYS00 ", "CYS01 ", "CYS02 ",
    "CYS03 ", "CYS04 ", "CYS05 ", "CYS06 ", "CYS07 ", "CYS08 ", "CYS13 ",
    "CYS14 ", "CYS17 ", "CYS18 ", "CYS19 ", "CYS20 ", "CYS21 ", "TITLE ",
    "USATITLE", "USATITLE01", "USATITLE02", "WARAI3", "NONE_SE", "NONE_SE",
    "NONE_SE", "NONE_SE", "NONE_SE", "NONE_SE", "NONE_SE", "NONE_SE", "NONE_SE",
    "NONE_SE", "NONE_SE", "NONE_SE", "NONE_SE", "NONE_SE", "NONE_SE", "NONE_SE",
    "NONE_SE", "NONE_SE", "NONE_SE", "NONE_SE", "NONE_SE", "NONE_SE", "NONE_SE",
    "NONE_SE", "NONE_SE"};
/* Read by no routine */
static Vtx D_8022F7C8_ovl_01[8] = {
    {{{-32, 32, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{-32, 0, 0}, 0, {0, 2048}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{32, 0, 0}, 0, {4096, 2048}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{32, 32, 0}, 0, {4096, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{-32, 0, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{-32, -32, 0}, 0, {0, 2048}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{32, -32, 0}, 0, {4096, 2048}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{32, 0, 0}, 0, {4096, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}}};
/* The star's frames */
static s32 D_8022F848_ovl_01[STAR_FRAME_COUNT] = {0x05BE, 0x05BF, 0x05C0,
    0x05C1, 0x05C2, 0x05C3, 0x05C4, 0x05C5, 0x05C6, 0x05C7, 0x05C8, 0x05C9,
    0x05CA, 0x05CB, 0x05CC};
/* The silver star's frames */
static s32 D_8022F884_ovl_01[STAR_FRAME_COUNT] = {0x05E5, 0x05E6, 0x05E7,
    0x05E8, 0x05E9, 0x05EA, 0x05EB, 0x05EC, 0x05ED, 0x05EE, 0x05EF, 0x05F0,
    0x05F1, 0x05F2, 0x05F3};
s32 D_8022F8C0_ovl_01 = 0;
u8 D_8022F8C4_ovl_01 = 0;
s8 D_8022F8C5_ovl_01 = 0;
static s32 D_8022F8C8_ovl_01 = 0;
/* Which of D_801B93E0's buffers holds the star frame shown */
static u8 D_8022F8CC_ovl_01 = 0;
static s8 D_8022F8CD_ovl_01 = 0;
static u8 D_8022F8CE_ovl_01 = 0;
/* Read by no routine */
static s32 D_8022F8D0_ovl_01 = 0;
/* The mini-golf disc's frame */
static s32 D_8022F8D4_ovl_01 = MINI_GOLF_FACING_FRAME;
/* The calls that the mini-golf disc has shown */
static s32 D_8022F8D8_ovl_01 = 0;
/* Which way the mini-golf disc turns */
static u8 D_8022F8DC_ovl_01 = MINI_GOLF_TURN_NONE;
/* Which buffers the mini-golf disc shows */
static u8 D_8022F8DD_ovl_01 = 0;
/* The menus' camera orbit's updates */
static s32 D_8022F8E0_ovl_01 = 0;
/* Each player's sound pan, by the player count */
static u8 D_8022F8E4_ovl_01[MAX_PLAYER_COUNT][MAX_PLAYER_COUNT] = {
    {127, 0, 0, 0}, {15, 240, 0, 0}, {15, 127, 240, 0}, {15, 85, 170, 240}};
static s32 D_8022F8F4_ovl_01[] = {
    0, 1, 10, 3, 4, 2, 6, 7, 8, 9, 5, 11, 12, 13, 14, 15, 16, 17};
/* Read by no routine */
static s32 D_8022F93C_ovl_01 = 0;
static s32 D_8022F940_ovl_01[TITLE_LABEL_COUNT][2] = {{110, 86}, {40, 114},
    {90, 114}, {147, 114}, {217, 114}, {96, 142}, {96, 170}};
static f32 D_8022F978_ovl_01 = 0.0f;
/* Where each player's controller icon and label go, by the player count */
static s32 D_8022F97C_ovl_01[MAX_PLAYER_COUNT][MAX_PLAYER_COUNT][2] = {
    {{42, 140}, {76, 120}, {206, 120}, {248, 140}},
    {{42, 140}, {248, 140}, {206, 120}, {248, 140}},
    {{42, 140}, {76, 120}, {211, 120}, {248, 140}},
    {{42, 140}, {76, 120}, {211, 120}, {248, 140}}};
/* Read by no routine */
static s32 D_8022F9FC_ovl_01[11] = {
    110, 109, 117, 97, 106, 105, 105, 97, 84, 124, 99};
/* How far left of the label's place each row of the modes' label image is
 * drawn */
static s32 D_8022FA28_ovl_01[MODE_ICON_COUNT] = {
    85, 85, 41, 60, 55, 60, 75, 66, 59, 0, 61};
static f32 D_8022FA54_ovl_01 = 0.0f;
static f32 D_8022FA58_ovl_01 = 0.0f;
static f32 D_8022FA5C_ovl_01 = 0.0f;
/* Read by no routine */
static s32 D_8022FA60_ovl_01[2] = {0, 0};
static f32 f_MenuCharacterPositions
    [MENU_CHARACTER_LAYOUT_COUNT][MENU_CHARACTER_SLOT_COUNT][3] = {
        {{-84.0f, -43.0f, -1000.0f}, {106.0f, -32.0f, -1000.0f},
            {64.0f, 0.0f, -1000.0f}, {64.0f, 0.0f, -1000.0f},
            {0.0f, 0.0f, 0.0f}},
        {{-106.0f, -32.0f, -1000.0f}, {106.0f, -32.0f, -1000.0f},
            {-106.0f, -52.0f, -1000.0f}, {106.0f, -52.0f, -1000.0f},
            {0.0f, 0.0f, 0.0f}},
        {{-106.0f, -52.0f, -1000.0f}, {-76.0f, -8.0f, -1700.0f},
            {76.0f, -8.0f, -1700.0f}, {106.0f, -52.0f, -1000.0f},
            {0.0f, 0.0f, 0.0f}},
        {{-106.0f, -52.0f, -1000.0f}, {-76.0f, -8.0f, -1700.0f},
            {76.0f, -8.0f, -1700.0f}, {106.0f, -52.0f, -1000.0f},
            {0.0f, 0.0f, 0.0f}}};
/* The character for each grid mark */
static s32 D_8022FB58_ovl_01[CHARACTER_COUNT] = {
    0, 1, 5, 3, 4, 10, 6, 7, 8, 9, 2, 11, 12, 13, 14, 15, 16, 17};
static s32 D_8022FBA0_ovl_01 = NO_VIEWER_CHARACTER;
static s32 D_8022FBA4_ovl_01 = FALSE;
static f32 D_8022FBA8_ovl_01 = 0.0f;
static f32 D_8022FBAC_ovl_01 = 0.0f;
static f32 D_8022FBB0_ovl_01 = 0.0f;
static u32 D_8022FBB4_ovl_01 = 0;
static f32 D_8022FBB8_ovl_01 = 0.0f;
static f32 D_8022FBBC_ovl_01 = 0.0f;
/* The front course's bob phase */
static f32 D_8022FBC0_ovl_01 = 0.0f;
/* Read by no routine */
static s16 D_8022FBC4_ovl_01[6] = {0, 3, 4, 1, 5, 2};
/* Read by no routine; katakana for "Bet", "Out: holes 1-9", "In: holes 10-18"
 * and "1 round: 18 holes" */
static char D_8022FBD0_ovl_01[4][20] = {
    "ﾍﾞｯﾄ", "ｱｳﾄ : 1-9ﾎｰﾙﾌﾟﾚｲ", "ｲﾝ : 10-18ﾎｰﾙﾌﾟﾚｲ", "1ﾗｳﾝﾄﾞ : 18ﾎｰﾙﾌﾟﾚｲ"};
static ImageCorner D_8022FC20_ovl_01[HOLES_BADGE_COUNT] = {
    {252, 134}, {230, 150}, {274, 150}, {252, 162}};
/* Read by no routine */
static ImageCorner D_8022FC30_ovl_01[6] = {
    {24, 173}, {23, 173}, {23, 173}, {29, 161}, {19, 181}, {29, 171}};
/* Each course's trophies' corners, by the number of trophies */
static ImageCorner D_8022FC48_ovl_01
    [COURSE_COUNT][TOURNAMENT_PLACE_COUNT][TOURNAMENT_PLACE_COUNT] = {
        {{{24, 169}, {24, 173}, {24, 173}}, {{16, 169}, {32, 169}, {24, 173}},
            {{16, 169}, {32, 169}, {24, 173}}},
        {{{23, 169}, {23, 173}, {23, 173}}, {{15, 169}, {31, 169}, {23, 173}},
            {{15, 169}, {31, 169}, {23, 173}}},
        {{{23, 169}, {23, 173}, {23, 173}}, {{15, 169}, {31, 169}, {23, 173}},
            {{15, 169}, {31, 169}, {23, 173}}},
        {{{29, 157}, {29, 161}, {29, 161}}, {{21, 157}, {37, 157}, {29, 161}},
            {{21, 157}, {37, 157}, {29, 161}}},
        {{{19, 177}, {19, 181}, {19, 181}}, {{11, 177}, {27, 177}, {19, 181}},
            {{11, 177}, {27, 177}, {19, 181}}},
        {{{29, 167}, {29, 171}, {29, 171}}, {{21, 167}, {37, 167}, {29, 171}},
            {{21, 167}, {37, 167}, {29, 171}}}};
/* Read by no routine */
static char D_8022FD20_ovl_01[16] = "ｺｰｽｹﾞｯﾄ純ﾞ";
/* Read by no routine */
static u8 D_8022FD30_ovl_01[16] = {0x71, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
/* The Bet badge's color phase */
static f32 D_8022FD40_ovl_01 = 0.0f;
/* The chosen mini-golf image's center Y */
static f32 D_8022FD44_ovl_01 = 0.0f;
static Vtx D_8022FD48_ovl_01[WATER_VERTEX_COUNT] = {
    {{{-800, 64, -1024}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{-800, -128, 0}, 0, {0, 25600}, {0x7F, 0x7F, 0x7F, 0xFF}}},
    {{{0, -128, 0}, 0, {16384, 25600}, {0x7F, 0x7F, 0x7F, 0xFF}}},
    {{{800, -128, 0}, 0, {32736, 25600}, {0x7F, 0x7F, 0x7F, 0xFF}}},
    {{{800, 64, -1024}, 0, {32736, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}}};
/* The water's scroll */
static s32 D_8022FD98_ovl_01 = 0;
/* Read by no routine; the values of D_8022FBD0 */
static char D_8022FD9C_ovl_01[4][20] = {
    "ﾍﾞｯﾄ", "ｱｳﾄ : 1-9ﾎｰﾙﾌﾟﾚｲ", "ｲﾝ : 10-18ﾎｰﾙﾌﾟﾚｲ", "1ﾗｳﾝﾄﾞ : 18ﾎｰﾙﾌﾟﾚｲ"};
/* The records' names */
static const char* D_8022FDEC_ovl_01[MINI_GOLF_RECORD_COUNT] = {
    D_802316FC_ovl_01, D_802316EC_ovl_01};
static s32 D_8022FDF4_ovl_01[MAIN_CHOICE_COUNT][2] = {
    {96, 84}, {96, 114}, {96, 144}, {96, 174}};
/* The menu without Transmit skips the CODE ENTRY label */
static s32 D_8022FE14_ovl_01[MAIN_CHOICE_WITHOUT_TRANSMIT_COUNT][2] = {
    {96, 84}, {96, 124}, {96, 164}};
static s32 D_8022FE2C_ovl_01[CLUBHOUSE_ROW_PLACE_COUNT][2] = {{76, 80},
    {76, 102}, {76, 124}, {76, 146}, {76, 168}, {76, 190}, {76, 212},
    {76, 234}};
/* Read by no routine */
static s32 D_8022FE6C_ovl_01[10][2] = {{48, 76}, {132, 76}, {180, 76}, {48, 98},
    {72, 118}, {168, 118}, {48, 140}, {60, 160}, {176, 160}, {90, 182}};
/* Read by no routine */
static u8 D_8022FEBC_ovl_01[3][20] = {
    {0xC3, 0xDE, 0xB0, 0xC0, 0x66, 0x7D, 0x8D, 0xDE, 0x83, 0x79, 0x7C, 0x83,
        0x72, 0x72, 0x83, 0xDE, 0x7D, 0x76, 0x3F, 0x00},
    {0x8A, 0x72, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x72, 0x72, 0x74, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
/* Read by no routine */
static s32 D_8022FEF8_ovl_01 = 0;
static s32 D_8022FEFC_ovl_01 = 0;

/* Read by no routine */
static u8 D_80231AD0_ovl_01[0x1D0];
static Mat4 D_80231CA0_ovl_01;
static Mat4 D_80231CE0_ovl_01;
static LookAt D_80231D20_ovl_01[2];
static Mtx D_80231D60_ovl_01[MAX_PLAYER_COUNT][2];
static Mtx D_80231F60_ovl_01;
static f32 f_MenuCharacterScales[MAX_PLAYER_COUNT];
static Mtx D_80231FB0_ovl_01[2];
static Mtx D_80232030_ovl_01;
/* Read by no routine */
static u8 D_80232070_ovl_01[0x180];
static Mtx D_802321F0_ovl_01;
static Mtx D_80232230_ovl_01;
static Mat4 D_80232270_ovl_01;
static Mat4 D_802322B0_ovl_01;
static s32 D_802322F0_ovl_01;
static s32 D_802322F4_ovl_01;
/* An image buffer of the menu overlay */
static u8 f_MenuImageBuffer[MENU_IMAGE_BUFFER_SIZE];

void* GetMenuImageBuffer(void) {
  return f_MenuImageBuffer;
}

void StartLoadingChosenHole(void) {
  u32 slotIndex;

  GetSaveData();
  slotIndex = 0;
  func_8005AF74();
  do {
    if (IsCharacterSlotBusy(slotIndex)) {
      FreeCharacterSlot(slotIndex);
    }

    slotIndex++;
  } while (slotIndex != CHARACTER_SLOT_COUNT);
  D_800FE334 = 1;
  if (g_MenuScreen == MENU_SCREEN_UNK23 ||
      g_MenuScreen == MENU_SCREEN_RING_SHOT_RECORDS) {
    g_GameState.courseIndex = D_801323A0.courseIndex;
    g_GameState.holeIndex = D_801323A0.holeIndex;
  } else if (g_MenuScreen == MENU_SCREEN_CLUB_TOURNAMENT) {
    g_GameState.courseIndex = GetClubTournamentCourse();
    g_GameState.isMiniGolf = g_GameState.courseIndex >= MINI_GOLF_COURSE_INDEX;
    g_GameState.holeIndex = 0;
  } else if (g_GameState.gameMode == GAME_MODE_TRAINING) {
    g_GameState.courseIndex = 0;
    g_GameState.holeIndex = 0;
  } else {
    if (g_GameState.isMiniGolf) {
      g_GameState.courseIndex = D_801323A0.unk2C + MINI_GOLF_COURSE_INDEX +
          (D_801323A0.unk2B * MINI_GOLF_COURSES_PER_RECORD);
    } else {
      g_GameState.courseIndex = D_801323A0.courseChoice;
    }

    g_GameState.holeIndex = GetFirstHoleIndex();
  }

  g_GameState.unk14 = 0;
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
    return D_801323A0.unk2C + MINI_GOLF_COURSE_INDEX +
        (D_801323A0.unk2B * MINI_GOLF_COURSES_PER_RECORD);
  }

  return D_801323A0.courseChoice;
}

/*
 * The inset camera circles above the middle of the tee and the cup, a turn
 * every 256 updates, while the menus show
 */
#define ORBIT_ANGLE_PER_UPDATE 0x100
#define ORBIT_RADIUS 3000.0f
#define ORBIT_EYE_OFFSET_Y (-3000.0f)
/* Halves the sum of two course positions, and turns it into world units */
#define MIDPOINT_TO_WORLD_SCALE (0.5f / WORLD_TO_COURSE_SCALE)

/* The labels' images, lit and dimmed, that each screen reads as it opens */
#define ASSET_TITLE_LABELS 0x52B /* Players, One to Four, ... Transmit */
#define ASSET_TITLE_LABELS_DIM 0x52C
#define ASSET_MAIN_LABELS 0x523 /* Status, How to Play, Code Entry, Options */
#define ASSET_MAIN_LABELS_DIM 0x524
#define ASSET_CLUBHOUSE_LABELS 0x525 /* Tournament, ... Speed Golf */
#define ASSET_CLUBHOUSE_LABELS_DIM 0x526
#define ASSET_OPTIONS_LABELS 0x529 /* Music, ... Erase Data */
#define ASSET_OPTIONS_LABELS_DIM 0x52A
#define ASSET_OPTION_VALUES 0x97C /* On, ... Simple, in green */
/* The other images that the screens read */
#define ASSET_WIDE_BOARD 0x520 /* A wooden board, 256 by 160 */
#define ASSET_SQUARE_BOARD 0x591 /* A wooden board, 160 by 160 */
#define ASSET_OPTIONS_MARIO 0x97D
#define ASSET_OPTIONS_SHADOW 0x97E
#define ASSET_SKY 0x51F
#define ASSET_WATER 0x4D8
#define ASSET_HOLES_BADGES 0x65D /* Bet, Front 9, Back 9 and 18 */

/*
 * The menus' update: gathers every controller's buttons, starts loading the
 * chosen hole behind screens 6 and 23, circles the inset camera, and runs the
 * current screen. When the screen changes, reads the images that the new one
 * draws. The exit screen saves and starts play, swapping to the play module
 * unless GAME_FLAG_UNK2E is set.
 */
void func_801F4C78_ovl_01(void) {
  AssetFile file;
  Vec3 center;
  Vec3 eye;
  EncodedAsset asset;
  SaveData* save;
  s32 index;
  u8* imageBuffer;
  void* tournamentImage;
  u8* optionBits;
  u8 iconIndex;

  save = GetSaveData();
  func_8005AF74();
  g_MenuRepeatButtons = 0;
  g_MenuPressedButtons = 0;
  for (index = 0; index != g_ControllerCount; index++) {
    g_MenuRepeatButtons |= g_MenuControllerData[index].button;
    g_MenuPressedButtons |= g_MenuControllerData[index].trigger;
  }

  if (g_MenuScreen == MENU_SCREEN_UNK6) {
    if ((GetChosenCourseIndex() != g_GameState.courseIndex) || !D_800FE334) {
      StartLoadingChosenHole();
    }
  } else if (g_MenuScreen == MENU_SCREEN_UNK23) {
    if ((g_GameState.holeIndex != D_801323A0.holeIndex) ||
        (g_GameState.courseIndex != D_801323A0.courseIndex) || !D_800FE334) {
      StartLoadingChosenHole();
    }
  }

  D_8022F8E0_ovl_01++;
  center[AXIS_X] =
      (g_HoleSetup.cupPosition[AXIS_X] + g_HoleSetup.teePosition[AXIS_X]) *
      MIDPOINT_TO_WORLD_SCALE;
  center[AXIS_Y] =
      (g_HoleSetup.cupPosition[AXIS_Y] + g_HoleSetup.teePosition[AXIS_Y]) *
      MIDPOINT_TO_WORLD_SCALE;
  center[AXIS_Z] =
      (g_HoleSetup.cupPosition[AXIS_Z] + g_HoleSetup.teePosition[AXIS_Z]) *
      MIDPOINT_TO_WORLD_SCALE;
  eye[AXIS_X] =
      (cosf(((D_8022F8E0_ovl_01 * ORBIT_ANGLE_PER_UPDATE) & BINARY_ANGLE_MASK) *
           BINARY_ANGLE_TO_RADIANS) *
          ORBIT_RADIUS) +
      center[AXIS_X];
  eye[AXIS_Y] = center[AXIS_Y] + ORBIT_EYE_OFFSET_Y;
  eye[AXIS_Z] =
      (sinf(((D_8022F8E0_ovl_01 * ORBIT_ANGLE_PER_UPDATE) & BINARY_ANGLE_MASK) *
           BINARY_ANGLE_TO_RADIANS) *
          ORBIT_RADIUS) +
      center[AXIS_Z];
  g_Cameras[INSET_CAMERA].view.eye[AXIS_X] = eye[AXIS_X];
  g_Cameras[INSET_CAMERA].view.eye[AXIS_Y] = eye[AXIS_Y];
  g_Cameras[INSET_CAMERA].view.eye[AXIS_Z] = eye[AXIS_Z];
  g_Cameras[INSET_CAMERA].view.lookAt[AXIS_X] = center[AXIS_X];
  g_Cameras[INSET_CAMERA].view.lookAt[AXIS_Y] = center[AXIS_Y];
  g_Cameras[INSET_CAMERA].view.lookAt[AXIS_Z] = center[AXIS_Z];
  SetScreenEffect(SCREEN_EFFECT_NONE, 0);
  D_8022BE08_ovl_01 = 0;
  g_Cameras[0].view.drawFlags = CAMERA_DRAW_SCENE;
  g_Cameras[INSET_CAMERA].view.drawFlags = 0;
  g_InsetView.animation = INSET_VIEW_OPENING;
  switch (g_MenuScreen) {
    case MENU_SCREEN_TITLE:
      memset(&g_GameState, -1, sizeof(GameState));
      g_GameState.isClubTournament = FALSE;
      func_801F70F0_ovl_01();
      break;

    case MENU_SCREEN_UNK5:
      func_801F7D28_ovl_01();
      break;

    case MENU_SCREEN_CHARACTER_SELECT:
      func_80209D50_ovl_01(0, g_ChosenPlayerCount, FALSE);
      break;

    case MENU_SCREEN_UNK24:
      func_80209D50_ovl_01(0, 1, FALSE);
      break;

    case MENU_SCREEN_UNK16:
      func_80209D50_ovl_01(1, 2, TRUE);
      break;

    case MENU_SCREEN_SAVED_GAMES:
      func_8022209C_ovl_01();
      break;

    case MENU_SCREEN_MAIN:
      func_801F9C58_ovl_01();
      break;

    case MENU_SCREEN_MAIN_WITHOUT_TRANSMIT:
      func_801F9E38_ovl_01();
      break;

    case MENU_SCREEN_CLUB_TOURNAMENT:
      func_80228C00_ovl_01();
      break;

    case MENU_SCREEN_CLUBHOUSE:
      func_801F9FF8_ovl_01();
      break;

    case MENU_SCREEN_OPTIONS:
      func_801FA20C_ovl_01();
      break;

    case MENU_SCREEN_HELP:
      if (IsGameFlagSet(GAME_FLAG_UNK56)) {
        func_801F7CD0_ovl_01();
      } else {
        UpdateHelpBoard();
      }

      break;

    case MENU_SCREEN_UNK6:
      if (!g_GameState.isMiniGolf) {
        func_801F8BAC_ovl_01();
      } else {
        func_801F93D8_ovl_01();
      }

      break;

    case MENU_SCREEN_UNK8:
      func_801FADBC_ovl_01();
      break;

    case MENU_SCREEN_UNK20:
      func_80219948_ovl_01(1, g_ChosenPlayerCount);
      break;

    case MENU_SCREEN_UNK22:
      func_80217D20_ovl_01();
      break;

    case MENU_SCREEN_UNK18:
      func_801FB15C_ovl_01();
      break;

    case MENU_SCREEN_UNK12:
      func_8021D0B8_ovl_01();
      break;

    case MENU_SCREEN_RING_SHOT_RECORDS:
      func_8021F79C_ovl_01();
      break;

    case MENU_SCREEN_COURSE_STATS:
      func_8020E390_ovl_01();
      break;

    case MENU_SCREEN_BEST_TIMES:
      func_80213558_ovl_01();
      break;

    case MENU_SCREEN_BIRDIE_BADGES:
      func_801FB11C_ovl_01();
      break;

    case MENU_SCREEN_CHARACTER_TABLE:
      func_80215B84_ovl_01();
      break;

    case MENU_SCREEN_UNK23:
      LeaveMenuWithCourseWipe();
      break;

    case MENU_SCREEN_UNK26:
      LeaveMenuWithWipe();
      break;
  }

  if (g_LoadedMenuScreen != g_MenuScreen) {
    g_LoadedMenuScreen = g_MenuScreen;
    if (g_MenuScreen < MENU_SCREEN_COUNT) {
      D_8022BE8C_ovl_01 ^= 1;
      OpenEncodedAsset(g_MenuScreenImageAssets[g_MenuScreen], &asset);
      ReadEncodedAsset(D_801052BC[D_8022BE8C_ovl_01], &asset);
    }

    switch (g_MenuScreen) {
      case MENU_SCREEN_OPTIONS:
        D_8022BE8E_ovl_01 ^= 1;
        D_8022BE90_ovl_01 ^= 1;
        OpenEncodedAsset(ASSET_OPTIONS_LABELS, &asset);
        ReadEncodedAsset(D_8012D420[D_8022BE8E_ovl_01], &asset);
        OpenEncodedAsset(ASSET_OPTIONS_LABELS_DIM, &asset);
        ReadEncodedAsset(D_801B5CD0[D_8022BE90_ovl_01], &asset);
        OpenEncodedAsset(ASSET_OPTION_VALUES, &asset);
        ReadEncodedAsset(
            ImageAt(D_8012D420[D_8022BE8E_ovl_01], OPTION_VALUES_OFFSET),
            &asset);
        D_8022BE92_ovl_01 ^= 1;
        OpenEncodedAsset(ASSET_SQUARE_BOARD, &asset);
        ReadEncodedAsset(D_800FF420[D_8022BE92_ovl_01], &asset);
        func_802267B0_ovl_01(D_800FC6A0, TOURNAMENT_BUFFER_WIDE_CLOUD);
        func_802267B0_ovl_01(D_800FBE30[0], TOURNAMENT_BUFFER_YES_NO);
        imageBuffer = GetMenuImageBuffer();
        OpenEncodedAsset(ASSET_OPTIONS_MARIO, &asset);
        ReadEncodedAsset(imageBuffer, &asset);
        OpenEncodedAsset(ASSET_GREEN_ARROWS, &asset);
        ReadEncodedAsset(imageBuffer + OPTIONS_ARROWS_OFFSET, &asset);
        OpenEncodedAsset(ASSET_OPTIONS_SHADOW, &asset);
        ReadEncodedAsset(imageBuffer + OPTIONS_SHADOW_OFFSET, &asset);
        InitSram();
        TransferSaveData(save, FALSE);
        optionBits = &D_801323A0.optionBits;
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
        D_8022BE8E_ovl_01 ^= 1;
        D_8022BE90_ovl_01 ^= 1;
        OpenEncodedAsset(ASSET_TITLE_LABELS, &asset);
        ReadEncodedAsset(D_8012D420[D_8022BE8E_ovl_01], &asset);
        OpenEncodedAsset(ASSET_TITLE_LABELS_DIM, &asset);
        ReadEncodedAsset(D_801B5CD0[D_8022BE90_ovl_01], &asset);
        OpenEncodedAsset(ASSET_COURSE_SCENE, &asset);
        ReadEncodedAsset(g_MenuCourseSceneryAsset, &asset);
        D_8022BE92_ovl_01 ^= 1;
        OpenEncodedAsset(ASSET_WIDE_BOARD, &asset);
        ReadEncodedAsset(D_800FF420[D_8022BE92_ovl_01], &asset);
        g_ImportedCharacterColumns[0] = NO_IMPORTED_COLUMN;
        g_ImportedCharacterColumns[1] = NO_IMPORTED_COLUMN;
        g_ImportedCharacterColumns[2] = NO_IMPORTED_COLUMN;
        g_ImportedCharacterColumns[3] = NO_IMPORTED_COLUMN;
        func_8005D334();
        break;

      case MENU_SCREEN_UNK18:
        D_8022BE8E_ovl_01 ^= 1;
        D_8022BE90_ovl_01 ^= 1;
        OpenEncodedAsset(ASSET_TITLE_LABELS, &asset);
        ReadEncodedAsset(D_8012D420[D_8022BE8E_ovl_01], &asset);
        OpenEncodedAsset(ASSET_TITLE_LABELS_DIM, &asset);
        ReadEncodedAsset(D_801B5CD0[D_8022BE90_ovl_01], &asset);
        OpenEncodedAsset(ASSET_COURSE_SCENE, &asset);
        ReadEncodedAsset(g_MenuCourseSceneryAsset, &asset);
        D_8022BE92_ovl_01 ^= 1;
        OpenEncodedAsset(ASSET_WIDE_BOARD, &asset);
        ReadEncodedAsset(D_800FF420[D_8022BE92_ovl_01], &asset);
        break;

      case MENU_SCREEN_UNK5:
        for (index = 0; index != ARRAY_COUNT(D_8012F4B0); index++) {
          OpenEncodedAsset(
              g_PlayerCountIconAssets[g_ChosenPlayerCount - 1][index], &asset);
          ReadEncodedAsset(D_8012F4B0[index], &asset);
        }

        OpenEncodedAsset(ASSET_CONTROLLER_ICONS, &asset);
        ReadEncodedAsset(D_800FF180[0], &asset);
        OpenEncodedAsset(ASSET_PLAYER_LABELS, &asset);
        ReadEncodedAsset(D_800FF180[1], &asset);
        D_8022BE90_ovl_01 ^= 1;
        OpenEncodedAsset(ASSET_MENU_GAME_MODES, &asset);
        ReadEncodedAsset(D_801B5CD0[D_8022BE90_ovl_01], &asset);
        OpenEncodedAsset(ASSET_COURSE_SCENE, &asset);
        ReadEncodedAsset(g_MenuCourseSceneryAsset, &asset);
        D_8022BE92_ovl_01 ^= 1;
        OpenEncodedAsset(ASSET_SQUARE_BOARD, &asset);
        ReadEncodedAsset(D_800FF420[D_8022BE92_ovl_01], &asset);
        iconIndex =
            D_8022BEBC_ovl_01[g_ChosenPlayerCount - 1][D_801323A0.modeChoice];
        D_8022F8C0_ovl_01 = 0;
        D_8022F8C4_ovl_01 = 0;
        D_8022F8C5_ovl_01 = 0;
        OpenEncodedAsset(g_ModeIconAnimations[iconIndex][0], &asset);
        ReadEncodedAsset(D_8010526C[D_8022F8C4_ovl_01], &asset);
        func_802178B0_ovl_01(D_800FBE30[0], PLAYER_STATS_BUFFER_UNK13);
        D_8023BFC0_ovl_01[0] = guRandom() % FIDGET_DELAY_RANGE;
        D_8023BFC0_ovl_01[1] = guRandom() % FIDGET_DELAY_RANGE;
        D_8023BFC0_ovl_01[2] = guRandom() % FIDGET_DELAY_RANGE;
        D_8023BFC0_ovl_01[3] = guRandom() % FIDGET_DELAY_RANGE;
        for (index = 0; index != MAXCONTROLLERS; index++) {
          D_801B70F0[index] = FALSE;
        }

        OpenEncodedAsset(ASSET_SPINNING_BADGE, &asset);
        ReadEncodedAsset(D_800FC898, &asset);
        func_8020E26C_ovl_01(D_800FBE30[1], COURSE_MENU_BUFFER_LARGE_DIGITS);
        D_8022F8C8_ovl_01 = 0;
        D_8022F8CC_ovl_01 = 0;
        D_8022F8CD_ovl_01 = 0;
        OpenEncodedAsset(D_8022F848_ovl_01[D_8022F8C0_ovl_01], &asset);
        ReadEncodedAsset(D_801B93E0[0], &asset);
        break;

      case MENU_SCREEN_CHARACTER_SELECT:
      case MENU_SCREEN_UNK24:
        func_8005C674(g_ChosenPlayerCount, 0);
        D_8022BE92_ovl_01 ^= 1;
        func_8020B2C4_ovl_01(
            D_800FF420[D_8022BE92_ovl_01], CHARACTER_SELECT_BUFFER_BOARD);
        D_8022BE8E_ovl_01 ^= 1;
        D_8022BE90_ovl_01 ^= 1;
        func_8020B2C4_ovl_01(D_8012D420[D_8022BE8E_ovl_01],
            CHARACTER_SELECT_BUFFER_PLAYER_LABELS);
        func_8020B2C4_ovl_01(
            D_801B5CD0[D_8022BE90_ovl_01], CHARACTER_SELECT_BUFFER_CURSOR_BOX);
        func_8020B2C4_ovl_01(D_801B60D0, CHARACTER_SELECT_BUFFER_UNREAD);
        func_8020B2C4_ovl_01(
            D_800FBE30[0], CHARACTER_SELECT_BUFFER_CURVE_CELLS);
        func_8020B438_ovl_01();
        break;

      case MENU_SCREEN_UNK16:
        func_8005C674(1, 0);
        func_80209500_ovl_01();
        OpenEncodedAsset(ASSET_COURSE_SCENE, &asset);
        ReadEncodedAsset(g_MenuCourseSceneryAsset, &asset);
        D_8022BE92_ovl_01 ^= 1;
        func_8020B2C4_ovl_01(
            D_800FF420[D_8022BE92_ovl_01], CHARACTER_SELECT_BUFFER_BOARD);
        D_8022BE8E_ovl_01 ^= 1;
        D_8022BE90_ovl_01 ^= 1;
        func_8020B2C4_ovl_01(D_8012D420[D_8022BE8E_ovl_01],
            CHARACTER_SELECT_BUFFER_PLAYER_LABELS);
        func_8020B2C4_ovl_01(
            D_801B5CD0[D_8022BE90_ovl_01], CHARACTER_SELECT_BUFFER_CURSOR_BOX);
        func_8020B2C4_ovl_01(D_801B60D0, CHARACTER_SELECT_BUFFER_UNREAD);
        func_8020B2C4_ovl_01(
            D_800FBE30[0], CHARACTER_SELECT_BUFFER_CURVE_CELLS);
        func_8020B438_ovl_01();
        func_8020B3CC_ovl_01();
        break;

      case MENU_SCREEN_SAVED_GAMES:
        D_8022BE8E_ovl_01 ^= 1;
        D_8022BE90_ovl_01 ^= 1;
        OpenEncodedAsset(ASSET_TITLE_LABELS_DIM, &asset);
        ReadEncodedAsset(D_801B5CD0[D_8022BE90_ovl_01], &asset);
        OpenEncodedAsset(ASSET_COURSE_SCENE, &asset);
        ReadEncodedAsset(g_MenuCourseSceneryAsset, &asset);
        D_8022BE92_ovl_01 ^= 1;
        func_802178B0_ovl_01(
            D_800FF420[D_8022BE92_ovl_01], PLAYER_STATS_BUFFER_UNK12);
        D_8022F8C0_ovl_01 = 0;
        D_8022F8C4_ovl_01 = 0;
        D_8022F8C5_ovl_01 = 0;
        func_80221B20_ovl_01();
        func_80221EBC_ovl_01(D_801049A0[0], SAVE_MENU_BUFFER_SLOTS);
        func_80221EBC_ovl_01(D_8010526C, SAVE_MENU_BUFFER_CHOSEN);
        func_80222038_ovl_01(D_801B60D0, 1);
        func_80222038_ovl_01(D_801B5CD0[D_8022BE90_ovl_01], 2);
        func_802218C0_ovl_01(D_800FF180[0], SAVE_GAME_BUFFER_CONTROLLER_ICONS);
        func_802218C0_ovl_01(D_800FF180[1], SAVE_GAME_BUFFER_PLAYER_LABELS);
        func_802218C0_ovl_01(D_800FBE30[0], SAVE_GAME_BUFFER_FIRST_BOARD);
        func_802218C0_ovl_01(D_800FBE30[1], SAVE_GAME_BUFFER_SECOND_BOARD);
        func_802218C0_ovl_01(D_800FC6A0, SAVE_GAME_BUFFER_THIRD_BOARD);
        func_802218C0_ovl_01(
            D_8012D420[D_8022BE8E_ovl_01], SAVE_GAME_BUFFER_COURSE);
        func_802218C0_ovl_01(D_800FF4C4[0], SAVE_GAME_BUFFER_CHOSEN_COURSE);
        D_8023BFC0_ovl_01[0] = guRandom() % FIDGET_DELAY_RANGE;
        D_8023BFC0_ovl_01[1] = guRandom() % FIDGET_DELAY_RANGE;
        D_8023BFC0_ovl_01[2] = guRandom() % FIDGET_DELAY_RANGE;
        D_8023BFC0_ovl_01[3] = guRandom() % FIDGET_DELAY_RANGE;
        for (index = 0; index != MAXCONTROLLERS; index++) {
          D_801B70F0[index] = FALSE;
        }

        break;

      case MENU_SCREEN_MAIN:
        D_8022BE8E_ovl_01 ^= 1;
        D_8022BE90_ovl_01 ^= 1;
        OpenEncodedAsset(ASSET_MAIN_LABELS, &asset);
        ReadEncodedAsset(D_8012D420[D_8022BE8E_ovl_01], &asset);
        OpenEncodedAsset(ASSET_MAIN_LABELS_DIM, &asset);
        ReadEncodedAsset(D_801B5CD0[D_8022BE90_ovl_01], &asset);
        D_8022BE92_ovl_01 ^= 1;
        OpenEncodedAsset(ASSET_WIDE_BOARD, &asset);
        ReadEncodedAsset(D_800FF420[D_8022BE92_ovl_01], &asset);
        break;

      case MENU_SCREEN_MAIN_WITHOUT_TRANSMIT:
        D_8022BE8E_ovl_01 ^= 1;
        D_8022BE90_ovl_01 ^= 1;
        OpenEncodedAsset(ASSET_MAIN_LABELS, &asset);
        ReadEncodedAsset(D_8012D420[D_8022BE8E_ovl_01], &asset);
        OpenEncodedAsset(ASSET_MAIN_LABELS_DIM, &asset);
        ReadEncodedAsset(D_801B5CD0[D_8022BE90_ovl_01], &asset);
        D_8022BE92_ovl_01 ^= 1;
        OpenEncodedAsset(ASSET_WIDE_BOARD, &asset);
        ReadEncodedAsset(D_800FF420[D_8022BE92_ovl_01], &asset);
        break;

      case MENU_SCREEN_CLUB_TOURNAMENT:
        D_8022BE92_ovl_01 ^= 1;
        D_8022BE8E_ovl_01 ^= 1;
        tournamentImage = GetMenuImageBuffer();
        OpenEncodedAsset(ASSET_PLACE_DIGITS, &asset);
        ReadEncodedAsset(tournamentImage, &asset);
        func_80228A80_ovl_01(D_801B60D0, CODE_ENTRY_BUFFER_NAME_PLATE);
        func_80228A80_ovl_01(
            D_800FF420[D_8022BE92_ovl_01], CODE_ENTRY_BUFFER_BAR);
        func_80228A80_ovl_01(
            D_8012D420[D_8022BE8E_ovl_01], CODE_ENTRY_BUFFER_GLYPHS);
        func_80228A80_ovl_01(D_800FBE30[0], CODE_ENTRY_BUFFER_POINTING_HAND);
        func_80228A80_ovl_01(D_800FBE30[1], CODE_ENTRY_BUFFER_CURSOR_BOX);
        func_802267B0_ovl_01(D_800FC6A0, TOURNAMENT_BUFFER_WIDE_CLOUD);
        func_802267B0_ovl_01(D_801049A0[0][0], TOURNAMENT_BUFFER_YES_NO);
        func_802267B0_ovl_01(
            D_801049A0[0][1], TOURNAMENT_BUFFER_NINTENDO_BADGE);
        func_802267B0_ovl_01(D_801049A0[1][0], TOURNAMENT_BUFFER_ICON);
        func_802267B0_ovl_01(D_801049A0[1][1], TOURNAMENT_BUFFER_MENU_WINDOW);
        func_802267B0_ovl_01(D_801049A0[2][0], TOURNAMENT_BUFFER_ROW_FILE);
        break;

      case MENU_SCREEN_HELP:
        if (IsGameFlagSet(GAME_FLAG_UNK56)) {
          D_8022BE8E_ovl_01 ^= 1;
          D_8022BE90_ovl_01 ^= 1;
          OpenEncodedAsset(ASSET_TITLE_LABELS, &asset);
          ReadEncodedAsset(D_8012D420[D_8022BE8E_ovl_01], &asset);
          OpenEncodedAsset(ASSET_TITLE_LABELS_DIM, &asset);
          ReadEncodedAsset(D_801B5CD0[D_8022BE90_ovl_01], &asset);
          OpenEncodedAsset(ASSET_COURSE_SCENE, &asset);
          ReadEncodedAsset(g_MenuCourseSceneryAsset, &asset);
          D_8022BE92_ovl_01 ^= 1;
          OpenEncodedAsset(ASSET_WIDE_BOARD, &asset);
          ReadEncodedAsset(D_800FF420[D_8022BE92_ovl_01], &asset);
          D_8022F8C0_ovl_01 = 0;
          D_8022F8C4_ovl_01 = 0;
          D_8022F8C5_ovl_01 = 0;
          D_8022BE94_ovl_01 = 0;
        } else {
          LoadHelpBoardImage(D_801049A0[0][0], HELP_BOARD_IMAGE_HAND);
          LoadHelpBoardImage(D_801B93E0[1], HELP_BOARD_IMAGE_ARROWS);
          D_8022BE92_ovl_01 ^= 1;
          LoadHelpBoardImage(
              D_800FF420[D_8022BE92_ovl_01], HELP_BOARD_IMAGE_CHALKBOARD);
          D_8022F8CE_ovl_01 = 0;
        }

        break;

      case MENU_SCREEN_CLUBHOUSE:
        D_8022BE8E_ovl_01 ^= 1;
        D_8022BE90_ovl_01 ^= 1;
        OpenEncodedAsset(ASSET_CLUBHOUSE_LABELS, &asset);
        ReadEncodedAsset(D_8012D420[D_8022BE8E_ovl_01], &asset);
        OpenEncodedAsset(ASSET_CLUBHOUSE_LABELS_DIM, &asset);
        ReadEncodedAsset(D_801B5CD0[D_8022BE90_ovl_01], &asset);
        D_8022BE92_ovl_01 ^= 1;
        OpenEncodedAsset(ASSET_WIDE_BOARD, &asset);
        ReadEncodedAsset(D_800FF420[D_8022BE92_ovl_01], &asset);
        break;

      case MENU_SCREEN_UNK6:
        if (g_GameState.isMiniGolf) {
          for (index = 0; index != MINI_GOLF_DISC_IMAGE_COUNT; index++) {
            OpenEncodedAsset(
                g_MiniGolfDiscFrameAssets[(D_801323A0.unk2C *
                                              MINI_GOLF_DISC_IMAGE_COUNT) +
                    index][MINI_GOLF_FACING_FRAME],
                &asset);
            ReadEncodedAsset(D_801049A0[0][index], &asset);
            OpenEncodedAsset(
                g_MiniGolfDiscFrameAssets[(D_801323A0.unk2C *
                                              MINI_GOLF_DISC_IMAGE_COUNT) +
                    index][MINI_GOLF_FACING_FRAME],
                &asset);
            ReadEncodedAsset(D_801049A0[1][index], &asset);
          }

          for (index = 0; index != ARRAY_COUNT(D_801052C4); index++) {
            OpenAssetFile(
                g_MiniGolfDiscFiles[(D_801323A0.unk2C * 2) + index], &file);
            ReadAssetFile(D_801052C4[index], GetAssetFileSize(&file), &file);
          }
        } else {
          for (index = 0; index != COURSE_COUNT; index++) {
            OpenEncodedAsset(g_CourseIconAssets[index], &asset);
            ReadEncodedAsset(D_801049A0[0][index], &asset);
          }
        }

        OpenEncodedAsset(ASSET_SKY, &asset);
        ReadEncodedAsset(g_MenuCourseSceneryAsset, &asset);
        OpenEncodedAsset(ASSET_WATER, &asset);
        ReadEncodedAsset(
            ImageAt(g_MenuCourseSceneryAsset, COURSE_SELECT_WATER_OFFSET),
            &asset);
        OpenEncodedAsset(ASSET_BLENDED_TEXTURE_SCROLLING_LAYER, &asset);
        ReadEncodedAsset(
            ImageAt(g_MenuCourseSceneryAsset, COURSE_SELECT_LAYER_OFFSET),
            &asset);
        D_8022BE80_ovl_01 = 0;
        OpenEncodedAsset(
            g_CourseWheelImageAssets[D_801323A0.courseChoice][0], &asset);
        ReadEncodedAsset(D_800FBDFC[0], &asset);
        OpenEncodedAsset(
            g_CourseWheelImageAssets[D_801323A0.courseChoice][0], &asset);
        ReadEncodedAsset(D_800FBDFC[1], &asset);
        for (index = 0; index != COURSE_FILE_COUNT; index++) {
          OpenAssetFile(
              g_CourseTrophyFiles[D_801323A0.courseChoice][index], &file);
          ReadAssetFile(D_800FBE38[index], GetAssetFileSize(&file), &file);
        }

        OpenEncodedAsset(ASSET_HOLES_BADGES, &asset);
        ReadEncodedAsset(D_80106100, &asset);
        OpenEncodedAsset(ASSET_SPINNING_BADGE, &asset);
        ReadEncodedAsset(D_800FC898, &asset);
        D_8022BE90_ovl_01 ^= 1;
        func_8020E26C_ovl_01(
            D_801B5CD0[D_8022BE90_ovl_01], COURSE_MENU_BUFFER_LARGE_DIGITS);
        D_8022F8C8_ovl_01 = 0;
        D_8022F8CC_ovl_01 = 0;
        D_8022F8CD_ovl_01 = 0;
        D_8022BE8A_ovl_01 = 0;
        D_8022BE92_ovl_01 ^= 1;
        func_802178B0_ovl_01(
            D_800FF420[D_8022BE92_ovl_01], PLAYER_STATS_BUFFER_ICONS);
        func_802178B0_ovl_01(D_801B93E0[0], PLAYER_STATS_BUFFER_UNK2);
        func_802178B0_ovl_01(D_801B60D0, PLAYER_STATS_BUFFER_UNK3);
        func_802178B0_ovl_01(D_8010526C[0], PLAYER_STATS_BUFFER_UNK4);
        func_802178B0_ovl_01(D_8010526C[1], PLAYER_STATS_BUFFER_UNK5);
        func_802178B0_ovl_01(D_801B93E0[1], PLAYER_STATS_BUFFER_UNK7);
        func_802178B0_ovl_01(D_800FF180[0], PLAYER_STATS_BUFFER_PLAYER_LABELS);
        func_802178B0_ovl_01(D_800FF180[1], PLAYER_STATS_BUFFER_UNK8);
        D_8022BE8E_ovl_01 ^= 1;
        func_802178B0_ovl_01(
            D_8012D420[D_8022BE8E_ovl_01], PLAYER_STATS_BUFFER_GAME_LABELS);
        func_802178B0_ovl_01(D_8012D3AC, PLAYER_STATS_BUFFER_BRICK_WALL);
        func_802178B0_ovl_01(D_800FBE30[0], PLAYER_STATS_BUFFER_UNK12);
        ComputeRoundStatistics();
        D_8022F8D4_ovl_01 = MINI_GOLF_FACING_FRAME;
        D_8022F8D8_ovl_01 = 0;
        D_8022F8DC_ovl_01 = MINI_GOLF_TURN_NONE;
        D_8022F8DD_ovl_01 = 0;
        break;

      case MENU_SCREEN_COURSE_STATS:
        D_8022F8CD_ovl_01 = 0;
        D_8022F8C8_ovl_01 = 0;
        if (g_IsDebugEnabled) {
          func_80221B20_ovl_01();
        }

        func_8020E010_ovl_01(D_801049A0[0]);
        func_8020E0AC_ovl_01(D_800FF4C4, COURSE_BUFFER_FIRST);
        func_8020E26C_ovl_01(D_800FC898, COURSE_MENU_BUFFER_HOLE_IN_ONE_BADGE);
        func_8020E26C_ovl_01(D_801B60D0, COURSE_MENU_BUFFER_NAME_PLATE);
        func_8020E26C_ovl_01(D_8010526C[0], COURSE_MENU_BUFFER_SPINNING_BADGE);
        func_8020E26C_ovl_01(
            D_800FF180[0], COURSE_MENU_BUFFER_DIGITS_AND_ARROWS);
        D_8022BE90_ovl_01 ^= 1;
        func_8020E26C_ovl_01(
            D_801B5CD0[D_8022BE90_ovl_01], COURSE_MENU_BUFFER_LARGE_DIGITS);
        func_8020E26C_ovl_01(D_800FBE30[0], COURSE_MENU_BUFFER_GREEN_ARROWS);
        ComputeRoundStatistics();
        D_801323A0.statsCourse = GetOpenCourseCount();
        break;

      case MENU_SCREEN_BEST_TIMES:
        func_80221B20_ovl_01();
        func_80213390_ovl_01(D_801049A0[0]);
        func_80213464_ovl_01(NULL, BEST_TIMES_BUFFER_FACE_STRIP);
        D_8022BE90_ovl_01 ^= 1;
        func_8020E26C_ovl_01(
            D_801B5CD0[D_8022BE90_ovl_01], COURSE_MENU_BUFFER_LARGE_DIGITS);
        func_80213464_ovl_01(
            D_801B5CD0[D_8022BE90_ovl_01], BEST_TIMES_BUFFER_UNREAD);
        func_8020E26C_ovl_01(D_801B60D0, COURSE_MENU_BUFFER_NAME_PLATE);
        D_8022BE92_ovl_01 ^= 1;
        func_80213464_ovl_01(
            D_800FF420[D_8022BE92_ovl_01], BEST_TIMES_BUFFER_PLACES);
        func_80213464_ovl_01(D_800FBE30[0], BEST_TIMES_BUFFER_GREEN_ARROWS);
        func_8021585C_ovl_01();
        break;

      case MENU_SCREEN_CHARACTER_TABLE:
        func_80221B20_ovl_01();
        func_80213464_ovl_01(NULL, BEST_TIMES_BUFFER_FACE_STRIP);
        D_8022BE90_ovl_01 ^= 1;
        func_8020E26C_ovl_01(
            D_801B5CD0[D_8022BE90_ovl_01], COURSE_MENU_BUFFER_LARGE_DIGITS);
        D_8022BE90_ovl_01 ^= 1;
        func_802159C0_ovl_01(
            D_801B5CD0[D_8022BE90_ovl_01], TABLE_BUFFER_CURSOR);
        func_802159C0_ovl_01(D_801B60D0, TABLE_BUFFER_UNREAD);
        func_8021730C_ovl_01();
        func_802159C0_ovl_01(D_800FBE30[0], TABLE_BUFFER_ARROWS);
        func_802159C0_ovl_01(D_800FBE30[1], TABLE_BUFFER_STAR);
        func_802177D0_ovl_01();
        break;

      case MENU_SCREEN_UNK12:
        func_80221B20_ovl_01();
        func_8021CF10_ovl_01(D_801049A0[0]);
        D_8022BE92_ovl_01 ^= 1;
        func_8021342C_ovl_01(D_800FF420[D_8022BE92_ovl_01]);
        D_8022BE8E_ovl_01 ^= 1;
        D_8022BE90_ovl_01 ^= 1;
        func_8020E26C_ovl_01(
            D_801B5CD0[D_8022BE8E_ovl_01], COURSE_MENU_BUFFER_LARGE_DIGITS);
        func_8021CFAC_ovl_01(
            D_8012D420[D_8022BE90_ovl_01], SCORE_LABEL_BUFFER_CURSOR_BOX);
        func_8021CFAC_ovl_01(D_801B60D0, SCORE_LABEL_BUFFER_UNREAD);
        func_8021CFAC_ovl_01(D_800FBE30[0], SCORE_LABEL_BUFFER_LABELS);
        func_8021E01C_ovl_01();
        func_8021EF64_ovl_01();
        break;

      case MENU_SCREEN_RING_SHOT_RECORDS:
        func_80221B20_ovl_01();
        D_8022BE92_ovl_01 ^= 1;
        func_8021342C_ovl_01(D_800FF420[D_8022BE92_ovl_01]);
        D_8022BE8E_ovl_01 ^= 1;
        D_8022BE90_ovl_01 ^= 1;
        func_8020E26C_ovl_01(
            D_801B5CD0[D_8022BE90_ovl_01], COURSE_MENU_BUFFER_LARGE_DIGITS);
        func_8021F5F0_ovl_01(D_801B60D0, RING_SHOT_RECORD_BUFFER_UNREAD);
        func_8021F5F0_ovl_01(
            D_8012D420[D_8022BE8E_ovl_01], RING_SHOT_RECORD_BUFFER_RAINBOW_BOX);
        func_8021F5F0_ovl_01(
            D_8010526C[0], RING_SHOT_RECORD_BUFFER_COURSE_NUMBERS);
        func_8021F6E0_ovl_01(D_801049A0[0]);
        func_8021F5F0_ovl_01(
            D_800FBE30[0], RING_SHOT_RECORD_BUFFER_GREEN_ARROWS);
        func_80220A30_ovl_01();
        func_80220BB0_ovl_01();
        break;
    }
  }

  if (g_MenuScreen == MENU_SCREEN_EXIT) {
    TransferSaveData(save, TRUE);
    if (g_GameState.gameMode == GAME_MODE_TRAINING) {
      g_MenuScreen = MENU_SCREEN_UNK6;
    } else {
      g_MenuScreen = MENU_SCREEN_UNK18;
    }

    if (g_GameState.unk30) {
      InitPlay(PLAY_KIND_WITHOUT_HOLE_DISPLAY);
      InitRoundHud();
      osSyncPrintf("ContPhase2\n");
    } else {
      g_GameState.saveCount = FALSE;
      InitPlay(PLAY_KIND_WITHOUT_HOLE_DISPLAY);
      func_801F6B28_ovl_01();
      InitRoundHud();
    }

    SetGameFlag(GAME_FLAG_HOLE_LOADED);
    if (IsGameFlagSet(GAME_FLAG_UNK2E)) {
      LoadMenuResources();
      func_801F6B20_ovl_01();
    } else {
      g_Scene = SCENE_SWAPPING_MODULES;
      g_NextScene = SCENE_PLAY_UNK12;
      g_SceneModuleToUnload = MODULE_MENU;
      g_SceneModuleToLoad = MODULE_PLAY;
      RequestSubModule();
    }
  }
}

/*
 * Doesn't match: the original reloads g_GameState.playerCount for each pass of
 * the outer loop, where this keeps it in a register.
 */
#ifdef NON_MATCHING
void func_801F6A38_ovl_01(void) {
  s32 playerIndex;
  s32 otherIndex;

  D_801051F0 = TRUE;
  for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
    for (otherIndex = 0; otherIndex != g_GameState.playerCount; otherIndex++) {
      if (playerIndex != otherIndex &&
          g_PlayerControllerIndices[playerIndex] != NO_CONTROLLER_INDEX &&
          g_PlayerControllerIndices[otherIndex] != NO_CONTROLLER_INDEX &&
          g_PlayerControllerIndices[playerIndex] ==
              g_PlayerControllerIndices[otherIndex]) {
        D_801051F0 = FALSE;
      }
    }
  }

  for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
    g_Players[playerIndex].soundPan =
        D_8022F8E4_ovl_01[g_GameState.playerCount - 1][playerIndex];
  }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_801F6A38_ovl_01);
#endif

void func_801F6B20_ovl_01(void) {}

const char D_80231514_ovl_01[] = "%s";

/* Whether a game mode plays the holes chosen on the menu */
#define IsHoleChoiceMode(mode) \
  ((mode) == GAME_MODE_STROKE || (mode) == GAME_MODE_MATCH_GAME || \
      (mode) == GAME_MODE_RING_SHOT || (mode) == GAME_MODE_SKINS_MATCH || \
      (mode) == GAME_MODE_CLUB_SLOTS)

void func_801F6B28_ovl_01(void) {
  SaveData* save = GetSaveData();
  MenuMemory* memory = func_8005AF74();
  Player* player;
  s32 index;
  s32 slot;
  s32 clubSet;
  s32* holeSelection;
  s32 row;
  s32 column;

  memset(&g_RoundHud, 0, sizeof(g_RoundHud));
  if (g_GameState.replayKind != REPLAY_KIND_NONE) {
    if (g_GameState.replayKind == REPLAY_KIND_RING_SHOT) {
      LoadRingShotRecord(&save->ringShotRecords[memory->ringShotReplaySlot]);
    } else {
      LoadBestShotReplay(&save->rankedRecords[func_8021E468_ovl_01()]);
    }

    g_GameState.unk14 = 0;
    g_GameState.playerCount = 1;
    g_GameState.step = GAME_STATE_START;
    return;
  }

  StartBets();
  if (func_8022BDD0_ovl_01()) {
    g_GameState.holeSelection = HOLE_SELECTION_ALL;
    g_GameState.courseIndex = GetClubTournamentCourse();
    g_GameState.gameMode = GetClubTournamentGameMode();
    g_GameState.isMiniGolf = g_GameState.courseIndex >= MINI_GOLF_COURSE_INDEX;
    for (index = 0; index != MENU_UNKE_COUNT; index++) {
      g_GameState.betRates[index] = -1;
    }

    for (index = 0; index != MAX_PLAYER_COUNT; index++) {
      D_8023BFDC_ovl_01[index] = 0;
    }

    if (func_8022BDC4_ovl_01() != -1) {
      D_8012F480[0] = func_8022BDC4_ovl_01();
      D_800FBE08[0] = 0;
    }
  } else {
    if (g_GameState.gameMode == GAME_MODE_RING_SHOT) {
      g_GameState.courseIndex = D_801323A0.courseIndex;
    } else if (g_GameState.gameMode == GAME_MODE_TRAINING) {
      g_GameState.courseIndex = 0;
    } else {
      g_GameState.courseIndex = GetChosenCourseIndex();
    }

    if (IsHoleChoiceMode(g_GameState.gameMode)) {
      /* Through a pointer, as the original keeps the address in a register for
       * every store */
      holeSelection = &g_GameState.holeSelection;
      *holeSelection = D_8022BE1C_ovl_01;
      if (MAP_CAMERA_MODE_TOP_BYTE == 1) {
        switch (D_801323A0.holesChoice) {
          case HOLES_CHOICE_UNK0:
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
    } else {
      g_GameState.holeSelection = HOLE_SELECTION_ALL;
    }
  }

  g_GameState.unk14 = 0;
  g_GameState.playerCount = g_ChosenPlayerCount;
  if (g_GameState.gameMode == GAME_MODE_GET_CHARACTER) {
    g_GameState.playerCount = 2;
  }

  g_GameState.step = GAME_STATE_RELOAD_HOLE;
  func_801F6A38_ovl_01();
  if (g_GameState.gameMode == GAME_MODE_TOURNAMENT) {
    StartRoundStats();
  }

  for (index = 0; index != g_GameState.playerCount; index++) {
    player = &g_Players[index];
    player->handicap = D_8023BFDC_ovl_01[index];
    player->unk22 = D_800FBE08[index];
    player->characterIndex = D_8012F480[index];
    player->controllerIndex = g_PlayerControllerIndices[index];
    sprintf(player->name, D_80231514_ovl_01,
        D_8022BF70_ovl_01[D_8022F8F4_ovl_01[D_8012F480[index]]]);
    if (D_8012F480[index] >= CHARACTER_KID) {
      row = D_801323A0.characterRows[index];
      column = D_801323A0.characterColumns[index];
      if (row == GAME_BOY_CHARACTER_ROW) {
        player->gameBoyCharacterIndex = column;
      }

      sprintf(player->name, D_80231514_ovl_01,
          g_GameBoyCharacters[player->gameBoyCharacterIndex].name);
    } else {
      player->gameBoyCharacterIndex = -1;
      sprintf(player->name, D_80231514_ovl_01,
          D_8022BF70_ovl_01[D_8022F8F4_ovl_01[D_8012F480[index]]]);
    }

    if (player->controllerIndex == MAXCONTROLLERS) {
      g_Players[index].isComputer = TRUE;
    } else {
      g_Players[index].isComputer = FALSE;
    }

    for (slot = 0; slot != CLUB_SET_SIZE; slot++) {
      if (!((D_8022BE50_ovl_01[index] >> slot) & 1)) {
        player->clubs[slot] = NO_CLUB;
      } else {
        clubSet = D_8012F480[index];
        if (clubSet >= CHARACTER_KID) {
          clubSet = player->gameBoyCharacterIndex;
          clubSet += CHARACTER_KID;
        }

        player->clubs[slot] = slot + (clubSet * CLUB_SET_SIZE);
      }
    }

    for (; slot != CLUB_SLOT_COUNT; slot++) {
      player->clubs[slot] = NO_CLUB;
    }

    if (g_GameState.gameMode == GAME_MODE_TRAINING) {
      player->unk28 = PLAYER_UNK28_TRAINING;
    } else if (g_GameState.holeSelection == HOLE_SELECTION_FRONT_NINE ||
        g_GameState.holeSelection == HOLE_SELECTION_BACK_NINE) {
      player->unk28 = PLAYER_UNK28_NINE_HOLES;
    } else {
      player->unk28 = PLAYER_UNK28_EIGHTEEN_HOLES;
    }

    LoadPlayerFace(index, (s8)player->characterIndex, (s8)player->paletteIndex);
  }

  if (g_GameState.gameMode == GAME_MODE_GET_CHARACTER) {
    g_Players[1].isComputer = TRUE;
    g_Players[1].controllerIndex = MAXCONTROLLERS;
  }
}

void func_801F70F0_ovl_01(void) {
  MenuMemory* memory = func_8005AF74();
  s32 index;
  s8 titleChoice;

  if (IsHudAnimationDone() != TRUE) {
    return;
  }

  D_8022BE84_ovl_01 = 0;
  D_8022BE88_ovl_01 = 0;
  D_8022BE89_ovl_01 = 0;
  D_8023BFD0_ovl_01 = 0;
  D_802322F0_ovl_01 = 0;
  D_802322F4_ovl_01 = 0;
  g_GameState.unk30 = 0;
  g_GameState.isSuddenDeath = FALSE;
  g_GameState.isMiniGolf = FALSE;
  g_GameState.replayKind = REPLAY_KIND_NONE;
  g_GameState.currentPlayerIndex = 0;
  if (GetWipeKind() != WIPE_NONE && D_801B9410 == 0) {
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

  if (GetWipeKind() != WIPE_NONE && D_801B9410 == 1) {
    return;
  }

  if (g_MenuPressedButtons & A_BUTTON) {
    PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
    titleChoice = D_801323A0.titleChoice;
    if (titleChoice == TITLE_CHOICE_NEW_GAME) {
      for (index = 0; index != MAX_PLAYER_COUNT; index++) {
        memory->characterColumns[index] = 0;
        memory->characterRows[index] = 0;
      }

      memory->modeChoice = 0;
      memory->modeSpin = 0;
      memory->unkD = 0;
      for (index = 0; index != MENU_UNKE_COUNT; index++) {
        memory->unkE[index] = 0;
      }

      memory->courseChoice = 0;
      memory->courseSpin = 0;
      memory->holeIndex = 0;
      memory->courseIndex = 0;
      StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
      g_MenuScreen = MENU_SCREEN_CHARACTER_SELECT;
      g_ChosenPlayerCount = D_801323A0.playerCountChoice + 1;
      for (index = 0; index != MAX_PLAYER_COUNT; index++) {
        D_800FBE08[index] = 0;
      }

      D_801B7070 = 0;
      for (index = 0; index != D_8022BE0C_ovl_01; index++) {
        g_TakenCharacters.isCharacterTaken[index] = 0;
      }
    } else if (titleChoice == TITLE_CHOICE_SAVED_GAMES) {
      if (g_HasBroughtOverCharacter) {
        D_801B9410 = 0;
        LoadWipeShape(TRANSMIT_WIPE_SHAPE);
        StartWipe(WIPE_CLOSING);
        D_801052E8 = 0;
      } else {
        g_ChosenPlayerCount = titleChoice;
        StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
        g_MenuScreen = MENU_SCREEN_SAVED_GAMES;
      }
    } else if (titleChoice == TITLE_CHOICE_MENUS) {
      g_ChosenPlayerCount = 1;
      if ((g_MenuRepeatButtons == TRANSMIT_BUTTONS) |
          (g_MenuRepeatButtons == TRANSMIT_OTHER_BUTTONS)) {
        PlaySound(
            SOUND_CONTROLS_CLOSE, TRANSMIT_SOUND_CHANNEL, MENU_VOLUME_SOFT);
        D_801323A0.isTransmitShown = TRUE;
        g_MenuScreen = titleChoice;
      } else {
        D_801323A0.isTransmitShown = FALSE;
        g_MenuScreen = MENU_SCREEN_MAIN_WITHOUT_TRANSMIT;
      }
    }

    if (D_801B71CC != g_MenuScreen ||
        D_800FBE2C != D_801323A0.playerCountChoice) {
      D_801323A0.modeChoice = 0;
      g_TakenCharacters.unk0 = 0;
    }

    D_801052E8 = 0;
    D_801B9410 = 0;
    D_801B71CC = g_MenuScreen;
    D_800FBE2C = D_801323A0.playerCountChoice;
    return;
  }

  if (g_MenuPressedButtons & B_BUTTON) {
    StartHudAnimation(HUD_ANIMATION_SPIRAL);
    g_Scene = SCENE_SWAPPING_MODULES;
    g_NextScene = SCENE_TITLE_SCREEN;
    g_SceneModuleToUnload = MODULE_MENU;
    g_SceneModuleToLoad = MODULE_TITLE_SCREEN;
    PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
    SetTextSlot(SCREEN_WIDTH / 2, TITLE_SCREEN_TEXT_Y, NO_MESSAGE,
        TEXT_SLOT_LARGE_FONT, 0, 0, 0, 0, 0, 0);
    return;
  }

  /* titleChoice is moved through the array's bytes, which keeps its address in
   * a register as the original does */
  if (g_MenuRepeatButtons & U_JPAD) {
    D_801323A0.titleChoice--;
    PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
  } else if (g_MenuRepeatButtons & D_JPAD) {
    D_801323A0.titleChoice++;
    PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
  }

  if (D_801323A0.titleChoice < 0) {
    D_801323A0.titleChoice = TITLE_CHOICE_COUNT - 1;
  }

  if (D_801323A0.titleChoice >= TITLE_CHOICE_COUNT) {
    D_801323A0.titleChoice = 0;
  }

  if (D_801323A0.titleChoice == TITLE_CHOICE_NEW_GAME) {
    if (g_MenuRepeatButtons & L_JPAD) {
      D_801323A0.playerCountChoice--;
      PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
    } else if (g_MenuRepeatButtons & R_JPAD) {
      D_801323A0.playerCountChoice++;
      PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
    }
  }

  if (D_801323A0.playerCountChoice < 0) {
    D_801323A0.playerCountChoice = MAX_PLAYER_COUNT - 1;
  }

  if (D_801323A0.playerCountChoice >= MAX_PLAYER_COUNT) {
    D_801323A0.playerCountChoice = 0;
  }
}

void func_801F75EC_ovl_01(void) {
  D_801323A0.unk2 = 0;
  if (g_TakenCharacters.isCharacterTaken[0] == 1) {
    do {
      D_801323A0.unk2++;
      if (D_801323A0.unk2 > D_8022BE0C_ovl_01 - 1) {
        D_801323A0.unk2 = 0;
      }
    } while (g_TakenCharacters.isCharacterTaken[D_801323A0.unk2] == 1);
  }
}

/*
 * func_801F7660 moves the character cursor past the characters marked as taken,
 * and plays the voice of the one chosen. Nothing reads arg0.
 */
s32 func_801F7660_ovl_01(s32 arg0, s32 playerNumber) {
  if (D_8022BE28_ovl_01 != 0) {
    while (g_TakenCharacters.isCharacterTaken[D_801323A0.unk2] == 1) {
      D_801323A0.unk2++;
      if (D_801323A0.unk2 > D_8022BE0C_ovl_01 - 1) {
        D_801323A0.unk2 = 0;
      }
    }

    if (g_ControllerStates[0].pressedButtons & U_JPAD) {
      D_801323A0.unk2--;
      PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
      while (g_TakenCharacters.isCharacterTaken[D_801323A0.unk2] == 1) {
        D_801323A0.unk2--;
        if (D_801323A0.unk2 < 0) {
          D_801323A0.unk2 = D_8022BE0C_ovl_01 - 1;
        }
      }
    } else if (g_ControllerStates[0].pressedButtons & D_JPAD) {
      PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
      D_801323A0.unk2++;
      while (g_TakenCharacters.isCharacterTaken[D_801323A0.unk2] == 1) {
        D_801323A0.unk2++;
        if (D_801323A0.unk2 > D_8022BE0C_ovl_01 - 1) {
          D_801323A0.unk2 = 0;
        }
      }
    }

    if (D_801323A0.unk2 < 0) {
      D_801323A0.unk2 = D_8022BE0C_ovl_01 - 1;
      while (g_TakenCharacters.isCharacterTaken[D_801323A0.unk2] == 1) {
        D_801323A0.unk2--;
      }
    }

    if (D_801323A0.unk2 > D_8022BE0C_ovl_01 - 1) {
      D_801323A0.unk2 = 0;
      while (g_TakenCharacters.isCharacterTaken[D_801323A0.unk2] == 1) {
        D_801323A0.unk2++;
      }
    }
  } else if (D_8022BE2C_ovl_01 == 1) {
    if (GetCurrentAnimationEndFrame(0) == GetAnimationFrame(0)) {
      D_8022BE2C_ovl_01 = 0;
      if (D_801B7070 == playerNumber) {
        return 1;
      }
    }
  } else if (!(g_ControllerStates[0].buttons & Z_TRIG)) {
    if ((g_MenuPressedButtons & (A_BUTTON | R_TRIG)) &&
        g_TakenCharacters.isCharacterTaken[D_801323A0.unk2] == 0) {
      PlayCharacterSound(D_801323A0.unk2, 0, CHARACTER_SOUND_CHOSEN,
          MENU_VOLUME_SOFT, SOUND_FULL_SCALE);
      D_8022BE2C_ovl_01 = 1;
      SetAnimationSpeed(0, 1.0f);
      return 0;
    }

    if (g_MenuPressedButtons & B_BUTTON) {
      PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
      return -1;
    }
    while (g_TakenCharacters.isCharacterTaken[D_801323A0.unk2] == 1) {
      D_801323A0.unk2++;
      if (D_801323A0.unk2 > D_8022BE0C_ovl_01 - 1) {
        D_801323A0.unk2 = 0;
      }
    }

    if (g_MenuRepeatButtons & U_JPAD) {
      D_801323A0.unk2--;
      PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
      while (g_TakenCharacters.isCharacterTaken[D_801323A0.unk2] == 1) {
        D_801323A0.unk2--;
        if (D_801323A0.unk2 < 0) {
          D_801323A0.unk2 = D_8022BE0C_ovl_01 - 1;
        }
      }
    } else if (g_MenuRepeatButtons & D_JPAD) {
      PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
      D_801323A0.unk2++;
      while (g_TakenCharacters.isCharacterTaken[D_801323A0.unk2] == 1) {
        D_801323A0.unk2++;
        if (D_801323A0.unk2 > D_8022BE0C_ovl_01 - 1) {
          D_801323A0.unk2 = 0;
        }
      }
    }

    if (D_801323A0.unk2 < 0) {
      D_801323A0.unk2 = D_8022BE0C_ovl_01 - 1;
      while (g_TakenCharacters.isCharacterTaken[D_801323A0.unk2] == 1) {
        D_801323A0.unk2--;
      }
    }

    if (D_801323A0.unk2 > D_8022BE0C_ovl_01 - 1) {
      D_801323A0.unk2 = 0;
      while (g_TakenCharacters.isCharacterTaken[D_801323A0.unk2] == 1) {
        D_801323A0.unk2++;
      }
    }
  }

  return 0;
}

void func_801F7CD0_ovl_01(void) {
  Boolean isDone = IsHudAnimationDone();

  if (isDone == TRUE && func_801F7660_ovl_01(1, 2) == -1) {
    D_801B7070 = isDone;
    g_MenuScreen = MENU_SCREEN_TITLE;
    FreeAllCharacterSlots();
  }
}

/* The mode select's wheel, turned like the course select's */
#define MODE_SELECT_TURN 90
#define MODE_SELECT_SPIN_SPEED 18

/* The chosen mode's message, below the wheel */
#define MODE_TEXT_X 40
#define MODE_TEXT_Y 188
#define MODE_TEXT_SLOT 0
#define MESSAGE_FIRST_MODE 4 /* Followed by one per mode icon */
#define MESSAGE_SOLO_STROKE 0x39
#define MESSAGE_MODE_NEEDS_CONTROLLERS 0x4A

/*
 * The mode icons are double-buffered like the course select's images: the
 * chosen mode's first icon frame is read into the buffer not shown, and then
 * the buffers swap.
 */
#define LoadChosenModeIcon() \
  D_8022F8C0_ovl_01 = 0; \
  D_8022F8C5_ovl_01 = 0; \
  OpenEncodedAsset( \
      g_ModeIconAnimations[D_8022BEBC_ovl_01[g_ChosenPlayerCount - 1] \
                                            [D_801323A0.modeChoice]][0], \
      &asset); \
  ReadEncodedAsset(D_8010526C[D_8022F8C4_ovl_01 ^ 1], &asset); \
  D_8022F8C4_ovl_01 ^= 1

/*
 * Updates the mode select (MENU_SCREEN_UNK5), the wheel of the modes that
 * the chosen player count can play. A starts the chosen mode, or first
 * stops the wheel on it while it still turns; B goes back to the
 * character select, undoing the last player's character.
 */
/*
 * Doesn't match: the stroke and mini golf cases load their 1 and store
 * g_GameState.gameMode in another order, the B button's path loads D_8012F480
 * and g_CharacterGrid in another order, and the spin's slowing keeps the spin
 * in a register where the original reads it again after the step.
 */
#ifdef NON_MATCHING
void func_801F7D28_ovl_01(void) {
  EncodedAsset asset;
  Boolean isDone = IsHudAnimationDone();
  s32 direction;
  s32 step;
  s32 index;
  s16 slot;
  s32 mode;

  if (isDone != TRUE || GetWipeKind() != WIPE_NONE) {
    return;
  }

  D_800C2BD5 = 0;
  if (g_MenuPressedButtons & A_BUTTON) {
    if (D_801323A0.modeSpin != 0) {
      D_801323A0.modeSpin = 0;
      LoadChosenModeIcon();
      return;
    }

    PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
    SetTextSlot(0, 0, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, MODE_TEXT_SLOT,
        TEXT_FLAG_TYPED, 0xFF, 0xFF, 0xFF, 0xFF);
    D_8022BE84_ovl_01 = 0;
    D_8022BE88_ovl_01 = 0;
    D_8022BE89_ovl_01 = 0;
    D_8023BFD0_ovl_01 = 0;
    D_802322F0_ovl_01 = 0;
    D_802322F4_ovl_01 = 0;
    g_GameState.isMiniGolf = FALSE;
    mode = D_8022BEBC_ovl_01[g_ChosenPlayerCount - 1][D_801323A0.modeChoice];
    g_GameState.unk2C = rand();
    switch (mode) {
      case MODE_ICON_TOURNAMENT:
        PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
        StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
        g_GameState.gameMode = GAME_MODE_TOURNAMENT;
        g_MenuScreen = MENU_SCREEN_UNK6;
        D_800FF4C0 = 2;
        MAP_CAMERA_MODE_TOP_BYTE = 0;
        D_801B5634 = 0;
        func_80217B88_ovl_01(MENU_UNKE_PRESET_NONE);
        break;
      case MODE_ICON_GET_CHARACTER:
        PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
        StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
        g_GameState.gameMode = GAME_MODE_GET_CHARACTER;
        g_GameState.playerCount = 2;
        g_MenuScreen = MENU_SCREEN_UNK16;
        D_800FF4C0 = 1;
        MAP_CAMERA_MODE_TOP_BYTE = 2;
        D_801B5634 = 0;
        func_80217B88_ovl_01(MENU_UNKE_PRESET_NONE);
        for (index = 0; index != MAX_PLAYER_COUNT; index++) {
          D_8023BFDC_ovl_01[index] = 0;
        }

        /* The last setting is left as it was */
        for (index = 0; index != MENU_UNKE_COUNT - 1; index++) {
          g_GameState.betRates[index] = -1;
        }

        break;
      case MODE_ICON_STROKE:
        PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
        StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
        g_MenuScreen = MENU_SCREEN_UNK6;
        D_801323A0.holesChoice = HOLES_CHOICE_ALL;
        g_GameState.gameMode = GAME_MODE_STROKE;
        D_800FF4C0 = 1;
        MAP_CAMERA_MODE_TOP_BYTE = 1;
        if (g_ChosenPlayerCount == MAX_PLAYER_COUNT) {
          D_801B5634 = 2;
        } else {
          D_801B5634 = 1;
        }

        func_80217B88_ovl_01(MENU_UNKE_PRESET_STROKE);
        if (g_ChosenPlayerCount == 1) {
          for (index = 0; index != MAX_PLAYER_COUNT; index++) {
            D_8023BFDC_ovl_01[index] = 0;
          }

          for (index = 0; index != MENU_UNKE_COUNT - 1; index++) {
            g_GameState.betRates[index] = -1;
          }

          D_800C2BD5 = 1;
        }

        break;
      case MODE_ICON_MATCH_GAME:
        PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
        StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
        g_GameState.gameMode = GAME_MODE_MATCH_GAME;
        g_MenuScreen = MENU_SCREEN_UNK6;
        D_800FF4C0 = 1;
        MAP_CAMERA_MODE_TOP_BYTE = 1;
        D_801B5634 = 3;
        D_801323A0.holesChoice = HOLES_CHOICE_ALL;
        func_80217B88_ovl_01(MENU_UNKE_PRESET_MATCH_GAME);
        break;
      case MODE_ICON_SKINS_MATCH:
        PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
        StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
        g_GameState.gameMode = GAME_MODE_SKINS_MATCH;
        g_MenuScreen = MENU_SCREEN_UNK6;
        D_800FF4C0 = 1;
        MAP_CAMERA_MODE_TOP_BYTE = 1;
        D_801B5634 = 4;
        D_801323A0.holesChoice = HOLES_CHOICE_ALL;
        func_80217B88_ovl_01(MENU_UNKE_PRESET_SKINS_MATCH);
        break;
      case MODE_ICON_SPEED_GOLF:
        PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
        StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
        g_GameState.gameMode = GAME_MODE_SPEED_GOLF;
        g_MenuScreen = MENU_SCREEN_UNK6;
        D_800FF4C0 = 1;
        MAP_CAMERA_MODE_TOP_BYTE = 2;
        D_801B5634 = 0;
        D_801323A0.holesChoice = HOLES_CHOICE_FRONT_NINE;
        func_80217B88_ovl_01(MENU_UNKE_PRESET_NONE);
        break;
      case MODE_ICON_MINI_GOLF:
        /* Mini golf needs every player on a controller */
        for (index = 0; index != g_ChosenPlayerCount; index++) {
          if (g_PlayerControllerIndices[index] == NO_CONTROLLER_INDEX) {
            break;
          }
        }

        if (index < g_ChosenPlayerCount) {
          PlaySound(SOUND_MENU_BLOCKED, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
          return;
        }

        PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
        StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
        g_MenuScreen = MENU_SCREEN_UNK6;
        D_801323A0.holesChoice = HOLES_CHOICE_ALL;
        g_GameState.gameMode = GAME_MODE_STROKE;
        g_GameState.isMiniGolf = TRUE;
        D_800FF4C0 = 1;
        MAP_CAMERA_MODE_TOP_BYTE = 1;
        if (g_ChosenPlayerCount == MAX_PLAYER_COUNT) {
          D_801B5634 = 7;
        } else {
          D_801B5634 = 6;
        }

        func_80217B88_ovl_01(MENU_UNKE_PRESET_MINI_GOLF);
        if (g_ChosenPlayerCount == 1) {
          for (index = 0; index != MAX_PLAYER_COUNT; index++) {
            D_8023BFDC_ovl_01[index] = 0;
          }

          for (index = 0; index != MENU_UNKE_COUNT - 1; index++) {
            g_GameState.betRates[index] = -1;
          }

          D_800C2BD5 = 1;
        }

        break;
      case MODE_ICON_TRAINING:
        PlaySound(SOUND_MENU_START, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
        StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
        g_GameState.gameMode = GAME_MODE_TRAINING;
        g_GameState.holeSelection = HOLE_SELECTION_ALL;
        StartLoadingChosenHole();
        g_MenuScreen = MENU_SCREEN_UNK26;
        D_800FF4C0 = 1;
        MAP_CAMERA_MODE_TOP_BYTE = 2;
        D_801B5634 = 0;
        D_801323A0.holesChoice = HOLES_CHOICE_FRONT_NINE;
        func_80217B88_ovl_01(MENU_UNKE_PRESET_NONE);
        break;
      case MODE_ICON_UNK9:
        PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
        StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
        g_GameState.gameMode = GAME_MODE_UNK1;
        g_MenuScreen = MENU_SCREEN_UNK6;
        D_800FF4C0 = 1;
        MAP_CAMERA_MODE_TOP_BYTE = 2;
        D_801B5634 = 0;
        D_801323A0.holesChoice = HOLES_CHOICE_FRONT_NINE;
        func_80217B88_ovl_01(MENU_UNKE_PRESET_NONE);
        break;
      case MODE_ICON_RING_SHOT:
        PlaySound(SOUND_MENU_START, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
        StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
        g_GameState.gameMode = GAME_MODE_RING_SHOT;
        g_MenuScreen = MENU_SCREEN_UNK23;
        D_800FF4C0 = 1;
        MAP_CAMERA_MODE_TOP_BYTE = 2;
        D_801B5634 = 0;
        func_80217B88_ovl_01(MENU_UNKE_PRESET_NONE);
        break;
      case MODE_ICON_CLUB_SLOTS:
        /* The club slots need every player on a controller too */
        for (index = 0; index != g_ChosenPlayerCount; index++) {
          if (g_PlayerControllerIndices[index] == NO_CONTROLLER_INDEX) {
            break;
          }
        }

        if (index < g_ChosenPlayerCount) {
          PlaySound(SOUND_MENU_BLOCKED, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
          return;
        }

        PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
        StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
        g_GameState.gameMode = GAME_MODE_CLUB_SLOTS;
        g_MenuScreen = MENU_SCREEN_UNK6;
        D_800FF4C0 = 1;
        MAP_CAMERA_MODE_TOP_BYTE = 1;
        D_801B5634 = 5;
        D_801323A0.holesChoice = HOLES_CHOICE_ALL;
        func_80217B88_ovl_01(MENU_UNKE_PRESET_CLUB_SLOTS);
        break;
    }

    g_GameState.holeSelection = HOLE_SELECTION_ALL;

    /* Training's hole is already loading */
    if (g_MenuScreen != MENU_SCREEN_UNK26) {
      StopLoaderThread();
    }

    FreeAllCharacterSlots();
    D_800FE334 = 0;
    return;
  }

  if (g_MenuPressedButtons & B_BUTTON) {
    if (D_801323A0.modeSpin != 0) {
      D_801323A0.modeSpin = 0;
      LoadChosenModeIcon();
    }

    StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
    D_801323A0.modeSpin = 0;
    SetTextSlot(0, 0, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, MODE_TEXT_SLOT,
        TEXT_FLAG_TYPED, 0xFF, 0xFF, 0xFF, 0xFF);
    PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
    g_MenuScreen = MENU_SCREEN_CHARACTER_SELECT;
    D_801B7070--;
    D_800FBE08[D_801B7070] = 0;
    g_TakenCharacters.isCharacterTaken[D_8012F480[D_801B7070]] = 0;
    g_PlayerControllerIndices[D_801B7070] = 0;
    slot = g_CharacterGrid[D_801323A0.characterRows[D_801B7070]]
                          [D_801323A0.characterColumns[D_801B7070]];
    if (slot >= FIRST_IMPORTED_CHARACTER) {
      g_ImportedCharacters[slot - FIRST_IMPORTED_CHARACTER].isChosen = 0;
      g_ImportedCharacterColumns[D_801B7070] = NO_IMPORTED_COLUMN;
    }

    StopLoaderThread();
    FreeAllCharacterSlots();
    return;
  }

  if (D_801323A0.modeSpin == 0) {
    if (g_MenuRepeatButtons & L_JPAD) {
      PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
      if (--D_801323A0.modeChoice < 0) {
        D_801323A0.modeChoice = D_8022BE30_ovl_01[g_ChosenPlayerCount - 1] - 1;
      }

      D_801323A0.modeSpin = AmountPerMode(MODE_SELECT_TURN);
      if (g_ChosenPlayerCount == 1) {
        D_8022F8C5_ovl_01 = 0;
        D_8022F8C0_ovl_01 = 0;
        D_8022F8CD_ovl_01 = 0;
        D_8022F8C8_ovl_01 = 0;
      }
    } else if (g_MenuRepeatButtons & R_JPAD) {
      PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
      if (++D_801323A0.modeChoice >
          D_8022BE30_ovl_01[g_ChosenPlayerCount - 1] - 1) {
        D_801323A0.modeChoice = 0;
      }

      D_801323A0.modeSpin = AmountPerMode(-MODE_SELECT_TURN);
      if (g_ChosenPlayerCount == 1) {
        D_8022F8C5_ovl_01 = 0;
        D_8022F8C0_ovl_01 = 0;
        D_8022F8CD_ovl_01 = 0;
        D_8022F8C8_ovl_01 = 0;
      }
    }
  }

  if (Abs(D_801323A0.modeSpin) != 0) {
    step = AmountPerMode(MODE_SELECT_SPIN_SPEED);
    direction = D_801323A0.modeSpin < 0 ? -1 : 1;
    if (step == 0) {
      step = 1;
    }

    D_801323A0.modeSpin -= direction * step;

    /* Stop on reaching the mode rather than turning past it */
    if ((D_801323A0.modeSpin < 0 ? -1 : 1) != direction) {
      D_801323A0.modeSpin = 0;
    }

    if (D_801323A0.modeSpin == 0) {
      LoadChosenModeIcon();
    }
  }

  mode = D_8022BEBC_ovl_01[g_ChosenPlayerCount - 1][D_801323A0.modeChoice];
  for (index = 0; index != g_ChosenPlayerCount; index++) {
    if (g_PlayerControllerIndices[index] == NO_CONTROLLER_INDEX) {
      break;
    }
  }

  if (((mode == MODE_ICON_CLUB_SLOTS) | (mode == MODE_ICON_MINI_GOLF)) &&
      index < g_ChosenPlayerCount) {
    SetTextSlot(MODE_TEXT_X, MODE_TEXT_Y, MESSAGE_MODE_NEEDS_CONTROLLERS,
        TEXT_SLOT_LARGE_FONT, MODE_TEXT_SLOT, TEXT_FLAG_TYPED, 0xFF, 0xFF, 0xFF,
        0xFF);
  } else if (g_ChosenPlayerCount == 1 &&
      D_8022BEBC_ovl_01[0][D_801323A0.modeChoice] == MODE_ICON_STROKE) {
    SetTextSlot(MODE_TEXT_X, MODE_TEXT_Y, MESSAGE_SOLO_STROKE,
        TEXT_SLOT_LARGE_FONT, MODE_TEXT_SLOT, TEXT_FLAG_TYPED, 0xFF, 0xFF, 0xFF,
        0xFF);
  } else {
    SetTextSlot(MODE_TEXT_X, MODE_TEXT_Y, mode + MESSAGE_FIRST_MODE,
        TEXT_SLOT_LARGE_FONT, MODE_TEXT_SLOT, TEXT_FLAG_TYPED, 0xFF, 0xFF, 0xFF,
        0xFF);
  }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_801F7D28_ovl_01);
#endif

void func_801F8BAC_ovl_01(void) {
  EncodedAsset asset;
  AssetFile file;

  /* The open courses, and then those on the wheel */
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

  if (D_8022BE88_ovl_01 == MENU_PHASE_UNK2) {
    PlaySong(SONG_CONTROLS, 0);
    g_MenuScreen = MENU_SCREEN_UNK22;
    D_8022BE88_ovl_01 = 1;
    D_8022BE89_ovl_01 = 0;
    return;
  }

  if (D_8022BE89_ovl_01 != 0 || D_8022BE88_ovl_01 != 0) {
    return;
  }

  if (GetWipeKind() != WIPE_NONE && D_801B9410 == 0) {
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
    if (D_801323A0.courseSpin != 0) {
      D_801323A0.courseSpin = 0;
      OpenEncodedAsset(
          g_CourseWheelImageAssets[D_801323A0.courseChoice][0], &asset);
      ReadEncodedAsset(D_800FBDFC[D_8022BE80_ovl_01 ^ 1], &asset);
      for (index = 0; index != COURSE_FILE_COUNT; index++) {
        OpenAssetFile(
            g_CourseTrophyFiles[D_801323A0.courseChoice][index], &file);
        ReadAssetFile(D_800FBE38[index], GetAssetFileSize(&file), &file);
      }

      D_8022BE80_ovl_01 ^= 1;
      return;
    }

    if (D_801323A0.courseChoice < g_MenuCourseCount) {
      openCourseCount = GetOpenCourseCount();
      if (openCourseCount < COURSE_COUNT &&
          openCourseCount == D_801323A0.courseChoice) {
        PlaySound(SOUND_MENU_BLOCKED, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
        return;
      }

      for (index = 0; index != COURSE_SELECT_TEXT_SLOT_COUNT; index++) {
        SetTextSlot(0, 0, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, index, 0, 0xFF,
            0xFF, 0xFF, 0xFF);
      }

      D_801B9410 = 0;
      FreeAllCharacterSlots();
      LoadWipeShape(D_801323A0.courseChoice);
      StartWipe(WIPE_CLOSING);
      D_801052E8 = 0;
    }

    PlaySound(SOUND_MENU_START, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
    return;
  }

  if (g_MenuPressedButtons & B_BUTTON) {
    D_801323A0.courseSpin = 0;
    PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
    StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
    if (D_8022BEBC_ovl_01[g_ChosenPlayerCount - 1][D_801323A0.modeChoice] ==
        MODE_ICON_GET_CHARACTER) {
      g_MenuScreen = MENU_SCREEN_UNK16;
      D_801B7070--;
      g_TakenCharacters.isCharacterTaken[D_8012F480[D_801B7070]] = 0;
    } else {
      g_MenuScreen = MENU_SCREEN_UNK5;
    }

    MAP_CAMERA_MODE_TOP_BYTE = 0;
    D_800FE334 = 0;
    StopLoaderThread();
    FreeAllCharacterSlots();
    for (index = 0; index != COURSE_SELECT_TEXT_SLOT_COUNT; index++) {
      SetTextSlot(0, 0, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, index, 0, 0xFF, 0xFF,
          0xFF, 0xFF);
    }

    return;
  }

  /* isDone is still TRUE, and the C-up test compares with it */
  if (D_801323A0.courseSpin == 0) {
    if (g_MenuRepeatButtons & L_JPAD) {
      PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
      if (--D_801323A0.courseChoice < 0) {
        D_801323A0.courseChoice = courseCount - 1;
      }

      D_801323A0.courseSpin = COURSE_SELECT_TURN / courseCount;
      for (index = 0; index != COURSE_SELECT_TEXT_SLOT_COUNT; index++) {
        SetTextSlot(0, 0, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, index, 0, 0xFF,
            0xFF, 0xFF, 0xFF);
      }
    } else if (g_MenuRepeatButtons & R_JPAD) {
      PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
      if (++D_801323A0.courseChoice > courseCount - 1) {
        D_801323A0.courseChoice = 0;
      }

      D_801323A0.courseSpin = -COURSE_SELECT_TURN / courseCount;
      for (index = 0; index != COURSE_SELECT_TEXT_SLOT_COUNT; index++) {
        SetTextSlot(0, 0, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, index, 0, 0xFF,
            0xFF, 0xFF, 0xFF);
      }
    } else if ((g_MenuRepeatButtons & U_CBUTTONS) &&
        MAP_CAMERA_MODE_TOP_BYTE == isDone) {
      if (D_800C2BD5 != MAP_CAMERA_MODE_TOP_BYTE) {
        PlaySound(SOUND_UNK106, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
        D_8022BE88_ovl_01 = 1;
        D_8022BE89_ovl_01 = 1;
      } else {
        PlaySound(SOUND_MENU_BLOCKED, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
      }
    } else if ((g_MenuRepeatButtons & D_CBUTTONS) &&
        MAP_CAMERA_MODE_TOP_BYTE == 1) {
      PlaySound(SOUND_SLICE, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
      D_801323A0.holesChoice = HOLES_CHOICE_ALL;
      g_GameState.holeSelection = HOLE_SELECTION_ALL;
      StartLoadingChosenHole();
    } else if ((g_MenuRepeatButtons & L_CBUTTONS) &&
        MAP_CAMERA_MODE_TOP_BYTE == 1) {
      PlaySound(SOUND_SLICE, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
      D_801323A0.holesChoice = HOLES_CHOICE_FRONT_NINE;
      g_GameState.holeSelection = HOLE_SELECTION_FRONT_NINE;
      StartLoadingChosenHole();
    } else if ((g_MenuRepeatButtons & R_CBUTTONS) &&
        MAP_CAMERA_MODE_TOP_BYTE == 1) {
      PlaySound(SOUND_SLICE, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
      D_801323A0.holesChoice = HOLES_CHOICE_BACK_NINE;
      g_GameState.holeSelection = HOLE_SELECTION_BACK_NINE;
      StartLoadingChosenHole();
    }
  }

  if (Abs(D_801323A0.courseSpin) != 0) {
    step = COURSE_SELECT_SPIN_SPEED / courseCount;
    direction = D_801323A0.courseSpin < 0 ? -1 : 1;
    if (step == 0) {
      step = 1;
    }

    D_801323A0.courseSpin -= direction * step;

    /* Stop on reaching the course rather than turning past it */
    if ((D_801323A0.courseSpin < 0 ? -1 : 1) != direction) {
      D_801323A0.courseSpin = 0;
    }

    if (D_801323A0.courseSpin == 0) {
      OpenEncodedAsset(
          g_CourseWheelImageAssets[D_801323A0.courseChoice][0], &asset);
      ReadEncodedAsset(D_800FBDFC[D_8022BE80_ovl_01 ^ 1], &asset);
      for (index = 0; index != COURSE_FILE_COUNT; index++) {
        OpenAssetFile(
            g_CourseTrophyFiles[D_801323A0.courseChoice][index], &file);
        ReadAssetFile(D_800FBE38[index], GetAssetFileSize(&file), &file);
      }

      D_8022BE80_ovl_01 ^= 1;
    }
  }
}

/*
 * Doesn't match: the original loads the -1 and 1 that the last sign test
 * compares with before the test of the slide's sign, leaving 8 diff lines
 * (4 opcodes).
 */
#ifdef NON_MATCHING
void func_801F93D8_ovl_01(void) {
  EncodedAsset asset;
  s32 unused[4];
  Boolean isDone;
  s32 course;
  s32 direction;
  s32 step;
  void*(*buffers)[MINI_GOLF_DISC_IMAGE_COUNT];

  isDone = IsHudAnimationDone();
  if (isDone != TRUE) {
    return;
  }

  if (D_8022F8DC_ovl_01 != MINI_GOLF_TURN_NONE) {
    switch (D_8022F8DC_ovl_01) {
      case MINI_GOLF_TURN_TO_PREVIOUS:
        if (++D_8022F8D8_ovl_01 == MINI_GOLF_TURN_FRAME_DURATION) {
          D_8022F8D8_ovl_01 = 0;
          if (++D_8022F8D4_ovl_01 >= MINI_GOLF_TURN_FRAME_COUNT) {
            D_8022F8D4_ovl_01 = 0;
          }

          if (D_8022F8D4_ovl_01 >= 0 &&
              D_8022F8D4_ovl_01 <= MINI_GOLF_FACING_FRAME) {
            course = D_801323A0.unk2C - 1;
            if (course < 0) {
              course = MINI_GOLF_COURSES_PER_RECORD - 1;
            }

            OpenEncodedAsset(g_MiniGolfDiscFrameAssets[course *
                                 MINI_GOLF_DISC_IMAGE_COUNT][D_8022F8D4_ovl_01],
                &asset);
            ReadEncodedAsset(D_801049A0[D_8022F8DD_ovl_01 ^ 1][0], &asset);
            OpenEncodedAsset(
                g_MiniGolfDiscFrameAssets[(course *
                                              MINI_GOLF_DISC_IMAGE_COUNT) +
                    1][D_8022F8D4_ovl_01],
                &asset);
            ReadEncodedAsset(D_801049A0[D_8022F8DD_ovl_01 ^ 1][1], &asset);
          } else {
            course = D_801323A0.unk2C;
            buffers = D_801049A0;
            OpenEncodedAsset(g_MiniGolfDiscFrameAssets[course *
                                 MINI_GOLF_DISC_IMAGE_COUNT][D_8022F8D4_ovl_01],
                &asset);
            ReadEncodedAsset(buffers[D_8022F8DD_ovl_01 ^ 1][0], &asset);
            OpenEncodedAsset(
                g_MiniGolfDiscFrameAssets[(course *
                                              MINI_GOLF_DISC_IMAGE_COUNT) +
                    1][D_8022F8D4_ovl_01],
                &asset);
            ReadEncodedAsset(buffers[D_8022F8DD_ovl_01 ^ 1][1], &asset);
          }

          D_8022F8DD_ovl_01 ^= 1;
          if (D_8022F8D4_ovl_01 == MINI_GOLF_FACING_FRAME) {
            if (--D_801323A0.unk2C < 0) {
              D_801323A0.unk2C = MINI_GOLF_COURSES_PER_RECORD - 1;
            }

            D_8022F8DC_ovl_01 = MINI_GOLF_TURN_NONE;
          }
        }

        break;
      case MINI_GOLF_TURN_TO_NEXT:
        if (++D_8022F8D8_ovl_01 == MINI_GOLF_TURN_FRAME_DURATION) {
          D_8022F8D8_ovl_01 = 0;
          if (--D_8022F8D4_ovl_01 < 0) {
            D_8022F8D4_ovl_01 = MINI_GOLF_TURN_FRAME_COUNT - 1;
          }

          if (D_8022F8D4_ovl_01 >= MINI_GOLF_FACING_FRAME &&
              D_8022F8D4_ovl_01 < MINI_GOLF_TURN_FRAME_COUNT) {
            course = D_801323A0.unk2C + 1;
            if (course >= MINI_GOLF_COURSES_PER_RECORD) {
              course = 0;
            }

            OpenEncodedAsset(g_MiniGolfDiscFrameAssets[course *
                                 MINI_GOLF_DISC_IMAGE_COUNT][D_8022F8D4_ovl_01],
                &asset);
            ReadEncodedAsset(D_801049A0[D_8022F8DD_ovl_01 ^ 1][0], &asset);
            OpenEncodedAsset(
                g_MiniGolfDiscFrameAssets[(course *
                                              MINI_GOLF_DISC_IMAGE_COUNT) +
                    1][D_8022F8D4_ovl_01],
                &asset);
            ReadEncodedAsset(D_801049A0[D_8022F8DD_ovl_01 ^ 1][1], &asset);
          } else {
            course = D_801323A0.unk2C;
            buffers = D_801049A0;
            OpenEncodedAsset(g_MiniGolfDiscFrameAssets[course *
                                 MINI_GOLF_DISC_IMAGE_COUNT][D_8022F8D4_ovl_01],
                &asset);
            ReadEncodedAsset(buffers[D_8022F8DD_ovl_01 ^ 1][0], &asset);
            OpenEncodedAsset(
                g_MiniGolfDiscFrameAssets[(course *
                                              MINI_GOLF_DISC_IMAGE_COUNT) +
                    1][D_8022F8D4_ovl_01],
                &asset);
            ReadEncodedAsset(buffers[D_8022F8DD_ovl_01 ^ 1][1], &asset);
          }

          D_8022F8DD_ovl_01 ^= 1;
          if (D_8022F8D4_ovl_01 == MINI_GOLF_FACING_FRAME) {
            if (++D_801323A0.unk2C >= MINI_GOLF_COURSES_PER_RECORD) {
              D_801323A0.unk2C = 0;
            }

            D_8022F8DC_ovl_01 = MINI_GOLF_TURN_NONE;
          }
        }

        break;
    }

    return;
  }

  if (D_8022BE88_ovl_01 == MENU_PHASE_UNK2) {
    PlaySong(SONG_CONTROLS, 0);
    g_MenuScreen = MENU_SCREEN_UNK22;
    D_8022BE88_ovl_01 = 1;
    D_8022BE89_ovl_01 = 0;
    return;
  }

  if (D_8022BE89_ovl_01 != 0 || D_8022BE88_ovl_01 != 0) {
    return;
  }

  if (GetWipeKind() != WIPE_NONE && D_801B9410 == 0) {
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
    if (D_801323A0.unk2D != 0) {
      D_801323A0.unk2D = 0;
      return;
    }

    if (D_801323A0.unk2B < g_MenuCourseCount) {
      D_801B9410 = 0;
      FreeAllCharacterSlots();
      LoadWipeShape(D_801323A0.unk2C +
          (D_801323A0.unk2B * MINI_GOLF_COURSES_PER_RECORD) +
          MINI_GOLF_COURSE_INDEX);
      StartWipe(WIPE_CLOSING);
      D_801052E8 = 0;
    }

    PlaySound(SOUND_MENU_START, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
    return;
  }

  if (g_MenuPressedButtons & B_BUTTON) {
    StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
    D_801323A0.unk2D = 0;
    PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
    g_GameState.isMiniGolf = FALSE;
    g_MenuScreen = MENU_SCREEN_UNK5;
    MAP_CAMERA_MODE_TOP_BYTE = 0;
    D_800FE334 = 0;
    StopLoaderThread();
    FreeAllCharacterSlots();
    return;
  }

  /* isDone is still TRUE, and the C-up test compares with it */
  if (D_801323A0.unk2D == 0) {
    if (g_MenuRepeatButtons & L_JPAD) {
      PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
      if (--D_801323A0.unk2B < 0) {
        D_801323A0.unk2B = MINI_GOLF_RECORD_COUNT - 1;
      }

      D_801323A0.unk2D = MINI_GOLF_RECORD_SLIDE;
    } else if (g_MenuRepeatButtons & R_JPAD) {
      PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
      if (++D_801323A0.unk2B >= MINI_GOLF_RECORD_COUNT) {
        D_801323A0.unk2B = 0;
      }

      D_801323A0.unk2D = -MINI_GOLF_RECORD_SLIDE;
    } else if (g_MenuRepeatButtons & U_JPAD) {
      PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
      D_8022F8DC_ovl_01 = MINI_GOLF_TURN_TO_PREVIOUS;
    } else if (g_MenuRepeatButtons & D_JPAD) {
      PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
      D_8022F8DC_ovl_01 = MINI_GOLF_TURN_TO_NEXT;
    } else if ((g_MenuRepeatButtons & U_CBUTTONS) &&
        MAP_CAMERA_MODE_TOP_BYTE == isDone) {
      if (D_800C2BD5 != MAP_CAMERA_MODE_TOP_BYTE) {
        PlaySound(SOUND_UNK106, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
        D_8022BE88_ovl_01 = 1;
        D_8022BE89_ovl_01 = 1;
      } else {
        PlaySound(SOUND_MENU_BLOCKED, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
      }
    } else if ((g_MenuRepeatButtons & D_CBUTTONS) &&
        MAP_CAMERA_MODE_TOP_BYTE == 1) {
      PlaySound(SOUND_SLICE, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
      D_801323A0.holesChoice = HOLES_CHOICE_ALL;
      g_GameState.holeSelection = HOLE_SELECTION_ALL;
      StartLoadingChosenHole();
    } else if ((g_MenuRepeatButtons & L_CBUTTONS) &&
        MAP_CAMERA_MODE_TOP_BYTE == 1) {
      PlaySound(SOUND_SLICE, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
      D_801323A0.holesChoice = HOLES_CHOICE_FRONT_NINE;
      g_GameState.holeSelection = HOLE_SELECTION_FRONT_NINE;
      StartLoadingChosenHole();
    } else if ((g_MenuRepeatButtons & R_CBUTTONS) &&
        MAP_CAMERA_MODE_TOP_BYTE == 1) {
      PlaySound(SOUND_SLICE, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
      D_801323A0.holesChoice = HOLES_CHOICE_BACK_NINE;
      g_GameState.holeSelection = HOLE_SELECTION_BACK_NINE;
      StartLoadingChosenHole();
    }
  }

  if (Abs(D_801323A0.unk2D) != 0) {
    direction = D_801323A0.unk2D >= 0 ? 1 : -1;
    step = MINI_GOLF_RECORD_SLIDE_STEP;
    D_801323A0.unk2D -= direction * step;

    /* Stop on reaching the record rather than sliding past it */
    if ((D_801323A0.unk2D >= 0 ? 1 : -1) != direction) {
      D_801323A0.unk2D = 0;
    }
  }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_801F93D8_ovl_01);
#endif

void func_801F9BCC_ovl_01(void) {
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

void func_801F9C58_ovl_01(void) {
  Boolean isDone = IsHudAnimationDone();

  if (isDone == TRUE) {
    if (g_MenuPressedButtons & A_BUTTON) {
      switch (D_801323A0.mainChoice) {
        case MAIN_CHOICE_CLUBHOUSE:
          g_MenuScreen = MENU_SCREEN_CLUBHOUSE;
          break;
        case MAIN_CHOICE_CLUB_TOURNAMENT:
          StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
          func_80228B60_ovl_01();
          g_MenuScreen = MENU_SCREEN_CLUB_TOURNAMENT;
          break;
        case MAIN_CHOICE_HELP:
          StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
          g_MenuScreen = MENU_SCREEN_HELP;
          if (IsGameFlagSet(GAME_FLAG_UNK56)) {
            g_GameState.gameMode = GAME_MODE_GET_CHARACTER;
            g_GameState.playerCount = 2;
            D_800FF4C0 = 1;
            MAP_CAMERA_MODE_TOP_BYTE = 2;
            D_801B5634 = 0;
            func_80217B88_ovl_01(MENU_UNKE_PRESET_NONE);
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
      D_801323A0.mainChoice--;
    } else if (g_MenuRepeatButtons & D_JPAD) {
      PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
      D_801323A0.mainChoice++;
    }

    if (D_801323A0.mainChoice >= MAIN_CHOICE_COUNT) {
      D_801323A0.mainChoice = 0;
    }

    if (D_801323A0.mainChoice < 0) {
      D_801323A0.mainChoice = MAIN_CHOICE_COUNT - 1;
    }
  }
}

void func_801F9E38_ovl_01(void) {
  Boolean isDone = IsHudAnimationDone();

  if (isDone == TRUE) {
    if (g_MenuPressedButtons & A_BUTTON) {
      switch (D_801323A0.mainChoiceWithoutTransmit) {
        case MAIN_CHOICE_CLUBHOUSE:
          g_MenuScreen = MENU_SCREEN_CLUBHOUSE;
          break;
        case MAIN_CHOICE_HELP:
          StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
          g_MenuScreen = MENU_SCREEN_HELP;
          if (IsGameFlagSet(GAME_FLAG_UNK56)) {
            g_GameState.gameMode = GAME_MODE_GET_CHARACTER;
            g_GameState.playerCount = 2;
            D_800FF4C0 = 1;
            MAP_CAMERA_MODE_TOP_BYTE = 2;
            D_801B5634 = 0;
            func_80217B88_ovl_01(MENU_UNKE_PRESET_NONE);
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
      D_801323A0.mainChoiceWithoutTransmit--;
    } else if (g_MenuRepeatButtons & D_JPAD) {
      PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
      D_801323A0.mainChoiceWithoutTransmit++;
    }

    if (D_801323A0.mainChoiceWithoutTransmit >=
        MAIN_CHOICE_WITHOUT_TRANSMIT_COUNT) {
      D_801323A0.mainChoiceWithoutTransmit = 0;
    }

    if (D_801323A0.mainChoiceWithoutTransmit < 0) {
      D_801323A0.mainChoiceWithoutTransmit =
          MAIN_CHOICE_WITHOUT_TRANSMIT_COUNT - 1;
    }
  }
}

void func_801F9FF8_ovl_01(void) {
  Boolean isDone = IsHudAnimationDone();

  if (isDone == TRUE) {
    if (g_MenuPressedButtons & A_BUTTON) {
      switch (D_8022BF1C_ovl_01[D_801323A0.clubhouseChoice]) {
        case CLUBHOUSE_ROOM_COURSE_STATS:
          StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
          g_MenuScreen = MENU_SCREEN_COURSE_STATS;
          break;
        case CLUBHOUSE_ROOM_UNK12:
          StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
          g_MenuScreen = MENU_SCREEN_UNK12;
          break;
        case CLUBHOUSE_ROOM_UNK13:
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
      if (D_801323A0.isTransmitShown == isDone) {
        g_MenuScreen = MENU_SCREEN_MAIN;
      } else {
        g_MenuScreen = MENU_SCREEN_MAIN_WITHOUT_TRANSMIT;
      }

      PlaySong(SONG_TITLE, 0);
      return;
    }

    if (g_MenuRepeatButtons & U_JPAD) {
      PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
      D_801323A0.clubhouseChoice--;
    } else if (g_MenuRepeatButtons & D_JPAD) {
      PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
      D_801323A0.clubhouseChoice++;
    }

    if (D_801323A0.clubhouseChoice >= CLUBHOUSE_CHOICE_COUNT) {
      D_801323A0.clubhouseChoice = 0;
    }

    if (D_801323A0.clubhouseChoice < 0) {
      D_801323A0.clubhouseChoice = CLUBHOUSE_CHOICE_COUNT - 1;
    }
  }
}

void func_801FA1B8_ovl_01(void) {
  if (IsHudAnimationDone() == TRUE && (g_MenuPressedButtons & B_BUTTON)) {
    PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
    g_MenuScreen = MENU_SCREEN_MAIN;
  }
}

#define OPTIONS_TEXT_SLOT_COUNT 11
#define ERASE_DECLINED_SOUND_CHANNEL 13
/* MenuMemory.eraseChoice values, which left and right toggle */
#define ERASE_CHOICE_YES 0
#define ERASE_CHOICE_NO 1
#define ERASE_CHOICE_COUNT 2
/* The sound test's limits; C left and right step its numbers by ten */
#define SOUND_TEST_COARSE_STEP 10
#define SOUND_TEST_LAST_SOUND 0x214
#define SOUND_TEST_MAX_REVERB 0x7F
#define SOUND_TEST_MAX_TEMPO 0xFF
#define SOUND_TEST_FREQUENCY_FINE_STEP 0.1f
#define SOUND_TEST_FREQUENCY_COARSE_STEP 1.0f
#define SOUND_TEST_FREQUENCY_LIMIT 12.0f

/*
 * Handles the options screen's buttons. Up and down choose an option, left
 * and right toggle it, and B saves the options and returns to the main menu.
 * A on the erase choice opens a yes or no window, which erases the save data
 * on yes. With game flag 0x55 set, the choices go on to a sound test whose
 * song, sound and their settings left, right and the C buttons change and A
 * plays.
 */
void func_801FA20C_ovl_01(void) {
  SaveData* save = GetSaveData();
  MenuMemory* memory = func_8005AF74();
  s32 closedCheck;
  s32 slot;
  u8* optionBits;

  if (IsHudAnimationDone() != TRUE) {
    return;
  }

  if (f_IsEraseWindowUp) {
    closedCheck = IsMenuWindowClosed();
    for (slot = 0; slot != OPTIONS_TEXT_SLOT_COUNT; slot++) {
      SetTextSlot(0, 0, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, slot, 0, 0xFF, 0xFF,
          0xFF, 0xFF);
    }

    if (closedCheck == MENU_WINDOW_CHECK_MOVING) {
      return;
    }

    if (closedCheck == MENU_WINDOW_CHECK_YES) {
      f_IsEraseWindowUp = FALSE;
      for (slot = 0; slot != OPTIONS_TEXT_SLOT_COUNT; slot++) {
        SetTextSlot(0, 0, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, slot, 0, 0xFF, 0xFF,
            0xFF, 0xFF);
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
      } else {
        PlaySound(SOUND_READY, ERASE_DECLINED_SOUND_CHANNEL, MENU_VOLUME_SOFT);
      }

      CloseMenuWindow();
      for (slot = 0; slot != OPTIONS_TEXT_SLOT_COUNT; slot++) {
        SetTextSlot(0, 0, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, slot, 0, 0xFF, 0xFF,
            0xFF, 0xFF);
      }
    } else if (g_MenuPressedButtons & B_BUTTON) {
      PlaySound(SOUND_READY, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
      CloseMenuWindow();
      for (slot = 0; slot != OPTIONS_TEXT_SLOT_COUNT; slot++) {
        SetTextSlot(0, 0, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, slot, 0, 0xFF, 0xFF,
            0xFF, 0xFF);
      }
    } else if (g_MenuRepeatButtons & L_JPAD) {
      PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
      memory->eraseChoice--;
      if (memory->eraseChoice < 0) {
        memory->eraseChoice = ERASE_CHOICE_COUNT - 1;
      }
    } else if (g_MenuRepeatButtons & R_JPAD) {
      PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
      memory->eraseChoice++;
      if (memory->eraseChoice >= ERASE_CHOICE_COUNT) {
        memory->eraseChoice = 0;
      }
    }
  } else if (g_MenuPressedButtons & A_BUTTON) {
    switch (D_801323A0.optionsChoice) {
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
        } else {
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
  } else if (g_MenuPressedButtons & B_BUTTON) {
    PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
    TransferSaveData(save, TRUE);
    if (D_801323A0.isTransmitShown) {
      g_MenuScreen = MENU_SCREEN_MAIN;
    } else {
      g_MenuScreen = MENU_SCREEN_MAIN_WITHOUT_TRANSMIT;
    }

    PlaySong(SONG_TITLE, 0);
  } else {
    if (g_MenuRepeatButtons & U_JPAD) {
      D_801323A0.optionsChoice--;
      PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
    } else if (g_MenuRepeatButtons & D_JPAD) {
      D_801323A0.optionsChoice++;
      PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
    }

    if (IsGameFlagSet(GAME_FLAG_UNK55)) {
      if (D_801323A0.optionsChoice >= OPTIONS_SOUND_TEST_CHOICE_COUNT) {
        D_801323A0.optionsChoice = 0;
      }

      if (D_801323A0.optionsChoice < 0) {
        D_801323A0.optionsChoice = OPTIONS_SOUND_TEST_CHOICE_COUNT - 1;
      }
    } else {
      if (D_801323A0.optionsChoice >= OPTIONS_CHOICE_COUNT) {
        D_801323A0.optionsChoice = 0;
      }

      if (D_801323A0.optionsChoice < 0) {
        D_801323A0.optionsChoice = OPTIONS_CHOICE_COUNT - 1;
      }
    }

    switch (D_801323A0.optionsChoice) {
      case OPTIONS_CHOICE_MUSIC:
        if (g_MenuRepeatButtons & (L_JPAD | R_JPAD)) {
          PlaySound(
              SOUND_OPTION_TOGGLED, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
          optionBits = &D_801323A0.optionBits;
          *optionBits ^= OPTION_BIT_MUSIC_OFF;
        }

        break;
      case OPTIONS_CHOICE_MONO:
        if (g_MenuRepeatButtons & (L_JPAD | R_JPAD)) {
          PlaySound(
              SOUND_OPTION_TOGGLED, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
          optionBits = &D_801323A0.optionBits;
          *optionBits ^= OPTION_BIT_MONO;
          g_SoundOptionBits = *optionBits;
          if (*optionBits & OPTION_BIT_MONO) {
            MusSetStereo(FALSE);
          } else {
            MusSetStereo(TRUE);
          }
        }

        break;
      case OPTIONS_CHOICE_CAMERA:
        if (g_MenuRepeatButtons & (L_JPAD | R_JPAD)) {
          PlaySound(
              SOUND_OPTION_TOGGLED, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
          optionBits = &D_801323A0.optionBits;
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

        if (f_SoundTestSongReverb > SOUND_TEST_MAX_REVERB) {
          f_SoundTestSongReverb = SOUND_TEST_MAX_REVERB;
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
            f_SoundTestSongTempo <= SOUND_TEST_MAX_TEMPO) {
          f_SoundTestSongTempo++;
        }

        if ((g_MenuRepeatButtons & L_CBUTTONS) && f_SoundTestSongTempo > 0) {
          f_SoundTestSongTempo -= SOUND_TEST_COARSE_STEP;
        }

        if ((g_MenuRepeatButtons & R_CBUTTONS) &&
            f_SoundTestSongTempo <= SOUND_TEST_MAX_TEMPO) {
          f_SoundTestSongTempo += SOUND_TEST_COARSE_STEP;
        }

        if (f_SoundTestSongTempo < 0) {
          f_SoundTestSongTempo = 0;
        }

        if (f_SoundTestSongTempo > SOUND_TEST_MAX_TEMPO) {
          f_SoundTestSongTempo = SOUND_TEST_MAX_TEMPO;
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

        if (f_SoundTestSoundReverb > SOUND_TEST_MAX_REVERB) {
          f_SoundTestSoundReverb = SOUND_TEST_MAX_REVERB;
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

void func_801FADBC_ovl_01(void) {
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
    if (D_801323A0.modeChoice == MENU_SCREEN_UNK5) {
      g_MenuScreen = D_801323A0.modeChoice;
    } else {
      g_MenuScreen = MENU_SCREEN_UNK6;
    }

    return;
  }

  if ((g_MenuRepeatButtons & U_JPAD) && D_801323A0.unk19 > 0) {
    PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
    D_801323A0.unk19--;
  } else if ((g_MenuRepeatButtons & D_JPAD) &&
      D_801323A0.unk19 < CADDY_COUNT - 1) {
    PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
    D_801323A0.unk19++;
  }
}

void LeaveMenuWithWipe(void) {
  if (GetWipeKind() != WIPE_NONE && D_801B9410 == 0) {
    if (GetWipeProgress() == WIPE_CLOSED_PROGRESS) {
      g_MenuScreen = MENU_SCREEN_EXIT;
    }
  } else {
    LoadWipeShape(0);
    StartWipe(WIPE_CLOSING);
    D_801052E8 = 0;
    D_801B9410 = 0;
  }
}

void LeaveMenuWithCourseWipe(void) {
  if (GetWipeKind() != WIPE_NONE && D_801B9410 == 0) {
    if (GetWipeProgress() == WIPE_CLOSED_PROGRESS) {
      g_MenuScreen = MENU_SCREEN_EXIT;
    }
  } else {
    LoadWipeShape(D_801323A0.courseIndex);
    StartWipe(WIPE_CLOSING);
    D_801052E8 = 0;
    D_801B9410 = 0;
  }
}

void func_801FAFEC_ovl_01(void) {
  GetSaveData();
  if (g_MenuPressedButtons & A_BUTTON) {
    PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
    g_MenuScreen = MENU_SCREEN_EXIT;
    D_800FE334 = 0;
    g_GameState.replayKind = REPLAY_KIND_RING_SHOT;
    return;
  }

  if (g_MenuPressedButtons & B_BUTTON) {
    PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
    if (D_801323A0.isTransmitShown) {
      g_MenuScreen = MENU_SCREEN_MAIN;
    } else {
      g_MenuScreen = MENU_SCREEN_MAIN_WITHOUT_TRANSMIT;
    }

    return;
  }

  if (g_MenuRepeatButtons & U_JPAD) {
    D_8022BE18_ovl_01--;
    PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
  } else if (g_MenuRepeatButtons & D_JPAD) {
    D_8022BE18_ovl_01++;
    PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
  }

  if (D_8022BE18_ovl_01 < 0) {
    D_8022BE18_ovl_01 = UNUSED_LIST_CHOICE_COUNT - 1;
  }

  if (D_8022BE18_ovl_01 >= UNUSED_LIST_CHOICE_COUNT) {
    D_8022BE18_ovl_01 = 0;
  }
}

void func_801FB11C_ovl_01(void) {
  if (g_MenuPressedButtons & B_BUTTON) {
    PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
    g_MenuScreen = MENU_SCREEN_MAIN;
  }
}

void func_801FB15C_ovl_01(void) {
  if (g_MenuPressedButtons & A_BUTTON) {
    PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
    g_MenuScreen = MENU_SCREEN_TITLE;
  }
}

void DrawMenuScreen(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;

  g_ModuleRdpRequestFrames[MODULE_MENU] = g_FrameCount;
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_NOOP, G_RM_NOOP2);
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_FILL);
  gDPSetFillColor(
      gfx++, GPACK_RGBA5551(0, 0, 0, 1) << 16 | GPACK_RGBA5551(0, 0, 0, 1));
  gDPFillRectangle(gfx++, SCREEN_BORDER, SCREEN_BORDER,
      SCREEN_WIDTH - SCREEN_BORDER - 1, SCREEN_HEIGHT - SCREEN_BORDER - 1);
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
  BeginShadowedText(&gfx);
  SetTextColor(&gfx, 255, 255, 255);
  f_MenuBobPhase += MENU_BOB_PHASE_STEP;
  f_MenuBobOffset = sinf(f_MenuBobPhase) * MENU_BOB_HEIGHT;
  switch (g_MenuScreen) {
    case MENU_SCREEN_TITLE:
      SetTextSlot(0, 0, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, 0, TEXT_FLAG_TYPED,
          255, 255, 255, 255);
      func_801FDA40_ovl_01(&gfx);
      break;
    case MENU_SCREEN_CHARACTER_SELECT:
    case MENU_SCREEN_UNK24:
      func_8020B590_ovl_01(&gfx, 0);
      break;
    case MENU_SCREEN_UNK16:
      func_8020B590_ovl_01(&gfx, 1);
      break;
    case MENU_SCREEN_SAVED_GAMES:
      func_802230C0_ovl_01(&gfx);
      break;
    case MENU_SCREEN_MAIN:
      func_80205038_ovl_01(&gfx);
      break;
    case MENU_SCREEN_MAIN_WITHOUT_TRANSMIT:
      func_802058B8_ovl_01(&gfx);
      break;
    case MENU_SCREEN_CLUB_TOURNAMENT:
      func_80229CB0_ovl_01(&gfx);
      break;
    case MENU_SCREEN_CLUBHOUSE:
      func_80206180_ovl_01(&gfx);
      break;
    case MENU_SCREEN_OPTIONS:
      func_80206A68_ovl_01(&gfx);
      break;
    case MENU_SCREEN_HELP:
      if (IsGameFlagSet(GAME_FLAG_UNK56)) {
        func_80201574_ovl_01(&gfx);
      } else {
        DrawHelpBoard(&gfx);
      }

      break;
    case MENU_SCREEN_UNK8:
      func_80207D08_ovl_01(&gfx);
      break;
    case MENU_SCREEN_UNK6:
    case MENU_SCREEN_UNK20:
    case MENU_SCREEN_UNK22:
      func_80202000_ovl_01(&gfx);
      break;
    case MENU_SCREEN_UNK18:
      func_80207E18_ovl_01(&gfx);
      break;
    case MENU_SCREEN_UNK12:
      func_8021D3AC_ovl_01(&gfx);
      break;
    case MENU_SCREEN_RING_SHOT_RECORDS:
      func_8021FC5C_ovl_01(&gfx);
      break;
    case MENU_SCREEN_COURSE_STATS:
      func_8020E898_ovl_01(&gfx);
      break;
    case MENU_SCREEN_BEST_TIMES:
      func_80213748_ovl_01(&gfx);
      break;
    case MENU_SCREEN_BIRDIE_BADGES:
      DrawBirdieBadgeLabel(&gfx);
      break;
    case MENU_SCREEN_CHARACTER_TABLE:
      func_80215DC4_ovl_01(&gfx);
      break;
    case MENU_SCREEN_UNK5:
    case MENU_SCREEN_UNK23:
    case MENU_SCREEN_UNK26:
      func_801FE4D0_ovl_01(&gfx);
      break;
  }

  BeginShadowedText(&gfx);
  SetTextColor(&gfx, 255, 255, 255);
  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}

/*
 * On screens 6, 20 and 22 the backdrop shows only its first strips, clamped,
 * and the last strip's rows repeat its bottom row down to this row
 */
#define CLAMPED_BACKDROP_STRIP_COUNT 4
#define CLAMPED_BACKDROP_LAST_TOP \
  (BACKDROP_TOP + \
      ((CLAMPED_BACKDROP_STRIP_COUNT - 1) * BACKDROP_STRIP_ROW_COUNT * \
          BACKDROP_SCALE))
#define CLAMPED_BACKDROP_BOTTOM 162

/*
 * The 256 texel wide board in D_800FF420's buffer, each strip of rows
 * stretched to the strips' spacing
 */
#define BOARD_WIDTH 256
#define BOARD_STRIP_HEIGHT 8
#define BOARD_STRIP_COUNT 20
#define BOARD_STRIP_SPACING 9
#define BOARD_LEFT 32
#define BOARD_TOP 44
/* MENU_LAYER_TALL_NARROW_BOARD reads the buffer as NARROW_BOARD_WIDTH texels
 * wide */
#define TALL_NARROW_BOARD_LEFT 16

/*
 * On some screens (IsBannerBobScreen) the banner bobs with the menu's items;
 * the game adds the first and takes the second, so it sits 4 to 14 rows lower
 */
#define BANNER_BOB_LOWERING 13.0f
#define BANNER_BOB_RAISING 4.0f
/* Whether the banner bobs on a menu screen */
#define IsBannerBobScreen(screen) \
  ((screen) == MENU_SCREEN_TITLE || (screen) == MENU_SCREEN_OPTIONS || \
      (screen) == MENU_SCREEN_MAIN || \
      (screen) == MENU_SCREEN_MAIN_WITHOUT_TRANSMIT || \
      (screen) == MENU_SCREEN_CLUBHOUSE)

/* Doesn't match: the game keeps 0xFD500000 in a register and spills
 * 0x073DB06C where this spills the other way, and it reads g_MenuScreen for
 * MENU_LAYER_UNK20 where this drops the read */
/* Draws the chosen layers of the menu's backdrop: the course backdrop, the
 * board and the banner */
#ifdef NON_MATCHING
void func_801FB54C_ovl_01(Gfx** displayListHead, s32 layers) {
  Gfx* gfx = *displayListHead;
  s32 stripIndex;
  s32 shownRowCount;
  s32 stripTop;
  s32 bobOffset;
  u8* image;

  /* The menu memory goes unused */
  func_8005AF74();

  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPLoadTLUT_pal256(
      gfx++, TextureAddress(ImageBody(g_MenuCourseSceneryAsset)));
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);

  if (layers & MENU_LAYER_SCENERY) {
    if (g_MenuScreen == MENU_SCREEN_UNK6 || g_MenuScreen == MENU_SCREEN_UNK22 ||
        g_MenuScreen == MENU_SCREEN_UNK20) {
      for (stripIndex = 0; stripIndex != CLAMPED_BACKDROP_STRIP_COUNT;
          stripIndex++) {
        if (stripIndex == 0) {
          gDPLoadTextureBlock(gfx++,
              TextureAddress(LargeImageTexels(g_MenuCourseSceneryAsset)),
              G_IM_FMT_CI, G_IM_SIZ_8b, BACKDROP_WIDTH,
              BACKDROP_STRIP_ROW_COUNT + BACKDROP_SEAM_ROW_COUNT, 0,
              G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
              G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
          gDPPipeSync(gfx++);

          gSPScisTextureRectangle(gfx++, QuarterPixels(BACKDROP_LEFT),
              QuarterPixels(BACKDROP_FIRST_TOP),
              QuarterPixels(BACKDROP_LEFT + (BACKDROP_WIDTH * BACKDROP_SCALE)),
              QuarterPixels(BACKDROP_FIRST_TOP +
                  ((BACKDROP_STRIP_ROW_COUNT + 1) * BACKDROP_SCALE)),
              G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE / BACKDROP_SCALE,
              TEXEL_STEP_ONE / BACKDROP_SCALE);
        } else {
          gDPPipeSync(gfx++);
          gDPLoadTextureBlock(gfx++,
              TextureAddress(LargeImageTexels(g_MenuCourseSceneryAsset) +
                  (stripIndex * BACKDROP_WIDTH * BACKDROP_STRIP_ROW_COUNT)),
              G_IM_FMT_CI, G_IM_SIZ_8b, BACKDROP_WIDTH,
              BACKDROP_STRIP_ROW_COUNT + BACKDROP_SEAM_ROW_COUNT, 0,
              G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP,
              G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
          gDPPipeSync(gfx++);

          if (stripIndex == CLAMPED_BACKDROP_STRIP_COUNT - 1) {
            gSPScisTextureRectangle(gfx++, QuarterPixels(BACKDROP_LEFT),
                QuarterPixels(CLAMPED_BACKDROP_LAST_TOP),
                QuarterPixels(
                    BACKDROP_LEFT + (BACKDROP_WIDTH * BACKDROP_SCALE)),
                QuarterPixels(CLAMPED_BACKDROP_BOTTOM), G_TX_RENDERTILE, 0,
                BACKDROP_SECOND_ROW_START, TEXEL_STEP_ONE / BACKDROP_SCALE,
                TEXEL_STEP_ONE / BACKDROP_SCALE);
          } else {
            stripTop = BACKDROP_TOP +
                (stripIndex * BACKDROP_STRIP_ROW_COUNT * BACKDROP_SCALE);
            gSPScisTextureRectangle(gfx++, QuarterPixels(BACKDROP_LEFT),
                QuarterPixels(stripTop),
                QuarterPixels(
                    BACKDROP_LEFT + (BACKDROP_WIDTH * BACKDROP_SCALE)),
                QuarterPixels(
                    stripTop + (BACKDROP_STRIP_ROW_COUNT * BACKDROP_SCALE)),
                G_TX_RENDERTILE, 0, BACKDROP_SECOND_ROW_START,
                TEXEL_STEP_ONE / BACKDROP_SCALE,
                TEXEL_STEP_ONE / BACKDROP_SCALE);
          }
        }
      }
    } else {
      for (stripIndex = 0; stripIndex != BACKDROP_STRIP_COUNT; stripIndex++) {
        /* The image ends in a strip of one row */
        shownRowCount = (stripIndex != BACKDROP_STRIP_COUNT - 1)
            ? BACKDROP_STRIP_ROW_COUNT
            : 1;

        if (stripIndex == 0) {
          /* The first strip has no row above it, so it shows its first row
           * too */
          gDPLoadTextureBlock(gfx++,
              TextureAddress(LargeImageTexels(g_MenuCourseSceneryAsset)),
              G_IM_FMT_CI, G_IM_SIZ_8b, BACKDROP_WIDTH,
              BACKDROP_STRIP_ROW_COUNT + BACKDROP_SEAM_ROW_COUNT, 0,
              G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
              G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
          gDPPipeSync(gfx++);

          gSPScisTextureRectangle(gfx++, QuarterPixels(BACKDROP_LEFT),
              QuarterPixels(BACKDROP_FIRST_TOP),
              QuarterPixels(BACKDROP_LEFT + (BACKDROP_WIDTH * BACKDROP_SCALE)),
              QuarterPixels(BACKDROP_FIRST_TOP +
                  ((BACKDROP_STRIP_ROW_COUNT + 1) * BACKDROP_SCALE)),
              G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE / BACKDROP_SCALE,
              TEXEL_STEP_ONE / BACKDROP_SCALE);
        } else {
          gDPPipeSync(gfx++);
          gDPLoadTextureBlock(gfx++,
              TextureAddress(LargeImageTexels(g_MenuCourseSceneryAsset) +
                  (stripIndex * BACKDROP_WIDTH * BACKDROP_STRIP_ROW_COUNT)),
              G_IM_FMT_CI, G_IM_SIZ_8b, BACKDROP_WIDTH,
              shownRowCount + BACKDROP_SEAM_ROW_COUNT, 0,
              G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
              G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
          gDPPipeSync(gfx++);

          stripTop = BACKDROP_TOP +
              (stripIndex * BACKDROP_STRIP_ROW_COUNT * BACKDROP_SCALE);
          gSPScisTextureRectangle(gfx++, QuarterPixels(BACKDROP_LEFT),
              QuarterPixels(stripTop),
              QuarterPixels(BACKDROP_LEFT + (BACKDROP_WIDTH * BACKDROP_SCALE)),
              QuarterPixels(BACKDROP_TOP +
                  (stripIndex * BACKDROP_STRIP_ROW_COUNT * BACKDROP_SCALE) +
                  (shownRowCount * BACKDROP_SCALE)),
              G_TX_RENDERTILE, 0, BACKDROP_SECOND_ROW_START,
              TEXEL_STEP_ONE / BACKDROP_SCALE, TEXEL_STEP_ONE / BACKDROP_SCALE);
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
        gfx++, TextureAddress(ImageBody(D_800FF420[D_8022BE92_ovl_01])));

    for (stripIndex = 0; stripIndex != BOARD_STRIP_COUNT; stripIndex++) {
      gDPLoadTextureBlock(gfx++,
          TextureAddress(LargeImageTexels(D_800FF420[D_8022BE92_ovl_01]) +
              (stripIndex * BOARD_WIDTH * BOARD_STRIP_HEIGHT)),
          G_IM_FMT_CI, G_IM_SIZ_8b, BOARD_WIDTH, BOARD_STRIP_HEIGHT, 0,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      gDPPipeSync(gfx++);

      stripTop = BOARD_TOP + (stripIndex * BOARD_STRIP_SPACING);
      gSPScisTextureRectangle(gfx++, QuarterPixels(BOARD_LEFT),
          QuarterPixels(stripTop), QuarterPixels(BOARD_LEFT + BOARD_WIDTH),
          QuarterPixels(stripTop + BOARD_STRIP_SPACING), G_TX_RENDERTILE, 0, 0,
          TEXEL_STEP_ONE, (BOARD_STRIP_HEIGHT << 10) / BOARD_STRIP_SPACING);
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
        gfx++, TextureAddress(ImageBody(D_800FF420[D_8022BE92_ovl_01])));

    for (stripIndex = 0; stripIndex != NARROW_BOARD_STRIP_COUNT; stripIndex++) {
      gDPLoadTextureBlock(gfx++,
          TextureAddress(LargeImageTexels(D_800FF420[D_8022BE92_ovl_01]) +
              (stripIndex * NARROW_BOARD_WIDTH * NARROW_BOARD_STRIP_HEIGHT)),
          G_IM_FMT_CI, G_IM_SIZ_8b, NARROW_BOARD_WIDTH,
          NARROW_BOARD_STRIP_HEIGHT, 0, G_TX_NOMIRROR | G_TX_WRAP,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
          G_TX_NOLOD);
      gDPPipeSync(gfx++);

      stripTop = BOARD_TOP + (stripIndex * BOARD_STRIP_SPACING);
      gSPScisTextureRectangle(gfx++, QuarterPixels(TALL_NARROW_BOARD_LEFT),
          QuarterPixels(stripTop),
          QuarterPixels(TALL_NARROW_BOARD_LEFT + NARROW_BOARD_WIDTH),
          QuarterPixels(stripTop + BOARD_STRIP_SPACING), G_TX_RENDERTILE, 0, 0,
          TEXEL_STEP_ONE,
          (NARROW_BOARD_STRIP_HEIGHT << 10) / BOARD_STRIP_SPACING);
      gDPPipeSync(gfx++);
    }
  }

  bobOffset = 0;
  if (IsBannerBobScreen(g_MenuScreen)) {
    bobOffset = (f_MenuBobOffset + BANNER_BOB_LOWERING) - BANNER_BOB_RAISING;
  }

  if (layers & MENU_LAYER_BANNER) {
    image = D_801052BC[D_8022BE8C_ovl_01];
    gDPLoadTLUT_pal256(gfx++, TextureAddress(ImageBody(image)));

    for (stripIndex = 0; stripIndex != MENU_BANNER_STRIP_COUNT; stripIndex++) {
      gDPLoadTextureBlock(gfx++,
          TextureAddress(LargeImageTexels(image) +
              (stripIndex * MENU_BANNER_WIDTH * MENU_BANNER_STRIP_HEIGHT)),
          G_IM_FMT_CI, G_IM_SIZ_8b, MENU_BANNER_WIDTH, MENU_BANNER_STRIP_HEIGHT,
          0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      gDPPipeSync(gfx++);

      stripTop = bobOffset + (stripIndex * MENU_BANNER_STRIP_HEIGHT);
      gSPScisTextureRectangle(gfx++, QuarterPixels(MENU_BANNER_LEFT),
          QuarterPixels(stripTop + MENU_BANNER_TOP),
          QuarterPixels(MENU_BANNER_LEFT + MENU_BANNER_WIDTH),
          QuarterPixels(stripTop + MENU_BANNER_TOP + MENU_BANNER_STRIP_HEIGHT),
          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
      gDPPipeSync(gfx++);
    }
  }

  /* The game reads g_MenuScreen for this layer but draws nothing */
  if (layers & MENU_LAYER_UNK20) {
    switch (g_MenuScreen) {}
  }

  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_801FB54C_ovl_01);
#endif

void func_801FC60C_ovl_01(Gfx** displayListHead, s32 layers) {
  Gfx* gfx = *displayListHead;

  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_NONE);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  if (layers & MENU_LAYER_SCENERY) {
    func_801FC754_ovl_01(&gfx);
  }

  if (layers & MENU_LAYER_BOARD) {
    func_801FCB74_ovl_01(&gfx);
  }

  if (layers & MENU_LAYER_LARGE_BANNER) {
    func_801FD4F0_ovl_01(&gfx);
  }

  if (layers & MENU_LAYER_BANNER) {
    func_801FD7B0_ovl_01(&gfx);
  }

  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}

void func_801FC754_ovl_01(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 stripIndex;
  s32 shownRowCount;
  s32 stripTop;

  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPLoadTLUT_pal256(
      gfx++, TextureAddress(ImageBody(g_MenuCourseSceneryAsset)));
  for (stripIndex = 0; stripIndex != BACKDROP_STRIP_COUNT; stripIndex++) {
    /* The image ends in a strip of one row */
    shownRowCount =
        (stripIndex != BACKDROP_STRIP_COUNT - 1) ? BACKDROP_STRIP_ROW_COUNT : 1;
    if (stripIndex == 0) {
      /* The first strip has no row above it, so it shows its first row too */
      gDPLoadTextureBlock(gfx++,
          TextureAddress(LargeImageTexels(g_MenuCourseSceneryAsset)),
          G_IM_FMT_CI, G_IM_SIZ_8b, BACKDROP_WIDTH,
          BACKDROP_STRIP_ROW_COUNT + BACKDROP_SEAM_ROW_COUNT, 0,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      gDPPipeSync(gfx++);
      gSPScisTextureRectangle(gfx++, QuarterPixels(BACKDROP_LEFT),
          QuarterPixels(BACKDROP_FIRST_TOP),
          QuarterPixels(BACKDROP_LEFT + (BACKDROP_WIDTH * BACKDROP_SCALE)),
          QuarterPixels(BACKDROP_FIRST_TOP +
              ((BACKDROP_STRIP_ROW_COUNT + 1) * BACKDROP_SCALE)),
          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE / BACKDROP_SCALE,
          TEXEL_STEP_ONE / BACKDROP_SCALE);
    } else {
      gDPPipeSync(gfx++);
      gDPLoadTextureBlock(gfx++,
          TextureAddress(LargeImageTexels(g_MenuCourseSceneryAsset) +
              (stripIndex * BACKDROP_WIDTH * BACKDROP_STRIP_ROW_COUNT)),
          G_IM_FMT_CI, G_IM_SIZ_8b, BACKDROP_WIDTH,
          shownRowCount + BACKDROP_SEAM_ROW_COUNT, 0, G_TX_NOMIRROR | G_TX_WRAP,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
          G_TX_NOLOD);
      gDPPipeSync(gfx++);
      stripTop = BACKDROP_TOP +
          (stripIndex * BACKDROP_STRIP_ROW_COUNT * BACKDROP_SCALE);
      gSPScisTextureRectangle(gfx++, QuarterPixels(BACKDROP_LEFT),
          QuarterPixels(stripTop),
          QuarterPixels(BACKDROP_LEFT + (BACKDROP_WIDTH * BACKDROP_SCALE)),
          QuarterPixels(BACKDROP_TOP +
              (stripIndex * BACKDROP_STRIP_ROW_COUNT * BACKDROP_SCALE) +
              (shownRowCount * BACKDROP_SCALE)),
          G_TX_RENDERTILE, 0, BACKDROP_SECOND_ROW_START,
          TEXEL_STEP_ONE / BACKDROP_SCALE, TEXEL_STEP_ONE / BACKDROP_SCALE);
    }
  }

  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}

void func_801FCB74_ovl_01(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
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
      gfx++, TextureAddress(ImageBody(D_800FF420[D_8022BE92_ovl_01])));
  for (stripIndex = 0; stripIndex != NARROW_BOARD_STRIP_COUNT; stripIndex++) {
    gDPLoadTextureBlock(gfx++,
        TextureAddress(LargeImageTexels(D_800FF420[D_8022BE92_ovl_01]) +
            (stripIndex * NARROW_BOARD_WIDTH * NARROW_BOARD_STRIP_HEIGHT)),
        G_IM_FMT_CI, G_IM_SIZ_8b, NARROW_BOARD_WIDTH, NARROW_BOARD_STRIP_HEIGHT,
        0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    stripTop = NARROW_BOARD_TOP + (stripIndex * NARROW_BOARD_STRIP_SPACING);
    gSPScisTextureRectangle(gfx++, QuarterPixels(NARROW_BOARD_LEFT),
        QuarterPixels(stripTop),
        QuarterPixels(NARROW_BOARD_LEFT + NARROW_BOARD_SHOWN_WIDTH),
        QuarterPixels(stripTop + NARROW_BOARD_STRIP_HEIGHT), G_TX_RENDERTILE, 0,
        0, (NARROW_BOARD_WIDTH << 10) / NARROW_BOARD_SHOWN_WIDTH,
        (NARROW_BOARD_STRIP_HEIGHT << 10) / NARROW_BOARD_ROW_SCALE);
    gDPPipeSync(gfx++);
  }

  *displayListHead = gfx;
}

/* Nothing in the menu calls this; menu.c loads its image, ASSET_NAME_PLATE */
void func_801FCEF8_ovl_01(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 stripIndex;
  s32 stripTop;

  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPLoadTLUT_pal256(gfx++, TextureAddress(ImageBody(D_801B60D0)));
  for (stripIndex = 0; stripIndex != MENU_BANNER_STRIP_COUNT; stripIndex++) {
    gDPLoadTextureBlock(gfx++,
        TextureAddress(LargeImageTexels(D_801B60D0) +
            (stripIndex * MENU_BANNER_WIDTH * MENU_BANNER_STRIP_HEIGHT)),
        G_IM_FMT_CI, G_IM_SIZ_8b, MENU_BANNER_WIDTH, MENU_BANNER_STRIP_HEIGHT,
        0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    stripTop = UNUSED_BANNER_TOP + (stripIndex * MENU_BANNER_STRIP_HEIGHT);
    gSPScisTextureRectangle(gfx++, QuarterPixels(MENU_BANNER_LEFT),
        QuarterPixels(stripTop),
        QuarterPixels(MENU_BANNER_LEFT + MENU_BANNER_WIDTH),
        QuarterPixels(stripTop + MENU_BANNER_STRIP_HEIGHT), G_TX_RENDERTILE, 0,
        0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    gDPPipeSync(gfx++);
  }

  *displayListHead = gfx;
}

/* Doesn't match: the original tests the s32 top << 2 for the texture's row
 * offset, not the s16 top */
#ifdef NON_MATCHING
void func_801FD1B4_ovl_01(Gfx** displayListHead, s16 left, s16 top) {
  Gfx* gfx = *displayListHead;
  s32 stripIndex;

  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPLoadTLUT_pal256(gfx++, TextureAddress(ImageBody(D_801B60D0)));
  for (stripIndex = 0; stripIndex != MENU_BANNER_STRIP_COUNT; stripIndex++) {
    gDPLoadTextureBlock(gfx++,
        TextureAddress(LargeImageTexels(D_801B60D0) +
            stripIndex * MENU_BANNER_WIDTH * MENU_BANNER_STRIP_HEIGHT),
        G_IM_FMT_CI, G_IM_SIZ_8b, MENU_BANNER_WIDTH, MENU_BANNER_STRIP_HEIGHT,
        0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, QuarterPixels(left), QuarterPixels(top),
        QuarterPixels(left + SMALL_BANNER_WIDTH),
        QuarterPixels(top + MENU_BANNER_STRIP_HEIGHT), G_TX_RENDERTILE, 0, 0,
        (MENU_BANNER_WIDTH << 10) / SMALL_BANNER_WIDTH, TEXEL_STEP_ONE);
    top += MENU_BANNER_STRIP_HEIGHT;
    gDPPipeSync(gfx++);
  }

  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_801FD1B4_ovl_01);
#endif

/* Nothing in the menu calls this either */
void func_801FD4F0_ovl_01(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 stripIndex;
  s32 stripTop;

  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPLoadTLUT_pal256(gfx++, TextureAddress(ImageBody(D_801B60D0)));
  for (stripIndex = 0; stripIndex != MENU_BANNER_STRIP_COUNT; stripIndex++) {
    gDPLoadTextureBlock(gfx++,
        TextureAddress(LargeImageTexels(D_801B60D0) +
            (stripIndex * MENU_BANNER_WIDTH * MENU_BANNER_STRIP_HEIGHT)),
        G_IM_FMT_CI, G_IM_SIZ_8b, MENU_BANNER_WIDTH, MENU_BANNER_STRIP_HEIGHT,
        0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    stripTop = LARGE_BANNER_TOP + (stripIndex * LARGE_BANNER_STRIP_SPACING);
    gSPScisTextureRectangle(gfx++, QuarterPixels(LARGE_BANNER_LEFT),
        QuarterPixels(stripTop),
        QuarterPixels(LARGE_BANNER_LEFT + LARGE_BANNER_WIDTH),
        QuarterPixels(stripTop + LARGE_BANNER_STRIP_HEIGHT), G_TX_RENDERTILE, 0,
        0, (MENU_BANNER_WIDTH << 10) / LARGE_BANNER_WIDTH,
        (MENU_BANNER_STRIP_HEIGHT << 10) / LARGE_BANNER_STRIP_HEIGHT);
    gDPPipeSync(gfx++);
  }

  *displayListHead = gfx;
}

void func_801FD7B0_ovl_01(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 stripIndex;
  u8* image = D_801052BC[D_8022BE8C_ovl_01];
  s32 stripTop;

  gDPLoadTLUT_pal256(gfx++, TextureAddress(ImageBody(image)));
  for (stripIndex = 0; stripIndex != MENU_BANNER_STRIP_COUNT; stripIndex++) {
    gDPLoadTextureBlock(gfx++,
        TextureAddress(LargeImageTexels(image) +
            (stripIndex * MENU_BANNER_WIDTH * MENU_BANNER_STRIP_HEIGHT)),
        G_IM_FMT_CI, G_IM_SIZ_8b, MENU_BANNER_WIDTH, MENU_BANNER_STRIP_HEIGHT,
        0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    stripTop = MENU_BANNER_TOP + (stripIndex * MENU_BANNER_STRIP_HEIGHT);
    gSPScisTextureRectangle(gfx++, QuarterPixels(MENU_BANNER_LEFT),
        QuarterPixels(stripTop),
        QuarterPixels(MENU_BANNER_LEFT + MENU_BANNER_WIDTH),
        QuarterPixels(stripTop + MENU_BANNER_STRIP_HEIGHT), G_TX_RENDERTILE, 0,
        0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    gDPPipeSync(gfx++);
  }

  *displayListHead = gfx;
}

/* NEW GAME comes first, then the player counts, then the saved games and the
 * menus */
#define TITLE_LABEL_NEW_GAME 0
#define TITLE_LABEL_FIRST_PLAYER_COUNT 1
#define TITLE_LABEL_FIRST_OTHER_CHOICE 4
#define TITLE_LABEL_SAVED_GAMES 5
/* While g_HasBroughtOverCharacter is set the saved games' row leads to
 * Transmit, and this label takes its place */
#define TITLE_LABEL_TRANSMIT 7
/* A plain-text menu under the board: "MAIN MENU", "1P GAME" to "3P GAME" and
 * the sizes of the save's ranked records, its ring shot records and the whole
 * save */
#define PLAIN_MENU_HEADING_X 64
#define PLAIN_MENU_HEADING_Y 32
#define PLAIN_MENU_NAME_COUNT 3
#define PLAIN_MENU_NAME_X 112
#define PLAIN_MENU_NAME_TOP 128
#define PLAIN_MENU_NAME_SPACING 8
#define PLAIN_MENU_FIGURES_X 32
#define PLAIN_MENU_FIGURES_Y 100
/* The scenery, the board and both banners, behind the title, the main menus,
 * the clubhouse and screen 8 */
#define BOARD_BACKDROP_LAYERS \
  (MENU_LAYER_SCENERY | MENU_LAYER_BOARD | MENU_LAYER_BANNER | \
      MENU_LAYER_LARGE_BANNER | MENU_LAYER_UNK20)

/*
 * Doesn't match: in the lit labels' loop GCC works out labelIndex *
 * TITLE_LABEL_IMAGE_SIZE afresh each pass, where the original steps an offset
 * by 5 alongside the label table's pointers, which also moves the title row's
 * pointer from s0 to t7. 100 diff lines are left, mostly from those registers;
 * a permuter run found nothing better.
 */
#ifdef NON_MATCHING
const char D_80231634_ovl_01[] = "MAIN MENU";
const char D_80231640_ovl_01[] = "%8d:%8d:%8d";

/*
 * Whether a title label is the chosen one: New Game and the chosen player
 * count once New Game is chosen, or else the chosen choice's own label
 */
#define IsChosenTitleLabel(labelIndex) \
  ((D_801323A0.titleChoice == TITLE_CHOICE_NEW_GAME && \
       (labelIndex) == TITLE_LABEL_NEW_GAME) || \
      (D_801323A0.titleChoice == TITLE_CHOICE_NEW_GAME && \
          (labelIndex) == \
              D_801323A0.playerCountChoice + \
                  TITLE_LABEL_FIRST_PLAYER_COUNT) || \
      (D_801323A0.titleChoice + TITLE_LABEL_FIRST_OTHER_CHOICE == \
              (labelIndex) && \
          D_801323A0.titleChoice != TITLE_CHOICE_NEW_GAME))

void func_801FDA40_ovl_01(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 labelIndex;
  s32 labelOffset;
  s16 growth;

  BeginShadowedText(&gfx);
  SetTextColor(&gfx, 255, 255, 255);
  DrawShadowedText(
      &gfx, PLAIN_MENU_HEADING_X, PLAIN_MENU_HEADING_Y, D_80231634_ovl_01);
  for (labelIndex = 0; labelIndex != PLAIN_MENU_NAME_COUNT; labelIndex++) {
    DrawShadowedText(&gfx, PLAIN_MENU_NAME_X,
        PLAIN_MENU_NAME_TOP + (labelIndex * PLAIN_MENU_NAME_SPACING),
        D_8022BF28_ovl_01[labelIndex]);
  }

  sprintf(g_PrintBuffer, D_80231640_ovl_01, MemberSize(SaveData, rankedRecords),
      MemberSize(SaveData, ringShotRecords), (s32)sizeof(SaveData));
  DrawShadowedText(
      &gfx, PLAIN_MENU_FIGURES_X, PLAIN_MENU_FIGURES_Y, g_PrintBuffer);
  func_801FB54C_ovl_01(&gfx, BOARD_BACKDROP_LAYERS);
  if (g_ControllerStates[0].buttons & B_BUTTON) {
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_AVERAGE);
  }

  gDPLoadTLUT_pal16(
      gfx++, 0, TextureAddress(ImageBody(D_801B5CD0[D_8022BE90_ovl_01])));
  for (labelIndex = 0; labelIndex != TITLE_LABEL_COUNT; labelIndex++) {
    if (IsChosenTitleLabel(labelIndex)) {
      continue;
    }
    {
      if (labelIndex == TITLE_LABEL_SAVED_GAMES &&
          g_HasBroughtOverCharacter == TRUE) {
        labelOffset = TITLE_LABEL_TRANSMIT * TITLE_LABEL_IMAGE_SIZE;
      } else {
        labelOffset = labelIndex * TITLE_LABEL_IMAGE_SIZE;
      }

      gDPLoadTextureBlock_4b(gfx++,
          TextureAddress(
              ImageTexels(D_801B5CD0[D_8022BE90_ovl_01]) + labelOffset),
          G_IM_FMT_CI, TITLE_LABEL_WIDTH, TITLE_LABEL_HEIGHT, 0,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      gDPPipeSync(gfx++);
      gSPScisTextureRectangle(gfx++,
          QuarterPixels(D_8022F940_ovl_01[labelIndex][0]),
          QuarterPixels(D_8022F940_ovl_01[labelIndex][1]),
          QuarterPixels(D_8022F940_ovl_01[labelIndex][0] + TITLE_LABEL_WIDTH),
          QuarterPixels(D_8022F940_ovl_01[labelIndex][1] + TITLE_LABEL_HEIGHT),
          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
      gDPPipeSync(gfx++);
    }
  }

  gDPLoadTLUT_pal16(
      gfx++, 0, TextureAddress(ImageBody(D_8012D420[D_8022BE8E_ovl_01])));
  for (labelIndex = 0; labelIndex != TITLE_LABEL_COUNT; labelIndex++) {
    if (IsChosenTitleLabel(labelIndex)) {
      if (labelIndex == TITLE_LABEL_SAVED_GAMES &&
          g_HasBroughtOverCharacter == TRUE) {
        labelOffset = TITLE_LABEL_TRANSMIT * TITLE_LABEL_IMAGE_SIZE;
      } else {
        labelOffset = labelIndex * TITLE_LABEL_IMAGE_SIZE;
      }

      gDPLoadTextureBlock_4b(gfx++,
          TextureAddress(
              ImageTexels(D_8012D420[D_8022BE8E_ovl_01]) + labelOffset),
          G_IM_FMT_CI, TITLE_LABEL_WIDTH, TITLE_LABEL_HEIGHT, 0,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      gDPPipeSync(gfx++);

      /* NEW GAME stays still */
      growth = 0;
      if (labelIndex != TITLE_LABEL_NEW_GAME) {
        growth = D_800FE33C;
      }

      gSPScisTextureRectangle(gfx++,
          QuarterPixels(D_8022F940_ovl_01[labelIndex][0] - (growth / 2)),
          QuarterPixels(D_8022F940_ovl_01[labelIndex][1] - (growth / 2)),
          QuarterPixels(D_8022F940_ovl_01[labelIndex][0] + TITLE_LABEL_WIDTH +
              (growth / 2)),
          QuarterPixels(D_8022F940_ovl_01[labelIndex][1] +
              (TITLE_LABEL_HEIGHT + (growth / 2))),
          G_TX_RENDERTILE, 0, 0,
          (TITLE_LABEL_WIDTH << 10) / (growth + TITLE_LABEL_WIDTH),
          (TITLE_LABEL_HEIGHT << 10) / (growth + TITLE_LABEL_HEIGHT));
      gDPPipeSync(gfx++);
    }
  }

  if (D_800FBDB9 == LABEL_GROWING) {
    D_800FE33C += CHOSEN_LABEL_GROWTH_STEP;
  }

  if (D_800FBDB9 == LABEL_SHRINKING) {
    D_800FE33C -= CHOSEN_LABEL_GROWTH_STEP;
  }

  if (D_800FE33C > CHOSEN_LABEL_GROWTH_LIMIT) {
    D_800FBDB9 = LABEL_SHRINKING;
  }

  if (D_800FE33C < 0) {
    D_800FBDB9 = LABEL_GROWING;
  }

  *displayListHead = gfx;
}

/* Nothing reads this */
const char D_8023164C_ovl_01[] = "";
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_801FDA40_ovl_01);
#endif

/*
 * The drawing of most of the menu screens: the main menu, the clubhouse, the
 * options, the help and birdie badge screens, and screens 5, 6, 8, 18, 20, 22,
 * 23 and 26, and the menus' labels, the chosen mode's icon, the players'
 * characters, the waving Toad, the debug character viewer and the debug caddy
 * select
 */

/* The STATUS, HOW TO PLAY, CODE ENTRY and OPTIONS labels, 128 by 20 each, plain
 * in D_801B5CD0 and lit in D_8012D420 */
#define MAIN_CHOICE_LABEL_WIDTH 128
#define MAIN_CHOICE_LABEL_HEIGHT 20
#define MAIN_CHOICE_LABEL_IMAGE_SIZE \
  (MAIN_CHOICE_LABEL_WIDTH * MAIN_CHOICE_LABEL_HEIGHT / 2)

/* The 24x24 icon that func_80207E28 animates, such as the birdie badge; each
 * frame shows for two calls */
#define ANIMATED_ICON_SIZE 24
#define ANIMATED_ICON_IMAGE_SIZE (ANIMATED_ICON_SIZE * ANIMATED_ICON_SIZE / 2)
#define ANIMATED_ICON_FRAME_COUNT 6
#define ANIMATED_ICON_FRAME_DURATION 2
/* func_80200A40 draws the chosen mode's icon, 64x64 CI8, in two strips */
#define MODE_ICON_WIDTH 64
#define MODE_ICON_STRIP_HEIGHT 32
#define MODE_ICON_STRIP_COUNT 2
#define MODE_ICON_FRAME_DURATION 4
/* Where func_802004A0 draws the mode icon, unscaled */
#define MODE_ICON_LEFT 168
#define MODE_ICON_TOP 96

/* DrawMenuCharacters draws the players' characters, and in Get Character the
 * rival as a spinning silhouette */
#define NO_RIVAL -1
#define RIVAL_SLOT 1
#define RIVAL_SPIN_STEP 10.0f

/* func_80208F8C draws a frame of the waving Toad (assets 0x710 onward), 48x51
 * CI8, in two strips */
#define TOAD_WIDTH 48
#define TOAD_STRIP_HEIGHT 30
#define TOAD_LAST_STRIP_HEIGHT 21
#define TOAD_STRIP_COUNT 2

/* Where the debug caddy select draws its title and names */
#define CADDY_SELECT_TITLE_X 64
#define CADDY_SELECT_TITLE_Y 32
#define CADDY_SELECT_NAME_X 96
#define CADDY_SELECT_FIRST_Y 64
#define CADDY_SELECT_LINE_HEIGHT 8

extern const char D_8023176C_ovl_01[];

/* The D_8022BE88 phase in which the course select's banners slide the other way
 */
#define MENU_PHASE_UNK7 7

/*
 * The mode select's boards turn on a circle like the saved games' boards,
 * wider for a single player; the chosen one shows its animated mode icon once
 * it stops turning
 */
#define MODE_SELECT_BACKDROP_LAYERS \
  (MENU_LAYER_SCENERY | MENU_LAYER_UNK2 | MENU_LAYER_BANNER | \
      MENU_LAYER_LARGE_BANNER)
#define MODE_SELECT_LIGHT_YAW 0
#define MODE_SELECT_LIGHT_PITCH 0x3000
#define MODE_BOARD_EYE_HEIGHT 70.0f
#define MODE_BOARD_EYE_DISTANCE 60.0f
#define MODE_BOARD_RADIUS_X 48.0f
#define SINGLE_PLAYER_MODE_BOARD_RADIUS_X 64.0f
#define MODE_BOARD_RADIUS_Z 40.0f
#define MODE_BOARD_HALF_SIZE 32.0f
/* The boards not chosen are darkened */
#define UNCHOSEN_MODE_BOARD_SHADE 127
/* The bob that a board past the last would have, a step of its phase per
 * update */
#define MODE_BOARD_BOB_PHASE_STEP 0.1f
#define MODE_BOARD_BOB_HEIGHT 8.0f
/* The strips of D_801B60D0 below the boards, drawn wider for a single player */
#define MODE_BANNER_TOP 152
#define MODE_BANNER_LEFT 80
#define MODE_BANNER_SHOWN_WIDTH 160
#define SINGLE_PLAYER_MODE_BANNER_SHOWN_WIDTH 180
/* The rows of the modes' label image, 192 by 20 CI4 each */
#define MODE_LABEL_WIDTH 192
#define MODE_LABEL_HEIGHT 20
#define MODE_LABEL_IMAGE_SIZE ((MODE_LABEL_WIDTH * MODE_LABEL_HEIGHT) / 2)
#define MODE_LABEL_X 160
#define SINGLE_PLAYER_MODE_LABEL_X 170
#define MODE_LABEL_TOP 156
#define MODE_ARROW_GAP 64
#define MODE_ARROW_Y 124
/* A single player's Tournament shows the birdie badges won, and Ring Shot the
 * stars won, centered by the count's number of digits */
#define WON_COUNT_ICON_X 224
#define WON_COUNT_ICON_Y 64
#define WON_COUNT_X 250
#define WON_COUNT_Y 72
#define WON_COUNT_DIGIT_WIDTH 16
/* The won counts from which a count takes two digits, and three */
#define MIN_TWO_DIGIT_WON_COUNT 10
#define MIN_THREE_DIGIT_WON_COUNT 100

/*
 * The course select's water plane, D_8022FD48, blends the water image and the
 * scrolling layer, which scroll different ways, and is fogged white
 */
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
/* The course select's wheel is seen from closer than the other wheels */
#define COURSE_SELECT_EYE_HEIGHT 50.0f
#define COURSE_SELECT_EYE_DISTANCE 60.0f
/* The front course's image bobs, a step of its phase per update, and the
 * chosen one updates the depth buffer at a fixed depth */
#define FRONT_COURSE_BOB_PHASE_STEP 0.1f
#define FRONT_COURSE_BOB_HEIGHT 8.0f
#define CHOSEN_COURSE_DEPTH 0x7FFF
/* The course after the last open one is grayed out, with a panel over it
 * typing the points it needs */
#define LOCKED_COURSE_SHADE 96
#define LOCKED_PANEL_LEFT 108
#define LOCKED_PANEL_TOP 100
#define LOCKED_PANEL_RIGHT 212
#define LOCKED_PANEL_BOTTOM 160
#define LOCKED_COURSE_TEXT_X 111
#define LOCKED_COURSE_TEXT_Y 116
#define MESSAGE_POINTS_TO_UNLOCK 0xC2
/* The course's birdie badge icon and count, centered by its number of digits
 */
#define COURSE_BADGE_ICON_X 226
#define COURSE_BADGE_ICON_Y 200
#define COURSE_BADGE_COUNT_X 252
#define COURSE_BADGE_COUNT_Y 208
/* The chosen holes badge is lit, and the Bet badge cycles its color unless
 * D_800C2BD5 is set */
#define UNCHOSEN_HOLES_BADGE_SHADE 127
#define BET_BADGE_COLOR_STEP (M_PI / 18)
#define BET_BADGE_COLOR_MIDPOINT 127
#define BET_BADGE_COLOR_SWING 127.0f
#define BET_BADGE_ENV_DIVISOR 4
/* func_801FB54C's layers drawn behind the course select, and in front of it
 * and of screens 20 and 22 */
#define COURSE_SELECT_BACK_LAYERS MENU_LAYER_SCENERY
#define COURSE_SELECT_FRONT_LAYERS (MENU_LAYER_UNK2 | MENU_LAYER_BANNER)
/* Screens 20 and 22 slide their pages this far, a step per update */
#define BET_SLIDE_STEP 32
#define BET_SLIDE_DISTANCE 512

void StartLoadingChosenHole(void);
void func_80200A40_ovl_01(
    Gfx** displayListHead, s32 left, s32 top, s32 halfSize, s32 scale);
void DrawMenuCharacters(Gfx** displayListHead);
void DrawCourseSelectBanners(Gfx** displayListHead);
void func_80207E28_ovl_01(Gfx** displayListHead, void* image, s32 x, s32 y);
void func_802081E8_ovl_01(Gfx** displayListHead, s32 left, s32 top,
    u8 starCount, u8 enlargesChosenHole);
void DrawUnalignedMenuNumber(
    Gfx** displayListHead, s32 x, s32 y, s32 value, u8 mark);
void func_8021826C_ovl_01(
    Gfx** displayListHead, s32 slide, u8 isSlideReversed, u8 isSettled);
void func_80219BD4_ovl_01(Gfx** displayListHead, s32 slide,
    u8 reverseVerticalSlide, u8 canShowPrompt);
s32 func_8021B618_ovl_01(Gfx** displayListHead, u8 slide);
void func_8021BE88_ovl_01(Gfx** displayListHead);
void func_8021C200_ovl_01(Gfx** displayListHead, s32 gap, s32 centerY);
void func_8021CBDC_ovl_01(Gfx** displayListHead, s32 centerY);

/*
 * Draws the mode select of screens 5, 23 and 26: the characters, the modes'
 * boards turning on their circle, the chosen mode's label, each player's
 * controller icon and label, and for a single player the birdie badges or
 * stars won.
 */
/* Doesn't match: the boards' strips and the mode label schedule their
 * address loads differently */
#ifdef NON_MATCHING
void func_801FE4D0_ovl_01(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  u16 perspNorm;
  /* Sized like the course wheel's */
  f32 boardX[WHEEL_SLOT_LIMIT];
  f32 boardY[WHEEL_SLOT_LIMIT];
  f32 boardScale[WHEEL_SLOT_LIMIT];
  s32 unusedSlots[WHEEL_SLOT_LIMIT];
  s32 drawOrder[WHEEL_SLOT_LIMIT];
  s32 position[2];
  SaveData* save = GetSaveData();
  s16 badgeCount = 0;
  s16 starCount;
  s32 modeCount;
  s32 index;
  s32 otherIndex;
  s32 key;
  f32 radiusX;
  s32 angleStep;
  Radians angle;
  f32 circleX;
  /* A board's turns from the chosen one, then the slot of the board drawn */
  s32 slot;
  s32 halfSize;
  s32 step;
  f32 bob;
  u8 areCharactersDrawn;
  s32 labelIndex;
  s32 labelX;
  s32* labelOffset;
  void* image;
  s32 countShift;
  s32 courseIndex;
  s32 holeIndex;

  g_HoleSetup.conditions.lightYaw = MODE_SELECT_LIGHT_YAW;
  g_HoleSetup.conditions.lightPitch = MODE_SELECT_LIGHT_PITCH;
  func_801FC60C_ovl_01(&gfx, MODE_SELECT_BACKDROP_LAYERS);
  guPerspectiveF(D_80231CA0_ovl_01, &perspNorm, WHEEL_FIELD_OF_VIEW,
      SCREEN_ASPECT_RATIO, WHEEL_NEAR_PLANE, WHEEL_FAR_PLANE, 1.0f);
  guLookAtReflectF(D_80231CE0_ovl_01, &D_80231D20_ovl_01[g_DrawBufferIndex],
      0.0f, MODE_BOARD_EYE_HEIGHT, MODE_BOARD_EYE_DISTANCE, 0.0f, 0.0f, 0.0f,
      0.0f, 1.0f, 0.0f);
  gSPLookAt(gfx++, &D_80231D20_ovl_01[g_DrawBufferIndex]);
  SetCpuProjectionF(D_80231CA0_ovl_01, D_80231CE0_ovl_01, g_Viewports);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_NONE);

  modeCount = D_8022BE30_ovl_01[g_ChosenPlayerCount - 1];
  for (index = 0; index != modeCount; index++) {
    ScreenPoint point;

    radiusX = MODE_BOARD_RADIUS_X;
    if (g_ChosenPlayerCount == 1) {
      radiusX = SINGLE_PLAYER_MODE_BOARD_RADIUS_X;
    }

    slot = (D_801323A0.modeChoice - index) % modeCount;
    if (modeCount != 0) {
      angleStep = DEGREES_PER_TURN / modeCount;
    } else {
      angleStep = DEGREES_PER_TURN;
    }

    angle = -DegreesToRadians(
        (slot * angleStep) + (D_801323A0.modeSpin * DEGREES_PER_SPIN_STEP));
    circleX = sinf(angle) * radiusX;
    if (ProjectPoint(circleX, 0.0f, cosf(angle) * MODE_BOARD_RADIUS_Z, &point) <
        0) {
      boardX[index] = point.x;
      boardY[index] = point.y - WHEEL_RAISE;
      boardScale[index] = point.scaleX;
    }
  }

  for (otherIndex = 0; otherIndex != modeCount; otherIndex++) {
    drawOrder[otherIndex] = otherIndex;
  }

  /* Draw the farthest boards first, so the nearer ones cover them */
  for (otherIndex = 1; otherIndex != modeCount; otherIndex++) {
    key = drawOrder[otherIndex];
    for (index = otherIndex - 1;
        (index >= 0) && (boardY[drawOrder[index]] > boardY[key]); index--) {
      drawOrder[index + 1] = drawOrder[index];
    }

    drawOrder[index + 1] = key;
  }

  gDPPipeSync(gfx++);
  gDPSetDepthSource(gfx++, G_ZS_PRIM);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);

  areCharactersDrawn = FALSE;
  D_8022F978_ovl_01 += MODE_BOARD_BOB_PHASE_STEP;
  for (index = 0; index != modeCount; index++) {
    slot = drawOrder[index];
    halfSize = boardScale[slot] * MODE_BOARD_HALF_SIZE;
    step = (f32)(MODE_ICON_STRIP_HEIGHT << 10) / halfSize;
    /* The loop stops before modeCount, so no board bobs */
    if (index == modeCount) {
      bob = sinf(D_8022F978_ovl_01) * MODE_BOARD_BOB_HEIGHT;
    } else {
      bob = 0.0f;
    }

    gDPLoadTLUT_pal256(gfx++, TextureAddress(ImageBody(D_8012F4B0[slot])));
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    if ((D_801323A0.modeChoice == slot) && (D_801323A0.modeSpin == 0)) {
      if (!areCharactersDrawn) {
        DrawMenuCharacters(&gfx);
        areCharactersDrawn = TRUE;
        gDPPipeSync(gfx++);
        gDPPipeSync(gfx++);
        gDPSetCycleType(gfx++, G_CYC_1CYCLE);
        gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
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
      func_80200A40_ovl_01(&gfx, boardX[slot] - halfSize,
          boardY[slot] - halfSize + bob, halfSize, step);
    } else {
      if (D_801323A0.modeChoice == slot) {
        /* The characters go behind the chosen board, and change the state */
        if (!areCharactersDrawn) {
          DrawMenuCharacters(&gfx);
          gDPPipeSync(gfx++);
          gDPPipeSync(gfx++);
          gDPSetCycleType(gfx++, G_CYC_1CYCLE);
          gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
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
              gfx++, TextureAddress(ImageBody(D_8012F4B0[slot])));
          areCharactersDrawn = TRUE;
        }
      } else {
        gDPSetCombineMode(
            gfx++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
        gDPSetPrimColor(gfx++, 0, 0, UNCHOSEN_MODE_BOARD_SHADE,
            UNCHOSEN_MODE_BOARD_SHADE, UNCHOSEN_MODE_BOARD_SHADE, 255);
      }

      for (otherIndex = 0; otherIndex != MODE_ICON_STRIP_COUNT; otherIndex++) {
        gDPLoadTextureBlock(gfx++,
            TextureAddress(LargeImageTexels(D_8012F4B0[slot]) +
                (otherIndex * MODE_ICON_WIDTH * MODE_ICON_STRIP_HEIGHT)),
            G_IM_FMT_CI, G_IM_SIZ_8b, MODE_ICON_WIDTH, MODE_ICON_STRIP_HEIGHT,
            0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(gfx++, (s32)(boardX[slot] - halfSize) << 2,
            (s32)(boardY[slot] - halfSize + (otherIndex * halfSize) + bob) << 2,
            (s32)(boardX[slot] + halfSize) << 2,
            (s32)(boardY[slot] + (otherIndex * halfSize) + bob) << 2,
            G_TX_RENDERTILE, 0, 0, step, step);
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

  gDPLoadTLUT_pal256(gfx++, TextureAddress(ImageBody(D_801B60D0)));
  if (g_ChosenPlayerCount == 1) {
    for (index = 0; index != MENU_BANNER_STRIP_COUNT; index++) {
      gDPLoadTextureBlock(gfx++,
          TextureAddress(LargeImageTexels(D_801B60D0) +
              (index * MENU_BANNER_WIDTH * MENU_BANNER_STRIP_HEIGHT)),
          G_IM_FMT_CI, G_IM_SIZ_8b, MENU_BANNER_WIDTH, MENU_BANNER_STRIP_HEIGHT,
          0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      gDPPipeSync(gfx++);
      gSPScisTextureRectangle(gfx++, QuarterPixels(MODE_BANNER_LEFT),
          QuarterPixels(MODE_BANNER_TOP + (index * MENU_BANNER_STRIP_HEIGHT)),
          QuarterPixels(
              MODE_BANNER_LEFT + SINGLE_PLAYER_MODE_BANNER_SHOWN_WIDTH),
          QuarterPixels(MODE_BANNER_TOP + MENU_BANNER_STRIP_HEIGHT +
              (index * MENU_BANNER_STRIP_HEIGHT)),
          G_TX_RENDERTILE, 0, 0,
          (MENU_BANNER_WIDTH << 10) / SINGLE_PLAYER_MODE_BANNER_SHOWN_WIDTH,
          TEXEL_STEP_ONE);
      gDPPipeSync(gfx++);
    }
  } else {
    for (index = 0; index != MENU_BANNER_STRIP_COUNT; index++) {
      gDPLoadTextureBlock(gfx++,
          TextureAddress(LargeImageTexels(D_801B60D0) +
              (index * MENU_BANNER_WIDTH * MENU_BANNER_STRIP_HEIGHT)),
          G_IM_FMT_CI, G_IM_SIZ_8b, MENU_BANNER_WIDTH, MENU_BANNER_STRIP_HEIGHT,
          0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      gDPPipeSync(gfx++);
      gSPScisTextureRectangle(gfx++, QuarterPixels(MODE_BANNER_LEFT),
          QuarterPixels(MODE_BANNER_TOP + (index * MENU_BANNER_STRIP_HEIGHT)),
          QuarterPixels(MODE_BANNER_LEFT + MODE_BANNER_SHOWN_WIDTH),
          QuarterPixels(MODE_BANNER_TOP + MENU_BANNER_STRIP_HEIGHT +
              (index * MENU_BANNER_STRIP_HEIGHT)),
          G_TX_RENDERTILE, 0, 0,
          (MENU_BANNER_WIDTH << 10) / MODE_BANNER_SHOWN_WIDTH, TEXEL_STEP_ONE);
      gDPPipeSync(gfx++);
    }
  }

  gDPLoadTLUT_pal16(
      gfx++, 0, TextureAddress(ImageBody(D_801B5CD0[D_8022BE90_ovl_01])));
  labelIndex =
      D_8022BEBC_ovl_01[g_ChosenPlayerCount - 1][D_801323A0.modeChoice];
  gDPLoadTextureBlock_4b(gfx++,
      TextureAddress(ImageTexels(D_801B5CD0[D_8022BE90_ovl_01]) +
          (labelIndex * MODE_LABEL_IMAGE_SIZE)),
      G_IM_FMT_CI, MODE_LABEL_WIDTH, MODE_LABEL_HEIGHT, 0,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
      G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
  gDPPipeSync(gfx++);

  labelOffset = &D_8022FA28_ovl_01[labelIndex];
  labelX =
      (g_ChosenPlayerCount == 1) ? SINGLE_PLAYER_MODE_LABEL_X : MODE_LABEL_X;
  gSPScisTextureRectangle(gfx++, QuarterPixels(labelX - *labelOffset),
      QuarterPixels(MODE_LABEL_TOP),
      QuarterPixels(labelX - *labelOffset + MODE_LABEL_WIDTH),
      QuarterPixels(MODE_LABEL_TOP + MODE_LABEL_HEIGHT), G_TX_RENDERTILE, 0, 0,
      TEXEL_STEP_ONE, TEXEL_STEP_ONE);
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

  image = D_800FF180[0];
  gDPLoadTLUT_pal256(gfx++, TextureAddress(ImageBody(image)));
  for (index = 0; index != g_ChosenPlayerCount; index++) {
    if (g_PlayerControllerIndices[index] != NO_CONTROLLER_INDEX) {
      position[AXIS_X] = D_8022F97C_ovl_01[g_ChosenPlayerCount - 1][index][0];
      position[AXIS_Y] = D_8022F97C_ovl_01[g_ChosenPlayerCount - 1][index][1];
      gDPLoadTextureBlock(gfx++,
          TextureAddress(LargeImageTexels(image) +
              (g_ChoosingControllerIndices[index] *
                  CONTROLLER_ICON_IMAGE_SIZE)),
          G_IM_FMT_CI, G_IM_SIZ_8b, CONTROLLER_ICON_SIZE, CONTROLLER_ICON_SIZE,
          0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      gDPPipeSync(gfx++);
      gSPScisTextureRectangle(gfx++, QuarterPixels(position[AXIS_X]),
          QuarterPixels(position[AXIS_Y]),
          QuarterPixels(position[AXIS_X] + CONTROLLER_ICON_SIZE),
          QuarterPixels(position[AXIS_Y] + CONTROLLER_ICON_SIZE),
          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
      gDPPipeSync(gfx++);
    }
  }

  image = D_800FF180[1];
  gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImageBody(image)));
  for (index = 0; index != g_ChosenPlayerCount; index++) {
    position[AXIS_X] = D_8022F97C_ovl_01[g_ChosenPlayerCount - 1][index][0];
    position[AXIS_Y] = D_8022F97C_ovl_01[g_ChosenPlayerCount - 1][index][1];
    if (g_PlayerControllerIndices[index] == NO_CONTROLLER_INDEX) {
      gDPLoadTextureBlock_4b(gfx++,
          TextureAddress(ImageTexels(image) +
              ((index + PLAYER_LABEL_COMPUTER_FIRST) *
                  PLAYER_LABEL_IMAGE_SIZE)),
          G_IM_FMT_CI, PLAYER_LABEL_WIDTH, PLAYER_LABEL_HEIGHT, 0,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    } else {
      gDPLoadTextureBlock_4b(gfx++,
          TextureAddress(
              ImageTexels(image) + (index * PLAYER_LABEL_IMAGE_SIZE)),
          G_IM_FMT_CI, PLAYER_LABEL_WIDTH, PLAYER_LABEL_HEIGHT, 0,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    }

    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++,
        QuarterPixels(position[AXIS_X] + PLAYER_LABEL_OFFSET_X),
        QuarterPixels(position[AXIS_Y] + PLAYER_LABEL_OFFSET_Y),
        QuarterPixels(
            position[AXIS_X] + PLAYER_LABEL_OFFSET_X + PLAYER_LABEL_WIDTH),
        QuarterPixels(
            position[AXIS_Y] + PLAYER_LABEL_OFFSET_Y + PLAYER_LABEL_HEIGHT),
        G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    gDPPipeSync(gfx++);
  }

  func_8021C200_ovl_01(&gfx, MODE_ARROW_GAP, MODE_ARROW_Y);

  if ((g_ChosenPlayerCount == 1) &&
      (D_8022BEBC_ovl_01[0][D_801323A0.modeChoice] == MODE_ICON_TOURNAMENT)) {
    badgeCount = 0;
    for (courseIndex = 0; courseIndex != COURSE_COUNT; courseIndex++) {
      for (holeIndex = 0; holeIndex != COURSE_HOLE_COUNT; holeIndex++) {
        if (save->awards.birdieBadgeCounts[courseIndex][holeIndex] > 0) {
          badgeCount++;
        }
      }
    }

    countShift = 0;
    if (badgeCount < MIN_TWO_DIGIT_WON_COUNT) {
      countShift = WON_COUNT_DIGIT_WIDTH;
    }

    if ((badgeCount >= MIN_TWO_DIGIT_WON_COUNT) &&
        (badgeCount < MIN_THREE_DIGIT_WON_COUNT)) {
      countShift = 0;
    }

    if (badgeCount >= MIN_THREE_DIGIT_WON_COUNT) {
      countShift = -WON_COUNT_DIGIT_WIDTH;
    }

    func_80207E28_ovl_01(
        &gfx, D_800FC898, WON_COUNT_ICON_X + countShift, WON_COUNT_ICON_Y);
    gDPPipeSync(gfx++);
    DrawUnalignedMenuNumber(&gfx, WON_COUNT_X + countShift, WON_COUNT_Y,
        badgeCount, MENU_NUMBER_TIMES);
    gDPPipeSync(gfx++);
  }

  if ((g_ChosenPlayerCount == 1) &&
      (D_8022BEBC_ovl_01[0][D_801323A0.modeChoice] == MODE_ICON_RING_SHOT)) {
    starCount = 0;
    for (courseIndex = 0; courseIndex != RING_SHOT_COURSE_COUNT;
        courseIndex++) {
      for (holeIndex = 0; holeIndex != RING_SHOT_HOLE_COUNT; holeIndex++) {
        if (save->hasRingShotStar[courseIndex][holeIndex] > 0) {
          starCount++;
        }
      }
    }

    countShift = 0;
    if (starCount < MIN_TWO_DIGIT_WON_COUNT) {
      countShift = WON_COUNT_DIGIT_WIDTH;
    }

    /* This tests the badge count, which is zero here, for the star count */
    if ((starCount >= MIN_TWO_DIGIT_WON_COUNT) &&
        (badgeCount < MIN_THREE_DIGIT_WON_COUNT)) {
      countShift = 0;
    }

    if (starCount >= MIN_THREE_DIGIT_WON_COUNT) {
      countShift = -WON_COUNT_DIGIT_WIDTH;
    }

    func_802081E8_ovl_01(
        &gfx, WON_COUNT_ICON_X + countShift, WON_COUNT_ICON_Y, 1, FALSE);
    gDPPipeSync(gfx++);
    DrawUnalignedMenuNumber(&gfx, WON_COUNT_X + countShift, WON_COUNT_Y,
        starCount, MENU_NUMBER_TIMES);
    gDPPipeSync(gfx++);
  }

  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_801FE4D0_ovl_01);
#endif

/* Nothing calls this */
void func_802004A0_ovl_01(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  EncodedAsset asset;
  s32 stripIndex;
  void* image;
  u8 iconIndex;

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  gDPPipeSync(gfx++);
  gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_NONE);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_POINT);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  image = D_8010526C[D_8022F8C4_ovl_01];
  gDPLoadTLUT_pal256(gfx++, TextureAddress(ImageBody(image)));
  for (stripIndex = 0; stripIndex != MODE_ICON_STRIP_COUNT; stripIndex++) {
    gDPLoadTextureBlock(gfx++,
        TextureAddress(LargeImageTexels(image) +
            (stripIndex * MODE_ICON_WIDTH * MODE_ICON_STRIP_HEIGHT)),
        G_IM_FMT_CI, G_IM_SIZ_8b, MODE_ICON_WIDTH, MODE_ICON_STRIP_HEIGHT, 0,
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, QuarterPixels(MODE_ICON_LEFT),
        QuarterPixels(MODE_ICON_TOP + (stripIndex * MODE_ICON_STRIP_HEIGHT)),
        QuarterPixels(MODE_ICON_LEFT + MODE_ICON_WIDTH),
        QuarterPixels(MODE_ICON_TOP + MODE_ICON_STRIP_HEIGHT +
            (stripIndex * MODE_ICON_STRIP_HEIGHT)),
        G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    gDPPipeSync(gfx++);
  }

  D_8022F8C5_ovl_01++;
  if (D_8022F8C5_ovl_01 == MODE_ICON_FRAME_DURATION) {
    iconIndex =
        D_8022BEBC_ovl_01[g_ChosenPlayerCount - 1][D_801323A0.modeChoice];
    D_8022F8C5_ovl_01 = 0;
    D_8022F8C0_ovl_01++;
    if (D_8022F8C0_ovl_01 >= MODE_ICON_FRAME_COUNT) {
      D_8022F8C0_ovl_01 = 0;
    }

    if (g_ModeIconAnimations[iconIndex][D_8022F8C0_ovl_01] ==
        END_OF_ICON_FRAMES) {
      D_8022F8C0_ovl_01 = 0;
    }

    D_8022F8C4_ovl_01 = D_8022F8C4_ovl_01 == 0;
    OpenEncodedAsset(
        g_ModeIconAnimations[iconIndex][D_8022F8C0_ovl_01], &asset);
    ReadEncodedAsset(D_8010526C[D_8022F8C4_ovl_01], &asset);
  }

  *displayListHead = gfx;
}

void func_80200A40_ovl_01(
    Gfx** displayListHead, s32 left, s32 top, s32 halfSize, s32 scale) {
  Gfx* gfx = *displayListHead;
  EncodedAsset asset;
  s32 stripIndex;
  void* image;
  u8 iconIndex;

  gDPPipeSync(gfx++);
  image = D_8010526C[D_8022F8C4_ovl_01];
  gDPLoadTLUT_pal256(gfx++, TextureAddress(ImageBody(image)));
  for (stripIndex = 0; stripIndex != MODE_ICON_STRIP_COUNT; stripIndex++) {
    gDPLoadTextureBlock(gfx++,
        TextureAddress(LargeImageTexels(image) +
            (stripIndex * MODE_ICON_WIDTH * MODE_ICON_STRIP_HEIGHT)),
        G_IM_FMT_CI, G_IM_SIZ_8b, MODE_ICON_WIDTH, MODE_ICON_STRIP_HEIGHT, 0,
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, QuarterPixels(left),
        QuarterPixels(top + (stripIndex * halfSize)),
        QuarterPixels(left + halfSize + halfSize),
        QuarterPixels(top + halfSize + (stripIndex * halfSize)),
        G_TX_RENDERTILE, 0, 0, scale, scale);
    gDPPipeSync(gfx++);
  }

  D_8022F8C5_ovl_01++;
  if (D_8022F8C5_ovl_01 == MODE_ICON_FRAME_DURATION) {
    iconIndex =
        D_8022BEBC_ovl_01[g_ChosenPlayerCount - 1][D_801323A0.modeChoice];
    D_8022F8C5_ovl_01 = 0;
    D_8022F8C0_ovl_01++;
    if (D_8022F8C0_ovl_01 >= MODE_ICON_FRAME_COUNT) {
      D_8022F8C0_ovl_01 = 0;
    }

    if (g_ModeIconAnimations[iconIndex][D_8022F8C0_ovl_01] ==
        END_OF_ICON_FRAMES) {
      D_8022F8C0_ovl_01 = 0;
    }

    D_8022F8C4_ovl_01 = D_8022F8C4_ovl_01 == 0;
    OpenEncodedAsset(
        g_ModeIconAnimations[iconIndex][D_8022F8C0_ovl_01], &asset);
    ReadEncodedAsset(D_8010526C[D_8022F8C4_ovl_01], &asset);
  }

  *displayListHead = gfx;
}

/*
 * Doesn't match: the original fills the delay slot of the rival test before
 * SetCharacterLighting with its first argument, and this leaves it empty.
 */
#ifdef NON_MATCHING
void DrawMenuCharacters(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 rival = NO_RIVAL;
  MenuMemory* memory;
  s32 characterCount;
  s32 row;
  s32 column;
  s32 layout;
  CharacterId character;
  s32 palette;
  s32 raise;
  u16 slot;
  Vec3 translation;
  Vec3 rotation;
  Vec3 scale;
  s32 unused[12];
  Mat4 transform;

  memory = func_8005AF74();
  characterCount = g_ChosenPlayerCount;
  if (D_8022BEBC_ovl_01[characterCount - 1][memory->modeChoice] ==
      MODE_ICON_GET_CHARACTER) {
    if (characterCount == 1) {
      for (row = 0; row != CHARACTER_CHOICE_ROW_COUNT; row++) {
        for (column = 0; column != CHARACTER_GRID_COLUMN_COUNT; column++) {
          if (g_CharacterGrid[row][column] < MARKED_CHARACTER_LIMIT) {
            rival = D_8022FB58_ovl_01[g_CharacterGrid[row][column] +
                MARKED_CHARACTER_OFFSET];
          }
        }
      }
    }
  }

  characterCount += (rival >= 0);
  switch (characterCount) {
    case 1:
      f_MenuCharacterScales[0] = MENU_CHARACTER_SOLO_SCALE;
      break;
    case 2:
      f_MenuCharacterScales[0] =
          (rival >= 0) ? MENU_CHARACTER_SOLO_SCALE : MENU_CHARACTER_PAIR_SCALE;
      f_MenuCharacterScales[1] =
          (rival >= 0) ? MENU_RIVAL_SCALE : MENU_CHARACTER_PAIR_SCALE;
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

  guOrtho(&D_80231F60_ovl_01, -(SCREEN_WIDTH / 2), SCREEN_WIDTH / 2,
      -(SCREEN_HEIGHT / 2), SCREEN_HEIGHT / 2, MENU_CHARACTERS_NEAR,
      MENU_CHARACTERS_FAR, MENU_CHARACTERS_ORTHO_SCALE);
  for (slot = 0; slot != characterCount; slot++) {
    if (D_8023BFC0_ovl_01[slot] > 0) {
      D_8023BFC0_ovl_01[slot]--;
    }

    if (D_8023BFC0_ovl_01[slot] == 0) {
      D_8023BFC0_ovl_01[slot] = -1;
      SetAnimationLooping(slot, FALSE);
      SetNextAnimation(slot, MENU_ANIMATION_FIDGET);
    }

    if (D_8023BFC0_ovl_01[slot] == -1) {
      if (GetAnimationEndFrame(slot, MENU_ANIMATION_FIDGET) ==
          GetAnimationFrame(slot)) {
        SetCharacterAnimation(slot, MENU_ANIMATION_STANDING);
        SetAnimationLooping(slot, TRUE);
        D_8023BFC0_ovl_01[slot] = guRandom() % FIDGET_DELAY_RANGE;
      }
    }

    if ((characterCount == 2) & (rival >= 0) && slot == RIVAL_SLOT) {
      character = rival;
      palette = 1;
    } else {
      character = (s8)g_Players[slot].characterIndex;
      palette = (s8)g_Players[slot].paletteIndex;
    }

    if (!D_801B70F0[slot]) {
      if (IsCharacterSlotBusy(slot)) {
        FreeCharacterSlot(slot);
      } else {
        LoadCharacter(slot, character + CHARACTER_COUNT, palette);
        D_801B70F0[slot] = TRUE;
        SetCharacterAnimation(slot, MENU_ANIMATION_STANDING);
        SetAnimationSpeed(slot, 1.0f);
        if ((characterCount == 2) & (rival >= 0) && slot == RIVAL_SLOT) {
          SetCharacterLighting(RIVAL_SLOT, CHARACTER_LIGHTING_BLACK);
        }
      }
    }

    layout = 0;
    if (rival < 0) {
      layout = characterCount - 1;
    }

    if ((characterCount == 2) & (rival >= 0) && slot == RIVAL_SLOT) {
      D_8022FA5C_ovl_01 -= RIVAL_SPIN_STEP;
      D_8022FA58_ovl_01 = D_8022FA5C_ovl_01;
    } else {
      D_8022FA58_ovl_01 = 0.0f;
    }

    rotation[0] = DegreesToRadians(D_8022FA54_ovl_01);
    rotation[1] = DegreesToRadians(D_8022FA58_ovl_01);
    rotation[2] = 0.0f;
    translation[0] = f_MenuCharacterPositions[layout][slot][0];
    raise = (character == CHARACTER_BABY_MARIO) ? BABY_MARIO_RAISE : 0;
    translation[1] = f_MenuCharacterPositions[layout][slot][1] + raise;
    translation[2] = f_MenuCharacterPositions[layout][slot][2];
    scale[0] = f_MenuCharacterScales[slot];
    scale[1] = f_MenuCharacterScales[slot];
    scale[2] = f_MenuCharacterScales[slot];
    BuildTransformMatrix(transform, rotation, translation, scale);
    ConvertMatrixToFixed(
        transform, &D_80231D60_ovl_01[slot][g_DrawBufferIndex]);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_PERSP);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&D_80231F60_ovl_01),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
    gSPMatrix(gfx++,
        OS_K0_TO_PHYSICAL(&D_80231D60_ovl_01[slot][g_DrawBufferIndex]),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    g_DisplayListHead = gfx;
    DrawCharacter(slot);
    *displayListHead = g_DisplayListHead;
    gfx = *displayListHead;
    AnimateCharacter(slot);
  }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", DrawMenuCharacters);
#endif

/*
 * A debug character viewer. START hides the board and turns on the controls:
 * the stick turns the character, the C buttons move it, A and B scale it, left
 * and right pick an animation, R plays it, Z with the C buttons steps through
 * it and L changes the character's kind.
 */
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
/* With Z held, the left and right C buttons step the animation a frame either
 * way, and up and down play it fast either way */
#define VIEWER_STEP_SPEED 1.0f
#define VIEWER_FAST_SPEED 4.0f
#define VIEWER_LAST_ANIMATION 34
/* From this animation on, the character's second model plays it */
#define VIEWER_FIRST_SECOND_MODEL_ANIMATION 12
#define VIEWER_SECOND_MODEL_OFFSET 18
#define VIEWER_KIND_COUNT 5
#define VIEWER_HALF_WIDTH 160.0f
#define VIEWER_HALF_HEIGHT 120.0f
#define VIEWER_NEAR 1.0f
#define VIEWER_FAR 3000.0f
#define VIEWER_PERSPECTIVE_SCALE 10.0f
#define VIEWER_BASE_SCALE 5.0f
#define VIEWER_BASE_OFFSET -64.0f
#define VIEWER_DEPTH -1000.0f

const char D_80231660_ovl_01[] = "%d:%s";
const char D_80231668_ovl_01[] = "%d:";
const char D_8023166C_ovl_01[] = "%d";
const char D_80231670_ovl_01[] = "POWER:%3d";
const char D_8023167C_ovl_01[] = "SPIN :%3d";

void func_80201574_ovl_01(Gfx** displayListHead) {
  Gfx* gfx;
  s32 index;
  s32 stickX;
  s32 stickY;
  s32 character;

  gfx = *displayListHead;

  if (g_ControllerStates[0].pressedButtons & START_BUTTON) {
    D_8022BE28_ovl_01 ^= 1;
  }

  if (D_8022BE28_ovl_01 == 0) {
    func_801FB54C_ovl_01(&gfx, PLAIN_BACKDROP_LAYERS);
  }

  SetUpTextDrawing(&gfx);
  gDPSetPrimColor(gfx++, 0, 0, 0x3F, 0x7F, 0xFF, 0xFF);
  gDPSetEnvColor(gfx++, 0x00, 0x10, 0x40, 0xFF);
  sprintf(g_PrintBuffer, D_80231660_ovl_01, D_801323A0.unk2 + 1,
      D_8022BF70_ovl_01[D_801323A0.unk2]);
  DrawLargeText(&gfx, VIEWER_NAME_X, VIEWER_NAME_Y, g_PrintBuffer);
  for (index = 0; index != g_ChosenPlayerCount; index++) {
    sprintf(g_PrintBuffer, D_80231668_ovl_01, index + 1);
    DrawTextStub(&gfx, VIEWER_PLAYER_NUMBER_X,
        VIEWER_PLAYER_TOP + (index * VIEWER_PLAYER_SPACING), g_PrintBuffer);
  }

  for (index = 0; index != D_801B7070; index++) {
    sprintf(
        g_PrintBuffer, D_80231514_ovl_01, D_8022BF70_ovl_01[D_8012F480[index]]);
    DrawTextStub(&gfx, VIEWER_PLAYER_NAME_X,
        VIEWER_PLAYER_TOP + (index * VIEWER_PLAYER_SPACING), g_PrintBuffer);
  }

  BeginShadowedText(&gfx);
  SetTextColor(&gfx, 255, 255, 255);
  sprintf(g_PrintBuffer, D_8023166C_ovl_01, D_8022BE24_ovl_01);
  DrawShadowedText(&gfx, VIEWER_ANIMATION_X, VIEWER_ANIMATION_Y, g_PrintBuffer);
  sprintf(g_PrintBuffer, D_80231670_ovl_01,
      g_Clubs[D_801323A0.unk2 * CLUB_SET_SIZE].distance);
  DrawShadowedText(&gfx, VIEWER_CLUB_X, VIEWER_POWER_Y, g_PrintBuffer);
  sprintf(g_PrintBuffer, D_8023167C_ovl_01,
      g_Clubs[D_801323A0.unk2 * CLUB_SET_SIZE].spin);
  DrawShadowedText(&gfx, VIEWER_CLUB_X, VIEWER_SPIN_Y, g_PrintBuffer);
  if (g_Clubs[D_801323A0.unk2 * CLUB_SET_SIZE].sideSpin > 0) {
    DrawShadowedText(&gfx, VIEWER_CLUB_X, VIEWER_SIDE_SPIN_Y, "DRAW");
  } else if (g_Clubs[D_801323A0.unk2 * CLUB_SET_SIZE].sideSpin < 0) {
    DrawShadowedText(&gfx, VIEWER_CLUB_X, VIEWER_SIDE_SPIN_Y, "FADE");
  } else {
    DrawShadowedText(&gfx, VIEWER_CLUB_X, VIEWER_SIDE_SPIN_Y, "STRAIGHT");
  }

  if (D_8022BE28_ovl_01 == 0) {
    ClearGameFlag(GAME_FLAG_SHOW_ANIMATION_FRAMES);
  } else {
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
      } else if (stickX < 0) {
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
      } else if (stickY < 0) {
        stickY += VIEWER_STICK_DEAD_ZONE;
        if (stickY > 0) {
          stickY = 0;
        }

        stickY /= VIEWER_STICK_DIVISOR;
      }

      D_8022FBA8_ovl_01 += stickY;
      D_8022FBAC_ovl_01 += stickX;
      if (g_ControllerStates[0].buttons & L_CBUTTONS) {
        D_8022FBB8_ovl_01 -= 1.0f;
      }

      if (g_ControllerStates[0].buttons & R_CBUTTONS) {
        D_8022FBB8_ovl_01 += 1.0f;
      }

      if (g_ControllerStates[0].buttons & U_CBUTTONS) {
        D_8022FBBC_ovl_01 += 1.0f;
      }

      if (g_ControllerStates[0].buttons & D_CBUTTONS) {
        D_8022FBBC_ovl_01 -= 1.0f;
      }

      if (g_ControllerStates[0].buttons & B_BUTTON) {
        D_8022FBB0_ovl_01 -= VIEWER_SCALE_STEP;
      }

      if (g_ControllerStates[0].buttons & A_BUTTON) {
        D_8022FBB0_ovl_01 += VIEWER_SCALE_STEP;
      }
    }

    if ((g_MenuPressedButtons & L_JPAD) && D_8022BE24_ovl_01 > 0) {
      D_8022BE24_ovl_01--;
      D_8022FBA4_ovl_01 = TRUE;
    }

    if ((g_MenuPressedButtons & R_JPAD) &&
        D_8022BE24_ovl_01 < VIEWER_LAST_ANIMATION) {
      D_8022BE24_ovl_01++;
      D_8022FBA4_ovl_01 = TRUE;
    }

    if (g_ControllerStates[0].buttons & Z_TRIG) {
      SetAnimationSpeed(D_8022FBB4_ovl_01, 0.0f);
      if (g_ControllerStates[0].repeatedButtons & L_CBUTTONS) {
        SetAnimationSpeed(D_8022FBB4_ovl_01, -VIEWER_STEP_SPEED);
      }

      if (g_ControllerStates[0].repeatedButtons & R_CBUTTONS) {
        SetAnimationSpeed(D_8022FBB4_ovl_01, VIEWER_STEP_SPEED);
      }

      if (g_ControllerStates[0].buttons & U_CBUTTONS) {
        SetAnimationSpeed(D_8022FBB4_ovl_01, VIEWER_FAST_SPEED);
      }

      if (g_ControllerStates[0].buttons & D_CBUTTONS) {
        SetAnimationSpeed(D_8022FBB4_ovl_01, -VIEWER_FAST_SPEED);
      }

      AnimateAllCharacters();
      SetAnimationSpeed(D_8022FBB4_ovl_01, 0.0f);
    }

    if (g_ControllerStates[0].buttons & R_TRIG) {
      if (!(g_ControllerStates[0].buttons & Z_TRIG)) {
        SetCharacterAnimation(D_8022FBB4_ovl_01, D_8022BE24_ovl_01);
      }

      SetAnimationSpeed(D_8022FBB4_ovl_01, 1.0f);
    }

    if ((g_MenuRepeatButtons & L_TRIG) && D_8022BE94_ovl_01 == FALSE) {
      D_8022BE81_ovl_01++;
      if (D_8022BE81_ovl_01 >= VIEWER_KIND_COUNT) {
        D_8022BE81_ovl_01 = 0;
      }

      if (D_8022BE81_ovl_01 < 0) {
        D_8022BE81_ovl_01 = VIEWER_KIND_COUNT - 1;
      }

      D_8022BE94_ovl_01 = TRUE;
    }
  }

  if (D_8022FBA0_ovl_01 != D_801323A0.unk2 || D_8022FBA4_ovl_01 == TRUE ||
      D_8022BE94_ovl_01 == TRUE) {
    if (IsCharacterSlotBusy(D_8022FBB4_ovl_01)) {
      FreeCharacterSlot(D_8022FBB4_ovl_01);
    } else {
      character = D_801323A0.unk2;
      D_8022BE94_ovl_01 = FALSE;
      D_8022FBA4_ovl_01 = FALSE;
      D_8022FBA0_ovl_01 = character;
      if (D_8022BE24_ovl_01 >= VIEWER_FIRST_SECOND_MODEL_ANIMATION) {
        LoadCharacter(D_8022FBB4_ovl_01, character + VIEWER_SECOND_MODEL_OFFSET,
            D_8022BE81_ovl_01);
      } else {
        LoadCharacter(D_8022FBB4_ovl_01, character, D_8022BE81_ovl_01);
      }

      SetCharacterAnimation(D_8022FBB4_ovl_01, D_8022BE24_ovl_01);
      SetAnimationSpeed(D_8022FBB4_ovl_01, 0.0f);
    }
  }

  AnimateAllCharacters();
  guOrtho(&D_80232030_ovl_01, -VIEWER_HALF_WIDTH, VIEWER_HALF_WIDTH,
      -VIEWER_HALF_HEIGHT, VIEWER_HALF_HEIGHT, VIEWER_NEAR, VIEWER_FAR,
      VIEWER_PERSPECTIVE_SCALE);
  {
    Vec3 translation;
    Vec3 rotation;
    Vec3 scale;
    Mat4 matrix;

    rotation[2] = 0;
    rotation[0] = DegreesToRadians(D_8022FBA8_ovl_01);
    rotation[1] = DegreesToRadians(D_8022FBAC_ovl_01);
    translation[0] = D_8022FBB8_ovl_01 + VIEWER_BASE_OFFSET;
    translation[1] = D_8022FBBC_ovl_01 + VIEWER_BASE_OFFSET;
    translation[2] = VIEWER_DEPTH;
    scale[0] = D_8022FBB0_ovl_01 + VIEWER_BASE_SCALE;
    scale[1] = D_8022FBB0_ovl_01 + VIEWER_BASE_SCALE;
    scale[2] = D_8022FBB0_ovl_01 + VIEWER_BASE_SCALE;
    BuildTransformMatrix(matrix, rotation, translation, scale);
    ConvertMatrixToFixed(matrix, &D_80231FB0_ovl_01[g_DrawBufferIndex]);
  }

  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_PERSP);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&D_80232030_ovl_01),
      G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
  gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&D_80231FB0_ovl_01[g_DrawBufferIndex]),
      G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
  gDPPipeSync(gfx++);
  g_DisplayListHead = gfx;
  DrawCharacter(D_8022FBB4_ovl_01);
  *displayListHead = g_DisplayListHead;
  gfx = *displayListHead;
}

/*
 * Draws screen 6, the course select, or the bet settings and screen 20's
 * columns as screens 20 and 22 slide between them. The course select draws
 * the water plane, then the wheel of the open courses and the next one, or
 * of the mini-golf disc's images, the front one bobbing. While the holes are
 * chosen (MAP_CAMERA_MODE_TOP_BYTE is 1) it draws the holes badges;
 * otherwise the course's birdie badge count and tournament trophies. Then
 * func_8021B618 slides the stats panel's rows the way D_8022BE89 picks; when
 * that is 3 or 4, the chosen hole starts loading once they stop.
 */
/*
 * Doesn't match: the original keeps the chosen course on the stack instead
 * of in a register, and orders the D_8022BE89 switch's branches and the
 * trophies' loads differently.
 */
#ifdef NON_MATCHING
void func_80202000_ovl_01(Gfx** displayListHead) {
  Gfx* gfx;
  u16 perspNorm;
  u16 wheelPerspNorm;
  u8 unusedStack[0x10];
  f32 iconX[COURSE_COUNT];
  f32 iconY[COURSE_COUNT];
  f32 iconScale[COURSE_COUNT];
  u8 unusedStack2[0x18];
  s32 drawOrder[COURSE_COUNT];
  s16 places[TOURNAMENT_PLACE_COUNT];
  SaveData* save;
  s32 courseCount;
  s32 choice;
  s32 spin;
  s32 texelsOffset;
  s32 angleStep;
  s32 index;
  /* Counts the sort's passes, the strips, the holes (then holds the badge
   * count's shift), the badges, the places and the trophies */
  s32 otherIndex;
  s32 key;
  s32 slot;
  s32 halfSize;
  s32 step;
  s32 stripTop;
  s32 stripOffset;
  Radians angle;
  f32 wheelX;
  f32 bob;
  void* palette;
  u8* texels;
  s16 badgeCount;
  s32 placeCount;
  s32 red;
  s32 green;
  s32 blue;
  s16 left;
  s16 top;

  save = GetSaveData();
  gfx = *displayListHead;
  if (g_MenuScreen == MENU_SCREEN_UNK6) {
    func_801FB54C_ovl_01(&gfx, COURSE_SELECT_BACK_LAYERS);
    guTranslate(&D_802321F0_ovl_01, 0.0f, 0.0f, -WATER_PLANE_DISTANCE);
    guPerspective(&D_80232230_ovl_01, &perspNorm, WATER_FIELD_OF_VIEW,
        SCREEN_ASPECT_RATIO, WHEEL_NEAR_PLANE, WHEEL_FAR_PLANE,
        WATER_PERSPECTIVE_SCALE);
    gSPPerspNormalize(gfx++, perspNorm);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_2CYCLE);
    gDPPipeSync(gfx++);
    gSPFogPosition(gfx++, WATER_FOG_START, WATER_FOG_END);
    gDPSetFogColor(gfx++, 255, 255, 255, 255);
    gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
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
    gDPSetCombineLERP(gfx++, TEXEL0, TEXEL1, PRIMITIVE_ALPHA, TEXEL1, 0, 0, 0,
        1, SHADE, 0, COMBINED, 0, 0, 0, 0, COMBINED);
    gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, WATER_BLEND_ALPHA);
    gDPLoadTextureBlock(gfx++,
        TextureAddress(ImageBody(
            ImageAt(g_MenuCourseSceneryAsset, COURSE_SELECT_WATER_OFFSET))),
        G_IM_FMT_RGBA, G_IM_SIZ_16b, WATER_TEXTURE_SIZE, WATER_TEXTURE_SIZE, 0,
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP,
        WATER_TEXTURE_MASK, WATER_TEXTURE_MASK, WATER_TEXTURE_SHIFT,
        WATER_TEXTURE_SHIFT);
    gDPLoadMultiBlock(gfx++,
        TextureAddress(ImageBody(
            ImageAt(g_MenuCourseSceneryAsset, COURSE_SELECT_LAYER_OFFSET))),
        WATER_LAYER_TMEM, WATER_LAYER_TILE, G_IM_FMT_RGBA, G_IM_SIZ_16b,
        WATER_TEXTURE_SIZE, WATER_TEXTURE_SIZE, 0, G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP, WATER_TEXTURE_MASK, WATER_TEXTURE_MASK,
        WATER_TEXTURE_SHIFT, WATER_TEXTURE_SHIFT);
    gDPPipeSync(gfx++);
    D_8022FD98_ovl_01++;
    gDPSetTileSize(gfx++, G_TX_RENDERTILE, D_8022FD98_ovl_01, 0,
        D_8022FD98_ovl_01 + WATER_TILE_SPAN, WATER_TILE_SPAN);
    gDPSetTileSize(gfx++, WATER_LAYER_TILE, -D_8022FD98_ovl_01,
        D_8022FD98_ovl_01, -D_8022FD98_ovl_01 + WATER_TILE_SPAN,
        D_8022FD98_ovl_01 + WATER_TILE_SPAN);
    gDPPipeSync(gfx++);
    gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&D_80232230_ovl_01),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
    gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&D_802321F0_ovl_01),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gDPPipeSync(gfx++);
    gSPVertex(gfx++, D_8022FD48_ovl_01, WATER_VERTEX_COUNT, 0);
    gDPPipeSync(gfx++);
    gSP1Triangle(gfx++, 0, 1, 2, 0);
    gSP1Triangle(gfx++, 4, 2, 3, 0);
    gSP1Triangle(gfx++, 0, 2, 4, 0);
    gDPPipeSync(gfx++);
    guPerspectiveF(D_80232270_ovl_01, &wheelPerspNorm, WHEEL_FIELD_OF_VIEW,
        SCREEN_ASPECT_RATIO, WHEEL_NEAR_PLANE, WHEEL_FAR_PLANE, 1.0f);
    guLookAtF(D_802322B0_ovl_01, 0.0f, COURSE_SELECT_EYE_HEIGHT,
        COURSE_SELECT_EYE_DISTANCE, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    SetCpuProjectionF(D_80232270_ovl_01, D_802322B0_ovl_01, g_Viewports);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    if (g_GameState.isMiniGolf) {
      choice = D_801323A0.unk2B;
      courseCount = MINI_GOLF_DISC_IMAGE_COUNT;
      angleStep = DEGREES_PER_TURN / MINI_GOLF_DISC_IMAGE_COUNT;
      texelsOffset = 0;
      spin = D_801323A0.unk2D;
    } else {
      courseCount = GetOpenCourseCount();
      courseCount =
          (courseCount < COURSE_COUNT) ? (courseCount + 1) : COURSE_COUNT;

      choice = D_801323A0.courseChoice;
      angleStep = DEGREES_PER_TURN / courseCount;
      spin = D_801323A0.courseSpin;
      texelsOffset = LARGE_PALETTED_TEXELS_OFFSET;
    }

    for (index = 0; index != courseCount; index++) {
      ScreenPoint point;

      angle = -DegreesToRadians((((choice - index) % courseCount) * angleStep) +
          (spin * DEGREES_PER_SPIN_STEP));
      wheelX = sinf(angle) * WHEEL_RADIUS_X;
      if (ProjectPoint(wheelX, 0.0f, cosf(angle) * WHEEL_RADIUS_Z, &point) <
          0) {
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
      for (index = otherIndex - 1;
          (index >= 0) && (iconY[drawOrder[index]] > iconY[key]); index--) {
        drawOrder[index + 1] = drawOrder[index];
      }

      drawOrder[index + 1] = key;
    }

    gDPPipeSync(gfx++);
    gDPSetDepthSource(gfx++, G_ZS_PRIM);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    D_8022FBC0_ovl_01 += FRONT_COURSE_BOB_PHASE_STEP;
    for (index = 0; index != courseCount; index++) {
      slot = drawOrder[index];
      halfSize = iconScale[slot] * COURSE_ICON_HALF_SIZE;
      step = (f32)(COURSE_ICON_STRIP_HEIGHT << 10) / halfSize;
      if (index == (courseCount - 1)) {
        bob = sinf(D_8022FBC0_ovl_01) * FRONT_COURSE_BOB_HEIGHT;
      } else {
        bob = 0.0f;
      }

      if (g_GameState.isMiniGolf == TRUE) {
        palette = D_801052C4[slot];
        /* Indexed as one row, as that matches the original better */
        texels =
            D_801049A0[0]
                      [(D_8022F8DD_ovl_01 * MINI_GOLF_DISC_IMAGE_COUNT) + slot];
      } else {
        palette = ImageBody(D_801049A0[0][slot]);
        texels = D_801049A0[0][slot];
      }

      gDPLoadTLUT_pal256(gfx++, TextureAddress(palette));
      if (choice == slot) {
        gDPPipeSync(gfx++);
        gDPSetRenderMode(
            gfx++, G_RM_AA_TEX_EDGE | Z_UPD, G_RM_AA_TEX_EDGE2 | Z_UPD);
        gDPSetPrimDepth(gfx++, CHOSEN_COURSE_DEPTH, 0);
      } else {
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
      }

      if (!g_GameState.isMiniGolf) {
        if ((slot == (courseCount - 1)) &&
            (GetOpenCourseCount() < COURSE_COUNT)) {
          gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
          gDPSetPrimColor(gfx++, 0, 0, LOCKED_COURSE_SHADE, LOCKED_COURSE_SHADE,
              LOCKED_COURSE_SHADE, 255);
        } else {
          gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
        }
      }

      stripTop = 0;
      stripOffset = texelsOffset;
      for (otherIndex = 0; otherIndex != COURSE_ICON_STRIP_COUNT;
          otherIndex++) {
        gDPLoadTextureBlock(gfx++, TextureAddress(texels + stripOffset),
            G_IM_FMT_CI, G_IM_SIZ_8b, COURSE_ICON_WIDTH,
            COURSE_ICON_STRIP_HEIGHT, 0, G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
            G_TX_NOLOD);
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(gfx++, (s32)(iconX[slot] - halfSize) << 2,
            (s32)((iconY[slot] - halfSize) + stripTop + bob) << 2,
            (s32)(iconX[slot] + halfSize) << 2,
            (s32)(iconY[slot] + stripTop + bob) << 2, G_TX_RENDERTILE, 0, 0,
            step, step);
        gDPPipeSync(gfx++);
        stripTop += halfSize;
        stripOffset += COURSE_ICON_WIDTH * COURSE_ICON_STRIP_HEIGHT;
      }

      if ((g_GameState.isMiniGolf == TRUE) && (D_801323A0.unk2B == slot)) {
        if ((spin == 0) || (D_8022FD44_ovl_01 == 0.0f)) {
          D_8022FD44_ovl_01 = iconY[slot];
        }

        func_8021CBDC_ovl_01(&gfx, D_8022FD44_ovl_01 + bob);
      }
    }

    gDPPipeSync(gfx++);
    gDPSetDepthSource(gfx++, G_ZS_PIXEL);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    if (MAP_CAMERA_MODE_TOP_BYTE == 0) {
      badgeCount = 0;
      for (otherIndex = 0; otherIndex != COURSE_HOLE_COUNT; otherIndex++) {
        if (save->awards
                .birdieBadgeCounts[D_801323A0.courseChoice][otherIndex]) {
          badgeCount++;
        }
      }

      /* Centers the count by its number of digits */
      if (badgeCount < MIN_TWO_DIGIT_WON_COUNT) {
        otherIndex = WON_COUNT_DIGIT_WIDTH;
      }

      if ((badgeCount >= MIN_TWO_DIGIT_WON_COUNT) &&
          (badgeCount < MIN_THREE_DIGIT_WON_COUNT)) {
        otherIndex = 0;
      }

      if (badgeCount >= MIN_THREE_DIGIT_WON_COUNT) {
        otherIndex = -WON_COUNT_DIGIT_WIDTH;
      }

      func_80207E28_ovl_01(&gfx, D_800FC898, otherIndex + COURSE_BADGE_ICON_X,
          COURSE_BADGE_ICON_Y);
    }

    if (MAP_CAMERA_MODE_TOP_BYTE == 1) {
      gDPPipeSync(gfx++);
      gDPPipeSync(gfx++);
      gDPSetTexturePersp(gfx++, G_TP_NONE);
      gDPPipeSync(gfx++);
      gDPSetTextureFilter(gfx++, G_TF_BILERP);
      gDPPipeSync(gfx++);
      gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
      gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImageBody(D_80106100)));
      for (otherIndex = 0; otherIndex != HOLES_BADGE_COUNT; otherIndex++) {
        if ((otherIndex == HOLES_BADGE_BET) && (D_800C2BD5 != 1)) {
          gDPPipeSync(gfx++);
          gDPLoadTextureTile_4b(gfx++,
              TextureAddress(ImageTexels(D_80106100) +
                  (otherIndex * HOLES_BADGE_IMAGE_SIZE)),
              G_IM_FMT_CI, HOLES_BADGE_SIZE, HOLES_BADGE_SIZE, 0, 0,
              HOLES_BADGE_SIZE - 1, HOLES_BADGE_SIZE - 1, 0,
              G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
              G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
          gDPPipeSync(gfx++);
          gSPScisTextureRectangle(gfx++,
              QuarterPixels(D_8022FC20_ovl_01[otherIndex].x),
              QuarterPixels(D_8022FC20_ovl_01[otherIndex].y),
              QuarterPixels(D_8022FC20_ovl_01[otherIndex].x + HOLES_BADGE_SIZE),
              QuarterPixels(D_8022FC20_ovl_01[otherIndex].y + HOLES_BADGE_SIZE),
              G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
          gDPPipeSync(gfx++);
          D_8022FD40_ovl_01 += BET_BADGE_COLOR_STEP;
          red = (s32)(sinf(D_8022FD40_ovl_01) * BET_BADGE_COLOR_SWING) +
              BET_BADGE_COLOR_MIDPOINT;
          blue = (s32)(sinf(D_8022FD40_ovl_01 + (M_PI / 2)) *
                     BET_BADGE_COLOR_SWING) +
              BET_BADGE_COLOR_MIDPOINT;
          green =
              (s32)(sinf(D_8022FD40_ovl_01 + M_PI) * BET_BADGE_COLOR_SWING) +
              BET_BADGE_COLOR_MIDPOINT;
          gDPSetPrimColor(gfx++, 0, 0, red, green, blue, 128);
          gDPSetEnvColor(gfx++, red / BET_BADGE_ENV_DIVISOR,
              green / BET_BADGE_ENV_DIVISOR, blue / BET_BADGE_ENV_DIVISOR, 255);
          gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
          gDPPipeSync(gfx++);
          gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
        } else if (D_801323A0.holesChoice == otherIndex) {
          gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
        } else {
          gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
          gDPSetPrimColor(gfx++, 0, 0, UNCHOSEN_HOLES_BADGE_SHADE,
              UNCHOSEN_HOLES_BADGE_SHADE, UNCHOSEN_HOLES_BADGE_SHADE, 255);
        }

        gDPPipeSync(gfx++);
        gDPLoadTextureTile_4b(gfx++,
            TextureAddress(ImageTexels(D_80106100) +
                (otherIndex * HOLES_BADGE_IMAGE_SIZE)),
            G_IM_FMT_CI, HOLES_BADGE_SIZE, HOLES_BADGE_SIZE, 0, 0,
            HOLES_BADGE_SIZE - 1, HOLES_BADGE_SIZE - 1, 0,
            G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
            G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(gfx++,
            QuarterPixels(D_8022FC20_ovl_01[otherIndex].x),
            QuarterPixels(D_8022FC20_ovl_01[otherIndex].y),
            QuarterPixels(D_8022FC20_ovl_01[otherIndex].x + HOLES_BADGE_SIZE),
            QuarterPixels(D_8022FC20_ovl_01[otherIndex].y + HOLES_BADGE_SIZE),
            G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
        gDPPipeSync(gfx++);
      }
    }

    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    if ((MAP_CAMERA_MODE_TOP_BYTE == 0) && !g_GameState.isMiniGolf) {
      placeCount = 0;
      for (otherIndex = 0; otherIndex != TOURNAMENT_PLACE_COUNT; otherIndex++) {
        if (g_CourseStatFigures[D_801323A0.courseChoice].hasPlace[otherIndex]) {
          places[placeCount] = otherIndex;
          placeCount++;
        }
      }

      if (D_801323A0.courseSpin == 0) {
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
          gDPLoadTLUT_pal256(
              gfx++, TextureAddress(D_800FBE38[places[otherIndex]]));
          gDPLoadTextureTile(gfx++,
              TextureAddress(D_800FBDFC[D_8022BE80_ovl_01]), G_IM_FMT_CI,
              G_IM_SIZ_8b, g_TrophySizes[D_801323A0.courseChoice].width,
              g_TrophySizes[D_801323A0.courseChoice].height, 0, 0,
              g_TrophySizes[D_801323A0.courseChoice].width - 1,
              g_TrophySizes[D_801323A0.courseChoice].height - 1, 0,
              G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
              G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
          gDPPipeSync(gfx++);
          left = D_8022FC48_ovl_01[D_801323A0.courseChoice][placeCount - 1]
                                  [otherIndex]
                                      .x;
          top = D_8022FC48_ovl_01[D_801323A0.courseChoice][placeCount - 1]
                                 [otherIndex]
                                     .y;
          gSPScisTextureRectangle(gfx++, QuarterPixels(left),
              QuarterPixels(top),
              QuarterPixels(
                  left + g_TrophySizes[D_801323A0.courseChoice].width),
              QuarterPixels(
                  top + g_TrophySizes[D_801323A0.courseChoice].height),
              G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
          gDPPipeSync(gfx++);
        }
      }

      badgeCount = 0;
      for (otherIndex = 0; otherIndex != COURSE_HOLE_COUNT; otherIndex++) {
        if (save->awards
                .birdieBadgeCounts[D_801323A0.courseChoice][otherIndex]) {
          badgeCount++;
        }
      }

      /* Centers the count by its number of digits */
      if (badgeCount < MIN_TWO_DIGIT_WON_COUNT) {
        otherIndex = WON_COUNT_DIGIT_WIDTH;
      }

      if ((badgeCount >= MIN_TWO_DIGIT_WON_COUNT) &&
          (badgeCount < MIN_THREE_DIGIT_WON_COUNT)) {
        otherIndex = 0;
      }

      if (badgeCount >= MIN_THREE_DIGIT_WON_COUNT) {
        otherIndex = -WON_COUNT_DIGIT_WIDTH;
      }

      gDPPipeSync(gfx++);
      DrawUnalignedMenuNumber(&gfx, otherIndex + COURSE_BADGE_COUNT_X,
          COURSE_BADGE_COUNT_Y, badgeCount, MENU_NUMBER_TIMES);
      gDPPipeSync(gfx++);
    }

    func_8021BE88_ovl_01(&gfx);
    DrawCourseSelectBanners(&gfx);
    if (!g_GameState.isMiniGolf &&
        (D_801323A0.courseChoice == (courseCount - 1)) &&
        (GetOpenCourseCount() < COURSE_COUNT)) {
      gDPSetPrimColor(gfx++, 0, 0, 64, 64, 64, 191);
      gDPPipeSync(gfx++);
      gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
      gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
      gDPPipeSync(gfx++);
      gSPTextureRectangle(gfx++, QuarterPixels(LOCKED_PANEL_LEFT),
          QuarterPixels(LOCKED_PANEL_TOP), QuarterPixels(LOCKED_PANEL_RIGHT),
          QuarterPixels(LOCKED_PANEL_BOTTOM), G_TX_RENDERTILE, 0, 0,
          TEXEL_STEP_ONE, TEXEL_STEP_ONE);
      gDPPipeSync(gfx++);
      SetUpGradientTextDrawing(&gfx, TRUE);
      gDPSetEnvColor(gfx++, 191, 191, 255, 255);
      gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
      SetTextSlotValue(
          GetCoursePointsNeeded(D_801323A0.courseChoice) - save->coursePoints,
          0, 0);
      SetTextSlot(LOCKED_COURSE_TEXT_X, LOCKED_COURSE_TEXT_Y,
          MESSAGE_POINTS_TO_UNLOCK, TEXT_SLOT_LARGE_FONT, 0, TEXT_FLAG_UNK20,
          255, 255, 255, 255);
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

    switch (D_8022BE89_ovl_01) {
      case 0:
        break;

      default:
        if (func_8021B618_ovl_01(&gfx, STATS_SLIDE_OUT_RIGHT) == 0) {
          D_8022BE88_ovl_01 = BET_PHASE_CHOOSING;
        }

        break;

      case MENU_UNK89_UNK3:
        if (func_8021B618_ovl_01(&gfx, STATS_SLIDE_IN_FROM_RIGHT) == 0) {
          D_8022BE88_ovl_01 = MENU_PHASE_START;
          D_8022BE89_ovl_01 = 0;
          StartLoadingChosenHole();
        }

        break;

      case MENU_UNK89_UNK4:
        if (func_8021B618_ovl_01(&gfx, STATS_SLIDE_IN_FROM_LEFT) == 0) {
          D_8022BE88_ovl_01 = MENU_PHASE_START;
          D_8022BE89_ovl_01 = 0;
          StartLoadingChosenHole();
        }

        break;
    }

    func_801FB54C_ovl_01(&gfx, COURSE_SELECT_FRONT_LAYERS);
  }

  if ((g_MenuScreen == MENU_SCREEN_UNK22) ||
      (g_MenuScreen == MENU_SCREEN_UNK20)) {
    switch (D_8022BE88_ovl_01) {
      case BET_PHASE_OPENING:
        D_8022BE84_ovl_01 += BET_SLIDE_STEP;
        D_8023BFD0_ovl_01 += BET_SLIDE_STEP;
        D_802322F0_ovl_01 += BET_SLIDE_STEP;
        if (D_8022BE84_ovl_01 >= BET_SLIDE_DISTANCE) {
          D_8022BE88_ovl_01 = BET_PHASE_CHOOSING;
          D_8022BE84_ovl_01 = 0;
          D_802322F0_ovl_01 = 0;
        }

        break;

      case BET_PHASE_CANCELED:
        D_8022BE84_ovl_01 += BET_SLIDE_STEP;
        D_802322F0_ovl_01 += BET_SLIDE_STEP;
        D_8023BFD0_ovl_01 -= BET_SLIDE_STEP;
        if (D_8022BE84_ovl_01 >= BET_SLIDE_DISTANCE) {
          D_8022BE88_ovl_01 = MENU_PHASE_START;
          D_8022BE84_ovl_01 = 0;
          D_802322F0_ovl_01 = 0;
          SetTextSlot(0, 0, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, 0,
              TEXT_FLAG_TYPED, 255, 255, 255, 255);
        }

        break;

      case BET_PHASE_CONFIRMED:
        D_8022BE84_ovl_01 += BET_SLIDE_STEP;
        D_802322F0_ovl_01 -= BET_SLIDE_STEP;
        D_802322F4_ovl_01 += BET_SLIDE_STEP;
        if (D_8022BE84_ovl_01 >= BET_SLIDE_DISTANCE) {
          D_8022BE88_ovl_01 = MENU_PHASE_CHOOSING;
          D_8022BE84_ovl_01 = 0;
          D_802322F0_ovl_01 = 0;
          D_802322F4_ovl_01 = 0;
        }

        break;

      case MENU_PHASE_CANCELED:
        D_8022BE84_ovl_01 += BET_SLIDE_STEP;
        D_802322F0_ovl_01 += BET_SLIDE_STEP;
        D_802322F4_ovl_01 += BET_SLIDE_STEP;
        if (D_8022BE84_ovl_01 >= BET_SLIDE_DISTANCE) {
          D_8022BE88_ovl_01 = BET_PHASE_CHOOSING;
          D_8022BE84_ovl_01 = 0;
          D_802322F0_ovl_01 = 0;
          D_802322F4_ovl_01 = 0;
        }

        break;

      case MENU_PHASE_CONFIRMED:
        D_8022BE84_ovl_01 += BET_SLIDE_STEP;
        D_802322F4_ovl_01 -= BET_SLIDE_STEP;
        D_8023BFD0_ovl_01 += BET_SLIDE_STEP;
        if (D_8022BE84_ovl_01 >= BET_SLIDE_DISTANCE) {
          D_8022BE88_ovl_01 = MENU_PHASE_START;
          D_8022BE84_ovl_01 = 0;
          D_802322F4_ovl_01 = 0;
          D_8023BFD0_ovl_01 = 0;
        }

        break;
    }

    func_8021B618_ovl_01(&gfx, STATS_SLIDE_OUT_RIGHT);
    if ((D_8022BE88_ovl_01 >= BET_PHASE_CHOOSING) &&
        ((D_8022BE88_ovl_01 != BET_PHASE_CANCELED) &&
            (D_8022BE88_ovl_01 != MENU_PHASE_CONFIRMED))) {
      DrawCourseSelectBanners(&gfx);
    }

    func_801FB54C_ovl_01(&gfx, COURSE_SELECT_FRONT_LAYERS);
    switch (D_8022BE88_ovl_01) {
      case BET_PHASE_OPENING:
        func_8021826C_ovl_01(
            &gfx, BET_SLIDE_DISTANCE - D_802322F0_ovl_01, 0, 0);
        break;

      case BET_PHASE_CHOOSING:
        func_8021826C_ovl_01(&gfx, 0, 0, 1);
        break;

      case BET_PHASE_CANCELED:
        func_8021826C_ovl_01(&gfx, D_802322F0_ovl_01, 0, 0);
        break;

      case BET_PHASE_CONFIRMED:
        func_8021826C_ovl_01(&gfx, D_802322F0_ovl_01, 1, 0);
        func_80219BD4_ovl_01(
            &gfx, BET_SLIDE_DISTANCE - D_802322F4_ovl_01, 0, 0);
        break;

      case MENU_PHASE_CHOOSING:
        func_80219BD4_ovl_01(&gfx, 0, 0, 1);
        break;

      case MENU_PHASE_CANCELED:
        func_8021826C_ovl_01(
            &gfx, D_802322F0_ovl_01 - BET_SLIDE_DISTANCE, 1, 0);
        func_80219BD4_ovl_01(&gfx, D_802322F4_ovl_01, 0, 0);
        break;

      case MENU_PHASE_CONFIRMED:
        func_80219BD4_ovl_01(&gfx, D_802322F4_ovl_01, 1, 0);
        break;
    }
  }

  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_80202000_ovl_01);
#endif

/*
 * DrawCourseSelectBanners draws the course select's banners of D_801B60D0: one
 * behind the course's name, and while the holes can be chosen, a wider one
 * below it behind the choice made, which squeezes the first.
 */
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
/* A HOLES_CHOICE_* value plus this is the message that explains it */
#define HOLES_CHOICE_MESSAGE_BASE 0xC3
#define HOLES_CHOICE_TEXT_SLOT 1

/*
 * Doesn't match: the original sets the text drawing's argument before the
 * mini-golf test that picks the name's colors, and keeps the course name
 * banner's strip top in one register instead of copying it.
 */
#ifdef NON_MATCHING
void DrawCourseSelectBanners(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 slide = D_8023BFD0_ovl_01;

  /* Also the course's index into the name colors, to match the original's
   * registers */
  s32 stripIndex;
  s32 stripTop;
  s32 squeeze;
  s32 cameraModeTopByte;

  if (D_8022BE88_ovl_01 == MENU_PHASE_UNK7) {
    slide = -slide;
  }

  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPLoadTLUT_pal256(gfx++, TextureAddress(ImageBody(D_801B60D0)));
  if (MAP_CAMERA_MODE_TOP_BYTE == 1) {
    for (stripIndex = 0; stripIndex != MENU_BANNER_STRIP_COUNT; stripIndex++) {
      gDPLoadTextureBlock(gfx++,
          TextureAddress(LargeImageTexels(D_801B60D0) +
              stripIndex * MENU_BANNER_WIDTH * MENU_BANNER_STRIP_HEIGHT),
          G_IM_FMT_CI, G_IM_SIZ_8b, MENU_BANNER_WIDTH, MENU_BANNER_STRIP_HEIGHT,
          0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      gDPPipeSync(gfx++);
      stripTop = HOLES_BANNER_TOP + (stripIndex * HOLES_BANNER_STRIP_HEIGHT);
      gSPScisTextureRectangle(gfx++, QuarterPixels(HOLES_BANNER_LEFT),
          QuarterPixels(slide + stripTop),
          QuarterPixels(HOLES_BANNER_LEFT + HOLES_BANNER_WIDTH),
          QuarterPixels(slide + stripTop + HOLES_BANNER_STRIP_HEIGHT),
          G_TX_RENDERTILE, 0, 0, (MENU_BANNER_WIDTH << 10) / HOLES_BANNER_WIDTH,
          (MENU_BANNER_STRIP_HEIGHT << 10) / HOLES_BANNER_STRIP_HEIGHT);
      gDPPipeSync(gfx++);
    }
  }

  squeeze = MAP_CAMERA_MODE_TOP_BYTE != 0 ? COURSE_NAME_BANNER_SQUEEZE : 0;
  for (stripIndex = 0; stripIndex != MENU_BANNER_STRIP_COUNT; stripIndex++) {
    gDPLoadTextureBlock(gfx++,
        TextureAddress(LargeImageTexels(D_801B60D0) +
            stripIndex * MENU_BANNER_WIDTH * MENU_BANNER_STRIP_HEIGHT),
        G_IM_FMT_CI, G_IM_SIZ_8b, MENU_BANNER_WIDTH, MENU_BANNER_STRIP_HEIGHT,
        0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    stripTop = COURSE_NAME_BANNER_TOP + (stripIndex * MENU_BANNER_STRIP_HEIGHT);
    gSPScisTextureRectangle(gfx++,
        QuarterPixels(COURSE_NAME_BANNER_LEFT + (squeeze / 2)),
        QuarterPixels(slide + stripTop),
        QuarterPixels(
            COURSE_NAME_BANNER_LEFT + COURSE_NAME_BANNER_WIDTH - (squeeze / 2)),
        QuarterPixels(slide + stripTop + MENU_BANNER_STRIP_HEIGHT),
        G_TX_RENDERTILE, 0, 0,
        (MENU_BANNER_WIDTH << 10) / (COURSE_NAME_BANNER_WIDTH - squeeze),
        TEXEL_STEP_ONE);
    gDPPipeSync(gfx++);
  }

  SetUpTextDrawing(&gfx);
  SetTextColor(&gfx, 255, 255, 255);
  if (!g_GameState.isMiniGolf) {
    if (D_800FF4C0 == 1) {
      sprintf(g_PrintBuffer, D_80231660_ovl_01, D_801323A0.courseChoice + 1,
          g_CourseRecords[D_801323A0.courseChoice].name);
    } else {
      sprintf(g_PrintBuffer, D_80231660_ovl_01, D_801323A0.courseChoice + 1,
          g_TournamentNames[D_801323A0.courseChoice]);
    }
  } else {
    sprintf(g_PrintBuffer, D_80231660_ovl_01, D_801323A0.unk2B + 1,
        D_8022FDEC_ovl_01[D_801323A0.unk2B]);
  }

  SetUpGradientTextDrawing(&gfx, 0);
  if (g_GameState.isMiniGolf == TRUE) {
    stripIndex = D_801323A0.unk2C + MINI_GOLF_COURSE_INDEX;
  } else {
    stripIndex = D_801323A0.courseChoice;
  }

  DrawLargeTextWithShadow(&gfx, SCREEN_WIDTH / 2, slide + COURSE_NAME_Y,
      g_PrintBuffer, g_CourseNameEnvColors[stripIndex],
      g_CourseNamePrimColors[stripIndex], TEXT_FLAG_CENTERED);
  cameraModeTopByte = MAP_CAMERA_MODE_TOP_BYTE;
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  if (cameraModeTopByte == 1) {
    gDPPipeSync(gfx++);
    SetUpTextDrawing(&gfx);
    SetTextColor(&gfx, 255, 255, 255);
    SetTextSlot(SCREEN_WIDTH / 2, slide + HOLES_CHOICE_Y,
        D_801323A0.holesChoice + HOLES_CHOICE_MESSAGE_BASE,
        TEXT_SLOT_LARGE_FONT, HOLES_CHOICE_TEXT_SLOT,
        TEXT_FLAG_CENTERED | TEXT_FLAG_UNK20, 255, 255, 255, 255);
    DrawLargeTextSlot(&gfx, HOLES_CHOICE_TEXT_SLOT);
  }

  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", DrawCourseSelectBanners);
#endif

void DrawContinueLabel(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;

  DrawShadowedText(&gfx, 96, 64, "CONTINUE");
  *displayListHead = gfx;
}

void func_80205038_ovl_01(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 choiceIndex;

  func_801FB54C_ovl_01(&gfx, BOARD_BACKDROP_LAYERS);
  gDPLoadTLUT_pal16(
      gfx++, 0, TextureAddress(ImageBody(D_801B5CD0[D_8022BE90_ovl_01])));
  for (choiceIndex = 0; choiceIndex != MAIN_CHOICE_COUNT; choiceIndex++) {
    if (D_801323A0.mainChoice != choiceIndex) {
      gDPLoadTextureBlock_4b(gfx++,
          TextureAddress(ImageTexels(D_801B5CD0[D_8022BE90_ovl_01]) +
              (choiceIndex * MAIN_CHOICE_LABEL_IMAGE_SIZE)),
          G_IM_FMT_CI, MAIN_CHOICE_LABEL_WIDTH, MAIN_CHOICE_LABEL_HEIGHT, 0,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      gDPPipeSync(gfx++);
      gSPScisTextureRectangle(gfx++,
          QuarterPixels(D_8022FDF4_ovl_01[choiceIndex][0]),
          QuarterPixels(D_8022FDF4_ovl_01[choiceIndex][1]),
          QuarterPixels(
              D_8022FDF4_ovl_01[choiceIndex][0] + MAIN_CHOICE_LABEL_WIDTH),
          QuarterPixels(
              D_8022FDF4_ovl_01[choiceIndex][1] + MAIN_CHOICE_LABEL_HEIGHT),
          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
      gDPPipeSync(gfx++);
    }
  }

  gDPLoadTLUT_pal16(
      gfx++, 0, TextureAddress(ImageBody(D_8012D420[D_8022BE8E_ovl_01])));
  choiceIndex = D_801323A0.mainChoice;
  gDPLoadTextureBlock_4b(gfx++,
      TextureAddress(ImageTexels(D_8012D420[D_8022BE8E_ovl_01]) +
          (choiceIndex * MAIN_CHOICE_LABEL_IMAGE_SIZE)),
      G_IM_FMT_CI, MAIN_CHOICE_LABEL_WIDTH, MAIN_CHOICE_LABEL_HEIGHT, 0,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
      G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
  gDPPipeSync(gfx++);
  gSPScisTextureRectangle(gfx++,
      QuarterPixels(D_8022FDF4_ovl_01[choiceIndex][0] - (D_800FE33C / 2)),
      QuarterPixels(D_8022FDF4_ovl_01[choiceIndex][1] - (D_800FE33C / 2)),
      QuarterPixels(D_8022FDF4_ovl_01[choiceIndex][0] +
          MAIN_CHOICE_LABEL_WIDTH + (D_800FE33C / 2)),
      QuarterPixels(D_8022FDF4_ovl_01[choiceIndex][1] +
          (MAIN_CHOICE_LABEL_HEIGHT + (D_800FE33C / 2))),
      G_TX_RENDERTILE, 0, 0,
      (MAIN_CHOICE_LABEL_WIDTH << 10) / (D_800FE33C + MAIN_CHOICE_LABEL_WIDTH),
      (MAIN_CHOICE_LABEL_HEIGHT << 10) /
          (D_800FE33C + MAIN_CHOICE_LABEL_HEIGHT));
  gDPPipeSync(gfx++);
  if (D_800FBDB9 == LABEL_GROWING) {
    D_800FE33C += CHOSEN_LABEL_GROWTH_STEP;
  }

  if (D_800FBDB9 == LABEL_SHRINKING) {
    D_800FE33C -= CHOSEN_LABEL_GROWTH_STEP;
  }

  if (D_800FE33C > CHOSEN_LABEL_GROWTH_LIMIT) {
    D_800FBDB9 = LABEL_SHRINKING;
  }

  if (D_800FE33C < 0) {
    D_800FBDB9 = LABEL_GROWING;
  }

  *displayListHead = gfx;
}

void func_802058B8_ovl_01(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 choiceIndex;
  s32 labelOffset;

  func_801FB54C_ovl_01(&gfx, BOARD_BACKDROP_LAYERS);
  gDPLoadTLUT_pal16(
      gfx++, 0, TextureAddress(ImageBody(D_801B5CD0[D_8022BE90_ovl_01])));
  for (choiceIndex = 0; choiceIndex != MAIN_CHOICE_WITHOUT_TRANSMIT_COUNT;
      choiceIndex++) {
    if (D_801323A0.mainChoiceWithoutTransmit != choiceIndex) {
      labelOffset = MAIN_CHOICE_FULL_OPTIONS * MAIN_CHOICE_LABEL_IMAGE_SIZE;
      if (choiceIndex < MAIN_CHOICE_OPTIONS) {
        labelOffset = choiceIndex * MAIN_CHOICE_LABEL_IMAGE_SIZE;
      }

      gDPLoadTextureBlock_4b(gfx++,
          TextureAddress(
              ImageTexels(D_801B5CD0[D_8022BE90_ovl_01]) + labelOffset),
          G_IM_FMT_CI, MAIN_CHOICE_LABEL_WIDTH, MAIN_CHOICE_LABEL_HEIGHT, 0,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      gDPPipeSync(gfx++);
      gSPScisTextureRectangle(gfx++,
          QuarterPixels(D_8022FE14_ovl_01[choiceIndex][0]),
          QuarterPixels(D_8022FE14_ovl_01[choiceIndex][1]),
          QuarterPixels(
              D_8022FE14_ovl_01[choiceIndex][0] + MAIN_CHOICE_LABEL_WIDTH),
          QuarterPixels(
              D_8022FE14_ovl_01[choiceIndex][1] + MAIN_CHOICE_LABEL_HEIGHT),
          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
      gDPPipeSync(gfx++);
    }
  }

  gDPLoadTLUT_pal16(
      gfx++, 0, TextureAddress(ImageBody(D_8012D420[D_8022BE8E_ovl_01])));
  choiceIndex = D_801323A0.mainChoiceWithoutTransmit;
  if (choiceIndex == MAIN_CHOICE_OPTIONS) {
    labelOffset = MAIN_CHOICE_FULL_OPTIONS * MAIN_CHOICE_LABEL_IMAGE_SIZE;
  } else {
    labelOffset = choiceIndex * MAIN_CHOICE_LABEL_IMAGE_SIZE;
  }

  gDPLoadTextureBlock_4b(gfx++,
      TextureAddress(ImageTexels(D_8012D420[D_8022BE8E_ovl_01]) + labelOffset),
      G_IM_FMT_CI, MAIN_CHOICE_LABEL_WIDTH, MAIN_CHOICE_LABEL_HEIGHT, 0,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
      G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
  gDPPipeSync(gfx++);
  gSPScisTextureRectangle(gfx++,
      QuarterPixels(D_8022FE14_ovl_01[choiceIndex][0] - (D_800FE33C / 2)),
      QuarterPixels(D_8022FE14_ovl_01[choiceIndex][1] - (D_800FE33C / 2)),
      QuarterPixels(D_8022FE14_ovl_01[choiceIndex][0] +
          MAIN_CHOICE_LABEL_WIDTH + (D_800FE33C / 2)),
      QuarterPixels(D_8022FE14_ovl_01[choiceIndex][1] +
          (MAIN_CHOICE_LABEL_HEIGHT + (D_800FE33C / 2))),
      G_TX_RENDERTILE, 0, 0,
      (MAIN_CHOICE_LABEL_WIDTH << 10) / (D_800FE33C + MAIN_CHOICE_LABEL_WIDTH),
      (MAIN_CHOICE_LABEL_HEIGHT << 10) /
          (D_800FE33C + MAIN_CHOICE_LABEL_HEIGHT));
  gDPPipeSync(gfx++);
  if (D_800FBDB9 == LABEL_GROWING) {
    D_800FE33C += CHOSEN_LABEL_GROWTH_STEP;
  }

  if (D_800FBDB9 == LABEL_SHRINKING) {
    D_800FE33C -= CHOSEN_LABEL_GROWTH_STEP;
  }

  if (D_800FE33C > CHOSEN_LABEL_GROWTH_LIMIT) {
    D_800FBDB9 = LABEL_SHRINKING;
  }

  if (D_800FE33C < 0) {
    D_800FBDB9 = LABEL_GROWING;
  }

  *displayListHead = gfx;
}

/* The clubhouse's room labels, 192 by 20 each, plain in D_801B5CD0 and lit in
 * D_8012D420 */
#define ROOM_LABEL_WIDTH 192
#define ROOM_LABEL_HEIGHT 20
#define ROOM_LABEL_IMAGE_SIZE (ROOM_LABEL_WIDTH * ROOM_LABEL_HEIGHT / 2)

void func_80206180_ovl_01(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 choiceIndex;
  s32 labelOffset;

  func_801FB54C_ovl_01(&gfx, BOARD_BACKDROP_LAYERS);
  gDPLoadTLUT_pal16(
      gfx++, 0, TextureAddress(ImageBody(D_801B5CD0[D_8022BE90_ovl_01])));
  for (choiceIndex = 0; choiceIndex != CLUBHOUSE_CHOICE_COUNT; choiceIndex++) {
    if (choiceIndex != D_801323A0.clubhouseChoice) {
      labelOffset = D_8022BF1C_ovl_01[choiceIndex] * ROOM_LABEL_IMAGE_SIZE;
      gDPLoadTextureBlock_4b(gfx++,
          TextureAddress(
              ImageTexels(D_801B5CD0[D_8022BE90_ovl_01]) + labelOffset),
          G_IM_FMT_CI, ROOM_LABEL_WIDTH, ROOM_LABEL_HEIGHT, 0,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      gDPPipeSync(gfx++);
      gSPScisTextureRectangle(gfx++,
          QuarterPixels(D_8022FE2C_ovl_01[choiceIndex][0]),
          QuarterPixels(D_8022FE2C_ovl_01[choiceIndex][1]),
          QuarterPixels(D_8022FE2C_ovl_01[choiceIndex][0] + ROOM_LABEL_WIDTH),
          QuarterPixels(D_8022FE2C_ovl_01[choiceIndex][1] + ROOM_LABEL_HEIGHT),
          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
      gDPPipeSync(gfx++);
    }
  }

  gDPLoadTLUT_pal16(
      gfx++, 0, TextureAddress(ImageBody(D_8012D420[D_8022BE8E_ovl_01])));
  choiceIndex = D_801323A0.clubhouseChoice;
  gDPLoadTextureBlock_4b(gfx++,
      TextureAddress(ImageTexels(D_8012D420[D_8022BE8E_ovl_01]) +
          (D_8022BF1C_ovl_01[choiceIndex] * ROOM_LABEL_IMAGE_SIZE)),
      G_IM_FMT_CI, ROOM_LABEL_WIDTH, ROOM_LABEL_HEIGHT, 0,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
      G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
  gDPPipeSync(gfx++);
  gSPScisTextureRectangle(gfx++,
      QuarterPixels(D_8022FE2C_ovl_01[choiceIndex][0] - (D_800FE33C / 2)),
      QuarterPixels(D_8022FE2C_ovl_01[choiceIndex][1] - (D_800FE33C / 2)),
      QuarterPixels(D_8022FE2C_ovl_01[choiceIndex][0] + ROOM_LABEL_WIDTH +
          (D_800FE33C / 2)),
      QuarterPixels(D_8022FE2C_ovl_01[choiceIndex][1] +
          (ROOM_LABEL_HEIGHT + (D_800FE33C / 2))),
      G_TX_RENDERTILE, 0, 0,
      (ROOM_LABEL_WIDTH << 10) / (D_800FE33C + ROOM_LABEL_WIDTH),
      (ROOM_LABEL_HEIGHT << 10) / (D_800FE33C + ROOM_LABEL_HEIGHT));
  gDPPipeSync(gfx++);
  if (D_800FBDB9 == LABEL_GROWING) {
    D_800FE33C += CHOSEN_LABEL_GROWTH_STEP;
  }

  if (D_800FBDB9 == LABEL_SHRINKING) {
    D_800FE33C -= CHOSEN_LABEL_GROWTH_STEP;
  }

  if (D_800FE33C > CHOSEN_LABEL_GROWTH_LIMIT) {
    D_800FBDB9 = LABEL_SHRINKING;
  }

  if (D_800FE33C < 0) {
    D_800FBDB9 = LABEL_GROWING;
  }

  *displayListHead = gfx;
}

void func_80206A2C_ovl_01(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;

  func_801FB54C_ovl_01(&gfx, BOARD_BACKDROP_LAYERS);
  *displayListHead = gfx;
}

const char D_802316EC_ovl_01[] = "Peach's Castle";
const char D_802316FC_ovl_01[] = "Luigi's Garden";

/* The options draw the narrow board in place of the board */
#define OPTIONS_BACKDROP_LAYERS \
  (MENU_LAYER_SCENERY | MENU_LAYER_BANNER | MENU_LAYER_LARGE_BANNER | \
      MENU_LAYER_UNK20 | MENU_LAYER_TALL_NARROW_BOARD)

/* The options screen's Mario and his shadow, in the menu image buffer */
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

/* Each option's row is this far below the last */
#define OPTION_ROW_SPACING 30

/*
 * The toggled options' values sit on banners of D_801B60D0, squeezed to 120
 * columns and 8 rows a strip. Their strips are placed from row 62 and drawn 20
 * rows lower. The chosen one's moves 2 columns left above a shadow 3 rows
 * lower.
 */
#define VALUE_BANNER_LEFT 136
#define VALUE_BANNER_WIDTH 120
#define VALUE_BANNER_ROW_TOP 62
#define VALUE_BANNER_OFFSET_Y 20
#define VALUE_BANNER_STRIP_HEIGHT 8
#define CHOSEN_BANNER_SHIFT 2
#define BANNER_SHADOW_DROP 3
#define BANNER_SHADOW_COLOR 31
#define BANNER_SHADOW_ALPHA 159
#define DIMMED_COLOR 191

/*
 * The options' labels, plain in D_801B5CD0 and lit in D_8012D420. Each label
 * is followed in its image by the option's two values, so the labels lie 60
 * rows apart; the lit values come from their own image
 */
#define OPTION_LABEL_WIDTH 128
#define OPTION_LABEL_HEIGHT 20
#define OPTION_LABEL_IMAGE_SIZE (OPTION_LABEL_WIDTH * OPTION_LABEL_HEIGHT / 2)
#define OPTION_LABEL_SPACING (OPTION_LABEL_IMAGE_SIZE * 3)
#define OPTION_LABEL_LEFT 32
#define OPTION_LABEL_TOP 84
#define OPTION_VALUE_LEFT 142
#define OPTION_TOGGLE_COUNT OPTIONS_CHOICE_ERASE_SAVE_DATA

/*
 * The arrows either side of the chosen toggle switch frames every 5 updates
 */
#define ARROW_FRAME_DURATION 5
#define ARROW_TIMER_PERIOD (GREEN_ARROW_FRAME_COUNT * ARROW_FRAME_DURATION)
#define LEFT_ARROW_LEFT 125
#define RIGHT_ARROW_LEFT 247
#define ARROW_TOP 86
#define ARROW_SHADOW_OFFSET_X 1
#define ARROW_SHADOW_OFFSET_Y 3
#define ARROW_PASS_COUNT 2

/* The sound test's lines, a song column and a sound column */
#define SONG_TEST_X 32
#define SOUND_TEST_X 168
#define TEST_NUMBER_Y 152
#define TEST_NAME_Y 162
#define SONG_NAME_X 94
#define SOUND_NAME_X 230
#define TEST_REVERB_Y 176
#define TEST_PITCH_Y 184
#define TEST_TEMPO_Y 192
/* The chosen line is yellow */
#define CHOSEN_TEXT_BLUE 0

/* The erase window, and its question in text slot 10 */
#define ERASE_WINDOW_WIDTH 240
#define ERASE_WINDOW_HEIGHT 160
#define ERASE_WINDOW_CENTER_X 160
#define ERASE_WINDOW_CENTER_Y 120
#define NO_OK_TEXT -1
#define ERASE_MESSAGE 0xC9
#define ERASE_MESSAGE_X 160
#define ERASE_MESSAGE_Y 106
#define ERASE_MESSAGE_SLOT 10
#define ERASE_MESSAGE_RED 95
#define ERASE_MESSAGE_GREEN 63
#define ERASE_MESSAGE_BLUE 255
#define ERASE_MESSAGE_PALETTE_RED 255
#define ERASE_MESSAGE_PALETTE_GREEN 127
#define ERASE_MESSAGE_PALETTE_BLUE 127

void func_80226A58_ovl_01(Gfx** displayListHead, s32 width, s32 height,
    s32 centerX, s32 centerY, s32 okTextShown, s32 chosenButton);

/*
 * Draws the options screen: Mario and his shadow, each option's label, the
 * toggles' values on their banners with arrows beside the chosen one, the
 * sound test's lines when game flag 0x55 is set, and the erase window while it
 * is up
 */
/*
 * Doesn't match: the original adds each value banner strip's row in both
 * branches without masking it to a byte
 */
#ifdef NON_MATCHING
void func_80206A68_ovl_01(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  MenuMemory* memory = func_8005AF74();
  u8* mario = GetMenuImageBuffer();
  u8* arrows = (u8*)GetMenuImageBuffer() + OPTIONS_ARROWS_OFFSET;
  u8* shadow = GetMenuImageBuffer();
  /* Counts the options' rows, then the arrows' passes */
  s32 loopIndex;
  s32 stripIndex;
  u8 stripTop;
  s32 valueOffset;
  s32 shiftX;
  s32 shiftY;
  s32 rowTop;
  s32 frameIndex;

  func_801FB54C_ovl_01(&gfx, OPTIONS_BACKDROP_LAYERS);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);

  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_NONE);
  gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, MARIO_SHADOW_ALPHA);
  gDPSetCombineLERP(gfx++, 0, 0, 0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0, 0, 0, 0,
      PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0);
  gDPLoadTextureTile_4b(gfx++,
      TextureAddress(ImageBody(shadow + OPTIONS_SHADOW_OFFSET)), G_IM_FMT_I,
      MARIO_SHADOW_WIDTH, MARIO_SHADOW_HEIGHT, 0, 0, MARIO_SHADOW_WIDTH - 1,
      MARIO_SHADOW_HEIGHT - 1, 0, G_TX_NOMIRROR | G_TX_WRAP,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
      G_TX_NOLOD);
  DrawTextureRect(&gfx, QuarterPixels(MARIO_SHADOW_LEFT),
      QuarterPixels(MARIO_SHADOW_TOP),
      QuarterPixels(MARIO_SHADOW_LEFT + MARIO_SHADOW_WIDTH),
      QuarterPixels(MARIO_SHADOW_TOP + MARIO_SHADOW_HEIGHT), G_TX_RENDERTILE, 0,
      0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);

  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  LoadLargePalette(&gfx, ImageBody(mario));
  for (stripIndex = 0; stripIndex != MARIO_STRIP_COUNT; stripIndex++) {
    gDPLoadTextureTile(gfx++, TextureAddress(LargeImageTexels(mario)),
        G_IM_FMT_CI, G_IM_SIZ_8b, MARIO_WIDTH, MARIO_HEIGHT, 0,
        stripIndex * MARIO_STRIP_HEIGHT, MARIO_WIDTH - 1,
        (stripIndex * MARIO_STRIP_HEIGHT) + (MARIO_STRIP_HEIGHT - 1), 0,
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    DrawTextureRect(&gfx, QuarterPixels(MARIO_LEFT),
        QuarterPixels(MARIO_TOP + (stripIndex * MARIO_STRIP_HEIGHT)),
        QuarterPixels(MARIO_LEFT + MARIO_WIDTH),
        QuarterPixels(
            MARIO_TOP + MARIO_STRIP_HEIGHT + (stripIndex * MARIO_STRIP_HEIGHT)),
        G_TX_RENDERTILE, 0, TexelCoordinate(stripIndex * MARIO_STRIP_HEIGHT),
        TEXEL_STEP_ONE, TEXEL_STEP_ONE);
  }

  gDPLoadTLUT_pal256(gfx++, TextureAddress(ImageBody(D_801B60D0)));
  gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, DIMMED_COLOR);
  for (loopIndex = 0; loopIndex != OPTION_TOGGLE_COUNT; loopIndex++) {
    for (stripIndex = 0; stripIndex != MENU_BANNER_STRIP_COUNT; stripIndex++) {
      LoadTextureBlock(&gfx,
          LargeImageTexels(D_801B60D0) +
              (stripIndex * MENU_BANNER_WIDTH * MENU_BANNER_STRIP_HEIGHT),
          G_IM_FMT_CI, G_IM_SIZ_8b, MENU_BANNER_WIDTH, MENU_BANNER_STRIP_HEIGHT,
          0, 0, 0, 0, 0, 0, 0);
      if (D_801323A0.optionsChoice == loopIndex) {
        stripTop = (loopIndex * OPTION_ROW_SPACING) +
            ((stripIndex * VALUE_BANNER_STRIP_HEIGHT) + VALUE_BANNER_ROW_TOP);
        gDPSetPrimColor(gfx++, 0, 0, BANNER_SHADOW_COLOR, BANNER_SHADOW_COLOR,
            BANNER_SHADOW_COLOR, BANNER_SHADOW_ALPHA);
        gDPSetCombineLERP(gfx++, 0, 0, 0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0, 0,
            0, 0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0);
        DrawTextureRect(&gfx, QuarterPixels(VALUE_BANNER_LEFT),
            QuarterPixels(
                stripTop + VALUE_BANNER_OFFSET_Y + BANNER_SHADOW_DROP),
            QuarterPixels(VALUE_BANNER_LEFT + VALUE_BANNER_WIDTH),
            QuarterPixels(stripTop + VALUE_BANNER_OFFSET_Y +
                BANNER_SHADOW_DROP + VALUE_BANNER_STRIP_HEIGHT),
            G_TX_RENDERTILE, 0, 0,
            (MENU_BANNER_WIDTH << 10) / VALUE_BANNER_WIDTH,
            (MENU_BANNER_STRIP_HEIGHT << 10) / VALUE_BANNER_STRIP_HEIGHT);
        gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
        DrawTextureRect(&gfx,
            QuarterPixels(VALUE_BANNER_LEFT - CHOSEN_BANNER_SHIFT),
            QuarterPixels(stripTop + VALUE_BANNER_OFFSET_Y),
            QuarterPixels(
                VALUE_BANNER_LEFT + VALUE_BANNER_WIDTH - CHOSEN_BANNER_SHIFT),
            QuarterPixels(
                stripTop + VALUE_BANNER_OFFSET_Y + VALUE_BANNER_STRIP_HEIGHT),
            G_TX_RENDERTILE, 0, 0,
            (MENU_BANNER_WIDTH << 10) / VALUE_BANNER_WIDTH,
            (MENU_BANNER_STRIP_HEIGHT << 10) / VALUE_BANNER_STRIP_HEIGHT);
      } else {
        stripTop = (loopIndex * OPTION_ROW_SPACING) +
            ((stripIndex * VALUE_BANNER_STRIP_HEIGHT) + VALUE_BANNER_ROW_TOP);
        gDPSetCombineLERP(gfx++, PRIMITIVE, 0, TEXEL0, 0, 0, 0, 0, TEXEL0,
            PRIMITIVE, 0, TEXEL0, 0, 0, 0, 0, TEXEL0);
        gDPSetPrimColor(
            gfx++, 0, 0, DIMMED_COLOR, DIMMED_COLOR, DIMMED_COLOR, 255);
        DrawTextureRect(&gfx, QuarterPixels(VALUE_BANNER_LEFT),
            QuarterPixels(stripTop + VALUE_BANNER_OFFSET_Y),
            QuarterPixels(VALUE_BANNER_LEFT + VALUE_BANNER_WIDTH),
            QuarterPixels(
                stripTop + VALUE_BANNER_OFFSET_Y + VALUE_BANNER_STRIP_HEIGHT),
            G_TX_RENDERTILE, 0, 0,
            (MENU_BANNER_WIDTH << 10) / VALUE_BANNER_WIDTH,
            (MENU_BANNER_STRIP_HEIGHT << 10) / VALUE_BANNER_STRIP_HEIGHT);
      }
    }
  }

  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPLoadTLUT_pal16(
      gfx++, 0, TextureAddress(ImageBody(D_801B5CD0[D_8022BE90_ovl_01])));
  for (loopIndex = 0; loopIndex != OPTIONS_CHOICE_COUNT; loopIndex++) {
    if (D_801323A0.optionsChoice != loopIndex) {
      LoadNibbleTextureBlock(&gfx,
          ImageTexels(D_801B5CD0[D_8022BE90_ovl_01]) +
              (loopIndex * OPTION_LABEL_SPACING),
          G_IM_FMT_CI, OPTION_LABEL_WIDTH, OPTION_LABEL_HEIGHT, 0, 0, 0, 0, 0,
          0, 0);
      DrawTextureRect(&gfx, QuarterPixels(OPTION_LABEL_LEFT),
          QuarterPixels(OPTION_LABEL_TOP + (loopIndex * OPTION_ROW_SPACING)),
          QuarterPixels(OPTION_LABEL_LEFT + OPTION_LABEL_WIDTH),
          QuarterPixels(OPTION_LABEL_TOP + OPTION_LABEL_HEIGHT +
              (loopIndex * OPTION_ROW_SPACING)),
          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    }
  }

  LoadSmallPalette(&gfx, 0, ImageBody(D_8012D420[D_8022BE8E_ovl_01]));
  for (loopIndex = 0; loopIndex != OPTIONS_CHOICE_COUNT; loopIndex++) {
    if (D_801323A0.optionsChoice == loopIndex) {
      LoadNibbleTextureBlock(&gfx,
          ImageTexels(D_8012D420[D_8022BE8E_ovl_01]) +
              (loopIndex * OPTION_LABEL_SPACING),
          G_IM_FMT_CI, OPTION_LABEL_WIDTH, OPTION_LABEL_HEIGHT, 0, 0, 0, 0, 0,
          0, 0);
      gDPPipeSync(gfx++);
      DrawTextureRect(&gfx, QuarterPixels(OPTION_LABEL_LEFT),
          QuarterPixels(OPTION_LABEL_TOP + (loopIndex * OPTION_ROW_SPACING)),
          QuarterPixels(OPTION_LABEL_LEFT + OPTION_LABEL_WIDTH),
          QuarterPixels(OPTION_LABEL_TOP + OPTION_LABEL_HEIGHT +
              (loopIndex * OPTION_ROW_SPACING)),
          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    }
  }

  gDPSetCombineLERP(gfx++, PRIMITIVE, 0, TEXEL0, 0, 0, 0, 0, TEXEL0, PRIMITIVE,
      0, TEXEL0, 0, 0, 0, 0, TEXEL0);
  gDPLoadTLUT_pal16(gfx++, 0,
      TextureAddress(ImageBody(
          ImageAt(D_8012D420[D_8022BE8E_ovl_01], OPTION_VALUES_OFFSET))));
  for (loopIndex = 0; loopIndex != OPTION_TOGGLE_COUNT; loopIndex++) {
    /* Each toggle has two values, the second shown while its bit is set */
    valueOffset = loopIndex * 2 * OPTION_LABEL_IMAGE_SIZE;
    switch (loopIndex) {
      case OPTIONS_CHOICE_MUSIC:
        if (D_801323A0.optionBits & OPTION_BIT_MUSIC_OFF) {
          valueOffset += OPTION_LABEL_IMAGE_SIZE;
        }

        break;
      case OPTIONS_CHOICE_MONO:
        if (D_801323A0.optionBits & OPTION_BIT_MONO) {
          valueOffset += OPTION_LABEL_IMAGE_SIZE;
        }

        break;
      case OPTIONS_CHOICE_CAMERA:
        if (D_801323A0.optionBits & OPTION_BIT_SIMPLE_CAMERA) {
          valueOffset += OPTION_LABEL_IMAGE_SIZE;
        }

        break;
    }

    LoadNibbleTextureBlock(&gfx,
        ImageTexels(
            ImageAt(D_8012D420[D_8022BE8E_ovl_01], OPTION_VALUES_OFFSET)) +
            valueOffset,
        G_IM_FMT_CI, OPTION_LABEL_WIDTH, OPTION_LABEL_HEIGHT, 0, 0, 0, 0, 0, 0,
        0);
    if (D_801323A0.optionsChoice == loopIndex) {
      gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
    } else {
      gDPSetPrimColor(
          gfx++, 0, 0, DIMMED_COLOR, DIMMED_COLOR, DIMMED_COLOR, 255);
    }

    DrawTextureRect(&gfx, QuarterPixels(OPTION_VALUE_LEFT),
        QuarterPixels(OPTION_LABEL_TOP + (loopIndex * OPTION_ROW_SPACING)),
        QuarterPixels(OPTION_VALUE_LEFT + OPTION_LABEL_WIDTH),
        QuarterPixels(OPTION_LABEL_TOP + OPTION_LABEL_HEIGHT +
            (loopIndex * OPTION_ROW_SPACING)),
        G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
  }

  gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImageBody(arrows)));
  if (D_801323A0.optionsChoice != OPTIONS_CHOICE_ERASE_SAVE_DATA) {
    D_8022FEFC_ovl_01 = (D_8022FEFC_ovl_01 + 1) % ARROW_TIMER_PERIOD;
    frameIndex = D_8022FEFC_ovl_01 < ARROW_FRAME_DURATION;
    LoadNibbleTextureBlock(&gfx,
        ImageTexels(arrows) + (frameIndex * GREEN_ARROW_FRAME_SIZE),
        G_IM_FMT_CI, GREEN_ARROW_SIZE, GREEN_ARROW_FRAME_HEIGHT, 0, 0, 0, 0, 0,
        0, 0);
    /* The first pass draws the arrows' shadows */
    for (loopIndex = 0; loopIndex != ARROW_PASS_COUNT; loopIndex++) {
      shiftX = 0;
      if (loopIndex == 0) {
        shiftX = ARROW_SHADOW_OFFSET_X;
        shiftY = ARROW_SHADOW_OFFSET_Y;
        gDPSetPrimColor(gfx++, 0, 0, BANNER_SHADOW_COLOR, BANNER_SHADOW_COLOR,
            BANNER_SHADOW_COLOR, BANNER_SHADOW_ALPHA);
        gDPSetCombineLERP(gfx++, 0, 0, 0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0, 0,
            0, 0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0);
      } else {
        shiftY = 0;
        gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
      }

      rowTop = D_801323A0.optionsChoice * OPTION_ROW_SPACING;
      DrawTextureRect(&gfx, QuarterPixels(LEFT_ARROW_LEFT + shiftX),
          QuarterPixels(rowTop + ARROW_TOP + shiftY),
          QuarterPixels(LEFT_ARROW_LEFT + GREEN_ARROW_SIZE + shiftX),
          QuarterPixels(rowTop + ARROW_TOP + GREEN_ARROW_SIZE + shiftY),
          G_TX_RENDERTILE, 0, TexelCoordinate(GreenArrowRow(MENU_ARROW_LEFT)),
          TEXEL_STEP_ONE, TEXEL_STEP_ONE);
      rowTop = D_801323A0.optionsChoice * OPTION_ROW_SPACING;
      DrawTextureRect(&gfx, QuarterPixels(RIGHT_ARROW_LEFT + shiftX),
          QuarterPixels(rowTop + ARROW_TOP + shiftY),
          QuarterPixels(RIGHT_ARROW_LEFT + GREEN_ARROW_SIZE + shiftX),
          QuarterPixels(rowTop + ARROW_TOP + GREEN_ARROW_SIZE + shiftY),
          G_TX_RENDERTILE, 0, TexelCoordinate(GreenArrowRow(MENU_ARROW_RIGHT)),
          TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    }
  }

  if (IsGameFlagSet(GAME_FLAG_UNK55)) {
    BeginShadowedText(&gfx);
    SetTextColor(&gfx, 255, 255, 255);
    if (D_801323A0.optionsChoice == OPTIONS_CHOICE_SONG) {
      SetTextColor(&gfx, 255, 255, CHOSEN_TEXT_BLUE);
    } else {
      SetTextColor(&gfx, 255, 255, 255);
    }

    if (f_SoundTestSong == NO_SONG) {
      /* The original passes the song, though STOP prints no number */
      sprintf(g_PrintBuffer, "BGM TEST : STOP", f_SoundTestSong);
      DrawShadowedText(&gfx, SONG_TEST_X, TEST_NUMBER_Y, g_PrintBuffer);
    } else {
      sprintf(g_PrintBuffer, "BGM TEST : %2d", f_SoundTestSong);
      DrawShadowedText(&gfx, SONG_TEST_X, TEST_NUMBER_Y, g_PrintBuffer);
      sprintf(g_PrintBuffer, D_80231514_ovl_01,
          f_SoundTestSongNames[f_SoundTestSong]);
      DrawShadowedText(&gfx, SONG_NAME_X, TEST_NAME_Y, g_PrintBuffer);
    }

    if (D_801323A0.optionsChoice == OPTIONS_CHOICE_SONG_REVERB) {
      SetTextColor(&gfx, 255, 255, CHOSEN_TEXT_BLUE);
    } else {
      SetTextColor(&gfx, 255, 255, 255);
    }

    sprintf(g_PrintBuffer, " REVERB: %3d", f_SoundTestSongReverb);
    DrawShadowedText(&gfx, SONG_TEST_X, TEST_REVERB_Y, g_PrintBuffer);
    if (D_801323A0.optionsChoice == OPTIONS_CHOICE_SONG_FREQUENCY) {
      SetTextColor(&gfx, 255, 255, CHOSEN_TEXT_BLUE);
    } else {
      SetTextColor(&gfx, 255, 255, 255);
    }

    sprintf(g_PrintBuffer, " PICH  : %3.1f", f_SoundTestSongFrequency);
    DrawShadowedText(&gfx, SONG_TEST_X, TEST_PITCH_Y, g_PrintBuffer);
    if (D_801323A0.optionsChoice == OPTIONS_CHOICE_SONG_TEMPO) {
      SetTextColor(&gfx, 255, 255, CHOSEN_TEXT_BLUE);
    } else {
      SetTextColor(&gfx, 255, 255, 255);
    }

    sprintf(g_PrintBuffer, " TEMPO : %3d", f_SoundTestSongTempo);
    DrawShadowedText(&gfx, SONG_TEST_X, TEST_TEMPO_Y, g_PrintBuffer);
    if (D_801323A0.optionsChoice == OPTIONS_CHOICE_SOUND) {
      SetTextColor(&gfx, 255, 255, CHOSEN_TEXT_BLUE);
    } else {
      SetTextColor(&gfx, 255, 255, 255);
    }

    sprintf(g_PrintBuffer, "SE TEST : %3d", f_SoundTestSound);
    DrawShadowedText(&gfx, SOUND_TEST_X, TEST_NUMBER_Y, g_PrintBuffer);
    sprintf(g_PrintBuffer, D_80231514_ovl_01,
        f_SoundTestSoundNames[f_SoundTestSound]);
    DrawShadowedText(&gfx, SOUND_NAME_X, TEST_NAME_Y, g_PrintBuffer);
    if (D_801323A0.optionsChoice == OPTIONS_CHOICE_SOUND_REVERB) {
      SetTextColor(&gfx, 255, 255, CHOSEN_TEXT_BLUE);
    } else {
      SetTextColor(&gfx, 255, 255, 255);
    }

    sprintf(g_PrintBuffer, " REVERB: %3d", f_SoundTestSoundReverb);
    DrawShadowedText(&gfx, SOUND_TEST_X, TEST_REVERB_Y, g_PrintBuffer);
    if (D_801323A0.optionsChoice == OPTIONS_CHOICE_SOUND_FREQUENCY) {
      SetTextColor(&gfx, 255, 255, CHOSEN_TEXT_BLUE);
    } else {
      SetTextColor(&gfx, 255, 255, 255);
    }

    sprintf(g_PrintBuffer, " PICH  : %3.1f", f_SoundTestSoundFrequency);
    DrawShadowedText(&gfx, SOUND_TEST_X, TEST_PITCH_Y, g_PrintBuffer);
  }

  if (D_800FBDB9 == LABEL_GROWING) {
    D_800FE33C += CHOSEN_LABEL_GROWTH_STEP;
  }

  if (D_800FBDB9 == LABEL_SHRINKING) {
    D_800FE33C -= CHOSEN_LABEL_GROWTH_STEP;
  }

  if (D_800FE33C > CHOSEN_LABEL_GROWTH_LIMIT) {
    D_800FBDB9 = LABEL_SHRINKING;
  }

  if (D_800FE33C < 0) {
    D_800FBDB9 = LABEL_GROWING;
  }

  if (f_IsEraseWindowUp == TRUE) {
    func_80226A58_ovl_01(&gfx, ERASE_WINDOW_WIDTH, ERASE_WINDOW_HEIGHT,
        ERASE_WINDOW_CENTER_X, ERASE_WINDOW_CENTER_Y, NO_OK_TEXT,
        memory->eraseChoice);
    if (IsMenuWindowOpen()) {
      SetTextPaletteColor(ERASE_MESSAGE_PALETTE_RED,
          ERASE_MESSAGE_PALETTE_GREEN, ERASE_MESSAGE_PALETTE_BLUE, 0);
      SetTextSlot(ERASE_MESSAGE_X, ERASE_MESSAGE_Y, ERASE_MESSAGE,
          TEXT_SLOT_LARGE_FONT, ERASE_MESSAGE_SLOT, TEXT_FLAG_CENTERED,
          ERASE_MESSAGE_RED, ERASE_MESSAGE_GREEN, ERASE_MESSAGE_BLUE, 255);
    } else {
      SetTextSlot(0, 0, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, ERASE_MESSAGE_SLOT,
          TEXT_FLAG_TYPED, 255, 255, 255, 255);
    }
  }

  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_80206A68_ovl_01);
#endif

void func_80207D08_ovl_01(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 caddyIndex;

  func_801FB54C_ovl_01(&gfx, BOARD_BACKDROP_LAYERS);
  BeginShadowedText(&gfx);
  SetTextColor(&gfx, 255, 255, 255);
  DrawShadowedText(
      &gfx, CADDY_SELECT_TITLE_X, CADDY_SELECT_TITLE_Y, D_8023176C_ovl_01);
  for (caddyIndex = 0; caddyIndex != CADDY_COUNT; caddyIndex++) {
    if (D_801323A0.unk19 == caddyIndex) {
      SetTextColor(&gfx, 255, 255, 0);
    } else {
      SetTextColor(&gfx, 255, 255, 255);
    }

    DrawShadowedText(&gfx, CADDY_SELECT_NAME_X,
        (caddyIndex * CADDY_SELECT_LINE_HEIGHT) + CADDY_SELECT_FIRST_Y,
        D_8022C0D0_ovl_01[caddyIndex]);
  }

  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}

/* Defined after the function, as the INCLUDE_ASM before it leaves the assembler
 * in .text */
const char D_8023176C_ovl_01[] = "CADDY SELECT";

void func_80207E18_ovl_01(Gfx** displayListHead) {}

void func_80207E20_ovl_01(void) {}

void func_80207E28_ovl_01(Gfx** displayListHead, void* image, s32 x, s32 y) {
  Gfx* gfx = *displayListHead;
  s32 unused[8];

  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImageBody(image)));
  gDPPipeSync(gfx++);
  gDPLoadTextureTile_4b(gfx++,
      TextureAddress(
          ImageTexels(image) + (D_8022F8C8_ovl_01 * ANIMATED_ICON_IMAGE_SIZE)),
      G_IM_FMT_CI, ANIMATED_ICON_SIZE, ANIMATED_ICON_SIZE, 0, 0,
      ANIMATED_ICON_SIZE - 1, ANIMATED_ICON_SIZE - 1, 0,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
      G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
  gDPPipeSync(gfx++);
  gSPScisTextureRectangle(gfx++, QuarterPixels(x), QuarterPixels(y),
      QuarterPixels(x + ANIMATED_ICON_SIZE),
      QuarterPixels(y + ANIMATED_ICON_SIZE), G_TX_RENDERTILE, 0, 0,
      TEXEL_STEP_ONE, TEXEL_STEP_ONE);
  gDPPipeSync(gfx++);
  D_8022F8CD_ovl_01++;
  if (D_8022F8CD_ovl_01 == ANIMATED_ICON_FRAME_DURATION) {
    D_8022F8CD_ovl_01 = 0;
    D_8022F8C8_ovl_01++;
    if (D_8022F8C8_ovl_01 >= ANIMATED_ICON_FRAME_COUNT) {
      D_8022F8C8_ovl_01 = 0;
    }
  }

  *displayListHead = gfx;
}

/*
 * Doesn't match: the two growths swap registers ($t1 and $t8), and the
 * right and bottom edges add their constants last.
 */
#ifdef NON_MATCHING
void func_802081E8_ovl_01(Gfx** displayListHead, s32 left, s32 top,
    u8 starCount, u8 enlargesChosenHole) {
  Gfx* gfx = *displayListHead;
  EncodedAsset asset;
  void* image = D_801B93E0[D_8022F8CC_ovl_01];
  u8 starIndex = 0;
  s32 growWidth = 0;
  s32 growHeight;

  gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImageBody(image)));
  gDPLoadTextureBlock_4b(gfx++, TextureAddress(ImageTexels(image)), G_IM_FMT_CI,
      STAR_WIDTH, STAR_HEIGHT, 0, G_TX_NOMIRROR | G_TX_WRAP,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
      G_TX_NOLOD);
  gDPPipeSync(gfx++);
  if (starCount > MAX_STAR_COUNT) {
    starCount = MAX_STAR_COUNT;
  }

  if (starCount == 0) {
    starCount = 1;
  }

  for (; starIndex < starCount; starIndex++) {
    growWidth = 0;
    if (enlargesChosenHole == TRUE) {
      growHeight = 0;
      if (starIndex == D_801323A0.holeIndex) {
        growWidth = CHOSEN_STAR_GROWTH;
        growHeight = CHOSEN_STAR_GROWTH;
      }
    } else {
      growHeight = 0;
    }

    gSPScisTextureRectangle(gfx++,
        QuarterPixels(left + starIndex * STAR_WIDTH - (growWidth / 2)),
        QuarterPixels(top - (growHeight / 2)),
        QuarterPixels(
            left + starIndex * STAR_WIDTH + (growWidth / 2) + STAR_WIDTH),
        QuarterPixels(top + (growHeight / 2) + STAR_HEIGHT), G_TX_RENDERTILE, 0,
        0, (STAR_WIDTH << 10) / (growWidth + STAR_WIDTH),
        (STAR_HEIGHT << 10) / (growHeight + STAR_HEIGHT));
    gDPPipeSync(gfx++);
  }

  D_8022F8CD_ovl_01++;
  if (D_8022F8CD_ovl_01 == ANIMATED_ICON_FRAME_DURATION) {
    D_8022F8CD_ovl_01 = 0;
    D_8022F8C8_ovl_01++;
    if (D_8022F8C8_ovl_01 >= STAR_FRAME_COUNT) {
      D_8022F8C8_ovl_01 = 0;
    }

    D_8022F8CC_ovl_01 = D_8022F8CC_ovl_01 == 0;
    OpenEncodedAsset(D_8022F848_ovl_01[D_8022F8C8_ovl_01], &asset);
    ReadEncodedAsset(D_801B93E0[D_8022F8CC_ovl_01], &asset);
  }

  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_802081E8_ovl_01);
#endif

/*
 * func_8020879C draws the course's Ring Shot holes as stars on a hexagon
 * around a center, gold where the hole's star was won and silver where not,
 * with the chosen hole larger. Nothing calls it.
 */
#define RING_SHOT_STAR_RADIUS 56
#define RING_SHOT_STAR_WIDTH 32
#define RING_SHOT_STAR_HEIGHT 24
#define RING_SHOT_STAR_GROWTH 16
#define RING_SHOT_STAR_FRAME_DURATION 2
#define GOLD_STAR_PALETTE 0
#define SILVER_STAR_PALETTE 1
/* The silver star's texels follow the gold star's in texture memory */
#define SILVER_STAR_TMEM 128
#define SILVER_STAR_TILE 1

void func_8020879C_ovl_01(Gfx** displayListHead, s32 centerX, s32 centerY) {
  EncodedAsset asset;
  SaveData* save = GetSaveData();
  Gfx* gfx = *displayListHead;
  f32 starPositions[RING_SHOT_HOLE_COUNT][2] = {
      {0.0f, -1.0f},
      {0.89f, -0.45f},
      {0.89f, 0.45f},
      {0.0f, 1.0f},
      {-0.89f, 0.45f},
      {-0.89f, -0.45f},
  };
  void* image;
  s32 tile;
  u8 holeIndex;
  s32 growWidth;
  s32 growHeight;
  s32 left;
  s32 top;

  image = D_8010526C[D_8022F8C4_ovl_01];
  gDPLoadTLUT_pal16(gfx++, GOLD_STAR_PALETTE, TextureAddress(ImageBody(image)));
  gDPLoadTextureBlock_4b(gfx++, TextureAddress(ImageTexels(image)), G_IM_FMT_CI,
      RING_SHOT_STAR_WIDTH, RING_SHOT_STAR_HEIGHT, GOLD_STAR_PALETTE,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
      G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
  gDPPipeSync(gfx++);
  image = D_800FF180[D_8022F8C4_ovl_01];
  gDPLoadTLUT_pal16(
      gfx++, SILVER_STAR_PALETTE, TextureAddress(ImageBody(image)));
  gDPLoadMultiBlock_4b(gfx++, TextureAddress(ImageTexels(image)),
      SILVER_STAR_TMEM, SILVER_STAR_TILE, G_IM_FMT_CI, RING_SHOT_STAR_WIDTH,
      RING_SHOT_STAR_HEIGHT, SILVER_STAR_PALETTE, G_TX_NOMIRROR | G_TX_WRAP,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
      G_TX_NOLOD);
  gDPPipeSync(gfx++);
  for (holeIndex = 0; holeIndex < RING_SHOT_HOLE_COUNT; holeIndex++) {
    growWidth = 0;
    if (holeIndex == D_801323A0.holeIndex) {
      growWidth = RING_SHOT_STAR_GROWTH;
      growHeight = RING_SHOT_STAR_GROWTH;
    } else {
      growHeight = 0;
    }

    left = starPositions[holeIndex][0] * RING_SHOT_STAR_RADIUS;
    top = starPositions[holeIndex][1] * RING_SHOT_STAR_RADIUS;
    left += centerX - (RING_SHOT_STAR_WIDTH / 2);
    top += centerY - (RING_SHOT_STAR_HEIGHT / 2);

    /* A hole without its star draws from the silver star's tile */
    tile = save->hasRingShotStar[D_801323A0.courseIndex][holeIndex] == FALSE;
    gSPScisTextureRectangle(gfx++, QuarterPixels(left - (growWidth / 2)),
        QuarterPixels(top - (growHeight / 2)),
        QuarterPixels(left + RING_SHOT_STAR_WIDTH + (growWidth / 2)),
        QuarterPixels(top + RING_SHOT_STAR_HEIGHT + (growHeight / 2)), tile, 0,
        0, (RING_SHOT_STAR_WIDTH << 10) / (growWidth + RING_SHOT_STAR_WIDTH),
        (RING_SHOT_STAR_HEIGHT << 10) / (growHeight + RING_SHOT_STAR_HEIGHT));
    gDPPipeSync(gfx++);
  }

  D_8022F8C5_ovl_01++;
  if (D_8022F8C5_ovl_01 == RING_SHOT_STAR_FRAME_DURATION) {
    D_8022F8C5_ovl_01 = 0;
    D_8022F8C0_ovl_01++;
    if (D_8022F8C0_ovl_01 >= STAR_FRAME_COUNT) {
      D_8022F8C0_ovl_01 = 0;
    }

    D_8022F8C4_ovl_01 = D_8022F8C4_ovl_01 == 0;
    OpenEncodedAsset(D_8022F848_ovl_01[D_8022F8C0_ovl_01], &asset);
    ReadEncodedAsset(D_8010526C[D_8022F8C4_ovl_01], &asset);
    OpenEncodedAsset(D_8022F884_ovl_01[D_8022F8C0_ovl_01], &asset);
    ReadEncodedAsset(D_800FF180[D_8022F8C4_ovl_01], &asset);
  }

  *displayListHead = gfx;
}

void func_80208F8C_ovl_01(Gfx** displayListHead, s32 left, s32 top) {
  Gfx* gfx = *displayListHead;
  s16 stripIndex;
  s32 rowCount;
  s32 unused[8];

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPLoadTLUT_pal256(
      gfx++, TextureAddress(ImageBody(D_800FBE30[D_8022F8CE_ovl_01])));
  for (stripIndex = 0; stripIndex != TOAD_STRIP_COUNT; stripIndex++) {
    rowCount = TOAD_LAST_STRIP_HEIGHT;
    if (stripIndex == 0) {
      rowCount = TOAD_STRIP_HEIGHT;
    }

    gDPLoadTextureBlock(gfx++,
        TextureAddress(LargeImageTexels(D_800FBE30[D_8022F8CE_ovl_01]) +
            (stripIndex * TOAD_WIDTH * TOAD_STRIP_HEIGHT)),
        G_IM_FMT_CI, G_IM_SIZ_8b, TOAD_WIDTH, rowCount, 0,
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, QuarterPixels(left),
        QuarterPixels(top + (stripIndex * TOAD_STRIP_HEIGHT)),
        QuarterPixels(left + TOAD_WIDTH),
        QuarterPixels(top + (stripIndex * TOAD_STRIP_HEIGHT) + rowCount),
        G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    gDPPipeSync(gfx++);
  }

  *displayListHead = gfx;
}

void DrawBirdieBadgeLabel(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;

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
