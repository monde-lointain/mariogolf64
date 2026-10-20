#include "common.h"
#include "../main/terrain.h"
#include "../main/rom_asset.h"

typedef struct CameraPresetVec {
    s32 x;
    s32 y;
    s32 z;
} CameraPresetVec;

typedef struct CameraPresetTable {
    CameraPresetVec table[4];
    CameraPresetVec lookat[4];
    CameraPresetVec unknown_60[8];
    CameraPresetVec unknown_c0[8];
} CameraPresetTable;

extern u32 D_ovl4_802542A0;
extern s16 D_800BAC0C[];
extern s32 D_800BB00C;
extern s8 g_terrain_vtx_xform_mode;
extern u8 D_8019D620;
extern s8 D_801B60B8;
extern s32 D_800BA9E4;
extern s32 D_800BA9E8;
extern s32 D_800BA9EC;
extern s32 camera_position_x;
extern s32 camera_position_y;
extern s32 camera_position_z;
extern CameraPresetTable D_80105E10;
void func_ovl4_802537EC(s32, s32, void **, void **);
s32 report_div_error(void);

void func_ovl4_8024D800(u32 seed) {
    D_ovl4_802542A0 = seed;
}

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_8024D80C);

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_8024DA78);

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_8024E02C);

void func_ovl4_8024E288(void) {
    s32 var_a0;
    s32 var_v0;
    s32 var_v1;

    D_800BB00C = 0;
    var_a0 = 0;
    do {
        var_v1 = 0xF;
        var_v0 = (var_a0 << 5) + 0x1E;
loop_2:
        *(s16*)((u8*)D_800BAC0C + var_v0) = -1;
        var_v1 -= 1;
        var_v0 -= 2;
        if (var_v1 >= 0) {
            goto loop_2;
        }
        var_a0 += 1;
    } while (var_a0 < 0x20);
}

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

extern u8 *D_ovl4_8025D350;
extern u8 *D_ovl4_8025D354;
s32 func_800402F4(s32 color);

/* Terrain tile covering 32x32 pixel block (x, y); tiles form an 8-wide grid. */
static inline s32 get_tile_index(s32 x, s32 y) {
    x /= 32;
    y >>= 5;
    return x + y * 8;
}

/* Color at pixel (x, y) of the 256x512 terrain map, read from its tile's
 * colors as a flat 32-wide grid; 0 outside the map. */
static inline u16 get_tile_color(s32 x, s32 y) {
    if (x < 0) {
        return 0;
    }
    if (x >= 0x100) {
        return 0;
    }
    if (y < 0) {
        return 0;
    }
    if (y >= 0x200) {
        return 0;
    }
    return g_terrain_tile_cache[get_tile_index(x, y)].colors[0][(y & 0x1F) * 32 + (x & 0x1F)];
}

/* Converts the terrain colors into a 256x512 palette-index image at 0x8030F800. */
void func_ovl4_8024F4DC(void) {
    s32 y;
    s32 x;

    D_ovl4_8025D354 = (u8 *)0x8030F800;
    D_ovl4_8025D350 = (u8 *)0x8030F800;
    for (y = 0; y < 0x200; y++) {
        for (x = 0; x < 0x100; x++) {
            D_ovl4_8025D354 = &D_ovl4_8025D350[y * 0x100 + x];
            *D_ovl4_8025D354 = func_800402F4(get_tile_color(x, y));
        }
    }
}

s32 validate_heap_string_with_output(char* source_string, s32* result_out);
s32 ci8_to_rgba5551(s32 index);

/* Stores color at pixel (x, y) of the 256x512 terrain map; ignores pixels outside it. */
static inline void set_tile_color(s32 x, s32 y, u16 color) {
    if (x < 0) {
        return;
    }
    if (x >= 0x100) {
        return;
    }
    if (y < 0) {
        return;
    }
    if (y >= 0x200) {
        return;
    }
    g_terrain_tile_cache[get_tile_index(x, y)].colors[0][(y & 0x1F) * 32 + (x & 0x1F)] = color;
}

/* Loads a 256x512 palette-index image from source_string into 0x8030F800 and
 * rebuilds the terrain tile colors from it. */
