#ifndef TRANSFER_PAK_H
#define TRANSFER_PAK_H

#include <lib/libnusys/nusys.h>
#include <ultra64.h>

#define GAME_BOY_CHARACTER_COUNT 4
#define GAME_BOY_CHARACTER_NAME_LENGTH 12
/* The most experience a Game Boy character can hold */
#define MAX_GAME_BOY_EXPERIENCE 9999

/* A slot's status when it holds a character */
#define GAME_BOY_CHARACTER_STATUS_PRESENT 1
/* A cartridge character's status once it has been brought over */
#define GAME_BOY_CHARACTER_STATUS_BROUGHT_OVER 2

/* The name at the start of a Game Boy save, with its terminator */
#define SAVE_NAME_SIZE 11
#define SAVE_UNKB_SIZE 5

/* The golfers of the Game Boy Color version, whose faces the transmit menu
 * shows */
#define GAME_BOY_GOLFER_COUNT 4

#define GAME_BOY_CHARACTER_UNK3C_COUNT 13

typedef struct GameBoyCharacterUnk3C {
  /* 0x0 */ u16 unk0;
  /* 0x2 */ u16 unk2;
  /* 0x4 */ u8 unk4;
  /* 0x5 */ u8 unk5;
} GameBoyCharacterUnk3C; /* size = 0x6 */

/* Game Boy balls 1 to 4 copy a character's contact effect, and 5 to 7 have
 * their own */
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

/* A character from the Game Boy Color version's save, read through the Transfer
 * Pak */
typedef struct GameBoyCharacter {
  /* 0x00 */ u8 status;
  /* 0x01 */ u8 name[GAME_BOY_CHARACTER_NAME_LENGTH];
  /* 0x0D */ u8 saveName[SAVE_NAME_SIZE]; /* As the Game Boy save spells it */
  /* 0x18 */ u8 unk18;
  /* 0x19 */ u8 character;
  /* 0x1A */ u8 unk1A;
  /* 0x1B */ u8 level;
  /* 0x1C */ s8 unk1C;
  /* 0x1D */ s8
      shotCurve; /* Positive for a draw, negative for a fade, 0 for straight */
  /* 0x1E */ u8 unk1E;
  /* 0x1F */ u8 mishitResistance; /* As ClubSetStats' */
  /* 0x20 */ s8 ballType; /* A GameBoyBallType */
  /* 0x24 */ s32 unk24;
  /* 0x28 */ s32 experience;
  /* 0x2C */ s16 driveDistance; /* Shown beside "Drive:" */
  /* 0x2E */ u8
      saveUnkB[SAVE_UNKB_SIZE]; /* Copied from the save's bytes at 0xB */
  /* 0x33 */ u8 unk33;
  /* 0x34 */ s32 unk34;
  /* 0x38 */ s32 unk38;
  /* 0x3C */ GameBoyCharacterUnk3C unk3C[GAME_BOY_CHARACTER_UNK3C_COUNT];
} GameBoyCharacter; /* size = 0x8C */

extern GameBoyCharacter g_GameBoyCharacters[GAME_BOY_CHARACTER_COUNT];
/* Set once a Game Boy character has been brought over */
extern Boolean g_HasBroughtOverCharacter;

/* Where the Game Boy transfer was opened from */
enum TransferMode {
  TRANSFER_MODE_AT_BOOT,
  TRANSFER_MODE_FROM_MENU
};

/* What the transmit menu shows */
enum TransmitScreen {
  TRANSMIT_SCREEN_OPTIONS,
  TRANSMIT_SCREEN_UNK1,
  TRANSMIT_SCREEN_BRING_CHARACTERS,
  TRANSMIT_SCREEN_SEND_STATUS,
  TRANSMIT_SCREEN_SEND_EXPERIENCE,
  TRANSMIT_SCREEN_LEAVING,
  TRANSMIT_SCREEN_UNK6,
  TRANSMIT_SCREEN_RUMBLE_PAK_PROMPT
};

/* Each controller's Game Boy cartridge holds up to three saved games */
#define CARTRIDGE_SAVE_COUNT 3
#define CARTRIDGE_CHARACTER_COUNT (MAXCONTROLLERS * CARTRIDGE_SAVE_COUNT)

