/*
 * The pause menu over a round, and the choice box that asks the player to
 * pick between a few options during a turn.
 */
#include "common.h"

#include "pause_menu.h"

#include "asset.h"
#include "billboard.h"
#include "controller.h"
#include "course.h"
#include "current_shot.h"
#include "effects.h"
#include "functions.h"
#include "heap.h"
#include "hole_display.h"
#include "main.h"
#include "overlay_manager.h"
#include "play.h"
#include "round_hud.h"
#include "save.h"
#include "scene.h"
#include "screen.h"
#include "screen_layers.h"
#include "sound.h"
#include "surface.h"
#include "text.h"
#include "transfer_pak.h"
#include "turn.h"
#include "variables.h"

/* The small cloud behind a choice box */
#define CHOICE_BOX_CLOUD_IMAGE_ASSET 0x71F

#define ROUND_FRAMEBUFFER_COUNT 3

/* Values of f_PauseMenuRequest, which the pause menu's drawing carries out */
enum PauseMenuRequest {
  PAUSE_MENU_REQUEST_FREE = -1,
  PAUSE_MENU_REQUEST_NONE,
  PAUSE_MENU_REQUEST_LOAD
};

/* The pause menu's option lists, which the game mode picks */
#define PAUSE_LAYOUT_NONE (-1) /* For a record being played back */
#define PAUSE_LAYOUT_SPEED_GOLF 0
#define PAUSE_LAYOUT_RING_SHOT 1
#define PAUSE_LAYOUT_TRAINING 2
#define PAUSE_LAYOUT_TRAINING_MINI_GOLF 3
#define PAUSE_LAYOUT_DRIVING_RANGE 4
/* Match Game, Get Character, Skins Match and Club Slots */
#define PAUSE_LAYOUT_VERSUS 5
#define PAUSE_LAYOUT_STROKE 6 /* Stroke, Tournament and the modes like them */
/* The layouts used while g_HasBroughtOverCharacter is set */
#define PAUSE_LAYOUT_BROUGHT_OVER_SPEED_GOLF 7
#define PAUSE_LAYOUT_BROUGHT_OVER 8
/* The layouts used while g_GameState.isClubTournament is set */
#define PAUSE_LAYOUT_UNK3B_SPEED_GOLF 9
#define PAUSE_LAYOUT_UNK3B 10

#define PAUSE_MENU_SONG_VOLUME 0x60

#define PAUSE_OPTION_KIND_WEATHER 1 /* Shows whether it rains */
/* Shows the player's grid colour as black or white */
#define PAUSE_OPTION_KIND_GRID_COLOR 2
#define PAUSE_OPTION_KIND_WIND_SPEED 3
#define PAUSE_OPTION_KIND_WIND_DIRECTION 4
#define PAUSE_OPTION_KIND_LIE 5

extern s32 f_IsPauseMenuLoaded;
extern s32 f_PauseSelection;
extern s32 f_PauseLayout;
extern s32 f_PauseMenuRequest;

static u8* f_PauseCursorImage;
static u8* f_MenuCloudImage; /* The pause menu's cloud, or the choice box's */
static u8* f_PauseArrowsImage;
extern s32 f_PauseArrowsTick;
static u8* f_ChoiceBoxCursorImage;

/* Not static: the original left it to the linker's common block */
s32 f_PauseMenuUnkD678;

#define CHOICE_BOX_MAX_OPTIONS 4
#define CHOICE_BOX_OPEN_SOUND 0x64
#define CHOICE_BOX_CLOSE_SOUND 0x65
#define CHOICE_BOX_MOVE_SOUND 0x5C
#define CHOICE_BOX_CHOOSE_SOUND 0x5D
#define MENU_SOUND_VOLUME 100

/* Values of f_ChoiceBoxState */
enum ChoiceBoxState {
  CHOICE_BOX_CLOSED = -1,
  CHOICE_BOX_CHOOSING,
  CHOICE_BOX_CHOSEN, /* Waits a frame before the box closes */
  CHOICE_BOX_CLOSING
};

typedef struct ChoiceBox {
  /* 0x00 */ s32 optionCount;
  /* 0x04 */ s32 halfWidth; /* Widened to fit the longest label */
  /* 0x08 */ s32 isVertical;
  /* 0x0C */ u8
      labels[CHOICE_BOX_MAX_OPTIONS]; /* Indexes into f_ChoiceBoxLabels */
  /* 0x10 */ u8 results[CHOICE_BOX_MAX_OPTIONS];
} ChoiceBox; /* size = 0x14 */

extern ChoiceBox f_ChoiceBoxes[];
extern char* f_ChoiceBoxLabels[];
extern s32 f_ChoiceBoxState;
extern s32 f_ChoiceBoxIndex;
extern s32 f_ChoiceBoxSelection;
extern f32 f_ChoiceGlowAngle; /* Cycles the selected label's colour */
extern f32 f_ChoiceCursorAngle; /* Bobs the cursor from side to side */

#define CHOICE_BOX_CENTER_X (SCREEN_WIDTH / 2)
#define CHOICE_BOX_MARGIN 32
#define CHOICE_BOX_TOP 72
#define CHOICE_BOX_BOTTOM 164
#define CHOICE_BOX_CLOUD_WIDTH 128
#define CHOICE_BOX_CLOUD_HEIGHT 64
#define CHOICE_BOX_CLOUD_PART_COUNT 1
#define CHOICE_BOX_CLOUD_PART_SIZE 0x1000
/* The cloud's height scaled to the box's, as a 5.10 texel step */
#define CHOICE_BOX_CLOUD_DTDY 0x2C8
#define CHOICE_LABELS_MIDDLE_Y 113
#define CHOICE_LABEL_SPACING 16
#define CHOICE_CURSOR_SIZE 24
#define CHOICE_CURSOR_LOADED_HEIGHT 76
/* The cursor bobs about this x, left of the selected label's middle */
#define CHOICE_CURSOR_X 124
#define CHOICE_CURSOR_SWAY 8.0f
#define COLOR_WAVE_MIDDLE 127
#define COLOR_WAVE_AMPLITUDE 127.0f

extern s8 f_PauseLayoutOptionCounts[];
/* Written once the menu is drawn and cleared by its input, but never read */
extern s32 f_PauseMenuDrawnMarker;

void SetFramebufferCount(s32 count);

void HideRoundHud(void) {
  g_RoundHud.isLetterboxShown = FALSE;
  g_RoundHud.unkC = 0;
  g_RoundHud.isWindIndicatorShown = 0;
  g_RoundHud.areLieArrowsShown = 0;
  g_RoundHud.areWindSpeedArrowsShown = 0;
  g_RoundHud.areWindDirectionArrowsShown = 0;
  g_RoundHud.unk8 = 0;
  g_RoundHud.isLieIconShown = 0;
  g_RoundHud.isClubHeadIconShown = 0;
  g_RoundHud.unkB = 0;
  g_RoundHud.unkD = 0;
}

