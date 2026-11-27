/*
 * The asset archive in ROM: opening and reading its files, decoding its
 * encoded assets, the layout of its images, and the numbers of the assets
 * that more than one file loads.
 */

#ifndef ASSET_H
#define ASSET_H

#include <ultra64.h>

#include "system/compiler.h"
#include "system/memory_address.h"

// EncodedAsset methods; any other is read as halfword runs
#define ENCODING_BYTE_RUNS 1

// Where ImageHeader's width and height start, for reading them a byte at a time
#define IMAGE_WIDTH_OFFSET 2
#define IMAGE_HEIGHT_OFFSET 4
// An image header's width and height are halfwords; these are their low bytes
#define IMAGE_WIDTH_LOW_BYTE 3
#define IMAGE_HEIGHT_LOW_BYTE 5

// An image's width and height, read from its header a byte at a time
#define ReadImageWidth(image) \
    ((((u8 *)(image))[IMAGE_WIDTH_OFFSET] << 8) | ((u8 *)(image))[IMAGE_WIDTH_LOW_BYTE])
#define ReadImageHeight(image) \
    ((((u8 *)(image))[IMAGE_HEIGHT_OFFSET] << 8) | ((u8 *)(image))[IMAGE_HEIGHT_LOW_BYTE])
// The same, for an image under 256 texels on a side
#define ReadSmallImageWidth(image) (((u8 *)(image))[IMAGE_WIDTH_LOW_BYTE])
#define ReadSmallImageHeight(image) (((u8 *)(image))[IMAGE_HEIGHT_LOW_BYTE])

// Encoded images start with a header, then a CI palette, then texels
#define IMAGE_HEADER_SIZE 8
// Each palette color is one RGBA16 value
#define PALETTE_COLOR_SIZE 2
#define SMALL_PALETTE_COLOR_COUNT 16
#define LARGE_PALETTE_COLOR_COUNT 256
// Where a four-bit image's texels start, after its header and palette
#define PALETTED_TEXELS_OFFSET \
    (IMAGE_HEADER_SIZE + (SMALL_PALETTE_COLOR_COUNT * PALETTE_COLOR_SIZE))
// Where an eight-bit image's texels start, after its header and palette
#define LARGE_PALETTED_TEXELS_OFFSET \
    (IMAGE_HEADER_SIZE + (LARGE_PALETTE_COLOR_COUNT * PALETTE_COLOR_SIZE))

// The image that starts offset bytes into a buffer holding several
#define ImageAt(buffer, offset) ((u8 *)(buffer) + (offset))
#ifdef AVOID_UB
// The effects load the textures of each kind they draw, whether or not
// that kind's image is loaded, which the N64 read from the address past 0.
// C leaves adding to a null pointer undefined; adding to its address isn't.
#    define ImageBody(image) ((u8 *)(AddressNumber(image) + IMAGE_HEADER_SIZE))
#    define ImageTexels(image) ((u8 *)(AddressNumber(image) + PALETTED_TEXELS_OFFSET))
#    define LargeImageTexels(image) ((u8 *)(AddressNumber(image) + LARGE_PALETTED_TEXELS_OFFSET))
#else
// What follows an image's header: a CI image's palette, any other's texels
#    define ImageBody(image) ((u8 *)(image) + IMAGE_HEADER_SIZE)
// A four-bit CI image's texels
#    define ImageTexels(image) ((u8 *)(image) + PALETTED_TEXELS_OFFSET)
// An eight-bit CI image's texels
#    define LargeImageTexels(image) ((u8 *)(image) + LARGE_PALETTED_TEXELS_OFFSET)
#endif
#define ImagePalette(image) ((PaletteColor *)ImageBody(image))
// The same, offset bytes in, as for a frame of an image that holds several
#define ImageBodyAt(image, offset) ((u8 *)(image) + ((offset) + IMAGE_HEADER_SIZE))
#define ImagePaletteAt(image, offset) ((PaletteColor *)ImageBodyAt(image, offset))
#define ImageTexelsAt(image, offset) ((u8 *)(image) + ((offset) + PALETTED_TEXELS_OFFSET))
#define LargeImageTexelsAt(image, offset) \
    ((u8 *)(image) + ((offset) + LARGE_PALETTED_TEXELS_OFFSET))

// The assets that more than one file loads, in the order of their numbers,
// each followed by the measures of its images

