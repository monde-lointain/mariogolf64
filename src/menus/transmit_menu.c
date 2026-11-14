/*
 * The Transmit menu, which brings characters over from Game Boy cartridges and
 * writes status and experience to them.
 */

#include "menus/transmit_menu.h"

#include <string.h>
#include <ultra64.h>

#include "audio/sound.h"
#include "characters/character.h"
#include "controllers/controller.h"
#include "controllers/transfer_pak.h"
#include "course/course.h"
#include "course/course_space.h"
#include "debug/debug_text.h"
#include "debug/game_flags.h"
#include "game/scene.h"
#include "math/math_util.h"
#include "menus/cursor_box.h"
#include "menus/menu.h"
#include "save/course_stats.h"
#include "save/save.h"
#include "screen/display_list.h"
#include "screen/screen.h"
#include "screen/screen_layers.h"
#include "shot/club.h"
#include "system/asset.h"
#include "system/c_language.h"
#include "system/compiler.h"
#include "system/frame_time.h"
#include "system/heap.h"
#include "system/host_file.h"
#include "system/memory_address.h"
#include "system/overlay_manager.h"
#include "text/text.h"

// The options of each transfer mode
#define TRANSFER_MODE_COUNT 2
#define MODE_OPTION_COUNT 3

// The rows of f_ModeOptions: Send Status in both modes, then Bring Characters
// at boot or Send Experience from the menu, then the main menu at boot only
#define STATUS_OPTION_ROW 0
#define SECOND_OPTION_ROW 1
#define MAIN_MENU_OPTION_ROW 2

// f_ModeOptions entries, counted from the message naming each
#define OPTION_ROW_FLIP 1
#define MESSAGE_FIRST_TRANSMIT_OPTION 0x13F

enum TransmitOption {
    TRANSMIT_OPTION_NONE = -1,
    TRANSMIT_OPTION_STATUS,
    TRANSMIT_OPTION_EXPERIENCE,
    TRANSMIT_OPTION_CHARACTERS,
    TRANSMIT_OPTION_MAIN_MENU
};

typedef s32 TransmitOption;

// The hint below each option, counted the same way
#define MESSAGE_FIRST_TRANSMIT_OPTION_HINT 0x143

// screenStep values on the Bring Characters screen
enum BringStep {
    BRING_READING,
    BRING_CHOOSING = 2,
    // Whether to bring more characters
    BRING_BRINGING,
    // Never set; its arm does nothing
    BRING_ASKING,
    BRING_UNUSED,
    BRING_BROUGHT = 7,
    // The character to replace
    BRING_CHOOSING_SLOT,
    BRING_NO_CHARACTERS = 10,
    // Asking the player to start a pak check
    BRING_CONFIRMING = 12,
    BRING_STARTING,
    BRING_SHOWING_ERROR,
    BRING_NEXT_CARTRIDGE = 16
};

typedef s32 BringStep;
#define BRINGING_FRAME_COUNT 16

// screenStep values on the Send Status screen
enum SendStatusStep {
    SEND_STATUS_SHOWING_ERROR, // Or the canceled pak check, closing
    SEND_STATUS_CHOOSING,
    SEND_STATUS_READY = 3,
    SEND_STATUS_CHECKING,
    SEND_STATUS_SENT,
    SEND_STATUS_ASKING = 8, // Whether to overwrite the cartridge's status
    SEND_STATUS_STARTING,
    SEND_STATUS_CONFIRMING, // Asking the player to start a pak check
    SEND_STATUS_TAGGING,
    SEND_STATUS_WRITING
};

// screenStep values on the Send Experience screen
enum SendExperienceStep {
    SEND_EXPERIENCE_CHOOSING,
    SEND_EXPERIENCE_SENT = 3,
    SEND_EXPERIENCE_WRITING,
    // Never set; each would end like a failure
    SEND_EXPERIENCE_UNUSED_A,
    SEND_EXPERIENCE_UNUSED_B,
    SEND_EXPERIENCE_UNUSED_C,
    SEND_EXPERIENCE_FAILED,
    SEND_EXPERIENCE_DONE,
    // Asking the player to start a pak check
    SEND_EXPERIENCE_CONFIRMING,
    SEND_EXPERIENCE_NOTHING_TO_SEND
};

// f_NoticeState: the notice cloud grows, shows its message, then shrinks
enum NoticeState {
    NOTICE_HIDDEN,
    NOTICE_OPENING,
    NOTICE_CLOSING,
    NOTICE_SHOWN
};

typedef s32 NoticeState;

enum PakResult {
    PAK_RESULT_STILL_READING = -1,
    PAK_RESULT_UNKNOWN_ERROR = PAK_RESULT_STILL_READING,
    PAK_RESULT_CONNECTOR_PROBLEM,
    PAK_RESULT_OK,
    PAK_RESULT_NO_CHARACTERS,
    PAK_RESULT_NO_PAK,
    PAK_RESULT_NO_CARTRIDGE,
    PAK_RESULT_WRONG_CARTRIDGE,
    PAK_RESULT_OTHER_PAK,
    PAK_RESULT_LOOSE_PAK,
    PAK_RESULT_REMOVED_RETRYING,
    PAK_RESULT_CARTRIDGE_REMOVED,
    PAK_RESULT_CORRUPTED,
    PAK_RESULT_CARTRIDGE_REPLACED,
    PAK_RESULT_UNINITIALIZED
};

typedef s32 PakResult;
#define MESSAGE_INSERT_TRANSFER_PAK 0x149
#define MESSAGE_SWAP_FOR_TRANSFER_PAK 0x148
#define MESSAGE_LOOSE_TRANSFER_PAK 0x14A
#define MESSAGE_NO_CARTRIDGE 0x14C
#define MESSAGE_WRONG_CARTRIDGE 0x14D
#define MESSAGE_CARTRIDGE_REMOVED 0x14E
#define MESSAGE_RETRYING_AFTER_REMOVAL 0x14F
#define MESSAGE_CONNECTOR_PROBLEM 0x151
#define MESSAGE_INSERT_RUMBLE_PAK 0x155
#define MESSAGE_CORRUPTED_SAVE 0x152
#define MESSAGE_UNINITIALIZED_SAVE 0x153
#define MESSAGE_SWAP_GAME_BOY_PAKS 0x154
#define MESSAGE_CARTRIDGE_REPLACED 0x156
#define MESSAGE_PAK_CHECK 0x147
#define MESSAGE_NO_EXPERIENCE 0x157
#define MESSAGE_CHOOSE_EXPERIENCE 0x158
#define MESSAGE_EXPERIENCE_SENT 0x159
#define MESSAGE_NOT_FROM_CARTRIDGE 0x15A
#define MESSAGE_CHOOSE_BROUGHT_CHARACTER 0x15C

// The hints below the status screen
#define MESSAGE_CHARACTER_BROUGHT 0x15D
#define MESSAGE_CHOOSE_SWAPPED_CHARACTER 0x15E
#define MESSAGE_NO_CHARACTERS 0x15F
#define MESSAGE_CHOOSE_CARTRIDGE 0x160
#define MESSAGE_KEEP_POWER_ON 0x161
#define MESSAGE_SEND_STATUS 0x162
#define HINT_LEFT 28
#define HINT_TOP 186
#define HINT_TEXT_SLOT 3

// DrawCharacterStats' layout
#define STATS_NAME_LEFT 192
#define STATS_NAME_TOP 140
// Of both the stat labels and the stats
#define STATS_LEFT 188
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
// From a fade of 4 to a draw of 4
#define CURVE_SEGMENT_COUNT 9

// The rows of the shot types image
enum ShotType {
    SHOT_TYPE_STRAIGHT,
    SHOT_TYPE_DRAW,
    SHOT_TYPE_FADE,
    SHOT_TYPE_COUNT
};

typedef s32 ShotType;

// The faces of the characters brought over, along the bottom
#define FACES_LEFT 24
#define FACES_TOP 136
#define FACE_SPACING 40
#define FACE_CURSOR_MARGIN 9

// The listed cartridge characters on the Bring Characters screen, each above
// its controller
#define LISTED_FACES_LEFT 28
#define LISTED_FACES_TOP 56
#define LISTED_FACE_SPACING 46
#define LISTED_ICONS_TOP 88
#define SHOWN_LISTED_COUNT 6
#define BRING_CURSOR_MARGIN 8

// The arrows beside the list when it scrolls, which show each frame for four
// updates
#define ARROW_FRAME_TICKS 4
#define ARROW_CYCLE_TICKS (GREEN_ARROW_FRAME_COUNT * ARROW_FRAME_TICKS)
#define ARROWS_TOP 92
#define LEFT_ARROW_LEFT 16
#define RIGHT_ARROW_LEFT 288
#define TRANSFER_PAK_ICONS_LEFT 60
#define TRANSFER_PAK_ICONS_TOP 72
#define TRANSFER_PAK_ICON_SPACING 56
#define CONTROLLER_CURSOR_MARGIN 8
// The pointer at the chosen cartridge on the Send Status screen
#define POINTER_TOP 76
// Both pointers swing out and back over half a turn of their angle
#define POINTER_SWING_STEP DegreesToRadians(10)
#define POINTER_SWING_DISTANCE 8.0f
// Below each face
#define EXPERIENCE_LEFT 20
#define EXPERIENCE_TOP 174
// The options, centered above the hint, with the pointer at the left of the
// chosen one
#define OPTIONS_CENTER_X (SCREEN_WIDTH / 2)
#define OPTIONS_TOP 64
#define OPTION_SPACING 18
#define OPTION_POINTER_TOP 60
// Between the pointer at rest and the chosen option's name
#define OPTION_POINTER_GAP 8
// The backdrop: the course scene at double size in strips, then the title and
// the wood panel
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

// The notice cloud grows by one step each time it is drawn, up to this size
#define NOTICE_FULL_SIZE 4
// The cloud is drawn in two strips about its center, its half-size growing by a
// step per size
#define NOTICE_CENTER_X (SCREEN_WIDTH / 2)
#define NOTICE_CENTER_Y 116
#define NOTICE_WIDTH 256
#define NOTICE_STRIP_HEIGHT 32
#define NOTICE_STRIP_COUNT 2
#define NOTICE_STRIP_SIZE (NOTICE_WIDTH * NOTICE_STRIP_HEIGHT / 2)
#define NOTICE_HALF_WIDTH_STEP 34
#define NOTICE_HALF_HEIGHT_STEP 16
#define NOTICE_TEXT_TOP 110
#define NOTICE_TEXT_SLOT 3

// The Accessing sign, drawn in strips
#define ACCESSING_LEFT 52
#define ACCESSING_TOP 64
#define ACCESSING_WIDTH 216
#define ACCESSING_STRIP_HEIGHT 9
#define ACCESSING_STRIP_COUNT 10
#define ACCESSING_STRIP_SIZE (ACCESSING_WIDTH * ACCESSING_STRIP_HEIGHT)

// Where the hand pressing the A button waits beside a notice
#define NOTICE_PRESSING_HAND_LEFT 256
#define NOTICE_PRESSING_HAND_TOP 128

// Where the Game Boy cartridge's RAM appears through the Transfer Pak
#define CARTRIDGE_RAM_ADDRESS 0xA000
#define CARTRIDGE_RAM_SIZE 0x2000

// ReadCartridgeCharacters takes a phase of steps for each controller: it opens
// the cartridge, then reads one save a step
#define CARTRIDGE_READ_PHASE_SIZE (CARTRIDGE_SAVE_COUNT + 1)
#define CARTRIDGE_OPENING_STEP 0
#define CARTRIDGE_FIRST_SAVE_STEP 1
#define CartridgeReadController(readStep) ((readStep) / CARTRIDGE_READ_PHASE_SIZE)
#define CartridgeReadPhaseStep(readStep) ((readStep) % CARTRIDGE_READ_PHASE_SIZE)
#define LastCartridgeReadStep(controllerCount) (((controllerCount) * CARTRIDGE_READ_PHASE_SIZE) - 1)

// The largest value the status sends for each figure
#define STATUS_VALUE_MAX 999

// The status rounds its figures to tenths through hundredths, rounding the
// hundredths half up
#define HUNDREDTHS_PER_ONE 100.0f
#define HUNDREDTHS_PER_TENTH 10
#define HALF_TENTH_HUNDREDTHS 5
#define TENTHS_PER_ONE 10

// The status data BuildStatusData fills for the Game Boy cartridge; its values
// are little-endian
#define STATUS_CLEARED_SIZE 0x400
// Scores are sent raised by this, to keep them positive
#define STATUS_SCORE_BIAS 54
#define STATUS_THIRD_PLACE (1 << 0)
#define STATUS_SECOND_PLACE (1 << 1)
#define STATUS_FIRST_PLACE (1 << 2)
// The computer characters sent, those before the Game Boy characters
#define STATUS_MATCHUP_COLUMN_COUNT 14

typedef struct StatusTime {
    u8 minutes;
    u8 seconds;
    u8 hundredths;
    UCharacterIndex8 characterIndex;
    u8 scoreToPar; // Plus STATUS_SCORE_BIAS
} StatusTime;

// The status packs a course's birdie badges one bit per hole, eight holes to a
// byte, from the top bit of the first byte
#define BIRDIE_BADGE_BYTE_COUNT ((COURSE_HOLE_COUNT + BITS_PER_BYTE - 1) / BITS_PER_BYTE)
#define BirdieBadgeBit(holeInByte) (1 << (BITS_PER_BYTE - 1 - (holeInByte)))

// One course's figures in the status data, or every course's together
typedef struct StatusCourse {
    u8 roundCount[2];
    u8 bestScore;
    u8 scoreAverage;
    u8 longestDrive[2];
    u8 driveAverage[2];
    u8 fairwayPercent; // As a percentage
    u8 puttAverage; // In tenths, from PackTenths
    u8 parSavePercent; // As percentages
    u8 parOnPercent;
    u8 sandSavePercent;
    u8 places; // STATUS_*_PLACE bits
    u8 characterBestScores[CHARACTER_COUNT];
    StatusTime bestTimes[BEST_TIME_COUNT];
    u8 birdieBadges[BIRDIE_BADGE_BYTE_COUNT]; // See BirdieBadgeBit
} StatusCourse;

// One bit per computer character, for one of the player's characters
typedef struct StatusMatchupBits {
    u8 wonBits[2];
    u8 wonUnsavedBits[2];
} StatusMatchupBits;

// What the player has opened and won, in the status data
typedef struct StatusProgress {
    u8 openCourses;
    u8 ringShotStars[RING_SHOT_COURSE_COUNT]; // One bit per hole
    u8 birdieBadgeCount;
    StatusMatchupBits matchupBits[CHARACTER_COUNT];
} StatusProgress;

typedef struct StatusData {
    StatusCourse courses[COURSE_COUNT];
    StatusCourse allCourses;
    StatusProgress progress;
} StatusData;

// A Game Boy save in the cartridge RAM
#define CARTRIDGE_SAVE_SIZE 0x200
#define SAVE_CHECKSUM_OFFSET 0x30
// The save's 16-bit values are little-endian
#define SAVE_HALFWORD_SIZE 2
#define SAVE_HAS_STATUS_OFFSET 0xA0
#define SAVE_STATUS_ADDRESS_OFFSET 0xA2
#define SAVE_STATUS_SIZE_OFFSET 0xA4
#define SAVE_STATUS_CHECKSUM_OFFSET 0xA6
#define STATUS_ADDRESS_BIAS 0x6000
#define STATUS_DATA_SIZE 0x800

#define SAVE_SIGNATURE_OFFSET 0x20
// Records of where the save's data is: one per Game Boy character, then the
// status's
#define SAVE_RECORDS_OFFSET 0x40
#define SAVE_RECORD_SIZE 0x20
#define RECORD_ADDRESS_OFFSET 2
#define RECORD_SIZE_OFFSET 4
#define RECORD_CHECKSUM_OFFSET 6
// A Game Boy character's data
#define CHARACTER_DATA_SIZE 0x800
#define DATA_SAVE_KEY_OFFSET 0xB
#define DATA_CHARACTER_OFFSET 0x10
#define DATA_IS_LEFTY_OFFSET 0x11
#define DATA_LEVEL_OFFSET 0x12
// A little-endian halfword
#define DATA_DRIVE_DISTANCE_OFFSET 0x1A
#define DATA_CLUB_PITCH_OFFSET 0x1C
#define DATA_SHOT_CURVE_OFFSET 0x1D
#define DATA_LIE_RATING_LEVEL_OFFSET 0x1E
#define DATA_MISHIT_RESISTANCE_OFFSET 0x1F
// Three little-endian bytes, copied to GameBoyCharacter.ignoredValue
#define DATA_IGNORED_VALUE_OFFSET 0x26
#define DATA_BALL_TYPE_OFFSET 0x3E
// GAME_BOY_CLUB_COUNT clubs' stats, each two little-endian halfwords and
// two bytes
#define DATA_CLUBS_OFFSET 0x100
#define DATA_CLUB_SIZE 6
#define CLUB_DATA_PITCH_OFFSET 2
#define CLUB_DATA_IGNORED_STAT_OFFSET 4
#define CLUB_DATA_SIDE_SPIN_OFFSET 5
#define DATA_EXPERIENCE_OFFSET 0x15C
// The byte after the experience, which the transfer sets to
// EXPERIENCE_ADDED_MARK whenever it writes the experience back; the Game Boy
// version's use of it is unknown
#define DATA_EXPERIENCE_MARK_OFFSET 0x15E
#define EXPERIENCE_ADDED_MARK 100
// The experience and the byte after it
#define DATA_EXPERIENCE_WRITE_SIZE 3
#define EXPERIENCE_MAX 0xFFFF

// What was wrong with the character's cartridge, reported when it was the only
// one found
enum CartridgeFault {
    CARTRIDGE_FAULT_NONE,
    CARTRIDGE_FAULT_UNINITIALIZED, // Its save lacks the signature
    CARTRIDGE_FAULT_CORRUPTED
};

typedef s32 CartridgeFault;

// How far ReadCartridgeCharacters has checked a save
#define NOT_CHECKED 0
// Its signature and checksum
#define SAVE_CHECKED 1
// Its character data's sum too
#define DATA_CHECKED 2
#define SAVE_READ_ATTEMPTS 5
#define DATA_READ_ATTEMPTS 3
// The first byte of a record in use
#define RECORD_PRESENT 1

// What CheckStatusCartridge found
enum StatusCheck {
    STATUS_CHECK_NO_STATUS,
    STATUS_CHECK_FAILED, // With the cause in transferResult
    STATUS_CHECK_HAS_STATUS,
    STATUS_CHECK_READ_FAILED // With the error in transferResult
};

typedef s32 StatusCheck;

// What WriteStatusToCartridge did
enum StatusWriteResult {
    STATUS_WRITE_DONE,
    STATUS_WRITE_PAK_ERROR, // With the error in transferResult
    STATUS_WRITE_FAILED // With the cause in transferResult
};

typedef s32 StatusWriteResult;

// What WriteExperienceToCartridge did
enum ExperienceWriteResult {
    EXPERIENCE_WRITE_DONE,
    EXPERIENCE_WRITE_NOT_FROM_CARTRIDGE,
    EXPERIENCE_WRITE_FAILED, // With the cause in transferResult
    EXPERIENCE_WRITE_PAK_ERROR // With the error in transferResult
};

typedef s32 ExperienceWriteResult;
#define ASSET_WOOD_PANEL 0x8FD
#define ASSET_TRANSFER_PAK_ICONS 0x900
#define TRANSFER_PAK_ICON_SIZE 32
#define ASSET_TITLE 0x93F
#define ASSET_CURVE_SEGMENTS 0x8EE
#define ASSET_STAT_BARS 0x95D
#define ASSET_ACCESSING 0x96B

#define FIRST_GAME_BOY_GOLFER CHARACTER_KID

