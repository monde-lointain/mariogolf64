#ifndef TRANSFER_PAK_H
#define TRANSFER_PAK_H

#include <nusys.h>
#include <ultra64.h>

#include "asset.h"
#include "language.h"
#include "math_util.h"

// The pak routines return this on success and a PFS_ERR_ code otherwise.
#define PAK_ACCESS_OK 0

#define GAME_BOY_CHARACTER_COUNT 4
#define GAME_BOY_CHARACTER_NAME_LENGTH 12
#define GAME_BOY_EXPERIENCE_MAX 9999

#define GAME_BOY_CHARACTER_STATUS_PRESENT 1
#define GAME_BOY_CHARACTER_STATUS_BROUGHT_OVER 2

#define SAVE_NAME_SIZE 11
#define SAVE_KEY_SIZE 5

#define GAME_BOY_GOLFER_COUNT 4

#define GAME_BOY_CLUB_COUNT 13

typedef struct GameBoyClubStats {
    u16 distance;
    BinaryAngle pitch;
    s8 ignoredStat;
    s8 sideSpin;
} GameBoyClubStats;

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

typedef struct GameBoyCharacter {
    u8 status;
    char name[GAME_BOY_CHARACTER_NAME_LENGTH];
    u8 saveName[SAVE_NAME_SIZE];
    u8 unusedAfterSaveName;
    u8 golferIndex;
    UBoolean8 isLefty;
    u8 level;
    s8 clubPitch;
    s8 shotCurve;
    s8 lieRatingLevel;
    s8 mishitResistance;
    GameBoyBallType ballType;
    s32 ignoredValue;
    s32 experience;
    s16 driveDistance;
    u8 saveKey[SAVE_KEY_SIZE];
    s32 companyCode;
    s32 saveIndex;
    GameBoyClubStats clubs[GAME_BOY_CLUB_COUNT];
} GameBoyCharacter;

extern GameBoyCharacter g_GameBoyCharacters[GAME_BOY_CHARACTER_COUNT];
extern Boolean g_HasBroughtOverCharacter;

enum TransferMode {
    TRANSFER_MODE_AT_BOOT,
    TRANSFER_MODE_FROM_MENU
};

typedef s32 TransferMode;

enum TransmitScreen {
    TRANSMIT_SCREEN_OPTIONS,
    TRANSMIT_SCREEN_UNUSED,
    TRANSMIT_SCREEN_BRING_CHARACTERS,
    TRANSMIT_SCREEN_SEND_STATUS,
    TRANSMIT_SCREEN_SEND_EXPERIENCE,
    TRANSMIT_SCREEN_LEAVING_FOR_TITLE,
    TRANSMIT_SCREEN_LEAVING_FOR_MENU,
    TRANSMIT_SCREEN_RUMBLE_PAK_PROMPT
};

typedef s32 TransmitScreen;

#define CARTRIDGE_SAVE_COUNT 3
#define CARTRIDGE_CHARACTER_COUNT (MAXCONTROLLERS * CARTRIDGE_SAVE_COUNT)

#define CARTRIDGE_TAG_SIZE 16

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
    TransmitScreen screen;
    TransferMode transferMode;
    s32 optionIndex;
    s32 characterCount;
    NUContPakFile pakFile;
    OSGbpakId cartridgeHeader;
    GameBoyCharacter cartridgeCharacters[CARTRIDGE_CHARACTER_COUNT];
    s32 sourceIndices[GAME_BOY_CHARACTER_COUNT];
    s32 listingStates[CARTRIDGE_CHARACTER_COUNT];
    s32 listedCharacterCount;
    s32 readStep;
    s32 transferResult;
    s32 screenStep;
    s32 bringingFrame;
    s32 listedCharacterIndex;
    s32 gameBoyCharacterIndex;
    s32 firstShownListedIndex;
    s32 openedCartridgeCount;
    Boolean hasMissingCartridge;
    Boolean hasOtherPak;
    Boolean hasMissingPak;
    Boolean hasWrongCartridge;
    Boolean hasUninitializedCartridge;
    Boolean hasIgnoredControllerFailure;
    s8 listedCharacters[CARTRIDGE_CHARACTER_COUNT];
    s32 chosenCartridgeIndex;
    s32 taggedCartridgeCount;
    s32 taggedControllers[MAXCONTROLLERS];
    u8 cartridgeTags[MAXCONTROLLERS][CARTRIDGE_TAG_SIZE];
    Boolean isCanceled;
    s32 pressingHandPosition;
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
