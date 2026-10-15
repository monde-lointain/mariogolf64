#include "common.h"
#include "rom_asset.h"

extern RomLoadSlot rom_load_slots[2];

extern void osSyncPrintf(const char*, ...);
extern const char ROM_SLOT_UNAVAILABLE_FORMAT[];
extern const char ROM_SLOT_ERROR_FORMAT[];

void reset_rom_load_slots(void) {
  s32 slot_index;
  RomLoadSlot* slot;

  slot_index = 0;
  slot = rom_load_slots;
  for (; slot_index != 2; slot_index++, slot++) {
    slot->read_address = 0;
  }
}

extern u8 rom_asset_directory[];
extern s32 current_rom_asset_index;

/* Read the asset directory's size and relative data offset through aligned
 * scratch buffers, then initialize a ROM read slot. */
void initialize_rom_load_slot(s32 asset_index, RomLoadSlot* slot) {
  u8 offset_storage[0x40];
  u8 size_storage[0x40];
  u32 directory_offset = asset_index * 8;
  u8* aligned_offset = (u8*)(((u32)&offset_storage[0xF]) & ~0xF);
  u8* aligned_size = (u8*)(((u32)&size_storage[0xF]) & ~0xF);
  u32 asset_size;

  nuPiReadRom((u32)(rom_asset_directory + directory_offset), aligned_size, 4);
  nuPiReadRom((u32)(rom_asset_directory + directory_offset + 4), aligned_offset,
              4);
  slot->start_address = (u32)(rom_asset_directory + *(u32*)aligned_offset);
  slot->read_address = slot->start_address;
  slot->end_address = slot->start_address + *(u32*)aligned_size;
  asset_size = *(u32*)aligned_size;
  current_rom_asset_index = asset_index;
  slot->size = asset_size;
}

void open_rom_asset_slot(s32 asset_index, RomLoadSlot* slot) {
  initialize_rom_load_slot(asset_index, slot);
}

RomLoadSlot* acquire_rom_load_slot(s32 asset_index) {
  s32 slot_index;
  RomLoadSlot* slot;

  slot_index = 0;
  slot = rom_load_slots;
  while (slot_index != 2) {
    slot_index++;
    if (slot->read_address == 0) {
      initialize_rom_load_slot(asset_index, slot);
      return slot;
    }
    slot++;
  }

  osSyncPrintf(ROM_SLOT_UNAVAILABLE_FORMAT, asset_index);
  osSyncPrintf(ROM_SLOT_ERROR_FORMAT, asset_index);
  return NULL;
}

void release_rom_load_slot(RomLoadSlot* slot) {
  slot->end_address = 0;
  slot->start_address = 0;
  slot->read_address = 0;
}

u32 get_rom_load_slot_size(RomLoadSlot* slot) { return slot->size; }