PakResult ReadCartridgeCharacters(void);
void BringOverCharacter(s32 listedIndex, s32 gameBoyCharacterIndex);
void ListPresentCharacters(void);
void ClearCartridgeCharacters(void);
void ResetSendStatus(void);
void ResetSendExperience(void);
PakResult TagCartridges(void);

// Results of opening and accessing the Game Boy cartridges, after the notice
// each shows
// ReadCartridgeCharacters has more saves to read
StatusCheck CheckStatusCartridge(s32 cartridgeIndex);
StatusWriteResult WriteStatusToCartridge(s32 cartridgeIndex);
ExperienceWriteResult WriteExperienceToCartridge(s32 gameBoyCharacterIndex);
void SetNoticeState(NoticeState state, MessageId messageId);
NoticeState GetNoticeState(void);
#define GAME_BOY_PAK_OPEN_ATTEMPTS 3

// OpenGameBoyPak knows Mario Golf's cartridge by its title's start and
// Nintendo's licensee code
#define GAME_BOY_TITLE_CHECKED_LENGTH 14
// The characters "01"
#define GAME_BOY_COMPANY_CODE 0x3031

// g_GameBoyTransfer.pressingHandImage under a scalar name. Reading it as
// a scalar is there to match: GCC keeps a struct field's constant address
// in a register when it is used twice. In DrawPressingHand, reading the
// field gives 24 words differing (12 in shape), and copying it to a local
// first 28 (6).
extern ImageAsset *g_PressingHandImage;

void DrawImportedFaces(Gfx **displayListHead, GameBoyTransfer *transfer);

PakResult OpenGameBoyPak(s32 port, OSGbpakId *header, NUContPakFile *file);
PakResult OpenGameBoyPakWithRetries(s32 port, OSGbpakId *header, NUContPakFile *file);

s32 RoundedWholePart(f32 value);

// The buttons of every controller together, which any player may use on the
// menu
static ControllerState f_TransmitController;

// Where DumpCartridgeRam prints whether each stage succeeded
#define DUMP_STATUS_COLUMN 10
#define DUMP_OPEN_ROW 3
#define DUMP_READ_ROW 4
#define DUMP_WRITE_ROW 5

// Debug code that writes the cartridge RAM of controller 1's Game Boy cartridge
// to the host
void DumpCartridgeRam(void) {
    // The transfer pointer is there to match: reading g_GameBoyTransfer
    // directly gives 7 words differing, 1 in shape
    GameBoyTransfer *transfer = &g_GameBoyTransfer;
    u8 cartridgeRam[CARTRIDGE_RAM_SIZE];

    nuContGBPakMgrInit();
    PrintDebugText("          ", DUMP_STATUS_COLUMN, DUMP_OPEN_ROW);
    PrintDebugText("          ", DUMP_STATUS_COLUMN, DUMP_READ_ROW);
    PrintDebugText("          ", DUMP_STATUS_COLUMN, DUMP_WRITE_ROW);

    // The original takes 0 from OpenGameBoyPakWithRetries as success, which is
    // not PAK_RESULT_OK
    if (OpenGameBoyPakWithRetries(0, &transfer->cartridgeHeader, &transfer->pakFile) ==
        PAK_RESULT_CONNECTOR_PROBLEM) {
        PrintDebugText("GOLF OK", DUMP_STATUS_COLUMN, DUMP_OPEN_ROW);
        if (nuContGBPakFread(
                &transfer->pakFile,
                CARTRIDGE_RAM_ADDRESS,
                cartridgeRam,
                CARTRIDGE_RAM_SIZE
            ) == PAK_ACCESS_OK) {
            PrintDebugText("READ OK", DUMP_STATUS_COLUMN, DUMP_READ_ROW);
            if (WriteHostFile("x:/golf/capture/gbsram.bin", CARTRIDGE_RAM_SIZE) == HOST_FILE_OK) {
                PrintDebugText("WRITE OK", DUMP_STATUS_COLUMN, DUMP_WRITE_ROW);
            }
        }
    }
}

// Loads the menu's images, and the faces and palettes of the Game Boy golfers,
// then starts the menu's song and stops any rumble.
void LoadTransmitMenu(void) {
    AssetFile file;
    EncodedAsset asset;
    s32 index;
    s32 size;

    ReadEncodedAsset(
        g_GameBoyTransfer.woodPanelImage = AllocMemory(OpenEncodedAsset(ASSET_WOOD_PANEL, &asset)),
        &asset
    );
    ReadEncodedAsset(
        g_GameBoyTransfer.courseSceneImage =
            AllocMemory(OpenEncodedAsset(ASSET_COURSE_SCENE, &asset)),
        &asset
    );
    ReadEncodedAsset(
        g_GameBoyTransfer.pointerImage = AllocMemory(OpenEncodedAsset(ASSET_POINTING_HAND, &asset)),
        &asset
    );
    ReadEncodedAsset(
        g_GameBoyTransfer.arrowsImage = AllocMemory(OpenEncodedAsset(ASSET_GREEN_ARROWS, &asset)),
        &asset
    );
    ReadEncodedAsset(
        g_GameBoyTransfer.transferPakIconsImage =
            AllocMemory(OpenEncodedAsset(ASSET_TRANSFER_PAK_ICONS, &asset)),
        &asset
    );
    ReadEncodedAsset(
        g_GameBoyTransfer.titleImage = AllocMemory(OpenEncodedAsset(ASSET_TITLE, &asset)),
        &asset
    );
    ReadEncodedAsset(
        g_GameBoyTransfer.cursorBoxImage = AllocMemory(OpenEncodedAsset(ASSET_RAINBOW_BOX, &asset)),
        &asset
    );
    ReadEncodedAsset(
        g_GameBoyTransfer.curveSegmentsImage =
            AllocMemory(OpenEncodedAsset(ASSET_CURVE_SEGMENTS, &asset)),
        &asset
    );
    ReadEncodedAsset(
        g_GameBoyTransfer.pressingHandImage =
            AllocMemory(OpenEncodedAsset(ASSET_PRESSING_HAND, &asset)),
        &asset
    );
    ReadEncodedAsset(
        g_GameBoyTransfer.statBarsImage = AllocMemory(OpenEncodedAsset(ASSET_STAT_BARS, &asset)),
        &asset
    );
    ReadEncodedAsset(
        g_GameBoyTransfer.shotTypesImage = AllocMemory(OpenEncodedAsset(ASSET_SHOT_TYPES, &asset)),
        &asset
    );
    ReadEncodedAsset(
        g_GameBoyTransfer.noticeCloudImage =
            AllocMemory(OpenEncodedAsset(ASSET_WIDE_CLOUD, &asset)),
        &asset
    );
    ReadEncodedAsset(
        g_GameBoyTransfer.accessingImage = AllocMemory(OpenEncodedAsset(ASSET_ACCESSING, &asset)),
        &asset
    );
    ReadEncodedAsset(
        g_GameBoyTransfer.statLabelsImage =
            AllocMemory(OpenEncodedAsset(ASSET_STAT_LABELS, &asset)),
        &asset
    );
    g_GameBoyTransfer.pressingHandPosition = PRESSING_HAND_HIDDEN;
    g_GameBoyTransfer.isAccessing = FALSE;
    for (index = 0; index != GAME_BOY_GOLFER_COUNT; index++) {
        ReadEncodedAsset(
            g_GameBoyTransfer.faceImages[index] = AllocMemory(
                OpenEncodedAsset(g_CharacterFirstFaceAssets[FIRST_GAME_BOY_GOLFER + index], &asset)
            ),
            &asset
        );
        OpenAssetFile(g_CharacterFirstPaletteFiles[FIRST_GAME_BOY_GOLFER + index], &file);
        size = GetAssetFileSize(&file);
        ReadAssetFile(g_GameBoyTransfer.facePalettes[index] = AllocMemory(size), size, &file);
    }

    PlaySong(SONG_TRANSMIT_MENU, 0);
    LoadCursorBox(g_GameBoyTransfer.cursorBoxImage);
    nuContRmbForceStop();
}

// Frees the images that the menu loaded, so the main menu has room
void FreeTransmitMenu(void) {
    s32 index;

    FreeMemory(&g_GameBoyTransfer.woodPanelImage);
    FreeMemory(&g_GameBoyTransfer.courseSceneImage);
    FreeMemory(&g_GameBoyTransfer.pointerImage);
    FreeMemory(&g_GameBoyTransfer.arrowsImage);
    FreeMemory(&g_GameBoyTransfer.transferPakIconsImage);
    FreeMemory(&g_GameBoyTransfer.titleImage);
    FreeMemory(&g_GameBoyTransfer.cursorBoxImage);
    FreeMemory(&g_GameBoyTransfer.curveSegmentsImage);
    FreeMemory(&g_GameBoyTransfer.pressingHandImage);
    FreeMemory(&g_GameBoyTransfer.statBarsImage);
    FreeMemory(&g_GameBoyTransfer.shotTypesImage);
    FreeMemory(&g_GameBoyTransfer.statLabelsImage);
    FreeMemory(&g_GameBoyTransfer.noticeCloudImage);
    FreeMemory(&g_GameBoyTransfer.accessingImage);
    for (index = 0; index != GAME_BOY_GOLFER_COUNT; index++) {
        FreeMemory(&g_GameBoyTransfer.faceImages[index]);
        FreeMemory(&g_GameBoyTransfer.facePalettes[index]);
    }
}

void ShowTransferError(PakResult result);

// Runs the Bring Characters screen. After a pak check it reads the cartridges'
// characters, lets the player choose one and, once all four slots are full, the
// slot to replace, then brings the character over and offers to bring more from
// another Game Boy cartridge.
void UpdateBringCharacters(void) {
    // The transfer pointer is there to match: reading g_GameBoyTransfer
    // directly gives 299 words differing, 97 in shape
    GameBoyTransfer *transfer = &g_GameBoyTransfer;
    PakResult result;

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
                        SetNoticeState(NOTICE_CLOSING, MESSAGE_PAK_CHECK);
                        PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, SOUND_FULL_SCALE);
                    }

                    break;
            }

            break;
        case BRING_NEXT_CARTRIDGE:
            SetNoticeState(NOTICE_OPENING, MESSAGE_SWAP_GAME_BOY_PAKS);
            ClearCartridgeCharacters();
            transfer->screenStep = BRING_CONFIRMING;
            break;
        case BRING_STARTING:
            SetNoticeState(NOTICE_OPENING, MESSAGE_PAK_CHECK);
            ClearCartridgeCharacters();
            transfer->screenStep = BRING_CONFIRMING;
            break;
        case BRING_CONFIRMING:
            switch (GetNoticeState()) {
                case NOTICE_HIDDEN:
                    if (!transfer->isCanceled) {
                        transfer->isAccessing = TRUE;
                        transfer->screenStep = BRING_READING;
                    }
                    else {
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
                        SetNoticeState(NOTICE_CLOSING, MESSAGE_PAK_CHECK);
                        PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, SOUND_FULL_SCALE);
                    }
                    else if (f_TransmitController.pressedButtons & B_BUTTON) {
                        transfer->pressingHandPosition = PRESSING_HAND_HIDDEN;
                        transfer->isCanceled = TRUE;
                        SetNoticeState(NOTICE_CLOSING, MESSAGE_PAK_CHECK);
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
                // The empty case is there to match: without it 11 words differ (9 in
                // shape), and leaving the default early for it 17 (13)
                case PAK_RESULT_STILL_READING:
                    break;
                case PAK_RESULT_NO_CHARACTERS:
                    SetNoticeState(NOTICE_OPENING, MESSAGE_NO_CHARACTERS);
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
                BringOverCharacter(transfer->listedCharacterIndex, transfer->gameBoyCharacterIndex);
                transfer
                    ->listingStates[transfer->listedCharacters[transfer->listedCharacterIndex]] =
                    LISTING_NONE;
                GenerateClubSet(
                    FIRST_GAME_BOY_GOLFER + transfer->gameBoyCharacterIndex,
                    &g_GameBoyCharacters[transfer->gameBoyCharacterIndex]
                );
                SetTextName(g_GameBoyCharacters[transfer->gameBoyCharacterIndex].name);
                SetTextPaletteColor(255, 255, 0, 0);
                if (transfer->characterCount < GAME_BOY_CHARACTER_COUNT) {
                    transfer->characterCount++;
                }

                transfer->screenStep = BRING_BROUGHT;
                ListPresentCharacters();
            }

            break;
        case BRING_CHOOSING_SLOT:
            if (f_TransmitController.pressedButtons & A_BUTTON) {
                PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, SOUND_FULL_SCALE);
                transfer
                    ->listingStates[transfer->listedCharacters[transfer->listedCharacterIndex]] =
                    LISTING_BRINGING;
                transfer->screenStep = BRING_BRINGING;
                transfer->bringingFrame = 0;
            }
            else if (f_TransmitController.pressedButtons & B_BUTTON) {
                PlaySound(SOUND_MENU_CANCEL, g_NextSoundChannel, SOUND_FULL_SCALE);
                transfer->screenStep = BRING_CHOOSING;
            }
            else if ((f_TransmitController.repeatedButtons & L_JPAD) &&
                transfer->gameBoyCharacterIndex > 0) {
                transfer->gameBoyCharacterIndex--;
                PlaySound(SOUND_MENU_MOVE, g_NextSoundChannel, SOUND_FULL_SCALE);
            }
            else if ((f_TransmitController.repeatedButtons & R_JPAD) &&
                transfer->gameBoyCharacterIndex < GAME_BOY_CHARACTER_COUNT - 1) {
                transfer->gameBoyCharacterIndex++;
                PlaySound(SOUND_MENU_MOVE, g_NextSoundChannel, SOUND_FULL_SCALE);
            }

            break;
        case BRING_CHOOSING:
            if (f_TransmitController.pressedButtons & A_BUTTON) {
                PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, SOUND_FULL_SCALE);
                if (transfer->characterCount == GAME_BOY_CHARACTER_COUNT) {
                    transfer->screenStep = BRING_CHOOSING_SLOT;
                }
                else {
                    transfer->screenStep = BRING_BRINGING;
                    transfer->gameBoyCharacterIndex = transfer->characterCount;
                    transfer->listingStates[transfer
                            ->listedCharacters[transfer->listedCharacterIndex]] = LISTING_BRINGING;
                    transfer->bringingFrame = 0;
                }
            }
            else if (f_TransmitController.pressedButtons & B_BUTTON) {
                PlaySound(SOUND_MENU_CANCEL, g_NextSoundChannel, SOUND_FULL_SCALE);
                transfer->screen = TRANSMIT_SCREEN_OPTIONS;
            }
            else if ((f_TransmitController.repeatedButtons & L_JPAD) &&
                transfer->listedCharacterIndex > 0) {
                transfer->listedCharacterIndex--;
                if (transfer->listedCharacterIndex < transfer->firstShownListedIndex) {
                    transfer->firstShownListedIndex--;
                }

                PlaySound(SOUND_MENU_MOVE, g_NextSoundChannel, SOUND_FULL_SCALE);
            }
            else if ((f_TransmitController.repeatedButtons & R_JPAD) &&
                transfer->listedCharacterIndex < transfer->listedCharacterCount - 1) {
                transfer->listedCharacterIndex++;
                if (transfer->firstShownListedIndex + (SHOWN_LISTED_COUNT - 1) <
                    transfer->listedCharacterIndex) {
                    transfer->firstShownListedIndex++;
                }

                PlaySound(SOUND_MENU_MOVE, g_NextSoundChannel, SOUND_FULL_SCALE);
            }

            break;
        case BRING_BROUGHT:
            transfer->pressingHandPosition = PRESSING_HAND_LOW;
            if (f_TransmitController.pressedButtons & A_BUTTON) {
                transfer->pressingHandPosition = PRESSING_HAND_HIDDEN;
                PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, SOUND_FULL_SCALE);
                if (transfer->characterCount != GAME_BOY_CHARACTER_COUNT) {
                    OpenPrompt(PROMPT_BRING_MORE_CHARACTERS);
                    transfer->screenStep = BRING_ASKING;
                }
                else {
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
        case BRING_UNUSED:
            break;
    }
}

// Opens the notice that explains a controller pak error to the player
void ShowPakError(s32 pfsError) {
    switch (pfsError) {
        case PFS_ERR_NOPACK:
            SetNoticeState(NOTICE_OPENING, MESSAGE_INSERT_TRANSFER_PAK);
            break;
        case PFS_ERR_DEVICE:
            SetNoticeState(NOTICE_OPENING, MESSAGE_SWAP_FOR_TRANSFER_PAK);
            break;
        case PFS_ERR_CONTRFAIL:
            SetNoticeState(NOTICE_OPENING, MESSAGE_CONNECTOR_PROBLEM);
            break;
        case PFS_ERR_NO_GBCART:
            SetNoticeState(NOTICE_OPENING, MESSAGE_NO_CARTRIDGE);
            break;
        case PFS_ERR_NEW_GBCART:
            SetNoticeState(NOTICE_OPENING, MESSAGE_RETRYING_AFTER_REMOVAL);
            break;
    }
}

// Opens the notice that explains a transfer's result, with the hand pressing
// A beside it
void ShowTransferError(PakResult result) {
    g_GameBoyTransfer.pressingHandPosition = PRESSING_HAND_FOR_NOTICE;
    switch (result) {
        case PAK_RESULT_NO_CHARACTERS:
            SetNoticeState(NOTICE_OPENING, MESSAGE_NO_CHARACTERS);
            break;
        case PAK_RESULT_NO_PAK:
            SetNoticeState(NOTICE_OPENING, MESSAGE_INSERT_TRANSFER_PAK);
            break;
        case PAK_RESULT_NO_CARTRIDGE:
            SetNoticeState(NOTICE_OPENING, MESSAGE_NO_CARTRIDGE);
            break;
        case PAK_RESULT_WRONG_CARTRIDGE:
            SetNoticeState(NOTICE_OPENING, MESSAGE_WRONG_CARTRIDGE);
            break;
        case PAK_RESULT_OTHER_PAK:
            SetNoticeState(NOTICE_OPENING, MESSAGE_SWAP_FOR_TRANSFER_PAK);
            break;
        case PAK_RESULT_LOOSE_PAK:
            SetNoticeState(NOTICE_OPENING, MESSAGE_LOOSE_TRANSFER_PAK);
            break;
        case PAK_RESULT_REMOVED_RETRYING:
            SetNoticeState(NOTICE_OPENING, MESSAGE_RETRYING_AFTER_REMOVAL);
            break;
        case PAK_RESULT_CARTRIDGE_REMOVED:
            SetNoticeState(NOTICE_OPENING, MESSAGE_CARTRIDGE_REMOVED);
            break;
        case PAK_RESULT_CONNECTOR_PROBLEM:
            SetNoticeState(NOTICE_OPENING, MESSAGE_CONNECTOR_PROBLEM);
            break;
        case PAK_RESULT_CORRUPTED:
            SetNoticeState(NOTICE_OPENING, MESSAGE_CORRUPTED_SAVE);
            break;
        case PAK_RESULT_CARTRIDGE_REPLACED:
            SetNoticeState(NOTICE_OPENING, MESSAGE_CARTRIDGE_REPLACED);
            break;
        case PAK_RESULT_UNINITIALIZED:
            SetNoticeState(NOTICE_OPENING, MESSAGE_UNINITIALIZED_SAVE);
            break;
    }
}