#define ASSET_SHADOW_TEXTURE 0x4C3
#define SHADOW_TEXTURE_SIZE 32
#define SHADOW_TEXTURE_SIZE_LOG2 5

#define ASSET_BLENDED_TEXTURE_SCROLLING_LAYER 0x4D7

// The first of the club head images, one for each club type in order,
// which the clubs' data names and the round HUD draws
#define ASSET_DRIVER_HEAD_IMAGE 0x4E3

// A fairway under a blue sky, 152 by 117
#define ASSET_COURSE_SCENE 0x51E
// A wooden board, 256 by 160
#define ASSET_WIDE_BOARD 0x520
// A long wooden plate, 200 by 30
#define ASSET_NAME_PLATE 0x521
// The image listing the game modes, in orange
#define ASSET_MENU_GAME_MODES 0x528

// The first images of a star's spin
#define ASSET_SPINNING_STAR 0x5BE
// 1P to 4P, 1C to 4C and Com
#define ASSET_PLAYER_LABELS 0x5E3
// A rounded square outline in rainbow colors
#define ASSET_RAINBOW_BOX 0x5F7
// Controllers numbered 1 to 4, 32 by 32 each
#define ASSET_CONTROLLER_ICONS 0x63D
// Bet, Front 9, Back 9 and 18
#define ASSET_HOLES_BADGES 0x65D

// A gold coin's turns
#define ASSET_SPINNING_BADGE 0x663
#define SPINNING_BADGE_SIZE 24
#define SPINNING_BADGE_FRAME_SIZE (SPINNING_BADGE_SIZE * SPINNING_BADGE_SIZE / 2)
#define SPINNING_BADGE_FRAME_COUNT 6

// 1 to 4 in rainbow colors, then st, nd, rd and th
#define ASSET_PLACES 0x688
#define PLACE_PART_SIZE 32
#define PLACE_PART_IMAGE_SIZE (PLACE_PART_SIZE * PLACE_PART_SIZE)
// The number icons before the suffixes
#define PLACE_NUMBER_PART_COUNT 4

// Orange digits and signs in a column
#define ASSET_LARGE_DIGITS 0x689
// Place, Name, Score, Hole, Bet and Exp.
#define ASSET_SCORE_LABELS 0x68A

// A 64x64 I8 gradient, which the opening and the club slots sweep across
// their logos and the round results draw too
#define ASSET_SHINE_IMAGE 0x6A4
#define SHINE_SIZE 64
// The buffers that the opening and the club slots read it into, 0x208 bytes
// longer than its 0x1008
#define SHINE_IMAGE_BUFFER_SIZE 0x1210

// A white streak fading upward, drawn behind Lakitu and as wind
#define ASSET_STREAK_IMAGE 0x6AF
#define STREAK_IMAGE_WIDTH 48
#define STREAK_IMAGE_HEIGHT 128

// A white rounded square outline
#define ASSET_CURSOR_BOX 0x6EF
// The wide cloud behind notices, prompts and the pause menu
#define ASSET_WIDE_CLOUD 0x70B

// The words YES and NO
#define ASSET_YES_NO 0x70C
#define YES_NO_BUTTON_WIDTH 72
#define YES_NO_BUTTON_HEIGHT 24

// Green 1 to 6, then COURSE
#define ASSET_COURSE_NUMBERS 0x70E

// A pointing glove and two tees
#define ASSET_POINTING_HAND 0x70F
#define POINTING_HAND_SIZE 24
#define POINTING_HAND_LOAD_HEIGHT 76

// Pixel for pixel the same as ASSET_GREEN_ARROWS
#define ASSET_GREEN_ARROWS_COPY 0x71E
// Green arrows pointing up, down, right and left
#define ASSET_GREEN_ARROWS 0x720

// ASSET_GREEN_ARROWS holds a frame of small arrows and then one of large
// arrows, which the menus alternate between. Each frame is a column of 16 by
// 16 arrows, four bits a texel, in MenuArrow order.
#define GREEN_ARROW_SIZE 16
#define GREEN_ARROW_IMAGE_SIZE (GREEN_ARROW_SIZE * GREEN_ARROW_SIZE / 2)
#define GREEN_ARROW_FRAME_COUNT 2
#define GREEN_ARROW_FRAME_HEIGHT (GREEN_ARROW_SIZE * MENU_ARROW_COUNT)
#define GREEN_ARROW_FRAME_SIZE (GREEN_ARROW_IMAGE_SIZE * MENU_ARROW_COUNT)
// The texel row of an arrow's image in its frame
#define GreenArrowRow(arrow) ((arrow) * GREEN_ARROW_SIZE)

