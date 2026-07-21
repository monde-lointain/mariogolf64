#include <ultra64.h>

extern s32 func_80051FCC(void);
extern s32 putter_mode_flag;
extern s32 D_800C5EEC;
extern s32 D_800C5EF4;
extern s32 D_800C5EF8;
extern u8 D_800C6014[];
extern u8 D_800C69CC[];
extern s32 g_fog_presetA_color_r[];
extern s32 g_fog_presetA_z_min;
extern s32 g_fog_presetA_z_max;

void func_8008DDDC(void) {
    u8 *kfA, *kfB;
    u8 *tbl;
    u8 *b0, *b1, *b2, *b3, *b4;
    s32 grp, ch;
    s32 *fog;
    s16 za, zb;

    if (D_800C5EF4 < D_800C5EF8) {
        D_800C5EF4 += 1;
    }
    grp = 0;
    tbl = D_800C6014;
    kfA = (u8 *)((func_80051FCC() << 8) +
                 (((putter_mode_flag * 4 + D_800C5EEC) << 5) + (s32)tbl));
    tbl += 0x20;
    kfB = (u8 *)((func_80051FCC() << 8) + (s32)tbl +
                 ((putter_mode_flag * 4 + D_800C5EEC) << 5));
    b0 = &D_800C69CC[0];
    b1 = &D_800C69CC[0x10];
    b2 = &D_800C69CC[0x20];
    b3 = &D_800C69CC[0x30];
    b4 = &D_800C69CC[0x40];
    do {
        u8 *p0 = b0, *p1 = b1, *p2 = b2, *p3 = b3, *p4 = b4;
        ch = 0;
        do {
            u8 *a = kfA + ch;
            u8 *b = kfB + ch;
            *p0 = a[4] + ((b[4] - a[4]) * D_800C5EF4) / 1000;
            *p1 = a[7] + ((b[7] - a[7]) * D_800C5EF4) / 1000;
            *p2 = a[7] + ((b[7] - a[7]) * D_800C5EF4) / 1000;
            *p3 = a[4] + ((b[4] - a[4]) * D_800C5EF4) / 1000;
            ch += 1;
            p0++;
            p1++;
            p2++;
            p3++;
            *p4 = a[4] + ((b[4] - a[4]) * D_800C5EF4) / 1000;
            p4++;
        } while (ch != 3);
        b4 += 0xA0;
        b3 += 0xA0;
        b2 += 0xA0;
        b1 += 0xA0;
        grp += 1;
        b0 += 0xA0;
    } while (grp != 4);

    fog = &g_fog_presetA_color_r[0];
    ch = 0;
    do {
        u8 *a = kfA + ch;
        u8 *b = kfB + ch;
        *fog = a[0x18] + ((b[0x18] - a[0x18]) * D_800C5EF4) / 1000;
        ch += 1;
        fog++;
    } while (ch != 3);

    za = *(s16 *)(kfA + 0x1C);
    g_fog_presetA_z_min = za + ((*(s16 *)(kfB + 0x1C) - za) * D_800C5EF4) / 1000;
    zb = *(s16 *)(kfA + 0x1E);
    g_fog_presetA_z_max = zb + ((*(s16 *)(kfB + 0x1E) - zb) * D_800C5EF4) / 1000;
}
