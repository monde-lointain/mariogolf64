/*
 * The characters' models: loading, animating, posing, lighting and drawing
 * them.
 */

#ifndef CHARACTER_H
#define CHARACTER_H

#include <ultra64.h>

#include "system/asset.h"
#include "system/c_language.h"
#include "system/compiler.h"
typedef s32 AnimationId;

// Indices of the playable characters, in the order of g_Players'
// characterIndex and of the tables that it indexes. A character's models
// are this index plus a multiple of CHARACTER_COUNT.
typedef s8 AnimationId8;
typedef u8 UAnimationId8;

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

// An enum CharacterId value; fields kept narrower say so in comments
typedef s32 CharacterIndex;
typedef s8 CharacterIndex8;
typedef u8 UCharacterIndex8;
typedef s16 CharacterIndex16;

// The playable characters in the order that the menus' character grids,
// g_CharacterNames and the character name messages use, which swaps three
// of them around from CharacterId's order
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

// The first of each character's image assets, of which its face is first
extern AssetId g_CharacterFirstFaceAssets[CHARACTER_COUNT];
// Added to a character's g_CharacterImageAssets entry for the face that menus
// show
#define CHARACTER_FACE_IMAGE_OFFSET 9
// The menus draw characters' faces, darkened or with the name beside them
#define FACE_WIDTH 32
#define FACE_HEIGHT 24
#define FACE_FRAME_SIZE 32

// The asset file of each character's first palette, followed by those of its
// other colors
extern SignedAssetId g_CharacterFirstPaletteFiles[CHARACTER_COUNT];
#define CHARACTER_PALETTE_COUNT 4
// What the menus read by character id: each face, its g_CharacterImageAssets
// entry plus CHARACTER_FACE_IMAGE_OFFSET, and its g_CharacterPaletteAssets
// entry with the palette files after it
extern AssetId g_CharacterFaceAssets[CHARACTER_COUNT];
extern SignedAssetId g_CharacterPaletteFiles[CHARACTER_COUNT][CHARACTER_PALETTE_COUNT];

#define IMPORTED_CHARACTER_COUNT 4
#define IMPORTED_CHARACTER_NAME_LENGTH 12
// The character ids that stand for the imported characters in the grid
#define FIRST_IMPORTED_CHARACTER 14

// A character imported from the Game Boy Color version
typedef struct ImportedCharacter {
    CharacterIndex16 characterIndex;
    // The distance, spin and side spin of the character's first club
    s16 clubDistance;
    s16 clubSpin;
    s16 clubSideSpin;
    s8 name[IMPORTED_CHARACTER_NAME_LENGTH];
    UBoolean8 isChosen; // Set while a player has chosen the character
} ImportedCharacter;

extern ImportedCharacter g_ImportedCharacters[IMPORTED_CHARACTER_COUNT];

// The names of the characters before the imported ones
extern char g_CharacterNames[][IMPORTED_CHARACTER_NAME_LENGTH];

// Each character's face palette file in each of the character's color schemes
extern SignedAssetId g_CharacterFacePaletteFiles[][CHARACTER_PALETTE_COUNT];

// Each character's face, as the asset IDs of its animation's frames, in two
// sets: Club Slots shows the first after three stars and the second after
// fewer, and the character select shows the second while
// GAME_FLAG_ALTERNATE_SELECT_FACES is set
#define FACE_ANIMATION_FRAME_COUNT 8
extern AssetId g_FaceAnimationAssets[][FACE_ANIMATION_FRAME_COUNT];
extern AssetId g_AlternateFaceAnimationAssets[][FACE_ANIMATION_FRAME_COUNT];
// A header, then the 32 by 32 intensity texture of shadows
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

s32 GetAnimationFrameCount(u32 slotIndex, AnimationId animationId);
void SetCharacterTranslucent(u32 slotIndex, Boolean isTranslucent);
void WriteBallDistancesIfEnabled(void);

#endif
