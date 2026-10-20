#include "common.h"

#define OVERLAY_COUNT 0x11
#define SEGMENT_COUNT 0x40
#define OVERLAY_ID_BASE 0x100
#define OVERLAY_INDEX_MASK 0xFF
#define SEGMENT_LIST_END -1
#define SEGMENT_UNUSED_FRAME 0x80000000
#define SEGMENT_REUSE_DELAY 2
#define FLAG_SUPPRESS_OVERLAY_MESSAGES 0x4D
#define BASE_RDRAM_LAST_ADDRESS 0x3FFFFF

/* Overlay IDs encode a descriptor index in their low byte. Segment lists are
 * sentinel-terminated; ownership and RDP-use timestamps are tracked separately. */
typedef struct OverlayDescriptor {
  /* 0x00 */ s8 is_loaded;
  /* 0x04 */ s8* segment_ids;
  /* 0x08 */ u32 rom_start;
  /* 0x0C */ u32 rom_end;
  /* 0x10 */ u32 ram_start;
  /* 0x14 */ u32 ram_end;
  /* 0x18 */ u32 bss_start;
  /* 0x1C */ u32 bss_end;
  /* 0x20 */ void (*initialize)(void);
  /* 0x24 */ void (*finalize)(void);
} OverlayDescriptor; /* 0x28 */

extern OverlayDescriptor g_overlay_descriptors[];
extern s8 g_segment_overlay_indices[];
extern u8 g_segment_is_loaded[];
extern s8 g_debug_mode_enabled;
extern u32 g_frame_counter;
extern u32 g_segment_last_used_frame[];

extern const char OVERLAY_INVALID_ID_MESSAGE[];
extern const char OVERLAY_ALREADY_LOADED_MESSAGE[];
extern const char OVERLAY_MEMORY_CONFLICT_MESSAGE[];
extern const char OVERLAY_RDP_CONFLICT_MESSAGE[];
extern const char OVERLAY_LOAD_START_MESSAGE[];
extern const char OVERLAY_LOAD_COMPLETE_MESSAGE[];
extern const char OVERLAY_LOAD_FAILED_MESSAGE[];
extern const char OVERLAY_UNLOADED_MESSAGE[];
extern const char OVERLAY_INACTIVE_MODULE_MESSAGE[];

extern s32 flag_is_set(s32 flag);
extern void bzero(void* address, s32 length);
extern void nuPiReadRom(u32 rom_address, void* destination, u32 size);
extern void func_80080DCC(void); /* Initialize the shared scene asset heap. */
extern void func_801FD624(void); /* Initialize overlay 0/1/2 resources. */
extern void func_802033B0(void); /* Initialize overlay 7 scene state. */
extern void func_801F5018(void); /* Initialize overlay 7 resources. */

/* Descriptor 2's initialization hook; descriptors 0 and 1 share its ROM. */
void init_scene_overlay_assets(void) {
  func_80080DCC();
  func_801FD624();
}

/* Descriptor 7's initialization hook. */
void init_scene_overlay_state_and_assets(void) {
  func_802033B0();
  func_801F5018();
}

s32 overlay_id_from_segment(s32 segment_id) {
  return g_segment_overlay_indices[segment_id] | OVERLAY_ID_BASE;
}

