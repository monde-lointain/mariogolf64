#include "system/asset.h"

#include <nusys.h>
#include <ultra64.h>

#include "debug/game_flags.h"
#include "system/compiler.h"

#define ASSET_FILE_POOL_SIZE 2

#define ASSET_CHUNK_SIZE 0x2000

#define AlignedReadBuffer(buffer) ((void *)(((u32)(buffer) + 0xF) & ~0xF))

#define ReadNextChunk(chunk, remaining, asset) \
    if ((remaining) < ASSET_CHUNK_SIZE) { \
        ReadAssetFile(chunk, remaining, &(asset)->file); \
        (remaining) = 0; \
    } \
    else { \
        ReadAssetFile(chunk, ASSET_CHUNK_SIZE, &(asset)->file); \
        (remaining) -= ASSET_CHUNK_SIZE; \
    }
#define AdvanceChunkPosition(chunk, position, remaining, asset) \
    { \
        (position)++; \
        if ((remaining) != 0 && (position) == ASSET_CHUNK_SIZE) { \
            ReadNextChunk(chunk, remaining, asset); \
            (position) = 0; \
        } \
    }

#define ASSET_TABLE ((AssetTableEntry *)g_AssetArchiveRomStart)

typedef struct AssetTableEntry {
    s32 size;
    u32 offset;
} AssetTableEntry;

typedef struct ChunkReader {
    u8 rawChunk[ASSET_CHUNK_SIZE + 0x10];
    s32 position;
    u32 remaining;
} ChunkReader;

typedef struct LargeChunkReader {
    u8 rawChunk[ASSET_CHUNK_SIZE + 0x20];
    s32 position;
    u32 remaining;
} LargeChunkReader;

extern u8 g_AssetArchiveRomStart[];
static AssetFile f_AssetFilePool[ASSET_FILE_POOL_SIZE];
static u8 f_UnusedBufferAfterAssetFilePool[0x40] UNUSED;

FILE_STATIC WideAssetId f_LastOpenedAssetId;

void DecodeByteRuns(u8 *buffer, EncodedAsset *asset);
void DecodeHalfwordRuns(u8 *buffer, EncodedAsset *asset);

void InitAssetFiles(void) {
    s32 index;
    AssetFile *file;

    for (index = 0, file = f_AssetFilePool; index != ASSET_FILE_POOL_SIZE; index++) {
        file->cursor = 0;
        file++;
    }
}

void ReadAssetFileBounds(WideAssetId assetId, AssetFile *file) {
    u8 rawOffset[0x40];
    u8 rawSize[0x40];
    u32 *offset = AlignedReadBuffer(rawOffset);
    s32 *size = AlignedReadBuffer(rawSize);

    nuPiReadRom((u32)&ASSET_TABLE[assetId].size, size, sizeof(*size));
    nuPiReadRom((u32)&ASSET_TABLE[assetId].offset, offset, sizeof(*offset));
    file->romStart = (u32)g_AssetArchiveRomStart + *offset;
    file->cursor = file->romStart;
    file->romEnd = file->romStart + *size;
    file->size = *size;
    f_LastOpenedAssetId = assetId;
}

void OpenAssetFile(WideAssetId assetId, AssetFile *file) {
    ReadAssetFileBounds(assetId, file);
}

AssetFile *OpenPooledAssetFile(WideAssetId assetId) {
    s32 index;
    AssetFile *file;

    for (index = 0, file = f_AssetFilePool; index != ASSET_FILE_POOL_SIZE; index++) {
        if (file->cursor == 0) {
            ReadAssetFileBounds(assetId, file);
            return file;
        }

        file++;
    }

    osSyncPrintf("ropen %d\n", assetId);

    osSyncPrintf("%d番のファイルオープン失敗。\n", assetId);
    return NULL;
}

void CloseAssetFile(AssetFile *file) {
    file->romEnd = 0;
    file->romStart = 0;
    file->cursor = 0;
}