void ResumeRound(void) {
  g_RoundHud.isLetterboxShown = TRUE;
  g_RoundHud.unkC = 1;
  g_RoundHud.isWindIndicatorShown = 1;
  g_RoundHud.areLieArrowsShown = 0;
  g_RoundHud.areWindSpeedArrowsShown = 0;
  g_RoundHud.areWindDirectionArrowsShown = 0;
  g_RoundHud.unk8 = 1;
  g_RoundHud.isLieIconShown = 1;
  g_RoundHud.isClubHeadIconShown = 1;
  g_RoundHud.unkB = 1;
  g_RoundHud.unkD = 1;
  SetMenuBackdrop(MENU_BACKDROP_NONE);
  g_PauseScreen = PAUSE_SCREEN_NONE;
  SetFramebufferCount(ROUND_FRAMEBUFFER_COUNT);
  SetScreenEffect(SCREEN_EFFECT_NONE, 0);
}

void RequestPauseMenu(s32 unkD678, s32 isClosing) {
  f_PauseMenuRequest =
      isClosing == FALSE ? PAUSE_MENU_REQUEST_LOAD : PAUSE_MENU_REQUEST_FREE;
  f_PauseMenuUnkD678 = unkD678;
}

INCLUDE_RODATA("asm/nonmatchings/ovl_00/pause_menu", D_80225440_ovl_00);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/pause_menu", D_80225450_ovl_00);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/pause_menu", D_8022545C_ovl_00);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/pause_menu", D_80225468_ovl_00);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/pause_menu", D_80225474_ovl_00);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/pause_menu", D_80225480_ovl_00);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/pause_menu", D_80225484_ovl_00);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/pause_menu", D_80225488_ovl_00);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/pause_menu", D_80225490_ovl_00);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/pause_menu", D_80225498_ovl_00);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/pause_menu", D_802254A0_ovl_00);

void LoadPauseMenu(void) {
  EncodedAsset asset;

  if (!f_IsPauseMenuLoaded) {
    f_IsPauseMenuLoaded = TRUE;
    f_PauseSelection = 0;
    if (g_GameState.isClubTournament) {
      if (g_GameState.gameMode == GAME_MODE_SPEED_GOLF) {
        f_PauseLayout = PAUSE_LAYOUT_UNK3B_SPEED_GOLF;
      } else {
        f_PauseLayout = PAUSE_LAYOUT_UNK3B;
      }
    } else if (g_HasBroughtOverCharacter) {
      if (g_GameState.gameMode == GAME_MODE_SPEED_GOLF) {
        f_PauseLayout = PAUSE_LAYOUT_BROUGHT_OVER_SPEED_GOLF;
      } else if (g_GameState.gameMode == GAME_MODE_RING_SHOT) {
        f_PauseLayout = PAUSE_LAYOUT_RING_SHOT;
      } else if (g_GameState.gameMode == GAME_MODE_TRAINING) {
        if (g_GameState.isMiniGolf == TRUE) {
          f_PauseLayout = PAUSE_LAYOUT_TRAINING_MINI_GOLF;
        } else if (GetCourseRecordIndex() == COURSE_RECORD_DRIVING_RANGE) {
          f_PauseLayout = PAUSE_LAYOUT_DRIVING_RANGE;
        } else {
          f_PauseLayout = PAUSE_LAYOUT_TRAINING;
        }
      } else {
        f_PauseLayout = PAUSE_LAYOUT_BROUGHT_OVER;
      }
    } else {
      switch (g_GameState.gameMode) {
        case GAME_MODE_SPEED_GOLF:
          f_PauseLayout = PAUSE_LAYOUT_SPEED_GOLF;
          break;
        case GAME_MODE_TRAINING:
          if (g_GameState.isMiniGolf == TRUE) {
            f_PauseLayout = PAUSE_LAYOUT_TRAINING_MINI_GOLF;
          } else if (GetCourseRecordIndex() == COURSE_RECORD_DRIVING_RANGE) {
            f_PauseLayout = PAUSE_LAYOUT_DRIVING_RANGE;
          } else {
            f_PauseLayout = PAUSE_LAYOUT_TRAINING;
          }

          break;
        case GAME_MODE_RING_SHOT:
          f_PauseLayout = PAUSE_LAYOUT_RING_SHOT;
          break;
        case GAME_MODE_MATCH_GAME:
        case GAME_MODE_GET_CHARACTER:
        case GAME_MODE_SKINS_MATCH:
        case GAME_MODE_CLUB_SLOTS:
          f_PauseLayout = PAUSE_LAYOUT_VERSUS;
          break;
        case GAME_MODE_UNK8:
          f_PauseLayout = PAUSE_LAYOUT_NONE;
          break;
        case GAME_MODE_STROKE:
        case GAME_MODE_UNK1:
        case GAME_MODE_TOURNAMENT:
        case GAME_MODE_UNK6:
          f_PauseLayout = PAUSE_LAYOUT_STROKE;
          break;
      }
    }

    f_PauseCursorImage =
        AllocMemory(OpenEncodedAsset(ASSET_POINTING_HAND, &asset));
    ReadEncodedAsset(f_PauseCursorImage, &asset);
    f_MenuCloudImage = AllocMemory(OpenEncodedAsset(ASSET_WIDE_CLOUD, &asset));
    ReadEncodedAsset(f_MenuCloudImage, &asset);
    g_WindPanelPosition[AXIS_X] = SCREEN_WIDTH;
    g_WindPanelPosition[AXIS_Y] = SCREEN_HEIGHT;
    g_LiePanelPosition[AXIS_X] = SCREEN_HEIGHT;
    f_PauseArrowsImage =
        AllocMemory(OpenEncodedAsset(ASSET_GREEN_ARROWS_COPY, &asset));
    ReadEncodedAsset(f_PauseArrowsImage, &asset);
    SetSongVolume(PAUSE_MENU_SONG_VOLUME);
  }
}

void FreePauseMenu(void) {
  if (f_IsPauseMenuLoaded == TRUE) {
    f_IsPauseMenuLoaded = FALSE;
    FreeMemory(&f_PauseCursorImage);
    FreeMemory(&f_MenuCloudImage);
    FreeMemory(&f_PauseArrowsImage);
  }

  SetSongVolume(SOUND_FULL_SCALE);
}

/* What a pause option shows beside its label, and how the cursor points at it
 */
enum PauseOptionKind {
  PAUSE_OPTION_KIND_PLAIN,
  /* Shows another label while f_PauseMenuUnkD678 is clear */
  PAUSE_OPTION_KIND_UNKD678 = 7
};

/* The option that is greyed out while f_PauseMenuUnkD678 is clear */
#define PAUSE_OPTION_UNKD678 2

#define PAUSE_LAYOUT_MAX_OPTIONS 9
#define PAUSE_OPTION_MESSAGE_BASE 0x77
#define PAUSE_OPTION_UNKD678_OFF_MESSAGE 0xE

typedef struct PauseOption {
  /* 0x0 */ s16 message; /* Counted from PAUSE_OPTION_MESSAGE_BASE */
  /* 0x2 */ s16
      action; /* A PAUSE_ACTION_* value, done when the option is chosen */
  /* 0x4 */ u16 kind;
} PauseOption; /* size = 0x6 */