void init_overlay_manager(void) {
  /* Sharing the traversal index keeps KMC's retail register allocation. */
  s32 traversal_index;

  for (traversal_index = 0; traversal_index < SEGMENT_COUNT;
       traversal_index++) {
    g_segment_overlay_indices[traversal_index] = SEGMENT_LIST_END;
    g_segment_is_loaded[traversal_index] = 0;
    g_segment_last_used_frame[traversal_index] = SEGMENT_UNUSED_FRAME;
  }

  for (traversal_index = 0; (u32)traversal_index < OVERLAY_COUNT;
       traversal_index++) {
    s8* segment_id_list = g_overlay_descriptors[traversal_index].segment_ids;
    u32 segment_index;

    segment_index = 0;
    while (segment_id_list[segment_index] != SEGMENT_LIST_END) {
      if (g_segment_overlay_indices[segment_id_list[segment_index]] !=
              SEGMENT_LIST_END &&
          !flag_is_set(FLAG_SUPPRESS_OVERLAY_MESSAGES)) {
        /* This diagnostic starts at the microcode data's end address. */
        osSyncPrintf((const char*)gspF3DEX2_fifoDataEnd);
      }
      g_segment_overlay_indices[segment_id_list[segment_index]] =
          traversal_index;
      segment_index++;
    }
  }

  for (traversal_index = 0; traversal_index < SEGMENT_COUNT;
       traversal_index++) {
    if (g_segment_overlay_indices[traversal_index] == SEGMENT_LIST_END) {
      g_segment_is_loaded[traversal_index] = 1;
    }
  }
}

/* Retail behavior maps invalid IDs to descriptor zero, not an error index. */
s32 overlay_index_from_id(s32 overlay_id) {
  if (overlay_id < OVERLAY_ID_BASE ||
      (u32)overlay_id >= OVERLAY_ID_BASE + OVERLAY_COUNT) {
    if (!flag_is_set(FLAG_SUPPRESS_OVERLAY_MESSAGES)) {
      osSyncPrintf(OVERLAY_INVALID_ID_MESSAGE);
    }
    return 0;
  }
  return overlay_id & OVERLAY_INDEX_MASK;
}

s32 can_load_overlay(s32 overlay_id) {
  s32 overlay_index = overlay_index_from_id(overlay_id);
  s32 other_overlay_index;

  if (g_overlay_descriptors[overlay_index].is_loaded != 0) {
    if (!flag_is_set(FLAG_SUPPRESS_OVERLAY_MESSAGES)) {
      osSyncPrintf(OVERLAY_ALREADY_LOADED_MESSAGE);
    }
    return 1;
  }

  for (other_overlay_index = 0;
       (u32)other_overlay_index < OVERLAY_COUNT; other_overlay_index++) {
    if (other_overlay_index == overlay_index ||
        g_overlay_descriptors[other_overlay_index].ram_start >
            g_overlay_descriptors[overlay_index].bss_end ||
        g_overlay_descriptors[other_overlay_index].bss_end <
            g_overlay_descriptors[overlay_index].ram_start) {
      continue;
    }
    if (g_overlay_descriptors[other_overlay_index].is_loaded != 0) {
      if (!flag_is_set(FLAG_SUPPRESS_OVERLAY_MESSAGES)) {
        osSyncPrintf(OVERLAY_MEMORY_CONFLICT_MESSAGE, overlay_index,
                     other_overlay_index);
      }
      return 0;
    }
    {
      s8* segment_id_list =
          g_overlay_descriptors[other_overlay_index].segment_ids;
      u32 segment_index;

      segment_index = 0;
      while (segment_id_list[segment_index] != SEGMENT_LIST_END) {
        s8 segment_id = segment_id_list[segment_index];
        if (g_frame_counter - g_segment_last_used_frame[segment_id] <
            SEGMENT_REUSE_DELAY) {
          if (!flag_is_set(FLAG_SUPPRESS_OVERLAY_MESSAGES)) {
            osSyncPrintf(OVERLAY_RDP_CONFLICT_MESSAGE, overlay_index,
                         other_overlay_index, segment_index);
          }
          return 0;
        }
        segment_index++;
      }
    }
  }
  return 1;
}

