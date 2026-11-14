/*
 * The Transfer Pak, which brings characters over from the Game Boy Color
 * version's save, and the Rumble Paks.
 */

#ifndef TRANSFER_PAK_H
#define TRANSFER_PAK_H

#include <nusys.h>
#include <ultra64.h>

#include "math/math_util.h"
#include "system/asset.h"
#include "system/c_language.h"
#define PAK_ACCESS_OK 0

#define GAME_BOY_CHARACTER_COUNT 4
#define GAME_BOY_CHARACTER_NAME_LENGTH 12
// The most experience a Game Boy character can hold
#define GAME_BOY_EXPERIENCE_MAX 9999

// A slot's status when it holds a character
#define GAME_BOY_CHARACTER_STATUS_PRESENT 1
// A cartridge character's status once it has been brought over
#define GAME_BOY_CHARACTER_STATUS_BROUGHT_OVER 2

// The name at the start of a Game Boy save, with its terminator
#define SAVE_NAME_SIZE 11
// The bytes after it, which IsCharacterFromSave also compares
#define SAVE_KEY_SIZE 5

// The golfers of the Game Boy Color version, whose faces the transmit menu
// shows
#define GAME_BOY_GOLFER_COUNT 4

// A Game Boy character has stats for every club but the putter
#define GAME_BOY_CLUB_COUNT 13

// A club's stats, from which GenerateClubSet builds the club
typedef struct GameBoyClubStats {
    u16 distance; // In yards
    BinaryAngle pitch;
    s8 ignoredStat; // Read by nothing; its meaning is unknown
    s8 sideSpin;
} GameBoyClubStats;

// Game Boy balls 1 to 4 copy a character's contact effect, and 5 to 7 have
// their own
enum GameBoyBallType {
    GAME_BOY_BALL_TYPE_PLAIN,
    GAME_BOY_BALL_TYPE_MARIO,
    GAME_BOY_BALL_TYPE_BOWSER,
    GAME_BOY_BALL_TYPE_WARIO,
    GAME_BOY_BALL_TYPE_RAINBOW,
    GAME_BOY_BALL_TYPE_5,
    GAME_BOY_BALL_TYPE_6,
    GAME_BOY_BALL_TYPE_7
};

typedef s8 GameBoyBallType;

// A character from the Game Boy Color version's save, read through the Transfer
// Pak
typedef struct GameBoyCharacter {
    u8 status;
    char name[GAME_BOY_CHARACTER_NAME_LENGTH];
    u8 saveName[SAVE_NAME_SIZE]; // As the Game Boy save spells it
    u8 unusedAfterSaveName;
    u8 golferIndex;
    UBoolean8 isLefty; // Plays left-handed, as the Game Boy save says
    u8 level;
    s8 clubPitch; // In degrees, added to each club's
    s8 shotCurve; // Positive for a draw, negative for a fade, 0 for straight
    s8 lieRatingLevel; // Picks the clubs' lieRatingPercent
    s8 mishitResistance; // Lowers the clubs' mishitScale
    GameBoyBallType ballType; // A GameBoyBallType
    // Three bytes of the Game Boy save that nothing reads; their meaning is
    // unknown
    s32 ignoredValue;
    s32 experience;
    s16 driveDistance; // Shown beside "Drive:"
    // The save's bytes from 0xB, which with saveName tell whether a save's
    // character was brought over
    u8 saveKey[SAVE_KEY_SIZE];
    s32 companyCode; // Its cartridge header's, or -1
    s32 saveIndex; // Its save on the cartridge, or -1
    GameBoyClubStats clubs[GAME_BOY_CLUB_COUNT];
} GameBoyCharacter;

extern GameBoyCharacter g_GameBoyCharacters[GAME_BOY_CHARACTER_COUNT];
// Set once a Game Boy character has been brought over
extern Boolean g_HasBroughtOverCharacter;

// Where the Game Boy transfer was opened from
enum TransferMode {
    TRANSFER_MODE_AT_BOOT,
    TRANSFER_MODE_FROM_MENU
};

typedef s32 TransferMode;

// What the transmit menu shows
enum TransmitScreen {
    TRANSMIT_SCREEN_OPTIONS,
    TRANSMIT_SCREEN_UNUSED,
    TRANSMIT_SCREEN_BRING_CHARACTERS,
    TRANSMIT_SCREEN_SEND_STATUS,
    TRANSMIT_SCREEN_SEND_EXPERIENCE,
    // Leaving closes a wipe, then swaps in the title screen, or the main menu
    // at its title when B left the options with a Rumble Pak missing; B with
    // every Rumble Pak in, like the options' main menu row, leaves for the title
    // screen.
    TRANSMIT_SCREEN_LEAVING_FOR_TITLE,
    TRANSMIT_SCREEN_LEAVING_FOR_MENU,
    TRANSMIT_SCREEN_RUMBLE_PAK_PROMPT
};