extern PauseOption f_PauseOptions[];
extern s8 f_PauseLayoutOptions[][PAUSE_LAYOUT_MAX_OPTIONS];
extern char* f_ClearText;
extern char* f_RainText;
extern char* f_BlackText;
extern char* f_WhiteText;
extern f32 f_PauseGlowAngle;
extern f32 f_PauseCursorAngle;

/* What choosing a pause option does, as its PauseOption.action */
enum PauseAction {
  PAUSE_ACTION_FREE_LOOK,
  PAUSE_ACTION_SCORECARD,
  /* Only while debugging or with f_PauseMenuUnkD678 set */
  PAUSE_ACTION_UNKD678,
  PAUSE_ACTION_UNK3,
  PAUSE_ACTION_UNK4,
  PAUSE_ACTION_UNK6 = 6,
  PAUSE_ACTION_RETRY_HOLE = 9,
  PAUSE_ACTION_UNK13 = 13,
  PAUSE_ACTION_UNK14,
  PAUSE_ACTION_UNK15,
  /* UpdatePauseMenu's result while no option is chosen */
  PAUSE_ACTION_NONE = -1
};

/* The prompts that some options open */
enum PromptId {
  PROMPT_UNK3 = 3,
  PROMPT_UNK4,
  PROMPT_UNK7 = 7,
  PROMPT_UNK8
};

#define RING_SHOT_RETRY_SOUND_CHANNEL 3

/* Where the panels of the wind and of the lie slide to while their option is
 * selected */
#define PAUSE_PANEL_SHOWN_X 72.0f
#define PAUSE_PANEL_SHOWN_Y 44.0f
#define PAUSE_PANEL_HIDDEN_X 224.0f
#define PAUSE_PANEL_HIDDEN_Y 184.0f
#define PAUSE_PANEL_EASING 0.5f

/*
 * The wind speed steps by about a mile an hour as the menu shows it, but
 * from calm the first step is larger, so that the shown speed starts at one.
 */
#define PAUSE_WIND_SPEED_STEP 0x71C7
#define PAUSE_WIND_SPEED_FIRST_STEP 0xAAAA
#define PAUSE_WIND_SPEED_CALM 0x93E8 /* Slower winds than this are calm */
#define PAUSE_WIND_SPEED_MAX 0x99CF0
#define PAUSE_WIND_SPEED_RAISE_LIMIT 0x9FFFF
#define PAUSE_WIND_TURN_PER_STICK 5
#define PAUSE_WIND_TURN_STEP 0xF0

/* The lies that the lie option sets the ball in, from the tee's */
#define PAUSE_LIE_TEE_UP 0
#define PAUSE_LIE_FAIRWAY 1
#define PAUSE_LIE_FAIRWAY_FAIR_LIE 2
#define PAUSE_LIE_FAIRWAY_POOR_LIE 3
#define PAUSE_LIE_SEMI_ROUGH 4
#define PAUSE_LIE_ROUGH 5
#define PAUSE_LIE_ROUGH_POOR_LIE 6
#define PAUSE_LIE_DEEP_ROUGH 7
#define PAUSE_LIE_BUNKER 8
#define PAUSE_LIE_BUNKER_FAIR_LIE 9
#define PAUSE_LIE_BUNKER_POOR_LIE 10
#define PAUSE_LIE_COUNT 11
/* The ball moves across from the tee onto the patches of the rougher lies */
#define PAUSE_ROUGH_LIES_OFFSET_X (-0x2D000)
#define PAUSE_DEEP_ROUGH_OFFSET_X (-0x5A000)
#define PAUSE_BUNKER_LIES_OFFSET_X 0x2D000
/* The tee holds the ball up off the ground */
#define PAUSE_TEE_UP_HEIGHT 307.2f

/*
 * Takes the player's input on the pause menu. It returns the chosen option's
 * action, or PAUSE_ACTION_NONE. The weather, wind and lie options are changed
 * with the control pad's left and right.
 */
