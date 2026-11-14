#ifndef CHARACTER_H
#define CHARACTER_H

#include <ultra64.h>

#include "asset.h"
#include "compiler.h"
#include "language.h"

enum CharacterIndex {
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

typedef s32 CharacterIndex;
// A CharacterIndex kept in a byte or halfword, as the original stores it in
// player, menu, saved, imported and Game Boy records and some menu locals.
typedef s8 CharacterIndex8;
typedef u8 UCharacterIndex8;
typedef s16 CharacterIndex16;

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

extern AssetId g_CharacterFirstFaceAssets[CHARACTER_COUNT];
#define CHARACTER_FACE_IMAGE_OFFSET 9
// The still face at CHARACTER_FACE_IMAGE_OFFSET is FACE_WIDTH by
// FACE_HEIGHT; the face animation frames before it are FACE_FRAME_SIZE
// square.
#define FACE_WIDTH 32
#define FACE_HEIGHT 24
#define FACE_FRAME_SIZE 32
extern SignedAssetId g_CharacterFirstPaletteFiles[CHARACTER_COUNT];
#define CHARACTER_PALETTE_COUNT 4
extern AssetId g_CharacterFaceAssets[CHARACTER_COUNT];
extern SignedAssetId g_CharacterPaletteFiles[CHARACTER_COUNT][CHARACTER_PALETTE_COUNT];

#define IMPORTED_CHARACTER_COUNT 4
#define IMPORTED_CHARACTER_NAME_LENGTH 12
#define FIRST_IMPORTED_CHARACTER 14

typedef struct ImportedCharacter {
    CharacterIndex16 characterIndex;
    s16 clubDistance;
    s16 clubSpin;
    s16 clubSideSpin;
    s8 name[IMPORTED_CHARACTER_NAME_LENGTH];
    UBoolean8 isChosen;
} ImportedCharacter;

extern ImportedCharacter g_ImportedCharacters[IMPORTED_CHARACTER_COUNT];

extern char g_CharacterNames[][IMPORTED_CHARACTER_NAME_LENGTH];

extern SignedAssetId g_CharacterFacePaletteFiles[][CHARACTER_PALETTE_COUNT];

#define FACE_ANIMATION_FRAME_COUNT 8
extern AssetId g_FaceAnimationAssets[][FACE_ANIMATION_FRAME_COUNT];
extern AssetId g_AlternateFaceAnimationAssets[][FACE_ANIMATION_FRAME_COUNT];
extern u8 *g_ShadowTexture;

void PlaceCharacterForShot(u32 slotIndex);
NO_RESULT ReuseLastCharacterMatrices(u32 slotIndex);
void SetCharacterSlotAlpha(u32 slotIndex, s32 alpha);
void SetCharacterFrontCulling(u32 slotIndex, Boolean usesFrontCulling);
void SetPlaysRandomTextureAnimation(u32 slotIndex, Boolean playsRandomTextureAnimation);
NO_RESULT SetTextureAnimationFrame(u32 slotIndex, s32 index, s32 frame);
void ResetCharacterSlots(void);
s32 GetCharacterBufferSize(void);
void SetCharacterBuffer(s32 slotIndex, u8 *buffer);

s32 GetAnimationFrameCount(u32 slotIndex, s32 animationId);
void SetCharacterTranslucent(u32 slotIndex, Boolean isTranslucent);
void WriteBallDistancesIfEnabled(void);

#endif