void func_ovl4_8024F60C(char* source_string) {
    s32 size;
    s32 i;
    u32 j;
    s32 y;
    s32 x;

    D_ovl4_8025D354 = (u8 *)0x8030F800;
    D_ovl4_8025D350 = (u8 *)0x8030F800;
    if (validate_heap_string_with_output(source_string, &size) == 0 && size == 0x20000) {
        D_ovl4_8025D354 = D_ovl4_8025D350;
    }
    for (i = 0; i < 0x80; i++) {
        for (j = 0; j < 0x4C8; j++) {
            g_terrain_tile_cache[i].colors[0][j] = 0;
        }
    }
    for (y = 0; y < 0x200; y++) {
        for (x = 0; x < 0x100; x++) {
            D_ovl4_8025D354 = &D_ovl4_8025D350[y * 0x100 + x];
            set_tile_color(x, y, ci8_to_rgba5551(*D_ovl4_8025D354));
        }
    }
}

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

extern s32 D_800BB020;
extern s32 game_mode;
extern s32 scenario_mode_id;
extern s32 flag_is_set(s32 flag);
extern void flag_clear(s32 flag);
extern void gen_terrain_detail_texture(s32 arg0, s32 arg1);
extern void update_scenery_cylinder_transforms(void);
extern void open_rom_asset_slot(s32 asset_index, RomLoadSlot *slot);
extern u32 get_rom_load_slot_size(RomLoadSlot *slot);
extern void read_rom_load_slot(void *destination, u32 size, RomLoadSlot *slot);
extern s32 D_ovl4_80254C4C[][18];
extern u16 D_ovl4_802543B2[][18][7];
extern u16 D_ovl4_802543B4[][18][7];
extern s32 D_ovl4_80255320;
extern char D_ovl4_80255144[];
extern char D_ovl4_80255154[];
extern char D_ovl4_80255158[];
extern char D_ovl4_80255160[];
extern char D_ovl4_80255168[];
extern char D_ovl4_80255188[];
extern char D_ovl4_802551A8[];
extern char D_ovl4_802551C8[];
extern char D_ovl4_802551E8[];
extern char D_ovl4_80255208[];
extern char D_ovl4_80255228[];
extern char D_ovl4_80255248[];
extern char D_ovl4_8025524C[];
void func_ovl4_80250DA0(void);
void func_ovl4_80250C54(void);
void func_ovl4_80251DFC(s32 arg0, s32 arg1, s32 arg2);
void func_ovl4_802513B8(void);
void func_ovl4_802517C4(void);
void func_ovl4_802518F0(void);
void func_ovl4_80251610(void);
void func_800425C8(void);
s32 func_ovl4_80250474(s32 arg0);
void func_ovl4_802504E0(s32 arg0, s32 arg1) {
}

