/*
 * The pause screens kept in this overlay: the save menu, which lists either
 * the three saved games or the five replay records beside each one's
 * mode icon, and the Controls screen, a picture of the controller.
 */
#include "common.h"

#include "asset.h"
#include "billboard.h"
#include "character.h"
#include "controller.h"
#include "course.h"
#include "framebuffer.h"
#include "heap.h"
#include "hole_display.h"
#include "math_util.h"
#include "menu.h"
#include "play.h"
#include "round_hud.h"
#include "save.h"
#include "scene.h"
#include "screen.h"
#include "screen_layers.h"
#include "sound.h"
#include "text_format.h"
#include "variables.h"

/* What OpenSaveMenu lists, and f_MenuKind */
enum SaveMenuList {
  SAVE_MENU_CLOSED,
  SAVE_MENU_GAMES,
  SAVE_MENU_REPLAYS
};

/* f_MenuState values */
enum SaveMenuState {
  SAVE_MENU_STATE_OPENING, /* The rows slide in */
  SAVE_MENU_STATE_CHOOSING,
  SAVE_MENU_STATE_BACKING_OUT, /* The rows slide out */
  SAVE_MENU_STATE_BACKED_OUT,
  SAVE_MENU_STATE_SAVED, /* Waits before leaving */
  /* The rows slide out, and the menu closes */
  SAVE_MENU_STATE_LEAVING
};

/* The frames that each frame of the chosen icon shows for */
#define ICON_FRAME_PERIOD 5

/* The game modes that f_ModeIcons covers */
#define GAME_MODE_ICON_COUNT 12
#define MODE_NAME_COUNT 7

/* Asset ids of the Controls screen's picture */
#define CONTROLS_PICTURE 0x92A
#define MINI_GOLF_CONTROLS_PICTURE 0x96A
#define CONTROLS_PICTURE_WIDTH 288
#define CONTROLS_PICTURE_LEFT ((SCREEN_WIDTH - CONTROLS_PICTURE_WIDTH) / 2)
/* The picture is loaded into texture memory a few rows at a time */
#define CONTROLS_STRIP_HEIGHT 7
#define CONTROLS_STRIP_COUNT 32

#define MENU_SOUND_REVERB 0x14

/* The frames that a slide takes, and the frame of its sound */
#define SLIDE_FRAMES 14
#define SLIDE_SOUND_FRAME 3
/* Rows slide out slowly until past this, then speed up */
#define SLIDE_OUT_START_OFFSET -1
#define SLIDE_OUT_START_STEP 4

/* A character image for each player of each saved game, or for each replay
 * record */
#define CHARACTER_IMAGE_COUNT (SAVED_GAME_COUNT * MAX_PLAYER_COUNT)
/* Albatross, Hole in One, Eagle, Birdie, Par and Chip In */
#define REPLAY_LABEL_COUNT 6

/* How loud the song plays behind the saved games */
#define SAVE_MENU_SONG_VOLUME 0x60
#define SAVE_MENU_SOUND_PAN 0xFF

#define SAVE_MENU_TITLE_IMAGE 0x6A5
/* Where SetTextSlot is told to put the cleared messages */
#define MESSAGE_CENTER_X 160
#define MESSAGE_CENTER_Y 114
/* Given this as its text, SetTextSlot clears the slot */
#define NO_TEXT -1

/* What UpdateReplaySaveMenu returns */
enum ReplayMenuState {
  REPLAY_MENU_OPEN,
  REPLAY_MENU_CLOSED,
  REPLAY_MENU_PROMPTING
};

/* The rows below the saved games and the replay records */
#define END_GAME_ROW SAVED_GAME_COUNT
#define SKIP_REPLAY_ROW RING_SHOT_RECORD_COUNT
#define SLIDE_ROW_COUNT 4
/* The frames that the menu waits after saving before it slides out */
#define SAVED_WAIT_FRAMES 15

/* The game menu's rows: the saved games, then End Game */
#define GAME_ROW_COUNT (SAVED_GAME_COUNT + 1)
#define FIRST_GAME_ROW_TOP 50
#define GAME_ROW_SPACING 47
#define GAME_PANEL_STRIP_HEIGHT 15
/*
 * The panels' middles, less their rows' offsets; each panel reaches half
 * its height above and below its middle
 */
#define GAME_PANEL_MIDDLE 72
#define END_GAME_PANEL_MIDDLE 64
#define END_GAME_PANEL_STRIP_HEIGHT 9
/*
 * The title image is drawn in strips that each overlap the next by two
 * rows, as it sways in a figure eight around its resting place
 */
#define TITLE_IMAGE_WIDTH 160
#define TITLE_STRIP_COUNT 3
#define TITLE_STRIP_HEIGHT 10
#define TITLE_STRIP_SIZE (TITLE_IMAGE_WIDTH * TITLE_STRIP_HEIGHT)
#define TITLE_STRIP_DRAWN_HEIGHT 12
#define TITLE_LAST_STRIP_HEIGHT 7
#define TITLE_IMAGE_LEFT 70
#define TITLE_SWAY_LEFT 10.0f
#define TITLE_SWAY_TOP 18.0f
#define TITLE_SWAY_DISTANCE 5.0f
/* Texture coordinates have five fraction bits, so a texel is 32 */
#define TEXEL_COORDINATE_ONE 32
#define HALF_TEXEL_COORDINATE 16.0f
#define SWAY_DEGREES_PER_FRAME 10
/* The mode icons are drawn at half size, in two halves */
#define MODE_ICON_SIZE 64
#define MODE_ICON_PART_HEIGHT 32
#define MODE_ICON_PART_COUNT 2
#define MODE_ICON_LEFT 26
#define MODE_ICON_TOP 54
#define HALF_SIZE_TEXEL_STEP (2 << 10)
#define GOLFERS_LEFT 240
#define GOLFER_SPACING 26
#define GOLFER_TOP 52
#define MODE_TEXT_LEFT 62
#define MODE_TEXT_TOP 57
#define COURSE_TEXT_TOP 74
#define GAME_HOLE_TEXT_LEFT 240
#define GAME_HOLE_TEXT_TOP 78
#define EMPTY_GAME_TEXT_TOP 64
#define END_GAME_TEXT_LEFT 116
#define END_GAME_TEXT_TOP 199
/* The cursor's frame pulses between these alphas */
#define CURSOR_ALPHA_MIDDLE 127
#define CURSOR_ALPHA_SWING 64.0f
/* Each mini-golf course record has three courses */
#define MINI_GOLF_GREEN 0
#define MINI_GOLF_SLOW 1
#define MINI_GOLF_FAST 2
#define LUIGIS_GARDEN_COURSE_INDEX MINI_GOLF_COURSE_INDEX
#define PEACHS_CASTLE_COURSE_INDEX \
  (MINI_GOLF_COURSE_INDEX + MINI_GOLF_COURSES_PER_RECORD)

/* Indices of f_ModeNames */
enum ModeName {
  MODE_NAME_TOURNAMENT,
  MODE_NAME_GET_CHARACTER,
  MODE_NAME_STROKE,
  MODE_NAME_MATCH_GAME,
  MODE_NAME_SKINS_MATCH,
  MODE_NAME_CLUB_SLOTS,
  MODE_NAME_MINI_GOLF
};

/* The replay menu's rows: the records, then Hole Out */
#define REPLAY_ROW_COUNT (RING_SHOT_RECORD_COUNT + 1)
#define FIRST_REPLAY_ROW_TOP 36
#define REPLAY_ROW_SPACING 34
/* The panel image is stretched over each row a strip at a time */
#define PANEL_IMAGE_WIDTH 200
#define PANEL_IMAGE_HEIGHT 30
#define PANEL_STRIP_COUNT 3
#define PANEL_STRIP_HEIGHT (PANEL_IMAGE_HEIGHT / PANEL_STRIP_COUNT)
#define PANEL_STRIP_SIZE (PANEL_IMAGE_WIDTH * PANEL_STRIP_HEIGHT)
#define WIDE_PANEL_LEFT 24
#define WIDE_PANEL_WIDTH 272
#define RECORD_PANEL_STRIP_HEIGHT 11
#define NARROW_PANEL_LEFT 80
#define NARROW_PANEL_WIDTH 160
#define HOLE_OUT_PANEL_STRIP_HEIGHT 8
#define TITLE_PANEL_LEFT 96
#define TITLE_PANEL_TOP 13
#define TITLE_PANEL_WIDTH 128
#define TITLE_PANEL_STRIP_HEIGHT 9
#define TITLE_TEXT_LEFT 120
#define TITLE_TEXT_TOP 18
#define TITLE_BOB_HEIGHT 4.0f
#define TITLE_BOB_DEGREES_PER_FRAME 10
/* The rows' shades; the chosen one is brighter */
#define CHOSEN_PANEL_SHADE 0xEF
#define OTHER_PANEL_SHADE 0xBF
#define CURSOR_FRAME_THICKNESS 2
/* The course numbers image holds the digits 1 to 6, then the word COURSE */
#define COURSE_NUMBERS_WIDTH 136
#define COURSE_NUMBERS_HEIGHT 16
#define COURSE_DIGIT_WIDTH 16
#define COURSE_DIGIT_COUNT 6
#define COURSE_WORD_TEXEL_LEFT (COURSE_DIGIT_COUNT * COURSE_DIGIT_WIDTH)
#define COURSE_WORD_WIDTH 40
#define COURSE_WORD_LEFT 64
#define COURSE_TOP 36
#define LABEL_LEFT 128
#define LABEL_TOP 38
#define CHARACTER_IMAGE_WIDTH 32
#define CHARACTER_IMAGE_HEIGHT 24
#define CHARACTER_LEFT 32
#define CHARACTER_TOP 40
#define HOLE_TEXT_LEFT 64
#define HOLE_TEXT_TOP 53
#define EMPTY_TEXT_TOP 47
#define HOLE_OUT_TEXT_LEFT 128
#define HOLE_OUT_TEXT_TOP 212
/* The text slots of SetTextSlot that the records' hole texts use */
#define FIRST_RECORD_TEXT_SLOT 2
/* The same hole texts that the course select shows for Ring Shot */
#define RING_SHOT_TEXT_FIRST_IMAGE 0x4B

/* Indices of f_ReplayLabelImages */
#define REPLAY_LABEL_ALBATROSS 0
#define REPLAY_LABEL_HOLE_IN_ONE 1
#define REPLAY_LABEL_EAGLE 2
#define REPLAY_LABEL_BIRDIE 3
#define REPLAY_LABEL_PAR 4
#define NO_REPLAY_LABEL -1