// A brick wall, 152 by 117
#define ASSET_BRICK_WALL 0x721

// Yellow digits, then m, y, ft and a point
#define ASSET_DISTANCE_DIGITS 0x722
// Its glyphs sit side by side in a strip DISTANCE_FONT_WIDTH wide
#define DISTANCE_FONT_WIDTH 128
#define DISTANCE_GLYPH_HEIGHT 12
#define DISTANCE_DIGIT_WIDTH 8

// A square wooden plaque, 48 by 48
#define ASSET_WOODEN_PLAQUE 0x832
#define WOODEN_PLAQUE_SIZE 48

// A glove's frames as it presses a button
#define ASSET_PRESSING_HAND 0x8EB
#define PRESSING_HAND_SIZE 24
#define PRESSING_HAND_IMAGE_SIZE (PRESSING_HAND_SIZE * PRESSING_HAND_SIZE)
#define PRESSING_HAND_IMAGE_COUNT 3
// Each image shows for this many draws
#define PRESSING_HAND_FRAMES_PER_IMAGE 3
#define PRESSING_HAND_FRAME_COUNT (PRESSING_HAND_IMAGE_COUNT * PRESSING_HAND_FRAMES_PER_IMAGE)
// Where the hand is drawn in the bottom right corner of the screen
#define CORNER_PRESSING_HAND_LEFT 272
#define CORNER_PRESSING_HAND_TOP 192

// A plate that reads Nintendo
#define ASSET_NINTENDO_BADGE 0x949

#define ASSET_STAT_LABELS 0x978
#define ASSET_SHOT_TYPES 0x979

// Green digits, then st, nd, rd and th
#define ASSET_PLACE_DIGITS 0x98E

// The arrows of each ASSET_GREEN_ARROWS frame, in the order they are stacked
enum MenuArrow {
    MENU_ARROW_UP,
    MENU_ARROW_DOWN,
    MENU_ARROW_RIGHT,
    MENU_ARROW_LEFT,
    MENU_ARROW_COUNT
};

typedef s32 MenuArrow;

// An asset's number in the archive. Routines that take one in a register take
// it as a WideAssetId, and the tables that store one signed hold a
// SignedAssetId.
typedef u16 AssetId;
typedef s32 WideAssetId;
typedef s16 SignedAssetId;

// An RGBA16 color in an image's palette
typedef u16 PaletteColor;

// The bytes of an image asset as read: an ImageHeader, then its palette, if
// any, then its texels
typedef void ImageAsset;

// An open file of the asset archive in ROM, read from front to back
typedef struct AssetFile {
    u32 cursor; // The ROM address to read next, or 0 if the file is closed
    s32 size;
    u32 romStart;
    u32 romEnd;
} AssetFile;

// An asset whose first word holds how the rest is encoded (top byte) and
// its size once decoded (the other 24 bits)
typedef struct EncodedAsset {
    // Nothing uses these bytes
    u8 unusedBeforeMethod[8];
    s32 method;
    s32 encodedSize; // The whole file's, including that word
    AssetFile file;
#ifdef TARGET_PC
    u32 decodedSize; // For marking what the decoders write as the ROM's order
#endif
} EncodedAsset;

// The boot logos, among other image assets, begin with this before their texels.
// The texels that follow must start on an 8-byte boundary for the RDP, so
// every structure that holds a header has that alignment.
typedef struct ImageHeader {
    // How the texels are stored: 0 for four-bit ones with no palette, 2 for
    // four-bit indices into a 16-color palette, 4 for eight-bit ones with no
    // palette, 6 for eight-bit indices into a 256-color palette and 8 for RGBA16
    u16 format;
    u16 width;
    u16 height;
    u16 unusedAfterHeight;
} RCP_ALIGNED ImageHeader;

void InitAssetFiles(void);
void OpenAssetFile(WideAssetId assetId, AssetFile *file);
AssetFile *OpenPooledAssetFile(WideAssetId assetId);
void CloseAssetFile(AssetFile *file);
s32 GetAssetFileSize(AssetFile *file);
void ReadAssetFile(void *buffer, u32 size, AssetFile *file);

s32 OpenEncodedAsset(AssetId assetId, EncodedAsset *asset);
void ReadEncodedAsset(void *buffer, EncodedAsset *asset);

#endif