void load_hole_terrain_assets(s32 arg0, s32 arg1) {
    RomLoadSlot slot;
    char name[0x80];
    s32 name_size;
    s32 var_s3;
    s32 var_v0;
    u16 var_a0;
    u32 size;

    var_s3 = arg1;

    if (arg0 == 2) {
        gen_terrain_detail_texture(0x60, 0xC8);
    } else if (arg0 == 3) {
        gen_terrain_detail_texture(0x60, 0xC8);
    } else {
        gen_terrain_detail_texture(0x60, 0xBE);
    }

    if (g_terrain_vtx_xform_mode != 0) {
        D_ovl4_80255320 = scenario_mode_id;
        if (scenario_mode_id >= 0xC) {
            D_ovl4_80255320 = 0xB;
        }
        gen_terrain_detail_texture(0x40, 0xD2);
    } else if (flag_is_set(0x50) != 0) {
        D_ovl4_80255320 = D_ovl4_80254C4C[arg0][var_s3 - 1];
        if (flag_is_set(0x6A) != 0) {
            D_ovl4_80255320 += 1;
        }
        if (flag_is_set(0x6B) != 0) {
            var_v0 = D_ovl4_80255320 + 1;
            goto block_15;
        }
    } else {
        var_v0 = func_ovl4_80250474(arg0);
block_15:
        D_ovl4_80255320 = var_v0;
    }

    osSyncPrintf(D_ovl4_80255144, D_ovl4_80255320);

    if (flag_is_set(0x50) == 0) {
        if (flag_is_set(0x51) != 0 && var_s3 == 0) {
            var_s3 = 0x12;
        }
        func_ovl4_80250DA0();
        func_ovl4_80251DFC(arg0, var_s3, 1);
        func_ovl4_802517C4();
        func_ovl4_80251DFC(arg0, var_s3, 2);
        func_ovl4_802518F0();
        func_ovl4_80251DFC(arg0, var_s3, 3);
        func_ovl4_802513B8();

        if (D_801B60B8 == 0 && D_800BB020 != 0x1E) {
            if (game_mode == 0xC) {
                var_a0 = 0x3F3;
            } else {
                var_a0 = D_ovl4_802543B2[arg0][var_s3][0];
            }
            open_rom_asset_slot(var_a0, &slot);
            read_rom_load_slot(&camera_position_x, get_rom_load_slot_size(&slot), &slot);
        }

        if (game_mode == 0xC) {
            var_a0 = 0x3F4;
        } else {
            var_a0 = D_ovl4_802543B4[arg0][var_s3][0];
        }
        open_rom_asset_slot(var_a0, &slot);
        size = get_rom_load_slot_size(&slot);
        read_rom_load_slot(&D_80105E10, size, &slot);
        osSyncPrintf(D_ovl4_80255154, size);

        func_ovl4_802504E0(arg0, var_s3);
        func_ovl4_80251DFC(arg0, var_s3, 4);
        func_ovl4_80251610();
        func_ovl4_80251DFC(arg0, var_s3, 5);
        func_ovl4_8024FF74();
        func_800425C8();
        D_ovl4_802543A8 = 1;
        osSyncPrintf(D_ovl4_80255158);
        func_ovl4_8024FAA0();
        osSyncPrintf(D_ovl4_80255160);
        update_scenery_cylinder_transforms();
        return;
    }

    func_ovl4_80250DA0();
    if (flag_is_set(0x52) != 0) {
        flag_clear(0x52);
    } else {
        sprintf(name, D_ovl4_80255168, arg0, arg0, var_s3);
        validate_heap_string_with_output(name, &name_size);
        sprintf(name, D_ovl4_80255188, arg0, arg0, var_s3);
        validate_heap_string_with_output(name, &name_size);
        sprintf(name, D_ovl4_802551A8, arg0, arg0, var_s3);
        validate_heap_string_with_output(name, &name_size);
        sprintf(name, D_ovl4_802551C8, arg0, arg0, var_s3);
        validate_heap_string_with_output(name, &name_size);
        sprintf(name, D_ovl4_802551E8, arg0, arg0, var_s3);
        validate_heap_string_with_output(name, &name_size);
        sprintf(name, D_ovl4_80255208, arg0, arg0, var_s3);
        if (validate_heap_string_with_output(name, &name_size) == 0) {
            func_ovl4_80250C54();
        }
        sprintf(name, D_ovl4_80255228, arg0, arg0, var_s3);
        validate_heap_string_with_output(name, &name_size);
    }

    osSyncPrintf(D_ovl4_80255248);
    func_ovl4_8024FF74();
    D_ovl4_802543A8 = 1;
    func_ovl4_8024FAA0();
    update_scenery_cylinder_transforms();
    osSyncPrintf(D_ovl4_8025524C);
}

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_802509BC);

typedef struct CollisionTriangle {
    s32 active;
    u8 unk_04[0x5C];
} CollisionTriangle;

extern void check_and_print_grid(char *str, s32 col, s32 row);
extern void bzero(void *address, s32 length);
extern char D_80105118[];
extern char D_ovl4_80255254[];
extern char D_ovl4_80255264[];
extern char D_ovl4_80255270[];
extern CollisionTriangle collision_triangles[];

void func_ovl4_80250C54(void) {
    s32 i;
    s32 j;

    check_and_print_grid(D_ovl4_80255254, 19, 19);
    for (i = 0; i != 157; i++) {
        if (collision_triangles[i].active == 1) {
            for (j = 157; j != 160; j++, i++) {
                sprintf(D_80105118, D_ovl4_80255264, j, i);
                check_and_print_grid(D_80105118, 4, -141);
                collision_triangles[j] = collision_triangles[i];
                bzero(&collision_triangles[i], sizeof(CollisionTriangle));
            }
            check_and_print_grid(D_ovl4_80255270, 19, 19);
            return;
        }
    }
}

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

