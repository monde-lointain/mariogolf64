/*
 * Reads assets out of the asset archive. Files from a pool are opened on an
 * asset and read a part at a time, and encoded assets are decoded from runs of
 * bytes or of halfwords.
 */

#include "system/asset.h"

#include <nusys.h>
#include <ultra64.h>

#include "debug/game_flags.h"
#include "system/big_endian.h"
#include "system/compiler.h"
#include "system/memory_address.h"
#include "system/rom_files.h"

// How many files OpenPooledAssetFile can keep open at once
#define ASSET_FILE_POOL_SIZE 2

// Encoded assets are read through a buffer of this many bytes
#define ASSET_CHUNK_SIZE 0x2000

// Rounds a stack buffer up to the 16-byte alignment that ROM reads need
#define AlignedReadBuffer(buffer) ((void *)((AddressNumber(buffer) + 0xF) & ~0xF))

// Reads the next chunk of the asset into chunk
#define ReadNextChunk(chunk, remaining, asset) \
    if ((remaining) < ASSET_CHUNK_SIZE) { \
        ReadAssetFile(chunk, remaining, &(asset)->file); \
        (remaining) = 0; \
    } \
    else { \
        ReadAssetFile(chunk, ASSET_CHUNK_SIZE, &(asset)->file); \
        (remaining) -= ASSET_CHUNK_SIZE; \
    }

// Moves past a byte of chunk, reading the next chunk at its end
#define AdvanceChunkPosition(chunk, position, remaining, asset) \
    { \
        (position)++; \
        if ((remaining) != 0 && (position) == ASSET_CHUNK_SIZE) { \
            ReadNextChunk(chunk, remaining, asset); \
            (position) = 0; \
        } \
    }

// The archive in ROM starts with a table of each asset's AssetTableEntry
#define ASSET_TABLE ((AssetTableEntry *)g_AssetArchiveRomStart)

typedef struct AssetTableEntry {
    s32 size;
    u32 offset; // From the archive's start
} AssetTableEntry;

// An encoded asset's bytes, read a chunk at a time
typedef struct ChunkReader {
    u8 rawChunk[ASSET_CHUNK_SIZE + 0x10]; // Aligned as chunk
    s32 position; // In chunk
    u32 remaining; // Unread bytes of the asset
} ChunkReader;

// DecodeHalfwordRuns' buffer, 0x10 bytes larger
typedef struct LargeChunkReader {
    u8 rawChunk[ASSET_CHUNK_SIZE + 0x20];
    s32 position;
    u32 remaining;
} LargeChunkReader;

static AssetFile f_AssetFilePool[ASSET_FILE_POOL_SIZE];
// Reserved, but nothing reads them
static u8 f_UnusedBufferAfterAssetFilePool[0x40] UNUSED;
// For the message when a read runs past a file's end
FILE_STATIC WideAssetId f_LastOpenedAssetId;

void DecodeByteRuns(u8 *buffer, EncodedAsset *asset);
void DecodeHalfwordRuns(u8 *buffer, EncodedAsset *asset);

// Marks every file of the pool free
void InitAssetFiles(void) {
    s32 index;
    AssetFile *file;

    for (index = 0, file = f_AssetFilePool; index != ASSET_FILE_POOL_SIZE; index++) {
        file->cursor = 0;
        file++;
    }
}

// Points file at the asset's bytes in the archive
void ReadAssetFileBounds(WideAssetId assetId, AssetFile *file) {
    u8 rawOffset[0x40];
    u8 rawSize[0x40];
    u32 *offset = AlignedReadBuffer(rawOffset);
    s32 *size = AlignedReadBuffer(rawSize);

    nuPiReadRom((u32)AddressNumber(&ASSET_TABLE[assetId].size), size, sizeof(*size));
    nuPiReadRom((u32)AddressNumber(&ASSET_TABLE[assetId].offset), offset, sizeof(*offset));
    ConvertFromBigEndian32(*size);
    ConvertFromBigEndian32(*offset);
    file->romStart = (u32)AddressNumber(g_AssetArchiveRomStart) + *offset;
    file->cursor = file->romStart;
    file->romEnd = file->romStart + *size;
    file->size = *size;
    f_LastOpenedAssetId = assetId;
}

void OpenAssetFile(WideAssetId assetId, AssetFile *file) {
    ReadAssetFileBounds(assetId, file);
}

// Opens the asset in a free file of the pool, or returns NULL if none is free
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
    // "File number %d failed to open."
    osSyncPrintf("%d番のファイルオープン失敗。\n", assetId);
    return NULL;
}

// Frees the file for OpenPooledAssetFile
void CloseAssetFile(AssetFile *file) {
    file->romEnd = 0;
    file->romStart = 0;
    file->cursor = 0;
}

s32 GetAssetFileSize(AssetFile *file) {
    return file->size;
}

// Reads the next size bytes of the file, or what remains of it if fewer
void ReadAssetFile(void *buffer, u32 size, AssetFile *file) {
    if (file->romEnd < file->cursor + size) {
        size = file->romEnd - file->cursor;
        osSyncPrintf("size error %d \n", f_LastOpenedAssetId);
    }

#ifdef AVOID_UB
    // Animations and screen changes read assets over the images and palettes
    // that frames already queued may still load, so whether such a frame
    // shows the old image or the new one depends on how far the RDP lags.
    // The graphics thread, which updates and draws the game, waits for those
    // frames to end before each read; decoding reads through here before it
    // writes, so the wait covers it too. Loader threads fill buffers for
    // scenes not yet drawn, and the game waits on their pace, so they don't
    // wait. The graphics thread is the only one at its priority, and the
    // library has no osGetThreadId.
    if (osGetThreadPri(NULL) == NU_GFX_THREAD_PRI) {
        nuGfxTaskAllEndWait();
    }
#endif

    nuPiReadRom(file->cursor, buffer, (size + 1) & ~1);
    file->cursor += size;
}

// Opens an encoded asset and returns its size once decoded
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
    ConvertFromBigEndian32(*header);
#ifdef TARGET_PC
    asset->decodedSize = *header & 0xFFFFFF;
#endif
    return *header &= 0xFFFFFF;
}

// Decodes an asset that OpenEncodedAsset opened into buffer, which must have
// room for its decoded size. The decoders copy the ROM's bytes as they are,
// so what they write is in the ROM's order.
void ReadEncodedAsset(void *buffer, EncodedAsset *asset) {
    if (asset->method == ENCODING_BYTE_RUNS) {
        DecodeByteRuns(buffer, asset);
    }
    else {
        DecodeHalfwordRuns(buffer, asset);
    }

    MarkRomOrder(buffer, asset->decodedSize);
}

// Decodes runs that each start with a signed count byte: a positive
// count repeats the next byte that many times, a negative one copies
// that many bytes, and zero ends the asset
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

// Decodes runs as DecodeByteRuns does, but of two-byte units
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
