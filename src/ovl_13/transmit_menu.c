/*
 * The Transmit menu, which brings characters over from Game Boy cartridges and
 * writes status and experience to them.
 */
#include "common.h"

#include "asset.h"
#include "character.h"
#include "controller.h"
#include "debug_text.h"
#include "game_flags.h"
#include "heap.h"
#include "host_file.h"
#include "libc.h"
#include "math_util.h"
#include "overlay_manager.h"
#include "save.h"
#include "scene.h"
#include "screen.h"
#include "screen_layers.h"
#include "sound.h"
#include "text.h"
#include "transfer_pak.h"
#include "variables.h"

/* The options of each transfer mode */
#define TRANSFER_MODE_COUNT 2
#define MODE_OPTION_COUNT 3

/* f_ModeOptions entries, counted from the message naming each */
#define TRANSMIT_OPTION_MESSAGES 0x13F

enum TransmitOption {
  TRANSMIT_OPTION_NONE = -1,
  TRANSMIT_OPTION_STATUS,
  TRANSMIT_OPTION_EXPERIENCE,
  TRANSMIT_OPTION_CHARACTERS,
  TRANSMIT_OPTION_MAIN_MENU
};

/* The hint below each option, counted the same way */
#define TRANSMIT_OPTION_HINTS 0x143

/* screenStep values on the Bring Characters screen */
#define BRING_READING 0
#define BRING_CHOOSING 2
#define BRING_BRINGING 3
#define BRING_ASKING 4 /* Whether to bring more characters */
#define BRING_BROUGHT 7
#define BRING_CHOOSING_SLOT 8 /* The character to replace */
#define BRING_NO_CHARACTERS 10
#define BRING_CONFIRMING 12 /* Asking the player to start a pak check */
#define BRING_STARTING 13
#define BRING_SHOWING_ERROR 14
#define BRING_NEXT_CARTRIDGE 16
#define BRINGING_FRAME_COUNT 16

/* screenStep values on the Send Status screen */
enum SendStatusStep {
  SEND_STATUS_SHOWING_ERROR, /* Or the canceled pak check, closing */
  SEND_STATUS_CHOOSING,
  SEND_STATUS_READY = 3,
  SEND_STATUS_CHECKING,
  SEND_STATUS_SENT,
  SEND_STATUS_ASKING = 8, /* Whether to overwrite the cartridge's status */
  SEND_STATUS_STARTING,
  SEND_STATUS_CONFIRMING, /* Asking the player to start a pak check */
  SEND_STATUS_TAGGING,
  SEND_STATUS_WRITING
};

/* screenStep values on the Send Experience screen */
enum SendExperienceStep {
  SEND_EXPERIENCE_CHOOSING,
  SEND_EXPERIENCE_SENT = 3,
  SEND_EXPERIENCE_WRITING,
  SEND_EXPERIENCE_FAILED = 8,
  SEND_EXPERIENCE_DONE,
  /* Asking the player to start a pak check */
  SEND_EXPERIENCE_CONFIRMING,
  SEND_EXPERIENCE_NOTHING_TO_SEND
};

/* f_NoticeState: the notice cloud grows, shows its message, then shrinks */
#define NOTICE_HIDDEN 0
#define NOTICE_OPENING 1
#define NOTICE_CLOSING 2
#define NOTICE_SHOWN 3
/* The notices */
#define INSERT_TRANSFER_PAK_MESSAGE 0x149
#define SWAP_FOR_TRANSFER_PAK_MESSAGE 0x148
#define LOOSE_TRANSFER_PAK_MESSAGE 0x14A
#define NO_CARTRIDGE_MESSAGE 0x14C
#define WRONG_CARTRIDGE_MESSAGE 0x14D
#define CARTRIDGE_REMOVED_MESSAGE 0x14E
#define RETRYING_AFTER_REMOVAL_MESSAGE 0x14F
#define CONNECTOR_PROBLEM_MESSAGE 0x151
#define INSERT_RUMBLE_PAK_MESSAGE 0x155
#define CORRUPTED_SAVE_MESSAGE 0x152
#define UNINITIALIZED_SAVE_MESSAGE 0x153
#define SWAP_GAME_BOY_PAKS_MESSAGE 0x154
#define CARTRIDGE_REPLACED_MESSAGE 0x156
#define PAK_CHECK_MESSAGE 0x147
#define NO_EXPERIENCE_MESSAGE 0x157
#define CHOOSE_EXPERIENCE_MESSAGE 0x158
#define EXPERIENCE_SENT_MESSAGE 0x159
#define NOT_FROM_CARTRIDGE_MESSAGE 0x15A
#define CHOOSE_CHARACTER_MESSAGE 0x15C
#define CHARACTER_BROUGHT_MESSAGE 0x15D
#define CHOOSE_SWAPPED_CHARACTER_MESSAGE 0x15E
#define NO_CHARACTERS_MESSAGE 0x15F

/* The hints below the status screen */
#define CHOOSE_CARTRIDGE_MESSAGE 0x160
#define KEEP_POWER_ON_MESSAGE 0x161
#define SEND_STATUS_MESSAGE 0x162
#define HINT_LEFT 28
#define HINT_TOP 186
#define HINT_TEXT_SLOT 3

/* DrawCharacterStats' layout */
#define STATS_NAME_LEFT 192
#define STATS_NAME_TOP 140
#define STATS_LEFT 188 /* Of both the stat labels and the stats */
#define STATS_TOP 172
#define STAT_LABELS_WIDTH 80
#define STAT_LABELS_HEIGHT 8
#define SHOT_TYPE_LEFT 212
#define SHOT_TYPE_TOP 152
#define SHOT_TYPE_WIDTH 64
#define SHOT_TYPE_HEIGHT 8
#define CURVE_BAR_LEFT 204
#define CURVE_BAR_TOP 162
#define CURVE_BAR_WIDTH 72
#define CURVE_BAR_HEIGHT 8
#define CURVE_SEGMENT_WIDTH 8
#define CURVE_SEGMENT_COUNT 9 /* From a fade of 4 to a draw of 4 */

/* The rows of the shot types image */
enum ShotType {
  SHOT_TYPE_STRAIGHT,
  SHOT_TYPE_DRAW,
  SHOT_TYPE_FADE
};

/* The faces of the characters brought over, along the bottom */
#define FACES_LEFT 24
#define FACES_TOP 136
#define FACE_SPACING 40
#define FACE_SIZE 32
#define FACE_CURSOR_MARGIN 9

/* Where a cartridge character is on the Bring Characters list */
enum ListingState {
  LISTING_NONE,
  LISTING_SHOWN,
  LISTING_BRINGING /* Moving to its slot */
};

/* The listed cartridge characters on the Bring Characters screen, each above
 * its controller */
#define LISTED_FACES_LEFT 28
#define LISTED_FACES_TOP 56
#define LISTED_FACE_SPACING 46
#define LISTED_ICONS_TOP 88
#define SHOWN_LISTED_COUNT 6
#define BRING_CURSOR_MARGIN 8

/* The arrows beside the list when it scrolls */
#define ARROW_FRAME_COUNT 8
#define ARROWS_IMAGE_SIZE 0x200 /* Of each frame's image */
#define ARROW_SIZE 16
#define ARROWS_TOP 92
#define LEFT_ARROW_LEFT 16
#define LEFT_ARROW_ROW 48
#define RIGHT_ARROW_LEFT 288
#define RIGHT_ARROW_ROW 32
#define CONTROLLER_ICONS_LEFT 60
#define CONTROLLER_ICONS_TOP 72
#define CONTROLLER_ICON_SPACING 56
#define CONTROLLER_ICON_SIZE 32
#define CONTROLLER_CURSOR_MARGIN 8
/* The pointer at the chosen cartridge on the Send Status screen */
#define POINTER_WIDTH 24
#define POINTER_HEIGHT 24
#define POINTER_IMAGE_HEIGHT 76
#define POINTER_TOP 76
#define EXPERIENCE_LEFT 20 /* Below each face */
#define EXPERIENCE_TOP 174
/* The options, centered above the hint, with the pointer at the left of the
 * chosen one */
#define OPTIONS_CENTER_X 160
#define OPTIONS_TOP 64
#define OPTION_SPACING 18
#define OPTION_POINTER_TOP 60
/* Between the pointer at rest and the chosen option's name */
#define OPTION_POINTER_GAP 8
/* The backdrop: the course scene at double size in strips, then the title and
 * the wood panel */
#define COURSE_SCENE_LEFT 8
#define COURSE_SCENE_TOP 8
#define COURSE_SCENE_WIDTH 152
#define COURSE_SCENE_STRIP_ROWS 11
#define COURSE_SCENE_STRIP_COUNT 11
#define TITLE_LEFT 60
#define TITLE_TOP 16
#define TITLE_WIDTH 200
#define TITLE_STRIP_ROWS 10
#define TITLE_STRIP_COUNT 3
#define WOOD_PANEL_LEFT 16
#define WOOD_PANEL_TOP 48
#define WOOD_PANEL_WIDTH 288
#define WOOD_PANEL_STRIP_ROWS 7
#define WOOD_PANEL_STRIP_COUNT 26

/* The notice cloud grows by one step each time it is drawn, up to this size */
#define NOTICE_FULL_SIZE 4
/* The cloud is drawn in two strips about its center, its half-size growing by a
 * step per size */
#define NOTICE_CENTER_X 160
#define NOTICE_CENTER_Y 116
#define NOTICE_WIDTH 256
#define NOTICE_STRIP_HEIGHT 32
#define NOTICE_STRIP_COUNT 2
#define NOTICE_STRIP_SIZE (NOTICE_WIDTH * NOTICE_STRIP_HEIGHT / 2)
#define NOTICE_HALF_WIDTH_STEP 34
#define NOTICE_HALF_HEIGHT_STEP 16
#define NOTICE_TEXT_TOP 110
#define NOTICE_TEXT_SLOT 3

/* The Accessing sign, drawn in strips */
#define ACCESSING_LEFT 52
#define ACCESSING_TOP 64
#define ACCESSING_WIDTH 216
#define ACCESSING_STRIP_HEIGHT 9
#define ACCESSING_STRIP_COUNT 10
#define ACCESSING_STRIP_SIZE (ACCESSING_WIDTH * ACCESSING_STRIP_HEIGHT)

/* pressingHandPosition values: the hand pressing the A button, while the menu
 * waits for it */
#define PRESSING_HAND_HIDDEN 0
#define PRESSING_HAND_LOW 1
#define PRESSING_HAND_FOR_NOTICE 2
#define LOW_PRESSING_HAND_LEFT 272
#define LOW_PRESSING_HAND_TOP 192
#define NOTICE_PRESSING_HAND_LEFT 256
#define NOTICE_PRESSING_HAND_TOP 128
/* The hand's three images each show for three draws */
#define PRESSING_HAND_FRAMES_PER_IMAGE 3
#define PRESSING_HAND_FRAME_COUNT (3 * PRESSING_HAND_FRAMES_PER_IMAGE)
#define PRESSING_HAND_SIZE 24
#define PRESSING_HAND_IMAGE_SIZE (PRESSING_HAND_SIZE * PRESSING_HAND_SIZE)

/* Where the Game Boy cartridge's RAM appears through the Transfer Pak */
#define CARTRIDGE_RAM_ADDRESS 0xA000
#define CARTRIDGE_RAM_SIZE 0x2000

/* The largest value the status sends for each figure */
#define MAX_STATUS_VALUE 999

/* The status data BuildStatusData fills for the Game Boy cartridge; its values
 * are little-endian */
#define STATUS_CLEARED_SIZE 0x400
/* Scores are sent raised by this, to keep them positive */
#define STATUS_SCORE_BIAS 54
#define STATUS_THIRD_PLACE (1 << 0)
#define STATUS_SECOND_PLACE (1 << 1)
#define STATUS_FIRST_PLACE (1 << 2)
#define STATUS_UNKAF0_COLUMN_COUNT 14

typedef struct StatusTime {
  /* 0x0 */ u8 minutes;
  /* 0x1 */ u8 seconds;
  /* 0x2 */ u8 hundredths;
  /* 0x3 */ u8 unk3;
  /* 0x4 */ u8 unk4;
} StatusTime; /* size = 0x5 */

/* One course's figures in the status data, or every course's together */
typedef struct StatusCourse {
  /* 0x00 */ u8 roundCount[2];
  /* 0x02 */ u8 bestScore;
  /* 0x03 */ u8 averageStrokes;
  /* 0x04 */ u8 longestDrive[2];
  /* 0x06 */ u8 averageDrive[2];
  /* 0x08 */ u8 unk8;
  /* 0x09 */ u8 averagePutts; /* In tenths, from PackTenths */
  /* 0x0A */ u8 parOrBetterRate; /* As percentages */
  /* 0x0B */ u8 parOnRate;
  /* 0x0C */ u8 unkC;
  /* 0x0D */ u8 places; /* STATUS_*_PLACE bits */
  /* 0x0E */ u8 characterBestScores[CHARACTER_COUNT];
  /* 0x20 */ StatusTime bestTimes[BEST_TIME_COUNT];
  /* 0x2F */ u8
      birdieBadges[3]; /* One bit per hole, from the first hole's top bit */
} StatusCourse; /* size = 0x32 */

typedef struct StatusUnkAF0Bits {
  /* 0x0 */ u8 nonzeroBits[2];
  /* 0x2 */ u8 flagBits[2];
} StatusUnkAF0Bits; /* size = 0x4 */

/* What the player has opened and won, in the status data */
typedef struct StatusProgress {
  /* 0x00 */ u8 openCourses;
  /* 0x01 */ u8 ringShotStars[RING_SHOT_COURSE_COUNT]; /* One bit per hole */
  /* 0x07 */ u8 birdieBadgeCount;
  /* 0x08 */ StatusUnkAF0Bits unkAF0Bits[SAVE_UNKAF0_ROW_COUNT];
} StatusProgress; /* size = 0x50 */

typedef struct StatusData {
  /* 0x000 */ StatusCourse courses[COURSE_COUNT];
  /* 0x12C */ StatusCourse allCourses;
  /* 0x15E */ StatusProgress progress;
} StatusData; /* size = 0x1AE */

/* A Game Boy save in the cartridge RAM */
#define CARTRIDGE_SAVE_SIZE 0x200
#define SAVE_CHECKSUM_OFFSET 0x30
#define SAVE_UNKB_OFFSET 0xB
#define SAVE_CHARACTER_OFFSET 0x10
/* The save's 16-bit values are little-endian */
#define SAVE_HALFWORD_SIZE 2
#define SAVE_HAS_STATUS_OFFSET 0xA0
#define SAVE_STATUS_ADDRESS_OFFSET 0xA2
#define SAVE_STATUS_SIZE_OFFSET 0xA4
#define SAVE_STATUS_SUM_OFFSET 0xA6
#define STATUS_ADDRESS_BIAS 0x6000
#define STATUS_DATA_SIZE 0x800

#define SAVE_SIGNATURE_OFFSET 0x20
/* Records of where the save's data is: one per Game Boy character, then the
 * status's */
#define SAVE_RECORDS_OFFSET 0x40
#define SAVE_RECORD_SIZE 0x20
#define RECORD_ADDRESS_OFFSET 2
#define RECORD_SIZE_OFFSET 4
#define RECORD_SUM_OFFSET 6
/* A Game Boy character's data */
#define CHARACTER_DATA_SIZE 0x800
#define DATA_EXPERIENCE_OFFSET 0x15C
#define DATA_UNK15E_OFFSET 0x15E
/* The experience and the byte after it */
#define DATA_EXPERIENCE_WRITE_SIZE 3
#define MAX_EXPERIENCE 0xFFFF

/* What was wrong with the character's cartridge, reported when it was the only
 * one found */
enum CartridgeFault {
  CARTRIDGE_FAULT_NONE,
  CARTRIDGE_FAULT_UNINITIALIZED, /* Its save lacks the signature */
  CARTRIDGE_FAULT_CORRUPTED
};

/* How far ReadCartridgeCharacters has checked a save */
#define NOT_CHECKED 0
#define SAVE_CHECKED 1 /* Its signature and checksum */
#define DATA_CHECKED 2 /* Its character data's sum too */
#define SAVE_READ_ATTEMPTS 5
#define DATA_READ_ATTEMPTS 3
#define RECORD_PRESENT 1 /* The first byte of a record in use */

/* What CheckStatusCartridge found */
enum StatusCheck {
  STATUS_CHECK_NO_STATUS,
  STATUS_CHECK_FAILED, /* With the cause in transferResult */
  STATUS_CHECK_HAS_STATUS,
  STATUS_CHECK_READ_FAILED /* With the error in transferResult */
};

/* What WriteStatusToCartridge did */
enum StatusWriteResult {
  STATUS_WRITE_DONE,
  STATUS_WRITE_PAK_ERROR, /* With the error in transferResult */
  STATUS_WRITE_FAILED /* With the cause in transferResult */
};

/* What WriteExperienceToCartridge did */
enum ExperienceWriteResult {
  EXPERIENCE_WRITE_DONE,
  EXPERIENCE_WRITE_NOT_FROM_CARTRIDGE,
  EXPERIENCE_WRITE_FAILED, /* With the cause in transferResult */
  EXPERIENCE_WRITE_PAK_ERROR /* With the error in transferResult */
};

#define WOOD_PANEL_ASSET 0x8FD
#define CONTROLLER_ICONS_ASSET 0x900
#define TITLE_ASSET 0x93F
#define CURVE_SEGMENTS_ASSET 0x8EE
#define STAT_BARS_ASSET 0x95D
#define SHOT_TYPES_ASSET 0x979
#define ACCESSING_ASSET 0x96B
#define STAT_LABELS_ASSET 0x978

#define FIRST_GAME_BOY_GOLFER CHARACTER_KID

/* WriteExperienceToCartridge and WriteStatusToCartridge share one copy of this
 */
extern const char f_WriteSumFormat[];

void LoadCursorBox(void* image);
void UpdateBringCharacters(void);
s32 ReadCartridgeCharacters(void);
void BringOverCharacter(s32 listedIndex, s32 slot);
void ListPresentCharacters(void);
/* The character's stats from 0x1C on are laid out as GenerateClubSet takes them
 */
void GenerateClubSet(s32 setIndex, GameBoyCharacter* stats);
void UpdateSendStatus(void);
void UpdateSendExperience(void);
s32 HaveAllRumblePaks(void);
void ClearCartridgeCharacters(void);
void func_8005DC50(void);
void ResetSendStatus(void);
void ResetSendExperience(void);
s32 TagCartridges(void);
s32 CheckStatusCartridge(s32 cartridgeIndex);
s32 WriteStatusToCartridge(s32 cartridgeIndex);
s32 WriteExperienceToCartridge(s32 characterIndex);
void SetNoticeState(s32 state, MessageId message);
s32 GetNoticeState(void);

/* Results of opening and accessing the Game Boy cartridges, after the notice
 * each shows */
/* ReadCartridgeCharacters has more saves to read */
#define PAK_RESULT_STILL_READING -1
#define PAK_RESULT_CONNECTOR_PROBLEM 0
#define PAK_RESULT_OK 1
#define PAK_RESULT_NO_CHARACTERS 2
#define PAK_RESULT_NO_PAK 3
#define PAK_RESULT_NO_CARTRIDGE 4
#define PAK_RESULT_WRONG_CARTRIDGE 5
#define PAK_RESULT_OTHER_PAK 6
#define PAK_RESULT_LOOSE_PAK 7
#define PAK_RESULT_REMOVED_RETRYING 8
#define PAK_RESULT_CARTRIDGE_REMOVED 9
#define PAK_RESULT_CORRUPTED 10
#define PAK_RESULT_CARTRIDGE_REPLACED 11
#define PAK_RESULT_UNINITIALIZED 12
#define GAME_BOY_PAK_OPEN_ATTEMPTS 3

/* OpenGameBoyPak knows Mario Golf's cartridge by its title's start and
 * Nintendo's licensee code */
#define GAME_BOY_TITLE_CHECKED_LENGTH 14
#define GAME_BOY_COMPANY_CODE 0x3031 /* "01" */

