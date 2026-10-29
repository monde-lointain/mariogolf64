/*
 * Reads assets out of the asset archive. Files from a pool are opened on an
 * asset and read a part at a time, and encoded assets are decoded from runs of
 * bytes or of halfwords.
 */
#include "common.h"

#include "asset.h"
#include "functions.h"
#include "game_flags.h"

/* How many files OpenPooledAssetFile can keep open at once */
#define ASSET_FILE_POOL_SIZE 2

/* Encoded assets are read through a buffer of this many bytes */
#define ASSET_CHUNK_SIZE 0x2000

/* Rounds a stack buffer up to the 16-byte alignment that ROM reads need */
#define ALIGN_BUFFER(buffer) ((void*)(((u32)(buffer) + 0xF) & ~0xF))

/* Reads the next chunk of the asset into chunk */
#define READ_CHUNK(chunk, remaining, asset) \
  if ((remaining) < ASSET_CHUNK_SIZE) { \
    ReadAssetFile(chunk, remaining, &(asset)->file); \
    (remaining) = 0; \
  } else { \
    ReadAssetFile(chunk, ASSET_CHUNK_SIZE, &(asset)->file); \
    (remaining) -= ASSET_CHUNK_SIZE; \
  }
/* Moves past a byte of chunk, reading the next chunk at its end */
#define ADVANCE(chunk, position, remaining, asset) \
  (position)++; \
  if ((remaining) != 0 && (position) == ASSET_CHUNK_SIZE) { \
    READ_CHUNK(chunk, remaining, asset); \
    (position) = 0; \
  }

typedef struct AssetTableEntry {
  /* 0x0 */ s32 size;
  /* 0x4 */ u32 offset; /* From the archive's start */
} AssetTableEntry;

/* The archive in ROM starts with a table of each asset's AssetTableEntry */
#define ASSET_TABLE ((AssetTableEntry*)g_AssetArchiveRomStart)
static AssetFile f_AssetFilePool[ASSET_FILE_POOL_SIZE];
/* Read by no routine */
static u8 D_800DE8C0[0x40];

void DecodeByteRuns(u8* buffer, EncodedAsset* asset);
void DecodeHalfwordRuns(u8* buffer, EncodedAsset* asset);

/* Not static: the original left it to the linker's common block */
s32 f_LastOpenedAssetId;

void InitAssetFiles(void) {
  s32 index;
  AssetFile* file;

  for (index = 0, file = f_AssetFilePool; index != ASSET_FILE_POOL_SIZE;
      index++) {
    file->cursor = 0;
    file++;
  }
}

/* Points file at the asset's bytes in the archive */
void ReadAssetFileBounds(s32 assetId, AssetFile* file) {
  u8 rawOffset[0x40];
  u8 rawSize[0x40];
  u32* offset = ALIGN_BUFFER(rawOffset);
  s32* size = ALIGN_BUFFER(rawSize);

  nuPiReadRom((u32)&ASSET_TABLE[assetId].size, size, sizeof(*size));
  nuPiReadRom((u32)&ASSET_TABLE[assetId].offset, offset, sizeof(*offset));
  file->romStart = (u32)g_AssetArchiveRomStart + *offset;
  file->cursor = file->romStart;
  file->romEnd = file->romStart + *size;
  file->size = *size;
  f_LastOpenedAssetId = assetId;
}

void OpenAssetFile(s32 assetId, AssetFile* file) {
  ReadAssetFileBounds(assetId, file);
}

/* Opens the asset in a free file of the pool, or returns NULL if none is free
 */
AssetFile* OpenPooledAssetFile(s32 assetId) {
  s32 index;
  AssetFile* file;

  for (index = 0, file = f_AssetFilePool; index != ASSET_FILE_POOL_SIZE;
      index++) {
    if (file->cursor == 0) {
      ReadAssetFileBounds(assetId, file);
      return file;
    }

    file++;
  }

  osSyncPrintf("ropen %d\n", assetId);

  /* "File number %d failed to open." */
  osSyncPrintf("%d番のファイルオープン失敗。\n", assetId);
  return NULL;
}

/* Frees the file for OpenPooledAssetFile */
void CloseAssetFile(AssetFile* file) {
  file->romEnd = 0;
  file->romStart = 0;
  file->cursor = 0;
}

s32 GetAssetFileSize(AssetFile* file) {
  return file->size;
}