/* The Game Boy cartridge header, from cartridge address 0x100 */

/* Random bytes written to each cartridge's RAM to tell the cartridges apart */
#define CARTRIDGE_TAG_SIZE 16

/* What the Game Boy transfer keeps while its overlay is loaded and between
 * visits */
typedef struct GameBoyTransfer {
  /* 0x000 */ void* courseSceneImage;
  /* 0x004 */ void* pointerImage;
  /* 0x008 */ void* arrowsImage;
  /* 0x00C */ void* facePalettes[GAME_BOY_CHARACTER_COUNT];
  /* 0x01C */ void* faceImages[GAME_BOY_CHARACTER_COUNT];
  /* 0x02C */ void* titleImage;
  /* 0x030 */ void* woodPanelImage;
  /* 0x034 */ void* controllerIconsImage;
  /* 0x038 */ void* cursorBoxImage;
  /* 0x03C */ void* curveSegmentsImage;
  /* 0x040 */ void* pressingHandImage;
  /* 0x044 */ void* statBarsImage;
  /* 0x048 */ void* statLabelsImage;
  /* 0x04C */ void* noticeCloudImage;
  /* 0x050 */ void* accessingImage;
  /* 0x054 */ void* shotTypesImage;
  /* 0x058 */ Boolean hasGameBoyPak;
  /* 0x05C */ s32 screen; /* TRANSMIT_SCREEN_* */
  /* 0x060 */ s32 transferMode;
  /* 0x064 */ s32 optionIndex;
  /* 0x068 */ s32 characterCount; /* Of g_GameBoyCharacters */
  /* 0x06C */ NUContPakFile pakFile;
  /* 0x07C */ OSGbpakId cartridgeHeader;
  /* 0x0CC */ GameBoyCharacter cartridgeCharacters[CARTRIDGE_CHARACTER_COUNT];
  /* 0x75C */ s32 sourceIndices[GAME_BOY_CHARACTER_COUNT];
  /* 0x76C */ s32 listingStates[CARTRIDGE_CHARACTER_COUNT]; /* LISTING_* */
  /* 0x79C */ s32 listedCharacterCount;
  /* 0x7A0 */ s32 unk7A0;
  /* 0x7A4 */ s32 transferResult;
  /* 0x7A8 */ s32 screenStep;
  /* 0x7AC */ s32 bringingFrame;
  /* 0x7B0 */ s32 listedCharacterIndex;
  /* 0x7B4 */ s32 characterIndex; /* Into g_GameBoyCharacters */
  /* 0x7B8 */ s32 unk7B8;
  /* 0x7BC */ s32 unk7BC;
  /* 0x7C0 */ s32 unk7C0;
  /* 0x7C4 */ s32 unk7C4;
  /* 0x7C8 */ s32 unk7C8;
  /* 0x7CC */ s32 unk7CC;
  /* 0x7D0 */ s32 unk7D0;
  /* 0x7D4 */ s32 unk7D4;
  /* 0x7D8 */ s8
      listedCharacters[CARTRIDGE_CHARACTER_COUNT]; /* Present ones, then -1 */
  /* 0x7E4 */ s32 chosenCartridgeIndex; /* Into taggedControllers */
  /* 0x7E8 */ s32 taggedCartridgeCount;
  /* 0x7EC */ s32 taggedControllers[MAXCONTROLLERS];
  /* 0x7FC */ u8 cartridgeTags[MAXCONTROLLERS][CARTRIDGE_TAG_SIZE];
  /* 0x83C */ Boolean isCanceled;
  /* 0x840 */ s32 pressingHandPosition; /* 0 hides the hand */
  /* 0x844 */ Boolean isAccessing;
} GameBoyTransfer; /* size = 0x848 */

extern GameBoyTransfer g_GameBoyTransfer;

void AddGameBoyExperience(s32 characterIndex, s32 experience);
Boolean HasGameBoyPak(void);
Boolean IsGameBoyCharacterPresent(s32 characterIndex);
u8* GetGameBoyCharacterName(s32 characterIndex);
void ClearBootTransmitPending(void);
Boolean IsBootTransmitPending(void);
void ResetGameBoyTransfer(s32 transferMode);
void ForceStopRumble(void);

#endif
