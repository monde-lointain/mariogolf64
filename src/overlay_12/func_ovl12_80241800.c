#include "common.h"

/* Per-player buffer pair at 0x58 + 8 * player index. */
typedef struct {
    /* 0x00 */ void* unknown_00;
    /* 0x04 */ void* unknown_04;
} Ovl12PlayerBuffers; /* 0x08 */

/* 0x78-byte block allocated by func_ovl12_80241800 and held in D_ovl12_802450C0. */
typedef struct {
    /* 0x00 */ u8 pad00[0x40];
    /* 0x40 */ void* unknown_40;
    /* 0x44 */ void* unknown_44;
    /* 0x48 */ void* unknown_48;
    /* 0x4C */ void* unknown_4C;
    /* 0x50 */ void* unknown_50;
    /* 0x54 */ void* unknown_54;
    /* 0x58 */ Ovl12PlayerBuffers players[4];
} Ovl12Buffers; /* 0x78 */

extern s32 D_ovl12_80244E10;
extern Ovl12Buffers* D_ovl12_802450C0;
extern s32 active_player_count;

extern void func_800934CC(s32 arg0);
extern void set_color_framebuffer_count(u32 buffer_count);
extern void func_8002A97C(s32 sky_mode, s32 blend_countdown);
extern void heap3_free(void** payload_ptr);

INCLUDE_ASM("asm/nonmatchings/overlay_12/func_ovl12_80241800", func_ovl12_80241800);

void func_ovl12_80241EE8(void) {
    s32 i;

    func_800934CC(-1);
    set_color_framebuffer_count(3);
    func_8002A97C(4, 0);
    heap3_free(&D_ovl12_802450C0->unknown_54);
    heap3_free(&D_ovl12_802450C0->unknown_48);
    heap3_free(&D_ovl12_802450C0->unknown_4C);
    heap3_free(&D_ovl12_802450C0->unknown_50);
    heap3_free(&D_ovl12_802450C0->unknown_40);
    heap3_free(&D_ovl12_802450C0->unknown_44);
    i = 0;
    if (active_player_count != 0) {
        for (; i != active_player_count; i++) {
            heap3_free(&D_ovl12_802450C0->players[i].unknown_00);
            heap3_free(&D_ovl12_802450C0->players[i].unknown_04);
        }
    }
    heap3_free((void**)&D_ovl12_802450C0);
}

INCLUDE_ASM("asm/nonmatchings/overlay_12/func_ovl12_80241800", func_ovl12_80241FE0);

INCLUDE_ASM("asm/nonmatchings/overlay_12/func_ovl12_80241800", func_ovl12_8024216C);

s32 func_ovl12_80242418(void) {
    return D_ovl12_80244E10;
}

INCLUDE_ASM("asm/nonmatchings/overlay_12/func_ovl12_80241800", func_ovl12_80242424);

INCLUDE_ASM("asm/nonmatchings/overlay_12/func_ovl12_80241800", func_ovl12_80243E24);

void func_ovl12_80244E04(void) {}
