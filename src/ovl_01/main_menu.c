/*
 * The menus' update, which loads each screen and runs it, and their drawing,
 * which hands each screen to its draw routine. It holds the title scene of the
 * menus, the main menu, the clubhouse, the options, the course select and the
 * mini-golf course select, and the wipes that leave the menus.
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
extern u32 g_GameMode;
extern f32 f_MenuBobOffset;
extern f32 f_MenuBobPhase;

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
 * func_801FC754 draws the course backdrop of D_801F4A20 at double size. Each
 * strip loads a row either side of the rows it shows, so that the filtering
 * blends across the seams.
 */
#define BACKDROP_WIDTH 152
#define BACKDROP_SCALE 2
#define BACKDROP_SEAM_ROW_COUNT 2
#define BACKDROP_SECOND_ROW_START (1 << 5)
#define BACKDROP_STRIP_ROW_COUNT 11
#define BACKDROP_STRIP_COUNT 11
#define BACKDROP_LEFT 8
#define BACKDROP_FIRST_TOP 8
#define BACKDROP_TOP 10

extern void* D_800FF420[2];
extern s32 D_801051F0;
extern void* D_801052BC[2];
extern void* D_801F4A20;
extern s16 D_8022BE8C_ovl_01;
extern s16 D_8022BE92_ovl_01;
/* The menus' camera orbit's updates */
extern s32 D_8022F8E0_ovl_01;
/* Each player's sound pan, by the player count */
extern u8 D_8022F8E4_ovl_01[MAX_PLAYER_COUNT][MAX_PLAYER_COUNT];
/* The parts of the menu backdrop that func_801FC60C and func_801FB54C draw */
#define MENU_LAYER_UNK1 (1 << 0)
#define MENU_LAYER_UNK4 (1 << 2)
#define MENU_LAYER_UNK8 (1 << 3)
#define MENU_LAYER_UNK10 (1 << 4)
#define MENU_LAYER_UNK20 (1 << 5)
#define MENU_LAYER_UNK80 (1 << 7)

/* Values of g_GameState.unk31, the replay that leaving the menus starts */
#define REPLAY_KIND_NONE 0
#define REPLAY_KIND_RING_SHOT 2
#define PLAYER_UNK28_TRAINING (-1)
#define PLAYER_UNK28_NINE_HOLES 3
#define PLAYER_UNK28_EIGHTEEN_HOLES 6

/* The choices of the list that func_801FAFEC runs, which no menu screen
 * dispatches to */
#define UNUSED_LIST_CHOICE_COUNT 21

extern s8 D_801052E8;
extern s32 D_800FE334;
extern u8 g_OptionBits;
extern s32 D_8022BE18_ovl_01;
extern s32 D_800FBD40[];
extern s32 D_8022BE0C_ovl_01;
extern s32 D_8022BE2C_ovl_01;
extern void* D_8010526C[2];
extern s32 D_8022F8C0_ovl_01;
extern u8 D_8022F8C4_ovl_01;
extern s8 D_8022F8C5_ovl_01;
extern s32 D_8022F8C8_ovl_01;
extern s8 D_8022F8CD_ovl_01;
extern s32 D_8022BE84_ovl_01;
extern u8 D_8022BE89_ovl_01;
extern s32 D_802322F0_ovl_01;
extern s32 D_802322F4_ovl_01;
extern s8 D_800FBE08[MAX_PLAYER_COUNT];
extern s16 D_801B71CC;
extern s16 D_800FBE2C;
extern u8 D_801B9410;
extern s8 D_801B5634;
extern s8 D_801B60BA[MENU_UNKE_COUNT];
extern s16 D_8023BFDC_ovl_01[MAX_PLAYER_COUNT];
extern s32 D_8022BE1C_ovl_01;
extern u16 D_8022BE50_ovl_01[MAX_PLAYER_COUNT]; /* Each player's clubs, a bit
                                                   per slot */
extern s32 D_8022F8F4_ovl_01[];

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
/* What g_MenuChoices.unk0 holds once the C buttons choose the holes to play */
#define HOLES_CHOICE_FRONT_NINE 1
#define HOLES_CHOICE_BACK_NINE 2
#define HOLES_CHOICE_ALL 3
/* D_8022BE88's value once the popup opened with C-up has finished */
#define MENU_PHASE_UNK2 2

extern void* D_801049A0[][MINI_GOLF_DISC_IMAGE_COUNT];
extern s32 D_8022F8D4_ovl_01; /* The disc's frame */
extern s32 D_8022F8D8_ovl_01; /* The calls that it has shown */
extern u8 D_8022F8DC_ovl_01; /* Which way it turns */
extern u8 D_8022F8DD_ovl_01; /* Which buffers it shows */

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
extern u8
    D_8022BE80_ovl_01; /* Which of D_800FBDFC's buffers shows the course */

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
extern s8 D_801323D0;
extern u8 D_801B70F0[MAXCONTROLLERS];
extern void* D_801B93E0[5];
extern s32 D_8022BE08_ovl_01;
extern s16 D_8022BE8A_ovl_01;
extern u8 D_8022BE94_ovl_01;
extern s32 D_8022F848_ovl_01[]; /* The star's frames */
extern u8 D_8022F8CC_ovl_01;
extern u8 D_8022F8CE_ovl_01;
extern s32 D_8023BFC0_ovl_01[MAX_PLAYER_COUNT];

void InitPlay(s32 kind);
/* Defined as returning s8, but their callers here use the whole register */
s32 func_8005DE00(void);
s32 func_8005DE20(void);
s32 func_8005DE40(void);
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
void LoadSuperShotReplay(PlayRecord* record);
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

/* Not static: the original left it to the linker's common block */
u8 D_801323E6;

