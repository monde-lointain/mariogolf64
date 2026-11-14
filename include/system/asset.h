#ifndef ASSET_H
#define ASSET_H

#include <ultra64.h>

#define ENCODING_BYTE_RUNS 1

#define IMAGE_WIDTH_OFFSET 2
#define IMAGE_HEIGHT_OFFSET 4
#define IMAGE_WIDTH_LOW_BYTE 3
#define IMAGE_HEIGHT_LOW_BYTE 5
#define ReadImageWidth(image) \
    ((((u8 *)(image))[IMAGE_WIDTH_OFFSET] << 8) | ((u8 *)(image))[IMAGE_WIDTH_LOW_BYTE])
#define ReadImageHeight(image) \
    ((((u8 *)(image))[IMAGE_HEIGHT_OFFSET] << 8) | ((u8 *)(image))[IMAGE_HEIGHT_LOW_BYTE])
#define ReadSmallImageWidth(image) (((u8 *)(image))[IMAGE_WIDTH_LOW_BYTE])
#define ReadSmallImageHeight(image) (((u8 *)(image))[IMAGE_HEIGHT_LOW_BYTE])

#define IMAGE_HEADER_SIZE 8
#define PALETTE_COLOR_SIZE 2
#define SMALL_PALETTE_COLOR_COUNT 16
#define LARGE_PALETTE_COLOR_COUNT 256
#define PALETTED_TEXELS_OFFSET \
    (IMAGE_HEADER_SIZE + (SMALL_PALETTE_COLOR_COUNT * PALETTE_COLOR_SIZE))
#define LARGE_PALETTED_TEXELS_OFFSET \
    (IMAGE_HEADER_SIZE + (LARGE_PALETTE_COLOR_COUNT * PALETTE_COLOR_SIZE))
#define ImageAt(buffer, offset) ((u8 *)(buffer) + (offset))
#define ImageBody(image) ((u8 *)(image) + IMAGE_HEADER_SIZE)
#define ImagePalette(image) ((PaletteColor *)ImageBody(image))
#define ImageTexels(image) ((u8 *)(image) + PALETTED_TEXELS_OFFSET)
#define LargeImageTexels(image) ((u8 *)(image) + LARGE_PALETTED_TEXELS_OFFSET)
#define ImageBodyAt(image, offset) ((u8 *)(image) + ((offset) + IMAGE_HEADER_SIZE))
#define ImagePaletteAt(image, offset) ((PaletteColor *)ImageBodyAt(image, offset))
#define ImageTexelsAt(image, offset) ((u8 *)(image) + ((offset) + PALETTED_TEXELS_OFFSET))
#define LargeImageTexelsAt(image, offset) \
    ((u8 *)(image) + ((offset) + LARGE_PALETTED_TEXELS_OFFSET))

#define ASSET_SHADOW_TEXTURE 0x4C3
#define SHADOW_TEXTURE_SIZE 32
#define SHADOW_TEXTURE_SIZE_LOG2 5
#define ASSET_BLENDED_TEXTURE_SCROLLING_LAYER 0x4D7
#define ASSET_DRIVER_HEAD_IMAGE 0x4E3
#define ASSET_COURSE_SCENE 0x51E
#define ASSET_WIDE_BOARD 0x520
#define ASSET_NAME_PLATE 0x521
#define ASSET_MENU_GAME_MODES 0x528
#define ASSET_SPINNING_STAR 0x5BE
#define ASSET_PLAYER_LABELS 0x5E3
#define ASSET_RAINBOW_BOX 0x5F7
#define ASSET_CONTROLLER_ICONS 0x63D
#define ASSET_HOLES_BADGES 0x65D
#define ASSET_SPINNING_BADGE 0x663
#define SPINNING_BADGE_SIZE 24
#define SPINNING_BADGE_FRAME_SIZE (SPINNING_BADGE_SIZE * SPINNING_BADGE_SIZE / 2)
#define SPINNING_BADGE_FRAME_COUNT 6
#define ASSET_PLACES 0x688
#define PLACE_PART_SIZE 32
#define PLACE_PART_IMAGE_SIZE (PLACE_PART_SIZE * PLACE_PART_SIZE)
#define PLACE_NUMBER_PART_COUNT 4
#define ASSET_LARGE_DIGITS 0x689
#define ASSET_SCORE_LABELS 0x68A
#define ASSET_SHINE_IMAGE 0x6A4
#define SHINE_SIZE 64
#define SHINE_IMAGE_BUFFER_SIZE 0x1210
#define ASSET_STREAK_IMAGE 0x6AF
#define STREAK_IMAGE_WIDTH 48
#define STREAK_IMAGE_HEIGHT 128
#define ASSET_CURSOR_BOX 0x6EF
#define ASSET_WIDE_CLOUD 0x70B
#define ASSET_YES_NO 0x70C
#define YES_NO_BUTTON_WIDTH 72
#define YES_NO_BUTTON_HEIGHT 24
#define ASSET_COURSE_NUMBERS 0x70E
#define ASSET_POINTING_HAND 0x70F
#define POINTING_HAND_SIZE 24
#define POINTING_HAND_LOAD_HEIGHT 76
#define ASSET_GREEN_ARROWS_COPY 0x71E
#define ASSET_GREEN_ARROWS 0x720