/*
 * g_GameBoyTransfer.pressingHandImage under a scalar name. Reading it as a
 * scalar is there to match: GCC keeps a struct field's constant address in a
 * register when it is used twice.
 */
extern void* g_PressingHandImage;

void DrawCharacterStats(Gfx** displayListHead, GameBoyCharacter* character,
    GameBoyTransfer* transfer);
void DrawImportedFaces(Gfx** displayListHead, GameBoyTransfer* transfer);
void DrawCursorBox(Gfx** displayListHead, s32 left, s32 top);

s32 OpenGameBoyPak(s32 port, OSGbpakId* header, NUContPakFile* file);
s32 OpenGameBoyPakWithRetries(s32 port, OSGbpakId* header, NUContPakFile* file);

s32 RoundedWholePart(f32 value);

/* The buttons of every controller together, which any player may use on the
 * menu */
static ControllerState f_TransmitController;

/* Debug code that writes the cartridge RAM of controller 1's Game Boy cartridge
 * to the host */
void DumpCartridgeRam(void) {
  /* The transfer pointer is there to match */
  GameBoyTransfer* transfer = &g_GameBoyTransfer;
  u8 cartridgeRam[CARTRIDGE_RAM_SIZE];

  nuContGBPakMgrInit();
  PrintDebugText("          ", 10, 3);
  PrintDebugText("          ", 10, 4);
  PrintDebugText("          ", 10, 5);

  /* The original takes 0 from OpenGameBoyPakWithRetries as success, which is
   * not PAK_RESULT_OK */
  if (OpenGameBoyPakWithRetries(
          0, &transfer->cartridgeHeader, &transfer->pakFile) == 0) {
    PrintDebugText("GOLF OK", 10, 3);
    if (nuContGBPakFread(&transfer->pakFile, CARTRIDGE_RAM_ADDRESS,
            cartridgeRam, CARTRIDGE_RAM_SIZE) == 0) {
      PrintDebugText("READ OK", 10, 4);
      if (WriteHostFile("x:/golf/capture/gbsram.bin", CARTRIDGE_RAM_SIZE) ==
          0) {
        PrintDebugText("WRITE OK", 10, 5);
      }
    }
  }
}

/*
 * Loads the menu's images, and the faces and palettes of the Game Boy golfers,
 * then starts the menu's song and stops any rumble.
 */
void LoadTransmitMenu(void) {
  AssetFile file;
  EncodedAsset asset;
  s32 index;
  s32 size;

  ReadEncodedAsset(g_GameBoyTransfer.woodPanelImage =
                       AllocMemory(OpenEncodedAsset(WOOD_PANEL_ASSET, &asset)),
      &asset);
  ReadEncodedAsset(g_GameBoyTransfer.courseSceneImage = AllocMemory(
                       OpenEncodedAsset(ASSET_COURSE_SCENE, &asset)),
      &asset);
  ReadEncodedAsset(g_GameBoyTransfer.pointerImage = AllocMemory(
                       OpenEncodedAsset(ASSET_POINTING_HAND, &asset)),
      &asset);
  ReadEncodedAsset(g_GameBoyTransfer.arrowsImage = AllocMemory(
                       OpenEncodedAsset(ASSET_GREEN_ARROWS, &asset)),
      &asset);
  ReadEncodedAsset(g_GameBoyTransfer.controllerIconsImage = AllocMemory(
                       OpenEncodedAsset(CONTROLLER_ICONS_ASSET, &asset)),
      &asset);
  ReadEncodedAsset(g_GameBoyTransfer.titleImage =
                       AllocMemory(OpenEncodedAsset(TITLE_ASSET, &asset)),
      &asset);
  ReadEncodedAsset(g_GameBoyTransfer.cursorBoxImage =
                       AllocMemory(OpenEncodedAsset(ASSET_RAINBOW_BOX, &asset)),
      &asset);
  ReadEncodedAsset(g_GameBoyTransfer.curveSegmentsImage = AllocMemory(
                       OpenEncodedAsset(CURVE_SEGMENTS_ASSET, &asset)),
      &asset);
  ReadEncodedAsset(g_GameBoyTransfer.pressingHandImage = AllocMemory(
                       OpenEncodedAsset(ASSET_PRESSING_HAND, &asset)),
      &asset);
  ReadEncodedAsset(g_GameBoyTransfer.statBarsImage =
                       AllocMemory(OpenEncodedAsset(STAT_BARS_ASSET, &asset)),
      &asset);
  ReadEncodedAsset(g_GameBoyTransfer.shotTypesImage =
                       AllocMemory(OpenEncodedAsset(SHOT_TYPES_ASSET, &asset)),
      &asset);
  ReadEncodedAsset(g_GameBoyTransfer.noticeCloudImage =
                       AllocMemory(OpenEncodedAsset(ASSET_WIDE_CLOUD, &asset)),
      &asset);
  ReadEncodedAsset(g_GameBoyTransfer.accessingImage =
                       AllocMemory(OpenEncodedAsset(ACCESSING_ASSET, &asset)),
      &asset);
  ReadEncodedAsset(g_GameBoyTransfer.statLabelsImage =
                       AllocMemory(OpenEncodedAsset(STAT_LABELS_ASSET, &asset)),
      &asset);
  g_GameBoyTransfer.pressingHandPosition = PRESSING_HAND_HIDDEN;
  g_GameBoyTransfer.isAccessing = FALSE;
  for (index = 0; index != GAME_BOY_GOLFER_COUNT; index++) {
    ReadEncodedAsset(
        g_GameBoyTransfer.faceImages[index] = AllocMemory(OpenEncodedAsset(
            g_CharacterImageAssets[FIRST_GAME_BOY_GOLFER + index], &asset)),
        &asset);
    OpenAssetFile(
        g_CharacterPaletteAssets[FIRST_GAME_BOY_GOLFER + index], &file);
    size = GetAssetFileSize(&file);
    ReadAssetFile(
        g_GameBoyTransfer.facePalettes[index] = AllocMemory(size), size, &file);
  }

  PlaySong(SONG_TRANSMIT_MENU, 0);
  LoadCursorBox(g_GameBoyTransfer.cursorBoxImage);
  nuContRmbForceStop();
}

void FreeTransmitMenu(void) {
  s32 index;

  FreeMemory(&g_GameBoyTransfer.woodPanelImage);
  FreeMemory(&g_GameBoyTransfer.courseSceneImage);
  FreeMemory(&g_GameBoyTransfer.pointerImage);
  FreeMemory(&g_GameBoyTransfer.arrowsImage);
  FreeMemory(&g_GameBoyTransfer.controllerIconsImage);
  FreeMemory(&g_GameBoyTransfer.titleImage);
  FreeMemory(&g_GameBoyTransfer.cursorBoxImage);
  FreeMemory(&g_GameBoyTransfer.curveSegmentsImage);
  FreeMemory(&g_GameBoyTransfer.pressingHandImage);
  FreeMemory(&g_GameBoyTransfer.statBarsImage);
  FreeMemory(&g_GameBoyTransfer.shotTypesImage);
  FreeMemory(&g_GameBoyTransfer.statLabelsImage);
  FreeMemory(&g_GameBoyTransfer.noticeCloudImage);
  FreeMemory(&g_GameBoyTransfer.accessingImage);
  for (index = 0; index != GAME_BOY_CHARACTER_COUNT; index++) {
    FreeMemory(&g_GameBoyTransfer.faceImages[index]);
    FreeMemory(&g_GameBoyTransfer.facePalettes[index]);
  }
}

/*
 * Runs the Bring Characters screen. After a pak check it reads the cartridges'
 * characters, lets the player choose one and, once all four slots are full, the
 * slot to replace, then brings the character over and offers to bring more from
 * another Game Boy cartridge.
 */
void UpdateBringCharacters(void) {
  /* The transfer pointer is there to match */
  GameBoyTransfer* transfer = &g_GameBoyTransfer;
  s32 result;

  switch (transfer->screenStep) {
    default:
      switch (GetNoticeState()) {
        case NOTICE_HIDDEN:
          transfer->screen = TRANSMIT_SCREEN_OPTIONS;
          break;
        case NOTICE_CLOSING:
          break;
        case NOTICE_SHOWN:
          transfer->pressingHandPosition = PRESSING_HAND_FOR_NOTICE;
          if (f_TransmitController.pressedButtons & A_BUTTON) {
            transfer->pressingHandPosition = PRESSING_HAND_HIDDEN;
            SetNoticeState(NOTICE_CLOSING, PAK_CHECK_MESSAGE);
            PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, SOUND_FULL_SCALE);
          }

          break;
      }

      break;
    case BRING_NEXT_CARTRIDGE:
      SetNoticeState(NOTICE_OPENING, SWAP_GAME_BOY_PAKS_MESSAGE);
      ClearCartridgeCharacters();
      transfer->screenStep = BRING_CONFIRMING;
      break;
    case BRING_STARTING:
      SetNoticeState(NOTICE_OPENING, PAK_CHECK_MESSAGE);
      ClearCartridgeCharacters();
      transfer->screenStep = BRING_CONFIRMING;
      break;
    case BRING_CONFIRMING:
      switch (GetNoticeState()) {
        case NOTICE_HIDDEN:
          if (!transfer->isCanceled) {
            transfer->isAccessing = TRUE;
            transfer->screenStep = BRING_READING;
          } else {
            transfer->isAccessing = FALSE;
            transfer->screen = TRANSMIT_SCREEN_OPTIONS;
          }

          break;
        case NOTICE_CLOSING:
          break;
        case NOTICE_SHOWN:
          transfer->pressingHandPosition = PRESSING_HAND_FOR_NOTICE;
          if (f_TransmitController.pressedButtons & A_BUTTON) {
            transfer->pressingHandPosition = PRESSING_HAND_HIDDEN;
            transfer->isCanceled = FALSE;
            SetNoticeState(NOTICE_CLOSING, PAK_CHECK_MESSAGE);
            PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, SOUND_FULL_SCALE);
          } else if (f_TransmitController.pressedButtons & B_BUTTON) {
            transfer->pressingHandPosition = PRESSING_HAND_HIDDEN;
            transfer->isCanceled = TRUE;
            SetNoticeState(NOTICE_CLOSING, PAK_CHECK_MESSAGE);
            PlaySound(SOUND_MENU_CANCEL, g_NextSoundChannel, SOUND_FULL_SCALE);
          }

          break;
      }

      break;
    case BRING_READING:
      result = ReadCartridgeCharacters();
      switch (result) {
        case PAK_RESULT_OK:
          transfer->screenStep = BRING_CHOOSING;
          transfer->isAccessing = FALSE;
          break;
        case PAK_RESULT_STILL_READING: /* The empty case is there to match */
          break;
        case PAK_RESULT_NO_CHARACTERS:
          SetNoticeState(NOTICE_OPENING, NO_CHARACTERS_MESSAGE);
          transfer->screenStep = BRING_NO_CHARACTERS;
          transfer->isAccessing = FALSE;
          break;
        default:
          ShowTransferError(result);
          transfer->screenStep = BRING_SHOWING_ERROR;
          transfer->isAccessing = FALSE;
          break;
      }

      break;
    case BRING_BRINGING:
      g_HasBroughtOverCharacter = TRUE;
      if (++transfer->bringingFrame == BRINGING_FRAME_COUNT) {
        BringOverCharacter(
            transfer->listedCharacterIndex, transfer->characterIndex);
        transfer->listingStates[transfer
                ->listedCharacters[transfer->listedCharacterIndex]] =
            LISTING_NONE;
        GenerateClubSet(FIRST_GAME_BOY_GOLFER + transfer->characterIndex,
            &g_GameBoyCharacters[transfer->characterIndex]);
        SetTextName(g_GameBoyCharacters[transfer->characterIndex].name);
        SetTextPaletteColor(255, 255, 0, 0);
        if (transfer->characterCount < GAME_BOY_GOLFER_COUNT) {
          transfer->characterCount++;
        }

        transfer->screenStep = BRING_BROUGHT;
        ListPresentCharacters();
      }

      break;
    case BRING_CHOOSING_SLOT:
      if (f_TransmitController.pressedButtons & A_BUTTON) {
        PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, SOUND_FULL_SCALE);
        transfer->listingStates[transfer
                ->listedCharacters[transfer->listedCharacterIndex]] =
            LISTING_BRINGING;
        transfer->screenStep = BRING_BRINGING;
        transfer->bringingFrame = 0;
      } else if (f_TransmitController.pressedButtons & B_BUTTON) {
        PlaySound(SOUND_MENU_CANCEL, g_NextSoundChannel, SOUND_FULL_SCALE);
        transfer->screenStep = BRING_CHOOSING;
      } else if ((f_TransmitController.repeatedButtons & L_JPAD) &&
          transfer->characterIndex > 0) {
        transfer->characterIndex--;
        PlaySound(SOUND_MENU_MOVE, g_NextSoundChannel, SOUND_FULL_SCALE);
      } else if ((f_TransmitController.repeatedButtons & R_JPAD) &&
          transfer->characterIndex < GAME_BOY_GOLFER_COUNT - 1) {
        transfer->characterIndex++;
        PlaySound(SOUND_MENU_MOVE, g_NextSoundChannel, SOUND_FULL_SCALE);
      }

      break;
    case BRING_CHOOSING:
      if (f_TransmitController.pressedButtons & A_BUTTON) {
        PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, SOUND_FULL_SCALE);
        if (transfer->characterCount == GAME_BOY_GOLFER_COUNT) {
          transfer->screenStep = BRING_CHOOSING_SLOT;
        } else {
          transfer->screenStep = BRING_BRINGING;
          transfer->characterIndex = transfer->characterCount;
          transfer->listingStates[transfer
                  ->listedCharacters[transfer->listedCharacterIndex]] =
              LISTING_BRINGING;
          transfer->bringingFrame = 0;
        }
      } else if (f_TransmitController.pressedButtons & B_BUTTON) {
        PlaySound(SOUND_MENU_CANCEL, g_NextSoundChannel, SOUND_FULL_SCALE);
        transfer->screen = TRANSMIT_SCREEN_OPTIONS;
      } else if ((f_TransmitController.repeatedButtons & L_JPAD) &&
          transfer->listedCharacterIndex > 0) {
        transfer->listedCharacterIndex--;
        if (transfer->listedCharacterIndex < transfer->unk7B8) {
          transfer->unk7B8--;
        }

        PlaySound(SOUND_MENU_MOVE, g_NextSoundChannel, SOUND_FULL_SCALE);
      } else if ((f_TransmitController.repeatedButtons & R_JPAD) &&
          transfer->listedCharacterIndex < transfer->listedCharacterCount - 1) {
        transfer->listedCharacterIndex++;
        if (transfer->unk7B8 + 5 < transfer->listedCharacterIndex) {
          transfer->unk7B8++;
        }

        PlaySound(SOUND_MENU_MOVE, g_NextSoundChannel, SOUND_FULL_SCALE);
      }

      break;
    case BRING_BROUGHT:
      transfer->pressingHandPosition = PRESSING_HAND_LOW;
      if (f_TransmitController.pressedButtons & A_BUTTON) {
        transfer->pressingHandPosition = PRESSING_HAND_HIDDEN;
        PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, SOUND_FULL_SCALE);
        if (transfer->characterCount != GAME_BOY_GOLFER_COUNT) {
          OpenPrompt(PROMPT_BRING_MORE_CHARACTERS);
          transfer->screenStep = BRING_ASKING;
        } else {
          transfer->screen = TRANSMIT_SCREEN_OPTIONS;
        }
      }

      break;
    case BRING_ASKING:
      switch (UpdatePrompt(&f_TransmitController)) {
        case PROMPT_RESULT_BRING_MORE:
          transfer->screenStep = BRING_NEXT_CARTRIDGE;
          break;
        case PROMPT_RESULT_BRING_NO_MORE:
          transfer->screen = TRANSMIT_SCREEN_OPTIONS;
          break;
      }

      break;
    case 5: /* Step 5 does nothing */
      break;
  }
}

void ShowPakError(s32 pfsError) {
  switch (pfsError) {
    case PFS_ERR_NOPACK:
      SetNoticeState(NOTICE_OPENING, INSERT_TRANSFER_PAK_MESSAGE);
      break;
    case PFS_ERR_DEVICE:
      SetNoticeState(NOTICE_OPENING, SWAP_FOR_TRANSFER_PAK_MESSAGE);
      break;
    case PFS_ERR_CONTRFAIL:
      SetNoticeState(NOTICE_OPENING, CONNECTOR_PROBLEM_MESSAGE);
      break;
    case PFS_ERR_NO_GBCART:
      SetNoticeState(NOTICE_OPENING, NO_CARTRIDGE_MESSAGE);
      break;
    case PFS_ERR_NEW_GBCART:
      SetNoticeState(NOTICE_OPENING, RETRYING_AFTER_REMOVAL_MESSAGE);
      break;
  }
}

void ShowTransferError(u32 result) {
  g_GameBoyTransfer.pressingHandPosition = PRESSING_HAND_FOR_NOTICE;
  switch (result) {
    case PAK_RESULT_NO_CHARACTERS:
      SetNoticeState(NOTICE_OPENING, NO_CHARACTERS_MESSAGE);
      break;
    case PAK_RESULT_NO_PAK:
      SetNoticeState(NOTICE_OPENING, INSERT_TRANSFER_PAK_MESSAGE);
      break;
    case PAK_RESULT_NO_CARTRIDGE:
      SetNoticeState(NOTICE_OPENING, NO_CARTRIDGE_MESSAGE);
      break;
    case PAK_RESULT_WRONG_CARTRIDGE:
      SetNoticeState(NOTICE_OPENING, WRONG_CARTRIDGE_MESSAGE);
      break;
    case PAK_RESULT_OTHER_PAK:
      SetNoticeState(NOTICE_OPENING, SWAP_FOR_TRANSFER_PAK_MESSAGE);
      break;
    case PAK_RESULT_LOOSE_PAK:
      SetNoticeState(NOTICE_OPENING, LOOSE_TRANSFER_PAK_MESSAGE);
      break;
    case PAK_RESULT_REMOVED_RETRYING:
      SetNoticeState(NOTICE_OPENING, RETRYING_AFTER_REMOVAL_MESSAGE);
      break;
    case PAK_RESULT_CARTRIDGE_REMOVED:
      SetNoticeState(NOTICE_OPENING, CARTRIDGE_REMOVED_MESSAGE);
      break;
    case PAK_RESULT_CONNECTOR_PROBLEM:
      SetNoticeState(NOTICE_OPENING, CONNECTOR_PROBLEM_MESSAGE);
      break;
    case PAK_RESULT_CORRUPTED:
      SetNoticeState(NOTICE_OPENING, CORRUPTED_SAVE_MESSAGE);
      break;
    case PAK_RESULT_CARTRIDGE_REPLACED:
      SetNoticeState(NOTICE_OPENING, CARTRIDGE_REPLACED_MESSAGE);
      break;
    case PAK_RESULT_UNINITIALIZED:
      SetNoticeState(NOTICE_OPENING, UNINITIALIZED_SAVE_MESSAGE);
      break;
  }
}

/*
 * Runs the Send Status screen. After a pak check that tags the cartridges, the
 * player chooses one, and the status is written to it, after asking whether to
 * overwrite any status already there.
 */