/*
 * This overlay reads the characters and variants of saved players and
 * records as signed, though UnkB8 and RingShotRecord keep them unsigned; the
 * casts are there to match.
 */
void SaveRingShotRecord(s32 recordIndex, RingShotRecord* record);
void func_8005D0D8(void);
void SetTextSlot(
    s32 x, s32 y, s32 image, s32, s32 slot, s32, s32, s32, s32, s32);
void InvalidateBrokenSavedGames(void);
s32 SaveOrContinueGame(s32 slotIndex, Boolean isContinuing);
void func_80060358(void);
void CloseControlsScreen(void);
void SetUpTextDrawing(Gfx** displayListHead);
void DrawLargeTextSlot(Gfx** displayListHead, s32 slot);
void SetUpGradientTextDrawing(Gfx** displayListHead, s32);
void DrawCenteredLargeText(Gfx** displayListHead, s16 x, s16 y, char* text);
void DrawLargeTextWithShadow(Gfx** displayListHead, s32 x, s32 y, char* text,
    s32 envColor, s32 primColor, TextFlags flags);
/*
 * The call leaves out MeasureText's flags, so this file declares its text
 * routines itself rather than including text.h; it is there to match.
 */
s32 MeasureText(char* text, s32 fontSize);
void DrawLargeText(Gfx** displayListHead, s16 x, s16 y, char* text);

/* The MODE_ICON_* index by GameState.gameMode; modes that the menu never names
 * hold 0 or 3 */
static s8 f_ModeIcons[GAME_MODE_ICON_COUNT] = {
    MODE_ICON_STROKE,
    0,
    MODE_ICON_MATCH_GAME,
    0,
    MODE_ICON_GET_CHARACTER,
    MODE_ICON_TOURNAMENT,
    MODE_ICON_MINI_GOLF,
    3,
    0,
    0,
    MODE_ICON_SKINS_MATCH,
    MODE_ICON_CLUB_SLOTS,
};
static s32 f_MenuKind = SAVE_MENU_CLOSED;
static Boolean f_IsControlsShown = FALSE;
static s32 f_CursorIndex = 0;
static s32 f_StateFrame = 0;
static s32 f_MenuState = SAVE_MENU_STATE_OPENING;
/* Indexed in the order of DrawGameSaveMenu's cases */
static char* f_ModeNames[MODE_NAME_COUNT] = {
    "Tournament",
    "Get Character",
    "Stroke",
    "Match Game",
    "Skins Match",
    "Club Slots",
    "Mini-Golf",
};
static char f_NoSavedGameText[] = "There is no saved data.";
static char f_NoReplayText[] = "There is no replay data.";
static char f_EndGameText[] = "End Game";
static char f_SaveMenuText[] = "Save Menu";
static s32 f_IconImageIndex = 0;
static s32 f_UnusedSaveMenuWord = 0;
static SongId f_ResumedSongId = NO_SONG;
static s16 f_ReplayLabelIds[REPLAY_LABEL_COUNT] = {
    0x697, 0x698, 0x699, 0x69A, 0x69B, 0x69C};
static s32 f_IconFrameTimer = 0;
static Radians f_PanelWaveAngle = 0.0f;
static Radians f_TitleSwayAngle = 0.0f;
static Radians f_CursorPulseAngle = 0.0f;
static Radians f_ReplayTitleBobAngle = 0.0f;

static s32 f_SlideOffsets[4];
/* Read by no routine */
static u8 D_80245A30_ovl_12[8];
static void* f_CharacterFiles[CHARACTER_IMAGE_COUNT];
static void* f_CharacterImages[CHARACTER_IMAGE_COUNT];
static void* f_ModeIconImages[SAVED_GAME_COUNT][2];
static void* f_PanelImage;
static void* f_ReplayLabelImages[REPLAY_LABEL_COUNT];
static void* f_CourseNumbersImage;
static void* f_SaveMenuTitleImage;
static s32 f_IconFrames[SAVED_GAME_COUNT];

/* Returns the MODE_ICON_* index of a saved game's mode, or NO_MODE_ICON */
s32 GetModeIconIndex(GameState* state) {
  s32 iconIndex;

  if (state->isMiniGolf) {
    iconIndex = MODE_ICON_MINI_GOLF;
  } else {
    switch (state->gameMode) {
      case GAME_MODE_TOURNAMENT:
        iconIndex = MODE_ICON_TOURNAMENT;
        break;
      case GAME_MODE_GET_CHARACTER:
        iconIndex = MODE_ICON_GET_CHARACTER;
        break;
      case GAME_MODE_STROKE:
        iconIndex = MODE_ICON_STROKE;
        break;
      case GAME_MODE_MATCH_GAME:
        iconIndex = MODE_ICON_MATCH_GAME;
        break;
      case GAME_MODE_SKINS_MATCH:
        iconIndex = MODE_ICON_SKINS_MATCH;
        break;
      case GAME_MODE_CLUB_SLOTS:
        iconIndex = MODE_ICON_CLUB_SLOTS;
        break;
      default:
        iconIndex = NO_MODE_ICON;
        break;
    }
  }

  return iconIndex;
}

/*
 * Opens the save menu on the saved games (SAVE_MENU_GAMES) or the replay
 * records (SAVE_MENU_REPLAYS), loading its images and putting the cursor on
 * the first free slot.
 */
#ifdef NON_MATCHING
void OpenSaveMenu(s32 kind) {
  EncodedAsset encodedAsset;
  AssetFile file;
  SaveData* save;
  s32 slot;
  s32 playerIndex;
  s32 iconIndex;
  RingShotRecord* record;
  UnkB8* player;
  GameState* state;
  u16 frameId;
  void* picture;
  s32 fileSize;

  save = GetSaveData();
  if (kind == SAVE_MENU_GAMES) {
    InvalidateBrokenSavedGames();
  } else {
    func_8005D0D8();
  }

  if (kind == SAVE_MENU_GAMES) {
    SetSongVolume(SAVE_MENU_SONG_VOLUME);
  }

  ReadEncodedAsset(f_PanelImage = AllocMemory(
                       OpenEncodedAsset(ASSET_NAME_PLATE, &encodedAsset)),
      &encodedAsset);
  ReadEncodedAsset(f_SaveMenuTitleImage = AllocMemory(
                       OpenEncodedAsset(SAVE_MENU_TITLE_IMAGE, &encodedAsset)),
      &encodedAsset);
  PlaySoundWithEffects(SOUND_PANEL_OPEN, g_NextSoundChannel, MENU_VOLUME_MEDIUM,
      MENU_SOUND_REVERB, SAVE_MENU_SOUND_PAN);
  for (slot = 0; slot != CHARACTER_IMAGE_COUNT; slot++) {
    ReadEncodedAsset(f_CharacterImages[slot] = AllocMemory(OpenEncodedAsset(
                         g_CharacterImageAssets[CHARACTER_PLUM] +
                             CHARACTER_FACE_IMAGE_OFFSET,
                         &encodedAsset)),
        &encodedAsset);
    OpenAssetFile(g_CharacterPaletteAssets[CHARACTER_PLUM], &file);
    fileSize = GetAssetFileSize(&file);
    ReadAssetFile(
        f_CharacterFiles[slot] = AllocMemory(fileSize), fileSize, &file);
  }

  if (kind == SAVE_MENU_GAMES) {
    for (slot = 0; slot != SAVED_GAME_COUNT; slot++) {
      if (save->savedGames[slot].isValid &&
          (iconIndex = GetModeIconIndex(&save->savedGames[slot].state)) !=
              NO_MODE_ICON) {
        ReadEncodedAsset(
            f_ModeIconImages[slot][0] = AllocMemory(OpenEncodedAsset(
                g_ModeIconAnimations[iconIndex][0], &encodedAsset)),
            &encodedAsset);
        frameId = g_ModeIconAnimations[iconIndex][1];
      } else {
        ReadEncodedAsset(
            f_ModeIconImages[slot][0] = AllocMemory(
                OpenEncodedAsset(g_ModeIconAnimations[0][0], &encodedAsset)),
            &encodedAsset);
        frameId = g_ModeIconAnimations[0][1];
      }

      picture = AllocMemory(OpenEncodedAsset(frameId, &encodedAsset));
      f_ModeIconImages[slot][1] = picture;
      ReadEncodedAsset(picture, &encodedAsset);
    }
  } else {
    for (slot = 0; slot != REPLAY_LABEL_COUNT; slot++) {
      ReadEncodedAsset(f_ReplayLabelImages[slot] = AllocMemory(OpenEncodedAsset(
                           f_ReplayLabelIds[slot], &encodedAsset)),
          &encodedAsset);
    }

    ReadEncodedAsset(f_CourseNumbersImage = AllocMemory(
                         OpenEncodedAsset(ASSET_COURSE_NUMBERS, &encodedAsset)),
        &encodedAsset);
  }

  if (kind == SAVE_MENU_GAMES) {
    for (slot = 0; slot != SAVED_GAME_COUNT; slot++) {
      if (save->savedGames[slot].isValid) {
        state = &save->savedGames[slot].state;
        for (playerIndex = 0; playerIndex != state->playerCount;
            playerIndex++) {
          player = &save->savedPlayers[slot][playerIndex];
          OpenEncodedAsset(g_CharacterImageAssets[(s8)player->characterIndex] +
                  CHARACTER_FACE_IMAGE_OFFSET,
              &encodedAsset);
          ReadEncodedAsset(
              f_CharacterImages[(slot * MAX_PLAYER_COUNT) + playerIndex],
              &encodedAsset);
          OpenAssetFile(g_CharacterPaletteAssets[(s8)player->characterIndex] +
                  (s8)player->paletteIndex,
              &file);
          ReadAssetFile(
              f_CharacterFiles[(slot * MAX_PLAYER_COUNT) + playerIndex],
              GetAssetFileSize(&file), &file);
        }
      }
    }
  } else {
    for (slot = 0; slot != RING_SHOT_RECORD_COUNT; slot++) {
      record = &save->ringShotRecords[slot];
      if (record->scoreKind & SCORE_KIND_RECORDED) {
        OpenEncodedAsset(g_CharacterImageAssets[(s8)record->characterIndex] +
                CHARACTER_FACE_IMAGE_OFFSET,
            &encodedAsset);
        ReadEncodedAsset(f_CharacterImages[slot], &encodedAsset);
        OpenAssetFile(g_CharacterPaletteAssets[(s8)record->characterIndex] +
                (s8)record->paletteIndex,
            &file);
        ReadAssetFile(f_CharacterFiles[slot], GetAssetFileSize(&file), &file);
      }
    }
  }

  f_IconImageIndex = 0;
  f_IconFrames[0] = 1;
  f_IconFrames[1] = 1;
  f_IconFrames[2] = 1;
  if (kind == SAVE_MENU_GAMES) {
    f_CursorIndex = END_GAME_ROW;
    for (slot = 0; slot != SAVED_GAME_COUNT; slot++) {
      if (!save->savedGames[slot].isValid) {
        f_CursorIndex = slot;
        break;
      }
    }
  } else {
    f_CursorIndex = SKIP_REPLAY_ROW;
    for (slot = 0; slot != RING_SHOT_RECORD_COUNT; slot++) {
      if (!(save->ringShotRecords[slot].scoreKind & SCORE_KIND_RECORDED)) {
        f_CursorIndex = slot;
        break;
      }
    }
  }

  f_StateFrame = 0;
  f_MenuState = SAVE_MENU_STATE_OPENING;
  f_SlideOffsets[0] = SCREEN_WIDTH;
  f_SlideOffsets[1] = SCREEN_WIDTH;
  f_SlideOffsets[2] = SCREEN_WIDTH;
  f_SlideOffsets[3] = SCREEN_WIDTH;
  f_MenuKind = kind;
}

