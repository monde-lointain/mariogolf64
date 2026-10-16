#ifndef ROM_ASSET_H
#define ROM_ASSET_H

#include "common.h"

/* Cursor and bounds for one asset's stored ROM byte range. */
typedef struct RomLoadSlot {
  /* 0x00 */ u32 read_address;
  /* 0x04 */ u32 size;
  /* 0x08 */ u32 start_address;
  /* 0x0C */ u32 end_address;
} RomLoadSlot; /* 0x10 */

/* Decoding metadata followed by the live input cursor. The first two words are
 * not read or written by the recovered loader/decoder code. stored_size includes
 * the four-byte compression header already consumed from payload. */
typedef struct {
  /* 0x00 */ u32 unknown_00[2];
  /* 0x08 */ u32 compression_mode;
  /* 0x0C */ u32 stored_size;
  /* 0x10 */ RomLoadSlot payload;
} RomAssetLoad; /* 0x20 */

void open_rom_asset_slot(s32 asset_index, RomLoadSlot* slot);
void read_rom_load_slot(void* destination, u32 requested_size,
                        RomLoadSlot* slot);

u32 get_rom_asset_allocation_size(u16 asset_index, RomAssetLoad* asset);
void load_rom_asset_payload(void* destination, RomAssetLoad* asset);

#endif /* ROM_ASSET_H */