s32 GetAssetFileSize(AssetFile *file) {
    return file->size;
}

void ReadAssetFile(void *buffer, u32 size, AssetFile *file) {
    if (file->romEnd < file->cursor + size) {
        size = file->romEnd - file->cursor;
        osSyncPrintf("size error %d \n", f_LastOpenedAssetId);
    }

    nuPiReadRom(file->cursor, buffer, (size + 1) & ~1);
    file->cursor += size;
}

s32 OpenEncodedAsset(AssetId assetId, EncodedAsset *asset) {
    u8 rawHeader[0x80];
    u32 *header = AlignedReadBuffer(rawHeader);

    if (IsGameFlagSet(GAME_FLAG_LOG_ENCODED_ASSETS)) {
        osSyncPrintf("OP:%d\n", assetId);
    }

    OpenAssetFile(assetId, &asset->file);
    asset->encodedSize = GetAssetFileSize(&asset->file);
    ReadAssetFile(header, sizeof(*header), &asset->file);
    asset->method = *(u8 *)header;
    return *header &= 0xFFFFFF;
}

void ReadEncodedAsset(void *buffer, EncodedAsset *asset) {
    if (asset->method == ENCODING_BYTE_RUNS) {
        DecodeByteRuns(buffer, asset);
    }
    else {
        DecodeHalfwordRuns(buffer, asset);
    }
}

void DecodeByteRuns(u8 *buffer, EncodedAsset *asset) {
    ChunkReader reader;
    u8 *chunk;
    s32 runLength;
    s32 index;

    reader.remaining = asset->encodedSize - sizeof(u32);
    chunk = AlignedReadBuffer(reader.rawChunk);
    ReadNextChunk(chunk, reader.remaining, asset);
    reader.position = 0;
    while (TRUE) {
        runLength = (s8)chunk[reader.position];
        AdvanceChunkPosition(chunk, reader.position, reader.remaining, asset);
        if (runLength == 0) {
            break;
        }

        if (runLength > 0) {
            for (index = 0; index != runLength; index++) {
                *buffer++ = chunk[reader.position];
            }

            AdvanceChunkPosition(chunk, reader.position, reader.remaining, asset);
        }
        else {
            runLength = -runLength;
            for (index = 0; index != runLength; index++) {
                *buffer++ = chunk[reader.position];
                AdvanceChunkPosition(chunk, reader.position, reader.remaining, asset);
            }
        }
    }
}

void DecodeHalfwordRuns(u8 *buffer, EncodedAsset *asset) {
    LargeChunkReader reader;
    u8 *chunk;
    s32 runLength;
    s32 index;
    s32 repeatCount;
    u8 highByte;
    u8 lowByte;

    reader.remaining = asset->encodedSize - sizeof(u32);
    chunk = AlignedReadBuffer(reader.rawChunk);
    ReadNextChunk(chunk, reader.remaining, asset);
    reader.position = 0;
    while (TRUE) {
        runLength = (s8)chunk[reader.position];
        AdvanceChunkPosition(chunk, reader.position, reader.remaining, asset);
        if (runLength == 0) {
            break;
        }

        if (runLength > 0) {
            highByte = chunk[reader.position];
            AdvanceChunkPosition(chunk, reader.position, reader.remaining, asset);
            lowByte = chunk[reader.position];
            repeatCount = runLength;
            AdvanceChunkPosition(chunk, reader.position, reader.remaining, asset);
            for (index = 0; index != repeatCount; index++) {
                *buffer++ = highByte;
                *buffer++ = lowByte;
            }
        }
        else {
            runLength = -runLength;
            for (index = 0; index != runLength; index++) {
                highByte = chunk[reader.position];
                AdvanceChunkPosition(chunk, reader.position, reader.remaining, asset);
                lowByte = chunk[reader.position];
                AdvanceChunkPosition(chunk, reader.position, reader.remaining, asset);
                *buffer++ = highByte;
                *buffer++ = lowByte;
            }
        }
    }
}