#else
INCLUDE_ASM("asm/nonmatchings/ovl_12/save_menu", OpenSaveMenu);
#endif

void CloseSaveMenu(void) {
  s32 index;

  SetSongVolume(SOUND_FULL_SCALE);
  FreeMemory(&f_PanelImage);
  FreeMemory(&f_SaveMenuTitleImage);
  if (f_MenuKind == SAVE_MENU_GAMES) {
    for (index = 0; index != SAVED_GAME_COUNT; index++) {
      FreeMemory(&f_ModeIconImages[index][0]);
      FreeMemory(&f_ModeIconImages[index][1]);
    }
  } else {
    for (index = 0; index != REPLAY_LABEL_COUNT; index++) {
      FreeMemory(&f_ReplayLabelImages[index]);
    }

    FreeMemory(&f_CourseNumbersImage);
  }

  for (index = 0; index != CHARACTER_IMAGE_COUNT; index++) {
    FreeMemory(&f_CharacterImages[index]);
    FreeMemory(&f_CharacterFiles[index]);
  }

  f_MenuKind = SAVE_MENU_CLOSED;
  SetTextSlot(MESSAGE_CENTER_X, MESSAGE_CENTER_Y, NO_TEXT, TEXT_SLOT_LARGE_FONT,
      0, 0, 0, 0, 0, 0);
  SetTextSlot(MESSAGE_CENTER_X, MESSAGE_CENTER_Y, NO_TEXT, TEXT_SLOT_LARGE_FONT,
      1, 0, 0, 0, 0, 0);
  SetTextSlot(MESSAGE_CENTER_X, MESSAGE_CENTER_Y, NO_TEXT, TEXT_SLOT_LARGE_FONT,
      2, 0, 0, 0, 0, 0);
  SetTextSlot(MESSAGE_CENTER_X, MESSAGE_CENTER_Y, NO_TEXT, TEXT_SLOT_LARGE_FONT,
      3, 0, 0, 0, 0, 0);
  SetTextSlot(MESSAGE_CENTER_X, MESSAGE_CENTER_Y, NO_TEXT, TEXT_SLOT_LARGE_FONT,
      4, 0, 0, 0, 0, 0);
}

/*
 * Every few frames, or at once when told to, shows the next frame of the
 * chosen saved game's mode icon and the first frame of the others. Each
 * icon has two images, read into in turn.
 */
#ifdef NON_MATCHING
void AnimateModeIcons(Boolean isImmediate) {
  EncodedAsset encodedAsset;
  SaveData* save;
  SavedGame* game;
  s32 slot;
  void** images;

  save = GetSaveData();
  if ((++f_IconFrameTimer >= ICON_FRAME_PERIOD) | (isImmediate == TRUE)) {
    f_IconFrameTimer = 0;
    for (slot = 0, images = f_ModeIconImages[0]; slot != SAVED_GAME_COUNT;
        slot++, images += 2) {
      game = &save->savedGames[slot];
      if (!save->savedGames[slot].isValid) {
        continue;
      }

      if (game->state.isMiniGolf) {
        if (f_CursorIndex == slot) {
          if (g_ModeIconAnimations[MODE_ICON_MINI_GOLF][++f_IconFrames[slot]] ==
              END_OF_ICON_FRAMES) {
            f_IconFrames[slot] = 0;
          }

          OpenEncodedAsset(
              g_ModeIconAnimations[MODE_ICON_MINI_GOLF][f_IconFrames[slot]],
              &encodedAsset);
        } else {
          OpenEncodedAsset(
              g_ModeIconAnimations[MODE_ICON_MINI_GOLF][0], &encodedAsset);
        }
      } else if (f_CursorIndex == slot) {
        if (g_ModeIconAnimations[f_ModeIcons[game->state.gameMode]]
                                [++f_IconFrames[slot]] == END_OF_ICON_FRAMES) {
          f_IconFrames[slot] = 0;
        }

        OpenEncodedAsset(g_ModeIconAnimations[f_ModeIcons[game->state.gameMode]]
                                             [f_IconFrames[slot]],
            &encodedAsset);
      } else {
        OpenEncodedAsset(
            g_ModeIconAnimations[f_ModeIcons[game->state.gameMode]][0],
            &encodedAsset);
      }

      ReadEncodedAsset(images[f_IconImageIndex], &encodedAsset);
    }

    f_IconImageIndex ^= 1;
  }
}

#else
INCLUDE_ASM("asm/nonmatchings/ovl_12/save_menu", AnimateModeIcons);
#endif

/*
 * Draws the saved games: each row's panel, mode icon, golfers, mode,
 * course and hole, the End Game row, the swaying title and the cursor's
 * pulsing frame, all slid by f_SlideOffsets.
 */