/* Reads the next size bytes of the file, or what remains of it if fewer */
void ReadAssetFile(void* buffer, u32 size, AssetFile* file) {
  if (file->romEnd < file->cursor + size) {
    size = file->romEnd - file->cursor;
    osSyncPrintf("size error %d \n", f_LastOpenedAssetId);
  }

  nuPiReadRom(file->cursor, buffer, (size + 1) & ~1);
  file->cursor += size;
}

s32 OpenEncodedAsset(AssetId assetId, EncodedAsset* asset) {
  u8 rawHeader[0x80];
  u32* header = ALIGN_BUFFER(rawHeader);

  if (IsGameFlagSet(GAME_FLAG_LOG_ENCODED_ASSETS)) {
    osSyncPrintf("OP:%d\n", assetId);
  }

  OpenAssetFile(assetId, &asset->file);
  asset->encodedSize = GetAssetFileSize(&asset->file);
  ReadAssetFile(header, sizeof(*header), &asset->file);
  asset->method = *(u8*)header;
  return *header &= 0xFFFFFF;
}

void ReadEncodedAsset(void* buffer, EncodedAsset* asset) {
  if (asset->method == ENCODING_BYTE_RUNS) {
    DecodeByteRuns(buffer, asset);
  } else {
    DecodeHalfwordRuns(buffer, asset);
  }
}

/* An encoded asset's bytes, read a chunk at a time */
typedef struct ChunkReader {
  /* 0x0000 */ u8 rawChunk[ASSET_CHUNK_SIZE + 0x10]; /* Aligned as chunk */
  /* 0x2010 */ s32 position; /* In chunk */
  /* 0x2014 */ u32 remaining; /* Unread bytes of the asset */
} ChunkReader;

/* DecodeHalfwordRuns' buffer is 0x10 bytes larger, to match */
typedef struct LargeChunkReader {
  /* 0x0000 */ u8 rawChunk[ASSET_CHUNK_SIZE + 0x20];
  /* 0x2020 */ s32 position;
  /* 0x2024 */ u32 remaining;
} LargeChunkReader;

/*
 * Decodes runs that each start with a signed count byte: a positive
 * count repeats the next byte that many times, a negative one copies
 * that many bytes, and zero ends the asset
 */
void DecodeByteRuns(u8* buffer, EncodedAsset* asset) {
  ChunkReader reader;
  u8* chunk;
  s32 count;
  s32 index;

  reader.remaining = asset->encodedSize - sizeof(u32);
  chunk = ALIGN_BUFFER(reader.rawChunk);
  READ_CHUNK(chunk, reader.remaining, asset);
  reader.position = 0;
  while (TRUE) {
    count = (s8)chunk[reader.position];
    ADVANCE(chunk, reader.position, reader.remaining, asset);
    if (count == 0) {
      break;
    }

    if (count > 0) {
      for (index = 0; index != count; index++) {
        *buffer++ = chunk[reader.position];
      }

      ADVANCE(chunk, reader.position, reader.remaining, asset);
    } else {
      count = -count;
      for (index = 0; index != count; index++) {
        *buffer++ = chunk[reader.position];
        ADVANCE(chunk, reader.position, reader.remaining, asset);
      }
    }
  }
}

/* Decodes runs as DecodeByteRuns does, but of two-byte units */
void DecodeHalfwordRuns(u8* buffer, EncodedAsset* asset) {
  LargeChunkReader reader;
  u8* chunk;
  s32 count;
  s32 index;
  s32 repeatCount;
  u8 high;
  u8 low;

  reader.remaining = asset->encodedSize - sizeof(u32);
  chunk = ALIGN_BUFFER(reader.rawChunk);
  READ_CHUNK(chunk, reader.remaining, asset);
  reader.position = 0;
  while (TRUE) {
    count = (s8)chunk[reader.position];
    ADVANCE(chunk, reader.position, reader.remaining, asset);
    if (count == 0) {
      break;
    }

    if (count > 0) {
      high = chunk[reader.position];
      ADVANCE(chunk, reader.position, reader.remaining, asset);
      low = chunk[reader.position];
      repeatCount = count; /* Copied here, rather than count used, to match */
      ADVANCE(chunk, reader.position, reader.remaining, asset);
      for (index = 0; index != repeatCount; index++) {
        *buffer++ = high;
        *buffer++ = low;
      }
    } else {
      count = -count;
      for (index = 0; index != count; index++) {
        high = chunk[reader.position];
        ADVANCE(chunk, reader.position, reader.remaining, asset);
        low = chunk[reader.position];
        ADVANCE(chunk, reader.position, reader.remaining, asset);
        *buffer++ = high;
        *buffer++ = low;
      }
    }
  }
}
