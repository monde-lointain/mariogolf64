#ifndef CHARACTER_H
#define CHARACTER_H

#include <ultra64.h>

#include "asset.h"

/*
 * Indices of the playable characters, in the order of the name table. A
 * character's models are this index plus a multiple of CHARACTER_COUNT.
 */
enum CharacterId {
  CHARACTER_PLUM,
  CHARACTER_CHARLIE,
  CHARACTER_PEACH,
  CHARACTER_BABY_MARIO,
  CHARACTER_LUIGI,
  CHARACTER_YOSHI,
  CHARACTER_SONNY,
  CHARACTER_WARIO,
  CHARACTER_HARRY,
  CHARACTER_MARIO,
  CHARACTER_MAPLE,
  CHARACTER_DONKEY_KONG,
  CHARACTER_BOWSER,
  CHARACTER_METAL_MARIO,
  CHARACTER_KID,
  CHARACTER_SHERRY,
  CHARACTER_AZALEA,
  CHARACTER_JOE
};

#define CHARACTER_COUNT 18

/* The first of each character's image assets, of which its face is first */
extern AssetId g_CharacterImageAssets[CHARACTER_COUNT];
/* Added to a character's g_CharacterImageAssets entry for the face that menus
 * show */
#define CHARACTER_FACE_IMAGE_OFFSET 9
/* The asset file of each character's first palette, followed by those of its
 * other colours */
extern s16 g_CharacterPaletteAssets[CHARACTER_COUNT];
#define CHARACTER_PALETTE_COUNT 4
/* What the menus read by character id: each face, its g_CharacterImageAssets
 * entry plus CHARACTER_FACE_IMAGE_OFFSET, and its g_CharacterPaletteAssets
 * entry with the palette files after it */
extern AssetId g_CharacterFaceAssets[CHARACTER_COUNT];
extern s16 g_CharacterPaletteFiles[CHARACTER_COUNT][CHARACTER_PALETTE_COUNT];

#define IMPORTED_CHARACTER_COUNT 4
#define IMPORTED_CHARACTER_NAME_LENGTH 12
/* The character ids that stand for the imported characters in the grid */
#define FIRST_IMPORTED_CHARACTER 14

/* A character imported from the Game Boy Color version */
typedef struct ImportedCharacter {
  /* 0x00 */ s16 character;
  /* 0x02 */ s16 unk2;
  /* 0x04 */ s16 unk4;
  /* 0x06 */ s16 unk6;
  /* 0x08 */ s8 name[IMPORTED_CHARACTER_NAME_LENGTH];
  /* 0x14 */ u8 isChosen; /* Set while a player has chosen the character */
} ImportedCharacter; /* size = 0x16 */

extern ImportedCharacter g_ImportedCharacters[IMPORTED_CHARACTER_COUNT];

/* The names of the characters before the imported ones */
extern char g_CharacterNames[][IMPORTED_CHARACTER_NAME_LENGTH];

/* Each character's face palette file in each of the character's colour schemes
 */
extern s16 g_CharacterFacePaletteFiles[][CHARACTER_PALETTE_COUNT];

/*
 * Each character's face, as the asset IDs of its animation's frames, in two
 * sets: Club Slots shows the first after three stars and the second after
 * fewer, and the character select shows the second while GAME_FLAG_UNK64 is
 * set
 */
#define FACE_ANIMATION_FRAME_COUNT 8
extern AssetId g_FaceAnimationAssets[][FACE_ANIMATION_FRAME_COUNT];
extern AssetId g_AlternateFaceAnimationAssets[][FACE_ANIMATION_FRAME_COUNT];

#endif