// Runs the Send Status screen. After a pak check that tags the cartridges, the
// player chooses one, and the status is written to it, after asking whether to
// overwrite any status already there.
void UpdateSendStatus(void) {
    // The transfer pointer is there to match: reading g_GameBoyTransfer
    // directly gives 285 words differing, 95 in shape
    GameBoyTransfer *transfer = &g_GameBoyTransfer;
    s32 result;

    switch (transfer->screenStep) {
        case SEND_STATUS_SHOWING_ERROR:
            switch (GetNoticeState()) {
                case NOTICE_SHOWN:
                    transfer->isAccessing = FALSE;
                    transfer->pressingHandPosition = PRESSING_HAND_FOR_NOTICE;
                    if (f_TransmitController.pressedButtons & A_BUTTON) {
                        transfer->pressingHandPosition = PRESSING_HAND_HIDDEN;
                        SetNoticeState(NOTICE_CLOSING, MESSAGE_PAK_CHECK);
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
            SetNoticeState(NOTICE_OPENING, MESSAGE_PAK_CHECK);
            break;
        case SEND_STATUS_CONFIRMING:
            switch (GetNoticeState()) {
                case NOTICE_HIDDEN:
                    transfer->pressingHandPosition = PRESSING_HAND_HIDDEN;
                    if (!transfer->isCanceled) {
                        transfer->isAccessing = TRUE;
                        transfer->screenStep = SEND_STATUS_TAGGING;
                    }
                    else {
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
                        SetNoticeState(NOTICE_CLOSING, MESSAGE_PAK_CHECK);
                        PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, SOUND_FULL_SCALE);
                    }
                    else if (f_TransmitController.pressedButtons & B_BUTTON) {
                        transfer->pressingHandPosition = PRESSING_HAND_HIDDEN;
                        transfer->isCanceled = TRUE;
                        SetNoticeState(NOTICE_CLOSING, MESSAGE_PAK_CHECK);
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
            }
            else {
                ShowTransferError(result);
                transfer->isAccessing = FALSE;
                transfer->screenStep = SEND_STATUS_SHOWING_ERROR;
            }

            break;
        case SEND_STATUS_CHOOSING:
            if (f_TransmitController.pressedButtons & A_BUTTON) {
                PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, SOUND_FULL_SCALE);
                transfer->screenStep = SEND_STATUS_READY;
            }
            else if (f_TransmitController.pressedButtons & B_BUTTON) {
                PlaySound(SOUND_MENU_CANCEL, g_NextSoundChannel, SOUND_FULL_SCALE);
                transfer->screen = TRANSMIT_SCREEN_OPTIONS;
            }
            else if ((f_TransmitController.repeatedButtons & L_JPAD) &&
                transfer->chosenCartridgeIndex > 0) {
                PlaySound(SOUND_MENU_MOVE, g_NextSoundChannel, SOUND_FULL_SCALE);
                transfer->chosenCartridgeIndex--;
            }
            else if ((f_TransmitController.repeatedButtons & R_JPAD) &&
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
            }
            else if (f_TransmitController.pressedButtons & B_BUTTON) {
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
                    }
                    else {
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

// Lets the player choose a character whose experience to send, asks them to
// start a pak check, then writes the experience to the cartridge and shows the
// result.
void UpdateSendExperience(void) {
    // The transfer pointer is there to match: reading g_GameBoyTransfer
    // directly gives 150 words differing, 70 in shape
    GameBoyTransfer *transfer = &g_GameBoyTransfer;

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
                        SetNoticeState(NOTICE_CLOSING, MESSAGE_PAK_CHECK);
                        PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, SOUND_FULL_SCALE);
                    }

                    break;
            }

            break;
        case SEND_EXPERIENCE_CHOOSING:
            if (f_TransmitController.pressedButtons & A_BUTTON) {
                if (g_GameBoyCharacters[transfer->gameBoyCharacterIndex].experience == 0) {
                    transfer->screenStep = SEND_EXPERIENCE_NOTHING_TO_SEND;
                    SetNoticeState(NOTICE_OPENING, MESSAGE_NO_EXPERIENCE);
                    PlaySound(SOUND_MENU_BLOCKED, g_NextSoundChannel, SOUND_FULL_SCALE);
                }
                else {
                    transfer->screenStep = SEND_EXPERIENCE_CONFIRMING;
                    SetNoticeState(NOTICE_OPENING, MESSAGE_PAK_CHECK);
                    PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, SOUND_FULL_SCALE);
                }
            }
            else if (f_TransmitController.pressedButtons & B_BUTTON) {
                PlaySound(SOUND_MENU_CANCEL, g_NextSoundChannel, SOUND_FULL_SCALE);
                transfer->screen = TRANSMIT_SCREEN_OPTIONS;
            }
            else if ((f_TransmitController.repeatedButtons & L_JPAD) &&
                transfer->gameBoyCharacterIndex > 0) {
                transfer->gameBoyCharacterIndex--;
                PlaySound(SOUND_MENU_MOVE, g_NextSoundChannel, SOUND_FULL_SCALE);
            }
            else if ((f_TransmitController.repeatedButtons & R_JPAD) &&
                transfer->gameBoyCharacterIndex < transfer->characterCount - 1) {
                transfer->gameBoyCharacterIndex++;
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
                    }
                    else {
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
                        SetNoticeState(NOTICE_CLOSING, MESSAGE_PAK_CHECK);
                        PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, SOUND_FULL_SCALE);
                    }
                    else if (f_TransmitController.pressedButtons & B_BUTTON) {
                        transfer->pressingHandPosition = PRESSING_HAND_HIDDEN;
                        transfer->isCanceled = TRUE;
                        SetNoticeState(NOTICE_CLOSING, MESSAGE_PAK_CHECK);
                        PlaySound(SOUND_MENU_CANCEL, g_NextSoundChannel, SOUND_FULL_SCALE);
                    }

                    break;
            }

            break;
        case SEND_EXPERIENCE_WRITING:
            transfer->isAccessing = FALSE;
            switch (WriteExperienceToCartridge(transfer->gameBoyCharacterIndex)) {
                case EXPERIENCE_WRITE_DONE:
                    transfer->screenStep = SEND_EXPERIENCE_SENT;
                    break;
                case EXPERIENCE_WRITE_NOT_FROM_CARTRIDGE:
                    SetNoticeState(NOTICE_OPENING, MESSAGE_NOT_FROM_CARTRIDGE);
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
        case SEND_EXPERIENCE_UNUSED_A:
        case SEND_EXPERIENCE_UNUSED_B:
        case SEND_EXPERIENCE_UNUSED_C:
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
                        SetNoticeState(NOTICE_CLOSING, MESSAGE_PAK_CHECK);
                        PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, SOUND_FULL_SCALE);
                    }

                    break;
            }

            break;
    }
}

// Whether every playing controller holds a Rumble Pak
Boolean HaveAllRumblePaks(void) {
    Boolean haveAll = TRUE;
    s32 index;

    for (index = 0; index != g_ControllerCount; index++) {
        if (nuContRmbCheck(g_PlayerControllerPorts[index]) != 0) {
            haveAll = FALSE;
        }
    }

    return haveAll;
}

// A macro rather than an inline function, which leaves a store-flag result
#define AreAllCButtonsHeld(buttons) \
    ((((buttons) & (L_CBUTTONS | D_CBUTTONS | U_CBUTTONS)) == \
         (L_CBUTTONS | D_CBUTTONS | U_CBUTTONS)) && \
        ((buttons) & R_CBUTTONS))

// Runs the options screen and hands each frame to the screen chosen there, then
// leaves for the title screen or the main menu, asking for any missing Rumble
// Pak first.
void UpdateTransmitMenu(void) {
    // The transfer pointer is there to match: reading g_GameBoyTransfer
    // directly gives 110 words differing, 40 in shape
    GameBoyTransfer *transfer = &g_GameBoyTransfer;
    s32 controllerIndex;

    f_TransmitController.repeatedButtons = 0;
    f_TransmitController.buttons = 0;
    f_TransmitController.pressedButtons = 0;
    for (controllerIndex = 0; controllerIndex != g_ControllerCount; controllerIndex++) {
        f_TransmitController.repeatedButtons |= g_ControllerStates[controllerIndex].repeatedButtons;
        f_TransmitController.buttons |= g_ControllerStates[controllerIndex].buttons;
        f_TransmitController.pressedButtons |= g_ControllerStates[controllerIndex].pressedButtons;
    }

    if (IsGameFlagSet(GAME_FLAG_ALLOW_CARTRIDGE_DUMP) &&
        AreAllCButtonsHeld(g_ControllerStates[0].buttons)) {
        DumpCartridgeRam();
    }

    switch (transfer->screen) {
        case TRANSMIT_SCREEN_OPTIONS:
            if (GetWipeProgress() != WIPE_OPEN_PROGRESS) {
                break;
            }

            if (f_TransmitController.pressedButtons & A_BUTTON) {
                PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, SOUND_FULL_SCALE);
                if (transfer->optionIndex == STATUS_OPTION_ROW) {
                    transfer->screen = TRANSMIT_SCREEN_SEND_STATUS;
                    transfer->screenStep = SEND_STATUS_STARTING;
                    ResetSendStatus();
                }
                else if (transfer->optionIndex == SECOND_OPTION_ROW) {
                    if (transfer->transferMode == TRANSFER_MODE_AT_BOOT) {
                        transfer->screen = TRANSMIT_SCREEN_BRING_CHARACTERS;
                        transfer->screenStep = BRING_STARTING;
                        ClearCartridgeCharacters();
                    }
                    else {
                        transfer->screen = TRANSMIT_SCREEN_SEND_EXPERIENCE;
                        transfer->screenStep = SEND_EXPERIENCE_DONE;
                        ResetSendExperience();
                    }
                }

                if (transfer->optionIndex == MAIN_MENU_OPTION_ROW) {
                    if (!HaveAllRumblePaks()) {
                        transfer->isCanceled = FALSE;
                        transfer->screen = TRANSMIT_SCREEN_RUMBLE_PAK_PROMPT;
                        SetNoticeState(NOTICE_OPENING, MESSAGE_INSERT_RUMBLE_PAK);
                    }
                    else {
                        transfer->screen = TRANSMIT_SCREEN_LEAVING_FOR_TITLE;
                        StartWipe(WIPE_CLOSING);
                    }
                }
            }
            else if (f_TransmitController.pressedButtons & B_BUTTON) {
                if (transfer->transferMode == TRANSFER_MODE_FROM_MENU) {
                    if (!HaveAllRumblePaks()) {
                        transfer->isCanceled = TRUE;
                        transfer->screen = TRANSMIT_SCREEN_RUMBLE_PAK_PROMPT;
                        SetNoticeState(NOTICE_OPENING, MESSAGE_INSERT_RUMBLE_PAK);
                    }
                    else {
                        transfer->screen = TRANSMIT_SCREEN_LEAVING_FOR_TITLE;
                        StartWipe(WIPE_CLOSING);
                    }
                }
            }
            else if (f_TransmitController.repeatedButtons & U_JPAD) {
                if (transfer->transferMode == TRANSFER_MODE_AT_BOOT) {
                    transfer->optionIndex =
                        (transfer->optionIndex + MODE_OPTION_COUNT - 1) % MODE_OPTION_COUNT;
                }
                else {
                    transfer->optionIndex ^= OPTION_ROW_FLIP;
                }

                PlaySound(SOUND_MENU_MOVE, g_NextSoundChannel, SOUND_FULL_SCALE);
            }
            else if (f_TransmitController.repeatedButtons & D_JPAD) {
                if (transfer->transferMode == TRANSFER_MODE_AT_BOOT) {
                    transfer->optionIndex =
                        (transfer->optionIndex + MODE_OPTION_COUNT + 1) % MODE_OPTION_COUNT;
                }
                else {
                    transfer->optionIndex ^= OPTION_ROW_FLIP;
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
                        transfer->screen = TRANSMIT_SCREEN_LEAVING_FOR_TITLE;
                    }
                    else {
                        transfer->screen = TRANSMIT_SCREEN_LEAVING_FOR_MENU;
                    }

                    StartWipe(WIPE_CLOSING);
                    break;
                case NOTICE_CLOSING:
                    break;
                case NOTICE_SHOWN:
                    transfer->pressingHandPosition = PRESSING_HAND_FOR_NOTICE;
                    if (f_TransmitController.pressedButtons & A_BUTTON) {
                        transfer->pressingHandPosition = PRESSING_HAND_HIDDEN;
                        SetNoticeState(NOTICE_CLOSING, MESSAGE_PAK_CHECK);
                        PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, SOUND_FULL_SCALE);
                    }

                    break;
            }

            break;
        case TRANSMIT_SCREEN_LEAVING_FOR_TITLE:
            if (GetWipeProgress() == WIPE_CLOSED_PROGRESS) {
                StartWipe(WIPE_OPENING);
                g_Scene = SCENE_SWAPPING_MODULES;
                g_SceneModuleToUnload = MODULE_TRANSMIT;
                g_NextScene = SCENE_TITLE_SCREEN;
                g_SceneModuleToLoad = MODULE_TITLE_SCREEN;
            }

            break;
        case TRANSMIT_SCREEN_LEAVING_FOR_MENU:
            if (GetWipeProgress() == WIPE_CLOSED_PROGRESS) {
                StartWipe(WIPE_OPENING);
                EnterMenuAtTitle();
                g_Scene = SCENE_SWAPPING_MODULES;
                g_SceneModuleToUnload = MODULE_TRANSMIT;
                g_NextScene = SCENE_MENU;
                g_SceneModuleToLoad = MODULE_MENU;
            }

            break;
    }
}

// Fills the slot for a cartridge's save from the character data its record
// points to
void DecodeCartridgeCharacter(
    u8 *characterData,
    s32 cartridgeIndex,
    s32 saveIndex,
    s32 companyCode
) {
    GameBoyCharacter *gameBoyCharacter =
        &g_GameBoyTransfer.cartridgeCharacters[(cartridgeIndex * CARTRIDGE_SAVE_COUNT) + saveIndex];
    s32 index;

    gameBoyCharacter->status = GAME_BOY_CHARACTER_STATUS_PRESENT;
    for (index = 0; index != (SAVE_NAME_SIZE - 1); index++) {
        gameBoyCharacter->name[index] = characterData[index];
        gameBoyCharacter->saveName[index] = characterData[index];
        if (gameBoyCharacter->name[index] >= 'a' && gameBoyCharacter->name[index] <= 'z') {
            gameBoyCharacter->name[index] += LOWERCASE_CODE_OFFSET;
        }
    }

    gameBoyCharacter->name[(SAVE_NAME_SIZE - 1)] = '\0';
    gameBoyCharacter->saveName[(SAVE_NAME_SIZE - 1)] = '\0';
    for (index = 0; index != SAVE_KEY_SIZE; index++) {
        gameBoyCharacter->saveKey[index] = characterData[DATA_SAVE_KEY_OFFSET + index];
    }

    gameBoyCharacter->golferIndex = characterData[DATA_CHARACTER_OFFSET];
    if (gameBoyCharacter->golferIndex >= GAME_BOY_GOLFER_COUNT) {
        gameBoyCharacter->golferIndex = GAME_BOY_GOLFER_COUNT - 1;
    }

    if (IsGameFlagSet(GAME_FLAG_GAME_BOY_LEFTIES)) {
        gameBoyCharacter->isLefty = TRUE;
    }
    else {
        gameBoyCharacter->isLefty = characterData[DATA_IS_LEFTY_OFFSET];
    }

    gameBoyCharacter->level = characterData[DATA_LEVEL_OFFSET];
    gameBoyCharacter->clubPitch = characterData[DATA_CLUB_PITCH_OFFSET];
    gameBoyCharacter->shotCurve = characterData[DATA_SHOT_CURVE_OFFSET];
    gameBoyCharacter->lieRatingLevel = characterData[DATA_LIE_RATING_LEVEL_OFFSET];
    gameBoyCharacter->mishitResistance = characterData[DATA_MISHIT_RESISTANCE_OFFSET];
    if (IsGameFlagSet(GAME_FLAG_BALL_TYPE_1)) {
        gameBoyCharacter->ballType = GAME_BOY_BALL_TYPE_MARIO;
    }
    else if (IsGameFlagSet(GAME_FLAG_BALL_TYPE_2)) {
        gameBoyCharacter->ballType = GAME_BOY_BALL_TYPE_BOWSER;
    }
    else if (IsGameFlagSet(GAME_FLAG_BALL_TYPE_3)) {
        gameBoyCharacter->ballType = GAME_BOY_BALL_TYPE_WARIO;
    }
    else if (IsGameFlagSet(GAME_FLAG_BALL_TYPE_4)) {
        gameBoyCharacter->ballType = GAME_BOY_BALL_TYPE_RAINBOW;
    }
    else if (IsGameFlagSet(GAME_FLAG_BALL_TYPE_5)) {
        gameBoyCharacter->ballType = GAME_BOY_BALL_TYPE_5;
    }
    else if (IsGameFlagSet(GAME_FLAG_BALL_TYPE_6)) {
        gameBoyCharacter->ballType = GAME_BOY_BALL_TYPE_6;
    }
    else if (IsGameFlagSet(GAME_FLAG_BALL_TYPE_7)) {
        gameBoyCharacter->ballType = GAME_BOY_BALL_TYPE_7;
    }
    else {
        gameBoyCharacter->ballType = characterData[DATA_BALL_TYPE_OFFSET];
    }

    osSyncPrintf("balltype %d\n", gameBoyCharacter->ballType);
    gameBoyCharacter->ignoredValue =
        (characterData[DATA_IGNORED_VALUE_OFFSET + 2] << (BITS_PER_BYTE * 2)) |
        (characterData[DATA_IGNORED_VALUE_OFFSET + 1] << BITS_PER_BYTE) |
        characterData[DATA_IGNORED_VALUE_OFFSET];
    gameBoyCharacter->experience = 0;
    gameBoyCharacter->driveDistance = characterData[DATA_DRIVE_DISTANCE_OFFSET] |
        (characterData[DATA_DRIVE_DISTANCE_OFFSET + 1] << BITS_PER_BYTE);
    osSyncPrintf(
        "%d, %d %d\n",
        gameBoyCharacter->experience,
        gameBoyCharacter->clubPitch,
        gameBoyCharacter->shotCurve
    );
    for (index = 0; index != GAME_BOY_CLUB_COUNT; index++) {
        gameBoyCharacter->clubs[index].distance =
            characterData[DATA_CLUBS_OFFSET + (index * DATA_CLUB_SIZE)] |
            (characterData[DATA_CLUBS_OFFSET + (index * DATA_CLUB_SIZE) + 1] << BITS_PER_BYTE);
        gameBoyCharacter->clubs[index].pitch =
            characterData[DATA_CLUBS_OFFSET + (index * DATA_CLUB_SIZE) + CLUB_DATA_PITCH_OFFSET] |
            (characterData
                    [DATA_CLUBS_OFFSET + (index * DATA_CLUB_SIZE) + CLUB_DATA_PITCH_OFFSET + 1]
                << BITS_PER_BYTE);
        gameBoyCharacter->clubs[index].ignoredStat = characterData
            [DATA_CLUBS_OFFSET + (index * DATA_CLUB_SIZE) + CLUB_DATA_IGNORED_STAT_OFFSET];
        gameBoyCharacter->clubs[index].sideSpin = characterData
            [DATA_CLUBS_OFFSET + (index * DATA_CLUB_SIZE) + CLUB_DATA_SIDE_SPIN_OFFSET];
        osSyncPrintf(
            "%d, %d %d\n",
            index,
            gameBoyCharacter->clubs[index].distance,
            gameBoyCharacter->clubs[index].pitch
        );
    }

    gameBoyCharacter->companyCode = companyCode;
    gameBoyCharacter->saveIndex = saveIndex;
}

// Would skip a save without characters, but does nothing
void SkipCartridgeSave(
    u8 *unusedData UNUSED,
    s32 unusedCartridgeIndex UNUSED,
    s32 unusedSaveIndex UNUSED,
    u8 *unusedTitle UNUSED,
    s32 unusedCompanyCode UNUSED
) {
}

