#include "common.h"
#include "rom_asset.h"

extern s32 current_rom_asset_index;

extern const char ROM_READ_TRUNCATED_FORMAT[];
extern const char ROM_ASSET_OPEN_FORMAT[];

extern s32 flag_is_set(s32 flag);
extern void osSyncPrintf(const char*, ...);
extern void nuPiReadRom(u32 rom_addr, void* buf_ptr, u32 size);
extern void open_rom_asset_slot(s32 asset_index, RomLoadSlot* slot);
extern u32 get_rom_load_slot_size(RomLoadSlot* slot);
extern void func_80050710(void* data, RomAssetLoad* asset);
extern void func_80050914(void* data, RomAssetLoad* asset);

void read_rom_load_slot(void* destination, u32 requested_size,
                        RomLoadSlot* slot) {
  if (slot->end_address < slot->read_address + requested_size) {
    requested_size = slot->end_address - slot->read_address;
    osSyncPrintf(ROM_READ_TRUNCATED_FORMAT, current_rom_asset_index);
  }

  nuPiReadRom(slot->read_address, destination, (requested_size + 1) & ~1);
  slot->read_address += requested_size;
}

u32 get_rom_asset_allocation_size(u16 asset_index, RomAssetLoad* asset) {
  u8 scratch_storage[0x80];
  u8* aligned_header = (u8*)(((u32)scratch_storage + 0xF) & ~0xF);
  RomLoadSlot* payload_slot;
  u32 allocation_size;

  if (flag_is_set(0x49)) {
    osSyncPrintf(ROM_ASSET_OPEN_FORMAT, asset_index);
  }

  payload_slot = &asset->payload;
  open_rom_asset_slot(asset_index, payload_slot);
  asset->stored_size = get_rom_load_slot_size(payload_slot);
  read_rom_load_slot(aligned_header, 4, payload_slot);
  asset->compression_mode = aligned_header[0];

  allocation_size = *(u32*)aligned_header & 0xFFFFFF;
  *(u32*)aligned_header = allocation_size;
  return allocation_size;
}

void load_rom_asset_payload(void* destination, RomAssetLoad* asset) {
  if (asset->compression_mode == 1) {
    func_80050710(destination, asset);
  } else {
    func_80050914(destination, asset);
  }
}