#define GREEN_ARROW_SIZE 16
#define GREEN_ARROW_IMAGE_SIZE (GREEN_ARROW_SIZE * GREEN_ARROW_SIZE / 2)
#define GREEN_ARROW_FRAME_COUNT 2
#define GREEN_ARROW_FRAME_HEIGHT (GREEN_ARROW_SIZE * MENU_ARROW_COUNT)
#define GREEN_ARROW_FRAME_SIZE (GREEN_ARROW_IMAGE_SIZE * MENU_ARROW_COUNT)
#define GreenArrowRow(arrow) ((arrow) * GREEN_ARROW_SIZE)
#define ASSET_BRICK_WALL 0x721
#define ASSET_DISTANCE_DIGITS 0x722
#define DISTANCE_FONT_WIDTH 128
#define DISTANCE_GLYPH_HEIGHT 12
#define DISTANCE_DIGIT_WIDTH 8
#define ASSET_WOODEN_PLAQUE 0x832
#define WOODEN_PLAQUE_SIZE 48
#define ASSET_PRESSING_HAND 0x8EB
#define PRESSING_HAND_SIZE 24
#define PRESSING_HAND_IMAGE_SIZE (PRESSING_HAND_SIZE * PRESSING_HAND_SIZE)
#define PRESSING_HAND_IMAGE_COUNT 3
#define PRESSING_HAND_FRAMES_PER_IMAGE 3
#define PRESSING_HAND_FRAME_COUNT (PRESSING_HAND_IMAGE_COUNT * PRESSING_HAND_FRAMES_PER_IMAGE)
#define CORNER_PRESSING_HAND_LEFT 272
#define CORNER_PRESSING_HAND_TOP 192
#define ASSET_NINTENDO_BADGE 0x949
#define ASSET_STAT_LABELS 0x978
#define ASSET_SHOT_TYPES 0x979
#define ASSET_PLACE_DIGITS 0x98E

enum MenuArrow {
    MENU_ARROW_UP,
    MENU_ARROW_DOWN,
    MENU_ARROW_RIGHT,
    MENU_ARROW_LEFT,
    MENU_ARROW_COUNT
};

typedef s32 MenuArrow;

typedef u16 AssetId;
typedef s32 WideAssetId;
typedef s16 SignedAssetId;

typedef u16 PaletteColor;
typedef void ImageAsset;

typedef struct AssetFile {
    u32 cursor;
    s32 size;
    u32 romStart;
    u32 romEnd;
} AssetFile;

typedef struct EncodedAsset {
    u8 unusedBeforeMethod[8];
    s32 method;
    s32 encodedSize;
    AssetFile file;
} EncodedAsset;

typedef struct ImageHeader {
    u16 format;
    u16 width;
    u16 height;
    u16 unusedAfterHeight;
} ImageHeader;

void InitAssetFiles(void);

void OpenAssetFile(WideAssetId assetId, AssetFile *file);
AssetFile *OpenPooledAssetFile(WideAssetId assetId);
void CloseAssetFile(AssetFile *file);
s32 GetAssetFileSize(AssetFile *file);
void ReadAssetFile(void *buffer, u32 size, AssetFile *file);

s32 OpenEncodedAsset(AssetId assetId, EncodedAsset *asset);
void ReadEncodedAsset(void *buffer, EncodedAsset *asset);

#endif
