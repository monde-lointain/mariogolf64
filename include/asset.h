#ifndef ASSET_H
#define ASSET_H

#include <ultra64.h>

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

/* The boot logos, among other image assets, begin with this before their texels */
typedef struct ImageHeader {
    /* 0x0 */ u16 unk0;
    /* 0x2 */ u16 width;
    /* 0x4 */ u16 height;
    /* 0x6 */ u16 unk6;
} ImageHeader; /* size = 0x8 */
/* Where ImageHeader's width and height start, for reading them a byte at a time */
#define IMAGE_WIDTH_OFFSET 2
#define IMAGE_HEIGHT_OFFSET 4
/* An image header's width and height are halfwords; these are their low bytes */
#define IMAGE_WIDTH_LOW_BYTE 3
#define IMAGE_HEIGHT_LOW_BYTE 5

void OpenAssetFile(s32 assetId, AssetFile *file);
AssetFile *OpenPooledAssetFile(s32 assetId);
void CloseAssetFile(AssetFile *file);
s32 GetAssetFileSize(AssetFile *file);
void ReadAssetFile(void *buffer, u32 size, AssetFile *file);
/* Encoded images start with a header, then a CI palette, then texels */
#define IMAGE_HEADER_SIZE 8
/* Each palette colour is one RGBA16 value */
#define PALETTE_COLOR_SIZE 2
#define SMALL_PALETTE_COLOR_COUNT 16
#define LARGE_PALETTE_COLOR_COUNT 256
/* Where a four-bit image's texels start, after its header and palette */
#define PALETTED_TEXELS_OFFSET (IMAGE_HEADER_SIZE + SMALL_PALETTE_COLOR_COUNT * PALETTE_COLOR_SIZE)
/* Where an eight-bit image's texels start, after its header and palette */
#define LARGE_PALETTED_TEXELS_OFFSET (IMAGE_HEADER_SIZE + LARGE_PALETTE_COLOR_COUNT * PALETTE_COLOR_SIZE)

/* The asset that more than one menu loads */
#define ASSET_PLAYER_LABELS 0x5E3 /* 1P to 4P, 1C to 4C and Com */

s32 OpenEncodedAsset(u16 assetId, EncodedAsset *asset);
void ReadEncodedAsset(void *buffer, EncodedAsset *asset);

#endif
