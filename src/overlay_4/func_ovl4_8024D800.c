#include "common.h"

extern u32 D_ovl4_802542A0;

void func_ovl4_8024D800(u32 seed) {
    D_ovl4_802542A0 = seed;
}

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_8024D80C);

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_8024DA78);

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_8024E02C);

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_8024E288);

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_8024E2D4);

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_8024E514);

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_8024E71C);

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_8024E958);

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_8024EB98);

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_8024EDD8);

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_8024F014);

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_8024F118);

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_8024F21C);

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_8024F308);

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_8024F4DC);

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_8024F60C);

void func_ovl4_8024F60C(char* source_string);
extern char D_ovl4_802550E8[];
extern char D_ovl4_802550FC[];
extern s32 D_ovl4_802543A8;

void func_ovl4_8024F790(void) {
    func_ovl4_8024F60C(D_ovl4_802550E8);
}

void func_ovl4_8024F7B0(void) {
    func_ovl4_8024F60C(D_ovl4_802550FC);
    D_ovl4_802543A8 = 0;
}

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_8024F7DC);

void func_ovl4_8024FA98(void) {
}

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_8024FAA0);

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_8024FF14);

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_8024FF74);

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_80250194);

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_80250474);

void func_ovl4_802504E0(void) {
}

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", load_hole_terrain_assets);

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_802509BC);

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_80250C54);

void func_80041AA8(void);
void func_ovl4_80250DE4(void);
void func_ovl4_80250EE0(void);
void func_ovl4_80250F04(void);
void func_ovl4_80250F90(void);
void func_ovl4_80250FB4(void);

void func_ovl4_80250DA0(void) {
    func_ovl4_80250DE4();
    func_ovl4_80250FB4();
    func_80041AA8();
    func_ovl4_80250F04();
    func_ovl4_80250F90();
    func_ovl4_80250EE0();
}

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_80250DE4);

extern u8 *D_ovl4_8025D350;
extern u8 *D_ovl4_8025D354;

void func_ovl4_80250EE0(void) {
    D_ovl4_8025D354 = (u8 *)0x80315000;
    D_ovl4_8025D350 = (u8 *)0x80315000;
    D_ovl4_8025D354 = (u8 *)0x80315000;
}

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_80250F04);

extern void bzero(void* address, s32 length);
extern u8 collision_triangles[];

void func_ovl4_80250F90(void) {
    bzero(&collision_triangles, 0x3C00);
}

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_80250FB4);

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_80250FEC);

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_802510D4);

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_80251288);

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_802513B8);

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_80251610);

extern u8 D_80133692;
extern u8 *D_ovl4_80255330;

void func_ovl4_8025176C(void) {
    s32 i;
    s32 off;
    u8 *temp_a1;
    u8 *var_a0;
    u8 *var_v1;
    u8 temp_v0;

    for (i = 0; i < 0x80; i++) {
        off = i * 0xA00;
        var_v1 = D_ovl4_80255330;
        temp_a1 = var_v1 + 2;
        var_a0 = &D_80133692 + off;
        if (var_v1 != temp_a1) {
            do {
                temp_v0 = *var_v1;
                var_v1 += 1;
                *var_a0 = temp_v0;
                var_a0 += 1;
            } while (var_v1 != temp_a1);
        }
        D_ovl4_80255330 = temp_a1;
    }
}

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_802517C4);

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_802518F0);

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_80251A28);

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_80251B6C);

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_80251CB4);

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_80251DFC);

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_80252014);

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_802527FC);

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_8025283C);

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_80252B24);

void func_ovl4_80252014(void);
void func_ovl4_80252B24(void);
extern char D_ovl4_80255314[];
extern char D_ovl4_80255318[];

void func_ovl4_80253234(s32 mode) {
    func_ovl4_80252B24();
    osSyncPrintf(D_ovl4_80255314);
    func_ovl4_80252014();
    osSyncPrintf(D_ovl4_80255318);
}

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_80253270);

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_80253620);

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_80253680);

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_802536FC);

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_802537EC);

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_802541E8);
