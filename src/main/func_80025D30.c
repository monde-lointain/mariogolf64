#include "common.h"

/* Main-segment overlay loader. Each overlay is described by a 0x28-byte entry
 * in the D_800B5F58[] array (indexed by a normalized overlay id, 0x00..0x10).
 * load_overlay DMAs the overlay's text from ROM, zeroes its BSS, flushes the
 * caches, runs its init hook, and marks the segments it claims; unload_overlay
 * reverses that. func_80025F18 rejects a load that would overlap a resident
 * overlay's memory or reuse a live segment slot. */
typedef struct OverlayDesc {
  /* 0x00 */ s8 loaded;
  /* 0x04 */ s8* segments; /* segment-id list, -1 terminated */
  /* 0x08 */ u32 romStart;
  /* 0x0C */ u32 romEnd;
  /* 0x10 */ u32 ramStart;
  /* 0x14 */ u32 ramEnd;
  /* 0x18 */ u32 bssStart;
  /* 0x1C */ u32 bssEnd;
  /* 0x20 */ void (*initFunc)(void);
  /* 0x24 */ void (*finiFunc)(void);
} OverlayDesc; /* 0x28 */

extern OverlayDesc D_800B5F58[];
extern s8 main_bss_VRAM[]; /* segment-id -> owning overlay index, or -1 */
extern u8 D_800FC858[];    /* segment-id -> in-use flag */
extern s8 D_800B67C0;
extern u32 D_800B67F0;
extern u32 D_8012CFC0[];

extern const char D_800C9F70[];
extern const char D_800C9F88[];
extern const char D_800C9F98[];
extern const char D_800C9FD0[];
extern const char D_800CA00C[];
extern const char D_800CA030[];
extern const char D_800CA05C[];
extern const char D_800CA084[];
extern const char D_800CA09C[];

extern s32 flag_is_set(s32 flag);
extern void bzero(void* addr, s32 len);
extern void nuPiReadRom(u32 rom_addr, void* buf_ptr, u32 size);
extern void func_80080DCC(void);
extern void func_801FD624(void);
extern void func_802033B0(void);
extern void func_801F5018(void);

void func_80025D30(void) {
  func_80080DCC();
  func_801FD624();
}

void func_80025D54(void) {
  func_802033B0();
  func_801F5018();
}

s32 func_80025D78(s32 arg0) { return main_bss_VRAM[arg0] | 0x100; }

void func_80025D8C(void) {
  s32 i;
  s8* seg;

  for (i = 0; i < 0x40; i++) {
    main_bss_VRAM[i] = -1;
    D_800FC858[i] = 0;
    D_8012CFC0[i] = 0x80000000;
  }

  for (i = 0; (u32)i < 0x11; i++) {
    if (*D_800B5F58[i].segments != -1) {
      seg = D_800B5F58[i].segments;
      do {
        if (main_bss_VRAM[*seg] != -1 && !flag_is_set(0x4D)) {
          osSyncPrintf((const char*)gspF3DEX2_fifoDataEnd);
        }
        main_bss_VRAM[*seg] = i;
        seg++;
      } while (*seg != -1);
    }
  }

  for (i = 0; i < 0x40; i++) {
    if (main_bss_VRAM[i] == -1) {
      D_800FC858[i] = 1;
    }
  }
}

s32 func_80025EC8(s32 overlayId) {
  if (overlayId < 0x100 || (u32)overlayId >= 0x111) {
    if (!flag_is_set(0x4D)) {
      osSyncPrintf(D_800C9F70);
    }
    return 0;
  }
  return overlayId & 0xFF;
}

s32 func_80025F18(s32 overlayId) {
  s32 k;
  s8* seg;
  u32 i;
  s8 segId;

  overlayId = func_80025EC8(overlayId);
  if (D_800B5F58[overlayId].loaded != 0) {
    if (!flag_is_set(0x4D)) {
      osSyncPrintf(D_800C9F88);
    }
    return 1;
  }

  for (k = 0; (u32)k < 0x11; k++) {
    if (k == overlayId ||
        D_800B5F58[k].ramStart > D_800B5F58[overlayId].bssEnd ||
        D_800B5F58[k].bssEnd < D_800B5F58[overlayId].ramStart) {
      continue;
    }
    if (D_800B5F58[k].loaded != 0) {
      if (!flag_is_set(0x4D)) {
        osSyncPrintf(D_800C9F98, overlayId, k);
      }
      return 0;
    }
    seg = D_800B5F58[k].segments;
    for (i = 0; seg[i] != -1; i++) {
      segId = seg[i];
      if (D_800B67F0 - D_8012CFC0[segId] < 2) {
        if (!flag_is_set(0x4D)) {
          osSyncPrintf(D_800C9FD0, overlayId, k, i);
        }
        return 0;
      }
    }
  }
  return 1;
}

s32 load_overlay(s32 overlayId) {
  OverlayDesc* desc;
  s8* seg;

  if (func_80025F18(overlayId) != 0) {
    overlayId = func_80025EC8(overlayId);
    if (!flag_is_set(0x4D)) {
      osSyncPrintf(D_800CA00C, overlayId);
    }

    desc = &D_800B5F58[overlayId];
    seg = desc->segments;
    osWritebackDCacheAll();
    nuPiReadRom(desc->romStart, (void*)desc->ramStart,
                desc->romEnd - desc->romStart);
    bzero((void*)desc->bssStart, desc->bssEnd - desc->bssStart);
    osWritebackDCache((void*)desc->bssStart, desc->bssEnd - desc->bssStart);
    osInvalICache((void*)desc->ramStart, desc->ramEnd - desc->ramStart);
    if (desc->initFunc != NULL) {
      desc->initFunc();
    }
    if (*seg != -1) {
      u32 i;
      s8 segId;
      for (i = 0; seg[i] != -1; i++) {
        segId = seg[i];
        D_800FC858[segId] = 1;
      }
    }
    desc->loaded = 1;
    if (!flag_is_set(0x4D)) {
      osSyncPrintf(D_800CA030, overlayId);
    }
    return 1;
  }

  if (!flag_is_set(0x4D)) {
    osSyncPrintf(D_800CA05C, overlayId);
  }
  return 0;
}

void unload_overlay(s32 overlayId) {
  OverlayDesc* desc;
  s8* seg;

  overlayId = func_80025EC8(overlayId);
  desc = &D_800B5F58[overlayId];
  if (desc->loaded == 0) {
    return;
  }
  if (desc->finiFunc != NULL) {
    desc->finiFunc();
  }
  seg = desc->segments;
  desc->loaded = 0;
  if (!flag_is_set(0x4D)) {
    osSyncPrintf(D_800CA084, overlayId);
  }
  if (*seg != -1) {
    u32 i;
    s8 segId;
    for (i = 0; seg[i] != -1; i++) {
      segId = seg[i];
      D_800FC858[segId] = 0;
    }
  }
  osWritebackDCacheAll();
  if (D_800B67C0 != 0) {
    if (osVirtualToPhysical((void*)desc->ramStart) <= 0x3FFFFF) {
      bzero((void*)desc->ramStart, desc->ramEnd - desc->ramStart);
      osWritebackDCache((void*)desc->ramStart, desc->ramEnd - desc->ramStart);
      osInvalICache((void*)desc->ramStart, desc->ramEnd - desc->ramStart);
    }
  }
}

s32 func_80026358(void) {
  if (!flag_is_set(0x4D)) {
    osSyncPrintf(D_800CA09C);
  }
  if (!flag_is_set(0x4D)) {
    osSyncPrintf(D_800CA09C);
  }
  return 0;
}
