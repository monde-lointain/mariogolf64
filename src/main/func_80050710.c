#include "common.h"
#include "rom_asset.h"

typedef struct {
  s32 pos;
  u32 remaining;
} StreamCursor;

/* Stream a compressed asset from ROM through a 0x2000-byte window. Positive
 * control bytes repeat the next byte; negative bytes copy -control literals.
 * A zero control byte ends the stream. */
void func_80050710(u8* destination, RomAssetLoad* asset_load) {
  u8 storage[0x2010];
  u8* buffer = (u8*)(((u32)storage + 0xF) & ~0xF);
  StreamCursor cursor;
  s32 code;
  s32 i;

  cursor.remaining = asset_load->stored_size - 4;
  if (cursor.remaining < 0x2000) {
    read_rom_load_slot(buffer, cursor.remaining, &asset_load->payload);
    cursor.remaining = 0;
  } else {
    read_rom_load_slot(buffer, 0x2000, &asset_load->payload);
    cursor.remaining -= 0x2000;
  }
  cursor.pos = 0;

  for (;;) {
    code = (s8)buffer[cursor.pos];
    cursor.pos++;
    if (cursor.remaining != 0) {
      if (cursor.pos == 0x2000) {
        if (cursor.remaining < 0x2000) {
          read_rom_load_slot(buffer, cursor.remaining, &asset_load->payload);
          cursor.remaining = 0;
        } else {
          read_rom_load_slot(buffer, 0x2000, &asset_load->payload);
          cursor.remaining -= 0x2000;
        }
        cursor.pos = 0;
      }
    }
    if (code == 0) {
      return;
    }
    if (code > 0) {
      for (i = 0; i != code; i++) {
        *destination++ = buffer[cursor.pos];
      }
      cursor.pos++;
      if (cursor.remaining != 0) {
        if (cursor.pos == 0x2000) {
          if (cursor.remaining < 0x2000) {
            read_rom_load_slot(buffer, cursor.remaining, &asset_load->payload);
            cursor.remaining = 0;
          } else {
            read_rom_load_slot(buffer, 0x2000, &asset_load->payload);
            cursor.remaining -= 0x2000;
          }
          cursor.pos = 0;
        }
      }
    } else {
      code = -code;
      for (i = 0; i != code; i++) {
        *destination++ = buffer[cursor.pos];
        cursor.pos++;
        if (cursor.remaining != 0) {
          if (cursor.pos == 0x2000) {
            if (cursor.remaining < 0x2000) {
              read_rom_load_slot(buffer, cursor.remaining, &asset_load->payload);
              cursor.remaining = 0;
            } else {
              read_rom_load_slot(buffer, 0x2000, &asset_load->payload);
              cursor.remaining -= 0x2000;
            }
            cursor.pos = 0;
          }
        }
      }
    }
  }
}

/* Stream a compressed asset from ROM through a 0x2000-byte window. Positive
 * control bytes repeat the next two-byte pair; negative bytes copy -control
 * two-byte pairs. A zero control byte ends the stream. */
void func_80050914(u8* destination, RomAssetLoad* asset_load) {
  u8 storage[0x2020];
  u8* buffer = (u8*)(((u32)storage + 0xF) & ~0xF);
  StreamCursor cursor;
  s32 code;
  s32 i;

  cursor.remaining = asset_load->stored_size - 4;
  if (cursor.remaining < 0x2000) {
    read_rom_load_slot(buffer, cursor.remaining, &asset_load->payload);
    cursor.remaining = 0;
  } else {
    read_rom_load_slot(buffer, 0x2000, &asset_load->payload);
    cursor.remaining -= 0x2000;
  }
  cursor.pos = 0;

  for (;;) {
    code = (s8)buffer[cursor.pos];
    cursor.pos++;
    if (cursor.remaining != 0) {
      if (cursor.pos == 0x2000) {
        if (cursor.remaining < 0x2000) {
          read_rom_load_slot(buffer, cursor.remaining, &asset_load->payload);
          cursor.remaining = 0;
        } else {
          read_rom_load_slot(buffer, 0x2000, &asset_load->payload);
          cursor.remaining -= 0x2000;
        }
        cursor.pos = 0;
      }
    }
    if (code == 0) {
      return;
    }
    if (code > 0) {
      u8 a;
      u8 b;
      a = buffer[cursor.pos];
      cursor.pos++;
      if (cursor.remaining != 0) {
        if (cursor.pos == 0x2000) {
          if (cursor.remaining < 0x2000) {
            read_rom_load_slot(buffer, cursor.remaining, &asset_load->payload);
            cursor.remaining = 0;
          } else {
            read_rom_load_slot(buffer, 0x2000, &asset_load->payload);
            cursor.remaining -= 0x2000;
          }
          cursor.pos = 0;
        }
      }
      b = buffer[cursor.pos];
      cursor.pos++;
      if (cursor.remaining != 0) {
        if (cursor.pos == 0x2000) {
          if (cursor.remaining < 0x2000) {
            read_rom_load_slot(buffer, cursor.remaining, &asset_load->payload);
            cursor.remaining = 0;
          } else {
            read_rom_load_slot(buffer, 0x2000, &asset_load->payload);
            cursor.remaining -= 0x2000;
          }
          cursor.pos = 0;
        }
      }
      for (i = 0; i != code; i++) {
        *destination++ = a;
        *destination++ = b;
      }
    } else {
      u8 a;
      u8 b;
      code = -code;
      for (i = 0; i != code; i++) {
        a = buffer[cursor.pos];
        cursor.pos++;
        if (cursor.remaining != 0) {
          if (cursor.pos == 0x2000) {
            if (cursor.remaining < 0x2000) {
              read_rom_load_slot(buffer, cursor.remaining, &asset_load->payload);
              cursor.remaining = 0;
            } else {
              read_rom_load_slot(buffer, 0x2000, &asset_load->payload);
              cursor.remaining -= 0x2000;
            }
            cursor.pos = 0;
          }
        }
        b = buffer[cursor.pos];
        cursor.pos++;
        if (cursor.remaining != 0) {
          if (cursor.pos == 0x2000) {
            if (cursor.remaining < 0x2000) {
              read_rom_load_slot(buffer, cursor.remaining, &asset_load->payload);
              cursor.remaining = 0;
            } else {
              read_rom_load_slot(buffer, 0x2000, &asset_load->payload);
              cursor.remaining -= 0x2000;
            }
            cursor.pos = 0;
          }
        }
        *destination++ = a;
        *destination++ = b;
      }
    }
  }
}