/* Zeroes the preset table; the live camera is zeroed only when D_801B60B8 == 0. */
void func_ovl4_80250DE4(void) {
    CameraPresetVec *row;
    s32 i;

    if (D_801B60B8 == 0) {
        camera_position_x = 0;
        camera_position_y = 0;
        camera_position_z = 0;
        D_800BA9E4 = 0;
        D_800BA9E8 = 0;
        D_800BA9EC = 0;
    }
    for (i = 0; i < 4; i++) {
        D_80105E10.table[i].x = 0;
        D_80105E10.table[i].y = 0;
        D_80105E10.table[i].z = 0;
        D_80105E10.lookat[i].x = 0;
        D_80105E10.lookat[i].y = 0;
        D_80105E10.lookat[i].z = 0;
    }
    i = 0;
    row = &D_80105E10.unknown_c0[0];
    for (; i < 8; i++) {
        D_80105E10.unknown_60[i].x = 0;
        D_80105E10.unknown_60[i].y = 0;
        D_80105E10.unknown_60[i].z = 0;
        row->x = 0;
        row++;
        D_80105E10.unknown_c0[i].z = 0;
    }
}

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

void func_ovl4_80250F90(void) {
    bzero(collision_triangles, sizeof(CollisionTriangle) * 160);
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

extern u8 *D_ovl4_80255330;
extern char D_ovl4_8025527C[];
extern char D_ovl4_80255284[];

void func_ovl4_80251288(void) {
    s16 count;
    u8 byte;
    u8 *src;
    u8 *end;
    u8 *next;
    u8 *out;
    u16 *dst;
    u16 *limit;
    u16 *stop;
    s32 value;

    dst = (u16 *) D_ovl4_8025D354;
    limit = dst + 0x10000;
    for (;;) {
        src = D_ovl4_80255330;
        end = src + 2;
        out = (u8 *) &count;
        if (src != end) {
            do {
                *out = *src;
                src++;
                out++;
            } while (src != end);
        }
        D_ovl4_80255330 = end;
        if (count == 0) {
            break;
        }
        src = end;
        next = src + 1;
        out = &byte;
        if (src != next) {
            do {
                *out = *src;
                src++;
                out++;
            } while (src != next);
        }
        D_ovl4_80255330 = next;
        stop = dst + count;
        if (limit < stop) {
            osSyncPrintf(D_ovl4_8025527C);
            break;
        }
        value = ((byte & 0xF0) << 4) | (byte & 0xF);
        if (dst != stop) {
            do {
                *dst = value;
                dst++;
            } while (dst != stop);
        }
    }
    if (dst != limit) {
        osSyncPrintf(D_ovl4_80255284);
    }
    while (dst < limit) {
        *dst = 0;
        dst++;
    }
    D_ovl4_8025D354 = D_ovl4_8025D350;
}

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_802513B8);

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_80251610);

extern u8 D_80133692;

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

extern char D_ovl4_8025529C[];

void func_ovl4_80251A28(u16 *arg0, s32 arg1);
void func_ovl4_80251B6C(u16 *arg0, s32 arg1);
void func_ovl4_80251CB4(u16 *arg0, s32 arg1);

/* Decodes the hole terrain vertex stream into g_terrain_grid_verts until tag 9. */
void func_ovl4_802517C4(void) {
    s8 tag;
    u16 x;
    u16 height;
    u16 z;
    s32 offset;
    s32 count;
    s8 *dst;
    u8 *end;
    u8 *src;
    u8 byte;

    count = 0;
    offset = 0;
next_record:
    src = D_ovl4_80255330;
    end = src + 1;
    dst = &tag;
    if (src != end) {
        do {
            byte = *src;
            src += 1;
            *dst = (s8) byte;
            dst += 1;
        } while (src != end);
    }
    D_ovl4_80255330 = end;
    if (tag != 9) {
        if (count >= 0x252) {
            osSyncPrintf(D_ovl4_8025529C, end, tag);
            return;
        }
        if (!(tag & 0x40)) {
            func_ovl4_80251A28(&x, (s32) tag);
            ((GridVertex *) ((u8 *) g_terrain_grid_verts + offset))->x = x;
            ((GridVertex *) ((u8 *) g_terrain_grid_verts + offset))->texture_s = ((s32) (x << 0x10) >> 0xE) + 0x20;
        }
        func_ovl4_80251B6C(&height, (s32) tag);
        ((GridVertex *) ((u8 *) g_terrain_grid_verts + offset))->height = height;
        if (!(tag & 0x80)) {
            func_ovl4_80251CB4(&z, (s32) tag);
            ((GridVertex *) ((u8 *) g_terrain_grid_verts + offset))->z = z;
            ((GridVertex *) ((u8 *) g_terrain_grid_verts + offset))->texture_t = ((s32) (z << 0x10) >> 0xE) + 0x20;
        }
        offset += 0x10;
        count += 1;
        goto next_record;
    }
}

