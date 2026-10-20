#ifndef OVERLAY_0_H
#define OVERLAY_0_H

#include "common.h"

/* Heap block D_ovl0_8022D650 (0x6270 bytes). Game modes reuse the same bytes
 * with different layouts, so each mode gets its own view. Fields are named by
 * their offset from the block start. Payload sizes that code never states are
 * sized up to the next named offset. */
typedef union Overlay0Heap {
    /* event 0 load: slot at 0x000, assets at 0x200 and 0x2880 */
    struct {
        /* 0x000 */ u8 slot_000[0x200];
        /* 0x200 */ u8 asset_200[0x2680];
        /* 0x2880 */ u8 asset_2880[0x39F0];
    } event0;
    /* event 1 load: asset at 0x000, asset at 0x1008 */
    struct {
        /* 0x000 */ u8 asset_000[0x1008];
        /* 0x1008 */ u8 asset_1008[0x5268];
    } event1;
    /* event 15 load: asset at 0x000, asset at 0x2508 */
    struct {
        /* 0x000 */ u8 asset_000[0x2508];
        /* 0x2508 */ u8 asset_2508[0x3D68];
    } event15;
    /* events 2, 3, 4, 5, 16 load: slot at 0x4208, asset at 0x4408 */
    struct {
        /* 0x000 */ u8 base_000[0x4208];
        /* 0x4208 */ u8 slot_4208[0x200];
        /* 0x4408 */ u8 asset_4408[0x1E68];
    } event_slot4208;
    /* setup load: asset at 0x000, asset at 0x808 */
    struct {
        /* 0x000 */ u8 base_000[0x808];
        /* 0x808 */ u8 asset_808[0x5A68];
    } setup;
    /* render bank A (D_ovl0_80224408 < 0x18): palette at 0x008, five 160-wide CI8 tiles at 0x208 */
    struct {
        /* 0x000 */ u8 unk_000[0x8];
        /* 0x008 */ u16 palette_008[0x100];
        /* 0x208 */ u8 tiles_208[5][0x780];
    } render_a;
    /* render bank B (D_ovl0_80224408 >= 0x18): palette at 0x2510, five 144-wide CI8 tiles at 0x2710 */
    struct {
        /* 0x000 */ u8 unk_000[0x2510];
        /* 0x2510 */ u16 palette_2510[0x100];
        /* 0x2710 */ u8 tiles_2710[5][0x7E0];
    } render_b;
} Overlay0Heap;

extern Overlay0Heap* D_ovl0_8022D650;

#endif