s32 UpdatePauseMenu(ControllerState* controller) {
  s32 action;
  u16 kind;
  s32 lie;
  s32 hasLieChanged;
  s32 surface;

  if (controller->repeatedButtons & A_BUTTON) {
    action =
        f_PauseOptions[f_PauseLayoutOptions[f_PauseLayout][f_PauseSelection]]
            .action;
    switch (action) {
      case PAUSE_ACTION_UNK4:
        PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, MENU_SOUND_VOLUME);
        RequestPauseMenu(0, -1);
        g_PauseScreen = PAUSE_SCREEN_UNK6;
        OpenPrompt(PROMPT_UNK4);
        break;
      case PAUSE_ACTION_UNK14:
        PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, MENU_SOUND_VOLUME);
        RequestPauseMenu(0, -1);
        g_PauseScreen = PAUSE_SCREEN_UNK6;
        OpenPrompt(PROMPT_UNK7);
        break;
      case PAUSE_ACTION_UNK15:
        PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, MENU_SOUND_VOLUME);
        RequestPauseMenu(0, -1);
        g_PauseScreen = PAUSE_SCREEN_UNK6;
        OpenPrompt(PROMPT_UNK8);
        break;
      case PAUSE_ACTION_UNK3:
        PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, MENU_SOUND_VOLUME);
        RequestPauseMenu(0, -1);
        g_PauseScreen = PAUSE_SCREEN_UNK8;
        g_SubModuleState = SUB_MODULE_SWAP_PENDING;
        g_PendingSubModuleId = MODULE_SAVE_MENU;
        break;
      case PAUSE_ACTION_UNK13:
        PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, MENU_SOUND_VOLUME);
        RequestPauseMenu(0, -1);
        g_PauseScreen = PAUSE_SCREEN_UNK9;
        g_SubModuleState = SUB_MODULE_SWAP_PENDING;
        g_PendingSubModuleId = MODULE_SAVE_MENU;
        break;
      case PAUSE_ACTION_UNKD678:
        if (g_IsDebugEnabled || f_PauseMenuUnkD678) {
          PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, MENU_SOUND_VOLUME);
          RequestPauseMenu(0, -1);
          g_PauseScreen = PAUSE_SCREEN_UNK6;
          OpenPrompt(PROMPT_UNK3);
        }

        break;
      case PAUSE_ACTION_SCORECARD:
        RequestPauseMenu(0, -1);
        g_PauseScreen = PAUSE_SCREEN_SCORECARD;
        g_SubModuleState = SUB_MODULE_SWAP_PENDING;
        g_PendingSubModuleId = MODULE_SCORECARD;
        break;
      case PAUSE_ACTION_FREE_LOOK:
        RequestPauseMenu(0, -1);
        PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, MENU_SOUND_VOLUME);
        StartFreeLook();
        break;
      case PAUSE_ACTION_RETRY_HOLE:
        if (g_GameState.gameMode == GAME_MODE_RING_SHOT) {
          PlaySound(SOUND_RING_SHOT_RETRY, RING_SHOT_RETRY_SOUND_CHANNEL,
              SOUND_FULL_SCALE);
        }
      case PAUSE_ACTION_UNK6:
        PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, MENU_SOUND_VOLUME);
        break;
    }

    return action;
  }

  if (controller->repeatedButtons & U_JPAD) {
    f_PauseSelection--;
    PlaySound(SOUND_MENU_MOVE, g_NextSoundChannel, MENU_SOUND_VOLUME);
  } else if (controller->repeatedButtons & D_JPAD) {
    f_PauseSelection++;
    PlaySound(SOUND_MENU_MOVE, g_NextSoundChannel, MENU_SOUND_VOLUME);
  }

  if (f_PauseSelection > f_PauseLayoutOptionCounts[f_PauseLayout] - 1) {
    f_PauseSelection = 0;
  }

  if (f_PauseSelection < 0) {
    f_PauseSelection = f_PauseLayoutOptionCounts[f_PauseLayout] - 1;
  }

  kind = f_PauseOptions[f_PauseLayoutOptions[f_PauseLayout][f_PauseSelection]]
             .kind;
  f_PauseMenuDrawnMarker = 0;
  g_RoundHud.areLieArrowsShown = 0;
  g_RoundHud.areWindSpeedArrowsShown = 0;
  g_RoundHud.areWindDirectionArrowsShown = 0;
  if (kind == PAUSE_OPTION_KIND_WIND_SPEED ||
      kind == PAUSE_OPTION_KIND_WIND_DIRECTION) {
    g_WindPanelPosition[AXIS_X] +=
        (PAUSE_PANEL_SHOWN_X - g_WindPanelPosition[AXIS_X]) *
        PAUSE_PANEL_EASING;
    g_WindPanelPosition[AXIS_Y] +=
        (PAUSE_PANEL_SHOWN_Y - g_WindPanelPosition[AXIS_Y]) *
        PAUSE_PANEL_EASING;
  } else {
    g_WindPanelPosition[AXIS_X] = PAUSE_PANEL_HIDDEN_X;
    g_WindPanelPosition[AXIS_Y] = PAUSE_PANEL_HIDDEN_Y;
  }

  if (kind == PAUSE_OPTION_KIND_LIE) {
    g_LiePanelPosition[AXIS_X] +=
        (PAUSE_PANEL_SHOWN_X - g_LiePanelPosition[AXIS_X]) * PAUSE_PANEL_EASING;
    g_LiePanelPosition[AXIS_Y] +=
        (PAUSE_PANEL_SHOWN_Y - g_LiePanelPosition[AXIS_Y]) * PAUSE_PANEL_EASING;
  } else {
    g_LiePanelPosition[AXIS_X] = PAUSE_PANEL_HIDDEN_X;
    g_LiePanelPosition[AXIS_Y] = -PAUSE_PANEL_HIDDEN_Y;
  }

  if (kind == PAUSE_OPTION_KIND_LIE) {
    hasLieChanged = FALSE;
    switch (GetSurfaceInfo(g_CurrentShot.surface)->kind) {
      case SURFACE_KIND_TEE_UP:
        lie = PAUSE_LIE_TEE_UP;
        break;
      case SURFACE_KIND_FAIRWAY:
        lie = PAUSE_LIE_FAIRWAY;
        break;
      case SURFACE_KIND_FAIRWAY_FAIR_LIE:
        lie = PAUSE_LIE_FAIRWAY_FAIR_LIE;
        break;
      case SURFACE_KIND_FAIRWAY_POOR_LIE:
        lie = PAUSE_LIE_FAIRWAY_POOR_LIE;
        break;
      case SURFACE_KIND_SEMI_ROUGH:
        lie = PAUSE_LIE_SEMI_ROUGH;
        break;
      case SURFACE_KIND_ROUGH:
        lie = PAUSE_LIE_ROUGH;
        break;
      case SURFACE_KIND_ROUGH_POOR_LIE:
        lie = PAUSE_LIE_ROUGH_POOR_LIE;
        break;
      case SURFACE_KIND_DEEP_ROUGH:
        lie = PAUSE_LIE_DEEP_ROUGH;
        break;
      case SURFACE_KIND_BUNKER:
        lie = PAUSE_LIE_BUNKER;
        break;
      case SURFACE_KIND_BUNKER_FAIR_LIE:
        lie = PAUSE_LIE_BUNKER_FAIR_LIE;
        break;
      default:
        lie = PAUSE_LIE_BUNKER_POOR_LIE;
        break;
    }

    g_RoundHud.areLieArrowsShown = 1;
    if (controller->repeatedButtons & L_JPAD) {
      hasLieChanged = TRUE;
      lie = (lie + PAUSE_LIE_COUNT - 1) % PAUSE_LIE_COUNT;
      PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, MENU_SOUND_VOLUME);
    } else if (controller->repeatedButtons & R_JPAD) {
      hasLieChanged = TRUE;
      lie = (lie + 1) % PAUSE_LIE_COUNT;
      PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, MENU_SOUND_VOLUME);
    }

    if (hasLieChanged) {
      g_Players[0].ballPosition[AXIS_Z] = g_HoleSetup.teePosition[AXIS_Z];
      g_CurrentShot.ballPosition[AXIS_Z] = g_HoleSetup.teePosition[AXIS_Z];
      if (lie < PAUSE_LIE_SEMI_ROUGH) {
        g_Players[0].ballPosition[AXIS_X] = g_HoleSetup.teePosition[AXIS_X];
        g_CurrentShot.ballPosition[AXIS_X] = g_Players[0].ballPosition[AXIS_X];
      } else if (lie < PAUSE_LIE_DEEP_ROUGH) {
        g_Players[0].ballPosition[AXIS_X] =
            g_HoleSetup.teePosition[AXIS_X] + PAUSE_ROUGH_LIES_OFFSET_X;
        g_CurrentShot.ballPosition[AXIS_X] = g_Players[0].ballPosition[AXIS_X];
      } else if (lie == PAUSE_LIE_DEEP_ROUGH) {
        g_Players[0].ballPosition[AXIS_X] =
            g_HoleSetup.teePosition[AXIS_X] + PAUSE_DEEP_ROUGH_OFFSET_X;
        g_CurrentShot.ballPosition[AXIS_X] = g_Players[0].ballPosition[AXIS_X];
      } else {
        g_Players[0].ballPosition[AXIS_X] =
            g_HoleSetup.teePosition[AXIS_X] + PAUSE_BUNKER_LIES_OFFSET_X;
        g_CurrentShot.ballPosition[AXIS_X] = g_Players[0].ballPosition[AXIS_X];
      }

      g_CurrentShot.ballPosition[AXIS_Y] =
          GetGroundHeight(g_CurrentShot.ballPosition[AXIS_X],
              g_CurrentShot.ballPosition[AXIS_Z]);
      switch (lie) {
        case PAUSE_LIE_TEE_UP:
          g_CurrentShot.ballPosition[AXIS_Y] -= PAUSE_TEE_UP_HEIGHT;
          surface = SURFACE_TEE_UP;
          break;
        case PAUSE_LIE_FAIRWAY:
          surface = SURFACE_FAIRWAY;
          break;
        case PAUSE_LIE_FAIRWAY_FAIR_LIE:
          surface = SURFACE_FAIRWAY_FAIR_LIE;
          break;
        case PAUSE_LIE_FAIRWAY_POOR_LIE:
          surface = SURFACE_FAIRWAY_POOR_LIE;
          break;
        case PAUSE_LIE_SEMI_ROUGH:
          surface = SURFACE_SEMI_ROUGH;
          break;
        case PAUSE_LIE_ROUGH:
          surface = SURFACE_ROUGH;
          break;
        case PAUSE_LIE_ROUGH_POOR_LIE:
          surface = SURFACE_ROUGH_POOR_LIE;
          break;
        case PAUSE_LIE_DEEP_ROUGH:
          surface = SURFACE_DEEP_ROUGH;
          break;
        case PAUSE_LIE_BUNKER:
          surface = SURFACE_BUNKER;
          break;
        case PAUSE_LIE_BUNKER_FAIR_LIE:
          surface = SURFACE_BUNKER_FAIR_LIE;
          break;
        case PAUSE_LIE_BUNKER_POOR_LIE:
        default:
          surface = SURFACE_BUNKER_POOR_LIE;
          break;
      }

      g_Players[0].ballSurface = surface;
      g_CurrentShot.surface = surface;
      g_Players[0].ballPosition[AXIS_Y] = g_CurrentShot.ballPosition[AXIS_Y];
      PlaceBallAtRest(0, &g_TurnBallSimulation);
      StartShotPanel();
    }
  } else if (kind == PAUSE_OPTION_KIND_WIND_DIRECTION) {
    g_RoundHud.areWindDirectionArrowsShown = 1;
    g_HoleSetup.conditions.windDirection +=
        controller->stickX * PAUSE_WIND_TURN_PER_STICK;
    if (controller->stickX == 0) {
      if (controller->buttons & L_JPAD) {
        g_HoleSetup.conditions.windDirection -= PAUSE_WIND_TURN_STEP;
      }

      if (controller->buttons & R_JPAD) {
        g_HoleSetup.conditions.windDirection += PAUSE_WIND_TURN_STEP;
      }
    }
  } else if (kind == PAUSE_OPTION_KIND_WIND_SPEED) {
    g_RoundHud.areWindSpeedArrowsShown = 1;
    if ((controller->repeatedButtons & L_JPAD) &&
        g_HoleSetup.conditions.windSpeed > 0) {
      g_HoleSetup.conditions.windSpeed -= PAUSE_WIND_SPEED_STEP;
      PlaySound(SOUND_MENU_MOVE, g_NextSoundChannel, MENU_SOUND_VOLUME);
    }

    if ((controller->repeatedButtons & R_JPAD) &&
        g_HoleSetup.conditions.windSpeed <= PAUSE_WIND_SPEED_RAISE_LIMIT) {
      if (g_HoleSetup.conditions.windSpeed < PAUSE_WIND_SPEED_FIRST_STEP) {
        g_HoleSetup.conditions.windSpeed += PAUSE_WIND_SPEED_FIRST_STEP;
      } else {
        g_HoleSetup.conditions.windSpeed += PAUSE_WIND_SPEED_STEP;
      }

      PlaySound(SOUND_MENU_MOVE, g_NextSoundChannel, MENU_SOUND_VOLUME);
    }

    if (g_HoleSetup.conditions.windSpeed <= PAUSE_WIND_SPEED_CALM) {
      g_HoleSetup.conditions.windSpeed = 0;
    }

    if (g_HoleSetup.conditions.windSpeed > PAUSE_WIND_SPEED_MAX) {
      g_HoleSetup.conditions.windSpeed = PAUSE_WIND_SPEED_MAX;
    }
  } else if (kind == PAUSE_OPTION_KIND_GRID_COLOR) {
    if (controller->repeatedButtons & (L_JPAD | R_JPAD)) {
      g_Players[g_CurrentShot.playerIndex].gridColor ^= 1;
      if (g_CurrentShot.playerIndex == 0) {
        GetSaveData()->options[SAVE_OPTION_GRID_COLOR] =
            g_Players[g_CurrentShot.playerIndex].gridColor;
        WriteSaveData();
      }

      PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, MENU_SOUND_VOLUME);
    }
  } else if (kind == PAUSE_OPTION_KIND_WEATHER) {
    if (controller->repeatedButtons & (L_JPAD | R_JPAD)) {
      PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, MENU_SOUND_VOLUME);
      if (g_HoleSetup.conditions.isRaining == TRUE) {
        StopSound(RAIN_SOUND_CHANNEL);
        g_HoleSetup.conditions.isRaining = FALSE;
      } else {
        PlaySound(RAIN_SOUND, RAIN_SOUND_CHANNEL, RAIN_SOUND_START_VOLUME);
        g_HoleSetup.conditions.isRaining = TRUE;
        g_RainSoundVolume = RAIN_SOUND_START_VOLUME;
      }
    }
  }

  return PAUSE_ACTION_NONE;
}

