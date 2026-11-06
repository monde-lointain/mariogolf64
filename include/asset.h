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
  /*
   * How the texels are stored: 0 for four-bit ones with no palette, 2 for
   * four-bit indices into a 16-color palette, 4 for eight-bit ones with no
   * palette, 6 for eight-bit indices into a 256-color palette and 8 for RGBA16
   */
  /* 0x0 */ u16 format;
  /* 0x2 */ u16 width;
  /* 0x4 */ u16 height;
  /* 0x6 */ u16 unusedAfterHeight;
} ImageHeader; /* size = 0x8 */

/* Where ImageHeader's width and height start, for reading them a byte at a time
 */
#define IMAGE_WIDTH_OFFSET 2
#define IMAGE_HEIGHT_OFFSET 4
/* An image header's width and height are halfwords; these are their low bytes
 */
#define IMAGE_WIDTH_LOW_BYTE 3
#define IMAGE_HEIGHT_LOW_BYTE 5
/* An image's width and height, read from its header a byte at a time */
#define ReadImageWidth(image) \
  ((((u8*)(image))[IMAGE_WIDTH_OFFSET] << 8) | \
      ((u8*)(image))[IMAGE_WIDTH_LOW_BYTE])
#define ReadImageHeight(image) \
  ((((u8*)(image))[IMAGE_HEIGHT_OFFSET] << 8) | \
      ((u8*)(image))[IMAGE_HEIGHT_LOW_BYTE])

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
/* The image that starts offset bytes into a buffer holding several */
#define ImageAt(buffer, offset) ((u8*)(buffer) + (offset))
/* What follows an image's header: a CI image's palette, any other's texels */
#define ImageBody(image) ((u8*)(image) + IMAGE_HEADER_SIZE)
/* A four-bit CI image's texels */
#define ImageTexels(image) ((u8*)(image) + PALETTED_TEXELS_OFFSET)
/* An eight-bit CI image's texels */
#define LargeImageTexels(image) ((u8*)(image) + LARGE_PALETTED_TEXELS_OFFSET)
/*
 * The same, offset bytes in, as for a frame of an image that holds several.
 * They add the two offsets first, as the original does.
 */
#define ImageBodyAt(image, offset) \
  ((u8*)(image) + ((offset) + IMAGE_HEADER_SIZE))
#define ImageTexelsAt(image, offset) \
  ((u8*)(image) + ((offset) + PALETTED_TEXELS_OFFSET))
#define LargeImageTexelsAt(image, offset) \
  ((u8*)(image) + ((offset) + LARGE_PALETTED_TEXELS_OFFSET))

/* The assets that more than one overlay loads */
#define ASSET_SHADOW_TEXTURE 0x4C3
#define ASSET_BLENDED_TEXTURE_SCROLLING_LAYER 0x4D7
/* A fairway under a blue sky, 152 by 117 */
#define ASSET_COURSE_SCENE 0x51E
/* A wooden board, 256 by 160 */
#define ASSET_WIDE_BOARD 0x520
/* A long wooden plate, 200 by 30 */
#define ASSET_NAME_PLATE 0x521
/* The image listing the game modes, in orange */
#define ASSET_MENU_GAME_MODES 0x528
/* The first images of a star's spin */
#define ASSET_SPINNING_STAR 0x5BE
/* 1P to 4P, 1C to 4C and Com */
#define ASSET_PLAYER_LABELS 0x5E3
/* A rounded square outline in rainbow colors */
#define ASSET_RAINBOW_BOX 0x5F7
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
/* The words YES and NO */
#define ASSET_YES_NO 0x70C
/* Green 1 to 6, then COURSE */
#define ASSET_COURSE_NUMBERS 0x70E
/* A pointing glove and two tees */
#define ASSET_POINTING_HAND 0x70F
/* Pixel for pixel the same as ASSET_GREEN_ARROWS */
#define ASSET_GREEN_ARROWS_COPY 0x71E
/* Green arrows pointing up, down, right and left */
#define ASSET_GREEN_ARROWS 0x720

/*
 * ASSET_GREEN_ARROWS holds a frame of small arrows and then one of large
 * arrows, which the menus alternate between. Each frame is a column of 16 by
 * 16 arrows, four bits a texel, in MenuArrow order.
 */
enum MenuArrow {
  MENU_ARROW_UP,
  MENU_ARROW_DOWN,
  MENU_ARROW_RIGHT,
  MENU_ARROW_LEFT,
  MENU_ARROW_COUNT
};

#define GREEN_ARROW_SIZE 16
#define GREEN_ARROW_IMAGE_SIZE (GREEN_ARROW_SIZE * GREEN_ARROW_SIZE / 2)
#define GREEN_ARROW_FRAME_COUNT 2
#define GREEN_ARROW_FRAME_HEIGHT (GREEN_ARROW_SIZE * MENU_ARROW_COUNT)
#define GREEN_ARROW_FRAME_SIZE (GREEN_ARROW_IMAGE_SIZE * MENU_ARROW_COUNT)
/* The texel row of an arrow's image in its frame */
#define GreenArrowRow(arrow) ((arrow) * GREEN_ARROW_SIZE)
/* Yellow digits, then m, y, ft and a point */
#define ASSET_DISTANCE_DIGITS 0x722
/* Its glyphs sit side by side in a strip DISTANCE_FONT_WIDTH wide */
#define DISTANCE_FONT_WIDTH 128
#define DISTANCE_GLYPH_HEIGHT 12
#define DISTANCE_DIGIT_WIDTH 8
/* A square wooden plaque, 48 by 48 */
#define ASSET_WOODEN_PLAQUE 0x832
/* A glove's frames as it presses a button */
#define ASSET_PRESSING_HAND 0x8EB
/* A plate that reads Nintendo */
#define ASSET_NINTENDO_BADGE 0x949
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
void InitAssetFiles(void);

#endif
