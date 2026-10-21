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

void OpenAssetFile(s32 assetId, AssetFile *file);
void ReadAssetFile(void *buffer, u32 size, AssetFile *file);

#endif