/*
 * The arrows image is a strip of 16 by 16 pixel tiles, two frames of four tiles
 * each. Each frame's left arrow is its last tile and its right arrow the one
 * before.
 */
#define PAUSE_ARROW_SIZE 16
#define PAUSE_ARROWS_FRAME_HEIGHT (PAUSE_ARROW_SIZE * 4)
#define PAUSE_ARROWS_FRAME_SIZE \
  (PAUSE_ARROW_SIZE * PAUSE_ARROWS_FRAME_HEIGHT / 2)
#define PAUSE_ARROWS_TICKS_PER_FRAME 4
#define PAUSE_ARROWS_TICK_COUNT (PAUSE_ARROWS_TICKS_PER_FRAME * 2)
#define PAUSE_LEFT_ARROW_T ((PAUSE_ARROW_SIZE * 3) << 5)
#define PAUSE_RIGHT_ARROW_T ((PAUSE_ARROW_SIZE * 2) << 5)

/* Draws a pair of flashing arrows centred on (x, y), with their centres gap
 * pixels apart */
void DrawPauseArrows(Gfx** displayListHead, s32 x, s32 y, s32 gap) {
  Gfx* gfx = *displayListHead;
  s32 top;
  s32 bottom;

  f_PauseArrowsTick = (f_PauseArrowsTick + 1) % PAUSE_ARROWS_TICK_COUNT;
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_NONE);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  LoadSmallPalette(&gfx, 0, f_PauseArrowsImage + IMAGE_HEADER_SIZE);
  LoadNibbleTextureBlock(&gfx,
      f_PauseArrowsImage + PALETTED_TEXELS_OFFSET +
          (f_PauseArrowsTick / PAUSE_ARROWS_TICKS_PER_FRAME *
              PAUSE_ARROWS_FRAME_SIZE),
      G_IM_FMT_CI, PAUSE_ARROW_SIZE, PAUSE_ARROWS_FRAME_HEIGHT, 0, 0, 0, 0, 0,
      0, 0);
  top = (y - (PAUSE_ARROW_SIZE / 2)) << 2;
  bottom = (y + (PAUSE_ARROW_SIZE / 2)) << 2;
  gDPPipeSync(gfx++);
  DrawTextureRect(&gfx, (x - (gap / 2) - (PAUSE_ARROW_SIZE / 2)) << 2, top,
      (x - (gap / 2) + (PAUSE_ARROW_SIZE / 2)) << 2, bottom, G_TX_RENDERTILE, 0,
      PAUSE_LEFT_ARROW_T, 1 << 10, 1 << 10);
  DrawTextureRect(&gfx, (x + (gap / 2) - (PAUSE_ARROW_SIZE / 2)) << 2, top,
      (x + (gap / 2) + (PAUSE_ARROW_SIZE / 2)) << 2, bottom, G_TX_RENDERTILE, 0,
      PAUSE_RIGHT_ARROW_T, 1 << 10, 1 << 10);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_PERSP);
  *displayListHead = gfx;
}

