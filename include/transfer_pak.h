#ifndef TRANSFER_PAK_H
#define TRANSFER_PAK_H

#include <ultra64.h>

#define GAME_BOY_CHARACTER_COUNT 4
#define GAME_BOY_CHARACTER_NAME_LENGTH 12
/* The most experience a Game Boy character can hold */
#define MAX_GAME_BOY_EXPERIENCE 9999

/* A slot's status when it holds a character */
#define GAME_BOY_CHARACTER_STATUS_PRESENT 1

/* A character from the Game Boy Color version's save, read through the Transfer Pak */
typedef struct GameBoyCharacter {
    /* 0x00 */ u8 status;
    /* 0x01 */ u8 name[GAME_BOY_CHARACTER_NAME_LENGTH];
    /* 0x0D */ u8 unkD[0x19 - 0xD];
    /* 0x19 */ u8 character;
    /* 0x1A */ u8 unk1A[0x28 - 0x1A];
    /* 0x28 */ s32 experience;
    /* 0x2C */ u8 unk2C[0x34 - 0x2C];
    /* 0x34 */ s32 unk34;
    /* 0x38 */ s32 unk38;
    /* 0x3C */ u8 unk3C[0x8C - 0x3C];
} GameBoyCharacter; /* size = 0x8C */

extern GameBoyCharacter g_GameBoyCharacters[GAME_BOY_CHARACTER_COUNT];

void AddGameBoyExperience(s32 characterIndex, s32 experience);
s32 HasGameBoyPak(void);
s32 IsGameBoyCharacterPresent(s32 characterIndex);
u8 *GetGameBoyCharacterName(s32 characterIndex);
void ClearBootTransmitPending(void);
s32 IsBootTransmitPending(void);
void ResetGameBoyTransfer(s32 transferMode);
void ForceStopRumble(void);

#endif