void func_ovl4_802518F0(void) {
    s8 tag;
    u16 x;
    u16 height;
    u16 z;
    s32 vertex;
    s32 offset;
    s32 index;
    s8 *dst;
    u8 *end;
    u8 *src;
    u8 byte;

    vertex = 0;
    offset = 0;
    for (;;) {
        src = D_ovl4_80255330;
        end = src + 1;
        dst = &tag;
        if (src != end) {
            do {
                byte = *src;
                src += 1;
                *dst = (s8) byte;
                dst += 1;
            } while (src != end);
        }
        D_ovl4_80255330 = end;
        if (tag == 9) {
            return;
        }
        if (!(tag & 0x40)) {
            s32 slot;

            func_ovl4_80251A28(&x, (s32) tag);
            slot = vertex * 16 + offset;
            ((GridVertex *) ((u8 *) D_80185220 + slot))->x = x;
            ((GridVertex *) ((u8 *) D_80185220 + slot))->texture_s = ((s32) (x << 0x10) >> 0xE) + 0x20;
        }
        func_ovl4_80251B6C(&height, (s32) tag);
        index = vertex * 16 + offset;
        ((GridVertex *) ((u8 *) D_80185220 + index))->height = height;
        if (!(tag & 0x80)) {
            func_ovl4_80251CB4(&z, (s32) tag);
            ((GridVertex *) ((u8 *) D_80185220 + index))->z = z;
            ((GridVertex *) ((u8 *) D_80185220 + index))->texture_t = ((s32) (z << 0x10) >> 0xE) + 0x20;
        }
        vertex += 1;
        if (vertex >= 16) {
            vertex = 0;
            offset += 0x100;
        }
    }
}

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

void func_ovl4_80253620(s32 arg0, s32 arg1, GridVertex *arg2, GridVertex *arg3) {
    GridVertex *var_a2;

    var_a2 = arg2;
    if (var_a2 < arg3) {
        do {
            s32 x;
            s32 z;
            s32 tex;

            x = var_a2->x;
            tex = x * 4 - (arg0 << 9);
            tex = (tex << 5) + 0x100;
            var_a2->texture_s = tex;
            z = var_a2->z;
            tex = z * 8 - (arg1 << 9);
            tex = (tex << 5) + 0x100;
            var_a2->texture_t = tex;
            var_a2 += 1;
        } while (var_a2 < arg3);
    }
}

void func_ovl4_80253680(s32 arg0, s32 arg1, GridVertex *arg2, GridVertex *arg3) {
    GridVertex *var_a2;

    var_a2 = arg2;
    if (g_terrain_vtx_xform_mode != 0) {
        func_ovl4_80253620(arg0, arg1, arg2, arg3);
        return;
    }
    if (var_a2 < arg3) {
        do {
            s32 x;
            s32 z;
            s32 tex;

            x = var_a2->x;
            tex = x - (arg0 << 9);
            tex = (tex << 5) + 0x100;
            var_a2->texture_s = tex;
            z = var_a2->z;
            tex = z - (arg1 << 9);
            tex = (tex << 5) + 0x100;
            var_a2->texture_t = tex;
            var_a2 += 1;
        } while (var_a2 < arg3);
    }
}

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_802536FC);

INCLUDE_ASM("asm/nonmatchings/overlay_4/func_ovl4_8024D800", func_ovl4_802537EC);

void func_ovl4_802541E8(void) {
    void *sp10;
    void *sp14;
    s32 var_s0;
    s32 var_s1;

    var_s1 = 0;
    sp10 = &D_8019D620 + 0x4000;
    sp14 = &D_8019D620 + 0xD600;
    do {
        var_s0 = 0;
loop_2:
        func_ovl4_802537EC(var_s0, var_s1, &sp10, &sp14);
        var_s0 += 1;
        if (var_s0 < 8) {
            goto loop_2;
        }
        var_s1 += 1;
    } while (var_s1 < 0x10);
    if (((u32) (&D_8019D620 + 0xD400) < (u32) sp10) || ((u32) (&D_8019D620 + 0x17E00) < (u32) sp14)) {
        report_div_error();
    }
}
