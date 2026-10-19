#include "common.h"
#include "../main/terrain.h"

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

void func_ovl4_8024F4DC(void);
void func_ovl4_8024F7DC(void);
void func_ovl4_8024FAA0(void);

void func_ovl4_8024FF14(void) {
    s32 unused[400];

    if (D_ovl4_802543A8 == 0) {
        func_ovl4_8024F4DC();
        D_ovl4_802543A8 = 1;
        func_ovl4_8024F7DC();
        func_ovl4_8024FAA0();
        func_ovl4_8024FA98();
        return;
    }
    func_ovl4_8024F790();
    D_ovl4_802543A8 = 0;
}

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

extern SceneryCylinder collision_cylinders[];

void func_ovl4_80250F04(void) {
    s32 i;

    for (i = 0; i < SCENERY_CYLINDER_COUNT; i++) {
        collision_cylinders[i].x = 0;
        collision_cylinders[i].terrain_y = 0;
        collision_cylinders[i].z = 0;
        collision_cylinders[i].sprite_index = -1;
        collision_cylinders[i].unknown_08[0] = 0;
        collision_cylinders[i].cull_state = 1;
        collision_cylinders[i].radius = 0x40;
        collision_cylinders[i].scale_y = 0x100;
    }
}

extern void bzero(void* address, s32 length);
extern u8 collision_triangles[];

void func_ovl4_80250F90(void) {
    bzero(&collision_triangles, 0x3C00);
}

void func_ovl4_80250FB4(void) {
    s32 i;

    for (i = 0; i < 0x80; i++) {
        g_terrain_tile_cache[i].colors[33][35] = 0;
        g_terrain_tile_cache[i].unknown_992 = 0;
    }
}

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

void func_ovl4_80251A28(u16 *arg0, s32 arg1) {
    u8 *src;
    u8 *end;
    u8 *dst;
    s32 mode;

    mode = arg1 & 3;
    *arg0 = 0;
    switch (mode) {
    case 1:
        src = D_ovl4_80255330;
        end = src + 1;
        dst = (u8 *) arg0;
        if (src != end) {
            do {
                *dst = *src;
                src++;
                dst++;
            } while (src != end);
        }
        D_ovl4_80255330 = end;
        *arg0 = *arg0 >> 8;
        break;
    case 2:
        src = D_ovl4_80255330;
        end = src + 1;
        dst = (u8 *) arg0;
        if (src != end) {
            do {
                *dst = *src;
                src++;
                dst++;
            } while (src != end);
        }
        D_ovl4_80255330 = end;
        *arg0 = (*arg0 >> 8) * 32;
        break;
    case 3:
        src = D_ovl4_80255330;
        end = src + 1;
        dst = (u8 *) arg0;
        if (src != end) {
            do {
                *dst = *src;
                src++;
                dst++;
            } while (src != end);
        }
        D_ovl4_80255330 = end;
        *arg0 = (*arg0 >> 8) * 64;
        break;
    case 0:
        {
            u8 *out = (u8 *) arg0;
            u8 *cursor = D_ovl4_80255330;
            u8 *stop = cursor + 2;
            if (cursor != stop) {
                do {
                    *out = *cursor;
                    cursor++;
                    out++;
                } while (cursor != stop);
            }
            D_ovl4_80255330 = stop;
        }
        break;
    }
}

void func_ovl4_80251B6C(u16 *arg0, s32 arg1) {
    u8 *src;
    u8 *end;
    u8 *dst;
    s32 mode;

    mode = (u32) (arg1 & 0xC) >> 2;
    *arg0 = 0;
    switch (mode) {
    case 1:
        src = D_ovl4_80255330;
        end = src + 1;
        dst = (u8 *) arg0;
        if (src != end) {
            do {
                *dst = *src;
                src++;
                dst++;
            } while (src != end);
        }
        D_ovl4_80255330 = end;
        *arg0 = *arg0 >> 8;
        break;
    case 2:
        src = D_ovl4_80255330;
        end = src + 1;
        dst = (u8 *) arg0;
        if (src != end) {
            do {
                *dst = *src;
                src++;
                dst++;
            } while (src != end);
        }
        D_ovl4_80255330 = end;
        *arg0 = (*arg0 >> 8) * 2;
        break;
    case 3:
        src = D_ovl4_80255330;
        end = src + 1;
        dst = (u8 *) arg0;
        if (src != end) {
            do {
                *dst = *src;
                src++;
                dst++;
            } while (src != end);
        }
        D_ovl4_80255330 = end;
        *arg0 = (*arg0 >> 8) * 4;
        break;
    case 0:
        {
            u8 *out = (u8 *) arg0;
            u8 *cursor = D_ovl4_80255330;
            u8 *stop = cursor + 2;
            if (cursor != stop) {
                do {
                    *out = *cursor;
                    cursor++;
                    out++;
                } while (cursor != stop);
            }
            D_ovl4_80255330 = stop;
        }
        break;
    }
}

void func_ovl4_80251CB4(u16 *arg0, s32 arg1) {
    u8 *src;
    u8 *end;
    u8 *dst;
    s32 mode;

    mode = (u32) (arg1 & 0x30) >> 4;
    *arg0 = 0;
    switch (mode) {
    case 1:
        src = D_ovl4_80255330;
        end = src + 1;
        dst = (u8 *) arg0;
        if (src != end) {
            do {
                *dst = *src;
                src++;
                dst++;
            } while (src != end);
        }
        D_ovl4_80255330 = end;
        *arg0 = *arg0 >> 8;
        break;
    case 2:
        src = D_ovl4_80255330;
        end = src + 1;
        dst = (u8 *) arg0;
        if (src != end) {
            do {
                *dst = *src;
                src++;
                dst++;
            } while (src != end);
        }
        D_ovl4_80255330 = end;
        *arg0 = (*arg0 >> 8) << 5;
        break;
    case 3:
        src = D_ovl4_80255330;
        end = src + 1;
        dst = (u8 *) arg0;
        if (src != end) {
            do {
                *dst = *src;
                src++;
                dst++;
            } while (src != end);
        }
        D_ovl4_80255330 = end;
        *arg0 = (*arg0 >> 8) << 6;
        break;
    case 0:
        {
            u8 *out = (u8 *) arg0;
            u8 *cursor = D_ovl4_80255330;
            u8 *stop = cursor + 2;
            if (cursor != stop) {
                do {
                    *out = *cursor;
                    cursor++;
                    out++;
                } while (cursor != stop);
            }
            D_ovl4_80255330 = stop;
        }
        break;
    }
}

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