void DrawGameSaveMenu(Gfx** displayListHead) {
  Gfx* gfx;
  SaveData* save;
  s32 row;

  /* Counts rows, icon halves or golfers; sharing one variable is there to match
   */
  s32 index;
  s32 strip;
  s32 stripHeight;
  f32 titleLeftExact;
  f32 titleTopExact;
  s32 titleLeft;
  s32 titleTop;
  s32 titleTexelLeft;
  s32 titleTexelTop;
  s32 drawnHeight;
  s32 imageLeft;

  save = GetSaveData();
  gfx = *displayListHead;
  if (f_MenuKind == SAVE_MENU_GAMES) {
    AnimateModeIcons(FALSE);

    /* The panels, stretched from the panel image a strip at a time */
    gDPPipeSync(gfx++);
    gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);

    /* The wave's height is never used */
    sinf(f_PanelWaveAngle);
    f_PanelWaveAngle += DEGREES_TO_RADIANS(SWAY_DEGREES_PER_FRAME);
    LoadLargePalette(&gfx, (u8*)f_PanelImage + IMAGE_HEADER_SIZE);
    for (strip = 0; strip != PANEL_STRIP_COUNT; strip++) {
      LoadTextureBlock(&gfx,
          (u8*)f_PanelImage + LARGE_PALETTED_TEXELS_OFFSET +
              (strip * PANEL_STRIP_SIZE),
          G_IM_FMT_CI, G_IM_SIZ_8b, PANEL_IMAGE_WIDTH, PANEL_STRIP_HEIGHT, 0,
          G_TX_MIRROR, G_TX_MIRROR, 0, 0, 0, 0);
      for (index = 0; index != SAVED_GAME_COUNT; index++) {
        if (index == f_CursorIndex) {
          gDPSetPrimColor(gfx++, 0, 0, CHOSEN_PANEL_SHADE, CHOSEN_PANEL_SHADE,
              CHOSEN_PANEL_SHADE, CHOSEN_PANEL_SHADE);
        } else {
          gDPSetPrimColor(gfx++, 0, 0, OTHER_PANEL_SHADE, OTHER_PANEL_SHADE,
              OTHER_PANEL_SHADE, 0xFF);
        }

        stripHeight = GAME_PANEL_STRIP_HEIGHT;
        DrawTextureRect(&gfx, (f_SlideOffsets[index] + WIDE_PANEL_LEFT) << 2,
            (GAME_PANEL_MIDDLE + (index * GAME_ROW_SPACING) +
                (strip * stripHeight) - (stripHeight * PANEL_STRIP_COUNT / 2))
                << 2,
            (f_SlideOffsets[index] + WIDE_PANEL_LEFT + WIDE_PANEL_WIDTH) << 2,
            (GAME_PANEL_MIDDLE + (index * GAME_ROW_SPACING) +
                (strip * stripHeight) - (stripHeight * PANEL_STRIP_COUNT / 2) +
                stripHeight)
                << 2,
            G_TX_RENDERTILE, 0, 0, (PANEL_IMAGE_WIDTH << 10) / WIDE_PANEL_WIDTH,
            (PANEL_IMAGE_HEIGHT << 10) / (stripHeight * PANEL_STRIP_COUNT));
      }

      if (f_CursorIndex == index) {
        gDPSetPrimColor(gfx++, 0, 0, CHOSEN_PANEL_SHADE, CHOSEN_PANEL_SHADE,
            CHOSEN_PANEL_SHADE, CHOSEN_PANEL_SHADE);
      } else {
        gDPSetPrimColor(gfx++, 0, 0, OTHER_PANEL_SHADE, OTHER_PANEL_SHADE,
            OTHER_PANEL_SHADE, 0xFF);
      }

      stripHeight = END_GAME_PANEL_STRIP_HEIGHT;
      DrawTextureRect(&gfx, (f_SlideOffsets[index] + NARROW_PANEL_LEFT) << 2,
          (END_GAME_PANEL_MIDDLE + (index * GAME_ROW_SPACING) +
              (strip * stripHeight) - (stripHeight * PANEL_STRIP_COUNT / 2))
              << 2,
          (f_SlideOffsets[index] + NARROW_PANEL_LEFT + NARROW_PANEL_WIDTH) << 2,
          (END_GAME_PANEL_MIDDLE + (index * GAME_ROW_SPACING) +
              (strip * stripHeight) - (stripHeight * PANEL_STRIP_COUNT / 2) +
              stripHeight)
              << 2,
          G_TX_RENDERTILE, 0, 0, (PANEL_IMAGE_WIDTH << 10) / NARROW_PANEL_WIDTH,
          (PANEL_IMAGE_HEIGHT << 10) / (stripHeight * PANEL_STRIP_COUNT));
      gDPSetPrimColor(gfx++, 0, 0, CHOSEN_PANEL_SHADE, CHOSEN_PANEL_SHADE,
          CHOSEN_PANEL_SHADE, CHOSEN_PANEL_SHADE);
    }

    /* The title, swaying to a fraction of a pixel */
    titleLeftExact =
        (cosf(f_TitleSwayAngle * 0.5f) * TITLE_SWAY_DISTANCE) + TITLE_SWAY_LEFT;
    titleTopExact =
        (sinf(f_TitleSwayAngle) * TITLE_SWAY_DISTANCE) + TITLE_SWAY_TOP;
    titleLeft =
        titleLeftExact < 0.0f ? titleLeftExact + 0.5f : titleLeftExact - 0.5f;
    titleTop =
        titleTopExact < 0.0f ? titleTopExact + 0.5f : titleTopExact - 0.5f;
    titleTexelLeft = (s32)(HALF_TEXEL_COORDINATE -
        ((titleLeftExact - titleLeft) * HALF_TEXEL_COORDINATE));
    titleTexelTop = (s32)(HALF_TEXEL_COORDINATE -
        ((titleTopExact - titleTop) * HALF_TEXEL_COORDINATE));
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_AVERAGE);
    f_TitleSwayAngle += DEGREES_TO_RADIANS(SWAY_DEGREES_PER_FRAME);
    LoadLargePalette(&gfx, (u8*)f_SaveMenuTitleImage + IMAGE_HEADER_SIZE);
    imageLeft = TITLE_IMAGE_LEFT;

    /* The strips are counted with row; reusing it is there to match */
    for (row = 0; row != TITLE_STRIP_COUNT; row++) {
      drawnHeight = TITLE_STRIP_DRAWN_HEIGHT;
      if (row == TITLE_STRIP_COUNT - 1) {
        drawnHeight = TITLE_LAST_STRIP_HEIGHT;
      }

      gDPLoadTextureTile(gfx++,
          ((u32)f_SaveMenuTitleImage + LARGE_PALETTED_TEXELS_OFFSET +
              (row * TITLE_STRIP_SIZE)) &
              ~7,
          G_IM_FMT_CI, G_IM_SIZ_8b, TITLE_IMAGE_WIDTH, TITLE_STRIP_DRAWN_HEIGHT,
          0, 0, TITLE_IMAGE_WIDTH - 1, TITLE_STRIP_DRAWN_HEIGHT - 1, 0, 0, 0, 0,
          0, 0, 0);
      gSPTextureRectangle(gfx++,
          (titleLeft + imageLeft - f_SlideOffsets[1]) << 2,
          (titleTop + (row * TITLE_STRIP_HEIGHT)) << 2,
          (titleLeft + (imageLeft + TITLE_IMAGE_WIDTH) - f_SlideOffsets[1])
              << 2,
          ((row * TITLE_STRIP_HEIGHT) + drawnHeight + titleTop) << 2,
          G_TX_RENDERTILE, titleTexelLeft + TEXEL_COORDINATE_ONE,
          titleTexelTop + TEXEL_COORDINATE_ONE, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    }

    gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF);

    /* Each saved game's mode icon, from the image not being loaded into */
    for (row = 0; row != SAVED_GAME_COUNT; row++) {
      LoadLargePalette(&gfx,
          (u8*)f_ModeIconImages[row][f_IconImageIndex ^ 1] + IMAGE_HEADER_SIZE);
      if (save->savedGames[row].isValid) {
        gDPPipeSync(gfx++);
        if (row == f_CursorIndex) {
          gDPSetPrimColor(gfx++, 0, 0, CHOSEN_PANEL_SHADE, CHOSEN_PANEL_SHADE,
              CHOSEN_PANEL_SHADE, CHOSEN_PANEL_SHADE);
        } else {
          gDPSetPrimColor(gfx++, 0, 0, OTHER_PANEL_SHADE, OTHER_PANEL_SHADE,
              OTHER_PANEL_SHADE, 0xFF);
        }

        gDPPipeSync(gfx++);
        for (index = 0; index != MODE_ICON_PART_COUNT; index++) {
          LoadTextureBlock(&gfx,
              (u8*)f_ModeIconImages[row][f_IconImageIndex ^ 1] +
                  LARGE_PALETTED_TEXELS_OFFSET +
                  (index * MODE_ICON_PART_HEIGHT * MODE_ICON_SIZE),
              G_IM_FMT_CI, G_IM_SIZ_8b, MODE_ICON_SIZE, MODE_ICON_PART_HEIGHT,
              0, G_TX_MIRROR, G_TX_MIRROR, 0, 0, 0, 0);
          DrawTextureRect(&gfx, (f_SlideOffsets[row] + MODE_ICON_LEFT) << 2,
              (MODE_ICON_TOP + (row * GAME_ROW_SPACING) +
                  (index * (MODE_ICON_PART_HEIGHT / 2)))
                  << 2,
              (f_SlideOffsets[row] + MODE_ICON_LEFT + (MODE_ICON_SIZE / 2))
                  << 2,
              (MODE_ICON_TOP + (row * GAME_ROW_SPACING) +
                  (index * (MODE_ICON_PART_HEIGHT / 2)) +
                  (MODE_ICON_PART_HEIGHT / 2))
                  << 2,
              G_TX_RENDERTILE, 0, 0, HALF_SIZE_TEXEL_STEP,
              HALF_SIZE_TEXEL_STEP);
        }
      }
    }

    /* Each saved game's golfers, side by side about GOLFERS_LEFT */
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    for (row = 0; row != SAVED_GAME_COUNT; row++) {
      if (save->savedGames[row].isValid) {
        GameState* state;
        s32 golfersLeft;
        s32 golfersRight;

        gDPPipeSync(gfx++);
        golfersLeft = GOLFERS_LEFT;
        golfersRight = GOLFERS_LEFT + CHARACTER_IMAGE_WIDTH;
        state = &save->savedGames[row].state;
        if (row == f_CursorIndex) {
          gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF);
        } else {
          gDPSetPrimColor(gfx++, 0, 0, OTHER_PANEL_SHADE, OTHER_PANEL_SHADE,
              OTHER_PANEL_SHADE, 0xFF);
        }

        gDPPipeSync(gfx++);
        for (index = 0; index != state->playerCount; index++) {
          LoadLargePalette(
              &gfx, f_CharacterFiles[(row * MAX_PLAYER_COUNT) + index]);
          LoadTextureBlock(&gfx,
              f_CharacterImages[(row * MAX_PLAYER_COUNT) + index], G_IM_FMT_CI,
              G_IM_SIZ_8b, CHARACTER_IMAGE_WIDTH, CHARACTER_IMAGE_HEIGHT, 0, 0,
              0, 0, 0, 0, 0);
          DrawTextureRect(&gfx,
              ((index * GOLFER_SPACING) + (f_SlideOffsets[row] + golfersLeft) -
                  (state->playerCount * (GOLFER_SPACING / 2)))
                  << 2,
              (GOLFER_TOP + (row * GAME_ROW_SPACING)) << 2,
              ((index * GOLFER_SPACING) + (f_SlideOffsets[row] + golfersRight) -
                  (state->playerCount * (GOLFER_SPACING / 2)))
                  << 2,
              (GOLFER_TOP + (row * GAME_ROW_SPACING) + CHARACTER_IMAGE_HEIGHT)
                  << 2,
              G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
        }
      }
    }

    /* The cursor's frame around the chosen row, pulsing */
    {
      s32 rowHeights[GAME_ROW_COUNT] = {42, 42, 42, 25};
      s32 rowHalfWidths[GAME_ROW_COUNT] = {136, 136, 136, 80};
      s32 centerX;
      s32 rowTop;

      f_CursorPulseAngle += DEGREES_TO_RADIANS(SWAY_DEGREES_PER_FRAME);
      centerX = SCREEN_WIDTH / 2;
      rowTop = FIRST_GAME_ROW_TOP;
      gDPSetPrimColor(gfx++, 0, 0, 0x00, 0xBF, 0xFF,
          (s32)(sinf(f_CursorPulseAngle) * CURSOR_ALPHA_SWING) +
              CURSOR_ALPHA_MIDDLE);
      gDPPipeSync(gfx++);
      gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
      gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
      DrawTextureRect(&gfx,
          (f_SlideOffsets[f_CursorIndex] + centerX -
              rowHalfWidths[f_CursorIndex])
              << 2,
          (rowTop + (f_CursorIndex * GAME_ROW_SPACING)) << 2,
          (rowHalfWidths[f_CursorIndex] +
              (f_SlideOffsets[f_CursorIndex] + centerX))
              << 2,
          (rowTop + (f_CursorIndex * GAME_ROW_SPACING) + CURSOR_FRAME_THICKNESS)
              << 2,
          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
      DrawTextureRect(&gfx,
          (f_SlideOffsets[f_CursorIndex] + centerX -
              rowHalfWidths[f_CursorIndex])
              << 2,
          (rowHeights[f_CursorIndex] +
              (rowTop + (f_CursorIndex * GAME_ROW_SPACING)))
              << 2,
          (rowHalfWidths[f_CursorIndex] +
              (f_SlideOffsets[f_CursorIndex] + centerX))
              << 2,
          (rowHeights[f_CursorIndex] +
              (rowTop + (f_CursorIndex * GAME_ROW_SPACING)) +
              CURSOR_FRAME_THICKNESS)
              << 2,
          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
      DrawTextureRect(&gfx,
          (f_SlideOffsets[f_CursorIndex] + centerX -
              rowHalfWidths[f_CursorIndex])
              << 2,
          (rowTop + (f_CursorIndex * GAME_ROW_SPACING)) << 2,
          (f_SlideOffsets[f_CursorIndex] + centerX -
              rowHalfWidths[f_CursorIndex] + CURSOR_FRAME_THICKNESS)
              << 2,
          (rowHeights[f_CursorIndex] +
              (rowTop + (f_CursorIndex * GAME_ROW_SPACING)) +
              CURSOR_FRAME_THICKNESS)
              << 2,
          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
      DrawTextureRect(&gfx,
          (rowHalfWidths[f_CursorIndex] +
              (f_SlideOffsets[f_CursorIndex] + centerX) -
              CURSOR_FRAME_THICKNESS)
              << 2,
          (rowTop + (f_CursorIndex * GAME_ROW_SPACING)) << 2,
          (rowHalfWidths[f_CursorIndex] +
              (f_SlideOffsets[f_CursorIndex] + centerX))
              << 2,
          (rowHeights[f_CursorIndex] +
              (rowTop + (f_CursorIndex * GAME_ROW_SPACING)) +
              CURSOR_FRAME_THICKNESS)
              << 2,
          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    }

    /* Each saved game's mode, course and hole, or that the slot is empty */
    SetUpGradientTextDrawing(&gfx, 0);
    for (row = 0; row != SAVED_GAME_COUNT; row++) {
      gDPPipeSync(gfx++);
      if (row == f_CursorIndex) {
        gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF);
      } else {
        gDPSetPrimColor(gfx++, 0, 0, OTHER_PANEL_SHADE, OTHER_PANEL_SHADE,
            OTHER_PANEL_SHADE, 0xFF);
      }

      gDPPipeSync(gfx++);
      if (save->savedGames[row].isValid) {
        GameState* state = &save->savedGames[row].state;

        if (state->isMiniGolf) {
          sprintf(g_PrintBuffer, "%s", f_ModeNames[MODE_NAME_MINI_GOLF]);
        } else {
          switch (state->gameMode) {
            case GAME_MODE_TOURNAMENT:
              sprintf(g_PrintBuffer, "%s", f_ModeNames[MODE_NAME_TOURNAMENT]);
              break;
            case GAME_MODE_GET_CHARACTER:
              sprintf(
                  g_PrintBuffer, "%s", f_ModeNames[MODE_NAME_GET_CHARACTER]);
              break;
            case GAME_MODE_STROKE:
              sprintf(g_PrintBuffer, "%s", f_ModeNames[MODE_NAME_STROKE]);
              break;
            case GAME_MODE_MATCH_GAME:
              sprintf(g_PrintBuffer, "%s", f_ModeNames[MODE_NAME_MATCH_GAME]);
              break;
            case GAME_MODE_SKINS_MATCH:
              sprintf(g_PrintBuffer, "%s", f_ModeNames[MODE_NAME_SKINS_MATCH]);
              break;
            case GAME_MODE_CLUB_SLOTS:
              sprintf(g_PrintBuffer, "%s", f_ModeNames[MODE_NAME_CLUB_SLOTS]);
              break;
          }
        }

        DrawLargeTextWithShadow(&gfx, f_SlideOffsets[row] + MODE_TEXT_LEFT,
            MODE_TEXT_TOP + (row * GAME_ROW_SPACING), g_PrintBuffer, 0xFFFF9300,
            0x44FF0000, 0);
        if (state->isMiniGolf) {
          if (state->courseIndex ==
              LUIGIS_GARDEN_COURSE_INDEX + MINI_GOLF_GREEN) {
            sprintf(g_PrintBuffer, "%s GREEN",
                g_CourseRecords[COURSE_RECORD_LUIGIS_GARDEN].name);
          }

          if (state->courseIndex ==
              LUIGIS_GARDEN_COURSE_INDEX + MINI_GOLF_SLOW) {
            sprintf(g_PrintBuffer, "%s SLOW",
                g_CourseRecords[COURSE_RECORD_LUIGIS_GARDEN].name);
          }

          if (state->courseIndex ==
              LUIGIS_GARDEN_COURSE_INDEX + MINI_GOLF_FAST) {
            sprintf(g_PrintBuffer, "%s FAST",
                g_CourseRecords[COURSE_RECORD_LUIGIS_GARDEN].name);
          }

          if (state->courseIndex ==
              PEACHS_CASTLE_COURSE_INDEX + MINI_GOLF_GREEN) {
            sprintf(g_PrintBuffer, "%s GREEN",
                g_CourseRecords[COURSE_RECORD_PEACHS_CASTLE].name);
          }

          if (state->courseIndex ==
              PEACHS_CASTLE_COURSE_INDEX + MINI_GOLF_SLOW) {
            sprintf(g_PrintBuffer, "%s SLOW",
                g_CourseRecords[COURSE_RECORD_PEACHS_CASTLE].name);
          }

          if (state->courseIndex ==
              PEACHS_CASTLE_COURSE_INDEX + MINI_GOLF_FAST) {
            sprintf(g_PrintBuffer, "%s FAST",
                g_CourseRecords[COURSE_RECORD_PEACHS_CASTLE].name);
          }
        } else if (state->gameMode == GAME_MODE_TOURNAMENT) {
          sprintf(g_PrintBuffer, "%s", g_TournamentNames[state->courseIndex]);
        } else {
          sprintf(
              g_PrintBuffer, "%s", g_CourseRecords[state->courseIndex].name);
        }

        MeasureText(g_PrintBuffer, FONT_SIZE_LARGE);
        DrawLargeTextWithShadow(&gfx, f_SlideOffsets[row] + MODE_TEXT_LEFT,
            COURSE_TEXT_TOP + (row * GAME_ROW_SPACING), g_PrintBuffer,
            0x00FFFF00, 0xFFFFFF00, 0);
        gDPSetEnvColor(gfx++, 0xFF, 0xFF, 0xFF, 0xFF);
        gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF);
        sprintf(g_PrintBuffer, "Hole%d", state->holeIndex + 1);
        DrawLargeText(&gfx, f_SlideOffsets[row] + GAME_HOLE_TEXT_LEFT,
            GAME_HOLE_TEXT_TOP + (row * GAME_ROW_SPACING), g_PrintBuffer);
      } else {
        gDPSetEnvColor(gfx++, 0xFF, 0xFF, 0x2C, 0xFF);
        gDPSetPrimColor(gfx++, 0, 0, 0x40, 0xFF, 0x00, 0xFF);
        sprintf(g_PrintBuffer, "%s", f_NoSavedGameText);
        DrawCenteredLargeText(&gfx, f_SlideOffsets[row] + (SCREEN_WIDTH / 2),
            EMPTY_GAME_TEXT_TOP + (row * GAME_ROW_SPACING), g_PrintBuffer);
      }
    }

    gDPPipeSync(gfx++);
    DrawLargeTextWithShadow(&gfx,
        f_SlideOffsets[END_GAME_ROW] + END_GAME_TEXT_LEFT, END_GAME_TEXT_TOP,
        f_EndGameText, 0xFF571300, 0xFFDB4C00, 0);
    gDPPipeSync(gfx++);
    gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0x00, 0xFF);
    gDPSetEnvColor(gfx++, 0xFF, 0x68, 0x00, 0xFF);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
  }
}