s32 load_overlay(s32 overlay_id) {
  if (can_load_overlay(overlay_id) != 0) {
    OverlayDescriptor* descriptor;
    s8* segment_id_list;
    u32 segment_index;

    /* Reuse the argument for the decoded index to preserve KMC codegen. */
    overlay_id = overlay_index_from_id(overlay_id);
    if (!flag_is_set(FLAG_SUPPRESS_OVERLAY_MESSAGES)) {
      osSyncPrintf(OVERLAY_LOAD_START_MESSAGE, overlay_id);
    }

    descriptor = &g_overlay_descriptors[overlay_id];
    segment_id_list = descriptor->segment_ids;
    /* Publish ROM/BSS changes to the caches before running overlay code. */
    osWritebackDCacheAll();
    nuPiReadRom(descriptor->rom_start, (void*)descriptor->ram_start,
                descriptor->rom_end - descriptor->rom_start);
    bzero((void*)descriptor->bss_start,
          descriptor->bss_end - descriptor->bss_start);
    osWritebackDCache((void*)descriptor->bss_start,
                      descriptor->bss_end - descriptor->bss_start);
    osInvalICache((void*)descriptor->ram_start,
                  descriptor->ram_end - descriptor->ram_start);
    if (descriptor->initialize != NULL) {
      descriptor->initialize();
    }
    segment_index = 0;
    while (segment_id_list[segment_index] != SEGMENT_LIST_END) {
      s8 segment_id = segment_id_list[segment_index];
      g_segment_is_loaded[segment_id] = 1;
      segment_index++;
    }
    descriptor->is_loaded = 1;
    if (!flag_is_set(FLAG_SUPPRESS_OVERLAY_MESSAGES)) {
      osSyncPrintf(OVERLAY_LOAD_COMPLETE_MESSAGE, overlay_id);
    }
    return 1;
  }

  if (!flag_is_set(FLAG_SUPPRESS_OVERLAY_MESSAGES)) {
    osSyncPrintf(OVERLAY_LOAD_FAILED_MESSAGE, overlay_id);
  }
  return 0;
}

void unload_overlay(s32 overlay_id) {
  OverlayDescriptor* descriptor;
  s8* segment_id_list;
  u32 segment_index;

  /* A separate decoded-index local adds a register move and delay-slot nop. */
  overlay_id = overlay_index_from_id(overlay_id);
  descriptor = &g_overlay_descriptors[overlay_id];
  if (descriptor->is_loaded == 0) {
    return;
  }
  if (descriptor->finalize != NULL) {
    descriptor->finalize();
  }
  segment_id_list = descriptor->segment_ids;
  descriptor->is_loaded = 0;
  if (!flag_is_set(FLAG_SUPPRESS_OVERLAY_MESSAGES)) {
    osSyncPrintf(OVERLAY_UNLOADED_MESSAGE, overlay_id);
  }
  segment_index = 0;
  while (segment_id_list[segment_index] != SEGMENT_LIST_END) {
    s8 segment_id = segment_id_list[segment_index];
    g_segment_is_loaded[segment_id] = 0;
    segment_index++;
  }
  osWritebackDCacheAll();
  if (g_debug_mode_enabled != 0) {
    if (osVirtualToPhysical((void*)descriptor->ram_start) <=
        BASE_RDRAM_LAST_ADDRESS) {
      bzero((void*)descriptor->ram_start,
            descriptor->ram_end - descriptor->ram_start);
      osWritebackDCache((void*)descriptor->ram_start,
                        descriptor->ram_end - descriptor->ram_start);
      osInvalICache((void*)descriptor->ram_start,
                    descriptor->ram_end - descriptor->ram_start);
    }
  }
}

s32 report_inactive_overlay_module(s32 module_id) {
  /* Preserve the retail diagnostic's duplicate calls and missing %d argument. */
  if (!flag_is_set(FLAG_SUPPRESS_OVERLAY_MESSAGES)) {
    osSyncPrintf(OVERLAY_INACTIVE_MODULE_MESSAGE);
  }
  if (!flag_is_set(FLAG_SUPPRESS_OVERLAY_MESSAGES)) {
    osSyncPrintf(OVERLAY_INACTIVE_MODULE_MESSAGE);
  }
  return 0;
}
