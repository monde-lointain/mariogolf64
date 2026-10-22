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

void OpenAssetFile(s32 assetId, AssetFile *file);
AssetFile *OpenPooledAssetFile(s32 assetId);
void CloseAssetFile(AssetFile *file);
s32 GetAssetFileSize(AssetFile *file);
void ReadAssetFile(void *buffer, u32 size, AssetFile *file);
s32 OpenEncodedAsset(u16 assetId, EncodedAsset *asset);
void ReadEncodedAsset(void *buffer, EncodedAsset *asset);

#endif
