#ifndef CHARACTER_H
#define CHARACTER_H

#include <ultra64.h>

#include "asset.h"
#include "compiler.h"

enum CharacterId {
    CHARACTER_PLUM,
    CHARACTER_CHARLIE,
    CHARACTER_MAPLE,
    CHARACTER_BABY_MARIO,
    CHARACTER_LUIGI,
    CHARACTER_PEACH,
    CHARACTER_SONNY,
    CHARACTER_WARIO,
    CHARACTER_HARRY,
    CHARACTER_MARIO,
    CHARACTER_YOSHI,
    CHARACTER_DONKEY_KONG,
    CHARACTER_BOWSER,
    CHARACTER_METAL_MARIO,
    CHARACTER_KID,
    CHARACTER_SHERRY,
    CHARACTER_AZALEA,
    CHARACTER_JOE
};

typedef s32 CharacterId;

enum CharacterChoice {
    CHARACTER_CHOICE_PLUM,
    CHARACTER_CHOICE_CHARLIE,
    CHARACTER_CHOICE_PEACH,
    CHARACTER_CHOICE_BABY_MARIO,
    CHARACTER_CHOICE_LUIGI,
    CHARACTER_CHOICE_YOSHI,
    CHARACTER_CHOICE_SONNY,
    CHARACTER_CHOICE_WARIO,
    CHARACTER_CHOICE_HARRY,
    CHARACTER_CHOICE_MARIO,
    CHARACTER_CHOICE_MAPLE,
    CHARACTER_CHOICE_DONKEY_KONG,
    CHARACTER_CHOICE_BOWSER,
    CHARACTER_CHOICE_METAL_MARIO,
    CHARACTER_CHOICE_KID,
    CHARACTER_CHOICE_SHERRY,
    CHARACTER_CHOICE_AZALEA,
    CHARACTER_CHOICE_JOE
};

#define CHARACTER_COUNT 18

extern AssetId g_CharacterImageAssets[CHARACTER_COUNT];
#define CHARACTER_FACE_IMAGE_OFFSET 9
extern s16 g_CharacterPaletteAssets[CHARACTER_COUNT];
#define CHARACTER_PALETTE_COUNT 4
extern AssetId g_CharacterFaceAssets[CHARACTER_COUNT];
extern s16 g_CharacterPaletteFiles[CHARACTER_COUNT][CHARACTER_PALETTE_COUNT];

#define IMPORTED_CHARACTER_COUNT 4
#define IMPORTED_CHARACTER_NAME_LENGTH 12
#define FIRST_IMPORTED_CHARACTER 14

typedef struct ImportedCharacter {
    s16 character;
    s16 clubDistance;
    s16 clubSpin;
    s16 clubSideSpin;
    s8 name[IMPORTED_CHARACTER_NAME_LENGTH];
    u8 isChosen;
} ImportedCharacter;

extern ImportedCharacter g_ImportedCharacters[IMPORTED_CHARACTER_COUNT];

extern char g_CharacterNames[][IMPORTED_CHARACTER_NAME_LENGTH];

extern s16 g_CharacterFacePaletteFiles[][CHARACTER_PALETTE_COUNT];

#define FACE_ANIMATION_FRAME_COUNT 8
extern AssetId g_FaceAnimationAssets[][FACE_ANIMATION_FRAME_COUNT];
extern AssetId g_AlternateFaceAnimationAssets[][FACE_ANIMATION_FRAME_COUNT];
extern u8 *g_ShadowTexture;

void PlaceCharacterForShot(u32 slotIndex);
NO_RESULT ReuseLastCharacterMatrices(u32 slotIndex);
void SetCharacterSlotAlpha(u32 slotIndex, s32 value);
void SetCharacterFrontCulling(u32 slotIndex, s32 value);
void SetPlaysRandomTextureAnimation(u32 slotIndex, s32 value);
NO_RESULT SetTextureAnimationFrame(u32 slotIndex, s32 index, s32 frame);
void ResetCharacterSlots(void);
s32 GetCharacterBufferSize(void);
void SetCharacterBuffer(s32 slotIndex, u8 *buffer);

s32 GetAnimationFrameCount(u32 slotIndex, s32 id);
void SetCharacterTranslucent(u32 slotIndex, s32 value);
void WriteBallDistancesIfEnabled(void);

#endif
