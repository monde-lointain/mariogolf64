#ifndef CHARACTER_H
#define CHARACTER_H

#include <ultra64.h>

/*
 * Indices of the playable characters, in the order of the name table. A
 * character's models are this index plus a multiple of CHARACTER_COUNT.
 */
#define CHARACTER_PLUM 0
#define CHARACTER_CHARLIE 1
#define CHARACTER_PEACH 2
#define CHARACTER_BABY_MARIO 3
#define CHARACTER_LUIGI 4
#define CHARACTER_YOSHI 5
#define CHARACTER_SONNY 6
#define CHARACTER_WARIO 7
#define CHARACTER_HARRY 8
#define CHARACTER_MARIO 9
#define CHARACTER_MAPLE 10
#define CHARACTER_DONKEY_KONG 11
#define CHARACTER_BOWSER 12
#define CHARACTER_METAL_MARIO 13
#define CHARACTER_KID 14
#define CHARACTER_SHERRY 15
#define CHARACTER_AZALEA 16
#define CHARACTER_JOE 17
#define CHARACTER_COUNT 18

/* The first of each character's image assets, of which its face is first */
extern u16 g_CharacterImageAssets[CHARACTER_COUNT];
/* Added to a character's g_CharacterImageAssets entry for the face that menus
 * show */
#define CHARACTER_FACE_IMAGE_OFFSET 9
/* The asset file of each character's first palette, followed by those of its
 * other colours */
extern s16 g_CharacterPaletteAssets[CHARACTER_COUNT];
#define CHARACTER_PALETTE_COUNT 4

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

/* Each character's face palette file in each of the character's colour schemes
 */
extern s16 D_800C2590[][CHARACTER_PALETTE_COUNT];

#endif