void UpdateSendStatus(void) {
  /* The transfer pointer is there to match */
  GameBoyTransfer* transfer = &g_GameBoyTransfer;
  s32 result;

  switch (transfer->screenStep) {
    case SEND_STATUS_SHOWING_ERROR:
      switch (GetNoticeState()) {
        case NOTICE_SHOWN:
          transfer->isAccessing = FALSE;
          transfer->pressingHandPosition = PRESSING_HAND_FOR_NOTICE;
          if (f_TransmitController.pressedButtons & A_BUTTON) {
            transfer->pressingHandPosition = PRESSING_HAND_HIDDEN;
            SetNoticeState(NOTICE_CLOSING, PAK_CHECK_MESSAGE);
            PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, SOUND_FULL_SCALE);
          }

          break;
        case NOTICE_CLOSING:
          break;
        case NOTICE_HIDDEN:
          transfer->screen = TRANSMIT_SCREEN_OPTIONS;
          break;
      }

      break;
    case SEND_STATUS_STARTING:
      transfer->screenStep = SEND_STATUS_CONFIRMING;
      ResetSendStatus();
      SetNoticeState(NOTICE_OPENING, PAK_CHECK_MESSAGE);
      break;
    case SEND_STATUS_CONFIRMING:
      switch (GetNoticeState()) {
        case NOTICE_HIDDEN:
          transfer->pressingHandPosition = PRESSING_HAND_HIDDEN;
          if (!transfer->isCanceled) {
            transfer->isAccessing = TRUE;
            transfer->screenStep = SEND_STATUS_TAGGING;
          } else {
            transfer->isAccessing = FALSE;
            transfer->screen = TRANSMIT_SCREEN_OPTIONS;
          }

          break;
        case NOTICE_CLOSING:
          break;
        case NOTICE_SHOWN:
          transfer->pressingHandPosition = PRESSING_HAND_FOR_NOTICE;
          if (f_TransmitController.pressedButtons & A_BUTTON) {
            transfer->pressingHandPosition = PRESSING_HAND_HIDDEN;
            transfer->isCanceled = FALSE;
            SetNoticeState(NOTICE_CLOSING, PAK_CHECK_MESSAGE);
            PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, SOUND_FULL_SCALE);
          } else if (f_TransmitController.pressedButtons & B_BUTTON) {
            transfer->pressingHandPosition = PRESSING_HAND_HIDDEN;
            transfer->isCanceled = TRUE;
            SetNoticeState(NOTICE_CLOSING, PAK_CHECK_MESSAGE);
            PlaySound(SOUND_MENU_CANCEL, g_NextSoundChannel, SOUND_FULL_SCALE);
          }

          break;
      }

      break;
    case SEND_STATUS_TAGGING:
      result = TagCartridges();
      if (result == PAK_RESULT_OK) {
        transfer->isAccessing = FALSE;
        transfer->screenStep = SEND_STATUS_CHOOSING;
      } else {
        ShowTransferError(result);
        transfer->isAccessing = FALSE;
        transfer->screenStep = SEND_STATUS_SHOWING_ERROR;
      }

      break;
    case SEND_STATUS_CHOOSING:
      if (f_TransmitController.pressedButtons & A_BUTTON) {
        PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, SOUND_FULL_SCALE);
        transfer->screenStep = SEND_STATUS_READY;
      } else if (f_TransmitController.pressedButtons & B_BUTTON) {
        PlaySound(SOUND_MENU_CANCEL, g_NextSoundChannel, SOUND_FULL_SCALE);
        transfer->screen = TRANSMIT_SCREEN_OPTIONS;
      } else if ((f_TransmitController.repeatedButtons & L_JPAD) &&
          transfer->chosenCartridgeIndex > 0) {
        PlaySound(SOUND_MENU_MOVE, g_NextSoundChannel, SOUND_FULL_SCALE);
        transfer->chosenCartridgeIndex--;
      } else if ((f_TransmitController.repeatedButtons & R_JPAD) &&
          transfer->chosenCartridgeIndex < transfer->taggedCartridgeCount - 1) {
        PlaySound(SOUND_MENU_MOVE, g_NextSoundChannel, SOUND_FULL_SCALE);
        transfer->chosenCartridgeIndex++;
      }

      break;
    case SEND_STATUS_READY:
      transfer->pressingHandPosition = PRESSING_HAND_LOW;
      if (f_TransmitController.pressedButtons & A_BUTTON) {
        transfer->pressingHandPosition = PRESSING_HAND_HIDDEN;
        transfer->isAccessing = TRUE;
        PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, SOUND_FULL_SCALE);
        transfer->screenStep = SEND_STATUS_CHECKING;
      } else if (f_TransmitController.pressedButtons & B_BUTTON) {
        transfer->pressingHandPosition = PRESSING_HAND_HIDDEN;
        PlaySound(SOUND_MENU_CANCEL, g_NextSoundChannel, SOUND_FULL_SCALE);
        transfer->screenStep = SEND_STATUS_CHOOSING;
      }

      break;
    case SEND_STATUS_CHECKING:
      switch (CheckStatusCartridge(transfer->chosenCartridgeIndex)) {
        case STATUS_CHECK_NO_STATUS:
          transfer->isAccessing = FALSE;
          transfer->screenStep = SEND_STATUS_WRITING;
          break;
        case STATUS_CHECK_HAS_STATUS:
          transfer->isAccessing = FALSE;
          transfer->screenStep = SEND_STATUS_ASKING;
          OpenPrompt(PROMPT_OVERWRITE_RECORDS);
          break;
        case STATUS_CHECK_FAILED:
          transfer->isAccessing = FALSE;
          ShowTransferError(transfer->transferResult);
          transfer->screenStep = SEND_STATUS_SHOWING_ERROR;
          break;
      }

      break;
    case SEND_STATUS_ASKING:
      switch (UpdatePrompt(&f_TransmitController)) {
        case PROMPT_RESULT_YES:
          transfer->isAccessing = TRUE;
          transfer->screenStep = SEND_STATUS_WRITING;
          break;
        case PROMPT_RESULT_NO:
          transfer->screenStep = SEND_STATUS_CHOOSING;
          break;
      }

      break;
    case SEND_STATUS_WRITING:
      result = WriteStatusToCartridge(transfer->chosenCartridgeIndex);
      transfer->isAccessing = FALSE;
      switch (result) {
        case STATUS_WRITE_DONE:
        case STATUS_WRITE_FAILED:
          if (transfer->transferResult == PAK_RESULT_OK) {
            transfer->screenStep = SEND_STATUS_SENT;
          } else {
            ShowTransferError(transfer->transferResult);
            transfer->screenStep = SEND_STATUS_SHOWING_ERROR;
          }

          break;
        case STATUS_WRITE_PAK_ERROR:
          ShowPakError(transfer->transferResult);
          transfer->screenStep = SEND_STATUS_SHOWING_ERROR;
          break;
      }

      break;
    case SEND_STATUS_SENT:
      transfer->pressingHandPosition = PRESSING_HAND_LOW;
      if (f_TransmitController.pressedButtons & A_BUTTON) {
        PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, SOUND_FULL_SCALE);
        transfer->pressingHandPosition = PRESSING_HAND_HIDDEN;
        transfer->screen = TRANSMIT_SCREEN_OPTIONS;
      }

      break;
  }
}

/*
 * Lets the player choose a character whose experience to send, asks them to
 * start a pak check, then writes the experience to the cartridge and shows the
 * result.
 */
void UpdateSendExperience(void) {
  /* The transfer pointer is there to match */
  GameBoyTransfer* transfer = &g_GameBoyTransfer;

  switch (transfer->screenStep) {
    case SEND_EXPERIENCE_DONE:
      transfer->screenStep = SEND_EXPERIENCE_CHOOSING;
      ResetSendExperience();
      break;
    case SEND_EXPERIENCE_NOTHING_TO_SEND:
      switch (GetNoticeState()) {
        case NOTICE_HIDDEN:
          transfer->screenStep = SEND_EXPERIENCE_CHOOSING;
          break;
        case NOTICE_CLOSING:
          break;
        case NOTICE_SHOWN:
          transfer->pressingHandPosition = PRESSING_HAND_FOR_NOTICE;
          if (f_TransmitController.pressedButtons & A_BUTTON) {
            transfer->pressingHandPosition = PRESSING_HAND_HIDDEN;
            SetNoticeState(NOTICE_CLOSING, PAK_CHECK_MESSAGE);
            PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, SOUND_FULL_SCALE);
          }

          break;
      }

      break;
    case SEND_EXPERIENCE_CHOOSING:
      if (f_TransmitController.pressedButtons & A_BUTTON) {
        if (g_GameBoyCharacters[transfer->characterIndex].experience == 0) {
          transfer->screenStep = SEND_EXPERIENCE_NOTHING_TO_SEND;
          SetNoticeState(NOTICE_OPENING, NO_EXPERIENCE_MESSAGE);
          PlaySound(SOUND_MENU_BLOCKED, g_NextSoundChannel, SOUND_FULL_SCALE);
        } else {
          transfer->screenStep = SEND_EXPERIENCE_CONFIRMING;
          SetNoticeState(NOTICE_OPENING, PAK_CHECK_MESSAGE);
          PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, SOUND_FULL_SCALE);
        }
      } else if (f_TransmitController.pressedButtons & B_BUTTON) {
        PlaySound(SOUND_MENU_CANCEL, g_NextSoundChannel, SOUND_FULL_SCALE);
        transfer->screen = TRANSMIT_SCREEN_OPTIONS;
      } else if ((f_TransmitController.repeatedButtons & L_JPAD) &&
          transfer->characterIndex > 0) {
        transfer->characterIndex--;
        PlaySound(SOUND_MENU_MOVE, g_NextSoundChannel, SOUND_FULL_SCALE);
      } else if ((f_TransmitController.repeatedButtons & R_JPAD) &&
          transfer->characterIndex < transfer->characterCount - 1) {
        transfer->characterIndex++;
        PlaySound(SOUND_MENU_MOVE, g_NextSoundChannel, SOUND_FULL_SCALE);
      }

      break;
    case SEND_EXPERIENCE_CONFIRMING:
      switch (GetNoticeState()) {
        case NOTICE_HIDDEN:
          transfer->pressingHandPosition = PRESSING_HAND_HIDDEN;
          if (!transfer->isCanceled) {
            transfer->isAccessing = TRUE;
            transfer->screenStep = SEND_EXPERIENCE_WRITING;
          } else {
            transfer->isAccessing = FALSE;
            transfer->screen = TRANSMIT_SCREEN_OPTIONS;
          }

          break;
        case NOTICE_CLOSING:
          break;
        case NOTICE_SHOWN:
          transfer->pressingHandPosition = PRESSING_HAND_FOR_NOTICE;
          if (f_TransmitController.pressedButtons & A_BUTTON) {
            transfer->pressingHandPosition = PRESSING_HAND_HIDDEN;
            transfer->isCanceled = FALSE;
            SetNoticeState(NOTICE_CLOSING, PAK_CHECK_MESSAGE);
            PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, SOUND_FULL_SCALE);
          } else if (f_TransmitController.pressedButtons & B_BUTTON) {
            transfer->pressingHandPosition = PRESSING_HAND_HIDDEN;
            transfer->isCanceled = TRUE;
            SetNoticeState(NOTICE_CLOSING, PAK_CHECK_MESSAGE);
            PlaySound(SOUND_MENU_CANCEL, g_NextSoundChannel, SOUND_FULL_SCALE);
          }

          break;
      }

      break;
    case SEND_EXPERIENCE_WRITING:
      transfer->isAccessing = FALSE;
      switch (WriteExperienceToCartridge(transfer->characterIndex)) {
        case EXPERIENCE_WRITE_DONE:
          transfer->screenStep = SEND_EXPERIENCE_SENT;
          break;
        case EXPERIENCE_WRITE_NOT_FROM_CARTRIDGE:
          SetNoticeState(NOTICE_OPENING, NOT_FROM_CARTRIDGE_MESSAGE);
          transfer->screenStep = SEND_EXPERIENCE_FAILED;
          break;
        case EXPERIENCE_WRITE_FAILED:
        default:
          ShowTransferError(transfer->transferResult);
          transfer->screenStep = SEND_EXPERIENCE_FAILED;
          break;
        case EXPERIENCE_WRITE_PAK_ERROR:
          ShowPakError(transfer->transferResult);
          transfer->screenStep = SEND_EXPERIENCE_FAILED;
          break;
      }

      break;
    case SEND_EXPERIENCE_SENT:
      transfer->pressingHandPosition = PRESSING_HAND_LOW;
      if (f_TransmitController.pressedButtons & A_BUTTON) {
        transfer->pressingHandPosition = PRESSING_HAND_HIDDEN;
        PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, SOUND_FULL_SCALE);
        transfer->screenStep = SEND_EXPERIENCE_DONE;
      }

      break;
    case 5: /* Steps 5 to 7 end like a failure */
    case 6:
    case 7:
    case SEND_EXPERIENCE_FAILED:
      switch (GetNoticeState()) {
        case NOTICE_HIDDEN:
          transfer->screenStep = SEND_EXPERIENCE_DONE;
          transfer->screen = TRANSMIT_SCREEN_OPTIONS;
          break;
        case NOTICE_CLOSING:
          break;
        case NOTICE_SHOWN:
          transfer->pressingHandPosition = PRESSING_HAND_FOR_NOTICE;
          if (f_TransmitController.pressedButtons & A_BUTTON) {
            transfer->pressingHandPosition = PRESSING_HAND_HIDDEN;
            SetNoticeState(NOTICE_CLOSING, PAK_CHECK_MESSAGE);
            PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, SOUND_FULL_SCALE);
          }

          break;
      }

      break;
  }
}

/* Whether every playing controller holds a Rumble Pak */
s32 HaveAllRumblePaks(void) {
  s32 haveAll = TRUE;
  s32 index;

  for (index = 0; index != g_ControllerCount; index++) {
    if (nuContRmbCheck(g_PlayerControllerPorts[index]) != 0) {
      haveAll = FALSE;
    }
  }

  return haveAll;
}

/* A macro rather than an inline function, which leaves a store-flag result */
#define AreAllCButtonsHeld(buttons) \
  ((((buttons) & (L_CBUTTONS | D_CBUTTONS | U_CBUTTONS)) == \
       (L_CBUTTONS | D_CBUTTONS | U_CBUTTONS)) && \
      ((buttons) & R_CBUTTONS))

/*
 * Runs the options screen and hands each frame to the screen chosen there, then
 * leaves for the title screen or the main menu, asking for any missing Rumble
 * Pak first.
 */
void UpdateTransmitMenu(void) {
  /* The transfer pointer is there to match */
  GameBoyTransfer* transfer = &g_GameBoyTransfer;
  s32 controller;

  f_TransmitController.repeatedButtons = 0;
  f_TransmitController.buttons = 0;
  f_TransmitController.pressedButtons = 0;
  for (controller = 0; controller != g_ControllerCount; controller++) {
    f_TransmitController.repeatedButtons |=
        g_ControllerStates[controller].repeatedButtons;
    f_TransmitController.buttons |= g_ControllerStates[controller].buttons;
    f_TransmitController.pressedButtons |=
        g_ControllerStates[controller].pressedButtons;
  }

  if (IsGameFlagSet(GAME_FLAG_ALLOW_CARTRIDGE_DUMP) &&
      AreAllCButtonsHeld(g_ControllerStates[0].buttons)) {
    DumpCartridgeRam();
  }

  switch (transfer->screen) {
    case TRANSMIT_SCREEN_OPTIONS:
      if (GetWipeProgress() != 20) {
        break;
      }

      if (f_TransmitController.pressedButtons & A_BUTTON) {
        PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, SOUND_FULL_SCALE);
        if (transfer->optionIndex == 0) {
          transfer->screen = TRANSMIT_SCREEN_SEND_STATUS;
          transfer->screenStep = SEND_STATUS_STARTING;
          ResetSendStatus();
        } else if (transfer->optionIndex == 1) {
          if (transfer->transferMode == TRANSFER_MODE_AT_BOOT) {
            transfer->screen = TRANSMIT_SCREEN_BRING_CHARACTERS;
            transfer->screenStep = BRING_STARTING;
            ClearCartridgeCharacters();
          } else {
            transfer->screen = TRANSMIT_SCREEN_SEND_EXPERIENCE;
            transfer->screenStep = SEND_EXPERIENCE_DONE;
            ResetSendExperience();
          }
        }

        if (transfer->optionIndex == 2) {
          if (!HaveAllRumblePaks()) {
            transfer->isCanceled = FALSE;
            transfer->screen = TRANSMIT_SCREEN_RUMBLE_PAK_PROMPT;
            SetNoticeState(NOTICE_OPENING, INSERT_RUMBLE_PAK_MESSAGE);
          } else {
            transfer->screen = TRANSMIT_SCREEN_LEAVING;
            StartWipe(WIPE_CLOSING);
          }
        }
      } else if (f_TransmitController.pressedButtons & B_BUTTON) {
        if (transfer->transferMode == TRANSFER_MODE_FROM_MENU) {
          if (!HaveAllRumblePaks()) {
            transfer->isCanceled = TRUE;
            transfer->screen = TRANSMIT_SCREEN_RUMBLE_PAK_PROMPT;
            SetNoticeState(NOTICE_OPENING, INSERT_RUMBLE_PAK_MESSAGE);
          } else {
            transfer->screen = TRANSMIT_SCREEN_LEAVING;
            StartWipe(WIPE_CLOSING);
          }
        }
      } else if (f_TransmitController.repeatedButtons & U_JPAD) {
        if (transfer->transferMode == TRANSFER_MODE_AT_BOOT) {
          transfer->optionIndex = (transfer->optionIndex + 2) % 3;
        } else {
          transfer->optionIndex ^= 1;
        }

        PlaySound(SOUND_MENU_MOVE, g_NextSoundChannel, SOUND_FULL_SCALE);
      } else if (f_TransmitController.repeatedButtons & D_JPAD) {
        if (transfer->transferMode == TRANSFER_MODE_AT_BOOT) {
          transfer->optionIndex = (transfer->optionIndex + 4) % 3;
        } else {
          transfer->optionIndex ^= 1;
        }

        PlaySound(SOUND_MENU_MOVE, g_NextSoundChannel, SOUND_FULL_SCALE);
      }

      break;
    case TRANSMIT_SCREEN_BRING_CHARACTERS:
      UpdateBringCharacters();
      break;
    case TRANSMIT_SCREEN_SEND_STATUS:
      UpdateSendStatus();
      break;
    case TRANSMIT_SCREEN_SEND_EXPERIENCE:
      UpdateSendExperience();
      break;
    case TRANSMIT_SCREEN_RUMBLE_PAK_PROMPT:
      switch (GetNoticeState()) {
        case NOTICE_HIDDEN:
          transfer->pressingHandPosition = PRESSING_HAND_HIDDEN;
          if (!transfer->isCanceled) {
            transfer->screen = TRANSMIT_SCREEN_LEAVING;
          } else {
            transfer->screen = TRANSMIT_SCREEN_UNK6;
          }

          StartWipe(WIPE_CLOSING);
          break;
        case NOTICE_CLOSING:
          break;
        case NOTICE_SHOWN:
          transfer->pressingHandPosition = PRESSING_HAND_FOR_NOTICE;
          if (f_TransmitController.pressedButtons & A_BUTTON) {
            transfer->pressingHandPosition = PRESSING_HAND_HIDDEN;
            SetNoticeState(NOTICE_CLOSING, PAK_CHECK_MESSAGE);
            PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, SOUND_FULL_SCALE);
          }

          break;
      }

      break;
    case TRANSMIT_SCREEN_LEAVING:
      if (GetWipeProgress() == 0) {
        StartWipe(WIPE_OPENING);
        g_Scene = SCENE_SWAPPING_MODULES;
        g_SceneModuleToUnload = MODULE_TRANSMIT;
        g_NextScene = SCENE_TITLE_SCREEN;
        g_SceneModuleToLoad = MODULE_TITLE_SCREEN;
      }

      break;
    case TRANSMIT_SCREEN_UNK6:
      if (GetWipeProgress() == 0) {
        StartWipe(WIPE_OPENING);
        func_8005DC50();
        g_Scene = SCENE_SWAPPING_MODULES;
        g_SceneModuleToUnload = MODULE_TRANSMIT;
        g_NextScene = SCENE_MENU;
        g_SceneModuleToLoad = MODULE_MENU;
      }

      break;
  }
}