/* Saves the current game in the chosen slot and loads its players' images */
void SaveGameToSlot(void) {
  EncodedAsset encodedAsset;
  AssetFile file;
  SaveData* save;
  SavedGame* game;
  GameState* state;
  UnkB8* player;
  s32 playerIndex;

  save = GetSaveData();
  game = &save->savedGames[f_CursorIndex];
  SaveOrContinueGame(f_CursorIndex, FALSE);
  state = &game->state;
  f_StateFrame = 0;
  InvalidateBrokenSavedGames();
  if (save->savedGames[f_CursorIndex].isValid) {
    f_MenuState = SAVE_MENU_STATE_SAVED;
    AnimateModeIcons(TRUE);
    for (playerIndex = 0; playerIndex != state->playerCount; playerIndex++) {
      player = &save->savedPlayers[f_CursorIndex][playerIndex];
      OpenEncodedAsset(g_CharacterImageAssets[(s8)player->characterIndex] +
              CHARACTER_FACE_IMAGE_OFFSET,
          &encodedAsset);
      ReadEncodedAsset(
          f_CharacterImages[(f_CursorIndex * MAX_PLAYER_COUNT) + playerIndex],
          &encodedAsset);
      OpenAssetFile(g_CharacterPaletteAssets[(s8)player->characterIndex] +
              (s8)player->paletteIndex,
          &file);
      ReadAssetFile(
          f_CharacterFiles[(f_CursorIndex * MAX_PLAYER_COUNT) + playerIndex],
          GetAssetFileSize(&file), &file);
    }
  } else {
    f_MenuState = SAVE_MENU_STATE_CHOOSING;
  }

  WriteSaveData();
}

void LeaveGameSaveMenu(void) {
  f_StateFrame = 0;
  f_MenuState = SAVE_MENU_STATE_LEAVING;
}

/* Stores the current replay in the chosen record and loads its character's
 * images */
void SaveReplayToSlot(void) {
  EncodedAsset encodedAsset;
  AssetFile file;
  RingShotRecord* record;

  record = &GetSaveData()->ringShotRecords[f_CursorIndex];
  SaveRingShotRecord(f_CursorIndex, D_80105F40);
  f_StateFrame = 0;
  func_8005D0D8();
  if (record->scoreKind & SCORE_KIND_RECORDED) {
    OpenEncodedAsset(g_CharacterImageAssets[(s8)record->characterIndex] +
            CHARACTER_FACE_IMAGE_OFFSET,
        &encodedAsset);
    ReadEncodedAsset(f_CharacterImages[f_CursorIndex], &encodedAsset);
    OpenAssetFile(g_CharacterPaletteAssets[(s8)record->characterIndex] +
            (s8)record->paletteIndex,
        &file);
    ReadAssetFile(
        f_CharacterFiles[f_CursorIndex], GetAssetFileSize(&file), &file);
    f_MenuState = SAVE_MENU_STATE_SAVED;
  } else {
    f_MenuState = SAVE_MENU_STATE_CHOOSING;
  }

  WriteSaveData();
}

void LeaveReplaySaveMenu(void) {
  f_StateFrame = 0;
  f_MenuState = SAVE_MENU_STATE_LEAVING;
}

/* Slides the saved games in and out and lets the player choose one to save over
 * or end the game */