#define PAUSE_CLOUD_LEFT 32
#define PAUSE_CLOUD_RIGHT 288
#define PAUSE_CLOUD_TOP 32
#define PAUSE_CLOUD_PART_WIDTH 256
#define PAUSE_CLOUD_PART_HEIGHT 32
#define PAUSE_CLOUD_PART_SIZE 0x1000
#define PAUSE_LABEL_X 80
#define PAUSE_LABELS_TOP 48
#define PAUSE_LABEL_SPACING 18
#define PAUSE_LABEL_FONT 1
#define PAUSE_LABEL_FLAGS 0x20
#define PAUSE_VALUE_X 212
#define PAUSE_RED_WAVE_MIDDLE 95
#define PAUSE_RED_WAVE_AMPLITUDE 95.0f
#define PAUSE_LABEL_RED 0x5F
#define PAUSE_LABEL_GREEN 0x3F
#define PAUSE_LABEL_BLUE 0xFF
#define PAUSE_CURSOR_SIZE 24
#define PAUSE_CURSOR_LOADED_HEIGHT 76
#define PAUSE_CURSOR_RIGHT 80
#define PAUSE_CURSOR_SWAY 8.0f
/* The cursor shows the tee frames instead of the glove for options with a value
 */
#define PAUSE_TEE_X 56
/* The tee leans the other way once the glove's sway passes this */
#define PAUSE_TEE_LEAN_SWAY 5
/* The tee frames follow the glove in the cursor image */
#define PAUSE_TEES_T (PAUSE_CURSOR_SIZE << 5)

#ifdef NON_MATCHING
/* Loads or frees the pause menu as RequestPauseMenu asked, then draws the menu
 * while it is loaded */