void* GetMenuImageBuffer(void) {
  return g_MenuImageBuffer;
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
    g_GameState.courseIndex = g_MenuChoices.courseIndex;
    g_GameState.holeIndex = g_MenuChoices.holeIndex;
  } else if (g_MenuScreen == MENU_SCREEN_CLUB_TOURNAMENT) {
    g_GameState.courseIndex = GetClubTournamentCourse();
    g_GameState.isMiniGolf = g_GameState.courseIndex >= MINI_GOLF_COURSE_INDEX;
    g_GameState.holeIndex = 0;
  } else if (g_GameState.gameMode == GAME_MODE_TRAINING) {
    g_GameState.courseIndex = 0;
    g_GameState.holeIndex = 0;
  } else {
    if (g_GameState.isMiniGolf) {
      g_GameState.courseIndex = g_MenuChoices.unk14 + MINI_GOLF_COURSE_INDEX +
          (g_MenuChoices.unk13 * MINI_GOLF_COURSES_PER_RECORD);
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
    return g_MenuChoices.unk14 + MINI_GOLF_COURSE_INDEX +
        (g_MenuChoices.unk13 * MINI_GOLF_COURSES_PER_RECORD);
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

  if (g_MenuScreen == 6) {
    if ((GetChosenCourseIndex() != g_GameState.courseIndex) || !D_800FE334) {
      StartLoadingChosenHole();
    }
  } else if (g_MenuScreen == MENU_SCREEN_UNK23) {
    if ((g_GameState.holeIndex != g_MenuChoices.holeIndex) ||
        (g_GameState.courseIndex != g_MenuChoices.courseIndex) || !D_800FE334) {
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

    case 5:
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

    case 6:
      if (!g_GameState.isMiniGolf) {
        func_801F8BAC_ovl_01();
      } else {
        func_801F93D8_ovl_01();
      }

      break;

    case 8:
      func_801FADBC_ovl_01();
      break;

    case MENU_SCREEN_UNK20:
      func_80219948_ovl_01(1, g_ChosenPlayerCount);
      break;

    case MENU_SCREEN_UNK22:
      func_80217D20_ovl_01();
      break;

    case 18:
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
            (u8*)D_8012D420[D_8022BE8E_ovl_01] + OPTION_VALUES_OFFSET, &asset);
        D_8022BE92_ovl_01 ^= 1;
        OpenEncodedAsset(ASSET_SQUARE_BOARD, &asset);
        ReadEncodedAsset(D_800FF420[D_8022BE92_ovl_01], &asset);
        func_802267B0_ovl_01(D_800FC6A0, 1);
        func_802267B0_ovl_01(D_800FBE30[0], 2);
        imageBuffer = GetMenuImageBuffer();
        OpenEncodedAsset(ASSET_OPTIONS_MARIO, &asset);
        ReadEncodedAsset(imageBuffer, &asset);
        OpenEncodedAsset(ASSET_GREEN_ARROWS, &asset);
        ReadEncodedAsset(imageBuffer + OPTIONS_ARROWS_OFFSET, &asset);
        OpenEncodedAsset(ASSET_OPTIONS_SHADOW, &asset);
        ReadEncodedAsset(imageBuffer + OPTIONS_SHADOW_OFFSET, &asset);
        InitSram();
        TransferSaveData(save, FALSE);
        optionBits = &g_OptionBits;
        *optionBits = 0;
        if (func_8005DE00()) {
          *optionBits |= OPTION_BIT_MUSIC_OFF;
        }

        if (func_8005DE20()) {
          *optionBits |= OPTION_BIT_MONO;
        }

        if (func_8005DE40()) {
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
        ReadEncodedAsset(D_801F4A20, &asset);
        D_8022BE92_ovl_01 ^= 1;
        OpenEncodedAsset(ASSET_WIDE_BOARD, &asset);
        ReadEncodedAsset(D_800FF420[D_8022BE92_ovl_01], &asset);
        g_ImportedCharacterColumns[0] = NO_IMPORTED_COLUMN;
        g_ImportedCharacterColumns[1] = NO_IMPORTED_COLUMN;
        g_ImportedCharacterColumns[2] = NO_IMPORTED_COLUMN;
        g_ImportedCharacterColumns[3] = NO_IMPORTED_COLUMN;
        func_8005D334();
        break;

      case 18:
        D_8022BE8E_ovl_01 ^= 1;
        D_8022BE90_ovl_01 ^= 1;
        OpenEncodedAsset(ASSET_TITLE_LABELS, &asset);
        ReadEncodedAsset(D_8012D420[D_8022BE8E_ovl_01], &asset);
        OpenEncodedAsset(ASSET_TITLE_LABELS_DIM, &asset);
        ReadEncodedAsset(D_801B5CD0[D_8022BE90_ovl_01], &asset);
        OpenEncodedAsset(ASSET_COURSE_SCENE, &asset);
        ReadEncodedAsset(D_801F4A20, &asset);
        D_8022BE92_ovl_01 ^= 1;
        OpenEncodedAsset(ASSET_WIDE_BOARD, &asset);
        ReadEncodedAsset(D_800FF420[D_8022BE92_ovl_01], &asset);
        break;

      case 5:
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
        ReadEncodedAsset(D_801F4A20, &asset);
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
        func_8020E26C_ovl_01(D_800FBE30[1], 6);
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
        func_8020B2C4_ovl_01(D_800FF420[D_8022BE92_ovl_01], 2);
        D_8022BE8E_ovl_01 ^= 1;
        D_8022BE90_ovl_01 ^= 1;
        func_8020B2C4_ovl_01(D_8012D420[D_8022BE8E_ovl_01], 1);
        func_8020B2C4_ovl_01(D_801B5CD0[D_8022BE90_ovl_01], 5);
        func_8020B2C4_ovl_01(D_801B60D0, CHARACTER_SELECT_UNREAD_KIND);
        func_8020B2C4_ovl_01(D_800FBE30[0], 8);
        func_8020B438_ovl_01();
        break;

      case MENU_SCREEN_UNK16:
        func_8005C674(1, 0);
        func_80209500_ovl_01();
        OpenEncodedAsset(ASSET_COURSE_SCENE, &asset);
        ReadEncodedAsset(D_801F4A20, &asset);
        D_8022BE92_ovl_01 ^= 1;
        func_8020B2C4_ovl_01(D_800FF420[D_8022BE92_ovl_01], 2);
        D_8022BE8E_ovl_01 ^= 1;
        D_8022BE90_ovl_01 ^= 1;
        func_8020B2C4_ovl_01(D_8012D420[D_8022BE8E_ovl_01], 1);
        func_8020B2C4_ovl_01(D_801B5CD0[D_8022BE90_ovl_01], 5);
        func_8020B2C4_ovl_01(D_801B60D0, CHARACTER_SELECT_UNREAD_KIND);
        func_8020B2C4_ovl_01(D_800FBE30[0], 8);
        func_8020B438_ovl_01();
        func_8020B3CC_ovl_01();
        break;

      case MENU_SCREEN_SAVED_GAMES:
        D_8022BE8E_ovl_01 ^= 1;
        D_8022BE90_ovl_01 ^= 1;
        OpenEncodedAsset(ASSET_TITLE_LABELS_DIM, &asset);
        ReadEncodedAsset(D_801B5CD0[D_8022BE90_ovl_01], &asset);
        OpenEncodedAsset(ASSET_COURSE_SCENE, &asset);
        ReadEncodedAsset(D_801F4A20, &asset);
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
        func_80228A80_ovl_01(D_801B60D0, 1);
        func_80228A80_ovl_01(D_800FF420[D_8022BE92_ovl_01], 2);
        func_80228A80_ovl_01(D_8012D420[D_8022BE8E_ovl_01], 3);
        func_80228A80_ovl_01(D_800FBE30[0], 4);
        func_80228A80_ovl_01(D_800FBE30[1], 5);
        func_802267B0_ovl_01(D_800FC6A0, 1);
        func_802267B0_ovl_01(D_801049A0[0][0], 2);
        func_802267B0_ovl_01(D_801049A0[0][1], 3);
        func_802267B0_ovl_01(D_801049A0[1][0], 4);
        func_802267B0_ovl_01(D_801049A0[1][1], 5);
        func_802267B0_ovl_01(D_801049A0[2][0], 6);
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
          ReadEncodedAsset(D_801F4A20, &asset);
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

      case 6:
        if (g_GameState.isMiniGolf) {
          for (index = 0; index != MINI_GOLF_DISC_IMAGE_COUNT; index++) {
            OpenEncodedAsset(
                g_MiniGolfDiscFrameAssets[(g_MenuChoices.unk14 *
                                              MINI_GOLF_DISC_IMAGE_COUNT) +
                    index][MINI_GOLF_FACING_FRAME],
                &asset);
            ReadEncodedAsset(D_801049A0[0][index], &asset);
            OpenEncodedAsset(
                g_MiniGolfDiscFrameAssets[(g_MenuChoices.unk14 *
                                              MINI_GOLF_DISC_IMAGE_COUNT) +
                    index][MINI_GOLF_FACING_FRAME],
                &asset);
            ReadEncodedAsset(D_801049A0[1][index], &asset);
          }

          for (index = 0; index != ARRAY_COUNT(D_801052C4); index++) {
            OpenAssetFile(
                g_MiniGolfDiscFiles[(g_MenuChoices.unk14 * 2) + index], &file);
            ReadAssetFile(D_801052C4[index], GetAssetFileSize(&file), &file);
          }
        } else {
          for (index = 0; index != 6; index++) {
            OpenEncodedAsset(g_CourseIconAssets[index], &asset);
            ReadEncodedAsset(D_801049A0[0][index], &asset);
          }
        }

        OpenEncodedAsset(ASSET_SKY, &asset);
        ReadEncodedAsset(D_801F4A20, &asset);
        OpenEncodedAsset(ASSET_WATER, &asset);
        ReadEncodedAsset((u8*)D_801F4A20 + COURSE_SELECT_WATER_OFFSET, &asset);
        OpenEncodedAsset(ASSET_BLENDED_TEXTURE_SCROLLING_LAYER, &asset);
        ReadEncodedAsset((u8*)D_801F4A20 + COURSE_SELECT_LAYER_OFFSET, &asset);
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
        func_8020E26C_ovl_01(D_801B5CD0[D_8022BE90_ovl_01], 6);
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
        func_8020E26C_ovl_01(D_800FC898, 2);
        func_8020E26C_ovl_01(D_801B60D0, 3);
        func_8020E26C_ovl_01(D_8010526C[0], 4);
        func_8020E26C_ovl_01(D_800FF180[0], 5);
        D_8022BE90_ovl_01 ^= 1;
        func_8020E26C_ovl_01(D_801B5CD0[D_8022BE90_ovl_01], 6);
        func_8020E26C_ovl_01(D_800FBE30[0], 7);
        ComputeRoundStatistics();
        D_801323D0 = GetOpenCourseCount();
        break;

      case MENU_SCREEN_BEST_TIMES:
        func_80221B20_ovl_01();
        func_80213390_ovl_01(D_801049A0[0]);
        func_80213464_ovl_01(NULL, 1);
        D_8022BE90_ovl_01 ^= 1;
        func_8020E26C_ovl_01(D_801B5CD0[D_8022BE90_ovl_01], 6);
        func_80213464_ovl_01(D_801B5CD0[D_8022BE90_ovl_01], 2);
        func_8020E26C_ovl_01(D_801B60D0, 3);
        D_8022BE92_ovl_01 ^= 1;
        func_80213464_ovl_01(D_800FF420[D_8022BE92_ovl_01], 3);
        func_80213464_ovl_01(D_800FBE30[0], 4);
        func_8021585C_ovl_01();
        break;

      case MENU_SCREEN_CHARACTER_TABLE:
        func_80221B20_ovl_01();
        func_80213464_ovl_01(NULL, 1);
        D_8022BE90_ovl_01 ^= 1;
        func_8020E26C_ovl_01(D_801B5CD0[D_8022BE90_ovl_01], 6);
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
        func_8020E26C_ovl_01(D_801B5CD0[D_8022BE8E_ovl_01], 6);
        func_8021CFAC_ovl_01(D_8012D420[D_8022BE90_ovl_01], 2);
        func_8021CFAC_ovl_01(D_801B60D0, 1);
        func_8021CFAC_ovl_01(D_800FBE30[0], 3);
        func_8021E01C_ovl_01();
        func_8021EF64_ovl_01();
        break;

      case MENU_SCREEN_RING_SHOT_RECORDS:
        func_80221B20_ovl_01();
        D_8022BE92_ovl_01 ^= 1;
        func_8021342C_ovl_01(D_800FF420[D_8022BE92_ovl_01]);
        D_8022BE8E_ovl_01 ^= 1;
        D_8022BE90_ovl_01 ^= 1;
        func_8020E26C_ovl_01(D_801B5CD0[D_8022BE90_ovl_01], 6);
        func_8021F5F0_ovl_01(D_801B60D0, 1);
        func_8021F5F0_ovl_01(D_8012D420[D_8022BE8E_ovl_01], 2);
        func_8021F5F0_ovl_01(D_8010526C[0], 3);
        func_8021F6E0_ovl_01(D_801049A0[0]);
        func_8021F5F0_ovl_01(D_800FBE30[0], 4);
        func_80220A30_ovl_01();
        func_80220BB0_ovl_01();
        break;
    }
  }

  if (g_MenuScreen == MENU_SCREEN_EXIT) {
    TransferSaveData(save, TRUE);
    if (g_GameState.gameMode == GAME_MODE_TRAINING) {
      g_MenuScreen = 6;
    } else {
      g_MenuScreen = 18;
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
 * Doesn't match: the original reloads g_PlayerCount for each pass of the
 * outer loop, where this keeps it in a register.
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

void func_801F6B28_ovl_01(void) {
  SaveData* save = GetSaveData();
  MenuMemory* memory = func_8005AF74();
  UnkB8* player;
  s32 index;
  s32 slot;
  s32 clubSet;
  s32* skippedNines;
  s32 row;
  s32 column;

  memset(&g_RoundHud, 0, sizeof(g_RoundHud));
  if (g_GameState.unk31 != REPLAY_KIND_NONE) {
    if (g_GameState.unk31 == REPLAY_KIND_RING_SHOT) {
      LoadRingShotRecord(
          (RingShotRecord*)&save->unk1DF0[memory->ringShotReplaySlot]);
    } else {
      LoadSuperShotReplay(
          (PlayRecord*)&save->rankedRecords[func_8021E468_ovl_01()]);
    }

    g_GameState.unk14 = 0;
    g_GameState.playerCount = 1;
    g_GameState.unk0 = 0;
    return;
  }

  StartBets();
  if (func_8022BDD0_ovl_01()) {
    g_SkippedNines = 0;
    g_GameState.courseIndex = GetClubTournamentCourse();
    g_GameState.gameMode = GetClubTournamentGameMode();
    g_GameState.isMiniGolf = g_GameState.courseIndex >= MINI_GOLF_COURSE_INDEX;
    for (index = 0; index != MENU_UNKE_COUNT; index++) {
      D_801B60BA[index] = -1;
    }

    for (index = 0; index != MAX_PLAYER_COUNT; index++) {
      D_8023BFDC_ovl_01[index] = 0;
    }

    if (func_8022BDC4_ovl_01() != -1) {
      D_8012F480[0] = func_8022BDC4_ovl_01();
      D_800FBE08[0] = 0;
    }
  } else {
    if (g_GameMode == GAME_MODE_RING_SHOT) {
      g_CourseIndex = g_MenuChoices.courseIndex;
    } else if (g_GameMode == GAME_MODE_TRAINING) {
      g_GameState.courseIndex = 0;
    } else {
      g_CourseIndex = GetChosenCourseIndex();
    }

    if (g_GameMode == GAME_MODE_STROKE || g_GameMode == GAME_MODE_MATCH_GAME ||
        g_GameMode == GAME_MODE_RING_SHOT ||
        g_GameMode == GAME_MODE_SKINS_MATCH ||
        g_GameMode == GAME_MODE_CLUB_SLOTS) {
      /* Through a pointer, as the original keeps the address in a register for
       * every store */
      skippedNines = &g_SkippedNines;
      *skippedNines = D_8022BE1C_ovl_01;
      if (MAP_CAMERA_MODE_TOP_BYTE == 1) {
        switch (g_MenuChoices.unk0) {
          case 0:
            *skippedNines = 0;
            break;
          case 1:
            *skippedNines = BACK_NINE_SKIPPED;
            break;
          case 2:
            *skippedNines = FRONT_NINE_SKIPPED;
            break;
          case 3:
            *skippedNines = 0;
            break;
        }
      }
    } else {
      g_SkippedNines = 0;
    }
  }

  g_GameState.unk14 = 0;
  g_GameState.playerCount = g_ChosenPlayerCount;
  if (g_GameState.gameMode == GAME_MODE_GET_CHARACTER) {
    g_GameState.playerCount = 2;
  }

  g_GameState.unk0 = GAME_STATE_RELOAD_HOLE;
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
      g_Players[index].unk1E[0] = 1;
    } else {
      g_Players[index].unk1E[0] = 0;
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
    } else if (g_SkippedNines == BACK_NINE_SKIPPED ||
        g_SkippedNines == FRONT_NINE_SKIPPED) {
      player->unk28 = PLAYER_UNK28_NINE_HOLES;
    } else {
      player->unk28 = PLAYER_UNK28_EIGHTEEN_HOLES;
    }

    LoadPlayerFace(index, (s8)player->characterIndex, (s8)player->paletteIndex);
  }

  if (g_GameState.gameMode == GAME_MODE_GET_CHARACTER) {
    g_Players[1].unk1E[0] = 1;
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
  g_GameState.isSuddenDeath = 0;
  g_GameState.isMiniGolf = FALSE;
  g_GameState.unk31 = 0;
  g_GameState.currentPlayerIndex = 0;
  if (GetWipeKind() != WIPE_NONE && D_801B9410 == 0) {
    if (GetWipeProgress() == 0) {
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
        *((s8*)memory + index + 3) = 0;
        *((s8*)memory + index + 7) = 0;
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
        D_800FBD40[index + 2] = 0;
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
        D_801323E6 = 1;
        g_MenuScreen = titleChoice;
      } else {
        D_801323E6 = 0;
        g_MenuScreen = MENU_SCREEN_MAIN_WITHOUT_TRANSMIT;
      }
    }

    if (D_801B71CC != g_MenuScreen ||
        D_800FBE2C != D_801323A0.playerCountChoice) {
      D_801323A0.modeChoice = 0;
      *(s16*)D_800FBD40 = 0;
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
    SetTextSlot(
        SCREEN_WIDTH / 2, TITLE_SCREEN_TEXT_Y, NO_MESSAGE, 1, 0, 0, 0, 0, 0, 0);
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
  if (D_800FBD40[2] == 1) {
    do {
      D_801323A0.unk2++;
      if (D_801323A0.unk2 > D_8022BE0C_ovl_01 - 1) {
        D_801323A0.unk2 = 0;
      }
    } while (D_800FBD40[D_801323A0.unk2 + 2] == 1);
  }
}

/*
 * func_801F7660 moves the character cursor past the characters marked as taken,
 * and plays the voice of the one chosen. Nothing reads arg0.
 */
#define CHOSEN_CHARACTER_VOICE 24

s32 func_801F7660_ovl_01(s32 arg0, s32 playerNumber) {
  if (D_8022BE28_ovl_01 != 0) {
    while (D_800FBD40[D_801323A0.unk2 + 2] == 1) {
      D_801323A0.unk2++;
      if (D_801323A0.unk2 > D_8022BE0C_ovl_01 - 1) {
        D_801323A0.unk2 = 0;
      }
    }

    if (g_ControllerStates[0].pressedButtons & U_JPAD) {
      D_801323A0.unk2--;
      PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
      while (D_800FBD40[D_801323A0.unk2 + 2] == 1) {
        D_801323A0.unk2--;
        if (D_801323A0.unk2 < 0) {
          D_801323A0.unk2 = D_8022BE0C_ovl_01 - 1;
        }
      }
    } else if (g_ControllerStates[0].pressedButtons & D_JPAD) {
      PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
      D_801323A0.unk2++;
      while (D_800FBD40[D_801323A0.unk2 + 2] == 1) {
        D_801323A0.unk2++;
        if (D_801323A0.unk2 > D_8022BE0C_ovl_01 - 1) {
          D_801323A0.unk2 = 0;
        }
      }
    }

    if (D_801323A0.unk2 < 0) {
      D_801323A0.unk2 = D_8022BE0C_ovl_01 - 1;
      while (D_800FBD40[D_801323A0.unk2 + 2] == 1) {
        D_801323A0.unk2--;
      }
    }

    if (D_801323A0.unk2 > D_8022BE0C_ovl_01 - 1) {
      D_801323A0.unk2 = 0;
      while (D_800FBD40[D_801323A0.unk2 + 2] == 1) {
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
        D_800FBD40[D_801323A0.unk2 + 2] == 0) {
      PlayCharacterSound(D_801323A0.unk2, 0, CHOSEN_CHARACTER_VOICE,
          MENU_VOLUME_SOFT, SOUND_FULL_SCALE);
      D_8022BE2C_ovl_01 = 1;
      SetAnimationSpeed(0, 1.0f);
      return 0;
    }

    if (g_MenuPressedButtons & B_BUTTON) {
      PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
      return -1;
    }
    while (D_800FBD40[D_801323A0.unk2 + 2] == 1) {
      D_801323A0.unk2++;
      if (D_801323A0.unk2 > D_8022BE0C_ovl_01 - 1) {
        D_801323A0.unk2 = 0;
      }
    }

    if (g_MenuRepeatButtons & U_JPAD) {
      D_801323A0.unk2--;
      PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
      while (D_800FBD40[D_801323A0.unk2 + 2] == 1) {
        D_801323A0.unk2--;
        if (D_801323A0.unk2 < 0) {
          D_801323A0.unk2 = D_8022BE0C_ovl_01 - 1;
        }
      }
    } else if (g_MenuRepeatButtons & D_JPAD) {
      PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
      D_801323A0.unk2++;
      while (D_800FBD40[D_801323A0.unk2 + 2] == 1) {
        D_801323A0.unk2++;
        if (D_801323A0.unk2 > D_8022BE0C_ovl_01 - 1) {
          D_801323A0.unk2 = 0;
        }
      }
    }

    if (D_801323A0.unk2 < 0) {
      D_801323A0.unk2 = D_8022BE0C_ovl_01 - 1;
      while (D_800FBD40[D_801323A0.unk2 + 2] == 1) {
        D_801323A0.unk2--;
      }
    }

    if (D_801323A0.unk2 > D_8022BE0C_ovl_01 - 1) {
      D_801323A0.unk2 = 0;
      while (D_800FBD40[D_801323A0.unk2 + 2] == 1) {
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
#define LOAD_CHOSEN_MODE_ICON() \
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
 * g_GameMode in another order, the B button's path loads D_8012F480 and
 * g_CharacterGrid in another order, and the spin's slowing keeps the spin
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
      LOAD_CHOSEN_MODE_ICON();
      return;
    }

    PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
    SetTextSlot(0, 0, NO_MESSAGE, 1, MODE_TEXT_SLOT, TEXT_FLAG_TYPED, 0xFF,
        0xFF, 0xFF, 0xFF);
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
          D_801B60BA[index] = -1;
        }

        break;
      case MODE_ICON_STROKE:
        PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
        StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
        g_MenuScreen = MENU_SCREEN_UNK6;
        g_MenuChoices.unk0 = HOLES_CHOICE_ALL;
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
            D_801B60BA[index] = -1;
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
        g_MenuChoices.unk0 = HOLES_CHOICE_ALL;
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
        g_MenuChoices.unk0 = HOLES_CHOICE_ALL;
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
        g_MenuChoices.unk0 = HOLES_CHOICE_FRONT_NINE;
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
        g_MenuChoices.unk0 = HOLES_CHOICE_ALL;
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
            D_801B60BA[index] = -1;
          }

          D_800C2BD5 = 1;
        }

        break;
      case MODE_ICON_TRAINING:
        PlaySound(SOUND_MENU_START, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
        StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
        g_GameState.gameMode = GAME_MODE_TRAINING;
        g_SkippedNines = 0;
        StartLoadingChosenHole();
        g_MenuScreen = MENU_SCREEN_UNK26;
        D_800FF4C0 = 1;
        MAP_CAMERA_MODE_TOP_BYTE = 2;
        D_801B5634 = 0;
        g_MenuChoices.unk0 = HOLES_CHOICE_FRONT_NINE;
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
        g_MenuChoices.unk0 = HOLES_CHOICE_FRONT_NINE;
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
        g_MenuChoices.unk0 = HOLES_CHOICE_ALL;
        func_80217B88_ovl_01(MENU_UNKE_PRESET_CLUB_SLOTS);
        break;
    }

    g_SkippedNines = 0;

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
      LOAD_CHOSEN_MODE_ICON();
    }

    StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
    D_801323A0.modeSpin = 0;
    SetTextSlot(0, 0, NO_MESSAGE, 1, MODE_TEXT_SLOT, TEXT_FLAG_TYPED, 0xFF,
        0xFF, 0xFF, 0xFF);
    PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
    g_MenuScreen = MENU_SCREEN_CHARACTER_SELECT;
    D_801B7070--;
    D_800FBE08[D_801B7070] = 0;
    D_800FBD40[D_8012F480[D_801B7070] + 2] = 0;
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

      D_801323A0.modeSpin = SPREAD_OVER_MODES(MODE_SELECT_TURN);
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

      D_801323A0.modeSpin = SPREAD_OVER_MODES(-MODE_SELECT_TURN);
      if (g_ChosenPlayerCount == 1) {
        D_8022F8C5_ovl_01 = 0;
        D_8022F8C0_ovl_01 = 0;
        D_8022F8CD_ovl_01 = 0;
        D_8022F8C8_ovl_01 = 0;
      }
    }
  }

  if (ABS(D_801323A0.modeSpin) != 0) {
    step = SPREAD_OVER_MODES(MODE_SELECT_SPIN_SPEED);
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
      LOAD_CHOSEN_MODE_ICON();
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
    SetTextSlot(MODE_TEXT_X, MODE_TEXT_Y, MESSAGE_MODE_NEEDS_CONTROLLERS, 1,
        MODE_TEXT_SLOT, TEXT_FLAG_TYPED, 0xFF, 0xFF, 0xFF, 0xFF);
  } else if (g_ChosenPlayerCount == 1 &&
      D_8022BEBC_ovl_01[0][D_801323A0.modeChoice] == MODE_ICON_STROKE) {
    SetTextSlot(MODE_TEXT_X, MODE_TEXT_Y, MESSAGE_SOLO_STROKE, 1,
        MODE_TEXT_SLOT, TEXT_FLAG_TYPED, 0xFF, 0xFF, 0xFF, 0xFF);
  } else {
    SetTextSlot(MODE_TEXT_X, MODE_TEXT_Y, mode + MESSAGE_FIRST_MODE, 1,
        MODE_TEXT_SLOT, TEXT_FLAG_TYPED, 0xFF, 0xFF, 0xFF, 0xFF);
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
    if (GetWipeProgress() == 0) {
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
        SetTextSlot(0, 0, NO_MESSAGE, 1, index, 0, 0xFF, 0xFF, 0xFF, 0xFF);
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
      D_800FBD40[D_8012F480[D_801B7070] + 2] = 0;
    } else {
      g_MenuScreen = MENU_SCREEN_UNK5;
    }

    MAP_CAMERA_MODE_TOP_BYTE = 0;
    D_800FE334 = 0;
    StopLoaderThread();
    FreeAllCharacterSlots();
    for (index = 0; index != COURSE_SELECT_TEXT_SLOT_COUNT; index++) {
      SetTextSlot(0, 0, NO_MESSAGE, 1, index, 0, 0xFF, 0xFF, 0xFF, 0xFF);
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
        SetTextSlot(0, 0, NO_MESSAGE, 1, index, 0, 0xFF, 0xFF, 0xFF, 0xFF);
      }
    } else if (g_MenuRepeatButtons & R_JPAD) {
      PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
      if (++D_801323A0.courseChoice > courseCount - 1) {
        D_801323A0.courseChoice = 0;
      }

      D_801323A0.courseSpin = -COURSE_SELECT_TURN / courseCount;
      for (index = 0; index != COURSE_SELECT_TEXT_SLOT_COUNT; index++) {
        SetTextSlot(0, 0, NO_MESSAGE, 1, index, 0, 0xFF, 0xFF, 0xFF, 0xFF);
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
      g_MenuChoices.unk0 = HOLES_CHOICE_ALL;
      g_SkippedNines = 0;
      StartLoadingChosenHole();
    } else if ((g_MenuRepeatButtons & L_CBUTTONS) &&
        MAP_CAMERA_MODE_TOP_BYTE == 1) {
      PlaySound(SOUND_SLICE, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
      g_MenuChoices.unk0 = HOLES_CHOICE_FRONT_NINE;
      g_SkippedNines = BACK_NINE_SKIPPED;
      StartLoadingChosenHole();
    } else if ((g_MenuRepeatButtons & R_CBUTTONS) &&
        MAP_CAMERA_MODE_TOP_BYTE == 1) {
      PlaySound(SOUND_SLICE, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
      g_MenuChoices.unk0 = HOLES_CHOICE_BACK_NINE;
      g_SkippedNines = FRONT_NINE_SKIPPED;
      StartLoadingChosenHole();
    }
  }

  if (ABS(D_801323A0.courseSpin) != 0) {
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
            course = g_MenuChoices.unk14 - 1;
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
            course = g_MenuChoices.unk14;
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
            if (--g_MenuChoices.unk14 < 0) {
              g_MenuChoices.unk14 = MINI_GOLF_COURSES_PER_RECORD - 1;
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
            course = g_MenuChoices.unk14 + 1;
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
            course = g_MenuChoices.unk14;
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
            if (++g_MenuChoices.unk14 >= MINI_GOLF_COURSES_PER_RECORD) {
              g_MenuChoices.unk14 = 0;
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
    if (GetWipeProgress() == 0) {
      g_MenuScreen = MENU_SCREEN_EXIT;
    }

    return;
  }

  isDone = IsHudAnimationDone();
  if (isDone != TRUE) {
    return;
  }

  if (g_MenuPressedButtons & A_BUTTON) {
    if (g_MenuChoices.unk15 != 0) {
      g_MenuChoices.unk15 = 0;
      return;
    }

    if (g_MenuChoices.unk13 < g_MenuCourseCount) {
      D_801B9410 = 0;
      FreeAllCharacterSlots();
      LoadWipeShape(g_MenuChoices.unk14 +
          (g_MenuChoices.unk13 * MINI_GOLF_COURSES_PER_RECORD) +
          MINI_GOLF_COURSE_INDEX);
      StartWipe(WIPE_CLOSING);
      D_801052E8 = 0;
    }

    PlaySound(SOUND_MENU_START, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
    return;
  }

  if (g_MenuPressedButtons & B_BUTTON) {
    StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
    g_MenuChoices.unk15 = 0;
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
  if (g_MenuChoices.unk15 == 0) {
    if (g_MenuRepeatButtons & L_JPAD) {
      PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
      if (--g_MenuChoices.unk13 < 0) {
        g_MenuChoices.unk13 = MINI_GOLF_RECORD_COUNT - 1;
      }

      g_MenuChoices.unk15 = MINI_GOLF_RECORD_SLIDE;
    } else if (g_MenuRepeatButtons & R_JPAD) {
      PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
      if (++g_MenuChoices.unk13 >= MINI_GOLF_RECORD_COUNT) {
        g_MenuChoices.unk13 = 0;
      }

      g_MenuChoices.unk15 = -MINI_GOLF_RECORD_SLIDE;
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
      g_MenuChoices.unk0 = HOLES_CHOICE_ALL;
      g_SkippedNines = 0;
      StartLoadingChosenHole();
    } else if ((g_MenuRepeatButtons & L_CBUTTONS) &&
        MAP_CAMERA_MODE_TOP_BYTE == 1) {
      PlaySound(SOUND_SLICE, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
      g_MenuChoices.unk0 = HOLES_CHOICE_FRONT_NINE;
      g_SkippedNines = BACK_NINE_SKIPPED;
      StartLoadingChosenHole();
    } else if ((g_MenuRepeatButtons & R_CBUTTONS) &&
        MAP_CAMERA_MODE_TOP_BYTE == 1) {
      PlaySound(SOUND_SLICE, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
      g_MenuChoices.unk0 = HOLES_CHOICE_BACK_NINE;
      g_SkippedNines = FRONT_NINE_SKIPPED;
      StartLoadingChosenHole();
    }
  }

  if (ABS(g_MenuChoices.unk15) != 0) {
    direction = g_MenuChoices.unk15 >= 0 ? 1 : -1;
    step = MINI_GOLF_RECORD_SLIDE_STEP;
    g_MenuChoices.unk15 -= direction * step;

    /* Stop on reaching the record rather than sliding past it */
    if ((g_MenuChoices.unk15 >= 0 ? 1 : -1) != direction) {
      g_MenuChoices.unk15 = 0;
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
      switch (g_MenuChoices.mainChoice) {
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
      g_MenuChoices.mainChoice--;
    } else if (g_MenuRepeatButtons & D_JPAD) {
      PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
      g_MenuChoices.mainChoice++;
    }

    if (g_MenuChoices.mainChoice >= MAIN_CHOICE_COUNT) {
      g_MenuChoices.mainChoice = 0;
    }

    if (g_MenuChoices.mainChoice < 0) {
      g_MenuChoices.mainChoice = MAIN_CHOICE_COUNT - 1;
    }
  }
}

void func_801F9E38_ovl_01(void) {
  Boolean isDone = IsHudAnimationDone();

  if (isDone == TRUE) {
    if (g_MenuPressedButtons & A_BUTTON) {
      switch (g_MenuChoices.mainChoiceWithoutTransmit) {
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
      g_MenuChoices.mainChoiceWithoutTransmit--;
    } else if (g_MenuRepeatButtons & D_JPAD) {
      PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
      g_MenuChoices.mainChoiceWithoutTransmit++;
    }

    if (g_MenuChoices.mainChoiceWithoutTransmit >=
        MAIN_CHOICE_WITHOUT_TRANSMIT_COUNT) {
      g_MenuChoices.mainChoiceWithoutTransmit = 0;
    }

    if (g_MenuChoices.mainChoiceWithoutTransmit < 0) {
      g_MenuChoices.mainChoiceWithoutTransmit =
          MAIN_CHOICE_WITHOUT_TRANSMIT_COUNT - 1;
    }
  }
}

void func_801F9FF8_ovl_01(void) {
  Boolean isDone = IsHudAnimationDone();

  if (isDone == TRUE) {
    if (g_MenuPressedButtons & A_BUTTON) {
      switch (D_8022BF1C_ovl_01[g_MenuChoices.clubhouseChoice]) {
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
      if (D_801323E6 == isDone) {
        g_MenuScreen = MENU_SCREEN_MAIN;
      } else {
        g_MenuScreen = MENU_SCREEN_MAIN_WITHOUT_TRANSMIT;
      }

      PlaySong(SONG_TITLE, 0);
      return;
    }

    if (g_MenuRepeatButtons & U_JPAD) {
      PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
      g_MenuChoices.clubhouseChoice--;
    } else if (g_MenuRepeatButtons & D_JPAD) {
      PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
      g_MenuChoices.clubhouseChoice++;
    }

    if (g_MenuChoices.clubhouseChoice >= CLUBHOUSE_CHOICE_COUNT) {
      g_MenuChoices.clubhouseChoice = 0;
    }

    if (g_MenuChoices.clubhouseChoice < 0) {
      g_MenuChoices.clubhouseChoice = CLUBHOUSE_CHOICE_COUNT - 1;
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
  s32 windowState;
  s32 slot;
  u8* optionBits;

  if (IsHudAnimationDone() != TRUE) {
    return;
  }

  if (g_IsEraseWindowUp) {
    windowState = IsMenuWindowClosed();
    for (slot = 0; slot != OPTIONS_TEXT_SLOT_COUNT; slot++) {
      SetTextSlot(0, 0, NO_MESSAGE, 1, slot, 0, 0xFF, 0xFF, 0xFF, 0xFF);
    }

    if (windowState == -1) {
      return;
    }

    if (windowState == TRUE) {
      g_IsEraseWindowUp = FALSE;
      for (slot = 0; slot != OPTIONS_TEXT_SLOT_COUNT; slot++) {
        SetTextSlot(0, 0, NO_MESSAGE, 1, slot, 0, 0xFF, 0xFF, 0xFF, 0xFF);
      }

      return;
    }

    if (g_MenuPressedButtons & A_BUTTON) {
      if (memory->eraseChoice == ERASE_CHOICE_YES) {
        PlaySound(SOUND_CAPTION_MEET, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
        ResetSaveData();
        memory->unk45 = 0;
        g_SoundOptionBits = 0;
        MusSetStereo(TRUE);
      } else {
        PlaySound(SOUND_READY, ERASE_DECLINED_SOUND_CHANNEL, MENU_VOLUME_SOFT);
      }

      CloseMenuWindow();
      for (slot = 0; slot != OPTIONS_TEXT_SLOT_COUNT; slot++) {
        SetTextSlot(0, 0, NO_MESSAGE, 1, slot, 0, 0xFF, 0xFF, 0xFF, 0xFF);
      }
    } else if (g_MenuPressedButtons & B_BUTTON) {
      PlaySound(SOUND_READY, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
      CloseMenuWindow();
      for (slot = 0; slot != OPTIONS_TEXT_SLOT_COUNT; slot++) {
        SetTextSlot(0, 0, NO_MESSAGE, 1, slot, 0, 0xFF, 0xFF, 0xFF, 0xFF);
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
    switch (g_MenuChoices.optionsChoice) {
      case OPTIONS_CHOICE_MUSIC:
      case OPTIONS_CHOICE_MONO:
      case OPTIONS_CHOICE_CAMERA:
        break;
      case OPTIONS_CHOICE_ERASE_SAVE_DATA:
        PlaySound(SOUND_GO, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
        g_IsEraseWindowUp = TRUE;
        memory->eraseChoice = ERASE_CHOICE_NO;
        ResetMenuWindow();
        break;
      case OPTIONS_CHOICE_SONG:
      case OPTIONS_CHOICE_SONG_REVERB:
      case OPTIONS_CHOICE_SONG_FREQUENCY:
      case OPTIONS_CHOICE_SONG_TEMPO:
        if (g_SoundTestSong == NO_SONG) {
          RequestSongStop();
        } else {
          PlaySong(g_SoundTestSong, SONG_FLAG_RESTART);
        }

        break;
      case OPTIONS_CHOICE_SOUND:
      case OPTIONS_CHOICE_SOUND_REVERB:
      case OPTIONS_CHOICE_SOUND_FREQUENCY:
        RequestSongStop();
        PlaySound(g_SoundTestSound, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
        SetSoundReverb(MENU_SOUND_CHANNEL, g_SoundTestSoundReverb);
        SetSoundFrequencyOffset(MENU_SOUND_CHANNEL, g_SoundTestSoundFrequency);
        break;
    }
  } else if (g_MenuPressedButtons & B_BUTTON) {
    PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
    TransferSaveData(save, TRUE);
    if (D_801323E6) {
      g_MenuScreen = MENU_SCREEN_MAIN;
    } else {
      g_MenuScreen = MENU_SCREEN_MAIN_WITHOUT_TRANSMIT;
    }

    PlaySong(SONG_TITLE, 0);
  } else {
    if (g_MenuRepeatButtons & U_JPAD) {
      g_MenuChoices.optionsChoice--;
      PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
    } else if (g_MenuRepeatButtons & D_JPAD) {
      g_MenuChoices.optionsChoice++;
      PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
    }

    if (IsGameFlagSet(GAME_FLAG_UNK55)) {
      if (g_MenuChoices.optionsChoice >= OPTIONS_SOUND_TEST_CHOICE_COUNT) {
        g_MenuChoices.optionsChoice = 0;
      }

      if (g_MenuChoices.optionsChoice < 0) {
        g_MenuChoices.optionsChoice = OPTIONS_SOUND_TEST_CHOICE_COUNT - 1;
      }
    } else {
      if (g_MenuChoices.optionsChoice >= OPTIONS_CHOICE_COUNT) {
        g_MenuChoices.optionsChoice = 0;
      }

      if (g_MenuChoices.optionsChoice < 0) {
        g_MenuChoices.optionsChoice = OPTIONS_CHOICE_COUNT - 1;
      }
    }

    switch (g_MenuChoices.optionsChoice) {
      case OPTIONS_CHOICE_MUSIC:
        if (g_MenuRepeatButtons & (L_JPAD | R_JPAD)) {
          PlaySound(
              SOUND_OPTION_TOGGLED, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
          optionBits = &g_OptionBits;
          *optionBits ^= OPTION_BIT_MUSIC_OFF;
        }

        break;
      case OPTIONS_CHOICE_MONO:
        if (g_MenuRepeatButtons & (L_JPAD | R_JPAD)) {
          PlaySound(
              SOUND_OPTION_TOGGLED, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
          optionBits = &g_OptionBits;
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
          optionBits = &g_OptionBits;
          *optionBits ^= OPTION_BIT_SIMPLE_CAMERA;
        }

        break;
      case OPTIONS_CHOICE_SONG:
        if (g_MenuRepeatButtons & L_JPAD) {
          g_SoundTestSong--;
        }

        if (g_MenuRepeatButtons & R_JPAD) {
          g_SoundTestSong++;
        }

        if (g_MenuRepeatButtons & L_CBUTTONS) {
          g_SoundTestSong -= SOUND_TEST_COARSE_STEP;
        }

        if (g_MenuRepeatButtons & R_CBUTTONS) {
          g_SoundTestSong += SOUND_TEST_COARSE_STEP;
        }

        if (g_SoundTestSong < NO_SONG) {
          g_SoundTestSong = NO_SONG;
        }

        break;
      case OPTIONS_CHOICE_SONG_REVERB:
        if (g_MenuRepeatButtons & L_JPAD) {
          g_SoundTestSongReverb--;
        }

        if (g_MenuRepeatButtons & R_JPAD) {
          g_SoundTestSongReverb++;
        }

        if (g_MenuRepeatButtons & L_CBUTTONS) {
          g_SoundTestSongReverb -= SOUND_TEST_COARSE_STEP;
        }

        if (g_MenuRepeatButtons & R_CBUTTONS) {
          g_SoundTestSongReverb += SOUND_TEST_COARSE_STEP;
        }

        if (g_SoundTestSongReverb < 0) {
          g_SoundTestSongReverb = 0;
        }

        if (g_SoundTestSongReverb > SOUND_TEST_MAX_REVERB) {
          g_SoundTestSongReverb = SOUND_TEST_MAX_REVERB;
        }

        break;
      case OPTIONS_CHOICE_SONG_FREQUENCY:
        if (g_MenuRepeatButtons & L_JPAD) {
          g_SoundTestSongFrequency -= SOUND_TEST_FREQUENCY_FINE_STEP;
        }

        if (g_MenuRepeatButtons & R_JPAD) {
          g_SoundTestSongFrequency += SOUND_TEST_FREQUENCY_FINE_STEP;
        }

        if (g_MenuRepeatButtons & L_CBUTTONS) {
          g_SoundTestSongFrequency -= SOUND_TEST_FREQUENCY_COARSE_STEP;
        }

        if (g_MenuRepeatButtons & R_CBUTTONS) {
          g_SoundTestSongFrequency += SOUND_TEST_FREQUENCY_COARSE_STEP;
        }

        if (g_SoundTestSongFrequency < -SOUND_TEST_FREQUENCY_LIMIT) {
          g_SoundTestSongFrequency = -SOUND_TEST_FREQUENCY_LIMIT;
        }

        if (g_SoundTestSongFrequency > SOUND_TEST_FREQUENCY_LIMIT) {
          g_SoundTestSongFrequency = SOUND_TEST_FREQUENCY_LIMIT;
        }

        break;
      case OPTIONS_CHOICE_SONG_TEMPO:
        if ((g_MenuRepeatButtons & L_JPAD) && g_SoundTestSongTempo > 0) {
          g_SoundTestSongTempo--;
        }

        if ((g_MenuRepeatButtons & R_JPAD) &&
            g_SoundTestSongTempo <= SOUND_TEST_MAX_TEMPO) {
          g_SoundTestSongTempo++;
        }

        if ((g_MenuRepeatButtons & L_CBUTTONS) && g_SoundTestSongTempo > 0) {
          g_SoundTestSongTempo -= SOUND_TEST_COARSE_STEP;
        }

        if ((g_MenuRepeatButtons & R_CBUTTONS) &&
            g_SoundTestSongTempo <= SOUND_TEST_MAX_TEMPO) {
          g_SoundTestSongTempo += SOUND_TEST_COARSE_STEP;
        }

        if (g_SoundTestSongTempo < 0) {
          g_SoundTestSongTempo = 0;
        }

        if (g_SoundTestSongTempo > SOUND_TEST_MAX_TEMPO) {
          g_SoundTestSongTempo = SOUND_TEST_MAX_TEMPO;
        }

        break;
      case OPTIONS_CHOICE_SOUND:
        if (g_MenuRepeatButtons & L_JPAD) {
          g_SoundTestSound--;
        }

        if (g_MenuRepeatButtons & R_JPAD) {
          g_SoundTestSound++;
        }

        if (g_MenuRepeatButtons & L_CBUTTONS) {
          g_SoundTestSound -= SOUND_TEST_COARSE_STEP;
        }

        if (g_MenuRepeatButtons & R_CBUTTONS) {
          g_SoundTestSound += SOUND_TEST_COARSE_STEP;
        }

        if (g_SoundTestSound < 0) {
          g_SoundTestSound = 0;
        }

        if (g_SoundTestSound > SOUND_TEST_LAST_SOUND) {
          g_SoundTestSound = SOUND_TEST_LAST_SOUND;
        }

        break;
      case OPTIONS_CHOICE_SOUND_REVERB:
        if (g_MenuRepeatButtons & L_JPAD) {
          g_SoundTestSoundReverb--;
        }

        if (g_MenuRepeatButtons & R_JPAD) {
          g_SoundTestSoundReverb++;
        }

        if (g_MenuRepeatButtons & L_CBUTTONS) {
          g_SoundTestSoundReverb -= SOUND_TEST_COARSE_STEP;
        }

        if (g_MenuRepeatButtons & R_CBUTTONS) {
          g_SoundTestSoundReverb += SOUND_TEST_COARSE_STEP;
        }

        if (g_SoundTestSoundReverb < 0) {
          g_SoundTestSoundReverb = 0;
        }

        if (g_SoundTestSoundReverb > SOUND_TEST_MAX_REVERB) {
          g_SoundTestSoundReverb = SOUND_TEST_MAX_REVERB;
        }

        break;
      case OPTIONS_CHOICE_SOUND_FREQUENCY:
        if (g_MenuRepeatButtons & L_JPAD) {
          g_SoundTestSoundFrequency -= SOUND_TEST_FREQUENCY_FINE_STEP;
        }

        if (g_MenuRepeatButtons & R_JPAD) {
          g_SoundTestSoundFrequency += SOUND_TEST_FREQUENCY_FINE_STEP;
        }

        if (g_MenuRepeatButtons & L_CBUTTONS) {
          g_SoundTestSoundFrequency -= SOUND_TEST_FREQUENCY_COARSE_STEP;
        }

        if (g_MenuRepeatButtons & R_CBUTTONS) {
          g_SoundTestSoundFrequency += SOUND_TEST_FREQUENCY_COARSE_STEP;
        }

        if (g_SoundTestSoundFrequency < -SOUND_TEST_FREQUENCY_LIMIT) {
          g_SoundTestSoundFrequency = -SOUND_TEST_FREQUENCY_LIMIT;
        }

        if (g_SoundTestSoundFrequency > SOUND_TEST_FREQUENCY_LIMIT) {
          g_SoundTestSoundFrequency = SOUND_TEST_FREQUENCY_LIMIT;
        }

        break;
    }
  }

  SetSongReverb(g_SoundTestSongReverb);
  SetSongTempo(g_SoundTestSongTempo);
  SetSongFrequencyOffset(g_SoundTestSongFrequency);
}

void func_801FADBC_ovl_01(void) {
  if (IsHudAnimationDone() != TRUE) {
    return;
  }

  if (GetWipeKind() != WIPE_NONE) {
    if (GetWipeProgress() == 0) {
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

  if ((g_MenuRepeatButtons & U_JPAD) && g_MenuChoices.unk1 > 0) {
    PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
    g_MenuChoices.unk1--;
  } else if ((g_MenuRepeatButtons & D_JPAD) &&
      g_MenuChoices.unk1 < CADDY_COUNT - 1) {
    PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
    g_MenuChoices.unk1++;
  }
}

void LeaveMenuWithWipe(void) {
  if (GetWipeKind() != WIPE_NONE && D_801B9410 == 0) {
    if (GetWipeProgress() == 0) {
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
    if (GetWipeProgress() == 0) {
      g_MenuScreen = MENU_SCREEN_EXIT;
    }
  } else {
    LoadWipeShape(g_MenuChoices.courseIndex);
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
    g_GameState.unk31 = 2;
    return;
  }

  if (g_MenuPressedButtons & B_BUTTON) {
    PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
    if (D_801323E6) {
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
      SetTextSlot(0, 0, NO_MESSAGE, 1, 0, 1, 255, 255, 255, 255);
      func_801FDA40_ovl_01(&gfx);
      break;
    case 1:
    case 24:
      func_8020B590_ovl_01(&gfx, 0);
      break;
    case 16:
      func_8020B590_ovl_01(&gfx, 1);
      break;
    case 3:
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
    case 8:
      func_80207D08_ovl_01(&gfx);
      break;
    case 6:
    case 20:
    case 22:
      func_80202000_ovl_01(&gfx);
      break;
    case 18:
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
    case 5:
    case MENU_SCREEN_UNK23:
    case 26:
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
/* MENU_LAYER_UNK80 reads the buffer as NARROW_BOARD_WIDTH texels wide */
#define TALL_NARROW_BOARD_LEFT 16

/*
 * On some screens the banner bobs with the menu's items; the game adds the
 * first and takes the second, so it sits 4 to 14 rows lower
 */
#define BANNER_BOB_LOWERING 13.0f
#define BANNER_BOB_RAISING 4.0f

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
  gDPLoadTLUT_pal256(gfx++, ((u32)D_801F4A20 + IMAGE_HEADER_SIZE) & ~7);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);

  if (layers & MENU_LAYER_UNK1) {
    if (g_MenuScreen == MENU_SCREEN_UNK6 || g_MenuScreen == MENU_SCREEN_UNK22 ||
        g_MenuScreen == MENU_SCREEN_UNK20) {
      for (stripIndex = 0; stripIndex != CLAMPED_BACKDROP_STRIP_COUNT;
          stripIndex++) {
        if (stripIndex == 0) {
          gDPLoadTextureBlock(gfx++,
              ((u32)D_801F4A20 + LARGE_PALETTED_TEXELS_OFFSET) & ~7,
              G_IM_FMT_CI, G_IM_SIZ_8b, BACKDROP_WIDTH,
              BACKDROP_STRIP_ROW_COUNT + BACKDROP_SEAM_ROW_COUNT, 0,
              G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
              G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
          gDPPipeSync(gfx++);

          gSPScisTextureRectangle(gfx++, BACKDROP_LEFT << 2,
              BACKDROP_FIRST_TOP << 2,
              (BACKDROP_LEFT + (BACKDROP_WIDTH * BACKDROP_SCALE)) << 2,
              (BACKDROP_FIRST_TOP +
                  ((BACKDROP_STRIP_ROW_COUNT + 1) * BACKDROP_SCALE))
                  << 2,
              G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE / BACKDROP_SCALE,
              TEXEL_STEP_ONE / BACKDROP_SCALE);
        } else {
          gDPPipeSync(gfx++);
          gDPLoadTextureBlock(gfx++,
              ((u32)D_801F4A20 + LARGE_PALETTED_TEXELS_OFFSET +
                  (stripIndex * BACKDROP_WIDTH * BACKDROP_STRIP_ROW_COUNT)) &
                  ~7,
              G_IM_FMT_CI, G_IM_SIZ_8b, BACKDROP_WIDTH,
              BACKDROP_STRIP_ROW_COUNT + BACKDROP_SEAM_ROW_COUNT, 0,
              G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP,
              G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
          gDPPipeSync(gfx++);

          if (stripIndex == CLAMPED_BACKDROP_STRIP_COUNT - 1) {
            gSPScisTextureRectangle(gfx++, BACKDROP_LEFT << 2,
                CLAMPED_BACKDROP_LAST_TOP << 2,
                (BACKDROP_LEFT + (BACKDROP_WIDTH * BACKDROP_SCALE)) << 2,
                CLAMPED_BACKDROP_BOTTOM << 2, G_TX_RENDERTILE, 0,
                BACKDROP_SECOND_ROW_START, TEXEL_STEP_ONE / BACKDROP_SCALE,
                TEXEL_STEP_ONE / BACKDROP_SCALE);
          } else {
            stripTop = BACKDROP_TOP +
                (stripIndex * BACKDROP_STRIP_ROW_COUNT * BACKDROP_SCALE);
            gSPScisTextureRectangle(gfx++, BACKDROP_LEFT << 2, stripTop << 2,
                (BACKDROP_LEFT + (BACKDROP_WIDTH * BACKDROP_SCALE)) << 2,
                (stripTop + (BACKDROP_STRIP_ROW_COUNT * BACKDROP_SCALE)) << 2,
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
              ((u32)D_801F4A20 + LARGE_PALETTED_TEXELS_OFFSET) & ~7,
              G_IM_FMT_CI, G_IM_SIZ_8b, BACKDROP_WIDTH,
              BACKDROP_STRIP_ROW_COUNT + BACKDROP_SEAM_ROW_COUNT, 0,
              G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
              G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
          gDPPipeSync(gfx++);

          gSPScisTextureRectangle(gfx++, BACKDROP_LEFT << 2,
              BACKDROP_FIRST_TOP << 2,
              (BACKDROP_LEFT + (BACKDROP_WIDTH * BACKDROP_SCALE)) << 2,
              (BACKDROP_FIRST_TOP +
                  ((BACKDROP_STRIP_ROW_COUNT + 1) * BACKDROP_SCALE))
                  << 2,
              G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE / BACKDROP_SCALE,
              TEXEL_STEP_ONE / BACKDROP_SCALE);
        } else {
          gDPPipeSync(gfx++);
          gDPLoadTextureBlock(gfx++,
              ((u32)D_801F4A20 + LARGE_PALETTED_TEXELS_OFFSET +
                  (stripIndex * BACKDROP_WIDTH * BACKDROP_STRIP_ROW_COUNT)) &
                  ~7,
              G_IM_FMT_CI, G_IM_SIZ_8b, BACKDROP_WIDTH,
              shownRowCount + BACKDROP_SEAM_ROW_COUNT, 0,
              G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
              G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
          gDPPipeSync(gfx++);

          stripTop = BACKDROP_TOP +
              (stripIndex * BACKDROP_STRIP_ROW_COUNT * BACKDROP_SCALE);
          gSPScisTextureRectangle(gfx++, BACKDROP_LEFT << 2, stripTop << 2,
              (BACKDROP_LEFT + (BACKDROP_WIDTH * BACKDROP_SCALE)) << 2,
              (BACKDROP_TOP +
                  (stripIndex * BACKDROP_STRIP_ROW_COUNT * BACKDROP_SCALE) +
                  (shownRowCount * BACKDROP_SCALE))
                  << 2,
              G_TX_RENDERTILE, 0, BACKDROP_SECOND_ROW_START,
              TEXEL_STEP_ONE / BACKDROP_SCALE, TEXEL_STEP_ONE / BACKDROP_SCALE);
        }
      }
    }
  }

  if (layers & MENU_LAYER_UNK4) {
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal256(
        gfx++, ((u32)D_800FF420[D_8022BE92_ovl_01] + IMAGE_HEADER_SIZE) & ~7);

    for (stripIndex = 0; stripIndex != BOARD_STRIP_COUNT; stripIndex++) {
      gDPLoadTextureBlock(gfx++,
          ((u32)D_800FF420[D_8022BE92_ovl_01] + LARGE_PALETTED_TEXELS_OFFSET +
              (stripIndex * BOARD_WIDTH * BOARD_STRIP_HEIGHT)) &
              ~7,
          G_IM_FMT_CI, G_IM_SIZ_8b, BOARD_WIDTH, BOARD_STRIP_HEIGHT, 0,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      gDPPipeSync(gfx++);

      stripTop = BOARD_TOP + (stripIndex * BOARD_STRIP_SPACING);
      gSPScisTextureRectangle(gfx++, BOARD_LEFT << 2, stripTop << 2,
          (BOARD_LEFT + BOARD_WIDTH) << 2,
          (stripTop + BOARD_STRIP_SPACING) << 2, G_TX_RENDERTILE, 0, 0,
          TEXEL_STEP_ONE, (BOARD_STRIP_HEIGHT << 10) / BOARD_STRIP_SPACING);
      gDPPipeSync(gfx++);
    }
  }

  if (layers & MENU_LAYER_UNK80) {
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal256(
        gfx++, ((u32)D_800FF420[D_8022BE92_ovl_01] + IMAGE_HEADER_SIZE) & ~7);

    for (stripIndex = 0; stripIndex != NARROW_BOARD_STRIP_COUNT; stripIndex++) {
      gDPLoadTextureBlock(gfx++,
          ((u32)D_800FF420[D_8022BE92_ovl_01] + LARGE_PALETTED_TEXELS_OFFSET +
              (stripIndex * NARROW_BOARD_WIDTH * NARROW_BOARD_STRIP_HEIGHT)) &
              ~7,
          G_IM_FMT_CI, G_IM_SIZ_8b, NARROW_BOARD_WIDTH,
          NARROW_BOARD_STRIP_HEIGHT, 0, G_TX_NOMIRROR | G_TX_WRAP,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
          G_TX_NOLOD);
      gDPPipeSync(gfx++);

      stripTop = BOARD_TOP + (stripIndex * BOARD_STRIP_SPACING);
      gSPScisTextureRectangle(gfx++, TALL_NARROW_BOARD_LEFT << 2, stripTop << 2,
          (TALL_NARROW_BOARD_LEFT + NARROW_BOARD_WIDTH) << 2,
          (stripTop + BOARD_STRIP_SPACING) << 2, G_TX_RENDERTILE, 0, 0,
          TEXEL_STEP_ONE,
          (NARROW_BOARD_STRIP_HEIGHT << 10) / BOARD_STRIP_SPACING);
      gDPPipeSync(gfx++);
    }
  }

  bobOffset = 0;
  if (g_MenuScreen == MENU_SCREEN_TITLE ||
      g_MenuScreen == MENU_SCREEN_OPTIONS || g_MenuScreen == MENU_SCREEN_MAIN ||
      g_MenuScreen == MENU_SCREEN_MAIN_WITHOUT_TRANSMIT ||
      g_MenuScreen == MENU_SCREEN_CLUBHOUSE) {
    bobOffset = (f_MenuBobOffset + BANNER_BOB_LOWERING) - BANNER_BOB_RAISING;
  }

  if (layers & MENU_LAYER_UNK8) {
    image = D_801052BC[D_8022BE8C_ovl_01];
    gDPLoadTLUT_pal256(gfx++, ((u32)image + IMAGE_HEADER_SIZE) & ~7);

    for (stripIndex = 0; stripIndex != MENU_BANNER_STRIP_COUNT; stripIndex++) {
      gDPLoadTextureBlock(gfx++,
          ((u32)image + LARGE_PALETTED_TEXELS_OFFSET +
              (stripIndex * MENU_BANNER_WIDTH * MENU_BANNER_STRIP_HEIGHT)) &
              ~7,
          G_IM_FMT_CI, G_IM_SIZ_8b, MENU_BANNER_WIDTH, MENU_BANNER_STRIP_HEIGHT,
          0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      gDPPipeSync(gfx++);

      stripTop = bobOffset + (stripIndex * MENU_BANNER_STRIP_HEIGHT);
      gSPScisTextureRectangle(gfx++, MENU_BANNER_LEFT << 2,
          (stripTop + MENU_BANNER_TOP) << 2,
          (MENU_BANNER_LEFT + MENU_BANNER_WIDTH) << 2,
          (stripTop + MENU_BANNER_TOP + MENU_BANNER_STRIP_HEIGHT) << 2,
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
  if (layers & MENU_LAYER_UNK1) {
    func_801FC754_ovl_01(&gfx);
  }

  if (layers & MENU_LAYER_UNK4) {
    func_801FCB74_ovl_01(&gfx);
  }

  if (layers & MENU_LAYER_UNK10) {
    func_801FD4F0_ovl_01(&gfx);
  }

  if (layers & MENU_LAYER_UNK8) {
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
  gDPLoadTLUT_pal256(gfx++, ((u32)D_801F4A20 + IMAGE_HEADER_SIZE) & ~7);
  for (stripIndex = 0; stripIndex != BACKDROP_STRIP_COUNT; stripIndex++) {
    /* The image ends in a strip of one row */
    shownRowCount =
        (stripIndex != BACKDROP_STRIP_COUNT - 1) ? BACKDROP_STRIP_ROW_COUNT : 1;
    if (stripIndex == 0) {
      /* The first strip has no row above it, so it shows its first row too */
      gDPLoadTextureBlock(gfx++,
          ((u32)D_801F4A20 + LARGE_PALETTED_TEXELS_OFFSET) & ~7, G_IM_FMT_CI,
          G_IM_SIZ_8b, BACKDROP_WIDTH,
          BACKDROP_STRIP_ROW_COUNT + BACKDROP_SEAM_ROW_COUNT, 0,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      gDPPipeSync(gfx++);
      gSPScisTextureRectangle(gfx++, BACKDROP_LEFT << 2,
          BACKDROP_FIRST_TOP << 2,
          (BACKDROP_LEFT + (BACKDROP_WIDTH * BACKDROP_SCALE)) << 2,
          (BACKDROP_FIRST_TOP +
              ((BACKDROP_STRIP_ROW_COUNT + 1) * BACKDROP_SCALE))
              << 2,
          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE / BACKDROP_SCALE,
          TEXEL_STEP_ONE / BACKDROP_SCALE);
    } else {
      gDPPipeSync(gfx++);
      gDPLoadTextureBlock(gfx++,
          ((u32)D_801F4A20 + LARGE_PALETTED_TEXELS_OFFSET +
              (stripIndex * BACKDROP_WIDTH * BACKDROP_STRIP_ROW_COUNT)) &
              ~7,
          G_IM_FMT_CI, G_IM_SIZ_8b, BACKDROP_WIDTH,
          shownRowCount + BACKDROP_SEAM_ROW_COUNT, 0, G_TX_NOMIRROR | G_TX_WRAP,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
          G_TX_NOLOD);
      gDPPipeSync(gfx++);
      stripTop = BACKDROP_TOP +
          (stripIndex * BACKDROP_STRIP_ROW_COUNT * BACKDROP_SCALE);
      gSPScisTextureRectangle(gfx++, BACKDROP_LEFT << 2, stripTop << 2,
          (BACKDROP_LEFT + (BACKDROP_WIDTH * BACKDROP_SCALE)) << 2,
          (BACKDROP_TOP +
              (stripIndex * BACKDROP_STRIP_ROW_COUNT * BACKDROP_SCALE) +
              (shownRowCount * BACKDROP_SCALE))
              << 2,
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
      gfx++, ((u32)D_800FF420[D_8022BE92_ovl_01] + IMAGE_HEADER_SIZE) & ~7);
  for (stripIndex = 0; stripIndex != NARROW_BOARD_STRIP_COUNT; stripIndex++) {
    gDPLoadTextureBlock(gfx++,
        ((u32)D_800FF420[D_8022BE92_ovl_01] + LARGE_PALETTED_TEXELS_OFFSET +
            (stripIndex * NARROW_BOARD_WIDTH * NARROW_BOARD_STRIP_HEIGHT)) &
            ~7,
        G_IM_FMT_CI, G_IM_SIZ_8b, NARROW_BOARD_WIDTH, NARROW_BOARD_STRIP_HEIGHT,
        0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    stripTop = NARROW_BOARD_TOP + (stripIndex * NARROW_BOARD_STRIP_SPACING);
    gSPScisTextureRectangle(gfx++, NARROW_BOARD_LEFT << 2, stripTop << 2,
        (NARROW_BOARD_LEFT + NARROW_BOARD_SHOWN_WIDTH) << 2,
        (stripTop + NARROW_BOARD_STRIP_HEIGHT) << 2, G_TX_RENDERTILE, 0, 0,
        (NARROW_BOARD_WIDTH << 10) / NARROW_BOARD_SHOWN_WIDTH,
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
  gDPLoadTLUT_pal256(gfx++, ((u32)D_801B60D0 + IMAGE_HEADER_SIZE) & ~7);
  for (stripIndex = 0; stripIndex != MENU_BANNER_STRIP_COUNT; stripIndex++) {
    gDPLoadTextureBlock(gfx++,
        ((u32)D_801B60D0 + LARGE_PALETTED_TEXELS_OFFSET +
            (stripIndex * MENU_BANNER_WIDTH * MENU_BANNER_STRIP_HEIGHT)) &
            ~7,
        G_IM_FMT_CI, G_IM_SIZ_8b, MENU_BANNER_WIDTH, MENU_BANNER_STRIP_HEIGHT,
        0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    stripTop = UNUSED_BANNER_TOP + (stripIndex * MENU_BANNER_STRIP_HEIGHT);
    gSPScisTextureRectangle(gfx++, MENU_BANNER_LEFT << 2, stripTop << 2,
        (MENU_BANNER_LEFT + MENU_BANNER_WIDTH) << 2,
        (stripTop + MENU_BANNER_STRIP_HEIGHT) << 2, G_TX_RENDERTILE, 0, 0,
        TEXEL_STEP_ONE, TEXEL_STEP_ONE);
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
  gDPLoadTLUT_pal256(gfx++, ((u32)D_801B60D0 + IMAGE_HEADER_SIZE) & ~7);
  for (stripIndex = 0; stripIndex != MENU_BANNER_STRIP_COUNT; stripIndex++) {
    gDPLoadTextureBlock(gfx++,
        ((u32)D_801B60D0 + LARGE_PALETTED_TEXELS_OFFSET +
            stripIndex * MENU_BANNER_WIDTH * MENU_BANNER_STRIP_HEIGHT) &
            ~7,
        G_IM_FMT_CI, G_IM_SIZ_8b, MENU_BANNER_WIDTH, MENU_BANNER_STRIP_HEIGHT,
        0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, left << 2, top << 2,
        (left + SMALL_BANNER_WIDTH) << 2, (top + MENU_BANNER_STRIP_HEIGHT) << 2,
        G_TX_RENDERTILE, 0, 0, (MENU_BANNER_WIDTH << 10) / SMALL_BANNER_WIDTH,
        TEXEL_STEP_ONE);
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
  gDPLoadTLUT_pal256(gfx++, ((u32)D_801B60D0 + IMAGE_HEADER_SIZE) & ~7);
  for (stripIndex = 0; stripIndex != MENU_BANNER_STRIP_COUNT; stripIndex++) {
    gDPLoadTextureBlock(gfx++,
        ((u32)D_801B60D0 + LARGE_PALETTED_TEXELS_OFFSET +
            (stripIndex * MENU_BANNER_WIDTH * MENU_BANNER_STRIP_HEIGHT)) &
            ~7,
        G_IM_FMT_CI, G_IM_SIZ_8b, MENU_BANNER_WIDTH, MENU_BANNER_STRIP_HEIGHT,
        0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    stripTop = LARGE_BANNER_TOP + (stripIndex * LARGE_BANNER_STRIP_SPACING);
    gSPScisTextureRectangle(gfx++, LARGE_BANNER_LEFT << 2, stripTop << 2,
        (LARGE_BANNER_LEFT + LARGE_BANNER_WIDTH) << 2,
        (stripTop + LARGE_BANNER_STRIP_HEIGHT) << 2, G_TX_RENDERTILE, 0, 0,
        (MENU_BANNER_WIDTH << 10) / LARGE_BANNER_WIDTH,
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

  gDPLoadTLUT_pal256(gfx++, ((u32)image + IMAGE_HEADER_SIZE) & ~7);
  for (stripIndex = 0; stripIndex != MENU_BANNER_STRIP_COUNT; stripIndex++) {
    gDPLoadTextureBlock(gfx++,
        ((u32)image + LARGE_PALETTED_TEXELS_OFFSET +
            (stripIndex * MENU_BANNER_WIDTH * MENU_BANNER_STRIP_HEIGHT)) &
            ~7,
        G_IM_FMT_CI, G_IM_SIZ_8b, MENU_BANNER_WIDTH, MENU_BANNER_STRIP_HEIGHT,
        0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    stripTop = MENU_BANNER_TOP + (stripIndex * MENU_BANNER_STRIP_HEIGHT);
    gSPScisTextureRectangle(gfx++, MENU_BANNER_LEFT << 2, stripTop << 2,
        (MENU_BANNER_LEFT + MENU_BANNER_WIDTH) << 2,
        (stripTop + MENU_BANNER_STRIP_HEIGHT) << 2, G_TX_RENDERTILE, 0, 0,
        TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    gDPPipeSync(gfx++);
  }

  *displayListHead = gfx;
}

/* The title screen's labels, 128 by 20 each, plain in D_801B5CD0 and lit in
 * D_8012D420 */
#define TITLE_LABEL_COUNT 7
#define TITLE_LABEL_WIDTH 128
#define TITLE_LABEL_HEIGHT 20
#define TITLE_LABEL_IMAGE_SIZE (TITLE_LABEL_WIDTH * TITLE_LABEL_HEIGHT / 2)
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
 * three figures */
#define PLAIN_MENU_HEADING_X 64
#define PLAIN_MENU_HEADING_Y 32
#define PLAIN_MENU_NAME_COUNT 3
#define PLAIN_MENU_NAME_X 112
#define PLAIN_MENU_NAME_TOP 128
#define PLAIN_MENU_NAME_SPACING 8
#define PLAIN_MENU_FIGURES_X 32
#define PLAIN_MENU_FIGURES_Y 100
#define TITLE_BOARD_KIND 61

extern char* D_8022BF28_ovl_01[];
extern s32 D_8022F940_ovl_01[TITLE_LABEL_COUNT][2];

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

  sprintf(g_PrintBuffer, D_80231640_ovl_01, 0x820, 0x208, 0x2A78);
  DrawShadowedText(
      &gfx, PLAIN_MENU_FIGURES_X, PLAIN_MENU_FIGURES_Y, g_PrintBuffer);
  func_801FB54C_ovl_01(&gfx, TITLE_BOARD_KIND);
  if (g_ControllerStates[0].buttons & B_BUTTON) {
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_AVERAGE);
  }

  gDPLoadTLUT_pal16(
      gfx++, 0, ((u32)D_801B5CD0[D_8022BE90_ovl_01] + IMAGE_HEADER_SIZE) & ~7);
  for (labelIndex = 0; labelIndex != TITLE_LABEL_COUNT; labelIndex++) {
    if ((D_801323A0.titleChoice == TITLE_CHOICE_NEW_GAME &&
            labelIndex == TITLE_LABEL_NEW_GAME) ||
        (D_801323A0.titleChoice == TITLE_CHOICE_NEW_GAME &&
            labelIndex ==
                D_801323A0.playerCountChoice +
                    TITLE_LABEL_FIRST_PLAYER_COUNT) ||
        (D_801323A0.titleChoice + TITLE_LABEL_FIRST_OTHER_CHOICE ==
                labelIndex &&
            D_801323A0.titleChoice != TITLE_CHOICE_NEW_GAME)) {
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
          ((u32)D_801B5CD0[D_8022BE90_ovl_01] + PALETTED_TEXELS_OFFSET +
              labelOffset) &
              ~7,
          G_IM_FMT_CI, TITLE_LABEL_WIDTH, TITLE_LABEL_HEIGHT, 0,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      gDPPipeSync(gfx++);
      gSPScisTextureRectangle(gfx++, D_8022F940_ovl_01[labelIndex][0] << 2,
          D_8022F940_ovl_01[labelIndex][1] << 2,
          (D_8022F940_ovl_01[labelIndex][0] + TITLE_LABEL_WIDTH) << 2,
          (D_8022F940_ovl_01[labelIndex][1] + TITLE_LABEL_HEIGHT) << 2,
          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
      gDPPipeSync(gfx++);
    }
  }

  gDPLoadTLUT_pal16(
      gfx++, 0, ((u32)D_8012D420[D_8022BE8E_ovl_01] + IMAGE_HEADER_SIZE) & ~7);
  for (labelIndex = 0; labelIndex != TITLE_LABEL_COUNT; labelIndex++) {
    if ((D_801323A0.titleChoice == TITLE_CHOICE_NEW_GAME &&
            labelIndex == TITLE_LABEL_NEW_GAME) ||
        (D_801323A0.titleChoice == TITLE_CHOICE_NEW_GAME &&
            labelIndex ==
                D_801323A0.playerCountChoice +
                    TITLE_LABEL_FIRST_PLAYER_COUNT) ||
        (labelIndex ==
                D_801323A0.titleChoice + TITLE_LABEL_FIRST_OTHER_CHOICE &&
            D_801323A0.titleChoice != TITLE_CHOICE_NEW_GAME)) {
      if (labelIndex == TITLE_LABEL_SAVED_GAMES &&
          g_HasBroughtOverCharacter == TRUE) {
        labelOffset = TITLE_LABEL_TRANSMIT * TITLE_LABEL_IMAGE_SIZE;
      } else {
        labelOffset = labelIndex * TITLE_LABEL_IMAGE_SIZE;
      }

      gDPLoadTextureBlock_4b(gfx++,
          ((u32)D_8012D420[D_8022BE8E_ovl_01] + PALETTED_TEXELS_OFFSET +
              labelOffset) &
              ~7,
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
          (D_8022F940_ovl_01[labelIndex][0] - (growth / 2)) << 2,
          (D_8022F940_ovl_01[labelIndex][1] - (growth / 2)) << 2,
          (D_8022F940_ovl_01[labelIndex][0] + TITLE_LABEL_WIDTH + (growth / 2))
              << 2,
          (D_8022F940_ovl_01[labelIndex][1] +
              (TITLE_LABEL_HEIGHT + (growth / 2)))
              << 2,
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