void UpdateGameSaveMenu(ControllerState* controller) {
  SaveData* save;
  s32 row;

  save = GetSaveData();
  f_StateFrame++;
  switch (f_MenuState) {
    case SAVE_MENU_STATE_OPENING:
      for (row = 0; row != SLIDE_ROW_COUNT; row++) {
        if (row < f_StateFrame && f_SlideOffsets[row] > 0) {
          if (f_SlideOffsets[row] == 1) {
            f_SlideOffsets[row] = 0;
          } else {
            f_SlideOffsets[row] += -f_SlideOffsets[row] / 2;
          }
        }
      }

      if (f_StateFrame == SLIDE_FRAMES) {
        f_MenuState = SAVE_MENU_STATE_CHOOSING;
      }

      break;
    case SAVE_MENU_STATE_CHOOSING:
      if (!IsPromptClosed()) {
        break;
      }

      if (controller->pressedButtons & B_BUTTON) {
        PlaySoundWithEffects(SOUND_PANEL_CLOSE, g_NextSoundChannel,
            MENU_VOLUME_MEDIUM, MENU_SOUND_REVERB, 0);
        AnimateModeIcons(TRUE);
        f_StateFrame = 0;
        f_MenuState = SAVE_MENU_STATE_BACKING_OUT;
        break;
      }

      if (controller->pressedButtons & A_BUTTON) {
        if (f_CursorIndex >= 0 && f_CursorIndex < SAVED_GAME_COUNT) {
          if (save->savedGames[f_CursorIndex].isValid) {
            g_PauseScreen = PAUSE_SCREEN_PROMPT;
            OpenPrompt(PROMPT_OVERWRITE_GAME);
            PlaySound(
                SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
          } else {
            SaveGameToSlot();
            PlaySound(SOUND_SAVED, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
          }
        } else {
          g_PauseScreen = PAUSE_SCREEN_PROMPT;
          OpenPrompt(PROMPT_END_GAME);
        }

        break;
      }

      if (controller->repeatedButtons & U_JPAD) {
        f_CursorIndex--;
        AnimateModeIcons(TRUE);
        PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
      } else if (controller->repeatedButtons & D_JPAD) {
        f_CursorIndex++;
        AnimateModeIcons(TRUE);
        PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
      }

      if (f_CursorIndex < 0) {
        f_CursorIndex = END_GAME_ROW;
      }

      if (f_CursorIndex > END_GAME_ROW) {
        f_CursorIndex = 0;
      }

      break;
    case SAVE_MENU_STATE_BACKING_OUT:
      for (row = 0; row != SLIDE_ROW_COUNT; row++) {
        if (row < f_StateFrame) {
          if (f_SlideOffsets[row] >= SLIDE_OUT_START_OFFSET) {
            f_SlideOffsets[row] -= SLIDE_OUT_START_STEP;
          } else {
            f_SlideOffsets[row] += f_SlideOffsets[row] / 2;
          }
        }
      }

      if (f_StateFrame == SLIDE_SOUND_FRAME) {
        PlaySoundWithEffects(SOUND_PANEL_CLOSE, g_NextSoundChannel,
            MENU_VOLUME_MEDIUM, MENU_SOUND_REVERB, 0);
      }

      if (f_StateFrame == SLIDE_FRAMES) {
        f_MenuState = SAVE_MENU_STATE_BACKED_OUT;
      }

      break;
    case SAVE_MENU_STATE_BACKED_OUT:
      CloseSaveMenu();
      g_PauseScreen = PAUSE_SCREEN_NONE;
      SetMenuBackdrop(MENU_BACKDROP_NONE);
      SetFramebufferCount(TRIPLE_BUFFERING);
      SetScreenEffect(SCREEN_EFFECT_NONE, 0);
      g_RoundHud.isLetterboxShown = TRUE;
      g_RoundHud.unkC = TRUE;
      g_RoundHud.isWindIndicatorShown = TRUE;
      g_RoundHud.unk6 = FALSE;
      g_RoundHud.unk8 = TRUE;
      g_RoundHud.isLieIconShown = TRUE;
      g_RoundHud.isClubHeadIconShown = TRUE;
      g_RoundHud.unkB = TRUE;
      g_RoundHud.unkD = TRUE;
      break;
    case SAVE_MENU_STATE_SAVED:
      if (f_StateFrame == SAVED_WAIT_FRAMES) {
        f_MenuState = SAVE_MENU_STATE_LEAVING;
        f_StateFrame = 0;
      }

      break;
    case SAVE_MENU_STATE_LEAVING:
      for (row = 0; row != SLIDE_ROW_COUNT; row++) {
        if (row < f_StateFrame) {
          if (f_SlideOffsets[row] >= SLIDE_OUT_START_OFFSET) {
            f_SlideOffsets[row] -= SLIDE_OUT_START_STEP;
          } else {
            f_SlideOffsets[row] += f_SlideOffsets[row] / 2;
          }
        }
      }

      if (f_StateFrame == SLIDE_SOUND_FRAME) {
        PlaySoundWithEffects(SOUND_PANEL_CLOSE, g_NextSoundChannel,
            MENU_VOLUME_MEDIUM, MENU_SOUND_REVERB, 0);
      }

      if (f_StateFrame == SLIDE_FRAMES) {
        g_PauseScreen = PAUSE_SCREEN_NONE;
        CloseSaveMenu();
        func_80060358();
      }

      break;
  }
}

/*
 * Slides the replay records in and out and lets the player choose one to
 * store the replay in. It returns a REPLAY_MENU_* value.
 */
s32 UpdateReplaySaveMenu(ControllerState* controller) {
  SaveData* save;
  s32 row;

  save = GetSaveData();
  f_StateFrame++;
  switch (f_MenuState) {
    case SAVE_MENU_STATE_OPENING:
      for (row = 0; row != SLIDE_ROW_COUNT; row++) {
        if (row < f_StateFrame && f_SlideOffsets[row] > 0) {
          if (f_SlideOffsets[row] == 1) {
            f_SlideOffsets[row] = 0;
          } else {
            f_SlideOffsets[row] += -f_SlideOffsets[row] / 2;
          }
        }
      }

      if (f_StateFrame == SLIDE_FRAMES) {
        f_MenuState = SAVE_MENU_STATE_CHOOSING;
      }

      break;
    case SAVE_MENU_STATE_CHOOSING:
      if (!IsPromptClosed()) {
        break;
      }

      if (controller->pressedButtons & A_BUTTON) {
        if (f_CursorIndex >= 0 && f_CursorIndex < RING_SHOT_RECORD_COUNT) {
          /* The pointer arithmetic is there to match */
          if ((save->ringShotRecords + f_CursorIndex)->scoreKind &
              SCORE_KIND_RECORDED) {
            g_PauseScreen = PAUSE_SCREEN_PROMPT;
            OpenPrompt(PROMPT_OVERWRITE_REPLAY);
            PlaySound(
                SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            return REPLAY_MENU_PROMPTING;
          }

          SaveReplayToSlot();
          PlaySound(SOUND_SAVED, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
        } else {
          g_PauseScreen = PAUSE_SCREEN_PROMPT;
          OpenPrompt(PROMPT_SKIP_REPLAY);
          return REPLAY_MENU_PROMPTING;
        }
      } else if (controller->pressedButtons & B_BUTTON) {
        PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
        f_CursorIndex = SKIP_REPLAY_ROW;
      } else if (controller->repeatedButtons & U_JPAD) {
        f_CursorIndex--;
        PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
      } else if (controller->repeatedButtons & D_JPAD) {
        f_CursorIndex++;
        PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
      }

      if (f_CursorIndex < 0) {
        f_CursorIndex = SKIP_REPLAY_ROW;
      }

      if (f_CursorIndex > SKIP_REPLAY_ROW) {
        f_CursorIndex = 0;
      }

      break;
    case SAVE_MENU_STATE_SAVED:
      if (f_StateFrame == SAVED_WAIT_FRAMES) {
        f_MenuState = SAVE_MENU_STATE_LEAVING;
        f_StateFrame = 0;
      }

      break;
    case SAVE_MENU_STATE_LEAVING:
      for (row = 0; row != SLIDE_ROW_COUNT; row++) {
        if (row < f_StateFrame) {
          if (f_SlideOffsets[row] >= SLIDE_OUT_START_OFFSET) {
            f_SlideOffsets[row] -= SLIDE_OUT_START_STEP;
          } else {
            f_SlideOffsets[row] += f_SlideOffsets[row] / 2;
          }
        }
      }

      if (f_StateFrame == SAVED_WAIT_FRAMES) {
        CloseSaveMenu();
        return REPLAY_MENU_CLOSED;
      }

      break;
  }

  return REPLAY_MENU_OPEN;
}

/*
 * Draws the replay records: each row's panel, course number, label,
 * golfer and hole text, the Hole Out row, the cursor's frame and the
 * bobbing Save Menu title, all slid by f_SlideOffsets.
 */
void DrawReplaySaveMenu(Gfx** displayListHead) {
  s32 rowLabels[REPLAY_ROW_COUNT];
  s32 rowOffsets[REPLAY_ROW_COUNT];
  Gfx* gfx;
  SaveData* save;
  s32 row;

  /* Counts strips or records; sharing one variable is there to match */
  s32 index;
  s32 stripHeight;
  s32 titleBob;
  s32 titleHeight;
  s32 slideOffset;
  u8* labelImage;
  s32 labelWidth;
  s32 labelHeight;
  s32 text;

  save = GetSaveData();
  gfx = *displayListHead;

  /* Rows slide in pairs, the first of each pair from the left */
  for (row = 0; row != REPLAY_ROW_COUNT; row++) {
    slideOffset = f_SlideOffsets[row / 2];
    rowOffsets[row] = slideOffset;
    if (!(row & 1)) {
      rowOffsets[row] = -slideOffset;
    }
  }

  if (f_MenuKind == SAVE_MENU_REPLAYS) {
    SetUpTextDrawing(&gfx);

    /* The panels, stretched from the panel image a strip at a time */
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF);
    LoadLargePalette(&gfx, (u8*)f_PanelImage + IMAGE_HEADER_SIZE);
    for (index = 0; index != PANEL_STRIP_COUNT; index++) {
      stripHeight = RECORD_PANEL_STRIP_HEIGHT;
      LoadTextureBlock(&gfx,
          (u8*)f_PanelImage + LARGE_PALETTED_TEXELS_OFFSET +
              (index * PANEL_STRIP_SIZE),
          G_IM_FMT_CI, G_IM_SIZ_8b, PANEL_IMAGE_WIDTH, PANEL_STRIP_HEIGHT, 0,
          G_TX_MIRROR, G_TX_MIRROR, 0, 0, 0, 0);
      gDPPipeSync(gfx++);
      for (row = 0; row != RING_SHOT_RECORD_COUNT; row++) {
        if (row == f_CursorIndex) {
          gDPSetPrimColor(gfx++, 0, 0, CHOSEN_PANEL_SHADE, CHOSEN_PANEL_SHADE,
              CHOSEN_PANEL_SHADE, CHOSEN_PANEL_SHADE);
        } else {
          gDPSetPrimColor(gfx++, 0, 0, OTHER_PANEL_SHADE, OTHER_PANEL_SHADE,
              OTHER_PANEL_SHADE, 0xFF);
        }

        DrawTextureRect(&gfx, (rowOffsets[row] + WIDE_PANEL_LEFT) << 2,
            (FIRST_REPLAY_ROW_TOP + (row * REPLAY_ROW_SPACING) +
                (index * stripHeight))
                << 2,
            (rowOffsets[row] + WIDE_PANEL_LEFT + WIDE_PANEL_WIDTH) << 2,
            (FIRST_REPLAY_ROW_TOP + (row * REPLAY_ROW_SPACING) +
                (index * stripHeight) + stripHeight)
                << 2,
            G_TX_RENDERTILE, 0, 0, (PANEL_IMAGE_WIDTH << 10) / WIDE_PANEL_WIDTH,
            (PANEL_IMAGE_HEIGHT << 10) / (stripHeight * PANEL_STRIP_COUNT));
      }

      stripHeight = HOLE_OUT_PANEL_STRIP_HEIGHT;
      gDPPipeSync(gfx++);
      if (f_CursorIndex == row) {
        gDPSetPrimColor(gfx++, 0, 0, CHOSEN_PANEL_SHADE, CHOSEN_PANEL_SHADE,
            CHOSEN_PANEL_SHADE, CHOSEN_PANEL_SHADE);
      } else {
        gDPSetPrimColor(gfx++, 0, 0, OTHER_PANEL_SHADE, OTHER_PANEL_SHADE,
            OTHER_PANEL_SHADE, 0xFF);
      }

      DrawTextureRect(&gfx, (rowOffsets[row] + NARROW_PANEL_LEFT) << 2,
          (FIRST_REPLAY_ROW_TOP + (row * REPLAY_ROW_SPACING) +
              (index * stripHeight))
              << 2,
          (rowOffsets[row] + NARROW_PANEL_LEFT + NARROW_PANEL_WIDTH) << 2,
          (FIRST_REPLAY_ROW_TOP + (row * REPLAY_ROW_SPACING) +
              (index * stripHeight) + stripHeight)
              << 2,
          G_TX_RENDERTILE, 0, 0, (PANEL_IMAGE_WIDTH << 10) / NARROW_PANEL_WIDTH,
          (PANEL_IMAGE_HEIGHT << 10) / (stripHeight * PANEL_STRIP_COUNT));
    }

    /* Each record's "COURSE" and course number */
    LoadSmallPalette(&gfx, 0, (u8*)f_CourseNumbersImage + IMAGE_HEADER_SIZE);
    gDPLoadTextureTile_4b(gfx++,
        ((u32)f_CourseNumbersImage + PALETTED_TEXELS_OFFSET) & ~7, G_IM_FMT_CI,
        COURSE_NUMBERS_WIDTH, COURSE_NUMBERS_HEIGHT, 0, 0,
        COURSE_NUMBERS_WIDTH - 1, COURSE_NUMBERS_HEIGHT - 1, 0, G_TX_MIRROR,
        G_TX_MIRROR, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    for (index = 0; index != RING_SHOT_RECORD_COUNT; index++) {
      RingShotRecord* record = &save->ringShotRecords[index];

      if (!(record->scoreKind & SCORE_KIND_RECORDED)) {
        rowLabels[index] = NO_REPLAY_LABEL;
      } else {
        gDPPipeSync(gfx++);
        if (index == f_CursorIndex) {
          gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF);
        } else {
          gDPSetPrimColor(gfx++, 0, 0, OTHER_PANEL_SHADE, OTHER_PANEL_SHADE,
              OTHER_PANEL_SHADE, 0xFF);
        }

        gDPPipeSync(gfx++);
        DrawTextureRect(&gfx, (rowOffsets[index] + COURSE_WORD_LEFT) << 2,
            (COURSE_TOP + (index * REPLAY_ROW_SPACING)) << 2,
            (rowOffsets[index] + COURSE_WORD_LEFT + COURSE_WORD_WIDTH) << 2,
            (COURSE_TOP + (index * REPLAY_ROW_SPACING) + COURSE_NUMBERS_HEIGHT)
                << 2,
            G_TX_RENDERTILE, COURSE_WORD_TEXEL_LEFT << 5, 0, TEXEL_STEP_ONE,
            TEXEL_STEP_ONE);
        DrawTextureRect(&gfx,
            (rowOffsets[index] + COURSE_WORD_LEFT + COURSE_WORD_WIDTH) << 2,
            (COURSE_TOP + (index * REPLAY_ROW_SPACING)) << 2,
            (rowOffsets[index] + COURSE_WORD_LEFT + COURSE_WORD_WIDTH +
                COURSE_DIGIT_WIDTH)
                << 2,
            (COURSE_TOP + (index * REPLAY_ROW_SPACING) + COURSE_NUMBERS_HEIGHT)
                << 2,
            G_TX_RENDERTILE,
            ((s8)record->courseIndex * COURSE_DIGIT_WIDTH) << 5, 0,
            TEXEL_STEP_ONE, TEXEL_STEP_ONE);
        switch (record->scoreKind & SCORE_KIND_MASK) {
          case SCORE_KIND_ALBATROSS:
            rowLabels[index] = REPLAY_LABEL_ALBATROSS;
            break;
          case SCORE_KIND_HOLE_IN_ONE:
            rowLabels[index] = REPLAY_LABEL_HOLE_IN_ONE;
            break;
          case SCORE_KIND_EAGLE:
            rowLabels[index] = REPLAY_LABEL_EAGLE;
            break;
          case SCORE_KIND_BIRDIE:
            rowLabels[index] = REPLAY_LABEL_BIRDIE;
            break;
          case SCORE_KIND_PAR:
            rowLabels[index] = REPLAY_LABEL_PAR;
            break;
        }
      }
    }

    /* Each record's label */
    gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF);
    for (index = 0; index != RING_SHOT_RECORD_COUNT; index++) {
      if (rowLabels[index] != NO_REPLAY_LABEL) {
        labelImage = f_ReplayLabelImages[rowLabels[index]];
        labelWidth = labelImage[IMAGE_WIDTH_LOW_BYTE];
        labelHeight = labelImage[IMAGE_HEIGHT_LOW_BYTE];
        gDPPipeSync(gfx++);
        if (index == f_CursorIndex) {
          gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF);
        } else {
          gDPSetPrimColor(gfx++, 0, 0, OTHER_PANEL_SHADE, OTHER_PANEL_SHADE,
              OTHER_PANEL_SHADE, 0xFF);
        }

        gDPPipeSync(gfx++);
        LoadSmallPalette(&gfx, 0,
            (u8*)f_ReplayLabelImages[rowLabels[index]] + IMAGE_HEADER_SIZE);
        LoadNibbleTextureBlock(&gfx,
            (u8*)f_ReplayLabelImages[rowLabels[index]] + PALETTED_TEXELS_OFFSET,
            G_IM_FMT_CI, labelWidth, labelHeight, 0, G_TX_MIRROR, G_TX_MIRROR,
            0, 0, 0, 0);
        DrawTextureRect(&gfx, (rowOffsets[index] + LABEL_LEFT) << 2,
            (LABEL_TOP + (index * REPLAY_ROW_SPACING)) << 2,
            (rowOffsets[index] + LABEL_LEFT + labelWidth) << 2,
            (LABEL_TOP + (index * REPLAY_ROW_SPACING) + labelHeight) << 2,
            G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
      }
    }

    /* Each record's golfer */
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    for (index = 0; index != RING_SHOT_RECORD_COUNT; index++) {
      RingShotRecord* record = &save->ringShotRecords[index];

      if (record->scoreKind & SCORE_KIND_RECORDED) {
        gDPPipeSync(gfx++);
        if (index == f_CursorIndex) {
          gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF);
        } else {
          gDPSetPrimColor(gfx++, 0, 0, OTHER_PANEL_SHADE, OTHER_PANEL_SHADE,
              OTHER_PANEL_SHADE, 0xFF);
        }

        gDPPipeSync(gfx++);
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_RGBA16);
        LoadLargePalette(&gfx, f_CharacterFiles[index]);
        LoadTextureBlock(&gfx, f_CharacterImages[index], G_IM_FMT_CI,
            G_IM_SIZ_8b, CHARACTER_IMAGE_WIDTH, CHARACTER_IMAGE_HEIGHT, 0, 0, 0,
            0, 0, 0, 0);
        DrawTextureRect(&gfx, (rowOffsets[index] + CHARACTER_LEFT) << 2,
            (CHARACTER_TOP + (index * REPLAY_ROW_SPACING)) << 2,
            (rowOffsets[index] + CHARACTER_LEFT + CHARACTER_IMAGE_WIDTH) << 2,
            (CHARACTER_TOP + (index * REPLAY_ROW_SPACING) +
                CHARACTER_IMAGE_HEIGHT)
                << 2,
            G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
      }
    }

    /* The cursor's frame around the chosen row */
    {
      s32 rowHeights[REPLAY_ROW_COUNT] = {33, 33, 33, 33, 33, 24};
      s32 rowHalfWidths[REPLAY_ROW_COUNT] = {136, 136, 136, 136, 136, 80};
      s32 centerX;

      centerX = SCREEN_WIDTH / 2;
      gDPSetPrimColor(gfx++, 0, 0, 0x00, 0xBF, 0xFF, 0xFF);
      gDPPipeSync(gfx++);
      gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
      gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
      DrawTextureRect(&gfx,
          (rowOffsets[f_CursorIndex] - (rowHalfWidths[f_CursorIndex] - centerX))
              << 2,
          (FIRST_REPLAY_ROW_TOP + (f_CursorIndex * REPLAY_ROW_SPACING)) << 2,
          (rowOffsets[f_CursorIndex] + (rowHalfWidths[f_CursorIndex] + centerX))
              << 2,
          (FIRST_REPLAY_ROW_TOP + (f_CursorIndex * REPLAY_ROW_SPACING) +
              CURSOR_FRAME_THICKNESS)
              << 2,
          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
      DrawTextureRect(&gfx,
          (rowOffsets[f_CursorIndex] - (rowHalfWidths[f_CursorIndex] - centerX))
              << 2,
          (FIRST_REPLAY_ROW_TOP + (f_CursorIndex * REPLAY_ROW_SPACING) +
              rowHeights[f_CursorIndex] - CURSOR_FRAME_THICKNESS)
              << 2,
          (rowOffsets[f_CursorIndex] + (rowHalfWidths[f_CursorIndex] + centerX))
              << 2,
          (FIRST_REPLAY_ROW_TOP + (f_CursorIndex * REPLAY_ROW_SPACING) +
              rowHeights[f_CursorIndex])
              << 2,
          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
      DrawTextureRect(&gfx,
          (rowOffsets[f_CursorIndex] - (rowHalfWidths[f_CursorIndex] - centerX))
              << 2,
          (FIRST_REPLAY_ROW_TOP + (f_CursorIndex * REPLAY_ROW_SPACING)) << 2,
          (rowOffsets[f_CursorIndex] -
              (rowHalfWidths[f_CursorIndex] - centerX) + CURSOR_FRAME_THICKNESS)
              << 2,
          (FIRST_REPLAY_ROW_TOP + (f_CursorIndex * REPLAY_ROW_SPACING) +
              rowHeights[f_CursorIndex])
              << 2,
          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
      DrawTextureRect(&gfx,
          (rowOffsets[f_CursorIndex] +
              (rowHalfWidths[f_CursorIndex] + centerX) - CURSOR_FRAME_THICKNESS)
              << 2,
          (FIRST_REPLAY_ROW_TOP + (f_CursorIndex * REPLAY_ROW_SPACING)) << 2,
          (rowOffsets[f_CursorIndex] + (rowHalfWidths[f_CursorIndex] + centerX))
              << 2,
          (FIRST_REPLAY_ROW_TOP + (f_CursorIndex * REPLAY_ROW_SPACING) +
              rowHeights[f_CursorIndex])
              << 2,
          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    }

    /* The title's panel, bobbing up and down */
    titleBob = sinf(f_ReplayTitleBobAngle) * TITLE_BOB_HEIGHT;
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    f_ReplayTitleBobAngle += DEGREES_TO_RADIANS(TITLE_BOB_DEGREES_PER_FRAME);
    LoadLargePalette(&gfx, (u8*)f_PanelImage + IMAGE_HEADER_SIZE);
    for (index = 0; index != PANEL_STRIP_COUNT; index++) {
      /* Unused; it is there to match */
      stripHeight = TITLE_PANEL_STRIP_HEIGHT;
      LoadTextureBlock(&gfx,
          (u8*)f_PanelImage + LARGE_PALETTED_TEXELS_OFFSET +
              (index * PANEL_STRIP_SIZE),
          G_IM_FMT_CI, G_IM_SIZ_8b, PANEL_IMAGE_WIDTH, PANEL_STRIP_HEIGHT, 0,
          G_TX_MIRROR, G_TX_MIRROR, 0, 0, 0, 0);
      titleHeight = TITLE_PANEL_STRIP_HEIGHT * PANEL_STRIP_COUNT;
      DrawTextureRect(&gfx, (rowOffsets[1] + TITLE_PANEL_LEFT) << 2,
          (titleBob + TITLE_PANEL_TOP + (index * TITLE_PANEL_STRIP_HEIGHT))
              << 2,
          (rowOffsets[1] + TITLE_PANEL_LEFT + TITLE_PANEL_WIDTH) << 2,
          (titleBob + TITLE_PANEL_TOP + (index * TITLE_PANEL_STRIP_HEIGHT) +
              TITLE_PANEL_STRIP_HEIGHT)
              << 2,
          G_TX_RENDERTILE, 0, 0, (PANEL_IMAGE_WIDTH << 10) / TITLE_PANEL_WIDTH,
          (PANEL_IMAGE_HEIGHT << 10) / titleHeight);
    }

    /* Each record's hole text, or that the row is empty */
    SetUpGradientTextDrawing(&gfx, 0);
    for (index = 0; index != RING_SHOT_RECORD_COUNT; index++) {
      RingShotRecord* record;

      gDPPipeSync(gfx++);
      record = &save->ringShotRecords[index];
      if (index == f_CursorIndex) {
        gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF);
      } else {
        gDPSetPrimColor(gfx++, 0, 0, OTHER_PANEL_SHADE, OTHER_PANEL_SHADE,
            OTHER_PANEL_SHADE, 0xFF);
      }

      gDPPipeSync(gfx++);
      if (!(record->scoreKind & SCORE_KIND_RECORDED)) {
        SetUpGradientTextDrawing(&gfx, 0);
        gDPSetEnvColor(gfx++, 0xFF, 0xFF, 0x2C, 0xFF);
        gDPSetPrimColor(gfx++, 0, 0, 0x40, 0xFF, 0x00, 0xFF);

        /* The saved-game message is printed but never drawn */
        sprintf(g_PrintBuffer, "%s", f_NoSavedGameText);
        DrawCenteredLargeText(&gfx, rowOffsets[index] + (SCREEN_WIDTH / 2),
            EMPTY_TEXT_TOP + (index * REPLAY_ROW_SPACING), f_NoReplayText);
      } else {
        text = ((s8)record->courseIndex * RING_SHOT_HOLE_COUNT) +
            (s8)record->holeIndex;
        SetUpTextDrawing(&gfx);
        SetTextSlot(rowOffsets[index] + HOLE_TEXT_LEFT,
            HOLE_TEXT_TOP + (index * REPLAY_ROW_SPACING),
            text + RING_SHOT_TEXT_FIRST_IMAGE, TEXT_SLOT_LARGE_FONT,
            FIRST_RECORD_TEXT_SLOT + index, TEXT_FLAG_UNK20, 0xFF, 0xFF, 0xFF,
            0xFF);
        DrawLargeTextSlot(&gfx, FIRST_RECORD_TEXT_SLOT + index);
      }
    }

    SetUpGradientTextDrawing(&gfx, 0);
    DrawLargeTextWithShadow(&gfx, rowOffsets[5] + HOLE_OUT_TEXT_LEFT,
        HOLE_OUT_TEXT_TOP, "HoleOut", 0xFF571300, 0xFFDB4C00, 0);
    gDPPipeSync(gfx++);
    DrawLargeTextWithShadow(&gfx, rowOffsets[1] + TITLE_TEXT_LEFT,
        titleBob + TITLE_TEXT_TOP, f_SaveMenuText, 0xFF000000, 0xFFFFFF00, 0);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
  }
}

void OpenControlsScreen(void) {
  EncodedAsset encodedAsset;
  void* picture;

  if (g_GameState.isMiniGolf) {
    picture = AllocMemory(
        OpenEncodedAsset(MINI_GOLF_CONTROLS_PICTURE, &encodedAsset));
    f_PanelImage = picture;
  } else {
    picture = AllocMemory(OpenEncodedAsset(CONTROLS_PICTURE, &encodedAsset));
    f_PanelImage = picture;
  }

  ReadEncodedAsset(picture, &encodedAsset);
  f_SlideOffsets[0] = SCREEN_HEIGHT;
  f_IsControlsShown = TRUE;
  f_StateFrame = 0;
  f_ResumedSongId = GetSongId();
  PlaySong(SONG_CONTROLS, SONG_FLAG_UNK10 | SONG_FLAG_UNREAD);
}

void CloseControlsScreen(void) {
  FreeMemory(&f_PanelImage);
  f_IsControlsShown = FALSE;
  PlaySong(f_ResumedSongId, SONG_FLAG_UNK10 | SONG_FLAG_UNREAD);
}

/* Draws the Controls picture in strips, slid down by f_SlideOffsets[0] */
void DrawControlsScreen(Gfx** displayListHead) {
  Gfx* gfx;
  s32 strip;
  s32 topRaise;
  s32 bottomRaise;

  gfx = *displayListHead;
  if (f_IsControlsShown == TRUE) {
    gDPPipeSync(gfx++);
    gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    LoadLargePalette(&gfx, (u8*)f_PanelImage + IMAGE_HEADER_SIZE);
    for (strip = 0; strip != CONTROLS_STRIP_COUNT; strip++) {
      LoadTextureBlock(&gfx,
          (u8*)f_PanelImage + LARGE_PALETTED_TEXELS_OFFSET +
              (strip * CONTROLS_STRIP_HEIGHT * CONTROLS_PICTURE_WIDTH),
          G_IM_FMT_CI, G_IM_SIZ_8b, CONTROLS_PICTURE_WIDTH,
          CONTROLS_STRIP_HEIGHT, 0, G_TX_MIRROR, G_TX_MIRROR, 0, 0, 0, 0);
      /* How far above the screen top the first strip's top and bottom edges sit
       */
      topRaise = f_SlideOffsets[0] - SCREEN_BORDER;
      bottomRaise = f_SlideOffsets[0] - (SCREEN_BORDER + CONTROLS_STRIP_HEIGHT);
      DrawTextureRect(&gfx, CONTROLS_PICTURE_LEFT << 2,
          ((strip * CONTROLS_STRIP_HEIGHT) - topRaise) << 2,
          (CONTROLS_PICTURE_LEFT + CONTROLS_PICTURE_WIDTH) << 2,
          ((strip * CONTROLS_STRIP_HEIGHT) - bottomRaise) << 2, G_TX_RENDERTILE,
          0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    }

    *displayListHead = gfx;
  }
}

void UpdateControlsScreen(ControllerState* controller) {
  f_StateFrame++;
  switch (f_MenuState) {
    case SAVE_MENU_STATE_OPENING:
      if (f_SlideOffsets[0] > 0) {
        if (f_SlideOffsets[0] == 1) {
          f_SlideOffsets[0] = 0;
        } else {
          f_SlideOffsets[0] += -f_SlideOffsets[0] / 2;
        }
      }

      if (f_StateFrame == SLIDE_SOUND_FRAME) {
        PlaySoundWithEffects(SOUND_CONTROLS_OPEN, g_NextSoundChannel,
            MENU_VOLUME_MEDIUM, MENU_SOUND_REVERB, SOUND_FULL_SCALE);
      }

      if (f_StateFrame == SLIDE_FRAMES) {
        f_MenuState = SAVE_MENU_STATE_CHOOSING;
      }

      break;
    case SAVE_MENU_STATE_CHOOSING:
      if (controller->pressedButtons & (A_BUTTON | B_BUTTON)) {
        PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, MENU_VOLUME_MEDIUM);
        f_StateFrame = 0;
        f_MenuState = SAVE_MENU_STATE_BACKING_OUT;
      }

      break;
    case SAVE_MENU_STATE_BACKING_OUT:
      if (f_SlideOffsets[0] >= SLIDE_OUT_START_OFFSET) {
        f_SlideOffsets[0] -= SLIDE_OUT_START_STEP;
      } else {
        f_SlideOffsets[0] += f_SlideOffsets[0] / 2;
      }

      if (f_StateFrame == SLIDE_SOUND_FRAME) {
        PlaySoundWithEffects(SOUND_CONTROLS_CLOSE, g_NextSoundChannel,
            MENU_VOLUME_MEDIUM, MENU_SOUND_REVERB, SOUND_FULL_SCALE);
      }

      if (f_StateFrame == SLIDE_FRAMES) {
        f_MenuState = SAVE_MENU_STATE_BACKED_OUT;
      }

      break;
    case SAVE_MENU_STATE_BACKED_OUT:
      CloseControlsScreen();
      g_PauseScreen = PAUSE_SCREEN_NONE;
      SetMenuBackdrop(MENU_BACKDROP_NONE);
      SetFramebufferCount(TRIPLE_BUFFERING);
      SetScreenEffect(SCREEN_EFFECT_NONE, 0);
      g_RoundHud.isLetterboxShown = TRUE;
      g_RoundHud.unkC = TRUE;
      g_RoundHud.isWindIndicatorShown = TRUE;
      g_RoundHud.unk6 = FALSE;
      g_RoundHud.unk8 = TRUE;
      g_RoundHud.isLieIconShown = TRUE;
      g_RoundHud.isClubHeadIconShown = TRUE;
      g_RoundHud.unkB = TRUE;
      g_RoundHud.unkD = TRUE;
      break;
  }
}
