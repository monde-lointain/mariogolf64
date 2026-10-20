#include "common.h"
#include "overlay_11.h"

extern s32 clear_animation_slot(s32 index);
extern void func_80050DA0(s32 sfx, s32 channel, s32 volume, s32 reverb, s32 pan);
extern s32 D_ovl11_8024C4A4;
extern s32 game_mode;
extern s32 D_800C4140;
extern s32 D_800C0E60;
void func_ovl11_80242FC0(s32 arg0);
void func_ovl11_80243050(void);
void func_ovl11_802433B4(u16 arg0);
void func_ovl11_802434A0(u16 arg0);

typedef struct SlotTimerState {
    /* 0x00 */ u8 unk_00[8];
    /* 0x08 */ f32 timer[10];
    /* 0x30 */ s8 active[10];
    /* 0x3A */ u8 unk_3A[2];
    /* 0x3C */ s32 frame_count;
} SlotTimerState;

extern SlotTimerState D_801050BC;
extern void play_sound_effect(s32 sfx, s32 channel, s32 volume);
extern s8 D_801061B8;
extern const f64 D_ovl11_8024C6F8;
extern const f64 D_ovl11_8024C700;
extern void func_8006A548(Gfx** pgfx, u32 tlut);
extern void load_texture_block(Gfx** pgfx, u32 timg, s32 fmt, s32 siz,
                               s32 width, s32 height, s32 pal, s32 cms,
                               s32 cmt, s32 masks, s32 maskt, s32 shifts,
                               s32 shiftt);
extern void func_8006A2C0(Gfx** pgfx, s32 ulx, s32 uly, s32 lrx, s32 lry,
                          s32 tile, s32 s, s32 t, s32 dsdx, s32 dtdy);

INCLUDE_ASM("asm/nonmatchings/overlay_11/func_ovl11_80241800", func_ovl11_80241800);

INCLUDE_ASM("asm/nonmatchings/overlay_11/func_ovl11_80241800", func_ovl11_80242074);

INCLUDE_ASM("asm/nonmatchings/overlay_11/func_ovl11_80241800", func_ovl11_80242334);

INCLUDE_ASM("asm/nonmatchings/overlay_11/func_ovl11_80241800", func_ovl11_80242B74);

void func_ovl11_80242FC0(s32 arg0) {
    if (arg0 == 0) {
        func_80050DA0(0x47, D_800C0E60, 0x64, 0x14, 0xDF);
    } else if (arg0 == 1) {
        func_80050DA0(0x48, D_800C0E60, 0x64, 0x14, 0x20);
    }
    if (arg0 == 0) {
        D_80104FD0.unkC8 = 0x140;
        D_80104FD0.unkCC = 0;
    }
    D_800C4140 = arg0;
    D_80104FD0.unkD4 = 0;
}

INCLUDE_ASM("asm/nonmatchings/overlay_11/func_ovl11_80241800", func_ovl11_80243050);

INCLUDE_ASM("asm/nonmatchings/overlay_11/func_ovl11_80241800", func_ovl11_802433B4);

void func_ovl11_802434A0(u16 arg0) {
    s32 i;
    f32 temp_fv0;

    if (D_801050BC.frame_count < 0x28) {
        D_801050BC.frame_count = D_801050BC.frame_count + 1;
    }
    i = 0;
    do {
        if (((i * 4) < D_801050BC.frame_count) && (D_801050BC.active[i] == 1)) {
            temp_fv0 = (f32) ((f64) D_801050BC.timer[i] + D_ovl11_8024C6F8);
            D_801050BC.timer[i] = temp_fv0;
            if (D_ovl11_8024C700 < (f64) temp_fv0) {
                D_801050BC.timer[i] = 0.0f;
                D_801050BC.active[i] = 0;
                play_sound_effect(0x5B, 0xD, 0x64);
            }
        }
        i += 1;
    } while (i != 10);
    if (D_801061B8 == 0) {
        if (arg0 & 0x800) {
            if (D_80104FD0.unkC4 > 0) {
                D_80104FD0.unkC4 = D_80104FD0.unkC4 - 1;
                play_sound_effect(0x5C, D_800C0E60, 0x64);
            }
        }
        if ((arg0 & 0x400) && (D_80104FD0.unkC4 < 0x14)) {
            D_80104FD0.unkC4 = D_80104FD0.unkC4 + 1;
            play_sound_effect(0x5C, D_800C0E60, 0x64);
        }
    }
}

void func_ovl11_80243634(u16 arg0, u16 arg1) {
    func_ovl11_80243050();
    if (D_800C4140 == 2) {
        if (arg1 & 0xC000) {
            func_ovl11_80242FC0(1);
        } else {
            switch (game_mode) {
                case 5:
                    func_ovl11_802434A0(arg0);
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 6:
                case 7:
                case 8:
                case 9:
                case 10:
                case 11:
                default:
                    func_ovl11_802433B4(arg0);
                    break;
            }
        }
    }
}

s32 func_ovl11_802436C8(u16 arg0) {
    func_ovl11_80243050();
    if (D_800C4140 != 2) {
        return 0;
    }
    switch (game_mode) {
        case 5:
            func_ovl11_802434A0(arg0);
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 6:
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
        default:
            func_ovl11_802433B4(arg0);
            break;
    }
    return 1;
}

void func_ovl11_8024373C(Gfx** gfxp, u8* data, s32 x, s32 y) {
    Gfx* gfx;
    s32 width;
    s32 height;
    s32 strips;
    s32 right;
    s32 acc;
    s32 count;
    s32 scale;

    gfx = *gfxp;
    width = (data[2] << 8) | data[3];
    height = (data[4] << 8) | data[5];
    strips = 1;
    if (width >= 0x61) {
        strips = 2;
    }
    func_8006A548(&gfx, (u32)(data + 8));
    height = height / strips;
    count = 0;
    if (strips != 0) {
        right = x + width;
        scale = 0x400;
        acc = 0;
        do {
            load_texture_block(&gfx, (u32)(data + (acc * height + 0x208)), G_IM_FMT_CI, G_IM_SIZ_8b, width, height, 0, 0, 0, 0, 0, 0, 0);
            acc += width;
            count += 1;
            func_8006A2C0(&gfx, x * 4, y * 4, right * 4, (y + height) * 4, 0, 0, 0, scale, scale);
            y += height;
        } while (count != strips);
    }
    *gfxp = gfx;
}

INCLUDE_ASM("asm/nonmatchings/overlay_11/func_ovl11_80241800", func_ovl11_802438D0);

INCLUDE_ASM("asm/nonmatchings/overlay_11/func_ovl11_80241800", func_ovl11_80243C48);

void func_ovl11_8024407C(void) {
    clear_animation_slot(0);
    clear_animation_slot(1);
    D_ovl11_8024C4A4 = -1;
}

INCLUDE_ASM("asm/nonmatchings/overlay_11/func_ovl11_80241800", func_ovl11_802440AC);

INCLUDE_ASM("asm/nonmatchings/overlay_11/func_ovl11_80241800", func_ovl11_80244244);