typedef s32 TransmitScreen;

// Each controller's Game Boy cartridge holds up to three saved games
#define CARTRIDGE_SAVE_COUNT 3
#define CARTRIDGE_CHARACTER_COUNT (MAXCONTROLLERS * CARTRIDGE_SAVE_COUNT)

// Random bytes written to each cartridge's RAM to tell the cartridges apart
#define CARTRIDGE_TAG_SIZE 16

// Where a cartridge character is on the Bring Characters list
enum ListingState {
    LISTING_NONE,
    LISTING_SHOWN,
    LISTING_BRINGING // Moving to its slot
};

typedef s32 ListingState;

// GameBoyTransfer.pressingHandPosition values: the hand pressing the A button,
// while the menu waits for it
enum PressingHand {
    PRESSING_HAND_HIDDEN,
    PRESSING_HAND_LOW,
    PRESSING_HAND_FOR_NOTICE
};

typedef s32 PressingHand;

// What the Game Boy transfer keeps while its overlay is loaded and between
// visits
typedef struct GameBoyTransfer {
    ImageAsset *courseSceneImage;
    ImageAsset *pointerImage;
    ImageAsset *arrowsImage;
    PaletteColor *facePalettes[GAME_BOY_GOLFER_COUNT];
    ImageAsset *faceImages[GAME_BOY_GOLFER_COUNT];
    ImageAsset *titleImage;
    ImageAsset *woodPanelImage;
    ImageAsset *transferPakIconsImage;
    ImageAsset *cursorBoxImage;
    ImageAsset *curveSegmentsImage;
    ImageAsset *pressingHandImage;
    ImageAsset *statBarsImage;
    ImageAsset *statLabelsImage;
    ImageAsset *noticeCloudImage;
    ImageAsset *accessingImage;
    ImageAsset *shotTypesImage;
    Boolean hasGameBoyPak;
    TransmitScreen screen; // TRANSMIT_SCREEN_*
    TransferMode transferMode;
    s32 optionIndex;
    s32 characterCount; // Of g_GameBoyCharacters
    NUContPakFile pakFile;
    OSGbpakId cartridgeHeader;
    GameBoyCharacter cartridgeCharacters[CARTRIDGE_CHARACTER_COUNT];
    s32 sourceIndices[GAME_BOY_CHARACTER_COUNT];
    ListingState listingStates[CARTRIDGE_CHARACTER_COUNT]; // LISTING_*
    s32 listedCharacterCount;
    // Each controller's cartridge is opened, then its saves read, a step each
    s32 readStep;
    s32 transferResult;
    s32 screenStep;
    s32 bringingFrame;
    s32 listedCharacterIndex;
    s32 gameBoyCharacterIndex; // Into g_GameBoyCharacters
    s32 firstShownListedIndex; // The list scrolls to show it first
    // The Mario Golf cartridges opened, and which other results some pak gave
    s32 openedCartridgeCount;
    Boolean hasMissingCartridge;
    Boolean hasOtherPak;
    Boolean hasMissingPak;
    Boolean hasWrongCartridge;
    Boolean hasUninitializedCartridge;
    // Set when a cartridge read fails with PFS_ERR_CONTRFAIL; nothing reads
    // or clears it
    Boolean hasIgnoredControllerFailure;
    s8 listedCharacters[CARTRIDGE_CHARACTER_COUNT]; // Present ones, then -1
    s32 chosenCartridgeIndex; // Into taggedControllers
    s32 taggedCartridgeCount;
    s32 taggedControllers[MAXCONTROLLERS];
    u8 cartridgeTags[MAXCONTROLLERS][CARTRIDGE_TAG_SIZE];
    Boolean isCanceled;
    PressingHand pressingHandPosition;
    Boolean isAccessing;
} GameBoyTransfer;

extern GameBoyTransfer g_GameBoyTransfer;

void AddGameBoyExperience(s32 gameBoyCharacterIndex, s32 experience);
Boolean HasGameBoyPak(void);
Boolean IsGameBoyCharacterPresent(s32 gameBoyCharacterIndex);
char *GetGameBoyCharacterName(s32 gameBoyCharacterIndex);
void ClearBootTransmitPending(void);
Boolean IsBootTransmitPending(void);
void ResetGameBoyTransfer(TransferMode transferMode);
void ForceStopRumble(void);
void InitRumblePaks(void);
void StopAllRumble(void);

#endif