void DrawPauseMenu(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 cloudMiddle;
  s32 labelsHeight;
  s32 glowRed;
  s32 glowGreen;
  s32 glowBlue;
  s32 red;
  s32 green;
  s32 blue;
  s32 i;
  s16 y;
  s32 message;
  u16 kind;
  u16 selectedKind;
  s32 sway;
  s32 teeFrame;

  if (f_PauseMenuRequest == PAUSE_MENU_REQUEST_LOAD) {
    LoadPauseMenu();
    f_PauseMenuRequest = PAUSE_MENU_REQUEST_NONE;
  } else if (f_PauseMenuRequest == PAUSE_MENU_REQUEST_FREE) {
    FreePauseMenu();
    f_PauseMenuRequest = PAUSE_MENU_REQUEST_NONE;
  }

  if (!f_IsPauseMenuLoaded) {
    return;
  }

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_NONE);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_NONE);
  gDPSetPrimColor(gfx++, 0, 0, 220, 220, 230, 210);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
  gDPSetCombineLERP(gfx++, TEXEL0, 0, PRIMITIVE, 0, PRIMITIVE, 0, TEXEL0, 0,
      TEXEL0, 0, PRIMITIVE, 0, PRIMITIVE, 0, TEXEL0, 0);
  labelsHeight = f_PauseLayoutOptionCounts[f_PauseLayout] * PAUSE_LABEL_SPACING;
  LoadNibbleTextureBlock(&gfx, f_MenuCloudImage + IMAGE_HEADER_SIZE, G_IM_FMT_I,
      PAUSE_CLOUD_PART_WIDTH, PAUSE_CLOUD_PART_HEIGHT, 0, G_TX_CLAMP,
      G_TX_CLAMP, 0, 0, 0, 0);
  cloudMiddle = (labelsHeight + PAUSE_CLOUD_TOP) * 4;
  DrawTextureRect(&gfx, PAUSE_CLOUD_LEFT * 4, PAUSE_CLOUD_TOP * 4,
      PAUSE_CLOUD_RIGHT * 4, cloudMiddle, G_TX_RENDERTILE, 0, 0, 1 << 10,
      1 << 10);
  LoadNibbleTextureBlock(&gfx,
      f_MenuCloudImage + IMAGE_HEADER_SIZE + PAUSE_CLOUD_PART_SIZE, G_IM_FMT_I,
      PAUSE_CLOUD_PART_WIDTH, PAUSE_CLOUD_PART_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
  DrawTextureRect(&gfx, PAUSE_CLOUD_LEFT * 4, cloudMiddle,
      PAUSE_CLOUD_RIGHT * 4,
      (labelsHeight + PAUSE_CLOUD_TOP + PAUSE_CLOUD_PART_HEIGHT) * 4,
      G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  f_PauseGlowAngle += M_PI / 18;
  glowRed = (s32)(sinf(f_PauseGlowAngle) * PAUSE_RED_WAVE_AMPLITUDE) +
      PAUSE_RED_WAVE_MIDDLE;
  glowGreen = (s32)(sinf(f_PauseGlowAngle + M_PI) * COLOR_WAVE_AMPLITUDE) +
      COLOR_WAVE_MIDDLE;
  glowBlue = (s32)(sinf(f_PauseGlowAngle + (M_PI / 2)) * COLOR_WAVE_AMPLITUDE) +
      COLOR_WAVE_MIDDLE;
  SetUpTextDrawing(&gfx);
  for (i = 0, y = PAUSE_LABELS_TOP;
      i != f_PauseLayoutOptionCounts[f_PauseLayout];
      i++, y += PAUSE_LABEL_SPACING) {
    message = f_PauseOptions[f_PauseLayoutOptions[f_PauseLayout][i]].message;
    kind = f_PauseOptions[f_PauseLayoutOptions[f_PauseLayout][i]].kind;
    red = PAUSE_LABEL_RED;
    if (f_PauseSelection == i) {
      red = glowRed;
      green = glowGreen;
      blue = glowBlue;
    } else {
      green = PAUSE_LABEL_GREEN;
      blue = PAUSE_LABEL_BLUE;
    }

    if (f_PauseOptions[f_PauseLayoutOptions[f_PauseLayout][i]].kind ==
        PAUSE_OPTION_KIND_UNKD678) {
      if (!f_PauseMenuUnkD678) {
        message = PAUSE_OPTION_UNKD678_OFF_MESSAGE;
      }
    }

    SetTextSlot(PAUSE_LABEL_X, y, message + PAUSE_OPTION_MESSAGE_BASE,
        PAUSE_LABEL_FONT, i, PAUSE_LABEL_FLAGS, red, green, blue, 0xFF);
    gDPSetPrimColor(gfx++, 0, 0, red, green, blue, 0xFF);
    DrawLargeTextSlot(&gfx, i);
    if (kind == PAUSE_OPTION_KIND_GRID_COLOR) {
      DrawLargeText(&gfx, PAUSE_VALUE_X, y,
          (s8)g_Players[g_CurrentShot.playerIndex].gridColor == GRID_COLOR_BLACK
              ? f_BlackText
              : f_WhiteText);
    }

    if (kind == PAUSE_OPTION_KIND_WEATHER) {
      DrawLargeText(&gfx, PAUSE_VALUE_X, y,
          g_HoleSetup.conditions.isRaining == TRUE ? f_RainText : f_ClearText);
    }
  }

  f_PauseCursorAngle += M_PI / 12;
  if (f_PauseCursorAngle > M_PI) {
    f_PauseCursorAngle -= M_PI;
  }

  sway = sinf(f_PauseCursorAngle) * PAUSE_CURSOR_SWAY;
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  teeFrame = sway >= PAUSE_TEE_LEAN_SWAY;
  LoadSmallPalette(&gfx, 0, f_PauseCursorImage + IMAGE_HEADER_SIZE);
  gDPLoadTextureTile_4b(gfx++,
      TEXTURE_ADDRESS(f_PauseCursorImage + PALETTED_TEXELS_OFFSET), G_IM_FMT_CI,
      PAUSE_CURSOR_SIZE, 0, 0, 0, PAUSE_CURSOR_SIZE - 1,
      PAUSE_CURSOR_LOADED_HEIGHT - 1, 0, 0, 0, 0, 0, 0, 0);
  gDPPipeSync(gfx++);
  if (f_PauseLayoutOptions[f_PauseLayout][f_PauseSelection] ==
          PAUSE_OPTION_UNKD678 &&
      !f_PauseMenuUnkD678) {
    gDPSetCombineLERP(gfx++, TEXEL0, 0, PRIMITIVE, 0, TEXEL0, 0, PRIMITIVE, 0,
        TEXEL0, 0, PRIMITIVE, 0, TEXEL0, 0, PRIMITIVE, 0);
    gDPSetPrimColor(gfx++, 0, 0, 0x7F, 0x7F, 0x7F, 0xFF);
  }

  selectedKind =
      f_PauseOptions[f_PauseLayoutOptions[f_PauseLayout][f_PauseSelection]]
          .kind;
  if (selectedKind == PAUSE_OPTION_KIND_WEATHER ||
      selectedKind == PAUSE_OPTION_KIND_GRID_COLOR ||
      selectedKind == PAUSE_OPTION_KIND_WIND_SPEED ||
      selectedKind == PAUSE_OPTION_KIND_WIND_DIRECTION ||
      selectedKind == PAUSE_OPTION_KIND_LIE) {
    DrawTextureRect(&gfx, PAUSE_TEE_X * 4,
        ((f_PauseSelection * PAUSE_LABEL_SPACING) + PAUSE_LABELS_TOP - 8) * 4,
        PAUSE_CURSOR_RIGHT * 4,
        ((f_PauseSelection * PAUSE_LABEL_SPACING) + PAUSE_LABELS_TOP - 8 +
            PAUSE_CURSOR_SIZE) *
            4,
        G_TX_RENDERTILE, 0,
        PAUSE_TEES_T + (teeFrame * (PAUSE_CURSOR_SIZE << 5)), 1 << 10, 1 << 10);
  } else {
    DrawTextureRect(&gfx, (PAUSE_TEE_X - sway) * 4,
        ((f_PauseSelection * PAUSE_LABEL_SPACING) + PAUSE_LABELS_TOP - 4) * 4,
        (PAUSE_CURSOR_RIGHT - sway) * 4,
        ((f_PauseSelection * PAUSE_LABEL_SPACING) + PAUSE_LABELS_TOP - 4 +
            PAUSE_CURSOR_SIZE) *
            4,
        G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
  }

  f_PauseMenuDrawnMarker = 0xFFFFFF;
  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/pause_menu", DrawPauseMenu);
#endif

void LoadChoiceBoxImages(void) {
  EncodedAsset asset;

  f_ChoiceBoxCursorImage =
      AllocMemory(OpenEncodedAsset(ASSET_POINTING_HAND, &asset));
  ReadEncodedAsset(f_ChoiceBoxCursorImage, &asset);
  f_MenuCloudImage =
      AllocMemory(OpenEncodedAsset(CHOICE_BOX_CLOUD_IMAGE_ASSET, &asset));
  ReadEncodedAsset(f_MenuCloudImage, &asset);
}

void FreeChoiceBoxImages(void) {
  FreeMemory(&f_ChoiceBoxCursorImage);
  FreeMemory(&f_MenuCloudImage);
}

/* Opens the choice box of the given index, or closes the box with
 * CHOICE_BOX_CLOSED */
void OpenChoiceBox(s32 choiceBoxIndex) {
  f_ChoiceBoxIndex = choiceBoxIndex;
  if (choiceBoxIndex == CHOICE_BOX_CLOSED) {
    f_ChoiceBoxState = choiceBoxIndex;
    FreeChoiceBoxImages();
    PlaySound(CHOICE_BOX_CLOSE_SOUND, g_NextSoundChannel, MENU_SOUND_VOLUME);
  } else {
    f_ChoiceBoxSelection = 0;
    f_ChoiceBoxState = CHOICE_BOX_CHOOSING;
    LoadChoiceBoxImages();
    PlaySound(CHOICE_BOX_OPEN_SOUND, g_NextSoundChannel, MENU_SOUND_VOLUME);
  }
}

/* Moves the choice box's selection, and returns the chosen option's result once
 * the box closes */
s32 UpdateChoiceBox(ControllerState* controller) {
  ChoiceBox* box = &f_ChoiceBoxes[f_ChoiceBoxIndex];

  if (f_ChoiceBoxIndex == CHOICE_BOX_CLOSED) {
    return -1;
  }

  switch (f_ChoiceBoxState) {
    case CHOICE_BOX_CHOSEN:
      f_ChoiceBoxState = CHOICE_BOX_CLOSING;
      return -1;
    case CHOICE_BOX_CLOSING:
      OpenChoiceBox(CHOICE_BOX_CLOSED);
      return box->results[f_ChoiceBoxSelection];
    case CHOICE_BOX_CHOOSING:
      if (controller->pressedButtons == A_BUTTON) {
        PlaySound(
            CHOICE_BOX_CHOOSE_SOUND, g_NextSoundChannel, MENU_SOUND_VOLUME);
        f_ChoiceBoxState = CHOICE_BOX_CHOSEN;
        return -1;
      }

      if (!box->isVertical) {
        if (controller->repeatedButtons & L_JPAD) {
          f_ChoiceBoxSelection--;
          PlaySound(
              CHOICE_BOX_MOVE_SOUND, g_NextSoundChannel, MENU_SOUND_VOLUME);
        }

        if (controller->repeatedButtons & R_JPAD) {
          f_ChoiceBoxSelection++;
          PlaySound(
              CHOICE_BOX_MOVE_SOUND, g_NextSoundChannel, MENU_SOUND_VOLUME);
        }
      } else {
        if (controller->repeatedButtons & U_JPAD) {
          PlaySound(
              CHOICE_BOX_MOVE_SOUND, g_NextSoundChannel, MENU_SOUND_VOLUME);
          f_ChoiceBoxSelection--;
        }

        if (controller->repeatedButtons & D_JPAD) {
          PlaySound(
              CHOICE_BOX_MOVE_SOUND, g_NextSoundChannel, MENU_SOUND_VOLUME);
          f_ChoiceBoxSelection++;
        }
      }

      if (f_ChoiceBoxSelection > box->optionCount - 1) {
        f_ChoiceBoxSelection = 0;
      }

      if (f_ChoiceBoxSelection < 0) {
        f_ChoiceBoxSelection = box->optionCount - 1;
      }

      return -1;
  }

  return -1;
}

#ifdef NON_MATCHING
void DrawChoiceBox(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  ChoiceBox* box = &f_ChoiceBoxes[f_ChoiceBoxIndex];
  s32 halfWidth;
  s32 red;
  s32 green;
  s32 blue;
  s32 textWidth;
  s32 labelWidth;
  s32 i;
  s32 cursorX;
  s32 cursorY;
  s32 labelsTop;
  s16 labelY;

  f_ChoiceGlowAngle += M_PI / 18;
  halfWidth = box->halfWidth + CHOICE_BOX_MARGIN;
  red =
      (s32)(sinf(f_ChoiceGlowAngle) * COLOR_WAVE_AMPLITUDE) + COLOR_WAVE_MIDDLE;
  blue = (s32)(sinf(f_ChoiceGlowAngle + (M_PI / 2)) * COLOR_WAVE_AMPLITUDE) +
      COLOR_WAVE_MIDDLE;
  green = (s32)(sinf(f_ChoiceGlowAngle + M_PI) * COLOR_WAVE_AMPLITUDE) +
      COLOR_WAVE_MIDDLE;
  textWidth = 0;
  if (f_ChoiceBoxIndex != CHOICE_BOX_CLOSED) {
    gDPPipeSync(gfx++);
    gDPSetPrimColor(gfx++, 0, 0, 0x1F, 0x1F, 0x1F, 0xBF);
    gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, 0xBF);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineLERP(gfx++, 0, 0, 0, PRIMITIVE, 0, 0, 0, TEXEL0, 0, 0, 0,
        PRIMITIVE, 0, 0, 0, TEXEL0);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    for (i = 0; i != box->optionCount; i++) {
      labelWidth = MeasureText(f_ChoiceBoxLabels[box->labels[i]],
          FONT_SIZE_LARGE, MEASURE_TEXT_PROPORTIONAL);
      if (textWidth < labelWidth) {
        textWidth = labelWidth;
      }

      halfWidth = (textWidth / 2) + CHOICE_BOX_MARGIN;
    }

    for (i = 0; i != CHOICE_BOX_CLOUD_PART_COUNT; i++) {
      LoadNibbleTextureBlock(&gfx,
          f_MenuCloudImage + IMAGE_HEADER_SIZE +
              (i * CHOICE_BOX_CLOUD_PART_SIZE),
          G_IM_FMT_I, CHOICE_BOX_CLOUD_WIDTH, CHOICE_BOX_CLOUD_HEIGHT, 0, 0, 0,
          0, 0, 0, 0);
      DrawTextureRect(&gfx, (CHOICE_BOX_CENTER_X - halfWidth) << 2,
          CHOICE_BOX_TOP << 2, (CHOICE_BOX_CENTER_X + halfWidth) << 2,
          CHOICE_BOX_BOTTOM << 2, G_TX_RENDERTILE, 0, 0,
          (CHOICE_BOX_CLOUD_WIDTH << 10) / 2 / halfWidth,
          CHOICE_BOX_CLOUD_DTDY);
    }

    gDPPipeSync(gfx++);
    SetUpTextDrawing(&gfx);
    if (box->isVertical) {
      labelsTop = CHOICE_LABELS_MIDDLE_Y -
          ((box->optionCount - 1) * (CHOICE_LABEL_SPACING / 2));
      for (i = 0, labelY = labelsTop; i != box->optionCount;
          i++, labelY += CHOICE_LABEL_SPACING) {
        if (i == f_ChoiceBoxSelection) {
          gDPPipeSync(gfx++);
          gDPSetPrimColor(gfx++, 0, 0, red, green, blue, 0xFF);
          gDPPipeSync(gfx++);
        } else {
          gDPPipeSync(gfx++);
          gDPSetPrimColor(gfx++, 0, 0, 0x5F, 0x3F, 0xFF, 0xFF);
          gDPPipeSync(gfx++);
        }

        DrawCenteredLargeText(&gfx, CHOICE_BOX_CENTER_X, labelY,
            f_ChoiceBoxLabels[box->labels[i]]);
      }

      gDPPipeSync(gfx++);
      gDPSetTextureLUT(gfx++, G_TT_RGBA16);
      gDPPipeSync(gfx++);
      gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
      gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
      f_ChoiceCursorAngle += M_PI / 12;
      if (f_ChoiceCursorAngle > M_PI) {
        f_ChoiceCursorAngle -= M_PI;
      }

      labelWidth =
          MeasureText(f_ChoiceBoxLabels[box->labels[f_ChoiceBoxSelection]],
              FONT_SIZE_LARGE, MEASURE_TEXT_PROPORTIONAL);
      cursorX = (s32)(sinf(f_ChoiceCursorAngle) * CHOICE_CURSOR_SWAY) +
          CHOICE_CURSOR_X;
      cursorX -= textWidth / 2;
      LoadSmallPalette(&gfx, 0, f_ChoiceBoxCursorImage + IMAGE_HEADER_SIZE);
      cursorY = labelsTop + (f_ChoiceBoxSelection * CHOICE_LABEL_SPACING);
      gDPLoadTextureTile_4b(gfx++,
          TEXTURE_ADDRESS(f_ChoiceBoxCursorImage + PALETTED_TEXELS_OFFSET),
          G_IM_FMT_CI, CHOICE_CURSOR_SIZE, 0, 0, 0, CHOICE_CURSOR_SIZE - 1,
          CHOICE_CURSOR_LOADED_HEIGHT - 1, 0, 0, 0, 0, 0, 0, 0);
      DrawTextureRect(&gfx, cursorX << 2, (cursorY - 2) << 2,
          (cursorX + CHOICE_CURSOR_SIZE) << 2,
          (cursorY + CHOICE_CURSOR_SIZE - 2) << 2, G_TX_RENDERTILE, 0, 0,
          1 << 10, 1 << 10);
    }
  }

  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/pause_menu", DrawChoiceBox);
#endif