/* Fills the slot for a cartridge's save from the save's bytes */
void DecodeCartridgeCharacter(
    u8* save, s32 cartridgeIndex, s32 saveIndex, s32 unk34) {
  GameBoyCharacter* character =
      &g_GameBoyTransfer
           .cartridgeCharacters[(cartridgeIndex * CARTRIDGE_SAVE_COUNT) +
               saveIndex];
  s32 index;

  character->status = GAME_BOY_CHARACTER_STATUS_PRESENT;
  for (index = 0; index != (SAVE_NAME_SIZE - 1); index++) {
    character->name[index] = save[index];
    character->saveName[index] = save[index];
    if (character->name[index] >= 'a' && character->name[index] <= 'z') {
      character->name[index] += LOWERCASE_CODE_OFFSET;
    }
  }

  character->name[(SAVE_NAME_SIZE - 1)] = '\0';
  character->saveName[(SAVE_NAME_SIZE - 1)] = '\0';
  for (index = 0; index != SAVE_UNKB_SIZE; index++) {
    character->saveUnkB[index] = save[SAVE_UNKB_OFFSET + index];
  }

  character->character = save[SAVE_CHARACTER_OFFSET];
  if (character->character >= GAME_BOY_GOLFER_COUNT) {
    character->character = GAME_BOY_GOLFER_COUNT - 1;
  }

  if (IsGameFlagSet(GAME_FLAG_UNK6F)) {
    character->unk1A = 1;
  } else {
    character->unk1A = save[0x11];
  }

  character->level = save[0x12];
  character->unk1C = save[0x1C];
  character->shotCurve = save[0x1D];
  character->unk1E = save[0x1E];
  character->mishitResistance = save[0x1F];
  if (IsGameFlagSet(GAME_FLAG_BALL_TYPE_1)) {
    character->ballType = 1;
  } else if (IsGameFlagSet(GAME_FLAG_BALL_TYPE_2)) {
    character->ballType = 2;
  } else if (IsGameFlagSet(GAME_FLAG_BALL_TYPE_3)) {
    character->ballType = 3;
  } else if (IsGameFlagSet(GAME_FLAG_BALL_TYPE_4)) {
    character->ballType = 4;
  } else if (IsGameFlagSet(GAME_FLAG_BALL_TYPE_5)) {
    character->ballType = 5;
  } else if (IsGameFlagSet(GAME_FLAG_BALL_TYPE_6)) {
    character->ballType = 6;
  } else if (IsGameFlagSet(GAME_FLAG_BALL_TYPE_7)) {
    character->ballType = 7;
  } else {
    character->ballType = save[0x3E];
  }

  osSyncPrintf("balltype %d\n", character->ballType);
  character->unk24 = (save[0x28] << 16) | (save[0x27] << 8) | save[0x26];
  character->experience = 0;
  character->driveDistance = save[0x1A] | (save[0x1B] << 8);
  osSyncPrintf("%d, %d %d\n", character->experience, character->unk1C,
      character->shotCurve);
  for (index = 0; index != GAME_BOY_CHARACTER_UNK3C_COUNT; index++) {
    character->unk3C[index].unk0 =
        save[0x100 + (index * 6)] | (save[0x101 + (index * 6)] << 8);
    character->unk3C[index].unk2 =
        save[0x102 + (index * 6)] | (save[0x103 + (index * 6)] << 8);
    character->unk3C[index].unk4 = save[0x104 + (index * 6)];
    character->unk3C[index].unk5 = save[0x105 + (index * 6)];
    osSyncPrintf("%d, %d %d\n", index, character->unk3C[index].unk0,
        character->unk3C[index].unk2);
  }

  character->unk34 = unk34;
  character->unk38 = saveIndex;
}

void SkipCartridgeSave(u8* unusedData, s32 unusedCartridgeIndex,
    s32 unusedSaveIndex, u8* unusedTitle, s32 unusedUnk34) {}

Boolean IsCharacterFromSave(GameBoyCharacter* character, u8* save) {
  s32 index;

  osSyncPrintf("PHASE a\n");
  for (index = 0; index != SAVE_NAME_SIZE; index++) {
    if (character->saveName[index] != save[index]) {
      if (FALSE) {
        osSyncPrintf("PHASE b\n");
      }

      return FALSE;
    }
  }

  osSyncPrintf("PHASE c\n");
  for (index = 0; index != SAVE_UNKB_SIZE; index++) {
    if (character->saveUnkB[index] != save[SAVE_UNKB_OFFSET + index]) {
      osSyncPrintf("PHASE d\n");
      return FALSE;
    }
  }

  osSyncPrintf("PHASE e\n");
  return TRUE;
}

/* The menu's options, each the offset of its name from TRANSMIT_OPTION_MESSAGES
 */
static s32 f_ModeOptions[TRANSFER_MODE_COUNT][MODE_OPTION_COUNT] = {
    {TRANSMIT_OPTION_STATUS, TRANSMIT_OPTION_CHARACTERS,
        TRANSMIT_OPTION_MAIN_MENU},
    {TRANSMIT_OPTION_STATUS, TRANSMIT_OPTION_EXPERIENCE, TRANSMIT_OPTION_NONE},
};
static char* f_UnusedShotTypeNames[] = {"Straight", "Draw", "Fade"};
static char f_UnusedDriveName[8] = "Drive";
static Radians f_StatusPointerAngle = 0.0f;
static s32 f_CharacterArrowFrame = 0;
static Radians f_OptionPointerAngle = 0.0f;
/* The title in the Game Boy cartridge header, then its game code */
static char f_GameBoyCartridgeTitle[16] = "MARIO GOLF\0AWXE";
/* Begins every save in the Game Boy cartridge RAM */
static char f_CartridgeSaveSignature[16] = "CAMELOT GAMEBOY";
static s32 f_NoticeState = NOTICE_HIDDEN;
static MessageId f_NoticeMessage = 0;
static s32 f_NoticeSize = 0;
static s32 f_PressingHandFrame = 0;
static s32 f_UnusedZeros[2] = {0, 0};
static s32 f_StatusHoleOrder[COURSE_HOLE_COUNT] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17};

void BringOverCharacter(s32 listedIndex, s32 slot) {
  /* The transfer pointer is there to match */
  GameBoyTransfer* transfer = &g_GameBoyTransfer;

  g_GameBoyCharacters[slot] =
      transfer->cartridgeCharacters[transfer->listedCharacters[listedIndex]];
  g_GameBoyCharacters[slot].status = GAME_BOY_CHARACTER_STATUS_PRESENT;
  transfer->cartridgeCharacters[transfer->listedCharacters[listedIndex]]
      .status = GAME_BOY_CHARACTER_STATUS_BROUGHT_OVER;
  transfer->sourceIndices[slot] = transfer->listedCharacters[listedIndex];
}

void ResetBringCharacters(void) {
  g_GameBoyTransfer.listedCharacterIndex = 0;
  g_GameBoyTransfer.characterIndex = 0;
  g_GameBoyTransfer.unk7B8 = 0;
  g_GameBoyTransfer.isCanceled = FALSE;
}

void ResetSendStatus(void) {
  g_GameBoyTransfer.chosenCartridgeIndex = 0;
  g_GameBoyTransfer.taggedCartridgeCount = 0;
  g_GameBoyTransfer.isCanceled = FALSE;
}

void ResetSendExperience(void) {
  g_GameBoyTransfer.chosenCartridgeIndex = 0;
  g_GameBoyTransfer.characterIndex = 0;
  g_GameBoyTransfer.taggedCartridgeCount = 0;
  g_GameBoyTransfer.isCanceled = FALSE;
}

s32 GetStoredChecksum(u8* save) {
  return save[SAVE_CHECKSUM_OFFSET] | (save[SAVE_CHECKSUM_OFFSET + 1] << 8);
}

/* Sums the save's bytes after its checksum */
s32 ComputeSaveChecksum(u8* save) {
  s32 sum = 0;
  s32 offset;

  for (offset = SAVE_CHECKSUM_OFFSET + 2; offset != CARTRIDGE_SAVE_SIZE;
      offset++) {
    sum += save[offset];
  }

  return sum;
}

void ListPresentCharacters(void) {
  /* The transfer pointer is there to match */
  GameBoyTransfer* transfer = &g_GameBoyTransfer;
  s32 listedCount = 0;
  s32 index;

  for (index = 0; index != CARTRIDGE_CHARACTER_COUNT; index++) {
    if (transfer->cartridgeCharacters[index].status ==
        GAME_BOY_CHARACTER_STATUS_PRESENT) {
      transfer->listedCharacters[listedCount++] = index;
    }
  }

  for (; listedCount != CARTRIDGE_CHARACTER_COUNT; listedCount++) {
    transfer->listedCharacters[listedCount] = -1;
  }
}

void DrawSendStatusHint(Gfx** displayListHead, GameBoyTransfer* transfer) {
  Gfx* displayList = *displayListHead;
  MessageId message = NO_MESSAGE;

  switch (transfer->screenStep) {
    case SEND_STATUS_CHOOSING:
      message = CHOOSE_CARTRIDGE_MESSAGE;
      break;
    case SEND_STATUS_SENT:
      message = SEND_STATUS_MESSAGE;
      break;
    case SEND_STATUS_READY:
      message = KEEP_POWER_ON_MESSAGE;
      break;
  }

  if (message != NO_MESSAGE) {
    SetTextSlot(HINT_LEFT, HINT_TOP, message, TEXT_SLOT_LARGE_FONT,
        HINT_TEXT_SLOT, TEXT_FLAG_UNK20, 255, 255, 255, 255);
    DrawLargeTextSlot(&displayList, HINT_TEXT_SLOT);
  }

  *displayListHead = displayList;
}

/*
 * Draws a Game Boy character's name, level and drive, the name of their shot
 * type and a bar from its center out to their shot curve.
 */
