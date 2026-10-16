#include "common.h"
#include "rom_asset.h"

extern s32 current_rom_asset_index;

extern const char ROM_READ_TRUNCATED_FORMAT[];
extern const char ROM_ASSET_OPEN_FORMAT[];

extern s32 flag_is_set(s32 flag_id);
extern void osSyncPrintf(const char* format, ...);
extern void nuPiReadRom(u32 rom_address, void* destination, u32 byte_count);
extern void open_rom_asset_slot(s32 asset_index, RomLoadSlot* load_slot);
extern u32 get_rom_load_slot_size(RomLoadSlot* load_slot);
extern void func_80050710(void* destination, RomAssetLoad* asset_load);
extern void func_80050914(void* destination, RomAssetLoad* asset_load);

void read_rom_load_slot(void* destination, u32 requested_size,
                        RomLoadSlot* load_slot) {
  /* Retain the adjusted count in the parameter to preserve KMC register allocation. */
  if (load_slot->end_address < load_slot->read_address + requested_size) {
    requested_size = load_slot->end_address - load_slot->read_address;
    osSyncPrintf(ROM_READ_TRUNCATED_FORMAT, current_rom_asset_index);
  }

  nuPiReadRom(load_slot->read_address, destination, (requested_size + 1) & ~1);
  load_slot->read_address += requested_size;
}

u32 get_rom_asset_allocation_size(u16 asset_index, RomAssetLoad* asset_load) {
  u8 scratch_storage[0x80];
  u8* aligned_header = (u8*)(((u32)scratch_storage + 0xF) & ~0xF);
  RomLoadSlot* payload_slot;
  u32 allocation_size;

  if (flag_is_set(0x49)) {
    osSyncPrintf(ROM_ASSET_OPEN_FORMAT, asset_index);
  }

  payload_slot = &asset_load->payload;
  open_rom_asset_slot(asset_index, payload_slot);
  asset_load->stored_size = get_rom_load_slot_size(payload_slot);
  read_rom_load_slot(aligned_header, 4, payload_slot);
  asset_load->compression_mode = aligned_header[0];

  allocation_size = *(u32*)aligned_header & 0xFFFFFF;
  *(u32*)aligned_header = allocation_size;
  return allocation_size;
}

void load_rom_asset_payload(void* destination, RomAssetLoad* asset_load) {
  if (asset_load->compression_mode == 1) {
    func_80050710(destination, asset_load);
  } else {
    func_80050914(destination, asset_load);
  }
}