// Whether a Game Boy character came from the save that the character data
// belongs to, by its save name and key
Boolean IsCharacterFromSave(GameBoyCharacter *gameBoyCharacter, u8 *characterData) {
    s32 index;

    osSyncPrintf("PHASE a\n");
    for (index = 0; index != SAVE_NAME_SIZE; index++) {
        if (gameBoyCharacter->saveName[index] != characterData[index]) {
            if (FALSE) {
                osSyncPrintf("PHASE b\n");
            }

            return FALSE;
        }
    }

    osSyncPrintf("PHASE c\n");
    for (index = 0; index != SAVE_KEY_SIZE; index++) {
        if (gameBoyCharacter->saveKey[index] != characterData[DATA_SAVE_KEY_OFFSET + index]) {
            osSyncPrintf("PHASE d\n");
            return FALSE;
        }
    }

    osSyncPrintf("PHASE e\n");
    return TRUE;
}

// The menu's options, each the offset of its name from TRANSMIT_OPTION_MESSAGES
static s32 f_ModeOptions[TRANSFER_MODE_COUNT][MODE_OPTION_COUNT] = {
    {TRANSMIT_OPTION_STATUS, TRANSMIT_OPTION_CHARACTERS, TRANSMIT_OPTION_MAIN_MENU},
    {TRANSMIT_OPTION_STATUS, TRANSMIT_OPTION_EXPERIENCE, TRANSMIT_OPTION_NONE},
};
static char *f_UnusedShotTypeNames[SHOT_TYPE_COUNT] UNUSED = {"Straight", "Draw", "Fade"};
static char f_UnusedDriveName[8] UNUSED = "Drive";
static Radians f_StatusPointerAngle = 0.0f;
static s32 f_CharacterArrowFrame = 0;
static Radians f_OptionPointerAngle = 0.0f;
// The title in the Game Boy cartridge header, then its game code
static char f_GameBoyCartridgeTitle[16] = "MARIO GOLF\0AWXE";
// Begins every save in the Game Boy cartridge RAM
static char f_CartridgeSaveSignature[16] = "CAMELOT GAMEBOY";
static NoticeState f_NoticeState = NOTICE_HIDDEN;
static MessageId f_NoticeMessageId = 0;
static s32 f_NoticeSize = 0;
static s32 f_PressingHandFrame = 0;
static s32 f_UnusedZeros[2] UNUSED = {0, 0};
// The order in which the status data lists the player's characters
static s32 f_StatusCharacterOrder[CHARACTER_COUNT] =
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17};

// Copies a listed cartridge character into a Game Boy character slot and
// marks it brought over, remembering where it came from
void BringOverCharacter(s32 listedIndex, s32 gameBoyCharacterIndex) {
    // The transfer pointer is there to match: reading g_GameBoyTransfer
    // directly gives 34 words differing, 14 in shape
    GameBoyTransfer *transfer = &g_GameBoyTransfer;

    g_GameBoyCharacters[gameBoyCharacterIndex] =
        transfer->cartridgeCharacters[transfer->listedCharacters[listedIndex]];
    g_GameBoyCharacters[gameBoyCharacterIndex].status = GAME_BOY_CHARACTER_STATUS_PRESENT;
    transfer->cartridgeCharacters[transfer->listedCharacters[listedIndex]].status =
        GAME_BOY_CHARACTER_STATUS_BROUGHT_OVER;
    transfer->sourceIndices[gameBoyCharacterIndex] = transfer->listedCharacters[listedIndex];
}

// Starts the Bring Characters screen with its cursors at the first entries
void ResetBringCharacters(void) {
    g_GameBoyTransfer.listedCharacterIndex = 0;
    g_GameBoyTransfer.gameBoyCharacterIndex = 0;
    g_GameBoyTransfer.firstShownListedIndex = 0;
    g_GameBoyTransfer.isCanceled = FALSE;
}

// Starts the Send Status screen with no cartridge tagged or chosen
void ResetSendStatus(void) {
    g_GameBoyTransfer.chosenCartridgeIndex = 0;
    g_GameBoyTransfer.taggedCartridgeCount = 0;
    g_GameBoyTransfer.isCanceled = FALSE;
}

// Starts the Send Experience screen with no cartridge tagged and the first
// character chosen
void ResetSendExperience(void) {
    g_GameBoyTransfer.chosenCartridgeIndex = 0;
    g_GameBoyTransfer.gameBoyCharacterIndex = 0;
    g_GameBoyTransfer.taggedCartridgeCount = 0;
    g_GameBoyTransfer.isCanceled = FALSE;
}

// Returns the checksum that a save holds, which ComputeSaveChecksum checks
s32 GetStoredChecksum(u8 *save) {
    return save[SAVE_CHECKSUM_OFFSET] | (save[SAVE_CHECKSUM_OFFSET + 1] << BITS_PER_BYTE);
}

// Sums the save's bytes after its checksum
s32 ComputeSaveChecksum(u8 *save) {
    s32 sum = 0;
    s32 offset;

    for (offset = SAVE_CHECKSUM_OFFSET + 2; offset != CARTRIDGE_SAVE_SIZE; offset++) {
        sum += save[offset];
    }

    return sum;
}

// Rebuilds the list of cartridge characters that can still be brought over,
// padding the rest with -1
void ListPresentCharacters(void) {
    // The transfer pointer is there to match: reading g_GameBoyTransfer
    // directly gives 39 words differing, 9 in shape
    GameBoyTransfer *transfer = &g_GameBoyTransfer;
    s32 listedCount = 0;
    s32 index;

    for (index = 0; index != CARTRIDGE_CHARACTER_COUNT; index++) {
        if (transfer->cartridgeCharacters[index].status == GAME_BOY_CHARACTER_STATUS_PRESENT) {
            transfer->listedCharacters[listedCount++] = index;
        }
    }

    for (; listedCount != CARTRIDGE_CHARACTER_COUNT; listedCount++) {
        transfer->listedCharacters[listedCount] = -1;
    }
}

// Draws the hint that tells the player what to do at the Send Status screen's
// step
void DrawSendStatusHint(Gfx **displayListHead, GameBoyTransfer *transfer) {
    Gfx *gfx = *displayListHead;
    MessageId messageId = NO_MESSAGE;

    switch (transfer->screenStep) {
        case SEND_STATUS_CHOOSING:
            messageId = MESSAGE_CHOOSE_CARTRIDGE;
            break;
        case SEND_STATUS_SENT:
            messageId = MESSAGE_SEND_STATUS;
            break;
        case SEND_STATUS_READY:
            messageId = MESSAGE_KEEP_POWER_ON;
            break;
    }

    if (messageId != NO_MESSAGE) {
        SetTextSlot(
            HINT_LEFT,
            HINT_TOP,
            messageId,
            TEXT_SLOT_LARGE_FONT,
            HINT_TEXT_SLOT,
            TEXT_FLAG_DRAWN_BY_OWNER,
            255,
            255,
            255,
            OPAQUE_ALPHA
        );
        DrawLargeTextSlot(&gfx, HINT_TEXT_SLOT);
    }

    *displayListHead = gfx;
}

// Draws a Game Boy character's name, level and drive, the name of their shot
// type and a bar from its center out to their shot curve.
void DrawCharacterStats(
    Gfx **displayListHead,
    GameBoyCharacter *gameBoyCharacter,
    GameBoyTransfer *transfer
) {
    Gfx *gfx = *displayListHead;
    s32 segment;

    // Holding the shot type row in segmentCurve first is there to match: a
    // shotType local gives 50 words differing, none in shape
    ShotType segmentCurve;

    SetUpTextDrawing(&gfx);
    DrawLargeText(&gfx, STATS_NAME_LEFT, STATS_NAME_TOP, gameBoyCharacter->name);
    SetUpShadowedTextDrawing(&gfx);
    sprintf(
        g_PrintBuffer,
        "  %2d     :%3d",
        gameBoyCharacter->level,
        gameBoyCharacter->driveDistance
    );
    DrawSmallText(&gfx, STATS_LEFT, STATS_TOP, g_PrintBuffer);
    if (gameBoyCharacter->shotCurve == 0) {
        segmentCurve = SHOT_TYPE_STRAIGHT;
    }
    else if (gameBoyCharacter->shotCurve > 0) {
        segmentCurve = SHOT_TYPE_DRAW;
    }
    else {
        segmentCurve = SHOT_TYPE_FADE;
    }

    SetTextSlot(
        HINT_LEFT,
        HINT_TOP,
        MESSAGE_CHOOSE_BROUGHT_CHARACTER,
        TEXT_SLOT_LARGE_FONT,
        HINT_TEXT_SLOT,
        TEXT_FLAG_DRAWN_BY_OWNER,
        255,
        255,
        255,
        OPAQUE_ALPHA
    );
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE | AA_EN, G_RM_TEX_EDGE2 | AA_EN);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    LoadSmallPalette(&gfx, 0, ImagePalette(transfer->shotTypesImage));
    LoadNibbleTextureBlock(
        &gfx,
        ImageTexels(transfer->shotTypesImage),
        G_IM_FMT_CI,
        SHOT_TYPE_WIDTH,
        SHOT_TYPE_HEIGHT * SHOT_TYPE_COUNT,
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
        QuarterPixels(SHOT_TYPE_LEFT),
        QuarterPixels(SHOT_TYPE_TOP),
        QuarterPixels(SHOT_TYPE_LEFT + SHOT_TYPE_WIDTH),
        QuarterPixels(SHOT_TYPE_TOP + SHOT_TYPE_HEIGHT),
        G_TX_RENDERTILE,
        0,
        TexelCoordinate(segmentCurve * SHOT_TYPE_HEIGHT),
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );
    LoadSmallPalette(&gfx, 0, ImagePalette(transfer->statLabelsImage));
    LoadNibbleTextureBlock(
        &gfx,
        ImageTexels(transfer->statLabelsImage),
        G_IM_FMT_CI,
        STAT_LABELS_WIDTH,
        STAT_LABELS_HEIGHT,
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
        QuarterPixels(STATS_LEFT),
        QuarterPixels(STATS_TOP),
        QuarterPixels(STATS_LEFT + STAT_LABELS_WIDTH),
        QuarterPixels(STATS_TOP + STAT_LABELS_HEIGHT),
        G_TX_RENDERTILE,
        0,
        0,
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );
    LoadSmallPalette(&gfx, 0, ImagePalette(transfer->statBarsImage));
    gDPLoadTextureTile_4b(
        gfx++,
        TextureAddress(ImageTexels(transfer->statBarsImage)),
        G_IM_FMT_CI,
        CURVE_BAR_WIDTH,
        0,
        0,
        0,
        CURVE_BAR_WIDTH - 1,
        CURVE_BAR_HEIGHT - 1,
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
        QuarterPixels(CURVE_BAR_LEFT),
        QuarterPixels(CURVE_BAR_TOP),
        QuarterPixels(CURVE_BAR_LEFT + CURVE_BAR_WIDTH),
        QuarterPixels(CURVE_BAR_TOP + CURVE_BAR_HEIGHT),
        G_TX_RENDERTILE,
        0,
        0,
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );
    LoadLargePalette(&gfx, ImagePalette(transfer->curveSegmentsImage));
    LoadTextureBlock(
        &gfx,
        LargeImageTexels(transfer->curveSegmentsImage),
        G_IM_FMT_CI,
        G_IM_SIZ_8b,
        CURVE_SEGMENT_WIDTH,
        CURVE_SEGMENT_WIDTH * CURVE_SEGMENT_COUNT,
        0,
        0,
        0,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    for (segment = 0, segmentCurve = -CURVE_SEGMENT_COUNT / 2; segment != CURVE_SEGMENT_COUNT;
        segment++, segmentCurve++) {
        // Light segments from the center out to the shot curve. The continues are
        // there to match: each pair joined with || gives 20 words differing, 8 in
        // shape
        if (gameBoyCharacter->shotCurve < 0) {
            if (segmentCurve < gameBoyCharacter->shotCurve) {
                continue;
            }

            if (segmentCurve > 0) {
                continue;
            }
        }

        if (gameBoyCharacter->shotCurve > 0) {
            if (segmentCurve > gameBoyCharacter->shotCurve) {
                continue;
            }

            if (segmentCurve < 0) {
                continue;
            }
        }

        if (gameBoyCharacter->shotCurve == 0 && segment != CURVE_SEGMENT_COUNT / 2) {
            continue;
        }

        DrawTextureRectangle(
            &gfx,
            QuarterPixels(CURVE_BAR_LEFT + (segment * CURVE_SEGMENT_WIDTH)),
            QuarterPixels(CURVE_BAR_TOP),
            QuarterPixels(CURVE_BAR_LEFT + ((segment + 1) * CURVE_SEGMENT_WIDTH)),
            QuarterPixels(CURVE_BAR_TOP + CURVE_BAR_HEIGHT),
            G_TX_RENDERTILE,
            0,
            TexelCoordinate(segment * CURVE_SEGMENT_WIDTH),
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
    }

    *displayListHead = gfx;
}

// Where DrawBringCharactersText prints the screen's step
#define STEP_DEBUG_COLUMN 20
#define STEP_DEBUG_ROW 2

// Draws the Bring Characters screen's text: the stats of the character under
// the cursor and the hint for the step
void DrawBringCharactersText(Gfx **displayListHead, GameBoyTransfer *transfer) {
    GameBoyCharacter *gameBoyCharacter = g_GameBoyCharacters;
    Gfx *gfx = *displayListHead;
    Boolean isCharacterShown = FALSE;
    MessageId messageId;

    SetUpTextDrawing(&gfx);
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, OPAQUE_ALPHA);
    sprintf(g_PrintBuffer, "%3d", (int)transfer->screenStep);
    PrintDebugText(g_PrintBuffer, STEP_DEBUG_COLUMN, STEP_DEBUG_ROW);
    if (transfer->screenStep == BRING_CHOOSING_SLOT) {
        isCharacterShown = TRUE;
        gameBoyCharacter = &g_GameBoyCharacters[transfer->gameBoyCharacterIndex];
    }

    if ((transfer->screenStep == BRING_CHOOSING || transfer->screenStep == BRING_BRINGING) &&
        g_GameBoyTransfer.listedCharacterCount > 0) {
        isCharacterShown = TRUE;
        gameBoyCharacter =
            &transfer
                 ->cartridgeCharacters[transfer->listedCharacters[transfer->listedCharacterIndex]];
    }

    if (isCharacterShown) {
        DrawCharacterStats(&gfx, gameBoyCharacter, transfer);
    }

    SetUpTextDrawing(&gfx);
    if (transfer->readStep == LastCartridgeReadStep(g_ControllerCount)) {
        messageId = NO_MESSAGE;
        switch (transfer->screenStep) {
            case BRING_CHOOSING:
                messageId = MESSAGE_CHOOSE_BROUGHT_CHARACTER;
                break;
            case BRING_CHOOSING_SLOT:
                messageId = MESSAGE_CHOOSE_SWAPPED_CHARACTER;
                break;
            case BRING_BROUGHT:
                messageId = MESSAGE_CHARACTER_BROUGHT;
                break;
        }

        if (messageId != NO_MESSAGE) {
            SetTextSlot(
                HINT_LEFT,
                HINT_TOP,
                messageId,
                TEXT_SLOT_LARGE_FONT,
                HINT_TEXT_SLOT,
                TEXT_FLAG_DRAWN_BY_OWNER,
                255,
                255,
                255,
                OPAQUE_ALPHA
            );
            DrawLargeTextSlot(&gfx, HINT_TEXT_SLOT);
        }
    }

    *displayListHead = gfx;
}

// Draws the Send Experience screen's text: each character's experience, the
// chosen one's stats and the hint for the step
void DrawSendExperienceText(Gfx **displayListHead, GameBoyTransfer *transfer) {
    Gfx *gfx = *displayListHead;
    s32 gameBoyCharacterIndex;
    MessageId messageId;

    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, OPAQUE_ALPHA);
    SetUpShadowedTextDrawing(&gfx);
    for (gameBoyCharacterIndex = 0; gameBoyCharacterIndex != g_GameBoyTransfer.characterCount;
        gameBoyCharacterIndex++) {
        GameBoyCharacter *gameBoyCharacter = &g_GameBoyCharacters[gameBoyCharacterIndex];
        sprintf(g_PrintBuffer, "%4d", (int)gameBoyCharacter->experience);
        DrawSmallText(
            &gfx,
            EXPERIENCE_LEFT + (gameBoyCharacterIndex * FACE_SPACING),
            EXPERIENCE_TOP,
            g_PrintBuffer
        );
    }

    DrawCharacterStats(&gfx, &g_GameBoyCharacters[transfer->gameBoyCharacterIndex], transfer);
    SetUpTextDrawing(&gfx);
    messageId = NO_MESSAGE;
    switch (transfer->screenStep) {
        case SEND_EXPERIENCE_CHOOSING:
            messageId = MESSAGE_CHOOSE_EXPERIENCE;
            break;
        case SEND_EXPERIENCE_SENT:
            messageId = MESSAGE_EXPERIENCE_SENT;
            break;
    }

    if (messageId != NO_MESSAGE) {
        SetTextSlot(
            HINT_LEFT,
            HINT_TOP,
            messageId,
            TEXT_SLOT_LARGE_FONT,
            HINT_TEXT_SLOT,
            TEXT_FLAG_DRAWN_BY_OWNER,
            255,
            255,
            255,
            OPAQUE_ALPHA
        );
        DrawLargeTextSlot(&gfx, HINT_TEXT_SLOT);
    }

    *displayListHead = gfx;
}