void DrawCharacterStats(Gfx** displayListHead, GameBoyCharacter* character,
    GameBoyTransfer* transfer) {
  Gfx* displayList = *displayListHead;
  s32 segment;

  /* Holding the shot type row in segmentCurve first is there to match */
  s32 segmentCurve;

  SetUpTextDrawing(&displayList);
  DrawLargeText(&displayList, STATS_NAME_LEFT, STATS_NAME_TOP, character->name);
  BeginShadowedText(&displayList);
  sprintf(g_PrintBuffer, "  %2d     :%3d", character->level,
      character->driveDistance);
  DrawSmallText(&displayList, STATS_LEFT, STATS_TOP, g_PrintBuffer);
  if (character->shotCurve == 0) {
    segmentCurve = SHOT_TYPE_STRAIGHT;
  } else if (character->shotCurve > 0) {
    segmentCurve = SHOT_TYPE_DRAW;
  } else {
    segmentCurve = SHOT_TYPE_FADE;
  }

  SetTextSlot(HINT_LEFT, HINT_TOP, CHOOSE_CHARACTER_MESSAGE,
      TEXT_SLOT_LARGE_FONT, HINT_TEXT_SLOT, TEXT_FLAG_UNK20, 255, 255, 255,
      255);
  gDPPipeSync(displayList++);
  gDPSetAlphaCompare(displayList++, G_AC_NONE);
  gDPPipeSync(displayList++);
  gDPSetRenderMode(
      displayList++, G_RM_TEX_EDGE | AA_EN, G_RM_TEX_EDGE2 | AA_EN);
  gDPSetCombineMode(displayList++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPPipeSync(displayList++);
  gDPSetTextureLUT(displayList++, G_TT_RGBA16);
  LoadSmallPalette(&displayList, 0, ImageBody(transfer->shotTypesImage));
  LoadNibbleTextureBlock(&displayList, ImageTexels(transfer->shotTypesImage),
      G_IM_FMT_CI, SHOT_TYPE_WIDTH, SHOT_TYPE_HEIGHT * 3, 0, 0, 0, 0, 0, 0, 0);
  DrawTextureRect(&displayList, SHOT_TYPE_LEFT << 2, SHOT_TYPE_TOP << 2,
      (SHOT_TYPE_LEFT + SHOT_TYPE_WIDTH) << 2,
      (SHOT_TYPE_TOP + SHOT_TYPE_HEIGHT) << 2, G_TX_RENDERTILE, 0,
      (segmentCurve * SHOT_TYPE_HEIGHT) << 5, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
  LoadSmallPalette(&displayList, 0, ImageBody(transfer->statLabelsImage));
  LoadNibbleTextureBlock(&displayList, ImageTexels(transfer->statLabelsImage),
      G_IM_FMT_CI, STAT_LABELS_WIDTH, STAT_LABELS_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
  DrawTextureRect(&displayList, STATS_LEFT << 2, STATS_TOP << 2,
      (STATS_LEFT + STAT_LABELS_WIDTH) << 2,
      (STATS_TOP + STAT_LABELS_HEIGHT) << 2, G_TX_RENDERTILE, 0, 0,
      TEXEL_STEP_ONE, TEXEL_STEP_ONE);
  LoadSmallPalette(&displayList, 0, ImageBody(transfer->statBarsImage));
  gDPLoadTextureTile_4b(displayList++,
      TextureAddress(ImageTexels(transfer->statBarsImage)), G_IM_FMT_CI,
      CURVE_BAR_WIDTH, 0, 0, 0, CURVE_BAR_WIDTH - 1, CURVE_BAR_HEIGHT - 1, 0, 0,
      0, 0, 0, 0, 0);
  DrawTextureRect(&displayList, CURVE_BAR_LEFT << 2, CURVE_BAR_TOP << 2,
      (CURVE_BAR_LEFT + CURVE_BAR_WIDTH) << 2,
      (CURVE_BAR_TOP + CURVE_BAR_HEIGHT) << 2, G_TX_RENDERTILE, 0, 0,
      TEXEL_STEP_ONE, TEXEL_STEP_ONE);
  LoadLargePalette(&displayList, ImageBody(transfer->curveSegmentsImage));
  LoadTextureBlock(&displayList, LargeImageTexels(transfer->curveSegmentsImage),
      G_IM_FMT_CI, G_IM_SIZ_8b, CURVE_SEGMENT_WIDTH,
      CURVE_SEGMENT_WIDTH * CURVE_SEGMENT_COUNT, 0, 0, 0, 0, 0, 0, 0);
  for (segment = 0, segmentCurve = -CURVE_SEGMENT_COUNT / 2;
      segment != CURVE_SEGMENT_COUNT; segment++, segmentCurve++) {
    /* Light segments from the center out to the shot curve; the continues are
     * there to match */
    if (character->shotCurve < 0) {
      if (segmentCurve < character->shotCurve) {
        continue;
      }

      if (segmentCurve > 0) {
        continue;
      }
    }

    if (character->shotCurve > 0) {
      if (segmentCurve > character->shotCurve) {
        continue;
      }

      if (segmentCurve < 0) {
        continue;
      }
    }

    if (character->shotCurve == 0 && segment != CURVE_SEGMENT_COUNT / 2) {
      continue;
    }

    DrawTextureRect(&displayList,
        (CURVE_BAR_LEFT + (segment * CURVE_SEGMENT_WIDTH)) << 2,
        CURVE_BAR_TOP << 2,
        (CURVE_BAR_LEFT + ((segment + 1) * CURVE_SEGMENT_WIDTH)) << 2,
        (CURVE_BAR_TOP + CURVE_BAR_HEIGHT) << 2, G_TX_RENDERTILE, 0,
        (segment * CURVE_SEGMENT_WIDTH) << 5, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
  }

  *displayListHead = displayList;
}

void DrawBringCharactersText(Gfx** displayListHead, GameBoyTransfer* transfer) {
  GameBoyCharacter* character = g_GameBoyCharacters;
  Gfx* displayList = *displayListHead;
  Boolean isCharacterShown = FALSE;
  MessageId message;

  SetUpTextDrawing(&displayList);
  gDPSetPrimColor(displayList++, 0, 0, 255, 255, 255, 255);
  sprintf(g_PrintBuffer, "%3d", transfer->screenStep);
  PrintDebugText(g_PrintBuffer, 20, 2);
  if (transfer->screenStep == BRING_CHOOSING_SLOT) {
    isCharacterShown = TRUE;
    character = &g_GameBoyCharacters[transfer->characterIndex];
  }

  if ((transfer->screenStep == BRING_CHOOSING ||
          transfer->screenStep == BRING_BRINGING) &&
      g_GameBoyTransfer.listedCharacterCount > 0) {
    isCharacterShown = TRUE;
    character = &transfer->cartridgeCharacters[transfer
            ->listedCharacters[transfer->listedCharacterIndex]];
  }

  if (isCharacterShown) {
    DrawCharacterStats(&displayList, character, transfer);
  }

  SetUpTextDrawing(&displayList);
  if (transfer->unk7A0 == (g_ControllerCount * 4) - 1) {
    message = NO_MESSAGE;
    switch (transfer->screenStep) {
      case BRING_CHOOSING:
        message = CHOOSE_CHARACTER_MESSAGE;
        break;
      case BRING_CHOOSING_SLOT:
        message = CHOOSE_SWAPPED_CHARACTER_MESSAGE;
        break;
      case BRING_BROUGHT:
        message = CHARACTER_BROUGHT_MESSAGE;
        break;
    }

    if (message != NO_MESSAGE) {
      SetTextSlot(HINT_LEFT, HINT_TOP, message, TEXT_SLOT_LARGE_FONT,
          HINT_TEXT_SLOT, TEXT_FLAG_UNK20, 255, 255, 255, 255);
      DrawLargeTextSlot(&displayList, HINT_TEXT_SLOT);
    }
  }

  *displayListHead = displayList;
}

void DrawSendExperienceText(Gfx** displayListHead, GameBoyTransfer* transfer) {
  Gfx* displayList = *displayListHead;
  s32 slot;
  MessageId message;

  gDPSetPrimColor(displayList++, 0, 0, 255, 255, 255, 255);
  BeginShadowedText(&displayList);
  for (slot = 0; slot != g_GameBoyTransfer.characterCount; slot++) {
    GameBoyCharacter* character = &g_GameBoyCharacters[slot];

    sprintf(g_PrintBuffer, "%4d", character->experience);
    DrawSmallText(&displayList, EXPERIENCE_LEFT + (slot * FACE_SPACING),
        EXPERIENCE_TOP, g_PrintBuffer);
  }

  DrawCharacterStats(
      &displayList, &g_GameBoyCharacters[transfer->characterIndex], transfer);
  SetUpTextDrawing(&displayList);
  message = NO_MESSAGE;
  switch (transfer->screenStep) {
    case SEND_EXPERIENCE_CHOOSING:
      message = CHOOSE_EXPERIENCE_MESSAGE;
      break;
    case SEND_EXPERIENCE_SENT:
      message = EXPERIENCE_SENT_MESSAGE;
      break;
  }

  if (message != NO_MESSAGE) {
    SetTextSlot(HINT_LEFT, HINT_TOP, message, TEXT_SLOT_LARGE_FONT,
        HINT_TEXT_SLOT, TEXT_FLAG_UNK20, 255, 255, 255, 255);
    DrawLargeTextSlot(&displayList, HINT_TEXT_SLOT);
  }

  *displayListHead = displayList;
}

/*
 * Draws the Send Status screen: the imported faces and the icons of the tagged
 * controllers, dimming the others once a cartridge is chosen. A swinging
 * pointer marks the cartridge while choosing and the cursor box once it is
 * chosen.
 */
void DrawSendStatusScreen(Gfx** displayListHead, GameBoyTransfer* transfer) {
  Gfx* displayList = *displayListHead;
  s32 index;
  s32 slot;
  s32 pointerRestLeft;
  s32 pointerSwing;

  DrawImportedFaces(&displayList, transfer);
  gDPPipeSync(displayList++);
  gDPSetTextureLUT(displayList++, G_TT_RGBA16);
  gDPSetCombineMode(
      displayList++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
  LoadSmallPalette(&displayList, 0, ImageBody(transfer->controllerIconsImage));
  LoadNibbleTextureBlock(&displayList,
      ImageTexels(transfer->controllerIconsImage), G_IM_FMT_CI,
      CONTROLLER_ICON_SIZE, CONTROLLER_ICON_SIZE * MAXCONTROLLERS, 0, 0, 0, 0,
      0, 0, 0);
  slot = 0;
  gDPSetPrimColor(displayList++, 0, 0, 255, 255, 255, 255);
  for (index = 0; index != transfer->taggedCartridgeCount; index++) {
    if (transfer->screenStep == SEND_STATUS_READY &&
        slot != transfer->chosenCartridgeIndex) {
      gDPSetPrimColor(displayList++, 0, 0, 127, 127, 127, 255);
    } else {
      gDPSetPrimColor(displayList++, 0, 0, 255, 255, 255, 255);
    }

    DrawTextureRect(&displayList,
        (CONTROLLER_ICONS_LEFT + (slot * CONTROLLER_ICON_SPACING)) << 2,
        CONTROLLER_ICONS_TOP << 2,
        (CONTROLLER_ICONS_LEFT + (slot * CONTROLLER_ICON_SPACING) +
            CONTROLLER_ICON_SIZE)
            << 2,
        (CONTROLLER_ICONS_TOP + CONTROLLER_ICON_SIZE) << 2, G_TX_RENDERTILE, 0,
        (transfer->taggedControllers[index] * CONTROLLER_ICON_SIZE) << 5,
        TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    slot++;
  }

  gDPSetPrimColor(displayList++, 0, 0, 255, 255, 255, 255);
  if (transfer->taggedCartridgeCount != 0) {
    /* An if-chain rather than a switch is there to match */
    if (transfer->screenStep == SEND_STATUS_CHOOSING) {
      /* The pointer swings in and out over half a turn of the angle */
      f_StatusPointerAngle += DegreesToRadians(10);
      if (f_StatusPointerAngle > M_PI) {
        f_StatusPointerAngle -= M_PI;
      }

      pointerSwing = sinf(f_StatusPointerAngle) * 8.0f;
      gDPPipeSync(displayList++);
      gDPSetTextureLUT(displayList++, G_TT_RGBA16);
      LoadSmallPalette(&displayList, 0, ImageBody(transfer->pointerImage));
      /* The original loads the controller icons again, which the pointer
       * replaces */
      LoadNibbleTextureBlock(&displayList,
          ImageTexels(transfer->controllerIconsImage), G_IM_FMT_CI,
          CONTROLLER_ICON_SIZE, CONTROLLER_ICON_SIZE * MAXCONTROLLERS, 0, 0, 0,
          0, 0, 0, 0);
      gDPLoadTextureTile_4b(displayList++,
          TextureAddress(ImageTexels(transfer->pointerImage)), G_IM_FMT_CI,
          POINTER_WIDTH, 0, 0, 0, POINTER_WIDTH - 1, POINTER_IMAGE_HEIGHT - 1,
          0, 0, 0, 0, 0, 0, 0);
      /* Subtracting the swing in each coordinate is there to match */
      pointerRestLeft =
          (transfer->chosenCartridgeIndex * CONTROLLER_ICON_SPACING) +
          (CONTROLLER_ICONS_LEFT - POINTER_WIDTH);
      DrawTextureRect(&displayList, (pointerRestLeft - pointerSwing) << 2,
          POINTER_TOP << 2,
          (pointerRestLeft - pointerSwing + POINTER_WIDTH) << 2,
          (POINTER_TOP + POINTER_HEIGHT) << 2, G_TX_RENDERTILE, 0, 0,
          TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    } else if (transfer->screenStep == SEND_STATUS_READY) {
      DrawCursorBox(&displayList,
          CONTROLLER_ICONS_LEFT - CONTROLLER_CURSOR_MARGIN +
              (transfer->chosenCartridgeIndex * CONTROLLER_ICON_SPACING),
          CONTROLLER_ICONS_TOP - CONTROLLER_CURSOR_MARGIN);
    }
  }

  *displayListHead = displayList;
}

void DrawImportedFaces(Gfx** displayListHead, GameBoyTransfer* transfer) {
  Gfx* displayList = *displayListHead;
  s32 slot;
  s32 character;

  gDPPipeSync(displayList++);
  gDPSetTextureLUT(displayList++, G_TT_RGBA16);
  for (slot = 0; slot != GAME_BOY_CHARACTER_COUNT; slot++) {
    if (g_GameBoyCharacters[slot].status == GAME_BOY_CHARACTER_STATUS_PRESENT) {
      character = g_GameBoyCharacters[slot].character;
      LoadLargePalette(&displayList, transfer->facePalettes[character]);
      LoadTextureBlock(&displayList, transfer->faceImages[character],
          G_IM_FMT_CI, G_IM_SIZ_8b, FACE_SIZE, FACE_SIZE, 0, 0, 0, 0, 0, 0, 0);
      DrawTextureRect(&displayList, (FACES_LEFT + (slot * FACE_SPACING)) << 2,
          FACES_TOP << 2, (FACES_LEFT + (slot * FACE_SPACING) + FACE_SIZE) << 2,
          (FACES_TOP + FACE_SIZE) << 2, G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE,
          TEXEL_STEP_ONE);
    }
  }

  *displayListHead = displayList;
}

void DrawOptionsScreen(Gfx** displayListHead, GameBoyTransfer* transfer) {
  Gfx* displayList = *displayListHead;

  DrawImportedFaces(&displayList, transfer);
  *displayListHead = displayList;
}

/* Whether a cartridge character is on the list: read in, or being brought over
 */
static inline Boolean IsListed(s32 listing) {
  return listing == LISTING_SHOWN || listing == LISTING_BRINGING;
}

/*
 * Draws the Bring Characters screen: the faces already brought over, the
 * cartridge characters on the list with their controllers, the cursor, and the
 * arrows when the list scrolls. A character being brought over moves from the
 * list to its slot.
 */
void DrawBringCharactersScreen(
    Gfx** displayListHead, GameBoyTransfer* transfer) {
  Gfx* displayList = *displayListHead;
  s32 index;
  s32 shownCount;
  s32 listed;
  s32 left;
  s32 slotLeft;
  s32 top;
  s32 character;

  gDPPipeSync(displayList++);
  gDPSetTextureLUT(displayList++, G_TT_RGBA16);
  DrawImportedFaces(&displayList, transfer);
  LoadSmallPalette(&displayList, 0, ImageBody(transfer->controllerIconsImage));
  LoadNibbleTextureBlock(&displayList,
      ImageTexels(transfer->controllerIconsImage), G_IM_FMT_CI,
      CONTROLLER_ICON_SIZE, CONTROLLER_ICON_SIZE * MAXCONTROLLERS, 0, 0, 0, 0,
      0, 0, 0);
  shownCount = 0;
  for (index = transfer->unk7B8; index != CARTRIDGE_CHARACTER_COUNT; index++) {
    if (transfer->cartridgeCharacters[transfer->listedCharacters[index]]
                .status == GAME_BOY_CHARACTER_STATUS_PRESENT &&
        IsListed(transfer->listingStates[transfer->listedCharacters[index]])) {
      DrawTextureRect(&displayList,
          (LISTED_FACES_LEFT + (shownCount * LISTED_FACE_SPACING)) << 2,
          LISTED_ICONS_TOP << 2,
          (LISTED_FACES_LEFT + (shownCount * LISTED_FACE_SPACING) +
              CONTROLLER_ICON_SIZE)
              << 2,
          (LISTED_ICONS_TOP + CONTROLLER_ICON_SIZE) << 2, G_TX_RENDERTILE, 0,
          (transfer->listedCharacters[index] / CARTRIDGE_SAVE_COUNT *
              CONTROLLER_ICON_SIZE)
              << 5,
          TEXEL_STEP_ONE, TEXEL_STEP_ONE);
      shownCount++;
      if (shownCount == SHOWN_LISTED_COUNT) {
        break;
      }
    }
  }

  LoadSmallPalette(&displayList, 0, ImageBody(transfer->statBarsImage));
  gDPLoadTextureTile_4b(displayList++,
      TextureAddress(ImageTexels(transfer->statBarsImage)), G_IM_FMT_CI,
      CURVE_BAR_WIDTH, 0, 0, 0, CURVE_BAR_WIDTH - 1, CURVE_BAR_HEIGHT - 1, 0, 0,
      0, 0, 0, 0, 0);
  DrawTextureRect(&displayList, CURVE_BAR_LEFT << 2, CURVE_BAR_TOP << 2,
      (CURVE_BAR_LEFT + CURVE_BAR_WIDTH) << 2,
      (CURVE_BAR_TOP + CURVE_BAR_HEIGHT) << 2, G_TX_RENDERTILE, 0, 0,
      TEXEL_STEP_ONE, TEXEL_STEP_ONE);
  shownCount = 0;
  for (index = transfer->unk7B8; index != CARTRIDGE_CHARACTER_COUNT; index++) {
    listed = transfer->listedCharacters[index];
    if (transfer->cartridgeCharacters[listed].status ==
            GAME_BOY_CHARACTER_STATUS_PRESENT &&
        IsListed(transfer->listingStates[listed])) {
      character = transfer->cartridgeCharacters[listed].character;
      left = LISTED_FACES_LEFT + (shownCount * LISTED_FACE_SPACING);
      top = LISTED_FACES_TOP;
      if (transfer->listingStates[listed] == LISTING_BRINGING) {
        slotLeft = FACES_LEFT + (transfer->characterIndex * FACE_SPACING);
        left += (slotLeft - left) * transfer->bringingFrame /
            (BRINGING_FRAME_COUNT - 1);
        top += (FACES_TOP - top) * transfer->bringingFrame /
            (BRINGING_FRAME_COUNT - 1);
      }

      LoadLargePalette(&displayList, transfer->facePalettes[character]);
      LoadTextureBlock(&displayList, transfer->faceImages[character],
          G_IM_FMT_CI, G_IM_SIZ_8b, FACE_SIZE, FACE_SIZE, 0, 0, 0, 0, 0, 0, 0);
      DrawTextureRect(&displayList, left << 2, top << 2,
          (left + FACE_SIZE) << 2, (top + FACE_SIZE) << 2, G_TX_RENDERTILE, 0,
          0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
      shownCount++;
      if (shownCount == SHOWN_LISTED_COUNT) {
        break;
      }
    }
  }

  if (g_GameBoyTransfer.listedCharacterCount != 0) {
    if (transfer->screenStep == BRING_CHOOSING) {
      DrawCursorBox(&displayList,
          LISTED_FACES_LEFT - BRING_CURSOR_MARGIN +
              ((transfer->listedCharacterIndex - transfer->unk7B8) *
                  LISTED_FACE_SPACING),
          LISTED_FACES_TOP - BRING_CURSOR_MARGIN);
    } else if (transfer->screenStep == BRING_CHOOSING_SLOT) {
      DrawCursorBox(&displayList,
          FACES_LEFT - BRING_CURSOR_MARGIN +
              (transfer->characterIndex * FACE_SPACING),
          FACES_TOP - BRING_CURSOR_MARGIN);
    }
  }

  gSPDisplayList(displayList++, g_DecalTextureSetup);
  gDPPipeSync(displayList++);
  gDPSetTextureLUT(displayList++, G_TT_RGBA16);
  gDPPipeSync(displayList++);
  gDPSetTexturePersp(displayList++, G_TP_NONE);
  if (transfer->screenStep == BRING_CHOOSING) {
    f_CharacterArrowFrame = (f_CharacterArrowFrame + 1) % ARROW_FRAME_COUNT;
    LoadSmallPalette(&displayList, 0, ImageBody(transfer->arrowsImage));
    LoadNibbleTextureBlock(&displayList,
        ImageTexelsAt(transfer->arrowsImage,
            f_CharacterArrowFrame / 4 * ARROWS_IMAGE_SIZE),
        G_IM_FMT_CI, ARROW_SIZE, ARROW_SIZE * 4, 0, 0, 0, 0, 0, 0, 0);
    gDPPipeSync(displayList++);
    if (transfer->unk7B8 > 0) {
      DrawTextureRect(&displayList, LEFT_ARROW_LEFT << 2, ARROWS_TOP << 2,
          (LEFT_ARROW_LEFT + ARROW_SIZE) << 2, (ARROWS_TOP + ARROW_SIZE) << 2,
          G_TX_RENDERTILE, 0, LEFT_ARROW_ROW << 5, TEXEL_STEP_ONE,
          TEXEL_STEP_ONE);
    }

    if (transfer->unk7B8 <
        transfer->listedCharacterCount - SHOWN_LISTED_COUNT) {
      DrawTextureRect(&displayList, RIGHT_ARROW_LEFT << 2, ARROWS_TOP << 2,
          (RIGHT_ARROW_LEFT + ARROW_SIZE) << 2, (ARROWS_TOP + ARROW_SIZE) << 2,
          G_TX_RENDERTILE, 0, RIGHT_ARROW_ROW << 5, TEXEL_STEP_ONE,
          TEXEL_STEP_ONE);
    }
  }

  *displayListHead = displayList;
}

void DrawSendExperienceScreen(
    Gfx** displayListHead, GameBoyTransfer* transfer) {
  Gfx* displayList = *displayListHead;
  s32 index;

  /* A second counter alongside index is there to match */
  s32 slot;

  DrawImportedFaces(&displayList, transfer);
  if (transfer->screenStep == 0) {
    DrawCursorBox(&displayList,
        FACES_LEFT - FACE_CURSOR_MARGIN +
            (transfer->characterIndex * FACE_SPACING),
        FACES_TOP - FACE_CURSOR_MARGIN);
    gDPPipeSync(displayList++);
    gDPSetTexturePersp(displayList++, G_TP_NONE);
    gDPPipeSync(displayList++);
    gDPSetTextureLUT(displayList++, G_TT_RGBA16);
  }

  LoadSmallPalette(&displayList, 0, ImageBody(transfer->controllerIconsImage));
  LoadNibbleTextureBlock(&displayList,
      ImageTexels(transfer->controllerIconsImage), G_IM_FMT_CI,
      CONTROLLER_ICON_SIZE, CONTROLLER_ICON_SIZE * MAXCONTROLLERS, 0, 0, 0, 0,
      0, 0, 0);
  slot = 0;
  gDPSetPrimColor(displayList++, 0, 0, 255, 255, 255, 255);
  for (index = 0; index != transfer->taggedCartridgeCount; index++) {
    if (transfer->screenStep == 3 && slot != transfer->chosenCartridgeIndex) {
      gDPSetPrimColor(displayList++, 0, 0, 127, 127, 127, 255);
    } else {
      gDPSetPrimColor(displayList++, 0, 0, 255, 255, 255, 255);
    }

    DrawTextureRect(&displayList,
        (CONTROLLER_ICONS_LEFT + (slot * CONTROLLER_ICON_SPACING)) << 2,
        CONTROLLER_ICONS_TOP << 2,
        (CONTROLLER_ICONS_LEFT + (slot * CONTROLLER_ICON_SPACING) +
            CONTROLLER_ICON_SIZE)
            << 2,
        (CONTROLLER_ICONS_TOP + CONTROLLER_ICON_SIZE) << 2, G_TX_RENDERTILE, 0,
        (transfer->taggedControllers[index] * CONTROLLER_ICON_SIZE) << 5,
        TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    slot++;
  }

  *displayListHead = displayList;
}

/* Whether DrawTransmitMenu shows the transfer mode's options on a screen */
#define AreOptionsShown(screen) \
  ((screen) == TRANSMIT_SCREEN_OPTIONS || \
      (screen) == TRANSMIT_SCREEN_LEAVING || \
      (screen) == TRANSMIT_SCREEN_UNK6 || \
      (screen) == TRANSMIT_SCREEN_RUMBLE_PAK_PROMPT)

void DrawTransmitMenu(Gfx** displayListHead) {
  GameBoyTransfer* transfer = &g_GameBoyTransfer;
  Gfx* displayList = *displayListHead;
  s32 index;
  s32 rowCount;
  s32 panelRowCount; /* Apart from rowCount, which is there to match */
  s32 option;
  s32 width;
  s32 height;
  s32 pointerLeft;
  s32 pointerSwing;

  gDPPipeSync(displayList++);
  gDPPipeSync(displayList++);
  gDPSetCycleType(displayList++, G_CYC_1CYCLE);
  gDPPipeSync(displayList++);
  gDPPipeSync(displayList++);
  gDPSetRenderMode(displayList++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
  gDPSetCombineMode(displayList++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPPipeSync(displayList++);
  gDPSetColorDither(displayList++, G_CD_DISABLE);
  gSPTexture(displayList++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
  gDPPipeSync(displayList++);
  gDPSetTexturePersp(displayList++, G_TP_NONE);
  gDPPipeSync(displayList++);
  gDPSetTextureFilter(displayList++, G_TF_POINT);
  gDPPipeSync(displayList++);
  gDPSetAlphaCompare(displayList++, G_AC_NONE);
  gDPPipeSync(displayList++);
  gDPSetTextureLUT(displayList++, G_TT_RGBA16);
  LoadLargePalette(&displayList, ImageBody(transfer->courseSceneImage));
  for (index = 0; index != COURSE_SCENE_STRIP_COUNT; index++) {
    rowCount =
        (index != COURSE_SCENE_STRIP_COUNT - 1) ? COURSE_SCENE_STRIP_ROWS : 1;
    if (index == 0) {
      LoadTextureBlock(&displayList,
          LargeImageTexels(transfer->courseSceneImage), G_IM_FMT_CI,
          G_IM_SIZ_8b, COURSE_SCENE_WIDTH, COURSE_SCENE_STRIP_ROWS + 2, 0, 0, 0,
          0, 0, 0, 0);
      DrawTextureRect(&displayList, COURSE_SCENE_LEFT << 2,
          COURSE_SCENE_TOP << 2,
          (COURSE_SCENE_LEFT + (COURSE_SCENE_WIDTH * 2)) << 2,
          (COURSE_SCENE_TOP + ((COURSE_SCENE_STRIP_ROWS + 1) * 2)) << 2,
          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE / 2, TEXEL_STEP_ONE / 2);
    } else {
      LoadTextureBlock(&displayList,
          LargeImageTexels(transfer->courseSceneImage) +
              (index * (COURSE_SCENE_WIDTH * COURSE_SCENE_STRIP_ROWS)),
          G_IM_FMT_CI, G_IM_SIZ_8b, COURSE_SCENE_WIDTH, rowCount + 2, 0, 0, 0,
          0, 0, 0, 0);
      DrawTextureRect(&displayList, COURSE_SCENE_LEFT << 2,
          (COURSE_SCENE_TOP + 2 + (index * COURSE_SCENE_STRIP_ROWS * 2)) << 2,
          (COURSE_SCENE_LEFT + (COURSE_SCENE_WIDTH * 2)) << 2,
          (COURSE_SCENE_TOP + 2 + (index * COURSE_SCENE_STRIP_ROWS * 2) +
              (rowCount * 2))
              << 2,
          G_TX_RENDERTILE, 0, 1 << 5, TEXEL_STEP_ONE / 2, TEXEL_STEP_ONE / 2);
    }
  }

  LoadLargePalette(&displayList, ImageBody(transfer->titleImage));
  for (index = 0; index != TITLE_STRIP_COUNT; index++) {
    gDPLoadTextureTile(displayList++,
        TextureAddress(LargeImageTexels(transfer->titleImage) +
            (index * (TITLE_WIDTH * TITLE_STRIP_ROWS))),
        G_IM_FMT_CI, G_IM_SIZ_8b, TITLE_WIDTH, 0, 0, 0, TITLE_WIDTH - 1,
        TITLE_STRIP_ROWS - 1, 0, 0, 0, 0, 0, 0, 0);
    DrawTextureRect(&displayList, TITLE_LEFT << 2,
        (TITLE_TOP + (index * TITLE_STRIP_ROWS)) << 2,
        (TITLE_LEFT + TITLE_WIDTH) << 2,
        (TITLE_TOP + (index * TITLE_STRIP_ROWS) + TITLE_STRIP_ROWS) << 2,
        G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
  }

  gDPPipeSync(displayList++);
  gDPSetTextureLUT(displayList++, G_TT_NONE);
  for (index = 0; index != WOOD_PANEL_STRIP_COUNT; index++) {
    panelRowCount =
        (index != WOOD_PANEL_STRIP_COUNT - 1) ? WOOD_PANEL_STRIP_ROWS : 1;
    gDPLoadTextureTile(displayList++,
        TextureAddress(ImageBody(transfer->woodPanelImage) +
            (index * (WOOD_PANEL_WIDTH * WOOD_PANEL_STRIP_ROWS * 2))),
        G_IM_FMT_RGBA, G_IM_SIZ_16b, WOOD_PANEL_WIDTH, 0, 0, 0,
        WOOD_PANEL_WIDTH - 1, panelRowCount - 1, 0, 0, 0, 0, 0, 0, 0);
    DrawTextureRect(&displayList, WOOD_PANEL_LEFT << 2,
        (WOOD_PANEL_TOP + (index * WOOD_PANEL_STRIP_ROWS)) << 2,
        (WOOD_PANEL_LEFT + WOOD_PANEL_WIDTH) << 2,
        (WOOD_PANEL_TOP + (index * WOOD_PANEL_STRIP_ROWS) + panelRowCount) << 2,
        G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
  }

  switch (transfer->screen) {
    case TRANSMIT_SCREEN_OPTIONS:
      DrawOptionsScreen(&displayList, transfer);
      break;
    case TRANSMIT_SCREEN_BRING_CHARACTERS:
      DrawBringCharactersScreen(&displayList, transfer);
      break;
    case TRANSMIT_SCREEN_SEND_STATUS:
      DrawSendStatusScreen(&displayList, transfer);
      break;
    case TRANSMIT_SCREEN_SEND_EXPERIENCE:
      DrawSendExperienceScreen(&displayList, transfer);
      break;
  }

  if (transfer->screen == TRANSMIT_SCREEN_OPTIONS) {
    MeasureMessage(
        f_ModeOptions[transfer->transferMode][transfer->optionIndex] +
            TRANSMIT_OPTION_MESSAGES,
        FONT_SIZE_LARGE, &width, &height, 0);
    /* The pointer swings in and out over half a turn of the angle */
    f_OptionPointerAngle += DegreesToRadians(10);
    if (f_OptionPointerAngle > M_PI) {
      f_OptionPointerAngle -= M_PI;
    }

    pointerSwing = sinf(f_OptionPointerAngle) * 8.0f;
    gDPPipeSync(displayList++);
    gDPSetTextureLUT(displayList++, G_TT_RGBA16);
    LoadSmallPalette(&displayList, 0, ImageBody(transfer->pointerImage));
    gDPLoadTextureTile_4b(displayList++,
        TextureAddress(ImageTexels(transfer->pointerImage)), G_IM_FMT_CI,
        POINTER_WIDTH, 0, 0, 0, POINTER_WIDTH - 1, POINTER_IMAGE_HEIGHT - 1, 0,
        0, 0, 0, 0, 0, 0);
    pointerLeft = OPTIONS_CENTER_X - OPTION_POINTER_GAP - POINTER_WIDTH -
        (width / 2) - pointerSwing;
    DrawTextureRect(&displayList, pointerLeft << 2,
        (OPTION_POINTER_TOP + (transfer->optionIndex * OPTION_SPACING)) << 2,
        (pointerLeft + POINTER_WIDTH) << 2,
        (OPTION_POINTER_TOP + (transfer->optionIndex * OPTION_SPACING) +
            POINTER_HEIGHT)
            << 2,
        G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
  }

  SetUpTextDrawing(&displayList);
  gDPSetPrimColor(displayList++, 0, 0, 255, 255, 255, 255);
  if (AreOptionsShown(transfer->screen)) {
    for (index = 0; index != MODE_OPTION_COUNT; index++) {
      option = f_ModeOptions[transfer->transferMode][index];
      if (option != TRANSMIT_OPTION_NONE) {
        SetTextSlot(OPTIONS_CENTER_X, OPTIONS_TOP + (index * OPTION_SPACING),
            option + TRANSMIT_OPTION_MESSAGES, TEXT_SLOT_LARGE_FONT, index,
            TEXT_FLAG_CENTERED | TEXT_FLAG_UNK20, 255, 255, 255, 255);
        DrawLargeTextSlot(&displayList, index);
      }
    }

    SetTextSlot(HINT_LEFT, HINT_TOP,
        f_ModeOptions[transfer->transferMode][transfer->optionIndex] +
            TRANSMIT_OPTION_HINTS,
        TEXT_SLOT_LARGE_FONT, HINT_TEXT_SLOT, TEXT_FLAG_UNK20, 255, 255, 255,
        255);
    DrawLargeTextSlot(&displayList, HINT_TEXT_SLOT);
  }

  if (transfer->screen == TRANSMIT_SCREEN_SEND_STATUS) {
    DrawSendStatusHint(&displayList, transfer);
  }

  if (transfer->screen == TRANSMIT_SCREEN_BRING_CHARACTERS) {
    DrawBringCharactersText(&displayList, transfer);
  }

  if (transfer->screen == TRANSMIT_SCREEN_SEND_EXPERIENCE) {
    DrawSendExperienceText(&displayList, transfer);
  }

  DrawNotice(&displayList);
  gSPDisplayList(displayList++, g_DecalTextureSetup);
  gDPPipeSync(displayList++);
  gDPSetTextureLUT(displayList++, G_TT_RGBA16);
  gDPPipeSync(displayList++);
  gDPSetTextureFilter(displayList++, G_TF_POINT);
  if (transfer->isAccessing == TRUE) {
    DrawAccessingSign(&displayList);
  }

  if (transfer->pressingHandPosition != 0) {
    DrawPressingHand(&displayList);
  }

  *displayListHead = displayList;
}

void ClearCartridgeCharacters(void) {
  /* The transfer pointer is there to match */
  GameBoyTransfer* transfer = &g_GameBoyTransfer;
  s32 controller;
  s32 save;

  if (transfer->screen == TRANSMIT_SCREEN_BRING_CHARACTERS) {
    transfer->unk7A0 = 0;
    ResetBringCharacters();
    for (controller = 0; controller != MAXCONTROLLERS; controller++) {
      for (save = 0; save != CARTRIDGE_SAVE_COUNT; save++) {
        transfer->listingStates[(controller * CARTRIDGE_SAVE_COUNT) + save] =
            LISTING_NONE;
        transfer
            ->cartridgeCharacters[(controller * CARTRIDGE_SAVE_COUNT) + save]
            .status = 0;
      }
    }

    transfer->listedCharacterCount = 0;
  }
}

/*
 * Adds the character's experience to its save on the Game Boy cartridge it came
 * from, looking through every controller's pak for it, and rewrites the save's
 * checksums. Doesn't match: the original copies the experience sum from a
 * scratch register into the one that holds the character data's experience.
 */
#ifdef NON_MATCHING
s32 WriteExperienceToCartridge(s32 characterIndex) {
  /* The transfer pointer is there to match */
  GameBoyTransfer* transfer = &g_GameBoyTransfer;
  u8 characterData[CHARACTER_DATA_SIZE];
  u8 save[CARTRIDGE_SAVE_SIZE];
  GameBoyCharacter* character;
  s32 fault = CARTRIDGE_FAULT_NONE;
  s32 controller;
  s32 result;
  Boolean isSignatureWrong;
  s32 index;
  s32 saveIndex;
  s32 recordOffset;
  s32 dataAddress;
  s32 dataSize;
  s32 storedSum;
  s32 sum;
  s32 experience;

  transfer->unk7C8 = FALSE;
  transfer->unk7C4 = FALSE;
  transfer->unk7C0 = FALSE;
  transfer->unk7CC = FALSE;
  transfer->unk7BC = 0;
  result = 0;
  for (controller = 0; controller != g_ControllerCount; controller++) {
    transfer->transferResult =
        OpenGameBoyPakWithRetries(g_PlayerControllerPorts[controller],
            &transfer->cartridgeHeader, &transfer->pakFile);
    if (transfer->transferResult == PAK_RESULT_NO_PAK) {
      transfer->unk7C8 = TRUE;
    }

    if (transfer->transferResult == PAK_RESULT_OTHER_PAK) {
      transfer->unk7C4 = TRUE;
    }

    if (transfer->transferResult == PAK_RESULT_NO_CARTRIDGE) {
      transfer->unk7C0 = TRUE;
    }

    if (transfer->transferResult == PAK_RESULT_WRONG_CARTRIDGE) {
      transfer->unk7CC = TRUE;
    }

    if (transfer->transferResult == PAK_RESULT_OK) {
      recordOffset = SAVE_RECORDS_OFFSET;
      isSignatureWrong = FALSE;
      character = &g_GameBoyCharacters[characterIndex];
      saveIndex = character->unk38;
      transfer->unk7BC++;
      osSyncPrintf("PHASE 1\n");
      result = nuContGBPakFread(
          &transfer->pakFile, CARTRIDGE_RAM_ADDRESS, save, CARTRIDGE_SAVE_SIZE);
      /* Powering off and continuing after each pak error is there to match */
      if (result != 0) {
        nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
        continue;
      }

      osSyncPrintf("PHASE 2\n");
      for (index = 0; index != CARTRIDGE_TAG_SIZE; index++) {
        if (save[SAVE_SIGNATURE_OFFSET + index] !=
            f_CartridgeSaveSignature[index]) {
          isSignatureWrong = TRUE;
        }
      }

      osSyncPrintf("PHASE 3\n");
      if (isSignatureWrong == TRUE) {
        fault = CARTRIDGE_FAULT_UNINITIALIZED;
      }

      if (ComputeSaveChecksum(save) != GetStoredChecksum(save)) {
        fault = CARTRIDGE_FAULT_CORRUPTED;
      } else {
        osSyncPrintf("PHASE 4\n");
        if (saveIndex == 0) {
          recordOffset = SAVE_RECORDS_OFFSET;
        }

        if (saveIndex == 1) {
          recordOffset = SAVE_RECORDS_OFFSET + SAVE_RECORD_SIZE;
        }

        if (saveIndex == 2) {
          recordOffset = SAVE_RECORDS_OFFSET + (SAVE_RECORD_SIZE * 2);
        }

        osSyncPrintf("PHASE 5\n");
        dataAddress = save[recordOffset + RECORD_ADDRESS_OFFSET] +
            (save[recordOffset + RECORD_ADDRESS_OFFSET + 1] << 8);
        dataSize = save[recordOffset + RECORD_SIZE_OFFSET] +
            (save[recordOffset + RECORD_SIZE_OFFSET + 1] << 8);
        osSyncPrintf("PHASE 6\n");
        result = nuContGBPakFread(&transfer->pakFile,
            dataAddress - STATUS_ADDRESS_BIAS, characterData, dataSize);
        if (result != 0) {
          nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
          continue;
        }

        sum = 0;
        osSyncPrintf("PHASE 7\n");
        storedSum = save[recordOffset + RECORD_SUM_OFFSET] |
            (save[recordOffset + RECORD_SUM_OFFSET + 1] << 8);
        for (index = 0; index != dataSize; index++) {
          sum += characterData[index];
        }

        osSyncPrintf("PHASE 8\n");
        sum &= 0xFFFF;
        if (sum != storedSum) {
          nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
          fault = CARTRIDGE_FAULT_CORRUPTED;
          continue;
        }

        osSyncPrintf("PHASE 9\n");
        if (IsCharacterFromSave(character, characterData)) {
          osSyncPrintf("PHASE 9.5\n");
          sum = character->experience +
              ((characterData[DATA_EXPERIENCE_OFFSET + 1] << 8) |
                  characterData[DATA_EXPERIENCE_OFFSET]);
          experience = (sum > MAX_EXPERIENCE) ? MAX_EXPERIENCE : sum;
          osSyncPrintf("PHASE 10\n");
          characterData[DATA_EXPERIENCE_OFFSET + 1] = experience >> 8;
          characterData[DATA_EXPERIENCE_OFFSET] = experience;
          characterData[DATA_UNK15E_OFFSET] = 100;
          result = nuContGBPakFwrite(&transfer->pakFile,
              dataAddress - STATUS_ADDRESS_BIAS + DATA_EXPERIENCE_OFFSET,
              &characterData[DATA_EXPERIENCE_OFFSET],
              DATA_EXPERIENCE_WRITE_SIZE);
          if (result != 0) {
            nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
            continue;
          }

          sum = 0;
          osSyncPrintf("PHASE 11\n");
          for (index = 0; index != dataSize; index++) {
            sum += characterData[index];
          }

          sum &= 0xFFFF;
          osSyncPrintf("write sum %04x\n", sum);
          osSyncPrintf("PHASE 12\n");
          save[recordOffset + RECORD_SUM_OFFSET] = sum;
          save[recordOffset + RECORD_SUM_OFFSET + 1] = sum >> 8;
          result = nuContGBPakFwrite(&transfer->pakFile,
              CARTRIDGE_RAM_ADDRESS + recordOffset + RECORD_SUM_OFFSET,
              &save[recordOffset + RECORD_SUM_OFFSET], SAVE_HALFWORD_SIZE);
          if (result != 0) {
            nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
            continue;
          }

          osSyncPrintf("PHASE 13\n");
          sum = ComputeSaveChecksum(save);
          save[SAVE_CHECKSUM_OFFSET] = sum;
          save[SAVE_CHECKSUM_OFFSET + 1] = sum >> 8;
          result = nuContGBPakFwrite(&transfer->pakFile,
              CARTRIDGE_RAM_ADDRESS + SAVE_CHECKSUM_OFFSET,
              &save[SAVE_CHECKSUM_OFFSET], SAVE_HALFWORD_SIZE);
          if (result != 0) {
            nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
            continue;
          }

          nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
          osSyncPrintf("PHASE 14\n");
          character->experience = 0;
          transfer->transferResult = PAK_RESULT_OK;
          return EXPERIENCE_WRITE_DONE;
        }
      }
    }

    nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
  }

  if (transfer->unk7BC == 1) {
    if (fault == CARTRIDGE_FAULT_UNINITIALIZED) {
      transfer->transferResult = PAK_RESULT_UNINITIALIZED;
      return EXPERIENCE_WRITE_FAILED;
    }

    if (fault == CARTRIDGE_FAULT_CORRUPTED) {
      transfer->transferResult = PAK_RESULT_CORRUPTED;
      return EXPERIENCE_WRITE_FAILED;
    }
  }

  if (transfer->unk7BC > 0) {
    transfer->transferResult = PAK_RESULT_NO_CHARACTERS;
    return EXPERIENCE_WRITE_NOT_FROM_CARTRIDGE;
  }

  if (transfer->unk7CC == TRUE) {
    transfer->transferResult = PAK_RESULT_WRONG_CARTRIDGE;
  } else if (transfer->unk7C0 == TRUE) {
    transfer->transferResult = PAK_RESULT_NO_CARTRIDGE;
  } else if (transfer->unk7C4 == TRUE) {
    transfer->transferResult = PAK_RESULT_OTHER_PAK;
  } else if (result != 0) {
    transfer->transferResult = result;
    return EXPERIENCE_WRITE_PAK_ERROR;
  } else {
    transfer->transferResult = PAK_RESULT_NO_PAK;
  }

  return EXPERIENCE_WRITE_FAILED;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_13/transmit_menu", WriteExperienceToCartridge);
#endif

#ifndef NON_MATCHING
INCLUDE_RODATA("asm/nonmatchings/ovl_13/transmit_menu", f_WriteSumFormat);
INCLUDE_RODATA("asm/nonmatchings/ovl_13/transmit_menu", D_801FBF18_ovl_13);
INCLUDE_RODATA("asm/nonmatchings/ovl_13/transmit_menu", D_801FBF24_ovl_13);
INCLUDE_RODATA("asm/nonmatchings/ovl_13/transmit_menu", D_801FBF30_ovl_13);
#else
const char f_WriteSumFormat[] = "write sum %04x\n";
#endif

/*
 * Checks that the chosen cartridge is the one tagged and that its save is
 * intact, and reports whether the save already holds a status, which
 * WriteStatusToCartridge marks.
 */
s32 CheckStatusCartridge(s32 cartridgeIndex) {
  /* The transfer pointer is there to match */
  GameBoyTransfer* transfer = &g_GameBoyTransfer;
  u8 save[CARTRIDGE_SAVE_SIZE];
  Boolean isSignatureWrong;
  s32 index;
  s32 result;
  s32 readResult;
  s32 computedSum;
  s32 storedSum;

  result =
      OpenGameBoyPakWithRetries(g_PlayerControllerPorts[g_GameBoyTransfer
                                        .taggedControllers[cartridgeIndex]],
          &transfer->cartridgeHeader, &transfer->pakFile);
  g_GameBoyTransfer.transferResult = result;
  if (result != PAK_RESULT_OK) {
    /* Going to the shared power-off is there to match */
    goto FAILED;
  }

  isSignatureWrong = FALSE;
  readResult = nuContGBPakFread(
      &transfer->pakFile, CARTRIDGE_RAM_ADDRESS, save, CARTRIDGE_SAVE_SIZE);
  if (readResult != 0) {
    osSyncPrintf("head error  2\n");
    g_GameBoyTransfer.transferResult = readResult;
    nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
    return STATUS_CHECK_READ_FAILED;
  }

  for (index = 0; index != CARTRIDGE_TAG_SIZE; index++) {
    if (save[SAVE_SIGNATURE_OFFSET + index] !=
        f_CartridgeSaveSignature[index]) {
      isSignatureWrong = TRUE;
    }
  }

  for (index = 0; index != CARTRIDGE_TAG_SIZE; index++) {
    if (save[index] !=
        transfer->cartridgeTags[transfer->taggedControllers[cartridgeIndex]]
                               [index]) {
      osSyncPrintf("ライトマーク違反\n"); /* "Write mark violation"
                                           */
      transfer->transferResult = PAK_RESULT_REMOVED_RETRYING;
      nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
      return STATUS_CHECK_FAILED;
    }
  }

  computedSum = ComputeSaveChecksum(save);
  storedSum = GetStoredChecksum(save);
  osSyncPrintf("sum1 %04x\n", computedSum);
  osSyncPrintf("sum2 %04x\n", storedSum);
  if (computedSum != storedSum || isSignatureWrong) {
    osSyncPrintf("sum error  2\n");
    transfer->transferResult = PAK_RESULT_CORRUPTED;
    nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
    return STATUS_CHECK_FAILED;
  }

  if (save[SAVE_HAS_STATUS_OFFSET] == 0) {
    nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
    return STATUS_CHECK_NO_STATUS;
  }

  nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
  return STATUS_CHECK_HAS_STATUS;

FAILED:

  nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
  return STATUS_CHECK_FAILED;
}

/*
 * Marks the chosen cartridge's save as holding a status and writes the status
 * data where the save says, then the sums of both. It first checks that the
 * cartridge is the one tagged and that its save is intact.
 */
s32 WriteStatusToCartridge(s32 cartridgeIndex) {
  /* The transfer pointer is there to match */
  GameBoyTransfer* transfer = &g_GameBoyTransfer;
  u8 save[CARTRIDGE_SAVE_SIZE];
  u8 status[STATUS_DATA_SIZE];
  s32 result;
  s32 index;
  s32 statusAddress;
  s32 statusSize;
  s32 sum;

  g_GameBoyTransfer.transferResult = OpenGameBoyPakWithRetries(
      g_PlayerControllerPorts[transfer->taggedControllers[cartridgeIndex]],
      &transfer->cartridgeHeader, &transfer->pakFile);
  if (g_GameBoyTransfer.transferResult != PAK_RESULT_OK) {
    /* Going to the shared power-off is there to match */
    goto FAILED;
  }

  result = nuContGBPakFread(
      &transfer->pakFile, CARTRIDGE_RAM_ADDRESS, save, CARTRIDGE_SAVE_SIZE);
  if (result != 0) {
    nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
    g_GameBoyTransfer.transferResult = result;
    return STATUS_WRITE_PAK_ERROR;
  }

  for (index = 0; index != CARTRIDGE_TAG_SIZE; index++) {
    if (save[index] !=
        transfer->cartridgeTags[transfer->taggedControllers[cartridgeIndex]]
                               [index]) {
      /* "Write mark violation"; the original passes sum before setting it */
      osSyncPrintf("ライトマーク違反\n", sum);
      nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
      transfer->transferResult = PAK_RESULT_CARTRIDGE_REPLACED;
      return STATUS_WRITE_FAILED;
    }
  }

  sum = ComputeSaveChecksum(save);
  if (sum != GetStoredChecksum(save)) {
    transfer->transferResult = PAK_RESULT_LOOSE_PAK;
    nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
    return STATUS_WRITE_FAILED;
  }

  statusAddress = save[SAVE_STATUS_ADDRESS_OFFSET] +
      (save[SAVE_STATUS_ADDRESS_OFFSET + 1] << 8);
  save[SAVE_HAS_STATUS_OFFSET] = TRUE;
  statusSize =
      save[SAVE_STATUS_SIZE_OFFSET] + (save[SAVE_STATUS_SIZE_OFFSET + 1] << 8);
  result = nuContGBPakFwrite(&transfer->pakFile,
      CARTRIDGE_RAM_ADDRESS + SAVE_HAS_STATUS_OFFSET,
      &save[SAVE_HAS_STATUS_OFFSET], SAVE_HALFWORD_SIZE);
  if (result != 0) {
    nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
    transfer->transferResult = result;
    return STATUS_WRITE_PAK_ERROR;
  }

  BuildStatusData(status);
  result = nuContGBPakFwrite(&transfer->pakFile,
      statusAddress - STATUS_ADDRESS_BIAS, status, statusSize);
  if (result != 0) {
    nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
    transfer->transferResult = result;
    return STATUS_WRITE_PAK_ERROR;
  }

  sum = 0;
  for (index = 0; index != statusSize; index++) {
    sum += status[index];
  }

  sum &= 0xFFFF;
  osSyncPrintf(f_WriteSumFormat, sum);
  save[SAVE_STATUS_SUM_OFFSET] = sum;
  save[SAVE_STATUS_SUM_OFFSET + 1] = sum >> 8;
  result = nuContGBPakFwrite(&transfer->pakFile,
      CARTRIDGE_RAM_ADDRESS + SAVE_STATUS_SUM_OFFSET,
      &save[SAVE_STATUS_SUM_OFFSET], SAVE_HALFWORD_SIZE);
  if (result != 0) {
    nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
    transfer->transferResult = result;
    return STATUS_WRITE_PAK_ERROR;
  }

  sum = ComputeSaveChecksum(save) & 0xFFFF;
  save[SAVE_CHECKSUM_OFFSET] = sum;
  save[SAVE_CHECKSUM_OFFSET + 1] = sum >> 8;
  osSyncPrintf("write sum2 %04x\n", sum);
  result = nuContGBPakFwrite(&transfer->pakFile,
      CARTRIDGE_RAM_ADDRESS + SAVE_CHECKSUM_OFFSET, &save[SAVE_CHECKSUM_OFFSET],
      SAVE_HALFWORD_SIZE);
  if (result != 0) {
    nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
    transfer->transferResult = result;
    return STATUS_WRITE_PAK_ERROR;
  }

  nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
  transfer->transferResult = PAK_RESULT_OK;
  return STATUS_WRITE_DONE;

FAILED:

  nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
  return STATUS_WRITE_FAILED;
}

/*
 * Reads one save of the Game Boy cartridges per call, opening each controller's
 * pak before its saves, and lists the characters in the saves that check out.
 * Returns PAK_RESULT_STILL_READING until the last save is read, then what was
 * found.
 */
s32 ReadCartridgeCharacters(void) {
  /* The transfer pointer is there to match */
  GameBoyTransfer* transfer = &g_GameBoyTransfer;
  u8 save[CARTRIDGE_SAVE_SIZE];
  u8 characterData[CHARACTER_DATA_SIZE];
  s32 port;
  s32 saveIndex;
  s32 recordOffset;
  s32 checkedState; /* How far the save has been checked */
  s32 result;
  s32 attempt;
  s32 index;
  Boolean isSignatureWrong;
  s32 storedSum;
  s32 dataAddress;
  s32 dataSize;
  s32 sum; /* Of the save, then of the character data; sharing it is there to
              match */
  Boolean isAlreadyBrought;
  s32 slot;

  if (transfer->unk7A0 == 0) {
    nuContGBPakMgrInit();
    transfer->unk7BC = 0;
    transfer->listedCharacterCount = 0;
    transfer->unk7C0 = FALSE;
    transfer->unk7C4 = FALSE;
    transfer->unk7C8 = FALSE;
    transfer->unk7CC = FALSE;
    transfer->unk7D0 = FALSE;
  }

  if (transfer->unk7A0 % 4 == 0) {
    port = g_PlayerControllerPorts[transfer->unk7A0 / 4];
    transfer->transferResult = OpenGameBoyPakWithRetries(
        port, &transfer->cartridgeHeader, &transfer->pakFile);
    osSyncPrintf("loadres %d  cont %d\n", transfer->transferResult, port);
    if (transfer->transferResult == PAK_RESULT_NO_PAK) {
      transfer->unk7C8 = TRUE;
    }

    if (transfer->transferResult == PAK_RESULT_OTHER_PAK) {
      transfer->unk7C4 = TRUE;
    }

    if (transfer->transferResult == PAK_RESULT_NO_CARTRIDGE) {
      transfer->unk7C0 = TRUE;
    }

    if (transfer->transferResult == PAK_RESULT_WRONG_CARTRIDGE) {
      transfer->unk7CC = TRUE;
    }
  } else if (transfer->transferResult == PAK_RESULT_OK) {
    saveIndex = (transfer->unk7A0 % 4) - 1;

    /* The original starts the offset at 0xA042, which every save index replaces
     */
    recordOffset = 0xA042;
    checkedState = NOT_CHECKED;
    for (index = 0; index != CHARACTER_DATA_SIZE; index++) {
      characterData[index] = 0;
    }

    transfer->unk7BC++;
    if (saveIndex == 0) {
      recordOffset = SAVE_RECORDS_OFFSET;
    }

    if (saveIndex == 1) {
      recordOffset = SAVE_RECORDS_OFFSET + SAVE_RECORD_SIZE;
    }

    if (saveIndex == 2) {
      recordOffset = SAVE_RECORDS_OFFSET + (SAVE_RECORD_SIZE * 2);
    }

    for (attempt = 0; attempt != SAVE_READ_ATTEMPTS; attempt++) {
      result = nuContGBPakFread(
          &transfer->pakFile, CARTRIDGE_RAM_ADDRESS, save, CARTRIDGE_SAVE_SIZE);
      if (result == 0) {
        sum = ComputeSaveChecksum(save);
        storedSum = GetStoredChecksum(save);
        isSignatureWrong = FALSE;
        for (index = 0; index != CARTRIDGE_TAG_SIZE; index++) {
          if (save[SAVE_SIGNATURE_OFFSET + index] !=
              f_CartridgeSaveSignature[index]) {
            isSignatureWrong = TRUE;
            transfer->unk7D0 = TRUE;
          }
        }

        if (sum == storedSum && isSignatureWrong == FALSE) {
          checkedState = SAVE_CHECKED;
          osSyncPrintf("checksum1 OK! \n");
          break;
        }
      }
    }

    /* The nested tests are there to match */
    if (checkedState == SAVE_CHECKED) {
      if (result == 0) {
        if (save[recordOffset] == RECORD_PRESENT) {
          checkedState = NOT_CHECKED;

          /* Working out the address in storedSum first is there to match */
          storedSum = save[recordOffset + RECORD_ADDRESS_OFFSET] +
              (save[recordOffset + RECORD_ADDRESS_OFFSET + 1] << 8) -
              STATUS_ADDRESS_BIAS;
          dataSize = save[recordOffset + RECORD_SIZE_OFFSET] +
              (save[recordOffset + RECORD_SIZE_OFFSET + 1] << 8);
          dataAddress = storedSum;
          for (attempt = 0; attempt != DATA_READ_ATTEMPTS; attempt++) {
            result = nuContGBPakFread(
                &transfer->pakFile, dataAddress, characterData, dataSize);
            storedSum = save[recordOffset + RECORD_SUM_OFFSET] |
                (save[recordOffset + RECORD_SUM_OFFSET + 1] << 8);
            sum = 0;
            for (index = 0; index != dataSize; index++) {
              sum += characterData[index];
            }

            sum &= 0xFFFF;
            if (sum == storedSum) {
              checkedState = DATA_CHECKED;
              osSyncPrintf("checksum2 OK! \n");
              break;
            }
          }
        }
      }
    }

    if (checkedState == DATA_CHECKED) {
      switch (result) {
        case 0:
          isAlreadyBrought = FALSE;

          /* "Slot %d read successfully" */
          osSyncPrintf("スロット%d 読み込み成功しました  \n", saveIndex);
          for (slot = 0; slot != GAME_BOY_CHARACTER_COUNT; slot++) {
            if (g_GameBoyCharacters[slot].status ==
                    GAME_BOY_CHARACTER_STATUS_PRESENT &&
                IsCharacterFromSave(
                    &g_GameBoyCharacters[slot], characterData)) {
              isAlreadyBrought = TRUE;
              transfer
                  ->cartridgeCharacters[(transfer->unk7A0 / 4 *
                                            CARTRIDGE_SAVE_COUNT) +
                      saveIndex]
                  .status = GAME_BOY_CHARACTER_STATUS_BROUGHT_OVER;
              ListPresentCharacters();
              break;
            }
          }

          if (!isAlreadyBrought) {
            /* transferResult is OK here, so the original never skips a save */
            if (transfer->transferResult == PAK_RESULT_NO_CHARACTERS) {
              SkipCartridgeSave(characterData, transfer->unk7A0 / 4, saveIndex,
                  transfer->cartridgeHeader.game_title,
                  transfer->cartridgeHeader.company_code);
            } else {
              DecodeCartridgeCharacter(characterData, transfer->unk7A0 / 4,
                  saveIndex, transfer->cartridgeHeader.company_code);
            }

            transfer
                ->listingStates[(transfer->unk7A0 / 4 * CARTRIDGE_SAVE_COUNT) +
                    saveIndex] = LISTING_SHOWN;
            transfer->listedCharacterCount++;
            ListPresentCharacters();
          }

          break;
        case PFS_ERR_NOPACK:
          transfer->unk7C8 = TRUE;
          osSyncPrintf("3rd PFS_ERR_NOPACK  \n");
          break;
        case PFS_ERR_DEVICE:
          transfer->unk7C4 = TRUE;
          osSyncPrintf("3rd PFS_ERR_DEVICE  \n");
          break;
        case PFS_ERR_CONTRFAIL:
          transfer->unk7D4 = TRUE;
          osSyncPrintf("3rd PFS_ERR_CONTRFAIL  \n");
          break;
        case PFS_ERR_NO_GBCART:
          transfer->unk7C0 = TRUE;
          osSyncPrintf("3rd PFS_ERR_NO_GBCART  \n");
          break;
        case PFS_ERR_NEW_GBCART:
          osSyncPrintf("3rd PFS_ERR_NEW_GBCART  \n");
          break;
      }
    }
  }

  if (transfer->unk7A0 % 4 == 3) {
    nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
  }

  if (transfer->unk7A0 == (g_ControllerCount * 4) - 1) {
    /* Repeating the results for one cartridge is there to match */
    if (transfer->unk7BC == 1) {
      if (transfer->unk7D0 == TRUE) {
        return PAK_RESULT_UNINITIALIZED;
      }

      if (transfer->listedCharacterCount == 0) {
        return PAK_RESULT_NO_CHARACTERS;
      }

      return PAK_RESULT_OK;
    }

    if (transfer->unk7BC > 0) {
      if (transfer->listedCharacterCount == 0) {
        return PAK_RESULT_NO_CHARACTERS;
      }

      return PAK_RESULT_OK;
    }

    if (transfer->unk7CC == TRUE) {
      return PAK_RESULT_WRONG_CARTRIDGE;
    }

    if (transfer->unk7C0 == TRUE) {
      return PAK_RESULT_NO_CARTRIDGE;
    }

    if (transfer->unk7C4 == TRUE) {
      return PAK_RESULT_OTHER_PAK;
    }

    if (transfer->unk7C8 == TRUE) {
      return PAK_RESULT_NO_PAK;
    }

    return PAK_RESULT_CONNECTOR_PROBLEM;
  }

  transfer->unk7A0++;
  return PAK_RESULT_STILL_READING;
}

s32 OpenGameBoyPakWithRetries(
    s32 port, OSGbpakId* header, NUContPakFile* file) {
  s32 failureCount;
  s32 result;

  for (failureCount = 0; failureCount != GAME_BOY_PAK_OPEN_ATTEMPTS;
      failureCount++) {
    result = OpenGameBoyPak(port, header, file);
    if (result == PAK_RESULT_OK) {
      return PAK_RESULT_OK;
    }

    nuContGBPakMgrInit();
  }

  return result;
}

/*
 * Opens the Game Boy pak on a port, powers it on if needed and reads the
 * cartridge header, then tells whether the cartridge is Mario Golf's.
 */
s32 OpenGameBoyPak(s32 port, OSGbpakId* header, NUContPakFile* file) {
  Boolean isMarioGolf = TRUE;
  s32 index;
  u8 status;

  switch (nuContGBPakOpen(file, port)) {
    case 0:
      osSyncPrintf("1st GB PACK OK %d\n", port);
      break;
    case PFS_ERR_NOPACK:
      osSyncPrintf("1st GB NOPACK %d\n", port);
      return PAK_RESULT_NO_PAK;
    case PFS_ERR_DEVICE:
      osSyncPrintf("1st GB DEVICE ER %d\n", port);
      return PAK_RESULT_OTHER_PAK;
    default:
      osSyncPrintf("1st GB DEFAULT ERROR %d\n", port);
      return -1;
  }

  switch (nuContGBPakGetStatus(file, &status)) {
    case 0:
    case PFS_ERR_NEW_GBCART:
      if (!(status & OS_GBPAK_POWER)) {
        switch (nuContGBPakPower(file, NU_CONT_GBPAK_POWER_ON)) {
          case 0:
            /* "3rd Power on" */
            osSyncPrintf("3rd 電源ＯＮ\n");
            break;
          case PFS_ERR_NOPACK:
            /* "3rd Nothing is plugged into the given controller" */
            osSyncPrintf(
                "3rd 指定したコントローラに何も差し込まれていません \n");
            return PAK_RESULT_NO_PAK;
          case PFS_ERR_CONTRFAIL:
            /* "3rd Data transfer with the controller failed." */
            osSyncPrintf("3rd コントローラとのデータ転送の失敗です。\n");
            return PAK_RESULT_CONNECTOR_PROBLEM;
        }
      }

      switch (nuContGBPakReadID(file, header, &status)) {
        case PFS_ERR_NEW_GBCART:
          /* "4th The GB cartridge was removed and inserted." */
          osSyncPrintf("4th GBカートリッジの挿抜が行われました。\n");
        case 0:
          if (!(status & OS_GBPAK_GBCART_ON)) {
            /* "5th No cassette is inserted in the GB pak." */
            osSyncPrintf("5th ＧＢパックにカセットが刺さってません。\n");
            return PAK_RESULT_CARTRIDGE_REMOVED;
          }

          if (status & OS_GBPAK_GBCART_PULL) {
            /* "The cassette was removed and inserted." */
            osSyncPrintf("カセットの抜き差しが行われました。\n");
            return PAK_RESULT_REMOVED_RETRYING;
          }

          for (index = 0; index != GAME_BOY_TITLE_CHECKED_LENGTH; index++) {
            if (header->game_title[index] != f_GameBoyCartridgeTitle[index]) {
              isMarioGolf = FALSE;
            }
          }

          if (header->company_code != GAME_BOY_COMPANY_CODE) {
            isMarioGolf = FALSE;
          }

          osSyncPrintf("%s\n", header->game_title);
          osSyncPrintf("%02x\n", header->company_code);
          if (isMarioGolf != TRUE) {
            return PAK_RESULT_WRONG_CARTRIDGE;
          }

          return PAK_RESULT_OK;
        case PFS_ERR_NOPACK:
          /* "4th Nothing is plugged into the given controller" */
          osSyncPrintf("4th 指定したコントローラに何も差し込まれていません\n");
          return PAK_RESULT_NO_PAK;
        case PFS_ERR_DEVICE:
          /* "4th A device other than the 64GB pak is plugged into the
           * controller" */
          osSyncPrintf(
              "4th "
              "64GBパック以外のデバイスがコントローラに差し込まれています\n");
          return PAK_RESULT_OTHER_PAK;
        case PFS_ERR_CONTRFAIL:
          /* "4th Data transfer with the controller failed." */
          osSyncPrintf("4th コントローラとのデータ転送の失敗です。\n");

          /* "The controller is not connected properly, or" */
          osSyncPrintf("コントローラが正しく接続されていないか、\n");

          /* "the 64GB pak or controller connector may be faulty. stat = %xH" */
          osSyncPrintf(
              "64GBパックかコントローラコネクタに異常がある可\224\134性がありま"
              "す。stat = %xH\n",
              status);
          return PAK_RESULT_CONNECTOR_PROBLEM;
        case PFS_ERR_NO_GBCART:
          /* "4th No GB cartridge is inserted in the 64GB pak." */
          osSyncPrintf(
              "4th 64GBパックにGBカートリッジが差し込まれていません。\n");
          return PAK_RESULT_NO_CARTRIDGE;
      }

      break;
    case PFS_ERR_NOPACK:
      /* "2nd Nothing is plugged into the given controller" */
      osSyncPrintf("2nd 指定したコントローラに何も差し込まれていません\n");
      return PAK_RESULT_NO_PAK;
    case PFS_ERR_DEVICE:
      /* "2nd A device other than the 64GB pak is plugged into the controller"
       */
      osSyncPrintf(
          "2nd 64GBパック以外のデバイスがコントローラに差し込まれています\n");
      return PAK_RESULT_OTHER_PAK;
    case PFS_ERR_CONTRFAIL:
      /* "2nd Data transfer with the controller failed." */
      osSyncPrintf("2nd コントローラとのデータ転送の失敗です。\n");
      return PAK_RESULT_CONNECTOR_PROBLEM;
    case PFS_ERR_NO_GBCART:
      /* "2ndt No GB cartridge is inserted in the 64GB pak." */
      osSyncPrintf("2ndt 64GBパックにGBカートリッジが差し込まれていません。\n");
      return PAK_RESULT_NO_CARTRIDGE;
  }

  return -1;
}

/*
 * Writes a random tag to the cartridge RAM of each Game Boy cartridge and lists
 * the controllers whose cartridges took it, so that later accesses can tell
 * whether a cartridge was swapped.
 */
s32 TagCartridges(void) {
  /* The transfer pointer is there to match */
  GameBoyTransfer* transfer = &g_GameBoyTransfer;
  OSGbpakId header;
  NUContPakFile file;
  u8 tag[CARTRIDGE_TAG_SIZE];
  u8 status;
  Boolean isAnyTagged = FALSE;
  Boolean wasNoCartridge = FALSE;
  Boolean wasNoPak = FALSE;
  Boolean wasOtherPak = FALSE;
  Boolean wasWrongCartridge = FALSE;
  Boolean wasCartridgePulled = FALSE;
  s32 controller;
  s32 index;
  s32 result;

  g_GameBoyTransfer.taggedCartridgeCount = 0;
  for (controller = 0; controller != MAXCONTROLLERS; controller++) {
    transfer->taggedControllers[controller] = 0;
  }

  nuContGBPakMgrInit();
  for (controller = 0; controller < g_ControllerCount; controller++) {
    result = OpenGameBoyPakWithRetries(
        g_PlayerControllerPorts[controller], &header, &file);
    if (result == PAK_RESULT_NO_PAK) {
      wasNoPak = TRUE;
    }

    if (result == PAK_RESULT_OTHER_PAK) {
      wasOtherPak = TRUE;
    }

    if (result == PAK_RESULT_NO_CARTRIDGE) {
      wasNoCartridge = TRUE;
    }

    if (result == PAK_RESULT_WRONG_CARTRIDGE) {
      wasWrongCartridge = TRUE;
    }

    if (result == PAK_RESULT_OK) {
      for (index = 0; index != CARTRIDGE_TAG_SIZE; index++) {
        tag[index] = guRandom() % 256;
        transfer->cartridgeTags[controller][index] = tag[index];
      }

      if (nuContGBPakFwrite(
              &file, CARTRIDGE_RAM_ADDRESS, tag, CARTRIDGE_TAG_SIZE) == 0 &&
          nuContGBPakGetStatus(&file, &status) == 0) {
        if (status & OS_GBPAK_GBCART_PULL) {
          wasCartridgePulled = TRUE;
        } else {
          transfer->taggedControllers[transfer->taggedCartridgeCount] =
              controller;
          isAnyTagged = TRUE;
          transfer->taggedCartridgeCount++;
        }
      }
    }

    nuContGBPakPower(&file, NU_CONT_GBPAK_POWER_OFF);
  }

  if (isAnyTagged == TRUE) {
    return PAK_RESULT_OK;
  }

  if (wasCartridgePulled == TRUE) {
    return PAK_RESULT_REMOVED_RETRYING;
  }

  if (wasWrongCartridge == TRUE) {
    return PAK_RESULT_WRONG_CARTRIDGE;
  }

  if (wasNoCartridge == TRUE) {
    return PAK_RESULT_NO_CARTRIDGE;
  }

  if (wasOtherPak == TRUE) {
    return PAK_RESULT_OTHER_PAK;
  }

  if (wasNoPak == TRUE) {
    return PAK_RESULT_NO_PAK;
  }

  return PAK_RESULT_LOOSE_PAK;
}

void SetNoticeState(s32 state, MessageId message) {
  f_NoticeState = state;
  if (state == NOTICE_OPENING) {
    f_NoticeSize = 0;
    f_NoticeMessage = message;
  }

  if (state == NOTICE_CLOSING) {
    f_NoticeSize = NOTICE_FULL_SIZE;
  }
}

s32 GetNoticeState(void) {
  return f_NoticeState;
}

/*
 * Steps the notice cloud's size and draws it from its center, with its message
 * once it is full size.
 */
void DrawNotice(Gfx** displayListHead) {
  Gfx* displayList = *displayListHead;
  s32 strip;
  s32 halfWidth;
  s32 halfHeight;

  /* The copy of halfHeight, and its starting value, are there to match */
  s32 stripHeight = 0;
  s32 bottom;

  if (f_NoticeState == NOTICE_HIDDEN) {
    return;
  }

  if (f_NoticeState == NOTICE_OPENING) {
    if (f_NoticeSize < NOTICE_FULL_SIZE) {
      f_NoticeSize++;
    } else {
      f_NoticeState = NOTICE_SHOWN;
    }
  } else if (f_NoticeState == NOTICE_CLOSING) {
    if (f_NoticeSize > 0) {
      f_NoticeSize--;
    } else {
      f_NoticeState = NOTICE_HIDDEN;
    }
  }

  gDPPipeSync(displayList++);
  gDPPipeSync(displayList++);
  gDPSetTextureLUT(displayList++, G_TT_NONE);
  gDPPipeSync(displayList++);
  gDPSetAlphaDither(displayList++, G_AD_PATTERN);
  gDPPipeSync(displayList++);
  gDPSetTextureFilter(displayList++, G_TF_BILERP);
  gDPPipeSync(displayList++);
  gDPSetRenderMode(displayList++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
  gDPSetCombineLERP(displayList++, 0, 0, 0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0,
      0, 0, 0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0);
  gDPPipeSync(displayList++);
  gDPSetAlphaCompare(displayList++, G_AC_NONE);
  gDPSetPrimColor(displayList++, 0, 0, 255, 255, 255, 240);
  if (f_NoticeSize > 0) {
    for (strip = 0; strip != NOTICE_STRIP_COUNT; strip++) {
      halfHeight = f_NoticeSize * NOTICE_HALF_HEIGHT_STEP;
      halfWidth = f_NoticeSize * NOTICE_HALF_WIDTH_STEP;
      LoadNibbleTextureBlock(&displayList,
          ImageBody(g_GameBoyTransfer.noticeCloudImage) +
              (strip * NOTICE_STRIP_SIZE),
          G_IM_FMT_I, NOTICE_WIDTH, NOTICE_STRIP_HEIGHT, 0, G_TX_CLAMP,
          G_TX_CLAMP, 0, 0, 0, 0);
      stripHeight = halfHeight;
      bottom = (strip * stripHeight) + NOTICE_CENTER_Y;
      DrawTextureRect(&displayList, (NOTICE_CENTER_X - halfWidth) << 2,
          (bottom - stripHeight) << 2, (NOTICE_CENTER_X + halfWidth) << 2,
          bottom << 2, G_TX_RENDERTILE, 0, 0,
          (NOTICE_WIDTH << 10) / 2 / halfWidth,
          (NOTICE_STRIP_HEIGHT << 10) / stripHeight);
    }
  }

  if (f_NoticeState == NOTICE_SHOWN) {
    /* The unused array is there to match */
    s32 unused[2];

    SetTextSlot(NOTICE_CENTER_X, NOTICE_TEXT_TOP, f_NoticeMessage,
        TEXT_SLOT_LARGE_FONT, NOTICE_TEXT_SLOT,
        TEXT_FLAG_CENTERED | TEXT_FLAG_UNK20, 255, 255, 255, 255);
    gDPSetPrimColor(displayList++, 0, 0, 32, 64, 255, 255);
    DrawLargeTextSlot(&displayList, NOTICE_TEXT_SLOT);
  }

  *displayListHead = displayList;
}

void DrawAccessingSign(Gfx** displayListHead) {
  Gfx* displayList = *displayListHead;
  s32 strip;

  LoadLargePalette(&displayList, ImageBody(g_GameBoyTransfer.accessingImage));
  for (strip = 0; strip != ACCESSING_STRIP_COUNT; strip++) {
    LoadTextureBlock(&displayList,
        LargeImageTexels(g_GameBoyTransfer.accessingImage) +
            (strip * ACCESSING_STRIP_SIZE),
        G_IM_FMT_CI, G_IM_SIZ_8b, ACCESSING_WIDTH, ACCESSING_STRIP_HEIGHT, 0, 0,
        0, 0, 0, 0, 0);
    DrawTextureRect(&displayList, ACCESSING_LEFT << 2,
        (ACCESSING_TOP + (strip * ACCESSING_STRIP_HEIGHT)) << 2,
        (ACCESSING_LEFT + ACCESSING_WIDTH) << 2,
        (ACCESSING_TOP + (strip * ACCESSING_STRIP_HEIGHT) +
            ACCESSING_STRIP_HEIGHT)
            << 2,
        G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
  }

  *displayListHead = displayList;
}

void DrawPressingHand(Gfx** displayListHead) {
  Gfx* displayList = *displayListHead;

  f_PressingHandFrame = (f_PressingHandFrame + 1) % PRESSING_HAND_FRAME_COUNT;
  LoadLargePalette(&displayList, ImageBody(g_PressingHandImage));
  LoadTextureBlock(&displayList,
      LargeImageTexels(g_PressingHandImage) +
          (f_PressingHandFrame / PRESSING_HAND_FRAMES_PER_IMAGE *
              PRESSING_HAND_IMAGE_SIZE),
      G_IM_FMT_CI, G_IM_SIZ_8b, PRESSING_HAND_SIZE, PRESSING_HAND_SIZE, 0, 0, 0,
      0, 0, 0, 0);
  if (g_GameBoyTransfer.pressingHandPosition == PRESSING_HAND_LOW) {
    DrawTextureRect(&displayList, LOW_PRESSING_HAND_LEFT << 2,
        LOW_PRESSING_HAND_TOP << 2,
        (LOW_PRESSING_HAND_LEFT + PRESSING_HAND_SIZE) << 2,
        (LOW_PRESSING_HAND_TOP + PRESSING_HAND_SIZE) << 2, G_TX_RENDERTILE, 0,
        0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
  } else {
    DrawTextureRect(&displayList, NOTICE_PRESSING_HAND_LEFT << 2,
        NOTICE_PRESSING_HAND_TOP << 2,
        (NOTICE_PRESSING_HAND_LEFT + PRESSING_HAND_SIZE) << 2,
        (NOTICE_PRESSING_HAND_TOP + PRESSING_HAND_SIZE) << 2, G_TX_RENDERTILE,
        0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
  }

  *displayListHead = displayList;
}

/* The save from unk990 on, which BuildStatusData reads through a pointer to
 * unk990 */
typedef struct SaveRecords {
  Unk990 unk990;
  s32 unkD78[COURSE_COUNT][BEST_TIME_COUNT];
  s32 bestTimes[COURSE_COUNT][BEST_TIME_COUNT];
  s32 unkE08[COURSE_COUNT][BEST_TIME_COUNT];
  s32 unkE50[COURSE_COUNT][BEST_TIME_COUNT];
  UnkE98 unkE98[COURSE_COUNT];
  u8 unk1150[0x1320 - 0x1150];
  s8 hasRingShotStar[RING_SHOT_COURSE_COUNT][RING_SHOT_HOLE_COUNT];
} SaveRecords;

/* Returns value rounded to tenths, times ten, up to MAX_STATUS_VALUE and nine
 * tenths */
static inline s32 PackTenths(f32 value) {
  s32 wholePart = value;
  s32 fraction = (value - wholePart) * 100.0f;

  if (fraction % 10 >= 5) {
    fraction += 10;
  }

  fraction /= 10;
  if (fraction >= 10) {
    wholePart++;
    fraction = 0;
    if (wholePart >= MAX_STATUS_VALUE + 1) {
      wholePart = MAX_STATUS_VALUE;
      fraction = 9;
    }
  }

  return (wholePart * 10) + fraction;
}

/*
 * Fills status with the save's course records, best times, badges and stars, in
 * the layout of the Game Boy cartridge's status data.
 */
void BuildStatusData(u8* status) {
  StatusData* data = (StatusData*)status;
  SaveData* save = GetSaveData();
  StatusCourse* course;
  StatusProgress* progress;

  /* Reading the records through a pointer to unk990 is there to match */
  SaveRecords* records = (SaveRecords*)&save->unk990;
  UnkE98* entry;
  UnkE98* scores;
  f32 reciprocal;
  s32 value;
  s32 sum;
  CourseIndex courseIndex;
  s32 index;
  s32 bits[2];
  s32 row;
  s32 openCourseCount;

  memset(status, 0, STATUS_CLEARED_SIZE);
  for (courseIndex = 0; courseIndex != COURSE_COUNT; courseIndex++) {
    entry = &save->unkE98[courseIndex];
    course = &data->courses[courseIndex];

    /* Testing for zero first is there to match */
    if (entry->roundCount != 0) {
      if (entry->roundCount > 0) {
        reciprocal = 1.0f / entry->roundCount;

        /* Copying the count into value is there to match */
        value = entry->roundCount;
        course->roundCount[0] = value;
        course->roundCount[1] = value >> 8;
        course->bestScore = entry->bestScore + STATUS_SCORE_BIAS;
        course->averageStrokes =
            RoundedWholePart(entry->strokeTotal * reciprocal);
        value = RoundedWholePart(entry->longestDrive * UNK_SCALE);
        course->longestDrive[0] = value;
        course->longestDrive[1] = value >> 8;
        value = RoundedWholePart(entry->averageDrive * UNK_SCALE);
        course->averageDrive[0] = value;
        course->averageDrive[1] = value >> 8;
        course->unk8 = RoundedWholePart(entry->measuredDriveTotal * reciprocal *
            (100.0f / HOLES_PER_ROUND));
        course->averagePutts = PackTenths(
            entry->puttTotal * reciprocal * (1.0f / HOLES_PER_ROUND));
        course->parOrBetterRate = RoundedWholePart(
            entry->parOrBetterCount * reciprocal * (100.0f / HOLES_PER_ROUND));
        course->parOnRate = RoundedWholePart(
            entry->parOnCount * reciprocal * (100.0f / HOLES_PER_ROUND));
        sum = entry->sandSaveCount + entry->missedSandSaveCount;
        if (sum == 0) {
          course->unkC = 0;
        } else {
          course->unkC =
              RoundedWholePart(((f32)entry->sandSaveCount / (f32)sum) * 100.0f);
        }

        course->places = 0;

        /* Setting the first bit with |= is there to match */
        if (entry->hasPlace[THIRD_PLACE_INDEX] > 0) {
          course->places |= STATUS_THIRD_PLACE;
        }

        if (entry->hasPlace[SECOND_PLACE_INDEX] > 0) {
          course->places |= STATUS_SECOND_PLACE;
        }

        if (entry->hasPlace[FIRST_PLACE_INDEX] > 0) {
          course->places |= STATUS_FIRST_PLACE;
        }

        for (index = 0; index != CHARACTER_COUNT; index++) {
          if (entry->hasCharacterBestScore[index] == TRUE) {
            course->characterBestScores[index] =
                entry->characterBestScores[index] + STATUS_SCORE_BIAS;
          } else {
            course->characterBestScores[index] = 0;
          }
        }
      }
    }

    for (index = 0; index != BEST_TIME_COUNT; index++) {
      if (records->bestTimes[courseIndex][index] > 0) {
        /* Passing the parts through value is there to match */
        value = records->bestTimes[courseIndex][index] / FRAMES_PER_MINUTE;
        course->bestTimes[index].minutes = value;
        value = records->bestTimes[courseIndex][index] / FRAMES_PER_SECOND %
            SECONDS_PER_MINUTE;
        course->bestTimes[index].seconds = value;
        value = (s32)(records->bestTimes[courseIndex][index] *
                    ((f32)HUNDREDTHS_PER_SECOND / FRAMES_PER_SECOND)) %
            HUNDREDTHS_PER_SECOND;
        course->bestTimes[index].hundredths = value;
        course->bestTimes[index].unk3 = records->unkE08[courseIndex][index];
        course->bestTimes[index].unk4 =
            records->unkD78[courseIndex][index] + STATUS_SCORE_BIAS;
      } else {
        course->bestTimes[index].minutes = 0;
        course->bestTimes[index].seconds = 0;
        course->bestTimes[index].hundredths = 0;
        course->bestTimes[index].unk3 = 0;
        course->bestTimes[index].unk4 = 0;
      }
    }

    course->birdieBadges[0] = 0;
    course->birdieBadges[1] = 0;
    course->birdieBadges[2] = 0;
    for (index = 0; index != 8; index++) {
      if (records->unk990.birdieBadgeCounts[courseIndex][index] > 0) {
        course->birdieBadges[0] |= 1 << (7 - index);
      }
    }

    for (index = 0; index != 8; index++) {
      if (records->unk990.birdieBadgeCounts[courseIndex][8 + index] > 0) {
        course->birdieBadges[1] |= 1 << (7 - index);
      }
    }

    for (index = 0; index != 2; index++) {
      if (records->unk990.birdieBadgeCounts[courseIndex][16 + index] > 0) {
        course->birdieBadges[2] |= 1 << (7 - index);
      }
    }
  }

  ComputeRoundStatistics();
  course = &data->allCourses;
  value = D_80105C38[COURSE_COUNT].roundCount;
  if (value > 0) {
    course->roundCount[0] = value;
    course->roundCount[1] = value >> 8;
    course->bestScore =
        (s32)D_80105C38[COURSE_COUNT].bestScore + STATUS_SCORE_BIAS;
    course->averageStrokes =
        RoundedWholePart(D_80105C38[COURSE_COUNT].averageScore);
    /* Holding the drives in sum, and the scores below in scores, is there to
     * match */
    sum = RoundedWholePart(D_80105C38[COURSE_COUNT].bestDrive);
    course->longestDrive[0] = sum;
    course->longestDrive[1] = sum >> 8;
    sum = RoundedWholePart(D_80105C38[COURSE_COUNT].averageDrive);
    course->averageDrive[0] = sum;
    course->averageDrive[1] = sum >> 8;
    course->unk8 = RoundedWholePart(D_80105C38[COURSE_COUNT].fairwayPercent);
    course->averagePutts = PackTenths(D_80105C38[COURSE_COUNT].averagePutts);
    course->parOrBetterRate =
        RoundedWholePart(D_80105C38[COURSE_COUNT].parSavePercent);
    course->parOnRate = RoundedWholePart(D_80105C38[COURSE_COUNT].parOnPercent);
    course->unkC = RoundedWholePart(D_80105C38[COURSE_COUNT].sandSavePercent);
    course->places = 0;
    for (index = 0; index != CHARACTER_COUNT; index++) {
      for (courseIndex = 0; courseIndex != COURSE_COUNT; courseIndex++) {
        scores = &save->unkE98[courseIndex];
        if (scores->hasCharacterBestScore[index] == TRUE) {
          course->characterBestScores[index] =
              scores->characterBestScores[index] + STATUS_SCORE_BIAS;
          break;
        }
      }

      if (courseIndex < COURSE_COUNT) {
        for (courseIndex = 0; courseIndex != COURSE_COUNT; courseIndex++) {
          scores = &save->unkE98[courseIndex];
          if (scores->hasCharacterBestScore[index] == TRUE &&
              course->characterBestScores[index] >
                  scores->characterBestScores[index] + STATUS_SCORE_BIAS) {
            course->characterBestScores[index] =
                scores->characterBestScores[index] + STATUS_SCORE_BIAS;
          }
        }
      } else {
        course->characterBestScores[index] = 0;
      }
    }

    for (index = 0; index != BEST_TIME_COUNT; index++) {
      course->bestTimes[index].minutes = 0;
      course->bestTimes[index].seconds = 0;
      course->bestTimes[index].hundredths = 0;
      course->bestTimes[index].unk3 = 0;
      course->bestTimes[index].unk4 = 0;
    }

    course->birdieBadges[0] = 0;
    course->birdieBadges[1] = 0;
    course->birdieBadges[2] = 0;
  }

  progress = &data->progress;
  openCourseCount = CountOpenCourses(save->coursePoints);
  progress->openCourses = 0;
  for (courseIndex = 0; courseIndex != openCourseCount; courseIndex++) {
    /* The original keeps only the last course's bit */
    progress->openCourses = 1 << courseIndex;
  }

  for (courseIndex = 0; courseIndex != RING_SHOT_COURSE_COUNT; courseIndex++) {
    progress->ringShotStars[courseIndex] = 0;
    for (index = 0; index != RING_SHOT_HOLE_COUNT; index++) {
      if (records->hasRingShotStar[courseIndex][index] != 0) {
        progress->ringShotStars[courseIndex] |= 1 << index;
      }
    }
  }

  progress->birdieBadgeCount = 0;
  for (courseIndex = 0; courseIndex != COURSE_COUNT; courseIndex++) {
    for (index = 0; index != COURSE_HOLE_COUNT; index++) {
      if (records->unk990.birdieBadgeCounts[courseIndex][index] != 0) {
        progress->birdieBadgeCount++;
      }
    }
  }

  for (courseIndex = 0; courseIndex != SAVE_UNKAF0_ROW_COUNT; courseIndex++) {
    /* The bits array, zeroed from its end, and courseIndex as row index are
     * there to match */
    row = f_StatusHoleOrder[courseIndex];
    bits[1] = 0;
    bits[0] = 0;
    for (index = 0; index != STATUS_UNKAF0_COLUMN_COUNT; index++) {
      if (records->unk990.unkAF0[row][index].unk1 != 0) {
        bits[0] |= 1 << index;
      }

      if (records->unk990.unkAF0[row][index].unk1 & UNKAF0_FLAG) {
        bits[1] |= 1 << index;
      }
    }

    progress->unkAF0Bits[courseIndex].nonzeroBits[0] = bits[0];
    progress->unkAF0Bits[courseIndex].nonzeroBits[1] = bits[0] >> 8;
    progress->unkAF0Bits[courseIndex].flagBits[0] = bits[1];
    progress->unkAF0Bits[courseIndex].flagBits[1] = bits[1] >> 8;
  }
}

/* The whole part of value once rounded to tenths, up to MAX_STATUS_VALUE */
s32 RoundedWholePart(f32 value) {
  s32 wholePart = value;

  /* The fraction in hundredths, then in tenths; reusing one variable is there
   * to match */
  s32 fraction = (value - wholePart) * 100.0f;

  if (fraction % 10 >= 5) {
    fraction += 10;
  }

  fraction /= 10;
  if (fraction >= 10) {
    wholePart++;
    if (wholePart >= MAX_STATUS_VALUE + 1) {
      wholePart = MAX_STATUS_VALUE;
    }
  }

  return wholePart;
}
