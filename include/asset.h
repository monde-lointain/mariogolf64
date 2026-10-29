#ifndef ASSET_H
#define ASSET_H

#include <ultra64.h>

/* An asset's number in the archive */
typedef u16 AssetId;

/* An open file of the asset archive in ROM, read from front to back */
typedef struct AssetFile {
  /* 0x0 */ u32 cursor;
  /* 0x4 */ s32 size;
  /* 0x8 */ u32 romStart;
  /* 0xC */ u32 romEnd;
} AssetFile; /* size = 0x10 */

/* EncodedAsset methods; any other is read as halfword runs */
#define ENCODING_BYTE_RUNS 1

/*
 * An asset whose first word holds how the rest is encoded (top byte) and
 * its size once decoded (the other 24 bits)
 */
typedef struct EncodedAsset {
  /* 0x00 */ u8 unk0[8];
  /* 0x08 */ s32 method;
  /* 0x0C */ s32 encodedSize; /* The whole file's, including that word */
  /* 0x10 */ AssetFile file;
} EncodedAsset; /* size = 0x20 */

/* The boot logos, among other image assets, begin with this before their texels
 */
typedef struct ImageHeader {
  /* 0x0 */ u16 unk0;
  /* 0x2 */ u16 width;
  /* 0x4 */ u16 height;
  /* 0x6 */ u16 unk6;
} ImageHeader; /* size = 0x8 */

/* Where ImageHeader's width and height start, for reading them a byte at a time
 */
#define IMAGE_WIDTH_OFFSET 2
#define IMAGE_HEIGHT_OFFSET 4
/* An image header's width and height are halfwords; these are their low bytes
 */
#define IMAGE_WIDTH_LOW_BYTE 3
#define IMAGE_HEIGHT_LOW_BYTE 5

void OpenAssetFile(s32 assetId, AssetFile* file);
AssetFile* OpenPooledAssetFile(s32 assetId);
void CloseAssetFile(AssetFile* file);
s32 GetAssetFileSize(AssetFile* file);
void ReadAssetFile(void* buffer, u32 size, AssetFile* file);
/* Encoded images start with a header, then a CI palette, then texels */
#define IMAGE_HEADER_SIZE 8
/* Each palette color is one RGBA16 value */
#define PALETTE_COLOR_SIZE 2
#define SMALL_PALETTE_COLOR_COUNT 16
#define LARGE_PALETTE_COLOR_COUNT 256
/* Where a four-bit image's texels start, after its header and palette */
#define PALETTED_TEXELS_OFFSET \
  (IMAGE_HEADER_SIZE + (SMALL_PALETTE_COLOR_COUNT * PALETTE_COLOR_SIZE))
/* Where an eight-bit image's texels start, after its header and palette */
#define LARGE_PALETTED_TEXELS_OFFSET \
  (IMAGE_HEADER_SIZE + (LARGE_PALETTE_COLOR_COUNT * PALETTE_COLOR_SIZE))

/* The assets that more than one overlay loads */
#define ASSET_SHADOW_TEXTURE 0x4C3
#define ASSET_BLENDED_TEXTURE_SCROLLING_LAYER 0x4D7
/* A fairway under a blue sky, 152 by 117 */
#define ASSET_COURSE_SCENE 0x51E
/* A long wooden plate, 200 by 30 */
#define ASSET_NAME_PLATE 0x521
/* The image listing the game modes, in orange */
#define ASSET_MENU_GAME_MODES 0x528
/* The first images of a star's spin */
#define ASSET_SPINNING_STAR 0x5BE
#define ASSET_PLAYER_LABELS 0x5E3 /* 1P to 4P, 1C to 4C and Com */
/* Controllers numbered 1 to 4, 32 by 32 each */
#define ASSET_CONTROLLER_ICONS 0x63D
/* A gold coin's turns */
#define ASSET_SPINNING_BADGE 0x663
/* 1 to 4 in rainbow colors, then st, nd, rd and th */
#define ASSET_PLACES 0x688
/* Orange digits and signs in a column */
#define ASSET_LARGE_DIGITS 0x689
/* Place, Name, Score, Hole, Bet and Exp. */
#define ASSET_SCORE_LABELS 0x68A
/* A white streak fading upward, drawn behind Lakitu and as wind */
#define ASSET_STREAK_IMAGE 0x6AF
/* A white rounded square outline */
#define ASSET_CURSOR_BOX 0x6EF
/* The wide cloud behind notices, prompts and the pause menu */
#define ASSET_WIDE_CLOUD 0x70B
/* A pointing glove and two tees */
#define ASSET_POINTING_HAND 0x70F
/* Pixel for pixel the same as ASSET_GREEN_ARROWS */
#define ASSET_GREEN_ARROWS_COPY 0x71E
/* Green arrows pointing up, down, right and left */
#define ASSET_GREEN_ARROWS 0x720
/* Yellow digits, then m, y, ft and a point */
#define ASSET_DISTANCE_DIGITS 0x722
/* A square wooden plaque, 48 by 48 */
#define ASSET_WOODEN_PLAQUE 0x832
/* A glove's frames as it presses a button */
#define ASSET_PRESSING_HAND 0x8EB
/* Green digits, then st, nd, rd and th */
#define ASSET_PLACE_DIGITS 0x98E
/* A 64x64 I8 gradient, which the opening and the club slots sweep across
 * their logos and the round results draw too */
#define ASSET_SHINE_IMAGE 0x6A4
/* The buffers that the opening and the club slots read it into, 0x208 bytes
 * longer than its 0x1008 */
#define SHINE_IMAGE_BUFFER_SIZE 0x1210
/*
 * The first of the club head images, one for each club type in order,
 * which the clubs' data names and the round HUD draws
 */
#define ASSET_DRIVER_HEAD_IMAGE 0x4E3

/* Where the asset archive starts in ROM, just after the last song */
extern u8 g_AssetArchiveRomStart[];

s32 OpenEncodedAsset(AssetId assetId, EncodedAsset* asset);
void ReadEncodedAsset(void* buffer, EncodedAsset* asset);

#endif