// Draws the Send Status screen: the imported faces and the icons of the tagged
// controllers, dimming the others once a cartridge is chosen. A swinging
// pointer marks the cartridge while choosing and the cursor box once it is
// chosen.
void DrawSendStatusScreen(Gfx **displayListHead, GameBoyTransfer *transfer) {
    Gfx *gfx = *displayListHead;
    s32 index;
    s32 slot;
    s32 pointerRestLeft;
    s32 pointerLeft;
    s32 pointerSwing;

    DrawImportedFaces(&gfx, transfer);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPSetCombineMode(gfx++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
    LoadSmallPalette(&gfx, 0, ImagePalette(transfer->transferPakIconsImage));
    LoadNibbleTextureBlock(
        &gfx,
        ImageTexels(transfer->transferPakIconsImage),
        G_IM_FMT_CI,
        TRANSFER_PAK_ICON_SIZE,
        TRANSFER_PAK_ICON_SIZE * MAXCONTROLLERS,
        0,
        0,
        0,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    slot = 0;
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, OPAQUE_ALPHA);
    for (index = 0; index != transfer->taggedCartridgeCount; index++) {
        if (transfer->screenStep == SEND_STATUS_READY && slot != transfer->chosenCartridgeIndex) {
            gDPSetPrimColor(gfx++, 0, 0, 127, 127, 127, OPAQUE_ALPHA);
        }
        else {
            gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, OPAQUE_ALPHA);
        }

        DrawTextureRectangle(
            &gfx,
            QuarterPixels(TRANSFER_PAK_ICONS_LEFT + (slot * TRANSFER_PAK_ICON_SPACING)),
            QuarterPixels(TRANSFER_PAK_ICONS_TOP),
            QuarterPixels(
                TRANSFER_PAK_ICONS_LEFT + (slot * TRANSFER_PAK_ICON_SPACING) +
                TRANSFER_PAK_ICON_SIZE
            ),
            QuarterPixels(TRANSFER_PAK_ICONS_TOP + TRANSFER_PAK_ICON_SIZE),
            G_TX_RENDERTILE,
            0,
            TexelCoordinate(transfer->taggedControllers[index] * TRANSFER_PAK_ICON_SIZE),
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        slot++;
    }

    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, OPAQUE_ALPHA);
    if (transfer->taggedCartridgeCount != 0) {
        // An if-chain rather than a switch is there to match: a switch gives 8
        // words differing, 8 in shape
        if (transfer->screenStep == SEND_STATUS_CHOOSING) {
            f_StatusPointerAngle += POINTER_SWING_STEP;
            if (f_StatusPointerAngle > M_PI) {
                f_StatusPointerAngle -= M_PI;
            }

            pointerSwing = sinf(f_StatusPointerAngle) * POINTER_SWING_DISTANCE;
            gDPPipeSync(gfx++);
            gDPSetTextureLUT(gfx++, G_TT_RGBA16);
            LoadSmallPalette(&gfx, 0, ImagePalette(transfer->pointerImage));
            // The original loads the controller icons again, which the pointer
            // replaces
            LoadNibbleTextureBlock(
                &gfx,
                ImageTexels(transfer->transferPakIconsImage),
                G_IM_FMT_CI,
                TRANSFER_PAK_ICON_SIZE,
                TRANSFER_PAK_ICON_SIZE * MAXCONTROLLERS,
                0,
                0,
                0,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gDPLoadTextureTile_4b(
                gfx++,
                TextureAddress(ImageTexels(transfer->pointerImage)),
                G_IM_FMT_CI,
                POINTING_HAND_SIZE,
                0,
                0,
                0,
                POINTING_HAND_SIZE - 1,
                POINTING_HAND_LOAD_HEIGHT - 1,
                0,
                0,
                0,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            pointerRestLeft = (transfer->chosenCartridgeIndex * TRANSFER_PAK_ICON_SPACING) +
                (TRANSFER_PAK_ICONS_LEFT - POINTING_HAND_SIZE);
            pointerLeft = pointerRestLeft - pointerSwing;
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(pointerLeft),
                QuarterPixels(POINTER_TOP),
                QuarterPixels(pointerLeft + POINTING_HAND_SIZE),
                QuarterPixels(POINTER_TOP + POINTING_HAND_SIZE),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
        }
        else if (transfer->screenStep == SEND_STATUS_READY) {
            DrawCursorBox(
                &gfx,
                TRANSFER_PAK_ICONS_LEFT - CONTROLLER_CURSOR_MARGIN +
                    (transfer->chosenCartridgeIndex * TRANSFER_PAK_ICON_SPACING),
                TRANSFER_PAK_ICONS_TOP - CONTROLLER_CURSOR_MARGIN
            );
        }
    }

    *displayListHead = gfx;
}

// Draws the faces of the characters already brought over, in their slots
void DrawImportedFaces(Gfx **displayListHead, GameBoyTransfer *transfer) {
    Gfx *gfx = *displayListHead;
    s32 gameBoyCharacterIndex;
    s32 golferIndex;

    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    for (gameBoyCharacterIndex = 0; gameBoyCharacterIndex != GAME_BOY_CHARACTER_COUNT;
        gameBoyCharacterIndex++) {
        if (g_GameBoyCharacters[gameBoyCharacterIndex].status ==
            GAME_BOY_CHARACTER_STATUS_PRESENT) {
            golferIndex = g_GameBoyCharacters[gameBoyCharacterIndex].golferIndex;
            LoadLargePalette(&gfx, transfer->facePalettes[golferIndex]);
            LoadTextureBlock(
                &gfx,
                transfer->faceImages[golferIndex],
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                FACE_FRAME_SIZE,
                FACE_FRAME_SIZE,
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
                QuarterPixels(FACES_LEFT + (gameBoyCharacterIndex * FACE_SPACING)),
                QuarterPixels(FACES_TOP),
                QuarterPixels(
                    FACES_LEFT + (gameBoyCharacterIndex * FACE_SPACING) + FACE_FRAME_SIZE
                ),
                QuarterPixels(FACES_TOP + FACE_FRAME_SIZE),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
        }
    }

    *displayListHead = gfx;
}

// Draws the options screen's faces; DrawTransmitMenu draws the options
void DrawTransmitOptionFaces(Gfx **displayListHead, GameBoyTransfer *transfer) {
    Gfx *gfx = *displayListHead;

    DrawImportedFaces(&gfx, transfer);
    *displayListHead = gfx;
}

// Whether a cartridge character is on the list: read in, or being brought over
static inline Boolean IsListed(ListingState listing) {
    return listing == LISTING_SHOWN || listing == LISTING_BRINGING;
}

// Draws the Bring Characters screen: the faces already brought over, the
// cartridge characters on the list with their controllers, the cursor, and the
// arrows when the list scrolls. A character being brought over moves from the
// list to its slot.
void DrawBringCharactersScreen(Gfx **displayListHead, GameBoyTransfer *transfer) {
    Gfx *gfx = *displayListHead;
    s32 index;
    s32 shownCount;
    s32 listed;
    s32 left;
    s32 slotLeft;
    s32 top;
    s32 golferIndex;

    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    DrawImportedFaces(&gfx, transfer);
    LoadSmallPalette(&gfx, 0, ImagePalette(transfer->transferPakIconsImage));
    LoadNibbleTextureBlock(
        &gfx,
        ImageTexels(transfer->transferPakIconsImage),
        G_IM_FMT_CI,
        TRANSFER_PAK_ICON_SIZE,
        TRANSFER_PAK_ICON_SIZE * MAXCONTROLLERS,
        0,
        0,
        0,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    shownCount = 0;
    for (index = transfer->firstShownListedIndex; index != CARTRIDGE_CHARACTER_COUNT; index++) {
        if (transfer->cartridgeCharacters[transfer->listedCharacters[index]].status ==
                GAME_BOY_CHARACTER_STATUS_PRESENT &&
            IsListed(transfer->listingStates[transfer->listedCharacters[index]])) {
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(LISTED_FACES_LEFT + (shownCount * LISTED_FACE_SPACING)),
                QuarterPixels(LISTED_ICONS_TOP),
                QuarterPixels(
                    LISTED_FACES_LEFT + (shownCount * LISTED_FACE_SPACING) + TRANSFER_PAK_ICON_SIZE
                ),
                QuarterPixels(LISTED_ICONS_TOP + TRANSFER_PAK_ICON_SIZE),
                G_TX_RENDERTILE,
                0,
                TexelCoordinate(
                    transfer->listedCharacters[index] / CARTRIDGE_SAVE_COUNT *
                    TRANSFER_PAK_ICON_SIZE
                ),
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
            shownCount++;
            if (shownCount == SHOWN_LISTED_COUNT) {
                break;
            }
        }
    }

    LoadSmallPalette(&gfx, 0, ImagePalette(transfer->statBarsImage));
    gDPLoadTextureTile_4b(
        gfx++,
        TextureAddress(ImageTexels(transfer->statBarsImage)),
        G_IM_FMT_CI,
        CURVE_BAR_WIDTH,
        0,
        0,
        0,
        CURVE_BAR_WIDTH - 1,
        CURVE_BAR_HEIGHT - 1,
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
        QuarterPixels(CURVE_BAR_LEFT),
        QuarterPixels(CURVE_BAR_TOP),
        QuarterPixels(CURVE_BAR_LEFT + CURVE_BAR_WIDTH),
        QuarterPixels(CURVE_BAR_TOP + CURVE_BAR_HEIGHT),
        G_TX_RENDERTILE,
        0,
        0,
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );
    shownCount = 0;
    for (index = transfer->firstShownListedIndex; index != CARTRIDGE_CHARACTER_COUNT; index++) {
        listed = transfer->listedCharacters[index];
        if (transfer->cartridgeCharacters[listed].status == GAME_BOY_CHARACTER_STATUS_PRESENT &&
            IsListed(transfer->listingStates[listed])) {
            golferIndex = transfer->cartridgeCharacters[listed].golferIndex;
            left = LISTED_FACES_LEFT + (shownCount * LISTED_FACE_SPACING);
            top = LISTED_FACES_TOP;
            if (transfer->listingStates[listed] == LISTING_BRINGING) {
                slotLeft = FACES_LEFT + (transfer->gameBoyCharacterIndex * FACE_SPACING);
                left += (slotLeft - left) * transfer->bringingFrame / (BRINGING_FRAME_COUNT - 1);
                top += (FACES_TOP - top) * transfer->bringingFrame / (BRINGING_FRAME_COUNT - 1);
            }

            LoadLargePalette(&gfx, transfer->facePalettes[golferIndex]);
            LoadTextureBlock(
                &gfx,
                transfer->faceImages[golferIndex],
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                FACE_FRAME_SIZE,
                FACE_FRAME_SIZE,
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
                QuarterPixels(left),
                QuarterPixels(top),
                QuarterPixels(left + FACE_FRAME_SIZE),
                QuarterPixels(top + FACE_FRAME_SIZE),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
            shownCount++;
            if (shownCount == SHOWN_LISTED_COUNT) {
                break;
            }
        }
    }

    if (g_GameBoyTransfer.listedCharacterCount != 0) {
        if (transfer->screenStep == BRING_CHOOSING) {
            DrawCursorBox(
                &gfx,
                LISTED_FACES_LEFT - BRING_CURSOR_MARGIN +
                    ((transfer->listedCharacterIndex - transfer->firstShownListedIndex) *
                        LISTED_FACE_SPACING),
                LISTED_FACES_TOP - BRING_CURSOR_MARGIN
            );
        }
        else if (transfer->screenStep == BRING_CHOOSING_SLOT) {
            DrawCursorBox(
                &gfx,
                FACES_LEFT - BRING_CURSOR_MARGIN + (transfer->gameBoyCharacterIndex * FACE_SPACING),
                FACES_TOP - BRING_CURSOR_MARGIN
            );
        }
    }

    gSPDisplayList(gfx++, g_DecalTextureSetup);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    if (transfer->screenStep == BRING_CHOOSING) {
        f_CharacterArrowFrame = (f_CharacterArrowFrame + 1) % ARROW_CYCLE_TICKS;
        LoadSmallPalette(&gfx, 0, ImagePalette(transfer->arrowsImage));
        LoadNibbleTextureBlock(
            &gfx,
            ImageTexelsAt(
                transfer->arrowsImage,
                f_CharacterArrowFrame / ARROW_FRAME_TICKS * GREEN_ARROW_FRAME_SIZE
            ),
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
        gDPPipeSync(gfx++);
        if (transfer->firstShownListedIndex > 0) {
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(LEFT_ARROW_LEFT),
                QuarterPixels(ARROWS_TOP),
                QuarterPixels(LEFT_ARROW_LEFT + GREEN_ARROW_SIZE),
                QuarterPixels(ARROWS_TOP + GREEN_ARROW_SIZE),
                G_TX_RENDERTILE,
                0,
                TexelCoordinate(GreenArrowRow(MENU_ARROW_LEFT)),
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
        }

        if (transfer->firstShownListedIndex < transfer->listedCharacterCount - SHOWN_LISTED_COUNT) {
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(RIGHT_ARROW_LEFT),
                QuarterPixels(ARROWS_TOP),
                QuarterPixels(RIGHT_ARROW_LEFT + GREEN_ARROW_SIZE),
                QuarterPixels(ARROWS_TOP + GREEN_ARROW_SIZE),
                G_TX_RENDERTILE,
                0,
                TexelCoordinate(GreenArrowRow(MENU_ARROW_RIGHT)),
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
        }
    }

    *displayListHead = gfx;
}

// Draws the Send Experience screen: the faces with their cursor and the
// cartridges' controller icons, dimming the unchosen ones once sent
void DrawSendExperienceScreen(Gfx **displayListHead, GameBoyTransfer *transfer) {
    Gfx *gfx = *displayListHead;
    s32 index;

    // A second counter alongside index is there to match: index in its place
    // gives 70 words differing (4 in shape), and counting it in the for 22 (4)
    s32 slot;

    DrawImportedFaces(&gfx, transfer);
    if (transfer->screenStep == SEND_EXPERIENCE_CHOOSING) {
        DrawCursorBox(
            &gfx,
            FACES_LEFT - FACE_CURSOR_MARGIN + (transfer->gameBoyCharacterIndex * FACE_SPACING),
            FACES_TOP - FACE_CURSOR_MARGIN
        );
        gDPPipeSync(gfx++);
        gDPSetTexturePersp(gfx++, G_TP_NONE);
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    }

    LoadSmallPalette(&gfx, 0, ImagePalette(transfer->transferPakIconsImage));
    LoadNibbleTextureBlock(
        &gfx,
        ImageTexels(transfer->transferPakIconsImage),
        G_IM_FMT_CI,
        TRANSFER_PAK_ICON_SIZE,
        TRANSFER_PAK_ICON_SIZE * MAXCONTROLLERS,
        0,
        0,
        0,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    slot = 0;
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, OPAQUE_ALPHA);
    for (index = 0; index != transfer->taggedCartridgeCount; index++) {
        if (transfer->screenStep == SEND_EXPERIENCE_SENT &&
            slot != transfer->chosenCartridgeIndex) {
            gDPSetPrimColor(gfx++, 0, 0, 127, 127, 127, OPAQUE_ALPHA);
        }
        else {
            gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, OPAQUE_ALPHA);
        }

        DrawTextureRectangle(
            &gfx,
            QuarterPixels(TRANSFER_PAK_ICONS_LEFT + (slot * TRANSFER_PAK_ICON_SPACING)),
            QuarterPixels(TRANSFER_PAK_ICONS_TOP),
            QuarterPixels(
                TRANSFER_PAK_ICONS_LEFT + (slot * TRANSFER_PAK_ICON_SPACING) +
                TRANSFER_PAK_ICON_SIZE
            ),
            QuarterPixels(TRANSFER_PAK_ICONS_TOP + TRANSFER_PAK_ICON_SIZE),
            G_TX_RENDERTILE,
            0,
            TexelCoordinate(transfer->taggedControllers[index] * TRANSFER_PAK_ICON_SIZE),
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        slot++;
    }

    *displayListHead = gfx;
}

void DrawAccessingSign(Gfx **displayListHead);
void DrawNotice(Gfx **displayListHead);
void DrawPressingHand(Gfx **displayListHead);

// Whether DrawTransmitMenu shows the transfer mode's options on a screen
#define AreOptionsShown(screen) \
    ((screen) == TRANSMIT_SCREEN_OPTIONS || \
        (screen) == TRANSMIT_SCREEN_LEAVING_FOR_TITLE || \
        (screen) == TRANSMIT_SCREEN_LEAVING_FOR_MENU || \
        (screen) == TRANSMIT_SCREEN_RUMBLE_PAK_PROMPT)

// Draws the backdrop, the title, the current screen and the options, notice
// and hand over it
void DrawTransmitMenu(Gfx **displayListHead) {
    // The drawing helpers take this pointer, and the fields are read through
    // it too: reading them from g_GameBoyTransfer, and passing its address,
    // gives 162 words differing, 40 in shape
    GameBoyTransfer *transfer = &g_GameBoyTransfer;
    Gfx *gfx = *displayListHead;
    s32 index;
    s32 rowCount;
    // Apart from rowCount, which is there to match: reusing rowCount for the
    // panel's strips gives 180 words differing, 20 in shape
    s32 panelRowCount;
    TransmitOption option;
    s32 width;
    s32 height;
    s32 pointerLeft;
    s32 pointerSwing;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetColorDither(gfx++, G_CD_DISABLE);
    gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    LoadLargePalette(&gfx, ImagePalette(transfer->courseSceneImage));
    for (index = 0; index != COURSE_SCENE_STRIP_COUNT; index++) {
        rowCount = (index != COURSE_SCENE_STRIP_COUNT - 1) ? COURSE_SCENE_STRIP_ROW_COUNT : 1;
        if (index == 0) {
            LoadTextureBlock(
                &gfx,
                LargeImageTexels(transfer->courseSceneImage),
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                COURSE_SCENE_WIDTH,
                COURSE_SCENE_STRIP_ROW_COUNT + COURSE_SCENE_SEAM_ROW_COUNT,
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
                TEXEL_STEP_ONE / 2,
                TEXEL_STEP_ONE / 2
            );
        }
        else {
            LoadTextureBlock(
                &gfx,
                LargeImageTexels(transfer->courseSceneImage) +
                    (index * (COURSE_SCENE_WIDTH * COURSE_SCENE_STRIP_ROW_COUNT)),
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                COURSE_SCENE_WIDTH,
                rowCount + COURSE_SCENE_SEAM_ROW_COUNT,
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
                QuarterPixels(COURSE_SCENE_LEFT),
                QuarterPixels(
                    COURSE_SCENE_TOP + (index * COURSE_SCENE_STRIP_ROW_COUNT * COURSE_SCENE_SCALE)
                ),
                QuarterPixels(COURSE_SCENE_LEFT + (COURSE_SCENE_WIDTH * COURSE_SCENE_SCALE)),
                QuarterPixels(
                    COURSE_SCENE_TOP + (index * COURSE_SCENE_STRIP_ROW_COUNT * COURSE_SCENE_SCALE) +
                    (rowCount * COURSE_SCENE_SCALE)
                ),
                G_TX_RENDERTILE,
                0,
                TexelCoordinate(1),
                TEXEL_STEP_ONE / 2,
                TEXEL_STEP_ONE / 2
            );
        }
    }

    LoadLargePalette(&gfx, ImagePalette(transfer->titleImage));
    for (index = 0; index != TITLE_STRIP_COUNT; index++) {
        gDPLoadTextureTile(
            gfx++,
            TextureAddress(
                LargeImageTexels(transfer->titleImage) + (index * (TITLE_WIDTH * TITLE_STRIP_ROWS))
            ),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            TITLE_WIDTH,
            0,
            0,
            0,
            TITLE_WIDTH - 1,
            TITLE_STRIP_ROWS - 1,
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
            QuarterPixels(TITLE_LEFT),
            QuarterPixels(TITLE_TOP + (index * TITLE_STRIP_ROWS)),
            QuarterPixels(TITLE_LEFT + TITLE_WIDTH),
            QuarterPixels(TITLE_TOP + (index * TITLE_STRIP_ROWS) + TITLE_STRIP_ROWS),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
    }

    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    for (index = 0; index != WOOD_PANEL_STRIP_COUNT; index++) {
        panelRowCount = (index != WOOD_PANEL_STRIP_COUNT - 1) ? WOOD_PANEL_STRIP_ROWS : 1;
        gDPLoadTextureTile(
            gfx++,
            TextureAddress(
                ImageBody(transfer->woodPanelImage) +
                (index * (WOOD_PANEL_WIDTH * WOOD_PANEL_STRIP_ROWS * 2))
            ),
            G_IM_FMT_RGBA,
            G_IM_SIZ_16b,
            WOOD_PANEL_WIDTH,
            0,
            0,
            0,
            WOOD_PANEL_WIDTH - 1,
            panelRowCount - 1,
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
            QuarterPixels(WOOD_PANEL_LEFT),
            QuarterPixels(WOOD_PANEL_TOP + (index * WOOD_PANEL_STRIP_ROWS)),
            QuarterPixels(WOOD_PANEL_LEFT + WOOD_PANEL_WIDTH),
            QuarterPixels(WOOD_PANEL_TOP + (index * WOOD_PANEL_STRIP_ROWS) + panelRowCount),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
    }

    switch (transfer->screen) {
        case TRANSMIT_SCREEN_OPTIONS:
            DrawTransmitOptionFaces(&gfx, transfer);
            break;
        case TRANSMIT_SCREEN_BRING_CHARACTERS:
            DrawBringCharactersScreen(&gfx, transfer);
            break;
        case TRANSMIT_SCREEN_SEND_STATUS:
            DrawSendStatusScreen(&gfx, transfer);
            break;
        case TRANSMIT_SCREEN_SEND_EXPERIENCE:
            DrawSendExperienceScreen(&gfx, transfer);
            break;
    }

    if (transfer->screen == TRANSMIT_SCREEN_OPTIONS) {
        MeasureMessage(
            f_ModeOptions[transfer->transferMode][transfer->optionIndex] +
                MESSAGE_FIRST_TRANSMIT_OPTION,
            FONT_SIZE_LARGE,
            &width,
            &height,
            0
        );
        // The pointer swings in and out over half a turn of the angle
        f_OptionPointerAngle += POINTER_SWING_STEP;
        if (f_OptionPointerAngle > M_PI) {
            f_OptionPointerAngle -= M_PI;
        }

        pointerSwing = sinf(f_OptionPointerAngle) * POINTER_SWING_DISTANCE;
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_RGBA16);
        LoadSmallPalette(&gfx, 0, ImagePalette(transfer->pointerImage));
        gDPLoadTextureTile_4b(
            gfx++,
            TextureAddress(ImageTexels(transfer->pointerImage)),
            G_IM_FMT_CI,
            POINTING_HAND_SIZE,
            0,
            0,
            0,
            POINTING_HAND_SIZE - 1,
            POINTING_HAND_LOAD_HEIGHT - 1,
            0,
            0,
            0,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        pointerLeft =
            OPTIONS_CENTER_X - OPTION_POINTER_GAP - POINTING_HAND_SIZE - (width / 2) - pointerSwing;
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(pointerLeft),
            QuarterPixels(OPTION_POINTER_TOP + (transfer->optionIndex * OPTION_SPACING)),
            QuarterPixels(pointerLeft + POINTING_HAND_SIZE),
            QuarterPixels(
                OPTION_POINTER_TOP + (transfer->optionIndex * OPTION_SPACING) + POINTING_HAND_SIZE
            ),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
    }

    SetUpTextDrawing(&gfx);
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, OPAQUE_ALPHA);
    if (AreOptionsShown(transfer->screen)) {
        for (index = 0; index != MODE_OPTION_COUNT; index++) {
            option = f_ModeOptions[transfer->transferMode][index];
            if (option != TRANSMIT_OPTION_NONE) {
                SetTextSlot(
                    OPTIONS_CENTER_X,
                    OPTIONS_TOP + (index * OPTION_SPACING),
                    option + MESSAGE_FIRST_TRANSMIT_OPTION,
                    TEXT_SLOT_LARGE_FONT,
                    index,
                    TEXT_FLAG_CENTERED | TEXT_FLAG_DRAWN_BY_OWNER,
                    255,
                    255,
                    255,
                    OPAQUE_ALPHA
                );
                DrawLargeTextSlot(&gfx, index);
            }
        }

        SetTextSlot(
            HINT_LEFT,
            HINT_TOP,
            f_ModeOptions[transfer->transferMode][transfer->optionIndex] +
                MESSAGE_FIRST_TRANSMIT_OPTION_HINT,
            TEXT_SLOT_LARGE_FONT,
            HINT_TEXT_SLOT,
            TEXT_FLAG_DRAWN_BY_OWNER,
            255,
            255,
            255,
            OPAQUE_ALPHA
        );
        DrawLargeTextSlot(&gfx, HINT_TEXT_SLOT);
    }

    if (transfer->screen == TRANSMIT_SCREEN_SEND_STATUS) {
        DrawSendStatusHint(&gfx, transfer);
    }

    if (transfer->screen == TRANSMIT_SCREEN_BRING_CHARACTERS) {
        DrawBringCharactersText(&gfx, transfer);
    }

    if (transfer->screen == TRANSMIT_SCREEN_SEND_EXPERIENCE) {
        DrawSendExperienceText(&gfx, transfer);
    }

    DrawNotice(&gfx);
    gSPDisplayList(gfx++, g_DecalTextureSetup);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    if (transfer->isAccessing == TRUE) {
        DrawAccessingSign(&gfx);
    }

    if (transfer->pressingHandPosition != PRESSING_HAND_HIDDEN) {
        DrawPressingHand(&gfx);
    }

    *displayListHead = gfx;
}

// Forgets the cartridge characters read before, so that the Bring Characters
// screen reads them again
void ClearCartridgeCharacters(void) {
    // The transfer pointer is there to match: reading g_GameBoyTransfer
    // directly gives 28 words differing, 12 in shape
    GameBoyTransfer *transfer = &g_GameBoyTransfer;
    s32 controllerIndex;
    s32 save;

    if (transfer->screen == TRANSMIT_SCREEN_BRING_CHARACTERS) {
        transfer->readStep = 0;
        ResetBringCharacters();
        for (controllerIndex = 0; controllerIndex != MAXCONTROLLERS; controllerIndex++) {
            for (save = 0; save != CARTRIDGE_SAVE_COUNT; save++) {
                transfer->listingStates[(controllerIndex * CARTRIDGE_SAVE_COUNT) + save] =
                    LISTING_NONE;
                transfer->cartridgeCharacters[(controllerIndex * CARTRIDGE_SAVE_COUNT) + save]
                    .status = 0;
            }
        }

        transfer->listedCharacterCount = 0;
    }
}

// Adds the character's experience to its save on the Game Boy cartridge it came
// from, looking through every controller's pak for it, and rewrites the save's
// checksums.
//
// storedSum is masked to 16 bits beside the computed sum, to match, though
// its two bytes can't exceed that. The mask's references raise storedSum's
// global-alloc priority above recordOffset's, so storedSum takes $s1 first,
// then recordOffset $s2 and saveIndex $s1, as in the original. Without it, 40
// words differ, none in shape: recordOffset (28 references over a live
// length of 159, priority 7044) goes first and takes $s1. Types, declaration
// orders, while loops and building storedSum in two steps found no natural
// form that does the same.
//
// The clamp repeats the sum in each arm, which gives the original's copy of
// the sum into the stored experience's register: adding into the experience
// and then clamping gives 9 words (1 in shape), and a ?: repeating the sum
// 19 (7).
ExperienceWriteResult WriteExperienceToCartridge(s32 gameBoyCharacterIndex) {
    // The transfer pointer is there to match: reaching g_GameBoyTransfer
    // directly gives 154 words differing, 66 in shape
    GameBoyTransfer *transfer = &g_GameBoyTransfer;
    u8 characterData[CHARACTER_DATA_SIZE];
    u8 save[CARTRIDGE_SAVE_SIZE];
    GameBoyCharacter *gameBoyCharacter;
    CartridgeFault fault = CARTRIDGE_FAULT_NONE;
    s32 controllerIndex;
    s32 result;
    Boolean isSignatureWrong;
    s32 index;
    s32 saveIndex;
    s32 recordOffset;
    s32 dataAddress;
    s32 dataSize;
    s32 storedChecksum;
    s32 checksum;
    s32 experience;

    transfer->hasMissingPak = FALSE;
    transfer->hasOtherPak = FALSE;
    transfer->hasMissingCartridge = FALSE;
    transfer->hasWrongCartridge = FALSE;
    transfer->openedCartridgeCount = 0;
    result = PAK_ACCESS_OK;
    for (controllerIndex = 0; controllerIndex != g_ControllerCount; controllerIndex++) {
        transfer->transferResult = OpenGameBoyPakWithRetries(
            g_PlayerControllerPorts[controllerIndex],
            &transfer->cartridgeHeader,
            &transfer->pakFile
        );
        if (transfer->transferResult == PAK_RESULT_NO_PAK) {
            transfer->hasMissingPak = TRUE;
        }

        if (transfer->transferResult == PAK_RESULT_OTHER_PAK) {
            transfer->hasOtherPak = TRUE;
        }

        if (transfer->transferResult == PAK_RESULT_NO_CARTRIDGE) {
            transfer->hasMissingCartridge = TRUE;
        }

        if (transfer->transferResult == PAK_RESULT_WRONG_CARTRIDGE) {
            transfer->hasWrongCartridge = TRUE;
        }

        if (transfer->transferResult == PAK_RESULT_OK) {
            recordOffset = SAVE_RECORDS_OFFSET;
            isSignatureWrong = FALSE;
            gameBoyCharacter = &g_GameBoyCharacters[gameBoyCharacterIndex];
            saveIndex = gameBoyCharacter->saveIndex;
            transfer->openedCartridgeCount++;
            osSyncPrintf("PHASE 1\n");
            result = nuContGBPakFread(
                &transfer->pakFile,
                CARTRIDGE_RAM_ADDRESS,
                save,
                CARTRIDGE_SAVE_SIZE
            );
            // Each pak error powers off and continues, to match: jumping to the
            // loop's last power off from the five read and write errors instead
            // gives 182 words differing, 68 in shape
            if (result != PAK_ACCESS_OK) {
                nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
                continue;
            }

            osSyncPrintf("PHASE 2\n");
            for (index = 0; index != CARTRIDGE_TAG_SIZE; index++) {
                if (save[SAVE_SIGNATURE_OFFSET + index] != f_CartridgeSaveSignature[index]) {
                    isSignatureWrong = TRUE;
                }
            }

            osSyncPrintf("PHASE 3\n");
            if (isSignatureWrong == TRUE) {
                fault = CARTRIDGE_FAULT_UNINITIALIZED;
            }

            if (ComputeSaveChecksum(save) != GetStoredChecksum(save)) {
                fault = CARTRIDGE_FAULT_CORRUPTED;
            }
            else {
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
                    (save[recordOffset + RECORD_ADDRESS_OFFSET + 1] << BITS_PER_BYTE);
                dataSize = save[recordOffset + RECORD_SIZE_OFFSET] +
                    (save[recordOffset + RECORD_SIZE_OFFSET + 1] << BITS_PER_BYTE);
                osSyncPrintf("PHASE 6\n");
                result = nuContGBPakFread(
                    &transfer->pakFile,
                    dataAddress - STATUS_ADDRESS_BIAS,
                    characterData,
                    dataSize
                );
                if (result != PAK_ACCESS_OK) {
                    nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
                    continue;
                }

                checksum = 0;
                osSyncPrintf("PHASE 7\n");
                storedChecksum = save[recordOffset + RECORD_CHECKSUM_OFFSET] |
                    (save[recordOffset + RECORD_CHECKSUM_OFFSET + 1] << BITS_PER_BYTE);
                for (index = 0; index != dataSize; index++) {
                    checksum += characterData[index];
                }

                osSyncPrintf("PHASE 8\n");
                checksum &= 0xFFFF;
                storedChecksum &= 0xFFFF;
                if (checksum != storedChecksum) {
                    nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
                    fault = CARTRIDGE_FAULT_CORRUPTED;
                    continue;
                }

                osSyncPrintf("PHASE 9\n");
                if (IsCharacterFromSave(gameBoyCharacter, characterData)) {
                    osSyncPrintf("PHASE 9.5\n");
                    experience =
                        ((characterData[DATA_EXPERIENCE_OFFSET + 1] << BITS_PER_BYTE) |
                            characterData[DATA_EXPERIENCE_OFFSET]);
                    if (gameBoyCharacter->experience + experience > EXPERIENCE_MAX) {
                        experience = EXPERIENCE_MAX;
                    }
                    else {
                        experience = gameBoyCharacter->experience + experience;
                    }
                    osSyncPrintf("PHASE 10\n");
                    characterData[DATA_EXPERIENCE_OFFSET + 1] = experience >> BITS_PER_BYTE;
                    characterData[DATA_EXPERIENCE_OFFSET] = experience;
                    characterData[DATA_EXPERIENCE_MARK_OFFSET] = EXPERIENCE_ADDED_MARK;
                    result = nuContGBPakFwrite(
                        &transfer->pakFile,
                        dataAddress - STATUS_ADDRESS_BIAS + DATA_EXPERIENCE_OFFSET,
                        &characterData[DATA_EXPERIENCE_OFFSET],
                        DATA_EXPERIENCE_WRITE_SIZE
                    );
                    if (result != PAK_ACCESS_OK) {
                        nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
                        continue;
                    }

                    checksum = 0;
                    osSyncPrintf("PHASE 11\n");
                    for (index = 0; index != dataSize; index++) {
                        checksum += characterData[index];
                    }

                    checksum &= 0xFFFF;
                    osSyncPrintf("write sum %04x\n", checksum);
                    osSyncPrintf("PHASE 12\n");
                    save[recordOffset + RECORD_CHECKSUM_OFFSET] = checksum;
                    save[recordOffset + RECORD_CHECKSUM_OFFSET + 1] = checksum >> BITS_PER_BYTE;
                    result = nuContGBPakFwrite(
                        &transfer->pakFile,
                        CARTRIDGE_RAM_ADDRESS + recordOffset + RECORD_CHECKSUM_OFFSET,
                        &save[recordOffset + RECORD_CHECKSUM_OFFSET],
                        SAVE_HALFWORD_SIZE
                    );
                    if (result != PAK_ACCESS_OK) {
                        nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
                        continue;
                    }

                    osSyncPrintf("PHASE 13\n");
                    checksum = ComputeSaveChecksum(save);
                    save[SAVE_CHECKSUM_OFFSET] = checksum;
                    save[SAVE_CHECKSUM_OFFSET + 1] = checksum >> BITS_PER_BYTE;
                    result = nuContGBPakFwrite(
                        &transfer->pakFile,
                        CARTRIDGE_RAM_ADDRESS + SAVE_CHECKSUM_OFFSET,
                        &save[SAVE_CHECKSUM_OFFSET],
                        SAVE_HALFWORD_SIZE
                    );
                    if (result != PAK_ACCESS_OK) {
                        nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
                        continue;
                    }

                    nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
                    osSyncPrintf("PHASE 14\n");
                    gameBoyCharacter->experience = 0;
                    transfer->transferResult = PAK_RESULT_OK;
                    return EXPERIENCE_WRITE_DONE;
                }
            }
        }

        nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
    }

    if (transfer->openedCartridgeCount == 1) {
        if (fault == CARTRIDGE_FAULT_UNINITIALIZED) {
            transfer->transferResult = PAK_RESULT_UNINITIALIZED;
            return EXPERIENCE_WRITE_FAILED;
        }

        if (fault == CARTRIDGE_FAULT_CORRUPTED) {
            transfer->transferResult = PAK_RESULT_CORRUPTED;
            return EXPERIENCE_WRITE_FAILED;
        }
    }

    if (transfer->openedCartridgeCount > 0) {
        transfer->transferResult = PAK_RESULT_NO_CHARACTERS;
        return EXPERIENCE_WRITE_NOT_FROM_CARTRIDGE;
    }

    if (transfer->hasWrongCartridge == TRUE) {
        transfer->transferResult = PAK_RESULT_WRONG_CARTRIDGE;
    }
    else if (transfer->hasMissingCartridge == TRUE) {
        transfer->transferResult = PAK_RESULT_NO_CARTRIDGE;
    }
    else if (transfer->hasOtherPak == TRUE) {
        transfer->transferResult = PAK_RESULT_OTHER_PAK;
    }
    else if (result != PAK_ACCESS_OK) {
        transfer->transferResult = result;
        return EXPERIENCE_WRITE_PAK_ERROR;
    }
    else {
        transfer->transferResult = PAK_RESULT_NO_PAK;
    }

    return EXPERIENCE_WRITE_FAILED;
}

// Checks that the chosen cartridge is the one tagged and that its save is
// intact, and reports whether the save already holds a status, which
// WriteStatusToCartridge marks.
StatusCheck CheckStatusCartridge(s32 cartridgeIndex) {
    // The transfer pointer is there to match: reading g_GameBoyTransfer
    // directly gives 81 words differing, 19 in shape
    GameBoyTransfer *transfer = &g_GameBoyTransfer;
    u8 save[CARTRIDGE_SAVE_SIZE];
    Boolean isSignatureWrong;
    s32 index;
    PakResult result;
    s32 readResult;
    s32 computedChecksum;
    s32 storedChecksum;

    result = OpenGameBoyPakWithRetries(
        g_PlayerControllerPorts[g_GameBoyTransfer.taggedControllers[cartridgeIndex]],
        &transfer->cartridgeHeader,
        &transfer->pakFile
    );
    transfer->transferResult = result;
    if (result == PAK_RESULT_OK) {
        isSignatureWrong = FALSE;
        readResult =
            nuContGBPakFread(&transfer->pakFile, CARTRIDGE_RAM_ADDRESS, save, CARTRIDGE_SAVE_SIZE);
        if (readResult != PAK_ACCESS_OK) {
            osSyncPrintf("head error  2\n");
            transfer->transferResult = readResult;
            nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
            return STATUS_CHECK_READ_FAILED;
        }

        for (index = 0; index != CARTRIDGE_TAG_SIZE; index++) {
            if (save[SAVE_SIGNATURE_OFFSET + index] != f_CartridgeSaveSignature[index]) {
                isSignatureWrong = TRUE;
            }
        }

        for (index = 0; index != CARTRIDGE_TAG_SIZE; index++) {
            if (save[index] !=
                transfer->cartridgeTags[transfer->taggedControllers[cartridgeIndex]][index]) {
                osSyncPrintf("ライトマーク違反\n"); // "Write mark violation"
                transfer->transferResult = PAK_RESULT_REMOVED_RETRYING;
                nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
                return STATUS_CHECK_FAILED;
            }
        }

        computedChecksum = ComputeSaveChecksum(save);
        storedChecksum = GetStoredChecksum(save);
        osSyncPrintf("sum1 %04x\n", computedChecksum);
        osSyncPrintf("sum2 %04x\n", storedChecksum);
        if (computedChecksum != storedChecksum || isSignatureWrong) {
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
    }

    nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
    return STATUS_CHECK_FAILED;
}

void BuildStatusData(StatusData *status);

// Marks the chosen cartridge's save as holding a status and writes the status
// data where the save says, then the sums of both. It first checks that the
// cartridge is the one tagged and that its save is intact.
StatusWriteResult WriteStatusToCartridge(s32 cartridgeIndex) {
    // The transfer pointer is there to match: reading g_GameBoyTransfer
    // directly gives 80 words differing, 24 in shape
    GameBoyTransfer *transfer = &g_GameBoyTransfer;
    u8 save[CARTRIDGE_SAVE_SIZE];
    u8 statusData[STATUS_DATA_SIZE];
    s32 result;
    s32 index;
    s32 statusAddress;
    s32 statusSize;
    s32 checksum;

    transfer->transferResult = OpenGameBoyPakWithRetries(
        g_PlayerControllerPorts[transfer->taggedControllers[cartridgeIndex]],
        &transfer->cartridgeHeader,
        &transfer->pakFile
    );
    if (transfer->transferResult == PAK_RESULT_OK) {
        result =
            nuContGBPakFread(&transfer->pakFile, CARTRIDGE_RAM_ADDRESS, save, CARTRIDGE_SAVE_SIZE);
        if (result != PAK_ACCESS_OK) {
            nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
            transfer->transferResult = result;
            return STATUS_WRITE_PAK_ERROR;
        }

        for (index = 0; index != CARTRIDGE_TAG_SIZE; index++) {
            if (save[index] !=
                transfer->cartridgeTags[transfer->taggedControllers[cartridgeIndex]][index]) {
// "Write mark violation"; the original passes sum before setting it
#ifdef AVOID_UB
                osSyncPrintf("ライトマーク違反\n");
#else
                osSyncPrintf("ライトマーク違反\n", checksum);
#endif
                nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
                transfer->transferResult = PAK_RESULT_CARTRIDGE_REPLACED;
                return STATUS_WRITE_FAILED;
            }
        }

        checksum = ComputeSaveChecksum(save);
        if (checksum != GetStoredChecksum(save)) {
            transfer->transferResult = PAK_RESULT_LOOSE_PAK;
            nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
            return STATUS_WRITE_FAILED;
        }

        statusAddress = save[SAVE_STATUS_ADDRESS_OFFSET] +
            (save[SAVE_STATUS_ADDRESS_OFFSET + 1] << BITS_PER_BYTE);
        save[SAVE_HAS_STATUS_OFFSET] = TRUE;
        statusSize =
            save[SAVE_STATUS_SIZE_OFFSET] + (save[SAVE_STATUS_SIZE_OFFSET + 1] << BITS_PER_BYTE);
        result = nuContGBPakFwrite(
            &transfer->pakFile,
            CARTRIDGE_RAM_ADDRESS + SAVE_HAS_STATUS_OFFSET,
            &save[SAVE_HAS_STATUS_OFFSET],
            SAVE_HALFWORD_SIZE
        );
        if (result != PAK_ACCESS_OK) {
            nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
            transfer->transferResult = result;
            return STATUS_WRITE_PAK_ERROR;
        }

        BuildStatusData((StatusData *)statusData);
        result = nuContGBPakFwrite(
            &transfer->pakFile,
            statusAddress - STATUS_ADDRESS_BIAS,
            statusData,
            statusSize
        );
        if (result != PAK_ACCESS_OK) {
            nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
            transfer->transferResult = result;
            return STATUS_WRITE_PAK_ERROR;
        }

        checksum = 0;
        for (index = 0; index != statusSize; index++) {
            checksum += statusData[index];
        }

        checksum &= 0xFFFF;
        osSyncPrintf("write sum %04x\n", checksum);
        save[SAVE_STATUS_CHECKSUM_OFFSET] = checksum;
        save[SAVE_STATUS_CHECKSUM_OFFSET + 1] = checksum >> BITS_PER_BYTE;
        result = nuContGBPakFwrite(
            &transfer->pakFile,
            CARTRIDGE_RAM_ADDRESS + SAVE_STATUS_CHECKSUM_OFFSET,
            &save[SAVE_STATUS_CHECKSUM_OFFSET],
            SAVE_HALFWORD_SIZE
        );
        if (result != PAK_ACCESS_OK) {
            nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
            transfer->transferResult = result;
            return STATUS_WRITE_PAK_ERROR;
        }

        checksum = ComputeSaveChecksum(save) & 0xFFFF;
        save[SAVE_CHECKSUM_OFFSET] = checksum;
        save[SAVE_CHECKSUM_OFFSET + 1] = checksum >> BITS_PER_BYTE;
        osSyncPrintf("write sum2 %04x\n", checksum);
        result = nuContGBPakFwrite(
            &transfer->pakFile,
            CARTRIDGE_RAM_ADDRESS + SAVE_CHECKSUM_OFFSET,
            &save[SAVE_CHECKSUM_OFFSET],
            SAVE_HALFWORD_SIZE
        );
        if (result != PAK_ACCESS_OK) {
            nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
            transfer->transferResult = result;
            return STATUS_WRITE_PAK_ERROR;
        }

        nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
        transfer->transferResult = PAK_RESULT_OK;
        return STATUS_WRITE_DONE;
    }

    nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
    return STATUS_WRITE_FAILED;
}

// Reads one save of the Game Boy cartridges per call, opening each controller's
// pak before its saves, and lists the characters in the saves that check out.
// Returns PAK_RESULT_STILL_READING until the last save is read, then what was
// found.
PakResult ReadCartridgeCharacters(void) {
    // The transfer pointer is there to match: reading g_GameBoyTransfer
    // directly gives 234 words differing, 64 in shape
    GameBoyTransfer *transfer = &g_GameBoyTransfer;
    u8 save[CARTRIDGE_SAVE_SIZE];
    u8 characterData[CHARACTER_DATA_SIZE];
    s32 port;
    s32 saveIndex;
    s32 recordOffset;
    s32 checkedState; // How far the save has been checked
    s32 result;
    s32 attempt;
    s32 index;
    Boolean isSignatureWrong;
    s32 storedChecksum;
    s32 dataAddress;
    s32 dataSize;
    // Of the save, then of the character data. Sharing it is there to match: a
    // dataSum local gives 112 words differing, 8 in shape
    s32 checksum;
    Boolean isAlreadyBrought;
    s32 gameBoyCharacterIndex;

    if (transfer->readStep == 0) {
        nuContGBPakMgrInit();
        transfer->openedCartridgeCount = 0;
        transfer->listedCharacterCount = 0;
        transfer->hasMissingCartridge = FALSE;
        transfer->hasOtherPak = FALSE;
        transfer->hasMissingPak = FALSE;
        transfer->hasWrongCartridge = FALSE;
        transfer->hasUninitializedCartridge = FALSE;
    }

    if (CartridgeReadPhaseStep(transfer->readStep) == CARTRIDGE_OPENING_STEP) {
        port = g_PlayerControllerPorts[CartridgeReadController(transfer->readStep)];
        transfer->transferResult =
            OpenGameBoyPakWithRetries(port, &transfer->cartridgeHeader, &transfer->pakFile);
        osSyncPrintf("loadres %d  cont %d\n", transfer->transferResult, port);
        if (transfer->transferResult == PAK_RESULT_NO_PAK) {
            transfer->hasMissingPak = TRUE;
        }

        if (transfer->transferResult == PAK_RESULT_OTHER_PAK) {
            transfer->hasOtherPak = TRUE;
        }

        if (transfer->transferResult == PAK_RESULT_NO_CARTRIDGE) {
            transfer->hasMissingCartridge = TRUE;
        }

        if (transfer->transferResult == PAK_RESULT_WRONG_CARTRIDGE) {
            transfer->hasWrongCartridge = TRUE;
        }
    }
    else if (transfer->transferResult == PAK_RESULT_OK) {
        saveIndex = CartridgeReadPhaseStep(transfer->readStep) - CARTRIDGE_FIRST_SAVE_STEP;

        // The original starts the offset at the cartridge address of the first
        // record's address, which every save index replaces
        recordOffset = CARTRIDGE_RAM_ADDRESS + SAVE_RECORDS_OFFSET + RECORD_ADDRESS_OFFSET;
        checkedState = NOT_CHECKED;
        for (index = 0; index != CHARACTER_DATA_SIZE; index++) {
            characterData[index] = 0;
        }

        transfer->openedCartridgeCount++;
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
                &transfer->pakFile,
                CARTRIDGE_RAM_ADDRESS,
                save,
                CARTRIDGE_SAVE_SIZE
            );
            if (result == PAK_ACCESS_OK) {
                checksum = ComputeSaveChecksum(save);
                storedChecksum = GetStoredChecksum(save);
                isSignatureWrong = FALSE;
                for (index = 0; index != CARTRIDGE_TAG_SIZE; index++) {
                    if (save[SAVE_SIGNATURE_OFFSET + index] != f_CartridgeSaveSignature[index]) {
                        isSignatureWrong = TRUE;
                        transfer->hasUninitializedCartridge = TRUE;
                    }
                }

                if (checksum == storedChecksum && isSignatureWrong == FALSE) {
                    checkedState = SAVE_CHECKED;
                    osSyncPrintf("checksum1 OK! \n");
                    break;
                }
            }
        }

        // The nested tests are there to match: one && chain gives 37 words
        // differing, 13 in shape
        if (checkedState == SAVE_CHECKED) {
            if (result == PAK_ACCESS_OK) {
                if (save[recordOffset] == RECORD_PRESENT) {
                    checkedState = NOT_CHECKED;

                    // Working out the address in storedSum first is there to match:
                    // working it out in dataAddress gives 20 words differing, 10 in
                    // shape
                    storedChecksum = save[recordOffset + RECORD_ADDRESS_OFFSET] +
                        (save[recordOffset + RECORD_ADDRESS_OFFSET + 1] << BITS_PER_BYTE) -
                        STATUS_ADDRESS_BIAS;
                    dataSize = save[recordOffset + RECORD_SIZE_OFFSET] +
                        (save[recordOffset + RECORD_SIZE_OFFSET + 1] << BITS_PER_BYTE);
                    dataAddress = storedChecksum;
                    for (attempt = 0; attempt != DATA_READ_ATTEMPTS; attempt++) {
                        result = nuContGBPakFread(
                            &transfer->pakFile,
                            dataAddress,
                            characterData,
                            dataSize
                        );
                        storedChecksum = save[recordOffset + RECORD_CHECKSUM_OFFSET] |
                            (save[recordOffset + RECORD_CHECKSUM_OFFSET + 1] << BITS_PER_BYTE);
                        checksum = 0;
                        for (index = 0; index != dataSize; index++) {
                            checksum += characterData[index];
                        }

                        checksum &= 0xFFFF;
                        if (checksum == storedChecksum) {
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
                case PAK_ACCESS_OK:
                    isAlreadyBrought = FALSE;

                    // "Slot %d read successfully"
                    osSyncPrintf("スロット%d 読み込み成功しました  \n", saveIndex);
                    for (gameBoyCharacterIndex = 0;
                        gameBoyCharacterIndex != GAME_BOY_CHARACTER_COUNT;
                        gameBoyCharacterIndex++) {
                        if (g_GameBoyCharacters[gameBoyCharacterIndex].status ==
                                GAME_BOY_CHARACTER_STATUS_PRESENT &&
                            IsCharacterFromSave(
                                &g_GameBoyCharacters[gameBoyCharacterIndex],
                                characterData
                            )) {
                            isAlreadyBrought = TRUE;
                            transfer
                                ->cartridgeCharacters
                                    [(CartridgeReadController(transfer->readStep) *
                                         CARTRIDGE_SAVE_COUNT) +
                                        saveIndex]
                                .status = GAME_BOY_CHARACTER_STATUS_BROUGHT_OVER;
                            ListPresentCharacters();
                            break;
                        }
                    }

                    if (!isAlreadyBrought) {
                        // transferResult is OK here, so the original never skips a save
                        if (transfer->transferResult == PAK_RESULT_NO_CHARACTERS) {
                            SkipCartridgeSave(
                                characterData,
                                CartridgeReadController(transfer->readStep),
                                saveIndex,
                                transfer->cartridgeHeader.game_title,
                                transfer->cartridgeHeader.company_code
                            );
                        }
                        else {
                            DecodeCartridgeCharacter(
                                characterData,
                                CartridgeReadController(transfer->readStep),
                                saveIndex,
                                transfer->cartridgeHeader.company_code
                            );
                        }

                        transfer->listingStates
                            [(CartridgeReadController(transfer->readStep) * CARTRIDGE_SAVE_COUNT) +
                                saveIndex] = LISTING_SHOWN;
                        transfer->listedCharacterCount++;
                        ListPresentCharacters();
                    }

                    break;
                case PFS_ERR_NOPACK:
                    transfer->hasMissingPak = TRUE;
                    osSyncPrintf("3rd PFS_ERR_NOPACK  \n");
                    break;
                case PFS_ERR_DEVICE:
                    transfer->hasOtherPak = TRUE;
                    osSyncPrintf("3rd PFS_ERR_DEVICE  \n");
                    break;
                case PFS_ERR_CONTRFAIL:
                    transfer->hasIgnoredControllerFailure = TRUE;
                    osSyncPrintf("3rd PFS_ERR_CONTRFAIL  \n");
                    break;
                case PFS_ERR_NO_GBCART:
                    transfer->hasMissingCartridge = TRUE;
                    osSyncPrintf("3rd PFS_ERR_NO_GBCART  \n");
                    break;
                case PFS_ERR_NEW_GBCART:
                    osSyncPrintf("3rd PFS_ERR_NEW_GBCART  \n");
                    break;
            }
        }
    }

    if (CartridgeReadPhaseStep(transfer->readStep) == CARTRIDGE_READ_PHASE_SIZE - 1) {
        nuContGBPakPower(&transfer->pakFile, NU_CONT_GBPAK_POWER_OFF);
    }

    if (transfer->readStep == LastCartridgeReadStep(g_ControllerCount)) {
        // Repeating the results for one cartridge is there to match: testing
        // only for an uninitialized one here gives 13 words differing, 7 in
        // shape
        if (transfer->openedCartridgeCount == 1) {
            if (transfer->hasUninitializedCartridge == TRUE) {
                return PAK_RESULT_UNINITIALIZED;
            }

            if (transfer->listedCharacterCount == 0) {
                return PAK_RESULT_NO_CHARACTERS;
            }

            return PAK_RESULT_OK;
        }

        if (transfer->openedCartridgeCount > 0) {
            if (transfer->listedCharacterCount == 0) {
                return PAK_RESULT_NO_CHARACTERS;
            }

            return PAK_RESULT_OK;
        }

        if (transfer->hasWrongCartridge == TRUE) {
            return PAK_RESULT_WRONG_CARTRIDGE;
        }

        if (transfer->hasMissingCartridge == TRUE) {
            return PAK_RESULT_NO_CARTRIDGE;
        }

        if (transfer->hasOtherPak == TRUE) {
            return PAK_RESULT_OTHER_PAK;
        }

        if (transfer->hasMissingPak == TRUE) {
            return PAK_RESULT_NO_PAK;
        }

        return PAK_RESULT_CONNECTOR_PROBLEM;
    }

    transfer->readStep++;
    return PAK_RESULT_STILL_READING;
}

// Opens a controller's Game Boy pak, restarting the Game Boy pak manager after
// each failed attempt, and returns the last PAK_RESULT_*
PakResult OpenGameBoyPakWithRetries(s32 port, OSGbpakId *header, NUContPakFile *file) {
    s32 failureCount;
    PakResult result;

    for (failureCount = 0; failureCount != GAME_BOY_PAK_OPEN_ATTEMPTS; failureCount++) {
        result = OpenGameBoyPak(port, header, file);
        if (result == PAK_RESULT_OK) {
            return PAK_RESULT_OK;
        }

        nuContGBPakMgrInit();
    }

    return result;
}

// Opens the Game Boy pak on a port, powers it on if needed and reads the
// cartridge header, then tells whether the cartridge is Mario Golf's.
PakResult OpenGameBoyPak(s32 port, OSGbpakId *header, NUContPakFile *file) {
    Boolean isMarioGolf = TRUE;
    s32 index;
    u8 pakStatus;

    switch (nuContGBPakOpen(file, port)) {
        case PAK_ACCESS_OK:
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
            return PAK_RESULT_UNKNOWN_ERROR;
    }

    switch (nuContGBPakGetStatus(file, &pakStatus)) {
        case PAK_ACCESS_OK:
        case PFS_ERR_NEW_GBCART:
            if (!(pakStatus & OS_GBPAK_POWER)) {
                switch (nuContGBPakPower(file, NU_CONT_GBPAK_POWER_ON)) {
                    case PAK_ACCESS_OK:
                        // "3rd Power on"
                        osSyncPrintf("3rd 電源ＯＮ\n");
                        break;
                    case PFS_ERR_NOPACK:
                        // "3rd Nothing is plugged into the given controller"
                        osSyncPrintf("3rd 指定したコントローラに何も差し込まれていません \n");
                        return PAK_RESULT_NO_PAK;
                    case PFS_ERR_CONTRFAIL:
                        // "3rd Data transfer with the controller failed."
                        osSyncPrintf("3rd コントローラとのデータ転送の失敗です。\n");
                        return PAK_RESULT_CONNECTOR_PROBLEM;
                }
            }

            switch (nuContGBPakReadID(file, header, &pakStatus)) {
                case PFS_ERR_NEW_GBCART:
                    // "4th The GB cartridge was removed and inserted."
                    osSyncPrintf("4th GBカートリッジの挿抜が行われました。\n");
                // Falls through
                case PAK_ACCESS_OK:
                    if (!(pakStatus & OS_GBPAK_GBCART_ON)) {
                        // "5th No cassette is inserted in the GB pak."
                        osSyncPrintf("5th ＧＢパックにカセットが刺さってません。\n");
                        return PAK_RESULT_CARTRIDGE_REMOVED;
                    }

                    if (pakStatus & OS_GBPAK_GBCART_PULL) {
                        // "The cassette was removed and inserted."
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
                    // "4th Nothing is plugged into the given controller"
                    osSyncPrintf("4th 指定したコントローラに何も差し込まれていません\n");
                    return PAK_RESULT_NO_PAK;
                case PFS_ERR_DEVICE:
                    // "4th A device other than the 64GB pak is plugged into the
                    // controller"
                    osSyncPrintf(
                        "4th "
                        "64GBパック以外のデバイスがコントローラに差し込まれています\n"
                    );
                    return PAK_RESULT_OTHER_PAK;
                case PFS_ERR_CONTRFAIL:
                    // "4th Data transfer with the controller failed."
                    osSyncPrintf("4th コントローラとのデータ転送の失敗です。\n");

                    // "The controller is not connected properly, or"
                    osSyncPrintf("コントローラが正しく接続されていないか、\n");

                    // "the 64GB pak or controller connector may be faulty. stat = %xH"
                    osSyncPrintf(
                        "64GBパックかコントローラコネクタに異常がある可\224\134性がありま"
                        "す。stat = %xH\n",
                        pakStatus
                    );
                    return PAK_RESULT_CONNECTOR_PROBLEM;
                case PFS_ERR_NO_GBCART:
                    // "4th No GB cartridge is inserted in the 64GB pak."
                    osSyncPrintf("4th 64GBパックにGBカートリッジが差し込まれていません。\n");
                    return PAK_RESULT_NO_CARTRIDGE;
            }

            break;
        case PFS_ERR_NOPACK:
            // "2nd Nothing is plugged into the given controller"
            osSyncPrintf("2nd 指定したコントローラに何も差し込まれていません\n");
            return PAK_RESULT_NO_PAK;
        case PFS_ERR_DEVICE:
            // "2nd A device other than the 64GB pak is plugged into the controller"
            osSyncPrintf("2nd 64GBパック以外のデバイスがコントローラに差し込まれています\n");
            return PAK_RESULT_OTHER_PAK;
        case PFS_ERR_CONTRFAIL:
            // "2nd Data transfer with the controller failed."
            osSyncPrintf("2nd コントローラとのデータ転送の失敗です。\n");
            return PAK_RESULT_CONNECTOR_PROBLEM;
        case PFS_ERR_NO_GBCART:
            // "2ndt No GB cartridge is inserted in the 64GB pak."
            osSyncPrintf("2ndt 64GBパックにGBカートリッジが差し込まれていません。\n");
            return PAK_RESULT_NO_CARTRIDGE;
    }

    return -1;
}

// Writes a random tag to the cartridge RAM of each Game Boy cartridge and lists
// the controllers whose cartridges took it, so that later accesses can tell
// whether a cartridge was swapped.
PakResult TagCartridges(void) {
    // The transfer pointer is there to match: reading g_GameBoyTransfer
    // directly gives 102 words differing, 22 in shape
    GameBoyTransfer *transfer = &g_GameBoyTransfer;
    OSGbpakId header;
    NUContPakFile file;
    u8 tag[CARTRIDGE_TAG_SIZE];
    u8 pakStatus;
    Boolean isAnyTagged = FALSE;
    Boolean wasNoCartridge = FALSE;
    Boolean wasNoPak = FALSE;
    Boolean wasOtherPak = FALSE;
    Boolean wasWrongCartridge = FALSE;
    Boolean wasCartridgePulled = FALSE;
    s32 controllerIndex;
    s32 index;
    PakResult result;

    transfer->taggedCartridgeCount = 0;
    for (controllerIndex = 0; controllerIndex != MAXCONTROLLERS; controllerIndex++) {
        transfer->taggedControllers[controllerIndex] = 0;
    }

    nuContGBPakMgrInit();
    for (controllerIndex = 0; controllerIndex < g_ControllerCount; controllerIndex++) {
        result =
            OpenGameBoyPakWithRetries(g_PlayerControllerPorts[controllerIndex], &header, &file);
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
                transfer->cartridgeTags[controllerIndex][index] = tag[index];
            }

            if (nuContGBPakFwrite(&file, CARTRIDGE_RAM_ADDRESS, tag, CARTRIDGE_TAG_SIZE) == 0 &&
                nuContGBPakGetStatus(&file, &pakStatus) == 0) {
                if (pakStatus & OS_GBPAK_GBCART_PULL) {
                    wasCartridgePulled = TRUE;
                }
                else {
                    transfer->taggedControllers[transfer->taggedCartridgeCount] = controllerIndex;
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

// Opens the notice cloud with a message, or starts closing it
void SetNoticeState(NoticeState state, MessageId messageId) {
    f_NoticeState = state;
    if (state == NOTICE_OPENING) {
        f_NoticeSize = 0;
        f_NoticeMessageId = messageId;
    }

    if (state == NOTICE_CLOSING) {
        f_NoticeSize = NOTICE_FULL_SIZE;
    }
}

// Returns the NOTICE_* state, so the screens wait for the notice to close
NoticeState GetNoticeState(void) {
    return f_NoticeState;
}

// Steps the notice cloud's size and draws it from its center, with its message
// once it is full size.
void DrawNotice(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 strip;
    s32 halfWidth;
    s32 halfHeight;

    // The copy of halfHeight, and its starting value, are there to match:
    // without the 0, or using halfHeight itself, 34 words differ, none in
    // shape; computing the strip height in place of halfHeight gives 98
    s32 stripHeight = 0;
    s32 bottom;

    if (f_NoticeState == NOTICE_HIDDEN) {
        return;
    }

    if (f_NoticeState == NOTICE_OPENING) {
        if (f_NoticeSize < NOTICE_FULL_SIZE) {
            f_NoticeSize++;
        }
        else {
            f_NoticeState = NOTICE_SHOWN;
        }
    }
    else if (f_NoticeState == NOTICE_CLOSING) {
        if (f_NoticeSize > 0) {
            f_NoticeSize--;
        }
        else {
            f_NoticeState = NOTICE_HIDDEN;
        }
    }

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPPipeSync(gfx++);
    gDPSetAlphaDither(gfx++, G_AD_PATTERN);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
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
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 240);
    if (f_NoticeSize > 0) {
        for (strip = 0; strip != NOTICE_STRIP_COUNT; strip++) {
            halfHeight = f_NoticeSize * NOTICE_HALF_HEIGHT_STEP;
            halfWidth = f_NoticeSize * NOTICE_HALF_WIDTH_STEP;
            LoadNibbleTextureBlock(
                &gfx,
                ImageBody(g_GameBoyTransfer.noticeCloudImage) + (strip * NOTICE_STRIP_SIZE),
                G_IM_FMT_I,
                NOTICE_WIDTH,
                NOTICE_STRIP_HEIGHT,
                0,
                G_TX_CLAMP,
                G_TX_CLAMP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            stripHeight = halfHeight;
            bottom = (strip * stripHeight) + NOTICE_CENTER_Y;
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(NOTICE_CENTER_X - halfWidth),
                QuarterPixels(bottom - stripHeight),
                QuarterPixels(NOTICE_CENTER_X + halfWidth),
                QuarterPixels(bottom),
                G_TX_RENDERTILE,
                0,
                0,
                TexelRunStep(NOTICE_WIDTH / 2, halfWidth),
                TexelRunStep(NOTICE_STRIP_HEIGHT, stripHeight)
            );
        }
    }

    if (f_NoticeState == NOTICE_SHOWN) {
        // The unused array is there to match; without it 44 words differ
        s32 unused[2] UNUSED;

        SetTextSlot(
            NOTICE_CENTER_X,
            NOTICE_TEXT_TOP,
            f_NoticeMessageId,
            TEXT_SLOT_LARGE_FONT,
            NOTICE_TEXT_SLOT,
            TEXT_FLAG_CENTERED | TEXT_FLAG_DRAWN_BY_OWNER,
            255,
            255,
            255,
            OPAQUE_ALPHA
        );
        gDPSetPrimColor(gfx++, 0, 0, 32, 64, 255, OPAQUE_ALPHA);
        DrawLargeTextSlot(&gfx, NOTICE_TEXT_SLOT);
    }

    *displayListHead = gfx;
}

// Draws the Accessing sign, which asks the player not to remove the Game Pak
// while a pak is being accessed
void DrawAccessingSign(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 strip;

    LoadLargePalette(&gfx, ImagePalette(g_GameBoyTransfer.accessingImage));
    for (strip = 0; strip != ACCESSING_STRIP_COUNT; strip++) {
        LoadTextureBlock(
            &gfx,
            LargeImageTexels(g_GameBoyTransfer.accessingImage) + (strip * ACCESSING_STRIP_SIZE),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            ACCESSING_WIDTH,
            ACCESSING_STRIP_HEIGHT,
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
            QuarterPixels(ACCESSING_LEFT),
            QuarterPixels(ACCESSING_TOP + (strip * ACCESSING_STRIP_HEIGHT)),
            QuarterPixels(ACCESSING_LEFT + ACCESSING_WIDTH),
            QuarterPixels(
                ACCESSING_TOP + (strip * ACCESSING_STRIP_HEIGHT) + ACCESSING_STRIP_HEIGHT
            ),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
    }

    *displayListHead = gfx;
}

// Draws the hand that presses A beside a notice or a prompt, at its position
void DrawPressingHand(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;

    f_PressingHandFrame = (f_PressingHandFrame + 1) % PRESSING_HAND_FRAME_COUNT;
    LoadLargePalette(&gfx, ImagePalette(g_PressingHandImage));
    LoadTextureBlock(
        &gfx,
        LargeImageTexels(g_PressingHandImage) +
            (f_PressingHandFrame / PRESSING_HAND_FRAMES_PER_IMAGE * PRESSING_HAND_IMAGE_SIZE),
        G_IM_FMT_CI,
        G_IM_SIZ_8b,
        PRESSING_HAND_SIZE,
        PRESSING_HAND_SIZE,
        0,
        0,
        0,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    if (g_GameBoyTransfer.pressingHandPosition == PRESSING_HAND_LOW) {
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(CORNER_PRESSING_HAND_LEFT),
            QuarterPixels(CORNER_PRESSING_HAND_TOP),
            QuarterPixels(CORNER_PRESSING_HAND_LEFT + PRESSING_HAND_SIZE),
            QuarterPixels(CORNER_PRESSING_HAND_TOP + PRESSING_HAND_SIZE),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
    }
    else {
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(NOTICE_PRESSING_HAND_LEFT),
            QuarterPixels(NOTICE_PRESSING_HAND_TOP),
            QuarterPixels(NOTICE_PRESSING_HAND_LEFT + PRESSING_HAND_SIZE),
            QuarterPixels(NOTICE_PRESSING_HAND_TOP + PRESSING_HAND_SIZE),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
    }

    *displayListHead = gfx;
}

// Returns value rounded to tenths, times ten, up to MAX_STATUS_VALUE and nine
// tenths
static inline s32 PackTenths(f32 value) {
    s32 wholePart = value;
    s32 fraction = (value - wholePart) * HUNDREDTHS_PER_ONE;

    if (fraction % HUNDREDTHS_PER_TENTH >= HALF_TENTH_HUNDREDTHS) {
        fraction += HUNDREDTHS_PER_TENTH;
    }

    fraction /= HUNDREDTHS_PER_TENTH;
    if (fraction >= TENTHS_PER_ONE) {
        wholePart++;
        fraction = 0;
        if (wholePart >= STATUS_VALUE_MAX + 1) {
            wholePart = STATUS_VALUE_MAX;
            fraction = TENTHS_PER_ONE - 1;
        }
    }

    return (wholePart * TENTHS_PER_ONE) + fraction;
}

// Fills status with the save's course records, best times, badges and stars, in
// the layout of the Game Boy cartridge's status data.
void BuildStatusData(StatusData *status) {
    SaveData *save = GetSaveData();
    StatusCourse *course;
    StatusProgress *progress;

    // Reading the records through save gives 211 words differing, 21 in shape
    SaveRecords *records = &save->records;
    CourseStats *courseStats;
    CourseStats *scannedStats;
    f32 reciprocal;
    s32 figure;
    s32 sandSaveAttemptCount;
    s32 roundedDrive;
    CourseIndex courseIndex;
    s32 index;
    s32 matchupBits[2];
    s32 row;
    s32 openCourseCount;

    memset(status, 0, STATUS_CLEARED_SIZE);
    for (courseIndex = 0; courseIndex != COURSE_COUNT; courseIndex++) {
        courseStats = &save->records.courseStats[courseIndex];
        course = &status->courses[courseIndex];

        // Testing for zero first is there to match: without it 2 words differ, 2
        // in shape
        if (courseStats->roundCount != 0) {
            if (courseStats->roundCount > 0) {
                reciprocal = 1.0f / courseStats->roundCount;

                // Copying the count into value is there to match: storing its bytes
                // straight from the entry gives 11 words differing, 7 in shape
                figure = courseStats->roundCount;
                course->roundCount[0] = figure;
                course->roundCount[1] = figure >> BITS_PER_BYTE;
                course->bestScore = courseStats->bestScore + STATUS_SCORE_BIAS;
                course->scoreAverage = RoundedWholePart(courseStats->strokeTotal * reciprocal);
                figure = RoundedWholePart(courseStats->longestDrive * YARDS_PER_UNIT);
                course->longestDrive[0] = figure;
                course->longestDrive[1] = figure >> BITS_PER_BYTE;
                figure = RoundedWholePart(courseStats->driveAverage * YARDS_PER_UNIT);
                course->driveAverage[0] = figure;
                course->driveAverage[1] = figure >> BITS_PER_BYTE;
                course->fairwayPercent = RoundedWholePart(
                    courseStats->measuredDriveTotal * reciprocal *
                    ((f32)PERCENT_PER_WHOLE / HOLES_PER_ROUND)
                );
                course->puttAverage =
                    PackTenths(courseStats->puttTotal * reciprocal * (1.0f / HOLES_PER_ROUND));
                course->parSavePercent = RoundedWholePart(
                    courseStats->parOrBetterCount * reciprocal *
                    ((f32)PERCENT_PER_WHOLE / HOLES_PER_ROUND)
                );
                course->parOnPercent = RoundedWholePart(
                    courseStats->parOnCount * reciprocal *
                    ((f32)PERCENT_PER_WHOLE / HOLES_PER_ROUND)
                );
                sandSaveAttemptCount =
                    courseStats->sandSaveCount + courseStats->missedSandSaveCount;
                if (sandSaveAttemptCount == 0) {
                    course->sandSavePercent = 0;
                }
                else {
                    course->sandSavePercent = RoundedWholePart(
                        ((f32)courseStats->sandSaveCount / (f32)sandSaveAttemptCount) *
                        (f32)PERCENT_PER_WHOLE
                    );
                }

                course->places = 0;

                // Setting the first bit with |= is there to match: setting places to
                // it or to 0 in an if-else gives 22 words differing, 12 in shape
                if (courseStats->hasPlace[THIRD_PLACE_INDEX] > 0) {
                    course->places |= STATUS_THIRD_PLACE;
                }

                if (courseStats->hasPlace[SECOND_PLACE_INDEX] > 0) {
                    course->places |= STATUS_SECOND_PLACE;
                }

                if (courseStats->hasPlace[FIRST_PLACE_INDEX] > 0) {
                    course->places |= STATUS_FIRST_PLACE;
                }

                for (index = 0; index != CHARACTER_COUNT; index++) {
                    if (courseStats->hasCharacterBestScore[index] == TRUE) {
                        course->characterBestScores[index] =
                            courseStats->characterBestScores[index] + STATUS_SCORE_BIAS;
                    }
                    else {
                        course->characterBestScores[index] = 0;
                    }
                }
            }
        }

        for (index = 0; index != BEST_TIME_COUNT; index++) {
            if (records->bestTimes[courseIndex][index] > 0) {
                // Passing the parts through value is there to match: storing them
                // directly gives 20 words differing, none in shape
                figure = records->bestTimes[courseIndex][index] / FRAMES_PER_MINUTE;
                course->bestTimes[index].minutes = figure;
                figure =
                    records->bestTimes[courseIndex][index] / FRAMES_PER_SECOND % SECONDS_PER_MINUTE;
                course->bestTimes[index].seconds = figure;
                figure = (s32)(records->bestTimes[courseIndex][index] *
                             ((f32)HUNDREDTHS_PER_SECOND / FRAMES_PER_SECOND)) %
                    HUNDREDTHS_PER_SECOND;
                course->bestTimes[index].hundredths = figure;
                course->bestTimes[index].characterIndex =
                    records->bestTimeCharacters[courseIndex][index];
                course->bestTimes[index].scoreToPar =
                    records->bestTimeScoresToPar[courseIndex][index] + STATUS_SCORE_BIAS;
            }
            else {
                course->bestTimes[index].minutes = 0;
                course->bestTimes[index].seconds = 0;
                course->bestTimes[index].hundredths = 0;
                course->bestTimes[index].characterIndex = 0;
                course->bestTimes[index].scoreToPar = 0;
            }
        }

        course->birdieBadges[0] = 0;
        course->birdieBadges[1] = 0;
        course->birdieBadges[2] = 0;
        for (index = 0; index != BITS_PER_BYTE; index++) {
            if (records->awards.birdieBadgeCounts[courseIndex][index] > 0) {
                course->birdieBadges[0] |= BirdieBadgeBit(index);
            }
        }

        for (index = 0; index != BITS_PER_BYTE; index++) {
            if (records->awards.birdieBadgeCounts[courseIndex][BITS_PER_BYTE + index] > 0) {
                course->birdieBadges[1] |= BirdieBadgeBit(index);
            }
        }

        for (index = 0; index != COURSE_HOLE_COUNT - (BITS_PER_BYTE * 2); index++) {
            if (records->awards.birdieBadgeCounts[courseIndex][(BITS_PER_BYTE * 2) + index] > 0) {
                course->birdieBadges[2] |= BirdieBadgeBit(index);
            }
        }
    }

    ComputeRoundStatistics();
    course = &status->allCourses;
    figure = g_CourseStatFigures[COURSE_COUNT].roundCount;
    if (figure > 0) {
        course->roundCount[0] = figure;
        course->roundCount[1] = figure >> BITS_PER_BYTE;
        course->bestScore = (s32)g_CourseStatFigures[COURSE_COUNT].bestScore + STATUS_SCORE_BIAS;
        course->scoreAverage = RoundedWholePart(g_CourseStatFigures[COURSE_COUNT].scoreAverage);
        // Holding the drives in sum, and the scores below in scores, is there to
        // match
        roundedDrive = RoundedWholePart(g_CourseStatFigures[COURSE_COUNT].bestDrive);
        course->longestDrive[0] = roundedDrive;
        course->longestDrive[1] = roundedDrive >> BITS_PER_BYTE;
        roundedDrive = RoundedWholePart(g_CourseStatFigures[COURSE_COUNT].driveAverage);
        course->driveAverage[0] = roundedDrive;
        course->driveAverage[1] = roundedDrive >> BITS_PER_BYTE;
        course->fairwayPercent = RoundedWholePart(g_CourseStatFigures[COURSE_COUNT].fairwayPercent);
        course->puttAverage = PackTenths(g_CourseStatFigures[COURSE_COUNT].puttAverage);
        course->parSavePercent = RoundedWholePart(g_CourseStatFigures[COURSE_COUNT].parSavePercent);
        course->parOnPercent = RoundedWholePart(g_CourseStatFigures[COURSE_COUNT].parOnPercent);
        course->sandSavePercent =
            RoundedWholePart(g_CourseStatFigures[COURSE_COUNT].sandSavePercent);
        course->places = 0;
        for (index = 0; index != CHARACTER_COUNT; index++) {
            for (courseIndex = 0; courseIndex != COURSE_COUNT; courseIndex++) {
                scannedStats = &save->records.courseStats[courseIndex];
                if (scannedStats->hasCharacterBestScore[index] == TRUE) {
                    course->characterBestScores[index] =
                        scannedStats->characterBestScores[index] + STATUS_SCORE_BIAS;
                    break;
                }
            }

            if (courseIndex < COURSE_COUNT) {
                for (courseIndex = 0; courseIndex != COURSE_COUNT; courseIndex++) {
                    scannedStats = &save->records.courseStats[courseIndex];
                    if (scannedStats->hasCharacterBestScore[index] == TRUE &&
                        course->characterBestScores[index] >
                            scannedStats->characterBestScores[index] + STATUS_SCORE_BIAS) {
                        course->characterBestScores[index] =
                            scannedStats->characterBestScores[index] + STATUS_SCORE_BIAS;
                    }
                }
            }
            else {
                course->characterBestScores[index] = 0;
            }
        }

        for (index = 0; index != BEST_TIME_COUNT; index++) {
            course->bestTimes[index].minutes = 0;
            course->bestTimes[index].seconds = 0;
            course->bestTimes[index].hundredths = 0;
            course->bestTimes[index].characterIndex = 0;
            course->bestTimes[index].scoreToPar = 0;
        }

        course->birdieBadges[0] = 0;
        course->birdieBadges[1] = 0;
        course->birdieBadges[2] = 0;
    }

    progress = &status->progress;
    openCourseCount = CountOpenCourses(save->coursePoints);
    progress->openCourses = 0;
    for (courseIndex = 0; courseIndex != openCourseCount; courseIndex++) {
        // The original keeps only the last course's bit
        progress->openCourses = 1 << courseIndex;
    }

    for (courseIndex = 0; courseIndex != RING_SHOT_COURSE_COUNT; courseIndex++) {
        progress->ringShotStars[courseIndex] = 0;
        for (index = 0; index != RING_SHOT_HOLE_COUNT; index++) {
            if (records->hasRingShotStar[courseIndex][index]) {
                progress->ringShotStars[courseIndex] |= 1 << index;
            }
        }
    }

    progress->birdieBadgeCount = 0;
    for (courseIndex = 0; courseIndex != COURSE_COUNT; courseIndex++) {
        for (index = 0; index != COURSE_HOLE_COUNT; index++) {
            if (records->awards.birdieBadgeCounts[courseIndex][index] != 0) {
                progress->birdieBadgeCount++;
            }
        }
    }

    for (courseIndex = 0; courseIndex != CHARACTER_COUNT; courseIndex++) {
        // The bits array, zeroed from its end, and courseIndex as row index are
        // there to match: two locals for the bits give 285 words differing (21
        // in shape), zeroing bits[0] first 2 (0), and a character loop variable
        // 16 (0)
        row = f_StatusCharacterOrder[courseIndex];
        matchupBits[1] = 0;
        matchupBits[0] = 0;
        for (index = 0; index != STATUS_MATCHUP_COLUMN_COUNT; index++) {
            if (records->awards.matchups[row][index].winBits != 0) {
                matchupBits[0] |= 1 << index;
            }

            if (records->awards.matchups[row][index].winBits & WON_UNSAVED_FLAG) {
                matchupBits[1] |= 1 << index;
            }
        }

        progress->matchupBits[courseIndex].wonBits[0] = matchupBits[0];
        progress->matchupBits[courseIndex].wonBits[1] = matchupBits[0] >> BITS_PER_BYTE;
        progress->matchupBits[courseIndex].wonUnsavedBits[0] = matchupBits[1];
        progress->matchupBits[courseIndex].wonUnsavedBits[1] = matchupBits[1] >> BITS_PER_BYTE;
    }
}

// The whole part of value once rounded to tenths, up to MAX_STATUS_VALUE
s32 RoundedWholePart(f32 value) {
    s32 wholePart = value;

    // The fraction in hundredths, then in tenths. Reusing one variable is there
    // to match: a tenths local, or testing the quotient, gives 8 words
    // differing, none in shape
    s32 fraction = (value - wholePart) * HUNDREDTHS_PER_ONE;

    if (fraction % HUNDREDTHS_PER_TENTH >= HALF_TENTH_HUNDREDTHS) {
        fraction += HUNDREDTHS_PER_TENTH;
    }

    fraction /= HUNDREDTHS_PER_TENTH;
    if (fraction >= TENTHS_PER_ONE) {
        wholePart++;
        if (wholePart >= STATUS_VALUE_MAX + 1) {
            wholePart = STATUS_VALUE_MAX;
        }
    }

    return wholePart;
}
